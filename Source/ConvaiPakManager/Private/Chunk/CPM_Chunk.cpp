// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Chunk/CPM_Chunk.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/PrimaryAssetLabel.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Utility/CPM_Log.h"
#include "Utility/CPM_UtilityLibrary.h"

namespace ConvaiPakManager::Chunk
{
namespace
{
	const TCHAR* EssentialsFolderName = TEXT("ConvaiEssentials");

	/**
	 * One state file, named by its stem so the flat and per-Chunk names cannot drift apart:
	 * `CreateAssetData.json` flat becomes `CreateAssetData_10.json` under a Chunk.
	 */
	struct FStateFile
	{
		const TCHAR* Stem;
		const TCHAR* Extension;

		FString LegacyName() const { return FString(Stem) + Extension; }
		FString PerChunkName(const int32 ChunkId) const
		{
			return FString::Printf(TEXT("%s_%d%s"), Stem, ChunkId, Extension);
		}
	};

	/**
	 * The three files a pre-Chunk project keeps flat in ConvaiEssentials.
	 *
	 * CreateAssetData is the one that matters: it holds the AssetID, which exists nowhere else.
	 * The other two are rebuildable from a Publish; that one is not.
	 */
	const FStateFile StateFiles[] = {
		{ TEXT("CreateAssetData"), TEXT(".json") },
		{ TEXT("PakMetaData"),     TEXT(".json") },
		{ TEXT("ModdingMetaData"), TEXT(".txt") },
	};

	/** Memoised GetSoleChunkId. Unset means "not yet resolved from a completed Asset Registry scan". */
	TOptional<int32> CachedSoleChunkId;
}

TArray<FCPM_Chunk> Discover()
{
	TArray<FCPM_Chunk> Chunks;

	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		CPM_LOG(Warning, TEXT("Chunk discovery: no Asset Registry; treating project as having no Chunks."));
		return Chunks;
	}

	// A partial scan yields a partial answer, and a caller cannot tell that from a project that
	// genuinely has fewer Chunks - so say so rather than let it pass as a result.
	if (AssetRegistry->IsLoadingAssets())
	{
		CPM_LOG(Warning, TEXT("Chunk discovery ran while the Asset Registry was still scanning; result may be incomplete."));
	}

	FARFilter Filter;
	Filter.ClassPaths.Add(UPrimaryAssetLabel::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> Labels;
	AssetRegistry->GetAssets(Filter, Labels);

	for (const FAssetData& Label : Labels)
	{
		// Loaded rather than read from registry tags: ChunkId lives in PrimaryAssetRules, which is
		// not tagged. Labels are tiny and this is not a per-frame path.
		const UPrimaryAssetLabel* Asset = Cast<UPrimaryAssetLabel>(Label.GetAsset());
		if (!Asset)
		{
			CPM_LOG(Warning, TEXT("Chunk discovery: could not load Primary Asset Label %s; skipping."),
				*Label.PackageName.ToString());
			continue;
		}

		const int32 ChunkId = Asset->Rules.ChunkId;
		if (ChunkId == INDEX_NONE)
		{
			// A label with no chunk is legitimate - it is labelling for cook rules, not packaging.
			continue;
		}

		if (const FCPM_Chunk* Existing = Chunks.FindByPredicate(
			[ChunkId](const FCPM_Chunk& Chunk) { return Chunk.Id == ChunkId; }))
		{
			// Two labels claiming one Chunk means two sets of Source Packages cook into one Pak and
			// publish as one Asset. That is a project misconfiguration the creator has to fix, and it
			// is worth naming both labels because the second is invisible from the first.
			CPM_LOG(Error, TEXT("Chunk %d is claimed by two Primary Asset Labels (%s and %s). Its Pak will hold both."),
				ChunkId, *Existing->LabelPackage.ToString(), *Label.PackageName.ToString());
			continue;
		}

		FCPM_Chunk Chunk;
		Chunk.Id = ChunkId;
		Chunk.LabelPackage = Label.PackageName;
		Chunks.Add(Chunk);
	}

	Chunks.Sort([](const FCPM_Chunk& A, const FCPM_Chunk& B) { return A.Id < B.Id; });
	return Chunks;
}

int32 GetSoleChunkId()
{
	if (CachedSoleChunkId.IsSet())
	{
		return CachedSoleChunkId.GetValue();
	}

	const TArray<FCPM_Chunk> Chunks = Discover();
	const int32 Resolved = Chunks.Num() == 1 ? Chunks[0].Id : INDEX_NONE;

	// A mid-scan answer is a guess about a project, not a fact about it. Recompute next time.
	const IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (AssetRegistry && !AssetRegistry->IsLoadingAssets())
	{
		CachedSoleChunkId = Resolved;
	}

	return Resolved;
}

void InvalidateSoleChunkCache()
{
	CachedSoleChunkId.Reset();
}

FString GetEssentialsDirectory()
{
	return FPaths::Combine(FPaths::ProjectDir(), EssentialsFolderName);
}

FString GetStateDirectory(const int32 ChunkId)
{
	return FPaths::Combine(GetEssentialsDirectory(), FString::Printf(TEXT("ChunkId_%d"), ChunkId));
}

FString GetCreateAssetDataPath(const int32 ChunkId)
{
	return FPaths::Combine(GetStateDirectory(ChunkId), FString::Printf(TEXT("CreateAssetData_%d.json"), ChunkId));
}

FString GetPakMetadataPath(const int32 ChunkId)
{
	return FPaths::Combine(GetStateDirectory(ChunkId), FString::Printf(TEXT("PakMetaData_%d.json"), ChunkId));
}

FString GetModdingMetadataPath(const int32 ChunkId)
{
	return FPaths::Combine(GetStateDirectory(ChunkId), FString::Printf(TEXT("ModdingMetaData_%d.txt"), ChunkId));
}

FString GetThumbnailPath(const int32 ChunkId)
{
	return FPaths::Combine(GetStateDirectory(ChunkId), FString::Printf(TEXT("Thumbnail_%d.png"), ChunkId));
}

EMigrationResult MigrateLegacyLayoutIn(
	const FString& EssentialsDirectory,
	const int32 ChunkId,
	TArray<FString>& OutMovedFiles)
{
	IFileManager& FileManager = IFileManager::Get();

	TArray<FString> Present;
	for (const FStateFile& StateFile : StateFiles)
	{
		if (FileManager.FileExists(*FPaths::Combine(EssentialsDirectory, StateFile.LegacyName())))
		{
			Present.Add(StateFile.LegacyName());
		}
	}

	if (Present.IsEmpty())
	{
		return EMigrationResult::NothingToMigrate;
	}

	if (ChunkId == INDEX_NONE)
	{
		// Named at Error rather than logged quietly: the creator's Assets are unreachable until this
		// is resolved, and silence here looks exactly like a project that was never published.
		CPM_LOG(Error,
			TEXT("Found a pre-Chunk ConvaiEssentials layout (%s) but this project does not have exactly one Chunk, ")
			TEXT("so it cannot be attributed. Nothing was moved."),
			*FString::Join(Present, TEXT(", ")));
		return EMigrationResult::Ambiguous;
	}

	const FString StateDirectory = FPaths::Combine(EssentialsDirectory, FString::Printf(TEXT("ChunkId_%d"), ChunkId));
	if (!FileManager.DirectoryExists(*StateDirectory) && !FileManager.MakeDirectory(*StateDirectory, true))
	{
		CPM_LOG(Error, TEXT("Could not create Chunk state directory %s. Nothing was moved."), *StateDirectory);
		return EMigrationResult::Failed;
	}

	bool bAnyFailed = false;
	for (const FStateFile& StateFile : StateFiles)
	{
		const FString LegacyPath = FPaths::Combine(EssentialsDirectory, StateFile.LegacyName());
		if (!FileManager.FileExists(*LegacyPath))
		{
			continue;
		}

		const FString DestinationPath = FPaths::Combine(StateDirectory, StateFile.PerChunkName(ChunkId));

		if (FileManager.FileExists(*DestinationPath))
		{
			// Already migrated. The per-Chunk copy is the live one, so leave both alone rather than
			// overwrite newer state with a stale flat file.
			CPM_LOG(Warning, TEXT("%s already exists; leaving the legacy %s in place untouched."),
				*DestinationPath, *StateFile.LegacyName());
			continue;
		}

		// Move rather than copy-then-delete: a half-finished copy that then loses its original is
		// how the AssetID gets destroyed, and Move either happens or does not.
		if (FileManager.Move(*DestinationPath, *LegacyPath))
		{
			OutMovedFiles.Add(DestinationPath);
		}
		else
		{
			bAnyFailed = true;
			CPM_LOG(Error, TEXT("Could not move %s to %s."), *LegacyPath, *DestinationPath);
		}
	}

	if (bAnyFailed)
	{
		return EMigrationResult::Failed;
	}

	if (!OutMovedFiles.IsEmpty())
	{
		CPM_LOG(Log, TEXT("Migrated %d pre-Chunk file(s) into %s."), OutMovedFiles.Num(), *StateDirectory);
		return EMigrationResult::Migrated;
	}

	return EMigrationResult::NothingToMigrate;
}

EMigrationResult MigrateLegacyLayout(TArray<FString>& OutMovedFiles)
{
	return MigrateLegacyLayoutIn(GetEssentialsDirectory(), GetSoleChunkId(), OutMovedFiles);
}

namespace
{
	/** The leaf of a package path: "/PLUGIN/Maps/Landing" -> "Landing". Already-short names pass through. */
	FString LeafOf(const FString& PackagePath)
	{
		int32 LastSlash = INDEX_NONE;
		return PackagePath.FindLastChar(TEXT('/'), LastSlash)
			? PackagePath.RightChop(LastSlash + 1)
			: PackagePath;
	}

	/** Sets Field to Fallback only if the document does not already carry a non-empty value for it. */
	void DefaultString(const TSharedRef<FJsonObject>& Root, const TCHAR* Field, const FString& Fallback)
	{
		FString Existing;
		if (!Root->TryGetStringField(Field, Existing) || Existing.IsEmpty())
		{
			Root->SetStringField(Field, Fallback);
		}
	}

	TSharedRef<FJsonObject> ObjectFieldOrEmpty(const TSharedRef<FJsonObject>& Root, const TCHAR* Field)
	{
		const TSharedPtr<FJsonObject>* Existing = nullptr;
		return Root->TryGetObjectField(Field, Existing) && Existing && Existing->IsValid()
			? (*Existing).ToSharedRef()
			: MakeShared<FJsonObject>();
	}
}

void FillRequiredMetadataFields(
	const TSharedRef<FJsonObject>& Root,
	const FString& ProjectName,
	const FString& PluginName,
	const FString& AssetType)
{
	const FString Type = AssetType.ToLower();

	Root->SetStringField(TEXT("project_name"), ProjectName);
	Root->SetStringField(TEXT("plugin_name"), PluginName);
	Root->SetStringField(TEXT("asset_type"), Type);

	// Relative to the packaged binary, and into the plugin the Modding Tool made - NOT the project's
	// own Content, which is where every field of this document was pointing when create-asset began
	// failing.
	Root->SetStringField(TEXT("content_path"),
		FString::Printf(TEXT("../../../%s/Plugins/%s/Content/"), *ProjectName, *PluginName));

	// Only a fallback: the mount point a package actually lives under is known exactly when the
	// creator picks the Entry Point, and an internal project can mount its content elsewhere.
	if (!PluginName.IsEmpty())
	{
		DefaultString(Root, TEXT("root_path"), FString::Printf(TEXT("/%s/"), *PluginName));
	}

	DefaultString(Root, TEXT("asset_name"), ProjectName);
	DefaultString(Root, TEXT("asset_description"), FString());
	DefaultString(Root, TEXT("level_name"), FString());
	DefaultString(Root, TEXT("blueprint_class_path"), FString());

	// "None" rather than "": the shape a published Asset carries for the class it does not have, and
	// what a product's class resolution reads as "there is no blueprint here".
	DefaultString(Root, TEXT("blueprint_class"), TEXT("None"));

	FString AssetName;
	Root->TryGetStringField(TEXT("asset_name"), AssetName);

	// entity_data is merged, never rebuilt: it is the half of the schema the Pak Manager models
	// least, and the server puts things in it that nothing here would know to put back.
	const TSharedRef<FJsonObject> Entity = ObjectFieldOrEmpty(Root, TEXT("entity_data"));
	if (Type == TEXT("avatar"))
	{
		FString ClassPath;
		Root->TryGetStringField(TEXT("blueprint_class_path"), ClassPath);
		if (ClassPath.IsEmpty())
		{
			DefaultString(Entity, TEXT("avatar_name"), AssetName);
		}
		else
		{
			Entity->SetStringField(TEXT("avatar_name"), LeafOf(ClassPath));
		}
		DefaultString(Entity, TEXT("gender"), TEXT("male"));
		if (!Entity->HasField(TEXT("avatar_config")))
		{
			Entity->SetObjectField(TEXT("avatar_config"), MakeShared<FJsonObject>());
		}
	}
	else
	{
		FString LevelName;
		Root->TryGetStringField(TEXT("level_name"), LevelName);
		if (LevelName.IsEmpty())
		{
			DefaultString(Entity, TEXT("scene_name"), AssetName);
		}
		else
		{
			Entity->SetStringField(TEXT("scene_name"), LeafOf(LevelName));
		}
		DefaultString(Entity, TEXT("scene_description"), TEXT("Pak scene"));
		if (!Entity->HasField(TEXT("scene_metadata")))
		{
			Entity->SetObjectField(TEXT("scene_metadata"), MakeShared<FJsonObject>());
		}
	}
	Root->SetObjectField(TEXT("entity_data"), Entity);
}

FString ResolveLevelPackage(const FString& LevelName, const FString& RootPath)
{
	if (LevelName.IsEmpty() || LevelName.StartsWith(TEXT("/")))
	{
		return LevelName;
	}

	FARFilter Filter;
	Filter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	FString SearchRoot = RootPath;
	SearchRoot.RemoveFromEnd(TEXT("/"));
	if (!SearchRoot.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*SearchRoot));
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

	// No longer on disk. The short name still says which level was picked.
	return LevelName;
}

bool NormalizePakMetadata(const int32 ChunkId)
{
	const FString Path = GetPakMetadataPath(ChunkId);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	FString Contents;
	if (FFileHelper::LoadFileToString(Contents, *Path))
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Contents);
		TSharedPtr<FJsonObject> Parsed;
		if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
		{
			// Same refusal as every other edit of this document: it holds the only copy of things
			// nobody can re-enter, so an unparseable one is left exactly as it is.
			CPM_LOG(Error, TEXT("Refusing to normalize %s: it is not valid JSON."), *Path);
			return false;
		}
		Root = Parsed;
	}

	// Upgraded here rather than left to the creator to re-pick: a document written by an older Pak
	// Manager holds the leaf alone, and the API wants the path.
	FString LevelName, RootPath;
	Root->TryGetStringField(TEXT("level_name"), LevelName);
	Root->TryGetStringField(TEXT("root_path"), RootPath);
	if (!LevelName.IsEmpty() && !LevelName.StartsWith(TEXT("/")))
	{
		Root->SetStringField(TEXT("level_name"), ResolveLevelPackage(LevelName, RootPath));
	}

	FCPM_ModdingMetadata Modding;
	UCPM_UtilityLibrary::GetModdingMetadata(Modding);

	FillRequiredMetadataFields(
		Root.ToSharedRef(),
		UCPM_UtilityLibrary::GetProjectName(),
		Modding.PluginName,
		Modding.AssetType);

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
