// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Jobs/CPM_PublishJobs.h"

#include "CPM_Defination.h"
#include "CPM_PakManagerSettings.h"
#include "Chunk/CPM_Chunk.h"
#include "ConvaiPakManagerEditorUtils.h"
#include "Core/WorkflowContext.h"
#include "Interface/WorkflowInterface.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Proxy/CPM_Proxy.h"
#include "Utility/CPM_Log.h"
#include "Utility/CPM_UtilityLibrary.h"

namespace
{
	/** UAT reports its outcome as one of these. See UATHelperModule. */
	const TCHAR* UatCompleted = TEXT("Completed");
	const TCHAR* UatCanceled = TEXT("Canceled");

	FString PlatformName(const ECPM_Platform Platform)
	{
		switch (Platform)
		{
		case ECPM_Platform::Windows:
			return TEXT("Windows");
		case ECPM_Platform::Linux:
			return TEXT("Linux");
		default:
			return FString();
		}
	}

	/** What every Asset published from here is: a Pak, and the kind of thing it holds. */
	TArray<FString> PakTagsFor(const FString& AssetType)
	{
		return { TEXT("Pak"),
			AssetType.Equals(TEXT("Scene"), ESearchCase::IgnoreCase) ? TEXT("Scene") : TEXT("Avatar") };
	}
}

// ---------------------------------------------------------------------------------------------
// UCPM_PublishJobBase
// ---------------------------------------------------------------------------------------------

void UCPM_PublishJobBase::IInitialize_Implementation(const TScriptInterface<IWorkflowInterface>& Workflow)
{
	CachedWorkflow = Workflow;
}

void UCPM_PublishJobBase::ICancel_Implementation(const bool bForce)
{
	bCancelled = true;

	// Reported as Cancelled, never Failed: a workflow counts Cancelling as still running, so a job
	// that reports Failed with retries left is retried - re-issuing the very request being cancelled.
	Report(EJobResult::Cancelled, TEXT("cancelled"));
}

bool UCPM_PublishJobBase::TryGetRequest(FCPM_PublishRequest& OutRequest) const
{
	UWorkflowContext* Context = CachedWorkflow.GetInterface() ? CachedWorkflow->IGetContext() : nullptr;
	return Context && Context->TryGet(OutRequest);
}

void UCPM_PublishJobBase::Report(const EJobResult Result, const FString& Error, TArray<FInstancedStruct>&& Outputs)
{
	if (bReported || !CachedWorkflow.GetInterface())
	{
		return;
	}
	bReported = true;

	FJobCompletionInfo Info;
	Info.Job = this;
	Info.Result = Result;
	Info.ErrorMessage = Error;
	Info.Outputs = MoveTemp(Outputs);
	CachedWorkflow->IOnJobCompleted(Info);
}

void UCPM_PublishJobBase::ReportProgress(const FString& Step, const float Percent)
{
	if (!CachedWorkflow.GetInterface())
	{
		return;
	}

	FJobProgressInfo Info;
	Info.Job = this;
	Info.Progress = FMath::Clamp(Percent, 0.0f, 1.0f);
	Info.ProgressText = FText::FromString(Step);
	CachedWorkflow->IReportJobProgress(Info);
}

// ---------------------------------------------------------------------------------------------
// UCPM_PackagePaksJob
// ---------------------------------------------------------------------------------------------

FJobConfig UCPM_PackagePaksJob::IGetJobConfig_Implementation() const
{
	FJobConfig Config;
	Config.Name = TEXT("Packaging");
	Config.Description = TEXT("Cooks and packages this Chunk into a Pak for each platform the policy asks for.");

	// No timeout and no retries. A cook is minutes to tens of minutes on a large project and any
	// number we picked would be wrong for somebody; a retry would silently start the whole thing
	// again, which is worse than reporting the failure UAT already explained.
	Config.TimeoutSeconds = 0.0f;
	Config.MaxRetries = 0;
	return Config;
}

FJobIOSpec UCPM_PackagePaksJob::IDeclareIO_Implementation() const
{
	FJobIOSpec Spec;
	Spec.Requires<FCPM_PublishRequest>();
	Spec.ProducesMany<FCPM_PakArtifact>();
	return Spec;
}

void UCPM_PackagePaksJob::IExecute_Implementation()
{
	if (!TryGetRequest(Request))
	{
		Report(EJobResult::Failed, TEXT("no publish request in the workflow context"));
		return;
	}

	Remaining = Request.Policy.PlatformsToPackage();
	if (Remaining.IsEmpty())
	{
		Report(EJobResult::Failed, TEXT("the publish policy asks for no platforms"));
		return;
	}

	PackageNextPlatform();
}

void UCPM_PackagePaksJob::PackageNextPlatform()
{
	if (bCancelled)
	{
		return;
	}

	if (Remaining.IsEmpty())
	{
		TArray<FInstancedStruct> Outputs;
		Outputs.Reserve(Built.Num());
		for (const FCPM_PakArtifact& Artifact : Built)
		{
			Outputs.Add(FInstancedStruct::Make(Artifact));
		}
		Report(EJobResult::Success, FString(), MoveTemp(Outputs));
		return;
	}

	const ECPM_Platform Platform = Remaining[0];

	const int32 Done = Built.Num();
	const int32 Total = Done + Remaining.Num();
	const float Progress = Total > 0 ? static_cast<float>(Done) / static_cast<float>(Total) : 0.0f;

	// One answer, decided by the subsystem before the queue was built. Deliberately NOT read from
	// the settings singleton here: this Job cannot tell a package-only run from a publish, and the
	// setting must never make a package-only run skip the cook it exists to perform.
	const FCPM_PakArtifact Existing = ArtifactFor(Platform);
	if (Request.bReuseExistingPaks && UCPM_UtilityLibrary::IsPakUsable(Existing.PakPath))
	{
		// Warned about rather than merely logged: from here on nothing distinguishes a Pak built
		// before the creator's last edit from one built from it, so the only chance to say so is now.
		UCPM_UtilityLibrary::CPM_LogMessage(
			FString::Printf(TEXT("Publishing the existing Pak at %s - packaging %s was skipped because this publish ")
				TEXT("was asked to reuse existing paks"),
				*Existing.PakPath, *PlatformName(Platform)),
			ECPM_LogLevel::Warning);

		ReportProgress(FString::Printf(TEXT("Using the existing %s Pak"), *PlatformName(Platform)), Progress);

		Remaining.RemoveAt(0);
		Built.Add(Existing);
		PackageNextPlatform();
		return;
	}

	const FCPM_PlatformPolicy* Policy = Request.Policy.Find(Platform);
	if (!Policy)
	{
		Report(EJobResult::Failed, FString::Printf(TEXT("no policy for platform %s"), *PlatformName(Platform)));
		return;
	}

	FCPM_PackageParam Param;
	Param.Platform = Platform;
	Param.Configuration = Policy->Configuration;
	Param.OutputDirectory = UCPM_UtilityLibrary::GetPackageDirectory();

	ReportProgress(FString::Printf(TEXT("Packaging %s"), *PlatformName(Platform)), Progress);

	FOnUatTaskResultCallack OnFinished;
	OnFinished.BindDynamic(this, &UCPM_PackagePaksJob::HandlePackageFinished);
	UConvaiPakManagerEditorUtils::CPM_PackageProject(Param, OnFinished);
}

FCPM_PakArtifact UCPM_PackagePaksJob::ArtifactFor(const ECPM_Platform Platform) const
{
	FCPM_PakArtifact Artifact;
	Artifact.Platform = Platform;
	Artifact.VersionSlot = FCPM_PakArtifact::VersionSlotFor(Platform);
	Artifact.PakPath = UCPM_UtilityLibrary::GetPakFilePathFromChunkID(Platform, FString::FromInt(Request.ChunkId));
	return Artifact;
}

void UCPM_PackagePaksJob::HandlePackageFinished(const FString& Result, double Runtime)
{
	if (bReported)
	{
		return;
	}

	if (Remaining.IsEmpty())
	{
		// A callback arriving with nothing outstanding means UAT reported twice. Ignoring it is
		// right: acting would package a platform that is not in the queue.
		return;
	}

	const ECPM_Platform Platform = Remaining[0];
	Remaining.RemoveAt(0);

	if (Result == UatCanceled || bCancelled)
	{
		Report(EJobResult::Cancelled, TEXT("packaging was cancelled"));
		return;
	}

	if (Result != UatCompleted)
	{
		Report(EJobResult::Failed,
			FString::Printf(TEXT("packaging %s reported '%s' after %.0fs"), *PlatformName(Platform), *Result, Runtime));
		return;
	}

	const FCPM_PakArtifact Artifact = ArtifactFor(Platform);

	// Checked here rather than at upload: UAT reporting Completed while producing no Pak for this
	// Chunk means the label did not take, and saying so now names the step that actually went wrong.
	if (!FPaths::FileExists(Artifact.PakPath))
	{
		Report(EJobResult::Failed,
			FString::Printf(TEXT("packaging %s completed but no Pak exists at %s"),
				*PlatformName(Platform), *Artifact.PakPath));
		return;
	}

	Built.Add(Artifact);
	PackageNextPlatform();
}

// ---------------------------------------------------------------------------------------------
// UCPM_ArchiveRawProjectJob
// ---------------------------------------------------------------------------------------------

FJobConfig UCPM_ArchiveRawProjectJob::IGetJobConfig_Implementation() const
{
	FJobConfig Config;
	Config.Name = TEXT("Archiving project");
	Config.Description = TEXT("Archives the creator's project for the raw Version.");
	Config.TimeoutSeconds = 0.0f;
	Config.MaxRetries = 0;
	return Config;
}

FJobIOSpec UCPM_ArchiveRawProjectJob::IDeclareIO_Implementation() const
{
	FJobIOSpec Spec;
	Spec.Produces<FCPM_RawArchive>();
	return Spec;
}

void UCPM_ArchiveRawProjectJob::IExecute_Implementation()
{
	ZipPath = UCPM_UtilityLibrary::CPM_GetRawProjectZipPath();

	ReportProgress(TEXT("Archiving project"), 0.0f);

	FOnUatTaskResultCallack OnFinished;
	OnFinished.BindDynamic(this, &UCPM_ArchiveRawProjectJob::HandleArchiveFinished);
	UConvaiPakManagerEditorUtils::CPM_CreateZipAsync(
		ZipPath,
		UCPM_UtilityLibrary::GetProjectFilesToZip(),
		UCPM_UtilityLibrary::GetProjectDirectoriesToZip(),
		OnFinished);
}

void UCPM_ArchiveRawProjectJob::HandleArchiveFinished(const FString& Result, double Runtime)
{
	if (bCancelled)
	{
		Report(EJobResult::Cancelled, TEXT("archiving was cancelled"));
		return;
	}

	if (Result != TEXT("Success"))
	{
		Report(EJobResult::Failed, FString::Printf(TEXT("archiving the project reported '%s'"), *Result));
		return;
	}

	FCPM_RawArchive Archive;
	Archive.ZipPath = ZipPath;

	TArray<FInstancedStruct> Outputs;
	Outputs.Add(FInstancedStruct::Make(Archive));
	Report(EJobResult::Success, FString(), MoveTemp(Outputs));
}

// ---------------------------------------------------------------------------------------------
// UCPM_CreateAssetJob
// ---------------------------------------------------------------------------------------------

FJobConfig UCPM_CreateAssetJob::IGetJobConfig_Implementation() const
{
	FJobConfig Config;
	Config.Name = TEXT("Creating asset");
	Config.Description = TEXT("Creates or updates this Chunk's Asset on Convai and takes back its upload URLs.");
	Config.TimeoutSeconds = 120.0f;

	// Not retried. This either creates an Asset or updates one, and a retry after a response that
	// was sent but not received would create a second Asset for the same Chunk - leaving the first
	// orphaned, because a Chunk records only one AssetID.
	Config.MaxRetries = 0;
	return Config;
}

FJobIOSpec UCPM_CreateAssetJob::IDeclareIO_Implementation() const
{
	FJobIOSpec Spec;
	Spec.Requires<FCPM_PublishRequest>();
	Spec.Produces<FCPM_PublishedAsset>();
	return Spec;
}

void UCPM_CreateAssetJob::IExecute_Implementation()
{
	FCPM_PublishRequest Request;
	if (!TryGetRequest(Request))
	{
		Report(EJobResult::Failed, TEXT("no publish request in the workflow context"));
		return;
	}

	FCPM_ModdingMetadata Modding;
	UCPM_UtilityLibrary::GetModdingMetadataForChunk(Request.ChunkId, Modding);

	// Composed first: what goes on the wire is this Chunk's Draft laid over what this backend last
	// echoed back, complete, whatever version of the Pak Manager last wrote either. Failed on rather
	// than logged, here where nothing has been sent yet and so nothing can be orphaned.
	if (!ConvaiPakManager::Chunk::ComposePakMetadata(Request.ChunkId, Request.EnvironmentSlug))
	{
		Report(EJobResult::Failed, TEXT("this Chunk's asset metadata could not be composed; see the log for which file"));
		return;
	}

	FCPM_CreatePakAssetParams Params;
	FFileHelper::LoadFileToString(Params.MetaData,
		*ConvaiPakManager::Chunk::GetPakMetadataPath(Request.ChunkId, Request.EnvironmentSlug));
	if (Params.MetaData.IsEmpty())
	{
		Report(EJobResult::Failed, TEXT("this Chunk has no asset metadata to publish"));
		return;
	}

	Params.Entity_Type = Modding.AssetType.ToLower();

	// The upload endpoint rejects a request with no tags.
	Params.Tags = PakTagsFor(Modding.AssetType);
	Params.Version = FCPM_PakArtifact::VersionSlotFor(
		Request.Policy.PlatformsToPackage().IsEmpty() ? ECPM_Platform::Windows : Request.Policy.PlatformsToPackage()[0]);
	RequestedVersion = Params.Version;
	Params.Thumbnail = UCPM_UtilityLibrary::CPM_LoadTexture2DFromDisk(
		ConvaiPakManager::Chunk::GetThumbnailPath(Request.ChunkId));

	ExistingAssetId = ConvaiPakManager::Chunk::ReadAssetId(Request.ChunkId, Request.EnvironmentSlug);

	ReportProgress(ExistingAssetId.IsEmpty() ? TEXT("Creating asset") : TEXT("Updating asset"), 0.0f);

	if (ExistingAssetId.IsEmpty())
	{
		CreateProxy = UCPM_CreatePakAssetProxy::CreatePakAssetProxy(
			Params, Request.ChunkId, Request.EnvironmentSlug);
		if (!CreateProxy)
		{
			Report(EJobResult::Failed, TEXT("could not build the create-asset request"));
			return;
		}
		CreateProxy->OnSuccess.AddDynamic(this, &UCPM_CreateAssetJob::HandleCreated);
		CreateProxy->OnFailure.AddDynamic(this, &UCPM_CreateAssetJob::HandleCreateFailed);
		CreateProxy->Activate();
	}
	else
	{
		UpdateProxy = UCPM_UpdatePakAssetProxy::UpdatePakAssetProxy(ExistingAssetId, Params);
		if (!UpdateProxy)
		{
			Report(EJobResult::Failed, TEXT("could not build the update-asset request"));
			return;
		}
		UpdateProxy->OnSuccess.AddDynamic(this, &UCPM_CreateAssetJob::HandleUpdated);
		UpdateProxy->OnFailure.AddDynamic(this, &UCPM_CreateAssetJob::HandleUpdateFailed);
		UpdateProxy->Activate();
	}
}

void UCPM_CreateAssetJob::ICancel_Implementation(const bool bForce)
{
	bCancelled = true;
	Report(EJobResult::Cancelled, TEXT("cancelled"));
}

void UCPM_CreateAssetJob::HandleCreated(const FCPM_CreatedAssets& Response)
{
	if (Response.Assets.IsEmpty())
	{
		Report(EJobResult::Failed, TEXT("the server created no asset"));
		return;
	}

	const FCPM_Asset& Created = Response.Assets[0];

	FCPM_PublishedAsset Published;
	Published.AssetId = Created.Asset.AssetId;
	Published.RawResponse = CreateProxy ? CreateProxy->GetResponseString() : FString();

	// Re-keyed by the Version asked for: the server files the URL under what the artefact is
	// ("scene_asset"), which is not what the upload step has to look one up by. One call names one
	// Version, so there is one URL here whatever it is called.
	if (const auto Minted = Created.UploadUrls.UploadURLsMap.CreateConstIterator())
	{
		Published.UploadUrlsByVersion.Add(RequestedVersion, Minted->Value);
	}
	else
	{
		UCPM_UtilityLibrary::CPM_LogMessage(
			FString::Printf(TEXT("assets/upload minted no URL for version '%s'. The server said: %s"),
				*RequestedVersion, *Published.RawResponse),
			ECPM_LogLevel::Error);
	}

	if (Published.AssetId.IsEmpty())
	{
		Report(EJobResult::Failed, TEXT("the server returned an asset with no id"));
		return;
	}

	TArray<FInstancedStruct> Outputs;
	Outputs.Add(FInstancedStruct::Make(Published));
	Report(EJobResult::Success, FString(), MoveTemp(Outputs));
}

void UCPM_CreateAssetJob::HandleCreateFailed(const FCPM_CreatedAssets& Response)
{
	Report(EJobResult::Failed, TEXT("the server refused to create the asset"));
}

void UCPM_CreateAssetJob::HandleUpdated(const FString& MintedUrl)
{
	// The proxy has already pulled the URL out of the response, so what arrives here is the URL
	// minted for the one Version this request named - filed under that Version, because the key the
	// server used names the artefact rather than the Version.
	// RawResponse is deliberately left empty: it is what the record of this Chunk's AssetID gets
	// overwritten with, and an update answers with a URL rather than a body worth recording.
	FCPM_PublishedAsset Published;
	Published.AssetId = ExistingAssetId;
	Published.UploadUrlsByVersion.Add(RequestedVersion, MintedUrl);

	TArray<FInstancedStruct> Outputs;
	Outputs.Add(FInstancedStruct::Make(Published));
	Report(EJobResult::Success, FString(), MoveTemp(Outputs));
}

void UCPM_CreateAssetJob::HandleUpdateFailed(const FString& ResponseString)
{
	Report(EJobResult::Failed, TEXT("the server refused to update the asset"));
}

// ---------------------------------------------------------------------------------------------
// UCPM_UploadArtifactsJob
// ---------------------------------------------------------------------------------------------

void UCPM_UploadArtifactsJob::Configure(const bool bInExpectPaks, const bool bInExpectRawArchive)
{
	bExpectPaks = bInExpectPaks;
	bExpectRawArchive = bInExpectRawArchive;
}

FJobConfig UCPM_UploadArtifactsJob::IGetJobConfig_Implementation() const
{
	FJobConfig Config;
	Config.Name = TEXT("Uploading");
	Config.Description = TEXT("Sends every built artefact to the URL minted for its Version.");

	// No timeout: a Pak is hundreds of megabytes and an upload's honest duration depends on the
	// creator's connection, not on anything we can predict.
	Config.TimeoutSeconds = 0.0f;
	Config.MaxRetries = 0;
	return Config;
}

FJobIOSpec UCPM_UploadArtifactsJob::IDeclareIO_Implementation() const
{
	FJobIOSpec Spec;
	Spec.Requires<FCPM_PublishedAsset>();
	if (bExpectPaks)
	{
		Spec.RequiresMany<FCPM_PakArtifact>();
	}
	if (bExpectRawArchive)
	{
		Spec.Requires<FCPM_RawArchive>();
	}
	return Spec;
}

void UCPM_UploadArtifactsJob::IExecute_Implementation()
{
	UWorkflowContext* Context = CachedWorkflow.GetInterface() ? CachedWorkflow->IGetContext() : nullptr;
	if (!Context)
	{
		Report(EJobResult::Failed, TEXT("no workflow context"));
		return;
	}

	FCPM_PublishedAsset Published;
	if (!Context->TryGet(Published))
	{
		Report(EJobResult::Failed, TEXT("no published asset in the workflow context"));
		return;
	}

	if (bExpectPaks)
	{
		TArray<FCPM_PakArtifact> Paks;
		Context->TryGetMany(Paks);
		for (const FCPM_PakArtifact& Pak : Paks)
		{
			Pending.Add({ Pak.VersionSlot, Pak.PakPath });
		}
	}

	if (bExpectRawArchive)
	{
		FCPM_RawArchive Archive;
		if (Context->TryGet(Archive))
		{
			Pending.Add({ FCPM_PakArtifact::VersionSlotFor(ECPM_Platform::Raw), Archive.ZipPath });
		}
	}

	if (Pending.IsEmpty())
	{
		Report(EJobResult::Failed, TEXT("nothing was built to upload"));
		return;
	}

	// Not resolved up front any more: one call to the Asset API names one Version and is answered
	// with the URL for that Version alone, so every Version past the one the create step named asks
	// for its own when its turn comes. Only the AssetID is needed to ask.
	if (Published.AssetId.IsEmpty())
	{
		Report(EJobResult::Failed, TEXT("no asset id to mint upload URLs against"));
		return;
	}

	TotalUploads = Pending.Num();
	PublishedForUpload = Published;
	UploadNext();
}

void UCPM_UploadArtifactsJob::ICancel_Implementation(const bool bForce)
{
	bCancelled = true;

	if (UploadProxy && UploadProxy->IsRequestInProgress())
	{
		// Cancels the transfer in flight rather than letting hundreds of megabytes finish being sent
		// to an Asset the creator has stopped publishing.
		UploadProxy->CancelRequest();
	}

	Report(EJobResult::Cancelled, TEXT("cancelled"));
}

void UCPM_UploadArtifactsJob::UploadNext()
{
	if (bCancelled || bReported)
	{
		return;
	}

	if (Pending.IsEmpty())
	{
		Report(EJobResult::Success, FString());
		return;
	}

	const FPendingUpload& Next = Pending[0];
	const FString* Url = PublishedForUpload.UploadUrlsByVersion.Find(Next.VersionSlot);
	if (!Url)
	{
		MintUrlForNext();
		return;
	}

	UCPM_UploadPakAssetProxy* Out = nullptr;
	UploadProxy = UCPM_UploadPakAssetProxy::UploadPakAssetProxy(*Url, Next.FilePath, Out);
	if (!UploadProxy)
	{
		Report(EJobResult::Failed, FString::Printf(TEXT("could not start the upload of %s"), *Next.FilePath));
		return;
	}

	UploadProxy->OnProgress.AddDynamic(this, &UCPM_UploadArtifactsJob::HandleUploadProgress);
	UploadProxy->OnSuccess.AddDynamic(this, &UCPM_UploadArtifactsJob::HandleUploadSucceeded);
	UploadProxy->OnFailure.AddDynamic(this, &UCPM_UploadArtifactsJob::HandleUploadFailed);
	UploadProxy->Activate();
}

void UCPM_UploadArtifactsJob::MintUrlForNext()
{
	const FString& VersionSlot = Pending[0].VersionSlot;

	// Only the Version and the AssetID: the Asset already carries the metadata, tags and thumbnail
	// the create step sent, and re-sending them here would let a second request rewrite them.
	FCPM_CreatePakAssetParams Params;
	Params.Version = VersionSlot;

	MintProxy = UCPM_UpdatePakAssetProxy::UpdatePakAssetProxy(PublishedForUpload.AssetId, Params);
	if (!MintProxy)
	{
		Report(EJobResult::Failed,
			FString::Printf(TEXT("could not ask for an upload URL for version '%s'"), *VersionSlot));
		return;
	}

	ReportProgress(FString::Printf(TEXT("Preparing %s"), *VersionSlot),
		TotalUploads > 0 ? static_cast<float>(TotalUploads - Pending.Num()) / static_cast<float>(TotalUploads) : 0.0f);

	MintProxy->OnSuccess.AddDynamic(this, &UCPM_UploadArtifactsJob::HandleMinted);
	MintProxy->OnFailure.AddDynamic(this, &UCPM_UploadArtifactsJob::HandleMintFailed);
	MintProxy->Activate();
}

void UCPM_UploadArtifactsJob::HandleMinted(const FString& MintedUrl)
{
	if (bCancelled || bReported || Pending.IsEmpty())
	{
		return;
	}

	if (!MintedUrl.StartsWith(TEXT("http")))
	{
		Report(EJobResult::Failed,
			FString::Printf(TEXT("the server minted no upload URL for version '%s'; it answered: %s"),
				*Pending[0].VersionSlot, *MintedUrl));
		return;
	}

	PublishedForUpload.UploadUrlsByVersion.Add(Pending[0].VersionSlot, MintedUrl);
	UploadNext();
}

void UCPM_UploadArtifactsJob::HandleMintFailed(const FString& ResponseString)
{
	if (bCancelled || bReported)
	{
		return;
	}

	Report(EJobResult::Failed,
		FString::Printf(TEXT("the server minted no upload URL for version '%s'; it answered: %s"),
			Pending.IsEmpty() ? TEXT("unknown") : *Pending[0].VersionSlot, *ResponseString));
}

void UCPM_UploadArtifactsJob::HandleUploadProgress(const float Progress)
{
	if (Pending.IsEmpty() || TotalUploads <= 0)
	{
		return;
	}

	// Reported across the whole job, not per file: a creator watching a two-file upload should not
	// see the bar reach the end twice.
	const int32 Done = TotalUploads - Pending.Num();
	const float Overall = (static_cast<float>(Done) + FMath::Clamp(Progress, 0.0f, 1.0f)) / static_cast<float>(TotalUploads);
	ReportProgress(FString::Printf(TEXT("Uploading %s"), *Pending[0].VersionSlot), Overall);
}

void UCPM_UploadArtifactsJob::HandleUploadSucceeded(float Progress)
{
	if (Pending.IsEmpty())
	{
		return;
	}
	Pending.RemoveAt(0);
	UploadNext();
}

void UCPM_UploadArtifactsJob::HandleUploadFailed(float Progress)
{
	const FString Version = Pending.IsEmpty() ? FString(TEXT("unknown")) : Pending[0].VersionSlot;
	Report(EJobResult::Failed, FString::Printf(TEXT("uploading version '%s' failed"), *Version));
}

// ---------------------------------------------------------------------------------------------
// UCPM_PersistChunkStateJob
// ---------------------------------------------------------------------------------------------

FJobConfig UCPM_PersistChunkStateJob::IGetJobConfig_Implementation() const
{
	FJobConfig Config;
	Config.Name = TEXT("Recording asset");
	Config.Description = TEXT("Writes this Chunk's record of the Asset it was published as.");
	Config.TimeoutSeconds = 30.0f;
	Config.MaxRetries = 1;
	return Config;
}

FJobIOSpec UCPM_PersistChunkStateJob::IDeclareIO_Implementation() const
{
	FJobIOSpec Spec;
	Spec.Requires<FCPM_PublishRequest>();
	Spec.Requires<FCPM_PublishedAsset>();
	return Spec;
}

void UCPM_PersistChunkStateJob::IExecute_Implementation()
{
	UWorkflowContext* Context = CachedWorkflow.GetInterface() ? CachedWorkflow->IGetContext() : nullptr;
	FCPM_PublishRequest Request;
	FCPM_PublishedAsset Published;
	if (!Context || !Context->TryGet(Request) || !Context->TryGet(Published))
	{
		Report(EJobResult::Failed, TEXT("no published asset to record"));
		return;
	}

	if (Published.RawResponse.IsEmpty())
	{
		// Nothing to write for an update that answered with a URL rather than a body - the Chunk
		// already records the AssetID it was updating.
		Report(EJobResult::Success, FString());
		return;
	}

	if (!ConvaiPakManager::Chunk::WriteCreateAssetData(
		Request.ChunkId, Request.EnvironmentSlug, Published.RawResponse))
	{
		CPM_LOG(Error, TEXT("Published asset %s but could not record it in the project. ")
			TEXT("Without that record the asset cannot be updated or deleted."), *Published.AssetId);
		Report(EJobResult::Failed, TEXT("could not record the published asset in this project"));
		return;
	}

	Report(EJobResult::Success, FString());
}
