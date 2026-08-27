// Fill out your copyright notice in the Description page of Project Settings.


#include "ConvaiPakEditorSubsystem.h"

#include "CPM_PakManagerSettings.h"
#include "Chunk/CPM_Chunk.h"
#include "ConvaiPakManagerEditorUtils.h"
#include "Core/WorkflowManagerSubsystem.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "EngineUtils.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "IContentBrowserSingleton.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Jobs/CPM_PublishJobs.h"
#include "Misc/FileHelper.h"
#include "Proxy/CPM_Proxy.h"
#include "Publish/CPM_PolicyRequest.h"
#include "Utility/CPM_Log.h"
#include "Utility/CPM_UtilityLibrary.h"

void UConvaiPakEditorSubsystem::Deinitialize()
{
	// Workflows outlive this subsystem otherwise, and their jobs hold raw callbacks back into it.
	for (const TPair<int32, FWorkflowHandle>& Active : ActivePublishes)
	{
		if (UWorkflowManagerSubsystem* Manager = UWorkflowManagerSubsystem::Get())
		{
			Manager->ICancelWorkflow(Active.Value, /*bForce=*/true);
		}
	}
	ActivePublishes.Reset();

	Super::Deinitialize();
}

void UConvaiPakEditorSubsystem::GetSelectedAssetPackageName(FString& PackageName)
{
	// Asked of the Content Browser rather than UEditorUtilityLibrary, which lives in Blutility - the
	// module that existed here only to host the Editor Utility Widget this plugin no longer has.
	FContentBrowserModule& ContentBrowser =
		FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	TArray<FAssetData> SelectedAssets;
	ContentBrowser.Get().GetSelectedAssets(SelectedAssets);
	if (SelectedAssets.Num() > 0)
	{
		PackageName = SelectedAssets[0].PackageName.ToString();
	}
}

TArray<int32> UConvaiPakEditorSubsystem::GetChunkIds() const
{
	TArray<int32> Ids;
	for (const FCPM_Chunk& Chunk : ConvaiPakManager::Chunk::Discover())
	{
		Ids.Add(Chunk.Id);
	}
	return Ids;
}

FCPM_ChunkStatus UConvaiPakEditorSubsystem::GetChunkStatus(const int32 ChunkId) const
{
	if (const FCPM_ChunkStatus* Status = StatusByChunk.Find(ChunkId))
	{
		return *Status;
	}

	FCPM_ChunkStatus Idle;
	Idle.ChunkId = ChunkId;
	return Idle;
}

FString UConvaiPakEditorSubsystem::GetAssetId(const int32 ChunkId) const
{
	FString AssetId;
	FString Contents;
	if (!FFileHelper::LoadFileToString(Contents, *ConvaiPakManager::Chunk::GetCreateAssetDataPath(ChunkId)))
	{
		return AssetId;
	}

	FCPM_CreatedAssets Created;
	if (UCPM_UtilityLibrary::GetCreatedAssetsFromJSON(Contents, Created) && !Created.Assets.IsEmpty())
	{
		AssetId = Created.Assets[0].Asset.AssetId;
	}
	return AssetId;
}

bool UConvaiPakEditorSubsystem::CanAddAnotherChunk() const
{
	return !UCPM_PakManagerSettings::Get().IsAtChunkLimit(GetChunkIds().Num());
}

ECPM_AssetType UConvaiPakEditorSubsystem::GetAssetType() const
{
	return UCPM_UtilityLibrary::GetAssetType();
}

TArray<FCPM_PakPlatformStatus> UConvaiPakEditorSubsystem::GetPakStatuses(const int32 ChunkId) const
{
	TArray<FCPM_PakPlatformStatus> Statuses;
	for (const ECPM_Platform Platform : { ECPM_Platform::Windows, ECPM_Platform::Linux })
	{
		FCPM_PakPlatformStatus& Status = Statuses.AddDefaulted_GetRef();
		Status.Platform = Platform;
		Status.PakPath = UCPM_UtilityLibrary::GetPakFilePathFromChunkID(Platform, FString::FromInt(ChunkId));

		// Size first: ValidatePakFile mounts the Pak, which is not worth doing on a file that is
		// absent or empty - and it logs an error for a missing file, which "not packaged yet" is not.
		const int64 Size = IFileManager::Get().FileSize(*Status.PakPath);
		Status.bExists = Size > 0 && UCPM_UtilityLibrary::ValidatePakFile(Status.PakPath);
		if (Status.bExists)
		{
			Status.LastPackagedTime = IFileManager::Get().GetTimeStamp(*Status.PakPath);
		}
	}
	return Statuses;
}

FCPM_SpawnPointStatus UConvaiPakEditorSubsystem::GetSpawnPointStatus() const
{
	FCPM_SpawnPointStatus Status;

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return Status;
	}

	// The tag AddSpawnPoint writes. FName makes "EditorSpawn" and "editorspawn" the same tag.
	static const FName EditorSpawnTag(TEXT("EditorSpawn"));

	const AActor* Sole = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const AActor* Actor = *It;
		if (IsValid(Actor) && !Actor->IsPendingKillPending() && Actor->ActorHasTag(EditorSpawnTag))
		{
			++Status.Count;
			Sole = Actor;
		}
	}

	if (Status.Count == 1)
	{
		Status.Transform = Sole->GetActorTransform();
	}
	return Status;
}

namespace
{
	/** Field names as the Convai asset metadata document spells them. */
	const TCHAR* AssetNameField = TEXT("asset_name");
	const TCHAR* AssetDescriptionField = TEXT("asset_description");

	FString ReadMetadataField(const int32 ChunkId, const TCHAR* Field)
	{
		FString Contents;
		if (!FFileHelper::LoadFileToString(Contents, *ConvaiPakManager::Chunk::GetPakMetadataPath(ChunkId)))
		{
			return FString();
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Contents);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return FString();
		}

		FString Value;
		Root->TryGetStringField(Field, Value);
		return Value;
	}

	/**
	 * Sets fields and writes the document back.
	 *
	 * Read-modify-write over the parsed document rather than serialising a struct: this metadata is
	 * the server's schema, not ours, and carries fields the Pak Manager has no type for -
	 * avatar_config, entity_data, gender. Rebuilding it from what we happen to model would silently
	 * drop whatever we do not.
	 */
	bool WriteMetadataFields(const int32 ChunkId, const TMap<FString, FString>& Fields)
	{
		const FString Path = ConvaiPakManager::Chunk::GetPakMetadataPath(ChunkId);

		TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
		FString Contents;
		if (FFileHelper::LoadFileToString(Contents, *Path))
		{
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Contents);
			TSharedPtr<FJsonObject> Parsed;
			if (FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid())
			{
				Root = Parsed;
			}
			else
			{
				// Refused rather than started fresh: overwriting a metadata document we could not
				// parse would discard whatever it held, and it holds things nothing else has a copy of.
				CPM_LOG(Error, TEXT("Refusing to edit %s: it is not valid JSON."), *Path);
				return false;
			}
		}

		for (const TPair<FString, FString>& Field : Fields)
		{
			Root->SetStringField(Field.Key, Field.Value);
		}

		FString Serialised;
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Serialised);
		if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
		{
			return false;
		}

		return FFileHelper::SaveStringToFile(Serialised, *Path);
	}
}

FString UConvaiPakEditorSubsystem::GetAssetName(const int32 ChunkId) const
{
	return ReadMetadataField(ChunkId, AssetNameField);
}

bool UConvaiPakEditorSubsystem::SetAssetName(const int32 ChunkId, const FString& Name)
{
	return WriteMetadataFields(ChunkId, { { AssetNameField, Name } });
}

FString UConvaiPakEditorSubsystem::GetAssetDescription(const int32 ChunkId) const
{
	return ReadMetadataField(ChunkId, AssetDescriptionField);
}

bool UConvaiPakEditorSubsystem::SetAssetDescription(const int32 ChunkId, const FString& Description)
{
	return WriteMetadataFields(ChunkId, { { AssetDescriptionField, Description } });
}

FString UConvaiPakEditorSubsystem::GetEntryPoint(const int32 ChunkId) const
{
	// An Avatar records a blueprint, a Scene records a level; whichever is set is the Entry Point.
	const FString BlueprintPath = ReadMetadataField(ChunkId, TEXT("blueprint_class_path"));
	if (!BlueprintPath.IsEmpty())
	{
		return BlueprintPath;
	}

	// A level is recorded by short name - that is what a product loads it by - and root_path is the
	// MOUNT ROOT ("/Game/") rather than its folder, so gluing the two together would invent a path
	// that drops any subfolder. The package is found again through the registry instead.
	const FString LevelName = ReadMetadataField(ChunkId, TEXT("level_name"));
	if (LevelName.IsEmpty())
	{
		return LevelName;
	}

	FARFilter Filter;
	Filter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	FString RootPath = ReadMetadataField(ChunkId, TEXT("root_path"));
	RootPath.RemoveFromEnd(TEXT("/"));
	if (!RootPath.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*RootPath));
	}

	TArray<FAssetData> Levels;
	if (const IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		AssetRegistry->GetAssets(Filter, Levels);
	}
	for (const FAssetData& Level : Levels)
	{
		if (Level.AssetName.ToString() == LevelName)
		{
			return Level.PackageName.ToString();
		}
	}

	// No longer on disk. The short name still says an Entry Point was picked, and which one.
	return LevelName;
}

bool UConvaiPakEditorSubsystem::SetEntryPoint(const int32 ChunkId, const FString& PackageName)
{
	if (PackageName.IsEmpty())
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Update_Failed, TEXT("no asset was picked"));
		return false;
	}

	const IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		return false;
	}

	TArray<FAssetData> Assets;
	AssetRegistry->GetAssetsByPackageName(FName(*PackageName), Assets);
	if (Assets.IsEmpty())
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Update_Failed,
			FString::Printf(TEXT("nothing exists at %s"), *PackageName));
		return false;
	}

	const FAssetData& Asset = Assets[0];
	const bool bIsLevel = Asset.AssetClassPath == UWorld::StaticClass()->GetClassPathName();
	const bool bIsBlueprint = Asset.AssetClassPath == UBlueprint::StaticClass()->GetClassPathName();

	FCPM_ModdingMetadata Modding;
	UCPM_UtilityLibrary::GetModdingMetadata(Modding);
	const bool bWantsLevel = Modding.AssetType.Equals(TEXT("Scene"), ESearchCase::IgnoreCase);

	// Checked here rather than discovered on the server: an Avatar whose entry point is a level, or a
	// Scene whose entry point is a blueprint, publishes an Asset no product can open - and nothing
	// between here and there would notice.
	if (bWantsLevel && !bIsLevel)
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Update_Failed,
			FString::Printf(TEXT("a scene's entry point must be a level, and %s is not"), *PackageName));
		return false;
	}
	if (!bWantsLevel && !bIsBlueprint)
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Update_Failed,
			FString::Printf(TEXT("an avatar's entry point must be a blueprint, and %s is not"), *PackageName));
		return false;
	}

	// "/AGXRDJZ.../Maps/Landing" -> "/AGXRDJZ.../" : the mount point the package lives under.
	FString RootPath = PackageName;
	{
		int32 SecondSlash = INDEX_NONE;
		if (PackageName.FindChar(TEXT('/'), SecondSlash) && PackageName.Len() > 1)
		{
			const int32 Next = PackageName.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 1);
			RootPath = Next != INDEX_NONE ? PackageName.Left(Next + 1) : PackageName;
		}
	}

	const FString AssetName = FPaths::GetCleanFilename(PackageName);

	TMap<FString, FString> Fields;
	Fields.Add(TEXT("root_path"), RootPath);
	if (bIsLevel)
	{
		Fields.Add(TEXT("level_name"), AssetName);
		Fields.Add(TEXT("blueprint_class"), FString());
		Fields.Add(TEXT("blueprint_class_path"), FString());
	}
	else
	{
		Fields.Add(TEXT("level_name"), FString());
		// The exact shape a product resolves the class by. Taken verbatim from a published avatar.
		Fields.Add(TEXT("blueprint_class"),
			FString::Printf(TEXT("/Script/Engine.BlueprintGeneratedClass'%s.%s_C'"), *PackageName, *AssetName));
		Fields.Add(TEXT("blueprint_class_path"), PackageName);
	}

	if (ReadMetadataField(ChunkId, TEXT("content_path")).IsEmpty())
	{
		Fields.Add(TEXT("content_path"),
			FString::Printf(TEXT("../../../%s/Content/"), *UCPM_UtilityLibrary::GetProjectName()));
	}
	Fields.Add(TEXT("project_name"), UCPM_UtilityLibrary::GetProjectName());
	Fields.Add(TEXT("plugin_name"), Modding.PluginName);
	Fields.Add(TEXT("asset_type"), Modding.AssetType.ToLower());

	return WriteMetadataFields(ChunkId, Fields);
}

bool UConvaiPakEditorSubsystem::PickEntryPointFromSelection(const int32 ChunkId)
{
	FString PackageName;
	GetSelectedAssetPackageName(PackageName);
	return SetEntryPoint(ChunkId, PackageName);
}

AActor* UConvaiPakEditorSubsystem::AddSpawnPoint()
{
	AActor* SpawnPoint = UConvaiPakManagerEditorUtils::SpawnAndSnapActorToView(ATargetPoint::StaticClass());
	if (!SpawnPoint)
	{
		return nullptr;
	}

	// The tag is the whole contract: it is how a Convai product finds where to put the avatar. The
	// actor's class is incidental.
	SpawnPoint->Tags.AddUnique(TEXT("EditorSpawn"));
	return SpawnPoint;
}

bool UConvaiPakEditorSubsystem::SetSpawnPointFromViewport()
{
	if (GetSpawnPointStatus().Count > 1)
	{
		return false;
	}

	// SpawnAndSnapActorToView already snaps the sole tagged actor to the view instead of spawning a
	// second one, so add and move share one path.
	return AddSpawnPoint() != nullptr;
}

FString UConvaiPakEditorSubsystem::GetThumbnailPath(const int32 ChunkId) const
{
	return ConvaiPakManager::Chunk::GetThumbnailPath(ChunkId);
}

bool UConvaiPakEditorSubsystem::CaptureThumbnail(const int32 ChunkId)
{
	const FString Path = ConvaiPakManager::Chunk::GetThumbnailPath(ChunkId);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	return UConvaiPakManagerEditorUtils::CPM_TakeViewportScreenshot(Path);
}

void UConvaiPakEditorSubsystem::SetStatus(
	const int32 ChunkId,
	const ECPM_AssetManagerStatus Status,
	const FString& Message,
	const float Progress,
	const FString& StepName)
{
	FCPM_ChunkStatus& Stored = StatusByChunk.FindOrAdd(ChunkId);
	Stored.ChunkId = ChunkId;
	Stored.Status = Status;
	Stored.Message = Message;
	Stored.Progress = Progress;
	Stored.StepName = StepName;

	OnChunkStatusChanged.Broadcast(Stored);
	OnChunkStatusChangedEvent.Broadcast(Stored);
}

void UConvaiPakEditorSubsystem::ResolvePolicy(
	const int32 ChunkId,
	TFunction<void(bool, const FCPM_PublishPolicy&, const FString&)> OnResolved)
{
	const UCPM_PakManagerSettings& Settings = UCPM_PakManagerSettings::Get();

	if (!Settings.PolicyOverrideFile.IsEmpty())
	{
		FString Contents;
		if (!FFileHelper::LoadFileToString(Contents, *Settings.PolicyOverrideFile))
		{
			OnResolved(false, FCPM_PublishPolicy(),
				FString::Printf(TEXT("could not read the publish policy override at %s"), *Settings.PolicyOverrideFile));
			return;
		}

		FCPM_PublishPolicy Policy;
		FString Error;
		const bool bParsed = Policy.ParseFromJson(Contents, Error);
		OnResolved(bParsed, Policy, Error);
		return;
	}

	UCPM_PolicyRequest::Start(
		Settings.PolicyRepository, Settings.PolicyRef, Settings.PolicyPath,
		UCPM_PolicyRequest::FOnPolicyFetched::CreateLambda(
			[OnResolved](const bool bSucceeded, const FString& Contents)
			{
				if (!bSucceeded)
				{
					// Refused rather than fallen back to a cached or compiled-in policy. The policy
					// exists so Convai can change what a Publish produces, so a stale copy is wrong
					// exactly when it matters - and publishing from one yields an Asset missing a
					// Version, which nothing here would notice. See docs/adr/0004.
					OnResolved(false, FCPM_PublishPolicy(), TEXT("the publish policy could not be fetched"));
					return;
				}

				FCPM_PublishPolicy Policy;
				FString Error;
				const bool bParsed = Policy.ParseFromJson(Contents, Error);
				OnResolved(bParsed, Policy, Error);
			}));
}

bool UConvaiPakEditorSubsystem::Publish(const int32 ChunkId)
{
	return BeginPolicyRun(ChunkId, /*bPackageOnly=*/false);
}

bool UConvaiPakEditorSubsystem::Package(const int32 ChunkId)
{
	return BeginPolicyRun(ChunkId, /*bPackageOnly=*/true);
}

bool UConvaiPakEditorSubsystem::BeginPolicyRun(const int32 ChunkId, const bool bPackageOnly)
{
	const ECPM_AssetManagerStatus Refusal =
		bPackageOnly ? ECPM_AssetManagerStatus::Packaging_Failed : ECPM_AssetManagerStatus::Create_Failed;

	if (!GetChunkIds().Contains(ChunkId))
	{
		SetStatus(ChunkId, Refusal,
			FString::Printf(TEXT("no Primary Asset Label in this project declares chunk %d"), ChunkId));
		return false;
	}

	if (IsRunInFlight(ChunkId))
	{
		SetStatus(ChunkId, Refusal, TEXT("this chunk is already publishing"));
		return false;
	}

	SetStatus(ChunkId, ECPM_AssetManagerStatus::Packaging_Begin, FString(), 0.0f, TEXT("Reading publish policy"));

	// Registered before the Policy is asked for, not when the queue exists: the Chunk is busy from
	// this line on, and a second Publish accepted in the meantime would start a second queue.
	PendingPolicyRuns.Add(ChunkId);

	// Deliberately NOT a Job of the publish queue. The policy decides which Jobs exist, and a queue's
	// shape may depend only on what the caller knew before building it - so it is resolved first and
	// the queue is built from the answer. See docs/adr/0004 and the Job System's docs/adr/0009.
	TWeakObjectPtr<UConvaiPakEditorSubsystem> WeakThis(this);
	ResolvePolicy(ChunkId, [WeakThis, ChunkId, bPackageOnly](const bool bSucceeded, const FCPM_PublishPolicy& Policy, const FString& Error)
	{
		UConvaiPakEditorSubsystem* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}

		Self->PendingPolicyRuns.Remove(ChunkId);
		if (Self->CancelledDuringPolicyRead.Remove(ChunkId) > 0)
		{
			Self->SetStatus(ChunkId, ECPM_AssetManagerStatus::Publish_Cancelled);
			return;
		}

		if (!bSucceeded)
		{
			Self->SetStatus(ChunkId, ECPM_AssetManagerStatus::Packaging_Failed, Error);
			return;
		}

		Self->StartPublishWorkflow(ChunkId, Policy, bPackageOnly);
	});

	// Accepted. Whether it succeeds arrives later, as this Chunk's status.
	return true;
}

FWorkflowHandle UConvaiPakEditorSubsystem::StartPublishWorkflow(const int32 ChunkId, const FCPM_PublishPolicy& Policy, const bool bPackageOnly)
{
	UWorkflowManagerSubsystem* Manager = UWorkflowManagerSubsystem::Get();
	if (!Manager)
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Packaging_Failed, TEXT("the job system is unavailable"));
		return FWorkflowHandle::Invalid();
	}

	const bool bHasPaks = !Policy.PlatformsToPackage().IsEmpty();

	if (bPackageOnly && !bHasPaks)
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Packaging_Failed, TEXT("the publish policy asks for no platforms"));
		return FWorkflowHandle::Invalid();
	}

	FWorkflowRequest Request;

	if (bHasPaks)
	{
		Request.Jobs.Add(NewObject<UCPM_PackagePaksJob>(this));
	}

	if (!bPackageOnly)
	{
		if (Policy.bUploadRawProject)
		{
			Request.Jobs.Add(NewObject<UCPM_ArchiveRawProjectJob>(this));
		}

		Request.Jobs.Add(NewObject<UCPM_CreateAssetJob>(this));

		UCPM_UploadArtifactsJob* Upload = NewObject<UCPM_UploadArtifactsJob>(this);
		// Configured before it joins the queue: IDeclareIO is asked once, at queue build, and must
		// already know whether to require Paks and a raw archive.
		Upload->Configure(bHasPaks, Policy.bUploadRawProject);
		Request.Jobs.Add(Upload);

		Request.Jobs.Add(NewObject<UCPM_PersistChunkStateJob>(this));
	}

	FCPM_PublishRequest PublishRequest;
	PublishRequest.ChunkId = ChunkId;
	PublishRequest.Policy = Policy;
	Request.Inputs.Add(FInstancedStruct::Make(PublishRequest));

	TWeakObjectPtr<UConvaiPakEditorSubsystem> WeakThis(this);
	Request.OnProgressNative.BindLambda([WeakThis, ChunkId](const FWorkflowStatusInfo& Info)
	{
		if (UConvaiPakEditorSubsystem* Self = WeakThis.Get())
		{
			Self->HandleWorkflowProgress(ChunkId, Info);
		}
	});
	Request.OnFinishedNative.BindLambda([WeakThis, ChunkId, bPackageOnly](const FWorkflowStatusInfo& Info, const FWorkflowResult&)
	{
		if (UConvaiPakEditorSubsystem* Self = WeakThis.Get())
		{
			Self->HandleWorkflowFinished(ChunkId, Info, bPackageOnly);
		}
	});

	// One planned step per queued Job, named as the Job names itself, filled BEFORE the workflow
	// starts so the first progress broadcast already carries them. Upload stays one step: the job
	// reports one progress stream, and the tracker must not claim granularity it cannot report.
	{
		FCPM_ChunkStatus& Stored = StatusByChunk.FindOrAdd(ChunkId);
		Stored.ChunkId = ChunkId;
		Stored.PlannedSteps.Reset();
		for (const TScriptInterface<IJobInterface>& Job : Request.Jobs)
		{
			Stored.PlannedSteps.Add(IJobInterface::Execute_IGetJobConfig(Job.GetObject()).Name);
		}
		Stored.CurrentStepIndex = INDEX_NONE;
	}

	const FWorkflowHandle Handle = Manager->ICreateWorkflow(Request);
	if (!Handle.IsValid())
	{
		if (FCPM_ChunkStatus* Stored = StatusByChunk.Find(ChunkId))
		{
			Stored->PlannedSteps.Reset();
			Stored->CurrentStepIndex = INDEX_NONE;
		}

		// The queue was rejected before anything ran - a Job requiring something no earlier Job
		// produces. Worth surfacing as-is rather than as a generic failure: it is a wiring bug, and
		// the job system has already logged which type went unsatisfied.
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Packaging_Failed,
			TEXT("the publish steps did not fit together; see the log for which value went unsatisfied"));
		return FWorkflowHandle::Invalid();
	}

	ActivePublishes.Add(ChunkId, Handle);
	return Handle;
}

void UConvaiPakEditorSubsystem::HandleWorkflowProgress(const int32 ChunkId, const FWorkflowStatusInfo& Info)
{
	// Taken from the running Job itself rather than mapped from its display name, so improving the
	// wording cannot silently change what the UI thinks is happening.
	ECPM_AssetManagerStatus Phase = ECPM_AssetManagerStatus::Max;
	if (const UCPM_PublishJobBase* Job = Cast<UCPM_PublishJobBase>(Info.CurrentJob.JobObject.GetObject()))
	{
		Phase = Job->GetPhaseStatus();
	}

	{
		// Queue position maps 1:1 onto PlannedSteps - no groups here, one planned step per Job.
		FCPM_ChunkStatus& Stored = StatusByChunk.FindOrAdd(ChunkId);
		Stored.CurrentStepIndex =
			Stored.PlannedSteps.IsValidIndex(Info.CurrentJob.Index) ? Info.CurrentJob.Index : INDEX_NONE;
	}

	SetStatus(ChunkId, Phase, FString(), Info.Progress, Info.CurrentJob.ProgressText.ToString());
}

void UConvaiPakEditorSubsystem::HandleWorkflowFinished(const int32 ChunkId, const FWorkflowStatusInfo& Info, const bool bPackageOnly)
{
	ActivePublishes.Remove(ChunkId);

	if (FCPM_ChunkStatus* Stored = StatusByChunk.Find(ChunkId))
	{
		Stored->PlannedSteps.Reset();
		Stored->CurrentStepIndex = INDEX_NONE;
	}

	switch (Info.Status)
	{
	case EWorkflowStatus::Completed:
		SetStatus(ChunkId,
			bPackageOnly ? ECPM_AssetManagerStatus::Packaging_Success : ECPM_AssetManagerStatus::UploadPak_Success,
			FString(), 1.0f);
		break;

	case EWorkflowStatus::Cancelled:
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Publish_Cancelled, FString(), Info.Progress);
		break;

	default:
		SetStatus(ChunkId,
			bPackageOnly ? ECPM_AssetManagerStatus::Packaging_Failed : ECPM_AssetManagerStatus::UploadPak_Failed,
			Info.ErrorMessage.IsEmpty()
				? (bPackageOnly ? TEXT("packaging failed") : TEXT("publishing failed"))
				: *Info.ErrorMessage, Info.Progress);
		break;
	}
}

bool UConvaiPakEditorSubsystem::IsRunInFlight(const int32 ChunkId) const
{
	return ActivePublishes.Contains(ChunkId) || PendingPolicyRuns.Contains(ChunkId);
}

bool UConvaiPakEditorSubsystem::CancelPublish(const int32 ChunkId)
{
	const FWorkflowHandle* Handle = ActivePublishes.Find(ChunkId);
	if (!Handle)
	{
		// Still reading the Policy: nothing runs yet, so the cancel is kept and honoured when it answers.
		if (PendingPolicyRuns.Contains(ChunkId))
		{
			CancelledDuringPolicyRead.Add(ChunkId);
			return true;
		}
		return false;
	}

	UWorkflowManagerSubsystem* Manager = UWorkflowManagerSubsystem::Get();
	// Not forced: the running Job is allowed to finish reporting, so an upload in flight unhooks its
	// request rather than being abandoned mid-transfer.
	return Manager && Manager->ICancelWorkflow(*Handle, /*bForce=*/false);
}

bool UConvaiPakEditorSubsystem::DeleteAsset(const int32 ChunkId, const FString& Version)
{
	const FString AssetId = GetAssetId(ChunkId);
	if (AssetId.IsEmpty())
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Failed, TEXT("this chunk has no published asset"));
		return false;
	}

	if (IsRunInFlight(ChunkId))
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Failed, TEXT("this chunk is publishing"));
		return false;
	}

	if (DeletingChunkId != INDEX_NONE)
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Failed, TEXT("another delete is already in flight"));
		return false;
	}

	DeleteProxy = UCPM_DeleteAssetProxy::DeleteAssetProxy(AssetId, Version);
	if (!DeleteProxy)
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Failed, TEXT("could not build the delete request"));
		return false;
	}

	DeletingChunkId = ChunkId;
	bDeletingWholeAsset = Version.IsEmpty();
	SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Begin);

	DeleteProxy->OnSuccess.AddDynamic(this, &UConvaiPakEditorSubsystem::HandleDeleteSucceeded);
	DeleteProxy->OnFailure.AddDynamic(this, &UConvaiPakEditorSubsystem::HandleDeleteFailed);
	DeleteProxy->Activate();
	return true;
}

void UConvaiPakEditorSubsystem::HandleDeleteSucceeded(const FString& ResponseString)
{
	const int32 ChunkId = DeletingChunkId;
	const bool bWholeAsset = bDeletingWholeAsset;
	DeletingChunkId = INDEX_NONE;
	bDeletingWholeAsset = false;
	DeleteProxy = nullptr;

	if (ChunkId == INDEX_NONE)
	{
		return;
	}

	if (bWholeAsset)
	{
		// The Asset is gone from Convai, so the record naming it must go too - a kept AssetId would
		// make the next Publish update something that no longer exists. Name, description and Entry
		// Point live in the pak metadata, not here, so deleting this file keeps the creator's draft.
		const FString RecordPath = ConvaiPakManager::Chunk::GetCreateAssetDataPath(ChunkId);
		const bool bRecordCleared = IFileManager::Get().Delete(*RecordPath, /*RequireExists=*/false, /*EvenReadOnly=*/true);

		// The pak metadata carries server-issued identity beside the draft fields, so the identity
		// is cleared field by field rather than by deleting the file that name and description live in.
		const bool bIdentityCleared = WriteMetadataFields(ChunkId, {
			{ TEXT("scene_id"), FString() },
			{ TEXT("entity_id"), FString() },
			{ TEXT("version"), FString() },
		});

		// Reported as a failure although the server did delete: success with the AssetId still on
		// disk would offer Update against an Asset that no longer exists.
		if (!bRecordCleared || !bIdentityCleared)
		{
			SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Failed,
				FString::Printf(TEXT("the asset was deleted on Convai but its local record could not be cleared; remove %s by hand"), *RecordPath));
			return;
		}
	}

	// Broadcast after the record is cleared, so a UI refreshing on this status reads Draft.
	SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Success);
}

void UConvaiPakEditorSubsystem::HandleDeleteFailed(const FString& ResponseString)
{
	const int32 ChunkId = DeletingChunkId;
	DeletingChunkId = INDEX_NONE;
	bDeletingWholeAsset = false;
	DeleteProxy = nullptr;

	if (ChunkId != INDEX_NONE)
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Failed, TEXT("the server refused to delete the asset"));
	}
}
