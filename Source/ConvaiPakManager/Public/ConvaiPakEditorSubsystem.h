// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Publish/CPM_PublishTypes.h"
#include "Type/JS_Definations.h"
#include "ConvaiPakEditorSubsystem.generated.h"

class UCPM_DeleteAssetProxy;

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
	 * Answers with an invalid handle when it cannot start - unknown Chunk, one already publishing, or
	 * a Publish Policy that could not be read. The refusal reason reaches the caller as a failed
	 * status on that Chunk, so a UI needs no second error path.
	 *
	 * The Policy is resolved BEFORE the queue is built, never as its first Job, because it decides
	 * which Jobs exist. See docs/adr/0004.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	FWorkflowHandle Publish(int32 ChunkId);

	/** Stops a Publish, letting the running step finish reporting first. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool CancelPublish(int32 ChunkId);

	/** Deletes one Version of this Chunk's Asset, or the whole Asset when Version is empty. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|Commands")
	bool DeleteAsset(int32 ChunkId, const FString& Version);

	// ---- Observation ----

	/** For C++ callers - the Slate panel binds this. */
	FCPM_OnChunkStatusChanged OnChunkStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Convai|PakManager|Commands")
	FCPM_OnChunkStatusChangedDynamic OnChunkStatusChangedEvent;

private:
	/** Reads the Publish Policy, from disk when a project overrides it and from the repository otherwise. */
	void ResolvePolicy(int32 ChunkId, TFunction<void(bool bSucceeded, const FCPM_PublishPolicy&, const FString& Error)> OnResolved);

	/** Builds the Job Queue this Policy asks for and starts it. */
	FWorkflowHandle StartPublishWorkflow(int32 ChunkId, const FCPM_PublishPolicy& Policy);

	void SetStatus(int32 ChunkId, ECPM_AssetManagerStatus Status, const FString& Message = FString(),
		float Progress = 0.0f, const FString& StepName = FString());

	void HandleWorkflowProgress(int32 ChunkId, const FWorkflowStatusInfo& Info);
	void HandleWorkflowFinished(int32 ChunkId, const FWorkflowStatusInfo& Info);

	/** Latest status per Chunk. Absent means never touched this session. */
	TMap<int32, FCPM_ChunkStatus> StatusByChunk;

	/** The Publish in flight for a Chunk, so it can be cancelled and so a second one is refused. */
	TMap<int32, FWorkflowHandle> ActivePublishes;

	UPROPERTY()
	TObjectPtr<UCPM_DeleteAssetProxy> DeleteProxy;

	/** The Chunk whose delete is in flight, so its outcome is attributed to the right Chunk. */
	int32 DeletingChunkId = INDEX_NONE;

	UFUNCTION()
	void HandleDeleteSucceeded(const FString& ResponseString);

	UFUNCTION()
	void HandleDeleteFailed(const FString& ResponseString);
};
