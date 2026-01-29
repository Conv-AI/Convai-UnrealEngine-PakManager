// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiPakManagerEditor.h"
#include "UI/SCPM_PakManagerWindow.h"
#include "Utility/CPM_Log.h"
#include "Services/CPM_WorkflowService.h"
#include "Services/CPM_ConfigService.h"

#include "ToolMenus.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"
#include "Misc/CoreDelegates.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FConvaiPakManagerEditorModule"

// Static tab ID
const FName FConvaiPakManagerEditorModule::PakManagerTabId("ConvaiPakManagerTab");

FConvaiPakManagerEditorModule& FConvaiPakManagerEditorModule::Get()
{
	return FModuleManager::LoadModuleChecked<FConvaiPakManagerEditorModule>("ConvaiPakManagerEditor");
}

void FConvaiPakManagerEditorModule::StartupModule()
{
	CPM_LOG(Log, TEXT("ConvaiPakManagerEditor module starting up"));

	// Initialize services
	FCPM_ConfigServiceManager::Initialize();
	FCPM_WorkflowServiceManager::Initialize();

	// Register the tab spawner immediately
	RegisterTabSpawner();

	// Wait for editor to be fully initialized before registering menus
	EditorInitializedHandle = FCoreDelegates::OnBeginFrame.AddLambda([this]()
	{
		if (GEditor && FSlateApplication::IsInitialized() && UToolMenus::IsToolMenuUIEnabled())
		{
			FCoreDelegates::OnBeginFrame.Remove(EditorInitializedHandle);
			EditorInitializedHandle.Reset();
			OnEditorInitialized();
		}
	});
}

void FConvaiPakManagerEditorModule::ShutdownModule()
{
	CPM_LOG(Log, TEXT("ConvaiPakManagerEditor module shutting down"));

	if (EditorInitializedHandle.IsValid())
	{
		FCoreDelegates::OnBeginFrame.Remove(EditorInitializedHandle);
		EditorInitializedHandle.Reset();
	}

	// Unregister menus
	UToolMenus::UnregisterOwner(this);

	// Unregister tab spawner
	UnregisterTabSpawner();

	PakManagerWidget.Reset();

	// Shutdown services (reverse order of initialization)
	FCPM_WorkflowServiceManager::Shutdown();
	FCPM_ConfigServiceManager::Shutdown();
}

void FConvaiPakManagerEditorModule::RegisterTabSpawner()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		PakManagerTabId,
		FOnSpawnTab::CreateRaw(this, &FConvaiPakManagerEditorModule::SpawnTab))
		.SetDisplayName(LOCTEXT("PakManagerTabTitle", "Pak Manager"))
		.SetTooltipText(LOCTEXT("PakManagerTabTooltip", "Open the Convai Pak Manager"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.GameSettings"))
		.SetMenuType(ETabSpawnerMenuType::Hidden); // We control visibility via our own menu

	CPM_LOG(Log, TEXT("Registered Pak Manager tab spawner"));
}

void FConvaiPakManagerEditorModule::UnregisterTabSpawner()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PakManagerTabId);
	CPM_LOG(Log, TEXT("Unregistered Pak Manager tab spawner"));
}

TSharedRef<SDockTab> FConvaiPakManagerEditorModule::SpawnTab(const FSpawnTabArgs& Args)
{
	// Create the widget
	PakManagerWidget = SNew(SCPM_PakManagerWindow);

	// Create and return the dock tab
	TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("PakManagerTabLabel", "Pak Manager"))
		.ToolTipText(LOCTEXT("PakManagerTabTooltip", "Convai Pak Manager - Create and manage pak files"))
		[
			PakManagerWidget.ToSharedRef()
		];

	// Optional: Handle tab close to cleanup
	DockTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([this](TSharedRef<SDockTab>)
	{
		PakManagerWidget.Reset();
		CPM_LOG(Log, TEXT("Pak Manager tab closed"));
	}));

	CPM_LOG(Log, TEXT("Spawned Pak Manager tab"));
	return DockTab;
}

void FConvaiPakManagerEditorModule::OnEditorInitialized()
{
	CPM_LOG(Log, TEXT("Editor initialized - registering Pak Manager menus"));
	
	RegisterMenus();
	RegisterToolbarExtension();
}

void FConvaiPakManagerEditorModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	// Add to the Window menu
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
	FToolMenuSection& Section = Menu->FindOrAddSection("Convai");
	Section.Label = LOCTEXT("ConvaiMenuLabel", "Convai");

	Section.AddMenuEntry(
		"OpenPakManager",
		LOCTEXT("OpenPakManagerLabel", "Pak Manager"),
		LOCTEXT("OpenPakManagerTooltip", "Open the Convai Pak Manager window"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.GameSettings"),
		FUIAction(FExecuteAction::CreateRaw(this, &FConvaiPakManagerEditorModule::SpawnPakManagerTab))
	);

	CPM_LOG(Log, TEXT("Registered Pak Manager menu entry"));
}

void FConvaiPakManagerEditorModule::RegisterToolbarExtension()
{
	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
	FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("ConvaiPakManager");

	Section.AddEntry(FToolMenuEntry::InitToolBarButton(
		"OpenPakManager",
		FUIAction(FExecuteAction::CreateRaw(this, &FConvaiPakManagerEditorModule::SpawnPakManagerTab)),
		LOCTEXT("PakManagerToolbarLabel", "Pak Manager"),
		LOCTEXT("PakManagerToolbarTooltip", "Open the Convai Pak Manager window"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.GameSettings")
	));

	CPM_LOG(Log, TEXT("Registered Pak Manager toolbar button"));
}

void FConvaiPakManagerEditorModule::SpawnPakManagerTab()
{
	// This will either create the tab or focus it if it already exists
	FGlobalTabmanager::Get()->TryInvokeTab(PakManagerTabId);
	CPM_LOG(Log, TEXT("Invoked Pak Manager tab"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FConvaiPakManagerEditorModule, ConvaiPakManagerEditor)
