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
#include "Engine/LevelStreaming.h"
#include "Containers/Ticker.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "ObjectTools.h"
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

		Status.bExists = UCPM_UtilityLibrary::IsPakUsable(Status.PakPath);
		if (Status.bExists)
		{
			Status.LastPackagedTime = IFileManager::Get().GetTimeStamp(*Status.PakPath);
		}
	}
	return Statuses;
}

FDateTime UConvaiPakEditorSubsystem::GetRawArchiveUploadTime(const int32 ChunkId) const
{
	// Gated on the Asset record, so every path that loses it - a delete whose local cleanup the
	// creator had to finish by hand, a migration that could not attribute the old layout - revokes
	// the reuse with it. A record that outlived its Asset would let the next Publish create a new
	// one and skip the archive it has never had.
	const FString RecordedAssetId = GetAssetId(ChunkId);
	if (RecordedAssetId.IsEmpty())
	{
		return FDateTime::MinValue();
	}

	// And gated on the two ways this project can name that Asset agreeing. The create step asks the
	// sole-Chunk helper rather than this Chunk, so a project that has gained a Primary Asset Label
	// since it published reads no Asset there and creates a second one - which has no archive, and
	// must not inherit the authority of the first one's.
	FString PublishWillUpdateAssetId;
	UCPM_UtilityLibrary::GetAssetID(PublishWillUpdateAssetId);
	if (PublishWillUpdateAssetId != RecordedAssetId)
	{
		return FDateTime::MinValue();
	}

	// MinValue for a file that is not there, which is the "never" this answers with.
	return IFileManager::Get().GetTimeStamp(*ConvaiPakManager::Chunk::GetRawArchiveRecordPath(ChunkId));
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

	return ConvaiPakManager::Chunk::ResolveLevelPackage(
		ReadMetadataField(ChunkId, TEXT("level_name")),
		ReadMetadataField(ChunkId, TEXT("root_path")));
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

	const FString EntryPointName = FPaths::GetCleanFilename(PackageName);

	TMap<FString, FString> Fields;
	Fields.Add(TEXT("root_path"), RootPath);
	if (bIsLevel)
	{
		// The whole package path, not the leaf: it is what the Asset API resolves the level by, and
		// a leaf alone cannot name a level in a subfolder.
		Fields.Add(TEXT("level_name"), PackageName);
		Fields.Add(TEXT("blueprint_class"), TEXT("None"));
		Fields.Add(TEXT("blueprint_class_path"), FString());
	}
	else
	{
		Fields.Add(TEXT("level_name"), FString());
		// The exact shape a product resolves the class by. Taken verbatim from a published avatar.
		Fields.Add(TEXT("blueprint_class"),
			FString::Printf(TEXT("/Script/Engine.BlueprintGeneratedClass'%s.%s_C'"), *PackageName, *EntryPointName));
		Fields.Add(TEXT("blueprint_class_path"), PackageName);
	}

	if (!WriteMetadataFields(ChunkId, Fields))
	{
		return false;
	}

	// The rest of the document - project_name, plugin_name, asset_type, content_path, entity_data -
	// follows from these and from the project, and entity_data names the Entry Point just set.
	return ConvaiPakManager::Chunk::NormalizePakMetadata(ChunkId);
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

	// Strictly the source. A path left in the file field used to win on its own, which made
	// "Convai repository" a choice the settings quietly ignored - and the field it lost to was
	// hidden, so nothing on screen said why.
	if (Settings.PolicySource == ECPM_PolicySource::Repository && !Settings.PolicyOverrideFile.IsEmpty())
	{
		CPM_LOG(Warning, TEXT("Ignoring the publish policy override at %s: Policy Source is the Convai repository."),
			*Settings.PolicyOverrideFile);
	}

	if (Settings.PolicySource == ECPM_PolicySource::OverrideFile)
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

	if (Settings.PolicySource == ECPM_PolicySource::OverrideText)
	{
		FCPM_PublishPolicy Policy;
		FString Error;
		const bool bParsed = Policy.ParseFromJson(Settings.PolicyOverrideJson, Error);
		OnResolved(bParsed, Policy, Error);
		return;
	}

	if (Settings.PolicySource == ECPM_PolicySource::OverrideSettings)
	{
		// Validated rather than trusted: these fields are typed by hand, and the two mistakes they
		// can hold - a platform with no configuration, a policy that produces nothing - are the two
		// nothing downstream would notice.
		FString Error;
		const bool bValid = Settings.PolicyOverride.Validate(Error);
		OnResolved(bValid, bValid ? Settings.PolicyOverride : FCPM_PublishPolicy(), Error);
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
	return BeginPolicyRun(ChunkId, /*bPackageOnly=*/false, FCPM_PublishOptions());
}

bool UConvaiPakEditorSubsystem::PublishWithOptions(const int32 ChunkId, const FCPM_PublishOptions& Options)
{
	return BeginPolicyRun(ChunkId, /*bPackageOnly=*/false, Options);
}

bool UConvaiPakEditorSubsystem::Package(const int32 ChunkId)
{
	return BeginPolicyRun(ChunkId, /*bPackageOnly=*/true, FCPM_PublishOptions());
}

bool UConvaiPakEditorSubsystem::PackageWithOptions(const int32 ChunkId, const FCPM_PublishOptions& Options)
{
	return BeginPolicyRun(ChunkId, /*bPackageOnly=*/true, Options);
}

bool UConvaiPakEditorSubsystem::GetPublishPolicy(
	FCPM_PublishPolicy& OutPolicy, FDateTime& OutReadAt, ECPM_PolicyReadState& OutState) const
{
	OutPolicy = CachedPolicy;
	OutReadAt = PolicyReadAt;
	OutState = PolicyState;
	return PolicyState == ECPM_PolicyReadState::Read;
}

void UConvaiPakEditorSubsystem::CachePolicy(const bool bSucceeded, const FCPM_PublishPolicy& Policy)
{
	// A failed read leaves the last good Policy in place but stops calling it current: callers gate
	// on the state, and overwriting it with a default-constructed one would make "packages nothing"
	// indistinguishable from an answer.
	if (bSucceeded)
	{
		CachedPolicy = Policy;
		PolicyReadAt = FDateTime::UtcNow();
	}

	PolicyState = bSucceeded ? ECPM_PolicyReadState::Read : ECPM_PolicyReadState::Failed;
	OnPolicyChanged.Broadcast();
}

void UConvaiPakEditorSubsystem::RefreshPolicy()
{
	if (bPolicyRefreshInFlight)
	{
		return;
	}

	bPolicyRefreshInFlight = true;
	PolicyState = ECPM_PolicyReadState::Reading;
	OnPolicyChanged.Broadcast();

	TWeakObjectPtr<UConvaiPakEditorSubsystem> WeakThis(this);
	ResolvePolicy(INDEX_NONE, [WeakThis](const bool bSucceeded, const FCPM_PublishPolicy& Policy, const FString&)
	{
		if (UConvaiPakEditorSubsystem* Self = WeakThis.Get())
		{
			Self->bPolicyRefreshInFlight = false;
			Self->CachePolicy(bSucceeded, Policy);
		}
	});
}

bool UConvaiPakEditorSubsystem::BeginPolicyRun(const int32 ChunkId, const bool bPackageOnly, const FCPM_PublishOptions& Options)
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

	// The mirror of the guard DeleteAsset already has: a Publish started while a delete is in flight
	// reads records that delete is about to remove, and one started while a content deletion is
	// queued would package Source Packages that are about to go.
	if (DeletingChunkId == ChunkId)
	{
		SetStatus(ChunkId, Refusal, TEXT("this chunk is being deleted"));
		return false;
	}

	// Any Chunk, not just that one: the content about to be deleted is a whole plugin, and Chunks
	// are not confined to one each - a cook started here could be reading packages that vanish
	// underneath it.
	if (PendingContentDeleteChunkId != INDEX_NONE)
	{
		SetStatus(ChunkId, Refusal, TEXT("this project is deleting a plugin's content"));
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
	ResolvePolicy(ChunkId, [WeakThis, ChunkId, bPackageOnly, Options](const bool bSucceeded, const FCPM_PublishPolicy& Policy, const FString& Error)
	{
		UConvaiPakEditorSubsystem* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}

		// This run read the Policy for real, so the display cache learns from it for free. Recorded
		// before the cancel and failure branches: what Convai answered is true regardless of what
		// this particular run went on to do.
		Self->CachePolicy(bSucceeded, Policy);

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

		// The Platform Selection is applied HERE, to the Policy, before the queue is built - so the
		// queue, the Version slots and every Job downstream keep reading one decision. See CONTEXT.md.
		const FCPM_PublishPolicy Effective =
			Options.bOverridePlatforms ? Policy.WithPlatforms(Options.Platforms) : Policy;

		Self->StartPublishWorkflow(ChunkId, Effective, bPackageOnly, Options);
	});

	// Accepted. Whether it succeeds arrives later, as this Chunk's status.
	return true;
}

FWorkflowHandle UConvaiPakEditorSubsystem::StartPublishWorkflow(
	const int32 ChunkId, const FCPM_PublishPolicy& Policy, const bool bPackageOnly, const FCPM_PublishOptions& Options)
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
		// Named apart, because the fix differs: one is a choice this run made and can unmake, the
		// other is what Convai asks of the project.
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Packaging_Failed,
			Options.bOverridePlatforms
				? TEXT("no platform was selected to package")
				: TEXT("the publish policy asks for no platforms"));
		return FWorkflowHandle::Invalid();
	}

	// Decided here rather than by a Job's Precheck, although ADR-0004 points at one for re-running a
	// step: a Precheck would satisfy the archive from the zip still sitting in the cache and pay the
	// upload anyway, and the upload is the half of the cost the creator is skipping.
	const bool bArchiveRaw =
		!bPackageOnly && UCPM_PakManagerSettings::Get().ShouldArchiveRawProject(Policy.bUploadRawProject);

	if (!bPackageOnly && !bHasPaks && !bArchiveRaw)
	{
		// Refused here rather than left to the upload Job's "nothing was built to upload", which
		// names neither the Policy nor the setting the creator would have to change.
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Create_Failed,
			TEXT("this publish would send nothing: the policy asks only for the project archive, and its upload is turned off"));
		return FWorkflowHandle::Invalid();
	}

	if (!bPackageOnly && Policy.bUploadRawProject && !bArchiveRaw)
	{
		// Warned about rather than merely logged, as with a reused Pak: from here on Convai holds
		// either an older archive or none, and nothing downstream of this line can tell.
		UCPM_UtilityLibrary::CPM_LogMessage(
			TEXT("Publishing without the project archive, because Upload Raw Project Archive is off. ")
			TEXT("Convai cannot repackage this asset for a future engine version without it."),
			ECPM_LogLevel::Warning);
	}

	FWorkflowRequest Request;

	if (bHasPaks)
	{
		Request.Jobs.Add(NewObject<UCPM_PackagePaksJob>(this));
	}

	if (!bPackageOnly)
	{
		if (bArchiveRaw)
		{
			Request.Jobs.Add(NewObject<UCPM_ArchiveRawProjectJob>(this));
		}

		Request.Jobs.Add(NewObject<UCPM_CreateAssetJob>(this));

		UCPM_UploadArtifactsJob* Upload = NewObject<UCPM_UploadArtifactsJob>(this);
		// Configured before it joins the queue: IDeclareIO is asked once, at queue build, and must
		// already know whether to require Paks and a raw archive. One bool with the Job above, or the
		// queue either requires an archive nothing produces or zips a project it never sends.
		Upload->Configure(bHasPaks, bArchiveRaw);
		Request.Jobs.Add(Upload);

		Request.Jobs.Add(NewObject<UCPM_PersistChunkStateJob>(this));
	}

	FCPM_PublishRequest PublishRequest;
	PublishRequest.ChunkId = ChunkId;
	PublishRequest.Policy = Policy;
	PublishRequest.bReuseExistingPaks = Options.bReuseExistingPaks;
	Request.Inputs.Add(FInstancedStruct::Make(PublishRequest));

	TWeakObjectPtr<UConvaiPakEditorSubsystem> WeakThis(this);
	Request.OnProgressNative.BindLambda([WeakThis, ChunkId](const FWorkflowStatusInfo& Info)
	{
		if (UConvaiPakEditorSubsystem* Self = WeakThis.Get())
		{
			Self->HandleWorkflowProgress(ChunkId, Info);
		}
	});
	Request.OnFinishedNative.BindLambda([WeakThis, ChunkId, bPackageOnly, bArchiveRaw](const FWorkflowStatusInfo& Info, const FWorkflowResult&)
	{
		if (UConvaiPakEditorSubsystem* Self = WeakThis.Get())
		{
			Self->HandleWorkflowFinished(ChunkId, Info, bPackageOnly, bArchiveRaw);
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

void UConvaiPakEditorSubsystem::HandleWorkflowFinished(
	const int32 ChunkId, const FWorkflowStatusInfo& Info, const bool bPackageOnly, const bool bArchivedRaw)
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
		// Recorded from here rather than from the Job that writes the Asset's record, because what
		// makes it true is the whole queue having finished: the create step writes the AssetID
		// before a byte of the archive is sent, so a Publish cancelled mid-upload leaves an Asset
		// that has an ID and no archive, and reusing THAT is the thing this record exists to refuse.
		if (bArchivedRaw)
		{
			FFileHelper::SaveStringToFile(
				TEXT("This chunk's Convai asset holds a raw project archive uploaded from this project.\r\n")
				TEXT("Delete this file to make the next publish upload the project again.\r\n"),
				*ConvaiPakManager::Chunk::GetRawArchiveRecordPath(ChunkId));
		}

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

bool UConvaiPakEditorSubsystem::DeleteAsset(const int32 ChunkId, const FString& Version, const bool bAlsoDeletePluginContent)
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

	if (DeletingChunkId != INDEX_NONE || PendingContentDeleteChunkId != INDEX_NONE)
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Failed, TEXT("another delete is already in flight"));
		return false;
	}

	// A content delete reaches beyond this Chunk, so it waits for the whole project to be idle -
	// another Chunk's cook reads the very packages it would remove.
	if (bAlsoDeletePluginContent && !ActivePublishes.IsEmpty())
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Failed,
			TEXT("another asset in this project is publishing; its content cannot be deleted while that runs"));
		return false;
	}

	DeleteProxy = UCPM_DeleteAssetProxy::DeleteAssetProxy(AssetId, Version);
	if (!DeleteProxy)
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Failed, TEXT("could not build the delete request"));
		return false;
	}

	DeletingChunkId = ChunkId;
	DeletingVersion = Version;

	// Only a whole-asset delete takes the content: deleting one Version leaves the Asset, and the
	// content is what every remaining Version was built from.
	bDeletingPluginContent = bAlsoDeletePluginContent && Version.IsEmpty();
	SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Begin);

	DeleteProxy->OnSuccess.AddDynamic(this, &UConvaiPakEditorSubsystem::HandleDeleteSucceeded);
	DeleteProxy->OnFailure.AddDynamic(this, &UConvaiPakEditorSubsystem::HandleDeleteFailed);
	DeleteProxy->Activate();
	return true;
}

bool UConvaiPakEditorSubsystem::DeleteVersion(const int32 ChunkId, const ECPM_Platform Platform)
{
	const FString Slot = FCPM_PakArtifact::VersionSlotFor(Platform);
	if (Slot.IsEmpty())
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Failed, TEXT("that platform has no version"));
		return false;
	}

	// Never the whole Asset: an empty Version is what DeleteAsset reads as "all of it", and a
	// platform that failed to name a slot must not fall through into that.
	return DeleteAsset(ChunkId, Slot, /*bAlsoDeletePluginContent=*/false);
}

bool UConvaiPakEditorSubsystem::DeleteBuiltPak(const int32 ChunkId, const ECPM_Platform Platform)
{
	// Mirrors BeginPolicyRun's guards: a cook in flight is writing the very file this would remove,
	// and a queued content delete is about to move the ground under both.
	if (IsRunInFlight(ChunkId) || DeletingChunkId == ChunkId || PendingContentDeleteChunkId != INDEX_NONE)
	{
		CPM_LOG(Warning, TEXT("Refusing to delete the %s pak for chunk %d: this chunk is busy."),
			*UEnum::GetDisplayValueAsText(Platform).ToString(), ChunkId);
		return false;
	}

	const FString PakPath = UCPM_UtilityLibrary::GetPakFilePathFromChunkID(Platform, FString::FromInt(ChunkId));
	if (PakPath.IsEmpty() || !FPaths::FileExists(PakPath))
	{
		// Already the state the caller asked for. Not a failure.
		return true;
	}

	const bool bDeleted = UCPM_UtilityLibrary::CPM_DeleteFileByPath(PakPath);
	if (!bDeleted)
	{
		CPM_LOG(Warning, TEXT("Could not delete the pak at %s. It may be open in another program."), *PakPath);
		return false;
	}

	// Broadcast so the panel re-reads GetPakStatuses: nothing else tells it a file it is describing
	// has gone, and bExists comes from mounting the pak rather than from anything cached here.
	SetStatus(ChunkId, GetChunkStatus(ChunkId).Status);
	return true;
}

int32 UConvaiPakEditorSubsystem::DeleteBuiltPaks(const int32 ChunkId)
{
	// Every platform GetPakStatuses reports, not only the ones the Policy asks for today: a pak
	// orphaned by a policy that changed is exactly the thing worth cleaning up.
	int32 Deleted = 0;
	for (const FCPM_PakPlatformStatus& Status : GetPakStatuses(ChunkId))
	{
		if (Status.bExists && DeleteBuiltPak(ChunkId, Status.Platform))
		{
			++Deleted;
		}
	}
	return Deleted;
}

void UConvaiPakEditorSubsystem::HandleDeleteSucceeded(const FString& ResponseString)
{
	const int32 ChunkId = DeletingChunkId;
	const FString Version = DeletingVersion;
	const bool bWholeAsset = Version.IsEmpty();
	const bool bDeleteContent = bDeletingPluginContent;
	DeletingChunkId = INDEX_NONE;
	DeletingVersion.Reset();
	bDeletingPluginContent = false;
	DeleteProxy = nullptr;

	if (ChunkId == INDEX_NONE)
	{
		return;
	}

	TArray<FString> Undeleted;

	if (bWholeAsset)
	{
		// Everything this Chunk said about the Asset goes with the Asset. Nothing here describes
		// anything any more: the id would offer Update against nothing, and the draft it used to be
		// worth keeping described an Asset the creator has just destroyed.
		ConvaiPakManager::Chunk::ClearAssetRecords(ChunkId, Undeleted);
	}
	else if (Version.Equals(TEXT("raw"), ESearchCase::IgnoreCase))
	{
		// Only the archive record, because only the archive is gone. The Asset survives a Version
		// delete, so this file is the one thing that would otherwise keep authorising a reuse.
		const FString ArchiveRecordPath = ConvaiPakManager::Chunk::GetRawArchiveRecordPath(ChunkId);
		if (!IFileManager::Get().Delete(*ArchiveRecordPath, /*RequireExists=*/false, /*EvenReadOnly=*/true))
		{
			Undeleted.Add(ArchiveRecordPath);
		}
	}

	// Reported as a failure although the server did delete: a record left on disk describes an Asset
	// that no longer exists, and the next Publish would believe it.
	if (!Undeleted.IsEmpty())
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Failed,
			FString::Printf(TEXT("the delete succeeded on Convai but %s could not be cleared; remove it by hand"),
				*FString::Join(Undeleted, TEXT(", "))));
		return;
	}

	// Broadcast after the records are cleared, so a UI refreshing on this status reads Draft.
	SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Success);

	if (bDeleteContent)
	{
		// NEVER from inside this callback. Deleting assets collects garbage and may change the open
		// map, and this function is running inside the delete request's own response dispatch with
		// DeleteProxy already cleared - so a collection here destroys the proxy whose callback is
		// still on the stack. That crashed the editor in the allocator, one ensure removed from the
		// actual cause. One tick later there is no request in flight to corrupt.
		PendingContentDeleteChunkId = ChunkId;

		TWeakObjectPtr<UConvaiPakEditorSubsystem> WeakThis(this);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, ChunkId](float)
		{
			if (UConvaiPakEditorSubsystem* Self = WeakThis.Get())
			{
				Self->PendingContentDeleteChunkId = INDEX_NONE;
				Self->DeletePluginContent(ChunkId);
			}
			return false;
		}), 0.0f);
	}
}

void UConvaiPakEditorSubsystem::DeletePluginContent(const int32 ChunkId)
{
	FCPM_ModdingMetadata Modding;
	UCPM_UtilityLibrary::GetModdingMetadataForChunk(ChunkId, Modding);
	if (Modding.PluginName.IsEmpty())
	{
		// Refused rather than guessed. The mount root is the only thing bounding what gets deleted,
		// and a guessed one could name the whole project.
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Failed,
			TEXT("the asset was deleted on Convai, but this chunk records no plugin, so its content was left alone"));
		return;
	}

	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Failed,
			TEXT("the asset was deleted on Convai, but its content could not be read"));
		return;
	}

	const FName MountRoot(*FString::Printf(TEXT("/%s"), *Modding.PluginName));
	TArray<FAssetData> Assets;
	AssetRegistry->GetAssetsByPath(MountRoot, Assets, /*bRecursive=*/true);

	// The label is what makes this Chunk exist and what the Pak Manager lists it by, so deleting it
	// would take the Chunk out of the tool along with the content the creator asked to clear.
	FName LabelPackage;
	for (const FCPM_Chunk& Chunk : ConvaiPakManager::Chunk::Discover())
	{
		if (Chunk.Id == ChunkId)
		{
			LabelPackage = Chunk.LabelPackage;
			break;
		}
	}
	Assets.RemoveAll([&LabelPackage](const FAssetData& Asset) { return Asset.PackageName == LabelPackage; });

	if (Assets.IsEmpty())
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Success);
		return;
	}

	// Every world the editor is holding open, not just the persistent one: the engine refuses to
	// delete a level that is loaded as a streaming sublevel or a Level Instance too, and it refuses
	// the WHOLE batch rather than that one asset.
	TSet<FName> OpenWorlds;
	if (const UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr)
	{
		OpenWorlds.Add(EditorWorld->GetOutermost()->GetFName());
		for (const ULevelStreaming* Streaming : EditorWorld->GetStreamingLevels())
		{
			if (const ULevel* Loaded = Streaming ? Streaming->GetLoadedLevel() : nullptr)
			{
				OpenWorlds.Add(Loaded->GetOutermost()->GetFName());
			}
		}
	}

	// A Scene's Entry Point is usually the open map, so this is the ordinary case rather than the
	// corner one. The blank map takes the sublevels with it.
	if (Assets.ContainsByPredicate([&OpenWorlds](const FAssetData& Asset) { return OpenWorlds.Contains(Asset.PackageName); }))
	{
		UEditorLoadingAndSavingUtils::NewBlankMap(/*bSaveExistingMap=*/false);
	}

	// The editor's own delete, not a file remove: it closes asset editors and unloads the objects.
	// Deleting the files underneath a loaded package would leave the editor holding objects whose
	// packages no longer exist.
	const int32 Requested = Assets.Num();
	const int32 Deleted = ObjectTools::DeleteAssets(Assets, /*bShowConfirmation=*/false);

	// Reported, because without the confirmation dialog the engine deletes NOTHING when anything in
	// the set is still referenced from outside it - it answers 0, and the creator would otherwise be
	// told their content was deleted while all of it is still there.
	if (Deleted < Requested)
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Failed,
			FString::Printf(
				TEXT("the asset was deleted on Convai, but %d of %d source packages were kept - something outside %s ")
				TEXT("still references them. Delete them from the Content Browser to see what."),
				Requested - Deleted, Requested, *MountRoot.ToString()));
		return;
	}

	CPM_LOG(Warning, TEXT("Deleted %d source package(s) from chunk %d's plugin; its asset label was kept."),
		Deleted, ChunkId);

	// Re-broadcast so the form re-reads a Chunk whose Entry Point has just gone. The UI toasts only
	// what was busy, and this Chunk stopped being busy when the delete succeeded, so this refreshes
	// without saying "deleted" a second time.
	SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Success);
}

void UConvaiPakEditorSubsystem::HandleDeleteFailed(const FString& ResponseString)
{
	const int32 ChunkId = DeletingChunkId;
	DeletingChunkId = INDEX_NONE;
	DeletingVersion.Reset();
	bDeletingPluginContent = false;
	DeleteProxy = nullptr;

	if (ChunkId != INDEX_NONE)
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Failed, TEXT("the server refused to delete the asset"));
	}
}
