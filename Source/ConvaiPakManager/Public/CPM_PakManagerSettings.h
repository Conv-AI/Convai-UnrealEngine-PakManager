// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CPM_PakManagerSettings.generated.h"

/**
 * Project settings for the Pak Manager, held in DefaultGame.ini.
 *
 * A project setting rather than a build flag so that one branch and one shipped artefact serve both
 * a creator's project and an internal one - the difference between them lives in project config,
 * where every other difference between those projects already lives. See docs/adr/0003.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Convai Pak Manager"))
class CONVAIPAKMANAGER_API UCPM_PakManagerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	static const UCPM_PakManagerSettings& Get()
	{
		return *GetDefault<UCPM_PakManagerSettings>();
	}

	/**
	 * How many Chunks this project may publish. Zero or less means no limit.
	 *
	 * Defaults to one so that a project which never sets it is limited - a creator project generated
	 * without this key still behaves as documented, rather than inheriting internal behaviour by
	 * omission. Internal projects raise it deliberately.
	 *
	 * A stated policy, not an enforcement boundary: the plugin ships as source, so this documents
	 * intent for creators who will never modify it and defends nothing against one who does.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Publishing",
		meta = (DisplayName = "Max Chunks Per Project", ClampMin = "0"))
	int32 MaxChunksPerProject = 1;

	/** True when this project may not gain another Chunk beyond the ExistingChunkCount it already has. */
	bool IsAtChunkLimit(const int32 ExistingChunkCount) const
	{
		return MaxChunksPerProject > 0 && ExistingChunkCount >= MaxChunksPerProject;
	}

	/**
	 * Publish the Pak already sitting in PackagedApp instead of cooking a new one.
	 *
	 * For iterating on everything downstream of the cook - upload, metadata, the Convai side - where
	 * minutes of packaging per attempt is the entire cost of a run.
	 *
	 * Off by default, and deliberately not something to leave on: a Pak built before the current
	 * edits publishes the content it was built from, and nothing downstream of here - not the
	 * upload, not Convai, not the creator - can tell that from a fresh one. A platform with no
	 * usable Pak on disk is still packaged normally.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Publishing",
		meta = (DisplayName = "Use Existing Pak File"))
	bool bUseExistingPakFile = false;

	/**
	 * Publish without archiving and uploading the project again, leaving the Asset the Raw Project
	 * Archive its last Publish put there.
	 *
	 * The other half of an iteration loop: the archive is the whole project, so re-sending it is the
	 * longest step of a Publish that changed nothing about the project's source.
	 *
	 * Honoured only where an archive actually reached Convai for this Chunk's Asset, which is why
	 * this is phrased as reusing a published archive rather than as disabling an upload - Convai
	 * rebuilds an Asset for a future engine version from that archive, so an Asset that never
	 * received one can never be rebuilt, and nothing downstream would notice for months.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Publishing",
		meta = (DisplayName = "Reuse Published Raw Project Archive"))
	bool bReusePublishedRawArchive = false;

	/**
	 * Whether a Publish must produce and send the Raw Project Archive.
	 *
	 * Subtracts from the Publish Policy and can never add to it: a Policy that asks for no archive
	 * still gets none whatever the creator set, because which Versions an Asset carries is Convai's
	 * decision and not a creator's. See CONTEXT.md.
	 *
	 * bPublishingPaks is not a detail of the creator's choice but of what is left: a Policy asking
	 * for the archive alone would, with the archive reused, leave the Publish with nothing to send.
	 */
	bool ShouldArchiveRawProject(
		const bool bPolicyAsksForIt,
		const bool bHasPublishedArchive,
		const bool bPublishingPaks) const
	{
		return bPolicyAsksForIt && !(bReusePublishedRawArchive && bHasPublishedArchive && bPublishingPaks);
	}

	/** Repository holding the Publish Policy Convai publishes. */
	UPROPERTY(config, EditAnywhere, Category = "Publish Policy")
	FString PolicyRepository = TEXT("Conv-AI/Convai-UnrealEngine-ModdingTool");

	/**
	 * Branch or tag to read the Publish Policy from.
	 *
	 * A tag makes a policy change ship deliberately; tracking a branch makes it reach every creator
	 * the instant it merges, with no step in between where anyone looks at it.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Publish Policy")
	FString PolicyRef = TEXT("main");

	UPROPERTY(config, EditAnywhere, Category = "Publish Policy")
	FString PolicyPath = TEXT("resources/asset_uploader_config.json");

	/**
	 * Read the Publish Policy from this file instead of from the repository. Empty to use the repository.
	 *
	 * The escape hatch for publishing that must not depend on a public repository being reachable -
	 * internal pipelines, and anyone genuinely offline. Set deliberately, per project.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Publish Policy", meta = (FilePathFilter = "json"))
	FString PolicyOverrideFile;
};
