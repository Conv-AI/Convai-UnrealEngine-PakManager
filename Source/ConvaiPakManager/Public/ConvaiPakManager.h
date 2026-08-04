// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Modules/ModuleManager.h"


class FToolBarBuilder;
class FMenuBuilder;

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

	/**
	 * Supplies the Pak Manager page to the Convai editor shell, once.
	 *
	 * On first use rather than at startup: the SDK's editor module loads in the same phase as this
	 * one, so its dependency container may not exist yet and asking for it then asserts.
	 */
	bool EnsureShellPageRegistered();

	/** Adds the Tools menu entry. Deferred by ToolMenus until menus exist. */
	void RegisterMenuEntry();
	void BuildMenuEntry();

	/** Registers the page if it has not been, then navigates the shell to it. */
	void OpenPakManager();

	bool bShellPageRegistered = false;

	/** Set only while waiting for the Asset Registry's initial scan. */
	FDelegateHandle FilesLoadedHandle;
};
