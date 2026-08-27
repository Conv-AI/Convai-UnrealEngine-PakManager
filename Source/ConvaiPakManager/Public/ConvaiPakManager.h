// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FSpawnTabArgs;
class SDockTab;

class FConvaiPakManagerModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/**
	 * Moves a pre-Chunk ConvaiEssentials layout into its per-Chunk directory, once the Asset
	 * Registry can answer which Chunk this project has.
	 *
	 * Runs before any UI opens, because reading Chunk state without migrating first reports a
	 * published Asset as absent - and publishing from there creates a duplicate and orphans the
	 * original permanently.
	 */
	void MigrateChunkStateLayout();

	/** Adds the Tools menu entry. Deferred by ToolMenus until menus exist. */
	void RegisterMenuEntry();
	void BuildMenuEntry();

	/** Foregrounds the Pak Manager tab, spawning it when it is not open. */
	void OpenPakManager();

	TSharedRef<SDockTab> SpawnPakManagerTab(const FSpawnTabArgs& Args);

	/** Set only while waiting for the Asset Registry's initial scan. */
	FDelegateHandle FilesLoadedHandle;
};
