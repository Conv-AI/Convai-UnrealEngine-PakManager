// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/TopLevelAssetPath.h"
#include "Utility/CPM_Utils.h"

struct FPluginDescriptor;

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
 * The Chunk a project gets when nothing says otherwise.
 *
 * 10 is what the Modding Tool has always written, both into the label it used to author and into the
 * `ChunkId_10/` state it writes today - so a project it generated and one bootstrapped here name
 * their Paks and their state directories the same, and nothing downstream has two cases.
 */
inline constexpr int32 DefaultChunkId = 10;

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

/**
 * Mints the Primary Asset Label that makes `<MountRoot>` a Chunk, or gives an existing one that
 * declares no Chunk the rules it is missing.
 *
 * The Modding Tool authors this label, so a generated project needs nothing here. A project that
 * predates the tool has none, and a creator who hand-authors one gets the `-1` ChunkId that every
 * new label defaults to - which Discover skips, so the project still has no Chunk and the tool has
 * nothing to publish. Nothing about the label is the creator's decision, so telling them to author
 * it was telling them to learn the one concept this tool exists to hide.
 *
 * A label that already declares a Chunk is returned untouched: it is one Discover already lists, and
 * rewriting its ChunkId would move published content into a different Pak.
 *
 * @param MountRoot  `/<PluginName>`, no trailing slash. The label is `<MountRoot>/PAL_<PluginName>`.
 * @param ChunkId    In: the Chunk to mint. Out: the Chunk the label declares, which is the one asked
 *                   for unless it already declared another.
 * @param OutError   Why nothing was minted, in words a creator can act on. Empty on success.
 */
CONVAIPAKMANAGER_API bool EnsureLabel(const FString& MountRoot, int32& ChunkId, FString& OutError);

/**
 * Adds MountRoot to the directories the Asset Manager scans for Primary Asset Labels, writing
 * DefaultGame.ini only when it is not already listed.
 *
 * A label in a directory nothing scans is not a Chunk as far as the cooker is concerned: its content
 * lands in chunk 0, no `pakchunk<N>` is emitted, and the Publish succeeds having shipped nothing.
 * The Modding Tool wrote this entry itself, which is why a generated project has never needed it and
 * a hand-made one has always been quietly broken.
 *
 * False means the config file could not be written - a project under source control that has not
 * checked DefaultGame.ini out. The setting still applies for this session.
 */
CONVAIPAKMANAGER_API bool EnsureLabelDirectoryScanned(const FString& MountRoot);

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
 *
 * A flat `ConvaiEssentials/ModdingMetaData.txt` is read when no per-Chunk copy exists at all. That
 * is the un-migrated project, which has no Chunk to name a per-Chunk path with - and this file is
 * where its plugin_name is, which is the one thing EnsureLabel needs to give it a Chunk. Without the
 * fallback the whole layer resolves `ChunkId_-1/ModdingMetaData_-1.json`, reads nothing, and reports
 * a project with no Asset Type.
 *
 * DefaultChunkId's copy answers for INDEX_NONE last. A project the Modding Tool generated today has
 * `ChunkId_10/ModdingMetaData_10.json` and no label at all, so it has no Chunk to ask with and no
 * flat file either - the same dead end, reached from the other side.
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
 * Versions it lists are the ones the create call named and the archive Version is minted after it.
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
 * The Draft and the thumbnail survive for as long as any OTHER backend still holds an Asset for
 * this Chunk. They are INPUTS to every backend rather than records of one, so a staging delete must
 * leave what the next Update to production still needs - and what the creator would otherwise
 * re-type. Once the last backend lets go they are deleted too: kept with nothing published, they
 * read as an Asset that still exists, and the creator gets a pre-filled form for one that does not.
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
 * ArtifactSizes writes one `<Platform>_PakSize` per artefact the run measured. Only the platforms it
 * names are touched: a size this run did not measure describes a Version the Asset still holds, and
 * clearing it would say the artefact was gone.
 *
 * Pure and file-free so it can be tested without a project on disk; ComposePakMetadata is the
 * load-fill-save around it.
 */
CONVAIPAKMANAGER_API void FillRequiredMetadataFields(
	const TSharedRef<class FJsonObject>& Root,
	const FString& ProjectName,
	const FString& PluginName,
	const FString& AssetType,
	const TMap<ECPM_Platform, int64>& ArtifactSizes = {});

/**
 * Whether a package lives inside the Modding Plugin a Chunk records.
 *
 * An Entry Point outside the plugin is not in what the label gathers, so it cooks into no Pak and
 * the published Asset opens nothing - a failure that surfaces only when a Convai product tries to
 * load it. StartsWith rather than Contains: a creator's copy kept at `/Game/<PluginName>_old/`
 * contains the name and is not in the plugin.
 *
 * An empty PluginName passes everything. A Modding Plugin is a convention for where a creator puts
 * things rather than part of what a Chunk is (see CONTEXT.md), and an internal project that labels
 * its own /Game content records no plugin to be inside.
 */
CONVAIPAKMANAGER_API bool IsUnderModdingPlugin(const FString& PackageName, const FString& PluginName);

/**
 * Declares the Convai SDK as a dependency of the Modding Plugin, so its content may reference it.
 *
 * A plugin's content may only reference the engine, the project, and the plugins its `.uplugin`
 * names (AssetValidator_AssetReferenceRestrictions). An Entry Point always ends up referencing
 * Convai's content - the BP chatbot component an Avatar is given, a Convai character placed in a
 * Scene - and the Modding Tool generates a descriptor that declares nothing, so every such project
 * fails validation on a reference the Pak Manager itself put there.
 *
 * Writing the descriptor broadcasts OnPluginEdited, which is what rebuilds the editor's domain
 * database; no restart is needed for the error to stop.
 *
 * An empty PluginName passes: a project that labels its own /Game content has no descriptor to
 * amend and needs none, project content already seeing every project plugin.
 *
 * Requires Convai to be enabled rather than merely discovered: the domain database resolves a
 * declared dependency with FindEnabledPlugin, so a name taken from a disabled plugin grants nothing.
 *
 * @param OutError  Why the dependency could not be declared - no plugin of that name is mounted,
 *                  Convai is not enabled in this project, or the `.uplugin` could not be written.
 *                  Untouched when there was nothing to do.
 */
CONVAIPAKMANAGER_API bool EnsureConvaiDependency(const FString& PluginName, FString& OutError);

/**
 * Adds Convai to a descriptor's dependencies, or enables the entry that is already there.
 *
 * Separated from the file it lives in so the rule can be exercised without a plugin on disk.
 *
 * @return Whether the descriptor changed.
 */
CONVAIPAKMANAGER_API bool DeclareConvaiDependency(FPluginDescriptor& Descriptor,
	const FString& ConvaiPluginName);

/**
 * Whether an asset is the kind a Chunk's Asset Type can publish.
 *
 * Decided in the editor rather than on the server: an Avatar whose Entry Point is a level, or a
 * Scene whose Entry Point is a blueprint, publishes an Asset no product can open, and nothing
 * between here and there would notice. Also the gate on copying an Entry Point into the plugin -
 * that copy drags a whole dependency closure under the mount and leaves it there, so it has to be
 * refused before it starts rather than when the Entry Point is finally recorded.
 *
 * Anything that is not a Scene is an Avatar, matching what the metadata records.
 *
 * @param OutWhy  Why the asset is refused, phrased for the creator. Untouched when it passes.
 */
CONVAIPAKMANAGER_API bool EntryPointSuitsAssetType(const FTopLevelAssetPath& AssetClass,
	const FString& PackageName, const FString& AssetType, FString& OutWhy);

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
CONVAIPAKMANAGER_API bool ComposePakMetadata(
	int32 ChunkId, const FString& EnvironmentSlug, const TMap<ECPM_Platform, int64>& ArtifactSizes = {});

/** The same composition against explicit paths, so it can be exercised without a project on disk. */
CONVAIPAKMANAGER_API bool ComposePakMetadataAt(
	const FString& MetadataPath,
	const FString& DraftPath,
	const FString& ProjectName,
	const FString& PluginName,
	const FString& AssetType,
	const TMap<ECPM_Platform, int64>& ArtifactSizes = {});

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
 * INDEX_NONE means the caller could not name one Chunk to attribute the layout to, and the answer is
 * Ambiguous with nothing moved: a flat layout predates multi-Chunk support, so it can only have
 * belonged to a single-Chunk project, and attributing it to one of several would silently bind an
 * Asset to the wrong Chunk. Silent about it - this runs on every panel refresh, so how loudly to say
 * so is the caller's decision.
 *
 * Never overwrites. A destination that already exists means migration has already happened and the
 * flat file is a leftover, so the newer per-Chunk state wins.
 *
 * Takes an explicit root and Chunk, so tests need no project on disk.
 *
 * @param OutMovedFiles  Destination paths of files actually moved. For logging and for tests.
 */
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

/**
 * Whether a pre-Chunk flat layout is still sitting in ConvaiEssentials.
 *
 * True means the only copy of this project's AssetID is in a file nothing reads any more, so the
 * tool would offer Create for a Chunk that already has an Asset - and taking that offer orphans the
 * Asset permanently. The UI says so and refuses to publish rather than leaving it to the log, which
 * is where the refusal used to end.
 */
CONVAIPAKMANAGER_API bool HasUnmigratedLegacyLayout();

/** Against an explicit root, so tests need no project on disk. */
CONVAIPAKMANAGER_API bool HasUnmigratedLegacyLayoutIn(const FString& EssentialsDirectory);

/**
 * Brings this project's ConvaiEssentials up to the layout this version reads, and makes sure every
 * Chunk's label sits in a directory the Asset Manager scans.
 *
 * The pre-Chunk migration then the per-backend adoption, in that order: the second one's input is
 * what the first one moved. Drops the memoised sole-Chunk answer first, because the reason to run
 * this again is that the label set may have changed since the last run - a cached "no Chunks" is
 * exactly what pinned a project at Ambiguous for the rest of the session.
 *
 * Safe to call repeatedly, and called from everywhere the Chunk set can have changed: boot, minting
 * a label, and every panel refresh. A project that gains its first Chunk mid-session used to need an
 * editor restart before its state was migrated.
 *
 * Does nothing while the Asset Registry is still scanning. It moves the only copy of the project's
 * AssetID, and a partial scan cannot say which Chunk owns it; every caller runs it again once the
 * scan completes.
 */
CONVAIPAKMANAGER_API void ReconcileStateLayout();

/**
 * The decision ReconcileStateLayout makes, with every registry, config and memo read hoisted out to
 * the caller: given the Chunks this project has and the backend its loose records belong to, migrate
 * the pre-Chunk layout and adopt what is still loose. Everything it touches is a file under
 * EssentialsDirectory, so a test can assert on the outcome without a project on disk - which is the
 * half of this that was never covered, the suite having only ever exercised the migration with the
 * Chunk handed to it.
 *
 * Returns the flat-layout migration's result; the per-Chunk adoptions log their own.
 */
CONVAIPAKMANAGER_API EMigrationResult ReconcileStateLayoutIn(
	const FString& EssentialsDirectory,
	const TArray<FCPM_Chunk>& Chunks,
	const FString& ProductionSlug,
	TArray<FString>& OutMovedFiles);
}
