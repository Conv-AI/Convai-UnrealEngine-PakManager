// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Publish/CPM_Compatibility.h"
#include "Publish/CPM_PublishTypes.h"
#include "Type/JS_Definations.h"
#include "ConvaiPakEditorSubsystem.generated.h"

class UCPM_DeleteAssetProxy;

/**
 * Where a Scene's spawn point stands in the currently open level.
 *
 * Count is what matters: zero means Add, one means the point Set-from-viewport moves, more than
 * one is a creator error the UI warns about rather than guessing which point a product will pick.
 */
USTRUCT(BlueprintType)
struct FCPM_SpawnPointStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	int32 Count = 0;

	/** Of the sole spawn point. Identity when Count != 1. */
	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	FTransform Transform = FTransform::Identity;
};

/** Broadcast whenever a Chunk's status changes. The UI's only subscription. See docs/adr/0008. */
DECLARE_MULTICAST_DELEGATE_OneParam(FCPM_OnChunkStatusChanged, const FCPM_ChunkStatus&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPM_OnChunkStatusChangedDynamic, const FCPM_ChunkStatus&, Status);

/**
 * Every Command the Pak Manager offers, and the only thing its UI talks to.
 *
 * Commands are typed functions rather than a string-keyed JSON registry because there is no process
 * boundary here to justify serialising across - the panel is Slate in this process, and scripts,
 * automation tests and Blueprint all reach these through Unreal's own reflection. See docs/adr/0001.
 *
 * Asynchronous Commands answer with a Workflow Handle and run on the Convai Job System. Callers
 * watch OnChunkStatusChanged rather than the Job System's own events, so swapping the machinery
 * underneath cannot change what a creator is told. See docs/adr/0008.
 */
UCLASS()
class CONVAIPAKMANAGER_API UConvaiPakEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	void GetSelectedAssetPackageName(FString& PackageName);

	// ---- Queries ----

	/** Every Chunk in this project, lowest ID first. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	TArray<int32> GetChunkIds() const;

	/** What this Chunk is doing. An unknown Chunk answers with a status of Max and no message. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	FCPM_ChunkStatus GetChunkStatus(int32 ChunkId) const;

	/** The Asset this Chunk was published as, or empty if it has never been published. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	FString GetAssetId(int32 ChunkId) const;

	/** This project's fixed Asset Type, decided by the Modding Tool. Max when the metadata is absent. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	ECPM_AssetType GetAssetType() const;

	/**
	 * What this Chunk's Paks look like on disk - Windows then Linux, fixed order.
	 *
	 * Paks are produced by a Publish, so before the first one these answer "missing"; that is
	 * information, not an error.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	TArray<FCPM_PakPlatformStatus> GetPakStatuses(int32 ChunkId) const;

	/**
	 * When this Chunk's Asset last received a Raw Project Archive. MinValue means it never has.
	 *
	 * The fact a Publish consults before reusing a published archive rather than sending a new one,
	 * and the only thing that makes that reuse safe: an Asset with no archive can never be rebuilt
	 * for a future engine version.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	FDateTime GetRawArchiveUploadTime(int32 ChunkId) const;

	/**
	 * The Publish Policy this session last read, and how that read went.
	 *
	 * A cache for DISPLAY only - which platforms to offer, what to say about them. A Publish never
	 * uses it: it resolves the Policy itself, because a stale copy is wrong exactly when it matters
	 * and publishing from one yields an Asset missing a Version. See docs/adr/0004.
	 *
	 * Returns whether OutPolicy is worth reading, which is only true in the Read state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool GetPublishPolicy(FCPM_PublishPolicy& OutPolicy, FDateTime& OutReadAt, ECPM_PolicyReadState& OutState) const;

	/**
	 * Re-reads the Publish Policy into that cache, then broadcasts so the UI repaints.
	 *
	 * Asynchronous, and never to be called from a paint path. Harmless to call while a read is
	 * already in flight - it returns without starting a second one.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	void RefreshPolicy();

	/**
	 * Whether this install is the one Convai targets - the tool against the published version, the
	 * engine against the one the Modding Tool ships.
	 *
	 * Returns whether anything has answered this session. Nothing gates on the answer: a creator
	 * behind a proxy still publishes, and telling them their engine is wrong because a fetch failed
	 * is worse than saying nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool GetCompatibility(FCPM_CompatibilityStatus& Out) const;

	/**
	 * Reads both version pins from their repositories, then broadcasts so the UI repaints.
	 *
	 * Asynchronous, and harmless to call while a read is already in flight.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	void RefreshCompatibility();

	/** Tagged spawn-point actors in the open editor world. Scenes only make sense asking. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	FCPM_SpawnPointStatus GetSpawnPointStatus() const;

	// ---- Chunks ----

	/**
	 * Whether this project may gain another Chunk.
	 *
	 * A stated policy, not an enforcement boundary - the plugin ships as source. See docs/adr/0003.
	 *
	 * False while the Asset Registry is still scanning: the Chunk set is not known yet, and offering
	 * a Create against a partial one mints an id another label may already claim.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool CanAddAnotherChunk() const;

	/**
	 * Gives this project a Chunk, by minting the Primary Asset Label that makes one.
	 *
	 * A Chunk is whatever a label gathers, so a project with no label has nothing to publish and no
	 * way in: everything else here takes a Chunk ID. Until now the only way to get one was to author
	 * a Primary Asset Label by hand, which is the one concept this tool exists to hide.
	 *
	 * The label goes in the Modding Plugin the project's metadata names, because that is where its
	 * content is; a project that records no plugin is told to author the label itself rather than
	 * having one put somewhere guessed.
	 *
	 * @param OutError  Why no Chunk was minted, in words a creator can act on. Empty on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool CreateChunk(FString& OutError);

	/**
	 * Brings this project's recorded state up to the layout this version reads.
	 *
	 * Called on every panel refresh and by CreateChunk after minting, not once at boot: the label set
	 * changes under a running session, and migration that had nothing to attribute a pre-Chunk layout
	 * to when the editor opened can attribute it the moment the project gains its first Chunk.
	 *
	 * Does nothing while the Asset Registry is still scanning - see Chunk::ReconcileStateLayout.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	void ReconcileChunkState();

	/**
	 * Whether a pre-Chunk layout is still sitting in ConvaiEssentials, unattributed to any Chunk.
	 *
	 * A condition of the PROJECT rather than of a Chunk, so deliberately not a Chunk status: there
	 * may be no Chunk to hang it on, which is the usual reason it is true. See docs/adr/0008.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool HasUnmigratedLegacyLayout() const;

	// ---- Edits ----
	//
	// Local-first: the creator's project is the record of their Assets and Convai is never asked what
	// it thinks they are called. Safe only because the Pak Manager is the sole writer. See docs/adr/0005.

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	FString GetAssetName(int32 ChunkId) const;

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool SetAssetName(int32 ChunkId, const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	FString GetAssetDescription(int32 ChunkId) const;

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool SetAssetDescription(int32 ChunkId, const FString& Description);

	/**
	 * The Source Package a Convai product opens out of this Chunk's Pak - the level for a Scene, the
	 * blueprint for an Avatar. Empty when none has been chosen.
	 *
	 * A Chunk gathers everything in its label's reach, so it does not on its own say which of those
	 * things is the thing to load. This does.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	FString GetEntryPoint(int32 ChunkId) const;

	/**
	 * Records a Source Package as this Chunk's Entry Point.
	 *
	 * Refuses a package whose kind does not match the Asset Type: a Scene must name a level and an
	 * Avatar a blueprint, and getting that wrong publishes an Asset that no product can open.
	 *
	 * @param OutSetupNotes  What was changed on the blueprint to make it usable, in one sentence.
	 *                       Empty when nothing was: a pick edits the creator's own asset, and an
	 *                       edit only the Output Log hears about is one nobody knows to undo.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool SetEntryPoint(int32 ChunkId, const FString& PackageName, FString& OutSetupNotes);

	/** Records whatever is selected in the Content Browser as this Chunk's Entry Point. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool PickEntryPointFromSelection(int32 ChunkId, FString& OutSetupNotes);

	/** Whether this package is in the Modding Plugin this Chunk publishes from. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool IsInsideModdingPlugin(int32 ChunkId, const FString& PackageName) const;

	/**
	 * Copies a package and everything it needs into the Modding Plugin, then records the copy as
	 * this Chunk's Entry Point.
	 *
	 * The way past the refusal above, which otherwise leaves a creator holding a working asset in
	 * the wrong folder and no way to move it without knowing what a mount point is.
	 *
	 * @param OutWhy  Why nothing was copied, phrased for the creator. Untouched on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool RelocateEntryPointIntoPlugin(int32 ChunkId, const FString& PackageName, FString& OutNewPackage, FString& OutWhy);

	/**
	 * The Source Packages this Entry Point drags into the Pak, split by whether they are in the
	 * Modding Plugin.
	 *
	 * A label gathers recursively, so a dependency outside the plugin is cooked in wherever it
	 * lives - which is how a Pak quietly grows by a folder of test content. Engine and Convai SDK
	 * content is left out of both lists: every product already ships it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool ListDependencies(int32 ChunkId, const FString& PackageName, TArray<FString>& OutInsidePlugin,
		TArray<FString>& OutOutsidePlugin) const;

	/**
	 * Places the actor a Convai product spawns its avatar at, tagged so the product can find it.
	 *
	 * Scenes only - an Avatar Chunk is the thing being spawned, not somewhere to spawn it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	AActor* AddSpawnPoint();

	/**
	 * Moves the sole spawn point to the viewport camera, or places one when none exists.
	 *
	 * Refuses when several exist: moving one of many would silently change which point wins, and
	 * the fix - delete the extras - is the creator's to make in the level, not this Command's to
	 * guess at.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool SetSpawnPointFromViewport();

	/**
	 * Makes this Chunk's thumbnail out of what the project already has - the Avatar's blueprint as
	 * the Content Browser draws it, or the Scene's viewport.
	 *
	 * Refuses a blank result instead of writing it: the thumbnail is the only thing a player sees
	 * before they take an Asset, and nothing downstream can fix a black card.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool CaptureThumbnail(int32 ChunkId, FString& OutWhy);

	/** Adopts an image the creator already has, re-encoded as PNG. Refuses a blank one. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool SetThumbnailFromFile(int32 ChunkId, const FString& ImagePath, FString& OutWhy);

	/** Where this Chunk's thumbnail lives, whether or not one has been captured. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	FString GetThumbnailPath(int32 ChunkId) const;

	// ---- Operations ----

	/**
	 * Takes a Chunk all the way to a usable Asset: package, archive, create, upload, record.
	 *
	 * Returns whether the request was ACCEPTED, not whether publishing succeeded - and deliberately
	 * not a Workflow Handle, because there is no workflow yet when this returns. The Publish Policy
	 * decides which Jobs exist and is read over the network, so the queue is built once it answers.
	 * See docs/adr/0004.
	 *
	 * Everything after acceptance - progress, the step running, success, failure - reaches the caller
	 * as this Chunk's status, which is the one thing a caller has to watch. See docs/adr/0008.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool Publish(int32 ChunkId);

	/**
	 * Publish, with this run's **Platform Selection** and Pak reuse chosen by the caller.
	 *
	 * The form the UI calls. Publish(ChunkId) is this with default Options - follow the Policy,
	 * cook every Pak - so a script that never heard of either keeps working.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool PublishWithOptions(int32 ChunkId, const FCPM_PublishOptions& Options);

	/** Stops a Publish, letting the running step finish reporting first. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool CancelPublish(int32 ChunkId);

	/**
	 * Builds this Chunk's Paks per the Publish Policy without uploading anything.
	 *
	 * Same acceptance semantics and status reporting as Publish.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool Package(int32 ChunkId);

	/** Package, with this run's **Platform Selection** chosen by the caller. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool PackageWithOptions(int32 ChunkId, const FCPM_PublishOptions& Options);

	/**
	 * Deletes one Version of this Chunk's Asset, or the whole Asset when Version is empty.
	 *
	 * A whole-asset delete also clears what THIS ENVIRONMENT recorded about that Asset - its id,
	 * metadata cache and archive marker - so the Chunk returns to Draft, keeping its name,
	 * description, Entry Point and thumbnail: those are inputs to every backend rather than records
	 * of the one just deleted from. Another backend's records are untouched.
	 *
	 * bAlsoDeletePluginContent additionally deletes the Source Packages in this Chunk's Modding
	 * Plugin, keeping the Primary Asset Label so the Chunk itself survives and can be filled again.
	 * NOT REVERSIBLE, and it is the creator's own authored content - callers must ask first.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool DeleteAsset(int32 ChunkId, const FString& Version, bool bAlsoDeletePluginContent = false);

	/**
	 * Deletes one platform's Version from this Chunk's Asset, keeping the Asset and its others.
	 *
	 * ECPM_Platform::Raw names the Raw Project Archive. Exists so a caller never has to spell a
	 * Version slot: one that spells it itself can spell it differently from whatever wrote it.
	 *
	 * Does NOT require this project to have a record of that upload. A fresh clone, a second
	 * machine, or a lost marker must not lock an operator out of removing something Convai holds -
	 * the request simply changes nothing when there is nothing there.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool DeleteVersion(int32 ChunkId, ECPM_Platform Platform);

	/**
	 * Deletes this Chunk's built Pak for one platform from THIS COMPUTER. Convai keeps whatever
	 * Version it already holds; the next Publish builds a new one.
	 *
	 * Returns whether the file is gone - including when there was none to begin with, which is the
	 * state the caller asked for.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool DeleteBuiltPak(int32 ChunkId, ECPM_Platform Platform);

	/** DeleteBuiltPak for every platform, including ones the Policy no longer asks for. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	int32 DeleteBuiltPaks(int32 ChunkId);

	// ---- Observation ----

	/** For C++ callers - the Slate panel binds this. */
	FCPM_OnChunkStatusChanged OnChunkStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Convai|PakManager|Commands")
	FCPM_OnChunkStatusChangedDynamic OnChunkStatusChangedEvent;

	/**
	 * The cached Publish Policy, or its read state, changed.
	 *
	 * Its own delegate rather than a synthetic Chunk status: the Policy belongs to the project, not
	 * to any one Chunk, and ADR-0008 keeps chunk status meaning what a Chunk is doing.
	 */
	DECLARE_MULTICAST_DELEGATE(FCPM_OnPolicyChanged);
	FCPM_OnPolicyChanged OnPolicyChanged;

	/**
	 * The compatibility check answered.
	 *
	 * Its own delegate for the same reason as OnPolicyChanged: which tool and engine this install
	 * runs is a fact about the project, not something a Chunk is doing. See docs/adr/0008.
	 */
	DECLARE_MULTICAST_DELEGATE(FCPM_OnCompatibilityChanged);
	FCPM_OnCompatibilityChanged OnCompatibilityChanged;

private:
	/**
	 * Makes this package fit to be that Chunk's Entry Point, or says why it cannot be - which for an
	 * Avatar blueprint means adding Convai's components and saving the asset.
	 *
	 * Shared by SetEntryPoint and by every Publish and Package, because a pick-time check only ever
	 * caught the pick: a creator can delete the chatbot component, move the asset out of the plugin
	 * or delete it outright afterwards, and a run is the last moment before the content is cooked.
	 *
	 * @param OutWhy       Why not, phrased for the creator. Untouched on success.
	 * @param bOutIsLevel  Whether the package is a level, which is how the Draft records it.
	 * @param OutChanges   What it had to change on the blueprint, so a caller can say so. Empty
	 *                     when it changed nothing, which is the usual case.
	 */
	bool PrepareEntryPoint(int32 ChunkId, const FString& PackageName, FString& OutWhy, bool& bOutIsLevel,
		TArray<FString>& OutChanges);

	/** Reads the Publish Policy, from disk when a project overrides it and from the repository otherwise. */
	void ResolvePolicy(int32 ChunkId, TFunction<void(bool bSucceeded, const FCPM_PublishPolicy&, const FString& Error)> OnResolved);

	/** Shared acceptance for Publish and Package: guards, then the Policy, then the Job Queue. */
	bool BeginPolicyRun(int32 ChunkId, bool bPackageOnly, const FCPM_PublishOptions& Options);

	/** Builds the Job Queue this Policy asks for and starts it. A package-only queue stops after the Paks. */
	FWorkflowHandle StartPublishWorkflow(int32 ChunkId, const FCPM_PublishPolicy& Policy, bool bPackageOnly,
		const FCPM_PublishOptions& Options);

	/** Records what a Policy read answered, for the display cache, and tells the UI. */
	void CachePolicy(bool bSucceeded, const FCPM_PublishPolicy& Policy);

	/** The Policy last read this session. Display only - see GetPublishPolicy. */
	FCPM_PublishPolicy CachedPolicy;

	FDateTime PolicyReadAt = FDateTime::MinValue();

	ECPM_PolicyReadState PolicyState = ECPM_PolicyReadState::Unread;

	/** True while a RefreshPolicy read is in flight, so a second one is not started. */
	bool bPolicyRefreshInFlight = false;

	/** What the version check last answered. Unread until RefreshCompatibility says otherwise. */
	FCPM_CompatibilityStatus Compatibility;

	bool bCompatibilityRefreshInFlight = false;

	/** How many of the two version reads are still outstanding. The last to answer publishes. */
	int32 PendingCompatibilityFetches = 0;

	/** One of those reads answered, whether or not it answered usefully. */
	void FinishCompatibilityFetch();

	void SetStatus(int32 ChunkId, ECPM_AssetManagerStatus Status, const FString& Message = FString(),
		float Progress = 0.0f, const FString& StepName = FString());

	void HandleWorkflowProgress(int32 ChunkId, const FWorkflowStatusInfo& Info);
	/**
	 * bArchivedRaw is what the queue was built to do, so success can record that the archive landed.
	 * EnvironmentSlug is the one the run started under, so that marker lands where the run published.
	 */
	void HandleWorkflowFinished(int32 ChunkId, const FWorkflowStatusInfo& Info, bool bPackageOnly, bool bArchivedRaw,
		const FString& EnvironmentSlug);

	/** Latest status per Chunk. Absent means never touched this session. */
	TMap<int32, FCPM_ChunkStatus> StatusByChunk;

	/** The Publish in flight for a Chunk, so it can be cancelled and so a second one is refused. */
	TMap<int32, FWorkflowHandle> ActivePublishes;

	/** Chunks whose Publish Policy is still being read: accepted and busy, but with no Workflow to cancel yet. */
	TSet<int32> PendingPolicyRuns;

	/**
	 * The Chunk whose Workflow is being created right now, and whether it finished before the call
	 * that created it returned.
	 *
	 * ICreateWorkflow both creates AND runs the queue, so a queue whose every Job completes
	 * synchronously - a single packaging Job that reuses the Pak already on disk - runs to
	 * completion inside it. HandleWorkflowFinished then removes the Chunk from ActivePublishes
	 * before it was ever added, and the add that follows registers a Workflow that is already over:
	 * the Chunk reads as publishing forever and every later command is refused with "this chunk is
	 * already publishing".
	 */
	int32 StartingChunkId = INDEX_NONE;
	bool bStartingWorkflowFinished = false;

	/** Cancels asked for during that read, honoured the moment the Policy answers. */
	TSet<int32> CancelledDuringPolicyRead;

	/** A Publish or Package is in flight for this Chunk, whether still reading the Policy or already running. */
	bool IsRunInFlight(int32 ChunkId) const;

	UPROPERTY()
	TObjectPtr<UCPM_DeleteAssetProxy> DeleteProxy;

	/** The Chunk whose delete is in flight, so its outcome is attributed to the right Chunk. */
	int32 DeletingChunkId = INDEX_NONE;

	/**
	 * Which Version that delete names, empty for the whole Asset. Kept rather than reduced to a
	 * bool: deleting the `raw` Version alone leaves the Asset, so only the version tells the record
	 * of the archive from the record of the Asset apart.
	 */
	FString DeletingVersion;

	/**
	 * Which backend that delete is aimed at, captured when the request was built rather than read
	 * when the response lands - a URL changed in between would clear another backend's records.
	 */
	FString DeletingEnvironmentSlug;

	/** Whether that delete was asked to take the Chunk's authored content with it. */
	bool bDeletingPluginContent = false;

	/**
	 * The Chunk whose content deletion is waiting for the next tick, or INDEX_NONE.
	 *
	 * Busy in every sense that matters even though no request is in flight: a Publish accepted in
	 * this window would package content that is about to be deleted underneath it.
	 */
	int32 PendingContentDeleteChunkId = INDEX_NONE;

	/**
	 * Deletes every Source Package in this Chunk's Modding Plugin except the Primary Asset Label.
	 *
	 * The label is what makes the Chunk exist, so keeping it leaves an empty Chunk the creator can
	 * fill again rather than one that vanishes from the tool along with its content.
	 */
	void DeletePluginContent(int32 ChunkId);

	UFUNCTION()
	void HandleDeleteSucceeded(const FString& ResponseString);

	UFUNCTION()
	void HandleDeleteFailed(const FString& ResponseString);
};
