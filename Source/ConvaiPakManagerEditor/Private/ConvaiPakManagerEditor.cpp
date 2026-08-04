// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiPakManagerEditor.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "Chunk/CPM_Chunk.h"
#include "Services/ConvaiDIContainer.h"
#include "Services/NavigationService.h"
#include "ToolMenus.h"
#include "UI/CPM_PakManagerPageFactory.h"
#include "UI/Factories/PageFactoryManager.h"
#include "Utility/CPM_Log.h"

#define LOCTEXT_NAMESPACE "FConvaiPakManagerEditorModule"

void FConvaiPakManagerEditorModule::StartupModule()
{
	RegisterShellPage();
	RegisterMenuEntry();

	// Deferred to OnFilesLoaded rather than run here: migration has to know which Chunk the project
	// has, that comes from discovering Primary Asset Labels, and at module startup the Asset
	// Registry has not finished scanning. Asking early would see no labels and refuse to migrate.
	if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		if (AssetRegistry->IsLoadingAssets())
		{
			FilesLoadedHandle = AssetRegistry->OnFilesLoaded().AddRaw(
				this, &FConvaiPakManagerEditorModule::MigrateChunkStateLayout);
		}
		else
		{
			MigrateChunkStateLayout();
		}
	}
}

void FConvaiPakManagerEditorModule::ShutdownModule()
{
	if (FilesLoadedHandle.IsValid())
	{
		if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
		{
			AssetRegistry->OnFilesLoaded().Remove(FilesLoadedHandle);
		}
		FilesLoadedHandle.Reset();
	}
}

void FConvaiPakManagerEditorModule::RegisterShellPage()
{
	// The SDK declares the route; this supplies what fills it. Registered unconditionally - if the
	// SDK's shell is never opened, nothing asks for the page and nothing is built. See docs/adr/0007.
	IConvaiDIContainer& Container = FConvaiDIContainerManager::Get();
	const auto Resolved = Container.Resolve<IPageFactoryManager>();
	if (Resolved.IsFailure() || !Resolved.GetValue().IsValid())
	{
		CPM_LOG(Warning, TEXT("Convai editor shell unavailable; the Pak Manager page was not registered."));
		return;
	}

	Resolved.GetValue()->RegisterFactory(MakeShared<FCPM_PakManagerPageFactory>());
}

void FConvaiPakManagerEditorModule::RegisterMenuEntry()
{
	// The entry point lives HERE rather than in the SDK's header bar, which builds its navigation
	// from a hard-coded list. Keeping it on this side means every line that knows this plugin exists
	// is in this plugin, and the SDK carries a route it never has to reason about.
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
		if (!Menu)
		{
			return;
		}

		FToolMenuSection& Section = Menu->FindOrAddSection(
			TEXT("Convai"), LOCTEXT("ConvaiSection", "Convai"));

		Section.AddMenuEntry(
			TEXT("ConvaiPakManager"),
			LOCTEXT("OpenPakManager", "Pak Manager"),
			LOCTEXT("OpenPakManagerTooltip", "Publish this project's chunks to Convai."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]
			{
				IConvaiDIContainer& Container = FConvaiDIContainerManager::Get();
				const auto Navigation = Container.Resolve<INavigationService>();
				if (Navigation.IsSuccess() && Navigation.GetValue().IsValid())
				{
					Navigation.GetValue()->Navigate(ConvaiEditor::Route::E::PakManager);
				}
			})));
	}));
}

void FConvaiPakManagerEditorModule::MigrateChunkStateLayout()
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
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FConvaiPakManagerEditorModule, ConvaiPakManagerEditor)
