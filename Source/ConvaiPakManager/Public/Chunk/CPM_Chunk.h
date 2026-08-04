// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * A Chunk: the Source Packages gathered into one publishable unit by one Primary Asset Label.
 * See CONTEXT.md. One Chunk is one Asset on Convai, never a part of one.
 */
struct CONVAIPAKMANAGER_API FCPM_Chunk
{
	/** From the label's PrimaryAssetRules. INDEX_NONE means the label declared no chunk. */
	int32 Id = INDEX_NONE;

	/** The Primary Asset Label that defines this Chunk, for error messages and for finding it again. */
	FName LabelPackage;

	bool IsValid() const { return Id != INDEX_NONE; }
};

namespace ConvaiPakManager::Chunk
{
/**
 * Every Chunk in this project, found by asking the Asset Registry for Primary Asset Labels.
 *
 * Discovered rather than configured, which is what lets one build serve a creator's project
 * (one label, authored by the Modding Tool) and an internal project (many) with no flag telling
 * them apart. It also makes the set of valid Chunk IDs enumerable: before this, a Chunk ID was a
 * hand-passed int and nothing stopped a caller naming one that no label declares, which silently
 * created state directories for Chunks that could never build.
 *
 * Labels are loaded to be read - ChunkId lives in PrimaryAssetRules and is not an Asset Registry
 * tag. They are small, and this runs on opening the panel rather than per frame.
 *
 * Sorted by Id so callers and tests see a stable order.
 */
CONVAIPAKMANAGER_API TArray<FCPM_Chunk> Discover();

/**
 * The single Chunk of a project that has exactly one, or INDEX_NONE.
 *
 * Deliberately refuses to guess when a project has several: picking the lowest would let a
 * multi-Chunk project quietly publish the wrong one. Callers that hold no Chunk ID of their own are
 * single-Chunk callers by definition, and a project that has stopped being single-Chunk should say
 * so rather than be answered anyway.
 *
 * Memoised, because the pre-Chunk entry points resolve through this on every call. A result found
 * while the Asset Registry was still scanning is NOT cached - caching one would pin a project at
 * "no Chunks" for the rest of the session on nothing but startup timing.
 */
CONVAIPAKMANAGER_API int32 GetSoleChunkId();

/** Drops the memoised answer. Call after anything that adds or removes a Primary Asset Label. */
CONVAIPAKMANAGER_API void InvalidateSoleChunkCache();

/** `<Project>/ConvaiEssentials` - the root of everything the creator's project records about its Assets. */
CONVAIPAKMANAGER_API FString GetEssentialsDirectory();

/** `<Project>/ConvaiEssentials/ChunkId_<N>` */
CONVAIPAKMANAGER_API FString GetStateDirectory(int32 ChunkId);

/** Where this Chunk records the Asset it was published as. Holds the AssetID; losing it orphans the Asset. */
CONVAIPAKMANAGER_API FString GetCreateAssetDataPath(int32 ChunkId);

/** Where this Chunk records the metadata uploaded alongside its Paks. */
CONVAIPAKMANAGER_API FString GetPakMetadataPath(int32 ChunkId);

/** Where this Chunk records what the Modding Tool decided about it - project, plugin, Asset Type. */
CONVAIPAKMANAGER_API FString GetModdingMetadataPath(int32 ChunkId);

/** The captured thumbnail for this Chunk. May not exist; a Chunk can be published without one. */
CONVAIPAKMANAGER_API FString GetThumbnailPath(int32 ChunkId);

/** Result of one migration attempt, so a caller can tell "nothing to do" from "could not". */
enum class EMigrationResult : uint8
{
	/** No legacy files present. The normal case on every run after the first. */
	NothingToMigrate,
	/** Legacy files were found and moved. */
	Migrated,
	/** Legacy files were found but the Chunk to attribute them to could not be determined. */
	Ambiguous,
	/** Legacy files were found but could not be moved. The originals are untouched. */
	Failed
};

/**
 * Moves a pre-Chunk `ConvaiEssentials/*.json` layout into `ConvaiEssentials/ChunkId_<N>/`.
 *
 * MUST run before anything reads Chunk state. The flat layout is what every already-published
 * creator project has on disk, and the AssetID inside it is the only copy in existence - read the
 * new path without migrating and the Pak Manager reports no Asset, offers Create where it should
 * offer Update, and publishes a duplicate while the original becomes permanently unreachable.
 *
 * Refuses rather than guesses when the project does not have exactly one Chunk: a flat layout
 * predates multi-Chunk support, so it can only have belonged to a single-Chunk project, and
 * attributing it to one of several would silently bind an Asset to the wrong Chunk.
 *
 * Never overwrites. A destination that already exists means migration has already happened and the
 * flat file is a leftover, so the newer per-Chunk state wins.
 *
 * @param OutMovedFiles  Destination paths of files actually moved. For logging and for tests.
 */
CONVAIPAKMANAGER_API EMigrationResult MigrateLegacyLayout(TArray<FString>& OutMovedFiles);

/** Migration against an explicit root and Chunk, so tests need no project on disk. */
CONVAIPAKMANAGER_API EMigrationResult MigrateLegacyLayoutIn(
	const FString& EssentialsDirectory,
	int32 ChunkId,
	TArray<FString>& OutMovedFiles);
}
