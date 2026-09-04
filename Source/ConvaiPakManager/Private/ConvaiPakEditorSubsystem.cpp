// Fill out your copyright notice in the Description page of Project Settings.


#include "ConvaiPakEditorSubsystem.h"

#include "CPM_DependencyCopyAPI.h"
#include "CPM_PakManagerSettings.h"
#include "Avatar/CPM_AvatarBlueprint.h"
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
#include "Interfaces/IPluginManager.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Jobs/CPM_PublishJobs.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Proxy/CPM_Proxy.h"
#include "Publish/CPM_Compatibility.h"
#include "Publish/CPM_PolicyRequest.h"
#include "Thumbnail/CPM_Thumbnail.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
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
	return ConvaiPakManager::Chunk::ReadAssetId(ChunkId, ConvaiPakManager::Chunk::CurrentEnvironmentSlug());
}

bool UConvaiPakEditorSubsystem::CanAddAnotherChunk() const
{
	// Mid-scan the Chunk count is an undercount, so "yes" here would offer a Create whose id is
	// picked from labels the scan has not surfaced yet - two labels claiming one Chunk. The panel
	// re-asks when the scan completes, so the offer appears seconds later.
	if (const IAssetRegistry* AssetRegistry = IAssetRegistry::Get(); AssetRegistry && AssetRegistry->IsLoadingAssets())
	{
		return false;
	}

	return !UCPM_PakManagerSettings::Get().IsAtChunkLimit(GetChunkIds().Num());
}

bool UConvaiPakEditorSubsystem::CreateChunk(FString& OutError)
{
	// Checked ahead of the limit, so a creator who gets here mid-scan is told what is actually
	// happening rather than that their project is full.
	if (const IAssetRegistry* AssetRegistry = IAssetRegistry::Get(); AssetRegistry && AssetRegistry->IsLoadingAssets())
	{
		OutError = TEXT("the editor is still scanning this project's assets, so it cannot yet tell which "
			"chunks exist; try again in a moment");
		return false;
	}

	if (!CanAddAnotherChunk())
	{
		OutError = TEXT("this project already has as many chunks as it may publish");
		return false;
	}

	// INDEX_NONE while the project has no Chunk, which is exactly what makes this read the flat
	// ConvaiEssentials/ModdingMetaData.txt - the only place a pre-Chunk project writes its plugin.
	FCPM_ModdingMetadata Modding;
	UCPM_UtilityLibrary::GetModdingMetadataForChunk(ConvaiPakManager::Chunk::GetSoleChunkId(), Modding);
	if (Modding.PluginName.IsEmpty())
	{
		OutError = TEXT("this project records no modding plugin, so the Pak Manager cannot say where its "
			"chunk's label belongs; add a Primary Asset Label by hand");
		return false;
	}

	// 10 for the first, which is what the Modding Tool has always written: a creator whose project it
	// generated and one minted here name their Paks the same, so nothing downstream has two cases.
	// GetChunkIds is sorted, so the last is the highest.
	const TArray<int32> Existing = GetChunkIds();
	// Not const: a label that already declares a Chunk keeps it, and EnsureLabel says which.
	int32 ChunkId = Existing.IsEmpty() ? 10 : Existing.Last() + 1;

	const FString MountRoot = TEXT("/") + Modding.PluginName;
	const int32 Requested = ChunkId;
	if (!ConvaiPakManager::Chunk::EnsureLabel(MountRoot, ChunkId, OutError))
	{
		return false;
	}

	// EnsureLabel leaves a label that already declares a Chunk exactly as it found it and hands back
	// the id it declares, so nothing was minted here - and reporting success would tell a creator
	// they gained a Chunk that has been there all along.
	if (ChunkId != Requested)
	{
		OutError = FString::Printf(TEXT("%s already declares chunk %d, so no new chunk was created"),
			*(MountRoot / (TEXT("PAL_") + Modding.PluginName)), ChunkId);
		return false;
	}

	// The project may have just gained its first Chunk, and a pre-Chunk layout that nothing could be
	// attributed to a moment ago now can be. This also registers the new label's directory for the
	// Asset Manager, for every label the scan found.
	ReconcileChunkState();

	if (!GetChunkIds().Contains(ChunkId))
	{
		OutError = FString::Printf(
			TEXT("the label was written but the project still reports no chunk %d; see the Output Log"), ChunkId);
		return false;
	}
	return true;
}

void UConvaiPakEditorSubsystem::ReconcileChunkState()
{
	ConvaiPakManager::Chunk::ReconcileStateLayout();
}

bool UConvaiPakEditorSubsystem::HasUnmigratedLegacyLayout() const
{
	return ConvaiPakManager::Chunk::HasUnmigratedLegacyLayout();
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
	if (GetAssetId(ChunkId).IsEmpty())
	{
		return FDateTime::MinValue();
	}

	// MinValue for a file that is not there, which is the "never" this answers with.
	return IFileManager::Get().GetTimeStamp(*ConvaiPakManager::Chunk::GetRawArchiveRecordPath(
		ChunkId, ConvaiPakManager::Chunk::CurrentEnvironmentSlug()));
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
	/**
	 * Where the Convai SDK is mounted, with its trailing slash.
	 *
	 * Kept out of a creator's plugin and out of its dependency lists: every Convai product already
	 * ships the SDK, so a copy of it is a duplicate the product will never load - and to the copy
	 * API it looks like ordinary project content, because IsEnginePackage counts project plugins as
	 * game content.
	 */
	FString ConvaiSdkMountRoot()
	{
		if (const TSharedPtr<IPlugin> Convai = IPluginManager::Get().FindPlugin(TEXT("ConvAI")))
		{
			return Convai->GetMountedAssetPath();
		}
		return TEXT("/ConvAI/");
	}

	/**
	 * The content no Pak has to carry, because every Convai product already ships it.
	 *
	 * Engine content is deliberately NOT here. A product cooks only the engine assets its own
	 * content references, so a creator's level built from engine shapes and materials opens with
	 * those references dangling; engine dependencies are copied into the plugin like any other.
	 */
	TArray<FString> ContentEveryProductShips()
	{
		return { ConvaiSdkMountRoot(), TEXT("/ConvaiHTTP/") };
	}

	/** Why a gather copied nothing, phrased for the creator. */
	FString WhyCopyFailed(const FCPM_DependencyCopyReport& Report)
	{
		if (!Report.ErrorMessage.IsEmpty())
		{
			return Report.ErrorMessage;
		}

		TArray<FString> Failed;
		for (const FName& Package : Report.FailedPackages)
		{
			Failed.Add(Package.ToString());
		}
		if (!Failed.IsEmpty())
		{
			return FString::Printf(TEXT("could not copy %s"), *FString::Join(Failed, TEXT(", ")));
		}
		return TEXT("the copy failed; see the Output Log");
	}

	/** How every gather into the Modding Plugin is done, wherever the Entry Point is picked from. */
	FCPM_DependencyCopyOptions GatherOptions()
	{
		FCPM_DependencyCopyOptions Options;
		// Copy, never move: the creator picked an asset that lives somewhere for a reason, and a copy
		// that goes wrong costs them nothing but the copies.
		Options.Operation = ECPM_DependencyCopyOp::Copy;
		// Engine content is copied in rather than left where it is: what a Convai product cooked of
		// /Engine/ is whatever its own content needed, which is not what a creator's level needs.
		Options.EnginePolicy = ECPM_EngineDependencyPolicy::CopyIntoDestination;
		Options.bOverwriteExisting = false;
		Options.bSaveAfterCopy = true;
		Options.bSuppressUI = true;
		Options.bFixupRedirectors = true;
		Options.ExcludedPaths = ContentEveryProductShips();
		return Options;
	}

	/** Field names as the Convai asset metadata document spells them; the Draft keeps them. */
	const TCHAR* AssetNameField = TEXT("asset_name");
	const TCHAR* AssetDescriptionField = TEXT("asset_description");

	FString ReadDraftField(const int32 ChunkId, const TCHAR* Field)
	{
		FString Contents;
		if (!FFileHelper::LoadFileToString(Contents, *ConvaiPakManager::Chunk::GetDraftPath(ChunkId)))
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
	 * Sets fields and writes the Draft back.
	 *
	 * The Draft is the Pak Manager's own document, but still read-modify-write rather than
	 * serialised from a struct: a project moving between plugin versions has a Draft written by one
	 * field set and read by another, and rebuilding it from what this version happens to model
	 * would drop whatever the other one knew about.
	 */
	bool WriteDraftFields(const int32 ChunkId, const TMap<FString, FString>& Fields)
	{
		const FString Path = ConvaiPakManager::Chunk::GetDraftPath(ChunkId);

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
				// Refused rather than started fresh: overwriting a Draft we could not parse would
				// discard whatever the creator had typed into it.
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
	return ReadDraftField(ChunkId, AssetNameField);
}

bool UConvaiPakEditorSubsystem::SetAssetName(const int32 ChunkId, const FString& Name)
{
	return WriteDraftFields(ChunkId, { { AssetNameField, Name } });
}

FString UConvaiPakEditorSubsystem::GetAssetDescription(const int32 ChunkId) const
{
	return ReadDraftField(ChunkId, AssetDescriptionField);
}

bool UConvaiPakEditorSubsystem::SetAssetDescription(const int32 ChunkId, const FString& Description)
{
	return WriteDraftFields(ChunkId, { { AssetDescriptionField, Description } });
}

FString UConvaiPakEditorSubsystem::GetEntryPoint(const int32 ChunkId) const
{
	// An Avatar records a blueprint, a Scene records a level; whichever is set is the Entry Point.
	const FString BlueprintPath = ReadDraftField(ChunkId, TEXT("blueprint_class_path"));
	if (!BlueprintPath.IsEmpty())
	{
		return BlueprintPath;
	}

	return ConvaiPakManager::Chunk::ResolveLevelPackage(
		ReadDraftField(ChunkId, TEXT("level_name")),
		ReadDraftField(ChunkId, TEXT("root_path")));
}

bool UConvaiPakEditorSubsystem::PrepareEntryPoint(
	const int32 ChunkId, const FString& PackageName, FString& OutWhy, bool& bOutIsLevel, TArray<FString>& OutChanges,
	FString& OutDeclarationWarning)
{
	if (PackageName.IsEmpty())
	{
		OutWhy = TEXT("no asset was picked");
		return false;
	}

	const IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		OutWhy = TEXT("the asset registry is unavailable");
		return false;
	}

	TArray<FAssetData> Assets;
	AssetRegistry->GetAssetsByPackageName(FName(*PackageName), Assets);
	if (Assets.IsEmpty())
	{
		OutWhy = FString::Printf(TEXT("nothing exists at %s"), *PackageName);
		return false;
	}

	const FAssetData& Asset = Assets[0];
	bOutIsLevel = Asset.AssetClassPath == UWorld::StaticClass()->GetClassPathName();

	FCPM_ModdingMetadata Modding;
	UCPM_UtilityLibrary::GetModdingMetadataForChunk(ChunkId, Modding);

	// Before the type checks, because it is the one a creator is most likely to trip and the least
	// visible: an Entry Point outside the plugin is not in what the label gathers, so it cooks into
	// no Pak and the published Asset opens nothing.
	if (!ConvaiPakManager::Chunk::IsUnderModdingPlugin(PackageName, Modding.PluginName))
	{
		OutWhy = FString::Printf(
			TEXT("%s is outside the %s plugin. A published asset can only reach content under /%s/ - "
				"move it there first."),
			*PackageName, *Modding.PluginName, *Modding.PluginName);
		return false;
	}

	if (!ConvaiPakManager::Chunk::EntryPointSuitsAssetType(Asset.AssetClassPath, PackageName, Modding.AssetType, OutWhy))
	{
		return false;
	}

	// An Entry Point references Convai's content - the BP chatbot component added below, a Convai
	// character placed in a Scene - and a plugin may only reference the plugins its descriptor names.
	// Warned about rather than refused: the reference is legal content, and a creator whose `.uplugin`
	// is read-only should still be able to publish.
	if (FString Why; !ConvaiPakManager::Chunk::EnsureConvaiDependency(Modding.PluginName, Why))
	{
		OutDeclarationWarning = FString::Printf(
			TEXT("Could not declare Convai as a dependency of the %s plugin (%s), so asset validation ")
			TEXT("will report %s as illegally referencing Convai's content."),
			*Modding.PluginName, *Why, *PackageName);
		// Still logged: a publish runs this same check with nobody standing in front of the panel.
		CPM_LOG(Warning, TEXT("%s"), *OutDeclarationWarning);
	}

	const bool bWantsLevel = Modding.AssetType.Equals(TEXT("Scene"), ESearchCase::IgnoreCase);
	if (!bWantsLevel)
	{
		// The blueprint is edited in place rather than merely inspected: a creator picks the character
		// they built, and everything Convai needs on top of it is wiring nobody should have to know
		// about. Refuses only what it cannot fix without overwriting the creator's own work.
		UBlueprint* Blueprint = Cast<UBlueprint>(Asset.GetAsset());
		FString Error;
		if (!Blueprint || !ConvaiPakManager::Avatar::PrepareAvatarBlueprint(Blueprint, Error, OutChanges))
		{
			OutWhy = Error.IsEmpty() ? FString::Printf(TEXT("could not load %s"), *PackageName) : Error;
			return false;
		}

		if (!OutChanges.IsEmpty())
		{
			UPackage* Package = Blueprint->GetOutermost();
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			// No dialog: this runs behind a pick or a publish the creator asked for, not to be asked
			// back about package saving.
			SaveArgs.SaveFlags = SAVE_NoError;

			if (!UPackage::SavePackage(
				Package, Blueprint,
				*FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()),
				SaveArgs))
			{
				OutWhy = FString::Printf(TEXT("%s was set up for Convai but could not be saved"), *PackageName);
				return false;
			}

			CPM_LOG(Display, TEXT("Saved %s after setting it up as an Avatar entry point: %s."),
				*PackageName, *FString::Join(OutChanges, TEXT(", ")));
		}
	}

	return true;
}

bool UConvaiPakEditorSubsystem::SetEntryPoint(const int32 ChunkId, const FString& PackageName, FString& OutSetupNotes)
{
	OutSetupNotes.Reset();

	FString Why;
	bool bIsLevel = false;
	TArray<FString> Changes;
	FString DeclarationWarning;
	if (!PrepareEntryPoint(ChunkId, PackageName, Why, bIsLevel, Changes, DeclarationWarning))
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Update_Failed, Why);
		return false;
	}

	if (!Changes.IsEmpty())
	{
		OutSetupNotes = FString::Printf(TEXT("Set up %s for Convai: %s"),
			*FPaths::GetCleanFilename(PackageName), *FString::Join(Changes, TEXT(", ")));
	}

	if (!DeclarationWarning.IsEmpty())
	{
		OutSetupNotes = OutSetupNotes.IsEmpty()
			? DeclarationWarning
			: FString::Printf(TEXT("%s. %s"), *OutSetupNotes, *DeclarationWarning);
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

	if (!WriteDraftFields(ChunkId, Fields))
	{
		// Every other refusal here leaves its reason in the status, and both callers report the
		// status back to the creator. Without this one they report the *previous* refusal, which on
		// the relocate path is the sentence already on screen - a failure that looks like nothing.
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Update_Failed,
			FString::Printf(TEXT("could not write chunk %d's Draft, so %s was not recorded as its entry point"),
				ChunkId, *PackageName));
		return false;
	}

	return true;
}

bool UConvaiPakEditorSubsystem::PickEntryPointFromSelection(const int32 ChunkId, FString& OutSetupNotes)
{
	FString PackageName;
	GetSelectedAssetPackageName(PackageName);
	return SetEntryPoint(ChunkId, PackageName, OutSetupNotes);
}

bool UConvaiPakEditorSubsystem::IsInsideModdingPlugin(const int32 ChunkId, const FString& PackageName) const
{
	FCPM_ModdingMetadata Modding;
	UCPM_UtilityLibrary::GetModdingMetadataForChunk(ChunkId, Modding);
	return ConvaiPakManager::Chunk::IsUnderModdingPlugin(PackageName, Modding.PluginName);
}

bool UConvaiPakEditorSubsystem::RelocateEntryPointIntoPlugin(
	const int32 ChunkId, const FString& PackageName, FString& OutNewPackage, FString& OutSetupNotes, FString& OutWhy)
{
	// A refusal that says nothing is what a creator reads as the button having done nothing, so every
	// one of them says so in the log as well as to them. Worded for both shapes: most run before the
	// copy, the last one after it landed.
	const auto Refuse = [&OutWhy, &PackageName](FString Why)
	{
		OutWhy = MoveTemp(Why);
		CPM_LOG(Warning, TEXT("Copy into plugin refused for %s: %s."), *PackageName, *OutWhy);
		return false;
	};

	FCPM_ModdingMetadata Modding;
	UCPM_UtilityLibrary::GetModdingMetadataForChunk(ChunkId, Modding);
	if (Modding.PluginName.IsEmpty())
	{
		return Refuse(TEXT("this project records no modding plugin to copy into"));
	}

	if (ConvaiPakManager::Chunk::IsUnderModdingPlugin(PackageName, Modding.PluginName))
	{
		return Refuse(FString::Printf(TEXT("%s is already inside the %s plugin"), *PackageName, *Modding.PluginName));
	}

	const IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		return Refuse(TEXT("the asset registry is unavailable"));
	}

	TArray<FAssetData> Assets;
	AssetRegistry->GetAssetsByPackageName(FName(*PackageName), Assets);
	if (Assets.IsEmpty())
	{
		return Refuse(FString::Printf(TEXT("nothing exists at %s"), *PackageName));
	}

	// Before the copy, not after it: SetEntryPoint refuses the same mismatch at the end of this
	// function, by which time the whole dependency closure is under the plugin mount and staying -
	// which is content the chunk's label would then cook into the Pak.
	if (FString Mismatch; !ConvaiPakManager::Chunk::EntryPointSuitsAssetType(
		Assets[0].AssetClassPath, PackageName, Modding.AssetType, Mismatch))
	{
		return Refuse(Mismatch);
	}

	const FCPM_DependencyCopyOptions Options = GatherOptions();

	const FString DestinationRoot = TEXT("/") + Modding.PluginName + TEXT("/");
	const FName Source(*PackageName);
	const FCPM_DependencyCopyReport Report =
		FCPM_DependencyCopyAPI::CopyPackageWithDependencies(Source, DestinationRoot, Options);
	if (!Report.bSuccess)
	{
		return Refuse(WhyCopyFailed(Report));
	}

	const FName* Copied = Report.Remap.Find(Source);
	OutNewPackage = Copied
		? Copied->ToString()
		: FCPM_DependencyCopyAPI::MakeDestinationPackage(Source, DestinationRoot).ToString();

	CPM_LOG(Display, TEXT("Copied %d packages into %s (%d skipped); %s is this chunk's entry point now."),
		Report.CopiedCount, *DestinationRoot, Report.SkippedCount, *OutNewPackage);

	// The copy is set up for Convai like any other pick, and the declaration warning applies to it
	// just the same - carried back rather than left in the log, for the same reason the pick does.
	if (!SetEntryPoint(ChunkId, OutNewPackage, OutSetupNotes))
	{
		// The copies stay. Deleting them would throw away the one part that worked, and the creator
		// can now pick the copy by hand once whatever SetEntryPoint objected to is fixed.
		const FString Message = GetChunkStatus(ChunkId).Message;
		return Refuse(Message.IsEmpty()
			? FString::Printf(TEXT("%s was copied to %s, which was then refused as an entry point"),
				*PackageName, *OutNewPackage)
			: Message);
	}
	return true;
}

bool UConvaiPakEditorSubsystem::GatherDependenciesIntoPlugin(
	const int32 ChunkId, const FString& PackageName, int32& OutCopied, FString& OutWhy)
{
	OutCopied = 0;

	FCPM_ModdingMetadata Modding;
	UCPM_UtilityLibrary::GetModdingMetadataForChunk(ChunkId, Modding);
	if (Modding.PluginName.IsEmpty())
	{
		OutWhy = TEXT("this project records no modding plugin to copy into");
		return false;
	}

	// The other command is for the other case, and running this one there would copy the
	// dependencies while leaving the Entry Point itself outside the Pak.
	if (!ConvaiPakManager::Chunk::IsUnderModdingPlugin(PackageName, Modding.PluginName))
	{
		OutWhy = FString::Printf(
			TEXT("%s is outside the %s plugin, so it has to be copied in rather than gathered for"),
			*PackageName, *Modding.PluginName);
		return false;
	}

	if (!FPackageName::DoesPackageExist(PackageName))
	{
		OutWhy = FString::Printf(TEXT("nothing exists at %s"), *PackageName);
		return false;
	}

	TArray<FString> Inside;
	TArray<FString> Outside;
	if (!ListDependencies(ChunkId, PackageName, Inside, Outside))
	{
		OutWhy = FString::Printf(TEXT("could not read what %s depends on"), *PackageName);
		return false;
	}
	if (Outside.IsEmpty())
	{
		return true;
	}

	FCPM_DependencyCopyOptions Options = GatherOptions();

	// Everything already under the mount is rewritten in place rather than copied, and that is more
	// than the Entry Point: a World Partition level holds its actors in external packages of their
	// own, and those - not the level - are what reference the creator's meshes. Anything in the
	// plugin that reaches outside it has to be repointed, or the copies below are dead weight.
	Options.AdditionalPackagesToFixup.Add(FName(*PackageName));
	for (const FString& Package : Inside)
	{
		Options.AdditionalPackagesToFixup.Add(FName(*Package));
	}

	TArray<FName> Sources;
	for (const FString& Package : Outside)
	{
		Sources.Add(FName(*Package));
	}

	const FString DestinationRoot = TEXT("/") + Modding.PluginName + TEXT("/");
	const FCPM_DependencyCopyReport Report =
		FCPM_DependencyCopyAPI::CopyPackagesWithDependencies(Sources, DestinationRoot, Options);
	if (!Report.bSuccess)
	{
		OutWhy = WhyCopyFailed(Report);
		return false;
	}
	if (!Report.bReferencesFixedUp)
	{
		OutWhy = FString::Printf(
			TEXT("%d packages were copied into %s but %s could not be pointed at them; see the Output Log"),
			Report.CopiedCount, *DestinationRoot, *FPaths::GetCleanFilename(PackageName));
		return false;
	}

	OutCopied = Report.CopiedCount;
	CPM_LOG(Display, TEXT("Gathered %d packages into %s for %s (%d already there, %d packages repointed)."),
		Report.CopiedCount, *DestinationRoot, *PackageName, Report.SkippedCount,
		Options.AdditionalPackagesToFixup.Num());
	return true;
}

bool UConvaiPakEditorSubsystem::ListDependencies(const int32 ChunkId, const FString& PackageName,
	TArray<FString>& OutInsidePlugin, TArray<FString>& OutOutsidePlugin) const
{
	OutInsidePlugin.Reset();
	OutOutsidePlugin.Reset();

	if (PackageName.IsEmpty() || !IAssetRegistry::Get())
	{
		return false;
	}

	FCPM_ModdingMetadata Modding;
	UCPM_UtilityLibrary::GetModdingMetadataForChunk(ChunkId, Modding);

	TSet<FName> AllDependencies;
	TSet<FString> ExternalObjectsPaths;
	TSet<FName> Excluded;
	// Its return says whether everything sat under one mount point, which is a different question -
	// what is wanted here is the walk, stopped at the content every product already has.
	UConvaiPakManagerEditorUtils::GetPackageDependencies(
		FName(*PackageName),
		ContentEveryProductShips(),
		AllDependencies, ExternalObjectsPaths, Excluded);

	for (const FName& Dependency : AllDependencies)
	{
		const FString Dep = Dependency.ToString();
		// A cycle walks back to the Entry Point itself, which is not one of its dependencies.
		if (Dep == PackageName)
		{
			continue;
		}

		TArray<FString>& Bucket = ConvaiPakManager::Chunk::IsUnderModdingPlugin(Dep, Modding.PluginName)
			? OutInsidePlugin
			: OutOutsidePlugin;
		Bucket.Add(Dep);
	}

	OutInsidePlugin.Sort();
	OutOutsidePlugin.Sort();
	return true;
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

bool UConvaiPakEditorSubsystem::CaptureThumbnail(const int32 ChunkId, FString& OutWhy)
{
	const FString Path = ConvaiPakManager::Chunk::GetThumbnailPath(ChunkId);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);

	if (GetAssetType() != ECPM_AssetType::Avatar)
	{
		if (!UConvaiPakManagerEditorUtils::CPM_TakeViewportScreenshot(Path))
		{
			OutWhy = TEXT("the viewport could not be captured; see the Output Log");
			return false;
		}

		if (!ConvaiPakManager::Thumbnail::FileHasContent(Path))
		{
			// Deleted rather than left: a blank file reads to every later check as a thumbnail this
			// Chunk has, and the Chunk is better off with none than with a black one.
			IFileManager::Get().Delete(*Path);
			OutWhy = TEXT("the capture was blank; is the viewport showing your level?");
			return false;
		}
		return true;
	}

	// An Avatar has nothing to point a camera at - the thing being published is a blueprint, and the
	// editor already knows how to draw one: it is what the Content Browser shows for it.
	const FString EntryPoint = GetEntryPoint(ChunkId);
	if (EntryPoint.IsEmpty())
	{
		OutWhy = TEXT("pick the avatar's blueprint first");
		return false;
	}

	const IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		OutWhy = TEXT("the asset registry is unavailable");
		return false;
	}

	TArray<FAssetData> Assets;
	AssetRegistry->GetAssetsByPackageName(FName(*EntryPoint), Assets);
	UBlueprint* Blueprint = Assets.IsEmpty() ? nullptr : Cast<UBlueprint>(Assets[0].GetAsset());
	if (!Blueprint)
	{
		OutWhy = FString::Printf(TEXT("could not load %s"), *EntryPoint);
		return false;
	}

	int32 Width = 1920;
	int32 Height = 1080;
	TArray<FColor> Pixels;
	if (!ConvaiPakManager::Thumbnail::RenderBlueprintThumbnail(Blueprint, Width, Height, Pixels, OutWhy))
	{
		return false;
	}

	if (!ConvaiPakManager::Thumbnail::HasContent(Pixels))
	{
		OutWhy = TEXT("the render was blank; does the blueprint have a visible mesh?");
		return false;
	}

	if (!ConvaiPakManager::Thumbnail::WritePng(Path, Width, Height, Pixels))
	{
		OutWhy = FString::Printf(TEXT("could not write %s"), *Path);
		return false;
	}
	return true;
}

bool UConvaiPakEditorSubsystem::SetThumbnailFromFile(const int32 ChunkId, const FString& ImagePath, FString& OutWhy)
{
	return ConvaiPakManager::Thumbnail::ImportImageFile(
		ImagePath, ConvaiPakManager::Chunk::GetThumbnailPath(ChunkId), OutWhy);
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

bool UConvaiPakEditorSubsystem::GetCompatibility(FCPM_CompatibilityStatus& Out) const
{
	Out = Compatibility;
	return Compatibility.bChecked;
}

void UConvaiPakEditorSubsystem::RefreshCompatibility()
{
	if (bCompatibilityRefreshInFlight)
	{
		return;
	}

	bCompatibilityRefreshInFlight = true;
	PendingCompatibilityFetches = 2;

	Compatibility.InstalledToolVersion = ConvaiPakManager::Compatibility::InstalledToolVersion();
	Compatibility.EngineVersion = FEngineVersion::Current().ToString(EVersionComponent::Patch);

	TWeakObjectPtr<UConvaiPakEditorSubsystem> WeakThis(this);

	UCPM_PolicyRequest::Start(
		ConvaiPakManager::Compatibility::ToolRepository,
		ConvaiPakManager::Compatibility::SourceRef,
		ConvaiPakManager::Compatibility::ToolVersionFile,
		UCPM_PolicyRequest::FOnPolicyFetched::CreateLambda(
			[WeakThis](const bool bSucceeded, const FString& Contents)
			{
				UConvaiPakEditorSubsystem* Self = WeakThis.Get();
				if (!Self)
				{
					return;
				}

				if (bSucceeded)
				{
					Self->Compatibility.LatestToolVersion =
						ConvaiPakManager::Compatibility::ParsePluginVersionName(Contents);
					Self->Compatibility.bToolOutdated = ConvaiPakManager::Compatibility::IsNewerVersion(
						Self->Compatibility.InstalledToolVersion, Self->Compatibility.LatestToolVersion);
				}
				else
				{
					// Log, not Warning: a creator offline or behind a proxy has done nothing wrong,
					// and the flag stays false, so the banner simply never appears.
					CPM_LOG(Log, TEXT("Could not read the published Pak Manager version."));
				}

				Self->FinishCompatibilityFetch();
			}));

	UCPM_PolicyRequest::Start(
		ConvaiPakManager::Compatibility::ModdingToolRepository,
		ConvaiPakManager::Compatibility::SourceRef,
		ConvaiPakManager::Compatibility::TargetEngineFile,
		UCPM_PolicyRequest::FOnPolicyFetched::CreateLambda(
			[WeakThis](const bool bSucceeded, const FString& Contents)
			{
				UConvaiPakEditorSubsystem* Self = WeakThis.Get();
				if (!Self)
				{
					return;
				}

				if (bSucceeded)
				{
					Self->Compatibility.TargetEngineVersion =
						ConvaiPakManager::Compatibility::ParseTargetEngineVersion(Contents);
					Self->Compatibility.bEngineMismatch = !ConvaiPakManager::Compatibility::EngineMatchesTarget(
						Self->Compatibility.EngineVersion, Self->Compatibility.TargetEngineVersion);
				}
				else
				{
					CPM_LOG(Log, TEXT("Could not read the engine version the Modding Tool targets."));
				}

				Self->FinishCompatibilityFetch();
			}));
}

void UConvaiPakEditorSubsystem::FinishCompatibilityFetch()
{
	if (--PendingCompatibilityFetches > 0)
	{
		return;
	}

	bCompatibilityRefreshInFlight = false;
	// True even when both reads failed: the check ran, and both flags being false is its answer.
	Compatibility.bChecked = true;
	OnCompatibilityChanged.Broadcast();
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

	// Checked again, having been checked when it was picked: the pick is the only thing that check
	// caught, and a creator is free afterwards to delete the chatbot component, move the asset out of
	// the plugin or delete it outright. This is the last moment before the content is cooked, and it
	// re-applies the fix-up as well as the refusals. An Entry Point that was never picked is left to
	// the UI's own gate - a script may package without one, and widening that is not this check's job.
	const FString EntryPoint = GetEntryPoint(ChunkId);
	if (!EntryPoint.IsEmpty())
	{
		FString Why;
		bool bIsLevel = false;
		TArray<FString> Changes;
		FString DeclarationWarning;
		if (!PrepareEntryPoint(ChunkId, EntryPoint, Why, bIsLevel, Changes, DeclarationWarning))
		{
			SetStatus(ChunkId, Refusal, Why);
			return false;
		}
	}

	// A thumbnail that exists has to be worth publishing; one that does not is the UI's gate, not
	// this one, because a script may package without ever taking a picture.
	const FString Thumbnail = ConvaiPakManager::Chunk::GetThumbnailPath(ChunkId);
	if (FPaths::FileExists(Thumbnail) && !ConvaiPakManager::Thumbnail::FileHasContent(Thumbnail))
	{
		SetStatus(ChunkId, Refusal, TEXT("this chunk's thumbnail is blank; capture it again or choose an image"));
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
	// Resolved once, here, and read by every Job and by the archive marker below. Asking again later
	// would file this run under whichever backend the creator had switched to by then.
	PublishRequest.EnvironmentSlug = ConvaiPakManager::Chunk::CurrentEnvironmentSlug();
	PublishRequest.Policy = Policy;
	// Decided HERE, not by the Job: a package-only run must cook whatever the debug setting says,
	// or "Package now" finishes instantly having built nothing.
	PublishRequest.bReuseExistingPaks = FCPM_PublishOptions::ShouldReuseExistingPaks(
		bPackageOnly, Options.bReuseExistingPaks, UCPM_PakManagerSettings::Get().bUseExistingPakFile);
	Request.Inputs.Add(FInstancedStruct::Make(PublishRequest));

	TWeakObjectPtr<UConvaiPakEditorSubsystem> WeakThis(this);
	Request.OnProgressNative.BindLambda([WeakThis, ChunkId](const FWorkflowStatusInfo& Info)
	{
		if (UConvaiPakEditorSubsystem* Self = WeakThis.Get())
		{
			Self->HandleWorkflowProgress(ChunkId, Info);
		}
	});
	Request.OnFinishedNative.BindLambda([WeakThis, ChunkId, bPackageOnly, bArchiveRaw, EnvironmentSlug = PublishRequest.EnvironmentSlug](const FWorkflowStatusInfo& Info, const FWorkflowResult&)
	{
		if (UConvaiPakEditorSubsystem* Self = WeakThis.Get())
		{
			Self->HandleWorkflowFinished(ChunkId, Info, bPackageOnly, bArchiveRaw, EnvironmentSlug);
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

	// Watched across the call, because ICreateWorkflow runs the queue as well as building it: a
	// queue that finishes inside it reports finished before there is a handle to register.
	StartingChunkId = ChunkId;
	bStartingWorkflowFinished = false;

	const FWorkflowHandle Handle = Manager->ICreateWorkflow(Request);

	const bool bFinishedWhileStarting = bStartingWorkflowFinished;
	StartingChunkId = INDEX_NONE;
	bStartingWorkflowFinished = false;

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

	// Registering a Workflow that has already reported finished would leave this Chunk publishing
	// for the rest of the session: nothing else ever removes it, and every command is refused.
	if (!bFinishedWhileStarting)
	{
		ActivePublishes.Add(ChunkId, Handle);
	}

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
	const int32 ChunkId, const FWorkflowStatusInfo& Info, const bool bPackageOnly, const bool bArchivedRaw,
	const FString& EnvironmentSlug)
{
	// Reported from inside ICreateWorkflow, before the handle exists to be registered. The Remove
	// below is a no-op in that case; this is what stops the caller registering it afterwards.
	if (StartingChunkId == ChunkId)
	{
		bStartingWorkflowFinished = true;
	}

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
				*ConvaiPakManager::Chunk::GetRawArchiveRecordPath(ChunkId, EnvironmentSlug));
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
	// Captured at request time, like a Publish's: the delete is aimed at the backend resolved now,
	// and reading the slug again when the response lands could clear another backend's records.
	const FString EnvironmentSlug = ConvaiPakManager::Chunk::CurrentEnvironmentSlug();
	const FString AssetId = ConvaiPakManager::Chunk::ReadAssetId(ChunkId, EnvironmentSlug);
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
	DeletingEnvironmentSlug = EnvironmentSlug;

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
	const FString EnvironmentSlug = DeletingEnvironmentSlug;
	const bool bWholeAsset = Version.IsEmpty();
	const bool bDeleteContent = bDeletingPluginContent;
	DeletingChunkId = INDEX_NONE;
	DeletingVersion.Reset();
	DeletingEnvironmentSlug.Reset();
	bDeletingPluginContent = false;
	DeleteProxy = nullptr;

	if (ChunkId == INDEX_NONE)
	{
		return;
	}

	TArray<FString> Undeleted;

	if (bWholeAsset)
	{
		// Everything this backend minted goes with the Asset it described - the id would offer
		// Update against nothing. What the creator typed and captured stays: it is the input to
		// every backend, including the one they publish to next.
		ConvaiPakManager::Chunk::ClearAssetRecords(ChunkId, EnvironmentSlug, Undeleted);
	}
	else if (Version.Equals(TEXT("raw"), ESearchCase::IgnoreCase))
	{
		// Only the archive record, because only the archive is gone. The Asset survives a Version
		// delete, so this file is the one thing that would otherwise keep authorising a reuse.
		const FString ArchiveRecordPath =
			ConvaiPakManager::Chunk::GetRawArchiveRecordPath(ChunkId, EnvironmentSlug);
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
	DeletingEnvironmentSlug.Reset();
	bDeletingPluginContent = false;
	DeleteProxy = nullptr;

	if (ChunkId != INDEX_NONE)
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Failed, TEXT("the server refused to delete the asset"));
	}
}
