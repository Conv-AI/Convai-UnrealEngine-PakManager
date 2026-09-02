// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
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

	/**
	 * Whether this project may gain another Chunk.
	 *
	 * A stated policy, not an enforcement boundary - the plugin ships as source. See docs/adr/0003.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool CanAddAnotherChunk() const;

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

	/** Tagged spawn-point actors in the open editor world. Scenes only make sense asking. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	FCPM_SpawnPointStatus GetSpawnPointStatus() const;

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
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool SetEntryPoint(int32 ChunkId, const FString& PackageName);

	/** Records whatever is selected in the Content Browser as this Chunk's Entry Point. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool PickEntryPointFromSelection(int32 ChunkId);

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

	/** Captures the active viewport as this Chunk's thumbnail. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool CaptureThumbnail(int32 ChunkId);

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

	/** Stops a Publish, letting the running step finish reporting first. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool CancelPublish(int32 ChunkId);

	/**
	 * Builds this Chunk's Paks per the Publish Policy without uploading anything.
	 *
	 * Not surfaced in the UI - packaging happens inside a Publish there. Exists so a standalone
	 * packaging flow can grow later without a new seam. Same acceptance semantics and status
	 * reporting as Publish.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool Package(int32 ChunkId);

	/**
	 * Deletes one Version of this Chunk's Asset, or the whole Asset when Version is empty.
	 *
	 * A whole-asset delete also clears everything this Chunk recorded about that Asset - its id,
	 * metadata document, thumbnail and archive record - so the Chunk returns to Draft with an empty
	 * form. What Convai holds is gone; a kept copy would describe nothing.
	 *
	 * bAlsoDeletePluginContent additionally deletes the Source Packages in this Chunk's Modding
	 * Plugin, keeping the Primary Asset Label so the Chunk itself survives and can be filled again.
	 * NOT REVERSIBLE, and it is the creator's own authored content - callers must ask first.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool DeleteAsset(int32 ChunkId, const FString& Version, bool bAlsoDeletePluginContent = false);

	// ---- Observation ----

	/** For C++ callers - the Slate panel binds this. */
	FCPM_OnChunkStatusChanged OnChunkStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Convai|PakManager|Commands")
	FCPM_OnChunkStatusChangedDynamic OnChunkStatusChangedEvent;

private:
	/** Reads the Publish Policy, from disk when a project overrides it and from the repository otherwise. */
	void ResolvePolicy(int32 ChunkId, TFunction<void(bool bSucceeded, const FCPM_PublishPolicy&, const FString& Error)> OnResolved);

	/** Shared acceptance for Publish and Package: guards, then the Policy, then the Job Queue. */
	bool BeginPolicyRun(int32 ChunkId, bool bPackageOnly);

	/** Builds the Job Queue this Policy asks for and starts it. A package-only queue stops after the Paks. */
	FWorkflowHandle StartPublishWorkflow(int32 ChunkId, const FCPM_PublishPolicy& Policy, bool bPackageOnly);

	void SetStatus(int32 ChunkId, ECPM_AssetManagerStatus Status, const FString& Message = FString(),
		float Progress = 0.0f, const FString& StepName = FString());

	void HandleWorkflowProgress(int32 ChunkId, const FWorkflowStatusInfo& Info);
	/** bArchivedRaw is what the queue was built to do, so success can record that the archive landed. */
	void HandleWorkflowFinished(int32 ChunkId, const FWorkflowStatusInfo& Info, bool bPackageOnly, bool bArchivedRaw);

	/** Latest status per Chunk. Absent means never touched this session. */
	TMap<int32, FCPM_ChunkStatus> StatusByChunk;

	/** The Publish in flight for a Chunk, so it can be cancelled and so a second one is refused. */
	TMap<int32, FWorkflowHandle> ActivePublishes;

	/** Chunks whose Publish Policy is still being read: accepted and busy, but with no Workflow to cancel yet. */
	TSet<int32> PendingPolicyRuns;

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
