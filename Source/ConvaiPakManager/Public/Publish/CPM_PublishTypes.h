// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Utility/CPM_Utils.h"
#include "CPM_PublishTypes.generated.h"

/**
 * How far the session has got with reading the Publish Policy.
 *
 * The UI shows platforms per the Policy, so it needs to tell "Convai does not ask for Linux" from
 * "nobody has asked Convai yet" - the second must never render as the first. Unread and Failed both
 * mean the UI shows MORE, never less: a platform vanishing because a GET timed out would hide a Pak
 * the creator has on disk.
 */
UENUM(BlueprintType)
enum class ECPM_PolicyReadState : uint8
{
	/** Nobody has asked yet this session. */
	Unread,
	/** A read is in flight. */
	Reading,
	/** The cached Policy is what Convai last answered. */
	Read,
	/** The last read failed. The cached Policy is meaningless; only a Publish's own read decides. */
	Failed
};

/** What the Publish Policy says about one platform. */
USTRUCT(BlueprintType)
struct CONVAIPAKMANAGER_API FCPM_PlatformPolicy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convai|PakManager")
	bool bShouldPackage = false;

	/**
	 * The build configuration to package at. Required when bShouldPackage.
	 *
	 * Defaulted for the hand-written policy a project types into its settings, so the field arrives
	 * filled rather than empty-and-refused. A policy READ from JSON must still name it - ParseFromJson
	 * clears this before reading, because inheriting a default there would package a Pak built
	 * differently from the one Convai asked for.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convai|PakManager",
		meta = (EditCondition = "bShouldPackage"))
	FString Configuration = TEXT("Shipping");
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convai|PakManager")
	FCPM_PlatformPolicy Windows;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convai|PakManager")
	FCPM_PlatformPolicy Linux;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convai|PakManager")
	bool bUploadRawProject = false;

	/**
	 * Reads the policy Convai publishes.
	 *
	 * Returns false and leaves this untouched on anything it cannot read, rather than degrading to
	 * defaults: a policy that silently became "Windows only" would publish an Asset missing a
	 * Version, and nothing downstream would notice.
	 */
	bool ParseFromJson(const FString& Json, FString& OutError);

	/**
	 * Whether this policy is one a Publish can be run from.
	 *
	 * Shared by every way a policy arrives - parsed from Convai's, typed into a project's settings -
	 * so a hand-written one is held to what a fetched one is held to. The two refusals it makes are
	 * the ones nothing downstream would notice: a platform with no build configuration, and a policy
	 * that would produce no artefact at all.
	 */
	bool Validate(FString& OutError) const;

	/**
	 * What Convai's own Policy says today: both platforms at Shipping, with the Raw Project Archive.
	 *
	 * The starting point a project's typed override is filled with, so overriding begins from a
	 * policy that publishes what production publishes and the creator edits away from it. NOT the
	 * default of the struct itself: a default-constructed Policy is the one a failed read leaves
	 * behind, and it has to stay "packages nothing" so a misread cannot look like an instruction.
	 */
	static FCPM_PublishPolicy Defaults();

	/** Platforms this policy asks for, in a fixed order so a Job Queue built twice is built the same. */
	TArray<ECPM_Platform> PlatformsToPackage() const;

	const FCPM_PlatformPolicy* Find(ECPM_Platform Platform) const;

	/**
	 * This Policy with its platform flags replaced by an explicit **Platform Selection**.
	 *
	 * The one thing allowed to add to a Policy rather than only subtract from it - see CONTEXT.md.
	 * Applied to the Policy rather than carried alongside it so that everything downstream, from
	 * PlatformsToPackage to the Version slot each Pak occupies, keeps reading one decision.
	 *
	 * A platform the Policy never asked for arrives with no build configuration, because
	 * ParseFromJson clears it and Validate refuses a platform that packages without one. It
	 * inherits the configuration of a platform the Policy DID ask for, so a forced Linux Pak is
	 * built the way production builds, falling back to Shipping only when the Policy asked for
	 * nothing at all.
	 */
	FCPM_PublishPolicy WithPlatforms(const TArray<ECPM_Platform>& Selection) const;
};

/**
 * What a caller asked of one Publish beyond what the Policy says, chosen per run.
 *
 * Deliberately not project settings. Both of these describe one run - the enterprise project
 * publishing Linux this once, the creator who knows this Pak is fresh - and an override that
 * outlives the run that needed it is how a project silently keeps publishing something nobody
 * remembers agreeing to.
 */
USTRUCT(BlueprintType)
struct CONVAIPAKMANAGER_API FCPM_PublishOptions
{
	GENERATED_BODY()

	/** The Platform Selection. Read only when bOverridePlatforms; order does not matter. */
	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	TArray<ECPM_Platform> Platforms;

	/**
	 * Whether Platforms replaces what the Policy asks for.
	 *
	 * Separate from an empty Platforms so that "this run builds no Pak, send the archive alone" is
	 * expressible and distinct from "this caller expressed no preference".
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	bool bOverridePlatforms = false;

	/**
	 * Publish the Pak already on disk instead of cooking a new one, for this run only.
	 *
	 * The per-run form of the bUseExistingPakFile debug setting, and the only form a creator is
	 * offered: a Pak built before the current edits publishes the content it was built from, and
	 * nothing downstream can tell that from a fresh one. A platform with no usable Pak on disk is
	 * packaged normally either way.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	bool bReuseExistingPaks = false;
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

	/** The Policy with this run's Platform Selection already applied. What every Job reads. */
	UPROPERTY()
	FCPM_PublishPolicy Policy;

	/** See FCPM_PublishOptions::bReuseExistingPaks. Read by the packaging Job, per run. */
	UPROPERTY()
	bool bReuseExistingPaks = false;
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

	/**
	 * The Version slot a platform occupies, e.g. "ue-5.8-Windows", or "raw" for the Raw Project
	 * Archive. Empty for a platform that has no Version.
	 *
	 * Built from the RUNNING engine rather than a stored value: a Pak is only loadable by the engine
	 * that cooked it, so the two can never legitimately differ, and reading it from anywhere else is
	 * a way for them to.
	 *
	 * Public because deleting one Version means naming its slot, and a caller that spells the name
	 * itself is a caller that can spell it differently from the one that wrote it.
	 */
	static FString VersionSlotFor(ECPM_Platform Platform);
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

	/**
	 * Keyed by Version slot. NOT how the server returns them - it keys upload_urls by what the
	 * artefact is ("scene_asset") and mints one per call - so each URL is filed here under the
	 * Version the call that minted it named.
	 */
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
