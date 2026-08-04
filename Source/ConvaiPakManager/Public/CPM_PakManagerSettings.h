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
};
