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
	 * Runs Chunk::ReconcileStateLayout once the Asset Registry can answer which Chunks this project
	 * has, so that no UI opens against a layout this version cannot read: reading Chunk state
	 * un-migrated reports a published Asset as absent, and publishing from there creates a duplicate
	 * and orphans the original permanently.
	 *
	 * Boot is only the first caller. The subsystem re-runs the reconcile after minting a label and
	 * the panel on every refresh, because a project can gain its first Chunk mid-session.
	 *
	 * Kept as a member for the OnFilesLoaded binding, which needs a raw delegate target.
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
