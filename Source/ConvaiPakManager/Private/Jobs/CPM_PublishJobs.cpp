// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Jobs/CPM_PublishJobs.h"

#include "CPM_Defination.h"
#include "Chunk/CPM_Chunk.h"
#include "ConvaiPakManagerEditorUtils.h"
#include "HAL/FileManager.h"
#include "ILiveCodingModule.h"
#include "Jobs/CPM_PublishRunner.h"
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
}

namespace ConvaiPakManager::Publish
{
void FillPublishFormFields(
	const FString& AssetType,
	const FCPM_PublishPolicy& Policy,
	const bool bIncludesRawArchive,
	const bool bIsUpdate,
	FCPM_CreatePakAssetParams& OutParams)
{
	const bool bIsScene = AssetType.Equals(TEXT("Scene"), ESearchCase::IgnoreCase);

	// The upload endpoint rejects a request with no tags, and these are the sets products search by:
	// a Scene without ConvaiSim and Background3D publishes fine and is then invisible to the very
	// product it was made for. Taken verbatim from the legacy uploader, whose Assets are the ones
	// already on the server.
	OutParams.Tags = bIsScene
		? TArray<FString>{ TEXT("Pak"), TEXT("ConvaiSim"), TEXT("Background3D"), TEXT("Scene") }
		: TArray<FString>{ TEXT("Pak"), TEXT("Avatar") };
	if (bIncludesRawArchive)
	{
		OutParams.Tags.Add(TEXT("Raw"));
	}

	OutParams.Entity_Type = AssetType.ToLower();

	OutParams.Version = FCPM_PakArtifact::VersionSlotFor(
		Policy.PlatformsToPackage().IsEmpty() ? ECPM_Platform::Windows : Policy.PlatformsToPackage()[0]);

	// Create only. Legacy pinned every new Asset private and never sent the field again, so an
	// update leaves it alone: whoever opened an Asset up on the website did that after the create,
	// and re-sending "private" every publish would quietly shut it again.
	OutParams.Visiblity = bIsUpdate ? FString() : TEXT("private");
}
}

// ---------------------------------------------------------------------------------------------
// UCPM_PublishJobBase
// ---------------------------------------------------------------------------------------------

void UCPM_PublishJobBase::Initialize(UCPM_PublishRunner* InRunner, FCPM_PublishContext* InContext)
{
	Runner = InRunner;
	Context = InContext;
}

void UCPM_PublishJobBase::Cancel(const bool bForce)
{
	bCancelled = true;

	// Reported as Cancelled, never Failed: this report is what the run's outcome is taken from, and
	// a creator who stopped a publish must not be shown one that broke.
	Report(ECPM_PublishResult::Cancelled, TEXT("cancelled"));
}

void UCPM_PublishJobBase::Report(const ECPM_PublishResult Result, const FString& Error)
{
	if (bReported || !Runner)
	{
		return;
	}
	bReported = true;

	Runner->ReportJobFinished(this, Result, Error);
}

void UCPM_PublishJobBase::ReportProgress(const FString& Step, const float Percent)
{
	if (!Runner)
	{
		return;
	}

	Runner->ReportJobProgress(this, Step, Percent);
}

// ---------------------------------------------------------------------------------------------
// UCPM_PackagePaksJob
// ---------------------------------------------------------------------------------------------

// No deadline. A cook is minutes to tens of minutes on a large project and any number we picked
// would be wrong for somebody - TimeoutSeconds stays 0, as the base leaves it.
void UCPM_PackagePaksJob::Execute()
{
	Request = Context->Request;

	Remaining = Request.Policy.PlatformsToPackage();
	if (Remaining.IsEmpty())
	{
		ReportAndRestore(ECPM_PublishResult::Failed, TEXT("the publish policy asks for no platforms"));
		return;
	}

	// Legacy turned Live Coding off before every cook. Kept session-scoped rather than written to the
	// creator's Live Coding preference, which is theirs and not this Job's to change.
	if (ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
		LiveCoding && LiveCoding->IsEnabledForSession())
	{
		LiveCoding->EnableForSession(false);
		bParkedLiveCoding = true;
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
		Context->Paks = Built;
		ReportAndRestore(ECPM_PublishResult::Success, FString());
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
		ReportAndRestore(ECPM_PublishResult::Failed,
			FString::Printf(TEXT("no policy for platform %s"), *PlatformName(Platform)));
		return;
	}

	// The post-UAT existence check below only proves this run built the Pak if nothing was there when
	// it started: a stale Pak surviving a cook that produced nothing is the very case it exists for.
	if (!IFileManager::Get().Delete(*Existing.PakPath, /*RequireExists=*/false, /*EvenReadOnly=*/true))
	{
		ReportAndRestore(ECPM_PublishResult::Failed,
			FString::Printf(TEXT("could not remove the previous %s Pak at %s before packaging; it may be open ")
				TEXT("in another program"), *PlatformName(Platform), *Existing.PakPath));
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
		ReportAndRestore(ECPM_PublishResult::Cancelled, TEXT("packaging was cancelled"));
		return;
	}

	if (Result != UatCompleted)
	{
		ReportAndRestore(ECPM_PublishResult::Failed,
			FString::Printf(TEXT("packaging %s reported '%s' after %.0fs"), *PlatformName(Platform), *Result, Runtime));
		return;
	}

	const FCPM_PakArtifact Artifact = ArtifactFor(Platform);

	// Checked here rather than at upload: UAT reporting Completed while producing no Pak for this
	// Chunk means the label did not take, and saying so now names the step that actually went wrong.
	if (!FPaths::FileExists(Artifact.PakPath))
	{
		ReportAndRestore(ECPM_PublishResult::Failed,
			FString::Printf(TEXT("packaging %s completed but no Pak exists at %s"),
				*PlatformName(Platform), *Artifact.PakPath));
		return;
	}

	Built.Add(Artifact);
	PackageNextPlatform();
}

void UCPM_PackagePaksJob::Cancel(const bool bForce)
{
	RestoreLiveCoding();
	Super::Cancel(bForce);
}

void UCPM_PackagePaksJob::ReportAndRestore(const ECPM_PublishResult Result, const FString& Error)
{
	RestoreLiveCoding();
	Report(Result, Error);
}

void UCPM_PackagePaksJob::RestoreLiveCoding()
{
	if (!bParkedLiveCoding)
	{
		return;
	}
	bParkedLiveCoding = false;

	if (ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME))
	{
		LiveCoding->EnableForSession(true);
	}
}

// ---------------------------------------------------------------------------------------------
// UCPM_ArchiveRawProjectJob
// ---------------------------------------------------------------------------------------------

void UCPM_ArchiveRawProjectJob::Execute()
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
		Report(ECPM_PublishResult::Cancelled, TEXT("archiving was cancelled"));
		return;
	}

	if (Result != TEXT("Success"))
	{
		Report(ECPM_PublishResult::Failed, FString::Printf(TEXT("archiving the project reported '%s'"), *Result));
		return;
	}

	Context->RawArchive.ZipPath = ZipPath;
	Context->bHasRawArchive = true;
	Report(ECPM_PublishResult::Success, FString());
}

// ---------------------------------------------------------------------------------------------
// UCPM_CreateAssetJob
// ---------------------------------------------------------------------------------------------

// Never re-run on its own. This either creates an Asset or updates one, and a second attempt after a
// response that was sent but not received would create a second Asset for the same Chunk - leaving
// the first orphaned, because a Chunk records only one AssetID.
void UCPM_CreateAssetJob::Execute()
{
	const FCPM_PublishRequest& Request = Context->Request;

	ExistingAssetId = ConvaiPakManager::Chunk::ReadAssetId(Request.ChunkId, Request.EnvironmentSlug);

	// Read back before composing, not after: composing lays the Draft over this Chunk's cached copy
	// of the server's document, and another tool may have edited the Asset since that copy was
	// written. Best effort - the publish carries on either way, because a backend that cannot answer
	// is not a reason to refuse to publish to it.
	if (!ExistingAssetId.IsEmpty())
	{
		ReportProgress(TEXT("Checking asset"), 0.0f);
		PreflightProxy = UCPM_GetAssetProxy::GetAssetProxy(
			ExistingAssetId, Request.ChunkId, Request.EnvironmentSlug);
		PreflightProxy->OnSuccess.AddDynamic(this, &UCPM_CreateAssetJob::HandlePreflightFinished);
		PreflightProxy->OnFailure.AddDynamic(this, &UCPM_CreateAssetJob::HandlePreflightFinished);
		PreflightProxy->Activate();
		return;
	}

	ComposeAndSend();
}

void UCPM_CreateAssetJob::HandlePreflightFinished(const FString&)
{
	if (bCancelled || bReported)
	{
		return;
	}

	ComposeAndSend();
}

void UCPM_CreateAssetJob::ComposeAndSend()
{
	const FCPM_PublishRequest& Request = Context->Request;

	FCPM_ModdingMetadata Modding;
	UCPM_UtilityLibrary::GetModdingMetadataForChunk(Request.ChunkId, Modding);

	// Measured from what the earlier Jobs left on disk, not from the Policy that asked for it: the
	// sizes have to describe the artefacts UCPM_UploadArtifactsJob is about to send.
	TMap<ECPM_Platform, int64> ArtifactSizes;
	for (const FCPM_PakArtifact& Pak : Context->Paks)
	{
		const int64 Size = IFileManager::Get().FileSize(*Pak.PakPath);
		if (Size > 0)
		{
			ArtifactSizes.Add(Pak.Platform, Size);
		}
	}
	if (Context->bHasRawArchive)
	{
		const int64 Size = IFileManager::Get().FileSize(*Context->RawArchive.ZipPath);
		if (Size > 0)
		{
			ArtifactSizes.Add(ECPM_Platform::Raw, Size);
		}
	}

	// Composed first: what goes on the wire is this Chunk's Draft laid over what this backend last
	// echoed back, complete, whatever version of the Pak Manager last wrote either. Failed on rather
	// than logged, here where nothing has been sent yet and so nothing can be orphaned.
	if (!ConvaiPakManager::Chunk::ComposePakMetadata(Request.ChunkId, Request.EnvironmentSlug, ArtifactSizes))
	{
		Report(ECPM_PublishResult::Failed, TEXT("this Chunk's asset metadata could not be composed; see the log for which file"));
		return;
	}

	FCPM_CreatePakAssetParams Params;
	FFileHelper::LoadFileToString(Params.MetaData,
		*ConvaiPakManager::Chunk::GetPakMetadataPath(Request.ChunkId, Request.EnvironmentSlug));
	if (Params.MetaData.IsEmpty())
	{
		Report(ECPM_PublishResult::Failed, TEXT("this Chunk has no asset metadata to publish"));
		return;
	}

	// What the archive Job left behind, not the setting it was built from: the tag has to say what
	// UCPM_UploadArtifactsJob is about to send, and that Job reads the same field.
	ConvaiPakManager::Publish::FillPublishFormFields(
		Modding.AssetType, Request.Policy, Context->bHasRawArchive, !ExistingAssetId.IsEmpty(), Params);
	RequestedVersion = Params.Version;
	Params.Thumbnail = UCPM_UtilityLibrary::CPM_LoadTexture2DFromDisk(
		ConvaiPakManager::Chunk::GetThumbnailPath(Request.ChunkId));

	ReportProgress(ExistingAssetId.IsEmpty() ? TEXT("Creating asset") : TEXT("Updating asset"), 0.0f);

	if (ExistingAssetId.IsEmpty())
	{
		CreateProxy = UCPM_CreatePakAssetProxy::CreatePakAssetProxy(
			Params, Request.ChunkId, Request.EnvironmentSlug);
		if (!CreateProxy)
		{
			Report(ECPM_PublishResult::Failed, TEXT("could not build the create-asset request"));
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
			Report(ECPM_PublishResult::Failed, TEXT("could not build the update-asset request"));
			return;
		}
		UpdateProxy->OnSuccess.AddDynamic(this, &UCPM_CreateAssetJob::HandleUpdated);
		UpdateProxy->OnFailure.AddDynamic(this, &UCPM_CreateAssetJob::HandleUpdateFailed);
		UpdateProxy->Activate();
	}
}

void UCPM_CreateAssetJob::Cancel(const bool bForce)
{
	bCancelled = true;
	Report(ECPM_PublishResult::Cancelled, TEXT("cancelled"));
}

void UCPM_CreateAssetJob::HandleCreated(const FCPM_CreatedAssets& Response)
{
	if (Response.Assets.IsEmpty())
	{
		Report(ECPM_PublishResult::Failed, TEXT("the server created no asset"));
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
		// Same reason as assets/update: the answer holds pre-signed URLs, so its size is logged and
		// its contents are not.
		CPM_LOG(Error, TEXT("assets/upload minted no URL for version '%s' (the answer was %d bytes)."),
			*RequestedVersion, Published.RawResponse.Len());
	}

	if (Published.AssetId.IsEmpty())
	{
		Report(ECPM_PublishResult::Failed, TEXT("the server returned an asset with no id"));
		return;
	}

	Context->Published = Published;
	Report(ECPM_PublishResult::Success, FString());
}

void UCPM_CreateAssetJob::HandleCreateFailed(const FCPM_CreatedAssets& Response)
{
	// The status the UI shows cannot say more than "refused"; the log can say which backend refused
	// and how big an answer it gave, which is what separates a rejection from an empty reply.
	CPM_LOG(Error, TEXT("assets/upload refused chunk %d on %s (the answer was %d bytes)."),
		Context->Request.ChunkId, *Context->Request.EnvironmentSlug,
		CreateProxy ? CreateProxy->GetResponseString().Len() : 0);

	Report(ECPM_PublishResult::Failed, TEXT("the server refused to create the asset"));
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

	Context->Published = Published;
	Report(ECPM_PublishResult::Success, FString());
}

void UCPM_CreateAssetJob::HandleUpdateFailed(const FString& ResponseString)
{
	CPM_LOG(Error, TEXT("assets/update refused asset %s on %s (the answer was %d bytes)."),
		*ExistingAssetId, *Context->Request.EnvironmentSlug, ResponseString.Len());

	Report(ECPM_PublishResult::Failed, TEXT("the server refused to update the asset"));
}

// ---------------------------------------------------------------------------------------------
// UCPM_UploadArtifactsJob
// ---------------------------------------------------------------------------------------------

void UCPM_UploadArtifactsJob::Configure(const bool bInExpectPaks, const bool bInExpectRawArchive)
{
	bExpectPaks = bInExpectPaks;
	bExpectRawArchive = bInExpectRawArchive;
}

void UCPM_UploadArtifactsJob::Execute()
{
	const FCPM_PublishedAsset& Published = Context->Published;

	if (bExpectPaks)
	{
		for (const FCPM_PakArtifact& Pak : Context->Paks)
		{
			Pending.Add({ Pak.VersionSlot, Pak.PakPath });
		}
	}

	if (bExpectRawArchive && Context->bHasRawArchive)
	{
		Pending.Add({ FCPM_PakArtifact::VersionSlotFor(ECPM_Platform::Raw), Context->RawArchive.ZipPath });
	}

	if (Pending.IsEmpty())
	{
		Report(ECPM_PublishResult::Failed, TEXT("nothing was built to upload"));
		return;
	}

	// Not resolved up front any more: one call to the Asset API names one Version and is answered
	// with the URL for that Version alone, so every Version past the one the create step named asks
	// for its own when its turn comes. Only the AssetID is needed to ask.
	if (Published.AssetId.IsEmpty())
	{
		Report(ECPM_PublishResult::Failed, TEXT("no asset id to mint upload URLs against"));
		return;
	}

	TotalUploads = Pending.Num();
	PublishedForUpload = Published;
	UploadNext();
}

void UCPM_UploadArtifactsJob::Cancel(const bool bForce)
{
	bCancelled = true;

	if (UploadProxy && UploadProxy->IsRequestInProgress())
	{
		// Cancels the transfer in flight rather than letting hundreds of megabytes finish being sent
		// to an Asset the creator has stopped publishing.
		UploadProxy->CancelRequest();
	}

	Report(ECPM_PublishResult::Cancelled, TEXT("cancelled"));
}

void UCPM_UploadArtifactsJob::UploadNext()
{
	if (bCancelled || bReported)
	{
		return;
	}

	if (Pending.IsEmpty())
	{
		Report(ECPM_PublishResult::Success, FString());
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
		Report(ECPM_PublishResult::Failed, FString::Printf(TEXT("could not start the upload of %s"), *Next.FilePath));
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
		Report(ECPM_PublishResult::Failed,
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
		Report(ECPM_PublishResult::Failed,
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

	Report(ECPM_PublishResult::Failed,
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

	// One line per artefact, not per progress tick - two or three a run, and the only record that a
	// Version's bytes actually landed.
	CPM_LOG(Log, TEXT("Uploaded version '%s' (%lld bytes)."),
		*Pending[0].VersionSlot, IFileManager::Get().FileSize(*Pending[0].FilePath));

	Pending.RemoveAt(0);
	UploadNext();
}

void UCPM_UploadArtifactsJob::HandleUploadFailed(float Progress)
{
	const FString Version = Pending.IsEmpty() ? FString(TEXT("unknown")) : Pending[0].VersionSlot;
	Report(ECPM_PublishResult::Failed, FString::Printf(TEXT("uploading version '%s' failed"), *Version));
}

// ---------------------------------------------------------------------------------------------
// UCPM_PersistChunkStateJob
// ---------------------------------------------------------------------------------------------

void UCPM_PersistChunkStateJob::Execute()
{
	const FCPM_PublishRequest& Request = Context->Request;
	const FCPM_PublishedAsset& Published = Context->Published;

	if (Published.RawResponse.IsEmpty())
	{
		// Nothing to write for an update that answered with a URL rather than a body - the Chunk
		// already records the AssetID it was updating.
		Report(ECPM_PublishResult::Success, FString());
		return;
	}

	// Attempted twice, immediately. This file holds the only copy of the AssetID anywhere in the
	// creator's world and the Asset it names already exists on Convai, so a transient lock on it is
	// worth one more try before the run is failed over it.
	if (!ConvaiPakManager::Chunk::WriteCreateAssetData(Request.ChunkId, Request.EnvironmentSlug, Published.RawResponse)
		&& !ConvaiPakManager::Chunk::WriteCreateAssetData(Request.ChunkId, Request.EnvironmentSlug, Published.RawResponse))
	{
		CPM_LOG(Error, TEXT("Published asset %s but could not record it in the project. ")
			TEXT("Without that record the asset cannot be updated or deleted."), *Published.AssetId);
		Report(ECPM_PublishResult::Failed, TEXT("could not record the published asset in this project"));
		return;
	}

	Report(ECPM_PublishResult::Success, FString());
}
