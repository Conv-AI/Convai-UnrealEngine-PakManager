// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Utility/CPM_Utils.h"
#include "CPM_PublishTypes.generated.h"

/** What the Publish Policy says about one platform. */
USTRUCT(BlueprintType)
struct CONVAIPAKMANAGER_API FCPM_PlatformPolicy
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	bool bShouldPackage = false;

	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	FString Configuration;
};

/**
 * Which platforms a Publish builds for, at which configuration, and whether it includes the Raw
 * Project Archive. Held by Convai, the same for every creator. See CONTEXT.md.
 *
 * Resolved BEFORE the Job Queue is built, never as its first Job: this decides which Jobs exist, and
 * a queue's shape may depend only on what the caller knew before building it. See docs/adr/0004.
 */
USTRUCT(BlueprintType)
struct CONVAIPAKMANAGER_API FCPM_PublishPolicy
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	FCPM_PlatformPolicy Windows;

	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	FCPM_PlatformPolicy Linux;

	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	bool bUploadRawProject = false;

	/**
	 * Reads the policy Convai publishes.
	 *
	 * Returns false and leaves this untouched on anything it cannot read, rather than degrading to
	 * defaults: a policy that silently became "Windows only" would publish an Asset missing a
	 * Version, and nothing downstream would notice.
	 */
	bool ParseFromJson(const FString& Json, FString& OutError);

	/** Platforms this policy asks for, in a fixed order so a Job Queue built twice is built the same. */
	TArray<ECPM_Platform> PlatformsToPackage() const;

	const FCPM_PlatformPolicy* Find(ECPM_Platform Platform) const;
};

/**
 * What the caller supplies to a Publish, placed in the Workflow Context before the first Job.
 *
 * The Policy travels with it rather than being fetched by a Job, so every Job reads the same
 * decision the queue was built from.
 */
USTRUCT()
struct CONVAIPAKMANAGER_API FCPM_PublishRequest
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ChunkId = INDEX_NONE;

	UPROPERTY()
	FCPM_PublishPolicy Policy;
};

/** One built Pak: a Chunk on one platform, and the Version slot it publishes into. */
USTRUCT()
struct CONVAIPAKMANAGER_API FCPM_PakArtifact
{
	GENERATED_BODY()

	UPROPERTY()
	ECPM_Platform Platform = ECPM_Platform::None;

	UPROPERTY()
	FString PakPath;

	/** e.g. "ue-5.8-Windows". Names the Version this Pak occupies on the Asset. */
	UPROPERTY()
	FString VersionSlot;
};

/**
 * What one platform's Pak looks like on disk right now.
 *
 * Existence and a timestamp only - no staleness verdict. Deciding whether a Pak is out of date
 * would mean diffing it against the label's reach, and a wrong "stale" is worse than no verdict;
 * the timestamp lets the creator judge.
 */
USTRUCT(BlueprintType)
struct CONVAIPAKMANAGER_API FCPM_PakPlatformStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	ECPM_Platform Platform = ECPM_Platform::None;

	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	FString PakPath;

	/** The file exists, is nonzero, and passes ValidatePakFile. */
	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	bool bExists = false;

	/** File modification time. Meaningless when bExists is false. */
	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	FDateTime LastPackagedTime;
};

/** The creator's project, archived for the `raw` Version. */
USTRUCT()
struct CONVAIPAKMANAGER_API FCPM_RawArchive
{
	GENERATED_BODY()

	UPROPERTY()
	FString ZipPath;
};

/** The Asset a Chunk was published as, and where its artefacts are to be PUT. */
USTRUCT()
struct CONVAIPAKMANAGER_API FCPM_PublishedAsset
{
	GENERATED_BODY()

	UPROPERTY()
	FString AssetId;

	/** Keyed by Version slot, as minted by assets/upload. */
	UPROPERTY()
	TMap<FString, FString> UploadUrlsByVersion;

	/** The whole server response, kept verbatim so the Chunk's own record is what the server said. */
	UPROPERTY()
	FString RawResponse;
};

/**
 * What a Chunk is doing, in the Pak Manager's own words.
 *
 * The UI watches this and never subscribes to the Job System - what a creator is told should not
 * change shape because the machinery underneath was swapped. See docs/adr/0008.
 */
USTRUCT(BlueprintType)
struct CONVAIPAKMANAGER_API FCPM_ChunkStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	int32 ChunkId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	ECPM_AssetManagerStatus Status = ECPM_AssetManagerStatus::Max;

	/** 0..1 across the whole Publish, not within the running step. */
	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	float Progress = 0.0f;

	/** The running Job's name, so the UI can say "Packaging Windows" without the status enum growing a platform. */
	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	FString StepName;

	/** Why it failed, when it did. Empty otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	FString Message;

	/**
	 * Display names of every step this Publish will run, in order. Filled when the Job Queue is
	 * built - the Policy decides which steps exist, so the list is known only then. Empty when no
	 * Publish is in flight.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	TArray<FString> PlannedSteps;

	/** Index of the running step in PlannedSteps. INDEX_NONE outside a Publish. */
	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	int32 CurrentStepIndex = INDEX_NONE;

	bool IsBusy() const;
};
