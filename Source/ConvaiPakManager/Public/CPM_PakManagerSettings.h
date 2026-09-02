// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Publish/CPM_PublishTypes.h"
#include "CPM_PakManagerSettings.generated.h"

/**
 * Where a Publish reads its Publish Policy from.
 *
 * An explicit choice rather than "whichever override happens to be filled in": with three ways to
 * state a policy, precedence by emptiness makes a half-edited field silently outrank the one being
 * read, and neither the creator nor the log can see which won.
 */
UENUM()
enum class ECPM_PolicySource : uint8
{
	/** Convai's own, fetched from the repository. What every creator project uses. */
	Repository UMETA(DisplayName = "Convai repository"),

	/** A JSON file on disk, in the shape Convai publishes. */
	OverrideFile UMETA(DisplayName = "Override: JSON file"),

	/** A JSON document typed into these settings, in the shape Convai publishes. */
	OverrideText UMETA(DisplayName = "Override: JSON text"),

	/** The fields below, with no JSON anywhere. */
	OverrideSettings UMETA(DisplayName = "Override: these settings"),
};

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
	 * Send the Raw Project Archive - the creator's project, zipped - alongside the Paks.
	 *
	 * On by default because Convai repackages an Asset for a new engine version from it, with
	 * nothing asked of the creator. Off, that Asset can only reach a new engine version by the
	 * creator publishing it again themselves.
	 *
	 * A creator may turn it off at any point, including before the first Publish: the archive is
	 * the whole project, so its upload is the longest step of a Publish, and an iteration loop that
	 * cannot skip it is an iteration loop nobody uses. The UI says what is given up before the
	 * choice takes effect; it is not this setting's job to refuse.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Publishing",
		meta = (DisplayName = "Upload Raw Project Archive"))
	bool bUploadRawProjectArchive = true;

	/**
	 * Whether a Publish must produce and send the Raw Project Archive.
	 *
	 * Subtracts from the Publish Policy and can never add to it: a Policy that asks for no archive
	 * still gets none whatever the creator set, because which Versions an Asset carries is Convai's
	 * decision and not a creator's. See CONTEXT.md.
	 */
	bool ShouldArchiveRawProject(const bool bPolicyAsksForIt) const
	{
		return bPolicyAsksForIt && bUploadRawProjectArchive;
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
	 * Where the Publish Policy comes from.
	 *
	 * The overrides exist for publishing that must not depend on a public repository being
	 * reachable - internal pipelines, a policy being trialled, anyone genuinely offline. A creator
	 * project leaves this on the repository; overriding it is a deliberate, per-project act.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Publish Policy Override")
	ECPM_PolicySource PolicySource = ECPM_PolicySource::Repository;

	/** Read the policy from this JSON file, in the shape Convai publishes. */
	UPROPERTY(config, EditAnywhere, Category = "Publish Policy Override", meta = (FilePathFilter = "json",
		EditCondition = "PolicySource == ECPM_PolicySource::OverrideFile"))
	FString PolicyOverrideFile;

	/**
	 * The policy itself, as JSON, in the shape Convai publishes.
	 *
	 * For pasting a policy verbatim out of the repository or a ticket, where retyping it into the
	 * fields below would be a chance to get it subtly wrong.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Publish Policy Override", meta = (MultiLine = true,
		EditCondition = "PolicySource == ECPM_PolicySource::OverrideText"))
	FString PolicyOverrideJson;

	/**
	 * The policy stated as fields, for a project that just wants to change what it builds.
	 *
	 * Held to what a fetched policy is held to - a platform being packaged names a configuration,
	 * and a policy produces something - so an override cannot express what Convai's own cannot.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Publish Policy Override",
		meta = (EditCondition = "PolicySource == ECPM_PolicySource::OverrideSettings"))
	FCPM_PublishPolicy PolicyOverride = FCPM_PublishPolicy::Defaults();

	/**
	 * True when this project states its own Policy and that Policy asks for no Raw Project Archive.
	 *
	 * For the UI, which otherwise offers an Upload checkbox that cannot do anything: the Policy is
	 * what decides whether an archive is published at all, and the creator's setting only subtracts
	 * from it. Answered only for the typed override - the other sources are a file read and a JSON
	 * parse, which is not work to do from a widget's paint.
	 */
	bool PolicyExcludesRawArchive() const
	{
		return PolicySource == ECPM_PolicySource::OverrideSettings && !PolicyOverride.bUploadRawProject;
	}

#if WITH_EDITOR
	/**
	 * Fills what a half-stated override leaves empty, at the moment it is stated.
	 *
	 * A default on the property only reaches a project that has never written the key - config on
	 * disk outranks it - so a project that saved these settings before this shipped would keep an
	 * empty configuration for a platform it has ticked, and be refused at publish time for a field
	 * it was never shown a value for.
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
