// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiPakManager.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "Chunk/CPM_Chunk.h"
#include "ConvaiUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Proxy/CPM_Proxy.h"
#include "Utility/CPM_Log.h"
#include "Framework/Docking/TabManager.h"
#include "ToolMenus.h"
#include "UI/CPM_PakManagerStyle.h"
#include "UI/SCPM_PakManagerPanel.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FConvaiPakManagerModule"

namespace
{
	const FName PakManagerTabId(TEXT("ConvaiPakManager"));
}

void FConvaiPakManagerModule::StartupModule()
{
	FCPM_PakManagerStyle::Initialize();

	// A nomad tab of this plugin's own, not a page in the SDK's shell: it docks beside the Content
	// Browser, which picking the Entry Point from the selection depends on. See docs/adr/0009.
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PakManagerTabId,
		FOnSpawnTab::CreateRaw(this, &FConvaiPakManagerModule::SpawnPakManagerTab))
		.SetDisplayName(LOCTEXT("PakManagerTabTitle", "Convai Pak Manager"))
		.SetTooltipText(LOCTEXT("PakManagerTabTooltip", "Publish this project's Chunks to Convai."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(FSlateIcon(FCPM_PakManagerStyle::GetStyleSetName(), TEXT("CPM.TabIcon")));

	RegisterMenuEntry();

	// Deferred to OnFilesLoaded rather than run here: migration has to know which Chunk the project
	// has, that comes from discovering Primary Asset Labels, and at module startup the Asset
	// Registry has not finished scanning. Asking early would see no labels and refuse to migrate.
	if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		if (AssetRegistry->IsLoadingAssets())
		{
			FilesLoadedHandle = AssetRegistry->OnFilesLoaded().AddRaw(
				this, &FConvaiPakManagerModule::MigrateChunkStateLayout);
		}
		else
		{
			MigrateChunkStateLayout();
		}
	}
}

void FConvaiPakManagerModule::ShutdownModule()
{
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PakManagerTabId);
	}
	FCPM_PakManagerStyle::Shutdown();

	if (FilesLoadedHandle.IsValid())
	{
		if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
		{
			AssetRegistry->OnFilesLoaded().Remove(FilesLoadedHandle);
		}
		FilesLoadedHandle.Reset();
	}
}

TSharedRef<SDockTab> FConvaiPakManagerModule::SpawnPakManagerTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SCPM_PakManagerPanel> Panel = SNew(SCPM_PakManagerPanel);

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		// Foregrounding is a refresh trigger: Chunks and their records change while the tab is hidden.
		.OnTabActivated_Lambda([WeakPanel = TWeakPtr<SCPM_PakManagerPanel>(Panel)](TSharedRef<SDockTab>, ETabActivationCause)
		{
			if (const TSharedPtr<SCPM_PakManagerPanel> Pinned = WeakPanel.Pin())
			{
				Pinned->RefreshProject();
			}
		})
		[
			Panel
		];
}

void FConvaiPakManagerModule::RegisterMenuEntry()
{
	// The entry point lives HERE rather than in the SDK's header bar, which builds its navigation
	// from a hard-coded list. Keeping it on this side means every line that knows this plugin exists
	// is in this plugin.
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
		this, &FConvaiPakManagerModule::BuildMenuEntry));
}

void FConvaiPakManagerModule::BuildMenuEntry()
{
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
	if (!Menu)
	{
		return;
	}

	FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("Convai"), LOCTEXT("ConvaiSection", "Convai"));

	Section.AddMenuEntry(
		TEXT("ConvaiPakManager"),
		LOCTEXT("OpenPakManager", "Pak Manager"),
		LOCTEXT("OpenPakManagerTooltip", "Publish this project's Chunks to Convai."),
		FSlateIcon(FCPM_PakManagerStyle::GetStyleSetName(), TEXT("CPM.TabIcon")),
		FUIAction(FExecuteAction::CreateRaw(this, &FConvaiPakManagerModule::OpenPakManager)));
}

void FConvaiPakManagerModule::OpenPakManager()
{
	FGlobalTabmanager::Get()->TryInvokeTab(PakManagerTabId);
}

void FConvaiPakManagerModule::MigrateChunkStateLayout()
{
	ConvaiPakManager::Chunk::ReconcileStateLayout();
	RefreshPublishedAssets();
}

void FConvaiPakManagerModule::RefreshPublishedAssets()
{
	// Gated on the key rather than left to fail per Chunk: without one every request refuses in the
	// SDK before it is sent, and a project with a hundred Chunks would log a hundred failures at
	// every launch for a creator who simply has not pasted their key in yet.
	if (!UConvaiFormValidation::ValidateAuthKey(UConvaiUtils::GetAuthHeaderAndKey().Value))
	{
		return;
	}

	// Resolved once. Asking again inside a response callback would file the answer under whichever
	// backend the creator had switched to by then.
	const FString EnvironmentSlug = ConvaiPakManager::Chunk::CurrentEnvironmentSlug();

	int32 Asked = 0;
	for (const FCPM_Chunk& Chunk : ConvaiPakManager::Chunk::Discover())
	{
		const FString AssetId = ConvaiPakManager::Chunk::ReadAssetId(Chunk.Id, EnvironmentSlug);
		if (AssetId.IsEmpty())
		{
			// Never published to this backend, so there is nothing to disagree with.
			continue;
		}

		UCPM_GetAssetProxy::GetAssetProxy(AssetId, Chunk.Id, EnvironmentSlug)->Activate();
		++Asked;
	}

	if (Asked > 0)
	{
		CPM_LOG(Log, TEXT("Refreshing %d published asset(s) from %s."), Asked, *EnvironmentSlug);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FConvaiPakManagerModule, ConvaiPakManager)
