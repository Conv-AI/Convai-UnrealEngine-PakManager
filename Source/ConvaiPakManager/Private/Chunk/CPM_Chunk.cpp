// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Chunk/CPM_Chunk.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/PrimaryAssetLabel.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Utility/CPM_Log.h"

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
}
