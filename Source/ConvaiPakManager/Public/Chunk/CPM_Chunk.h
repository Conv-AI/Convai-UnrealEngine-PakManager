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

/**
 * The backend a record came from, as a directory name: `Env_<host>_<8 hex>`.
 *
 * Derived from the URL the bytes will actually reach and never configured. A setting naming the
 * environment could disagree with the URL, and would then label a record with a backend nothing was
 * ever sent to; a custom URL would have no name at all. The hash - of the whole canonical URL - is
 * the identity, and the host in front of it is only so a creator can tell their own folders apart.
 *
 * Scheme and host fold to lower case, because they are case-insensitive and one backend must not
 * end up with two folders. The path does not: GetFullURL passes it through unchanged, so a gateway
 * that distinguishes /Convai from /convai is two backends and has to record as two.
 */
CONVAIPAKMANAGER_API FString EnvironmentSlug(const FString& BaseUrl);

/**
 * The slug of the backend this project is pointed at right now.
 *
 * Resolve it where a request is built and pass it down. A creator who changes the URL mid-publish
 * must still have that run recorded under the backend it reached, and asking again when the
 * response lands would file it under the one they switched to.
 */
CONVAIPAKMANAGER_API FString CurrentEnvironmentSlug();

/**
 * `<Project>/ConvaiEssentials/ChunkId_<N>/Env_<host>_<hash>` - everything one backend minted.
 *
 * What the creator authored stays a level up: it is the same whichever backend receives it, and
 * partitioning it would blank their form every time the URL changed.
 */
CONVAIPAKMANAGER_API FString GetEnvironmentDirectory(int32 ChunkId, const FString& EnvironmentSlug);

/**
 * Where this Chunk records the Asset it was published as, under the backend that minted the
 * AssetID so no other backend can read it. Losing this file orphans the Asset.
 */
CONVAIPAKMANAGER_API FString GetCreateAssetDataPath(int32 ChunkId, const FString& EnvironmentSlug);

/** The server's document for this Chunk on that backend - a cache of what it holds, not a draft. */
CONVAIPAKMANAGER_API FString GetPakMetadataPath(int32 ChunkId, const FString& EnvironmentSlug);

/**
 * The AssetID this Chunk holds on that backend, or empty when it has never published to it.
 *
 * The one place both sides of a Publish read it from. They used to disagree - the UI asked per
 * Chunk and the create step asked the sole-Chunk helper - so a project that had gained a second
 * Primary Asset Label read no Asset at create time and made a duplicate beside the first.
 */
CONVAIPAKMANAGER_API FString ReadAssetId(int32 ChunkId, const FString& EnvironmentSlug);

/** Records what the create call answered. Fails loudly: without it the Asset exists and nothing here names it. */
CONVAIPAKMANAGER_API bool WriteCreateAssetData(int32 ChunkId, const FString& EnvironmentSlug, const FString& ResponseString);

/** Caches the document the server echoed back for this Chunk on that backend. */
CONVAIPAKMANAGER_API bool WritePakMetadata(int32 ChunkId, const FString& EnvironmentSlug, const FString& Document);

/**
 * Where this Chunk records what the Modding Tool decided about it - project, plugin, Asset Type.
 *
 * `.json`, which is what the contents have always been. The Modding Tool writes this file rather
 * than the Pak Manager, so a `.txt` is still read as a fallback until the tool catches up; the
 * fallback goes once it has.
 */
CONVAIPAKMANAGER_API FString GetModdingMetadataPath(int32 ChunkId);

/** Against an explicit root, so tests need no project on disk. */
CONVAIPAKMANAGER_API FString GetModdingMetadataPathIn(const FString& EssentialsDirectory, int32 ChunkId);

/**
 * Where this Chunk records that its Asset received a Raw Project Archive, under the backend that
 * received it - another backend has not had one.
 *
 * Existence is the whole record and the file's own timestamp is when - nothing here is parsed, so
 * there is no schema to keep. Written only after a Publish that sent one completed, and read to
 * decide whether the creator may reuse it; the Asset's own record cannot answer, because the
 * Versions it lists are the ones the create call named and `raw` is minted after it.
 */
CONVAIPAKMANAGER_API FString GetRawArchiveRecordPath(int32 ChunkId, const FString& EnvironmentSlug);

/** The captured thumbnail for this Chunk. May not exist; a Chunk can be published without one. */
CONVAIPAKMANAGER_API FString GetThumbnailPath(int32 ChunkId);

/**
 * What the creator typed: the Asset's name, its description and its Entry Point.
 *
 * Chunk level, because it is the same whichever backend the Chunk is published to. It used to live
 * inside the server's own document, which is why splitting it out came first: partitioning that
 * document per backend without this would have emptied the creator's form on every switch.
 */
CONVAIPAKMANAGER_API FString GetDraftPath(int32 ChunkId);

/**
 * Deletes what ONE backend minted for this Chunk - the Asset record, the metadata cache and the
 * archive record filed under that slug.
 *
 * What these describe no longer exists on that backend, and a kept copy would offer Update against
 * nothing or reuse an archive that went with the Asset. Another backend's three files are left
 * exactly as they are: deleting on staging says nothing about what production holds.
 *
 * The Draft and the thumbnail survive, and that is the point of them being a level up. They are
 * INPUTS to every backend rather than records of this one, so a delete here must leave what the
 * next Update to production still needs - and what the creator would otherwise re-type.
 *
 * ModdingMetaData is untouched for the same reason it always was: it says what the Modding Tool
 * decided about this project - its plugin and Asset Type - which is true of the project whether or
 * not anything is published from it, and nothing regenerates it.
 *
 * @param OutUndeleted  Paths that exist and could not be removed. Empty on success.
 */
CONVAIPAKMANAGER_API void ClearAssetRecords(
	int32 ChunkId, const FString& EnvironmentSlug, TArray<FString>& OutUndeleted);

/** Against an explicit root, so tests need no project on disk. */
CONVAIPAKMANAGER_API void ClearAssetRecordsIn(
	const FString& EssentialsDirectory, int32 ChunkId, const FString& EnvironmentSlug,
	TArray<FString>& OutUndeleted);

/**
 * Fills in every field the Convai asset API requires, from the ones the document already carries.
 *
 * The document is the server's schema, not ours. It used to be assembled by the Asset Uploader
 * Blueprint, which went with the Slate rebuild and took the half of the schema no C++ ever wrote -
 * entity_data above all - with it, so create-asset now fails on a document that looks complete.
 *
 * Derived fields (project_name, plugin_name, asset_type, content_path, root_path) are rewritten
 * every time: they follow from the project and are wrong, not merely absent, in a document written
 * by a version that computed them differently. Everything a creator or the server chose is kept.
 *
 * Pure and file-free so it can be tested without a project on disk; ComposePakMetadata is the
 * load-fill-save around it.
 */
CONVAIPAKMANAGER_API void FillRequiredMetadataFields(
	const TSharedRef<class FJsonObject>& Root,
	const FString& ProjectName,
	const FString& PluginName,
	const FString& AssetType);

/**
 * The package path of a level recorded by short name, or the name unchanged if nothing matches.
 *
 * Pak Managers before this one recorded a level by its leaf, which cannot name a level in a
 * subfolder - and RootPath is the MOUNT ROOT ("/PLUGIN/"), so gluing the two together invents a
 * path rather than finding one. The Asset Registry is asked instead.
 */
CONVAIPAKMANAGER_API FString ResolveLevelPackage(const FString& LevelName, const FString& RootPath);

/**
 * Builds the document this Chunk sends to that backend: its last echo from the server, overlaid
 * with everything the creator typed, then filled to what the API requires.
 *
 * PakMetaData is purely the server-derived cache now - the creator's own fields live in the Draft
 * and are laid over it here, at publish time. The Draft wins on every field it names, because the
 * creator's project is the record of their Asset and Convai is never asked what it is called. See
 * docs/adr/0005.
 *
 * Called before every create or update rather than only when a creator edits something: a Chunk
 * published from a project last touched by an older Pak Manager has a document missing fields
 * nobody is going to re-enter.
 *
 * False means nothing was written: one of the two documents is on disk but unreadable, so the cache
 * is left holding whatever the server last echoed. A caller MUST NOT publish on a false - sending
 * that cache hands the server its own last name and description back as if a creator had typed them.
 */
CONVAIPAKMANAGER_API bool ComposePakMetadata(int32 ChunkId, const FString& EnvironmentSlug);

/** The same composition against explicit paths, so it can be exercised without a project on disk. */
CONVAIPAKMANAGER_API bool ComposePakMetadataAt(
	const FString& MetadataPath,
	const FString& DraftPath,
	const FString& ProjectName,
	const FString& PluginName,
	const FString& AssetType);

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

/**
 * Moves the records a backend minted out of `ChunkId_<N>/` and down into `ChunkId_<N>/<slug>/`.
 *
 * Everything still loose predates the partition, and nothing had shipped that could reach any
 * backend but production - so production is what those records belong to. The slug must be derived
 * from the SETTINGS production URL rather than the resolved one: the resolved URL honours
 * -ConvaiProdURL= on the command line, so one CI launch against staging would file a production
 * record under staging permanently, where nothing would ever look for it again.
 *
 * Seeds the Draft from the loose PakMetaData before anything moves. Without that, the creator's
 * name, description and Entry Point travel under the backend folder inside the server's document
 * and their form comes back empty.
 *
 * Never overwrites: a destination that already exists is a backend that has published, and the
 * loose file is the older one. Moves rather than copies, for the same reason MigrateLegacyLayoutIn
 * does - the AssetID has no second copy anywhere.
 *
 * @param OutMovedFiles  Destination paths of files actually moved. For logging and for tests.
 */
CONVAIPAKMANAGER_API EMigrationResult AdoptLooseRecords(
	int32 ChunkId,
	const FString& EnvironmentSlug,
	TArray<FString>& OutMovedFiles);

/** Against an explicit root, so tests need no project on disk. */
CONVAIPAKMANAGER_API EMigrationResult AdoptLooseRecordsIn(
	const FString& EssentialsDirectory,
	int32 ChunkId,
	const FString& EnvironmentSlug,
	TArray<FString>& OutMovedFiles);
}
