// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiPakManager.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "Chunk/CPM_Chunk.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/ConfigCacheIni.h"
#include "ToolMenus.h"
#include "UI/CPM_PakManagerStyle.h"
#include "UI/SCPM_PakManagerPanel.h"
#include "Utility/CPM_Log.h"
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
	// Browser, which the pick-the-selection workflow depends on. See docs/adr/0009.
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
	using namespace ConvaiPakManager::Chunk;

	// The label set is known for the first time here, and anything cached before now was answered
	// mid-scan.
	InvalidateSoleChunkCache();

	TArray<FString> MovedFiles;
	switch (MigrateLegacyLayout(MovedFiles))
	{
	case EMigrationResult::Migrated:
		CPM_LOG(Display, TEXT("Moved %d file(s) into this project's per-Chunk state directory."), MovedFiles.Num());
		break;

	case EMigrationResult::Ambiguous:
	case EMigrationResult::Failed:
		// MigrateLegacyLayout has already logged which files and why. Nothing was moved, so the
		// creator's own copy of their AssetID is still where it was.
		break;

	case EMigrationResult::NothingToMigrate:
		break;
	}

	// Everything still loose is production's by definition - nothing that could reach another
	// backend has shipped. The URL comes from the SETTINGS rather than UConvaiURL::GetBaseURL,
	// which honours -ConvaiProdURL= on the command line: one CI launch against staging would file a
	// production record under staging permanently, where nothing would ever look for it again.
	//
	// Read through GConfig because UConvaiSettings lives at the SDK module's root, outside its
	// Public folder, so its header is not includable from here. The default is the SDK's own, from
	// UConvaiURL::GetBaseURL.
	FString ProdUrl;
	GConfig->GetString(TEXT("/Script/Convai.ConvaiSettings"), TEXT("CustomProdURL"), ProdUrl, GEngineIni);
	ProdUrl.TrimStartAndEndInline();
	if (ProdUrl.IsEmpty())
	{
		ProdUrl = TEXT("https://api.convai.com");
	}

	const FString ProductionSlug = EnvironmentSlug(ProdUrl);
	for (const FCPM_Chunk& Chunk : Discover())
	{
		TArray<FString> Adopted;
		if (AdoptLooseRecords(Chunk.Id, ProductionSlug, Adopted) == EMigrationResult::Migrated)
		{
			CPM_LOG(Display, TEXT("Adopted %d loose record(s) of chunk %d into %s: %s"),
				Adopted.Num(), Chunk.Id, *ProductionSlug, *FString::Join(Adopted, TEXT(", ")));
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FConvaiPakManagerModule, ConvaiPakManager)
