// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"

class FToolBarBuilder;
class FMenuBuilder;
class SWindow;
class SCPM_PakManagerWindow;

class FConvaiPakManagerEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Spawn/focus the Pak Manager tab */
	void SpawnPakManagerTab();

	/** Get the module instance */
	static FConvaiPakManagerEditorModule& Get();

	/** Tab ID for the Pak Manager */
	static const FName PakManagerTabId;

private:
	/** Register the nomad tab spawner */
	void RegisterTabSpawner();

	/** Unregister the tab spawner */
	void UnregisterTabSpawner();

	/** Register menus - called when editor is ready */
	void RegisterMenus();

	/** Register toolbar button */
	void RegisterToolbarExtension();

	/** Called when editor is fully initialized */
	void OnEditorInitialized();

	/** Callback to spawn the tab */
	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);

	/** The Pak Manager widget instance */
	TSharedPtr<SCPM_PakManagerWindow> PakManagerWidget;

	/** Handle for editor initialized delegate */
	FDelegateHandle EditorInitializedHandle;
};
