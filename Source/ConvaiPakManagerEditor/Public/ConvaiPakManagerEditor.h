// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Modules/ModuleManager.h"


class FToolBarBuilder;
class FMenuBuilder;

class FConvaiPakManagerEditorModule : public IModuleInterface
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

	/** Set only while waiting for the Asset Registry's initial scan. */
	FDelegateHandle FilesLoadedHandle;
};
