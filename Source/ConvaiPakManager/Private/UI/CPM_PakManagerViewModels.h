// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Publish/CPM_PublishTypes.h"
#include "Utility/CPM_Utils.h"

class UConvaiPakEditorSubsystem;

/**
 * Per-Chunk state and edits - the middle of the three tiers the UI spec demands. One instance per
 * Chunk, never a project-wide singleton; every field the form shows lives here, not in a widget.
 *
 * The pure rules (dirty, badge, validation) take no subsystem and touch no editor state, so the
 * automation tests drive them with hand-filled structs.
 */
struct FCPM_AssetViewModel
{
	int32 ChunkId = INDEX_NONE;

	// What the record holds. Refreshed by LoadFrom and after Save.
	FString SavedName;
	FString SavedDescription;

	// What the form fields hold. Diverge from Saved* while the creator types.
	FString Name;
	FString Description;

	/** Empty until the first Publish succeeds. Presence is what turns Create into Update. */
	FString AssetId;

	/** Package path of the Entry Point. Empty until picked. */
	FString EntryPoint;

	ECPM_AssetType AssetType = ECPM_AssetType::Max;

	FString ThumbnailPath;
	bool bThumbnailExists = false;

	/** Windows then Linux, as the subsystem answers. */
	TArray<FCPM_PakPlatformStatus> PakStatuses;

	FCPM_ChunkStatus Status;

	/** Re-reads everything from the subsystem. Keeps in-flight edits when they are dirty. */
	void LoadFrom(UConvaiPakEditorSubsystem& Subsystem);

	bool IsDirty() const;

	/** Pushes dirty fields through the Set* Commands, then refreshes the snapshot. */
	bool Save(UConvaiPakEditorSubsystem& Subsystem);

	/** Drops edits back to the snapshot. */
	void Revert();

	enum class EBadge : uint8
	{
		Draft,
		ReadyToPublish,
		Publishing,
		Published,
		NeedsAttention,
	};

	EBadge Badge() const;

	/** Empty means the Create/Publish gate is open: name, valid Entry Point, captured thumbnail. */
	TArray<FText> ValidationMessages() const;

	/** Validation passes and this Chunk is not already busy. Callers still apply the one-publish-at-a-time project gate. */
	bool CanCreateOrPublish() const;

	FText BadgeText() const;
};

/**
 * Project tier: shared configuration plus the discovered Chunks, each wrapped in its view model.
 */
struct FCPM_ProjectViewModel
{
	FString ProjectName;
	FString EngineVersion;

	/** One per discovered Chunk, sorted by id. Rebuilt by Refresh; existing dirty VMs are kept. */
	TArray<TSharedPtr<FCPM_AssetViewModel>> Assets;

	TSharedPtr<FCPM_AssetViewModel> Active;

	/** Rediscovers Chunks and reloads every VM. Preserves the Active selection when it survives. */
	void Refresh(UConvaiPakEditorSubsystem& Subsystem);

	/** True while any Chunk publishes - the whole-project gate that keeps publishes serial. */
	bool AnyPublishInFlight() const;

	/** Name of the publishing Chunk's Asset, for the "Publishing X..." hint. Empty when idle. */
	FText PublishingAssetName() const;

	TSharedPtr<FCPM_AssetViewModel> FindByChunkId(int32 ChunkId) const;
};
