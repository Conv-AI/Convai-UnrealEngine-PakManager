// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiPakManager.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "Chunk/CPM_Chunk.h"
#include "Services/ConvaiDIContainer.h"
#include "Services/NavigationService.h"
#include "ToolMenus.h"
#include "UI/CPM_PakManagerPageFactory.h"
#include "UI/Factories/PageFactoryManager.h"
#include "Utility/CPM_Log.h"

#define LOCTEXT_NAMESPACE "FConvaiPakManagerModule"

void FConvaiPakManagerModule::StartupModule()
{
	// Only the menu entry, and even that is deferred by ToolMenus itself. The shell page is
	// registered on first use rather than here: the SDK's editor module loads in the same phase as
	// this one, so its dependency container may not exist yet, and asking for it at startup asserts.
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
	if (FilesLoadedHandle.IsValid())
	{
		if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
		{
			AssetRegistry->OnFilesLoaded().Remove(FilesLoadedHandle);
		}
		FilesLoadedHandle.Reset();
	}
}

bool FConvaiPakManagerModule::EnsureShellPageRegistered()
{
	if (bShellPageRegistered)
	{
		return true;
	}

	// Resolved on first use, not at startup. The SDK's dependency container is built by its own
	// editor module, which loads in the same phase as this one - so at startup it may not exist, and
	// asking for it then asserts rather than answering. By the time a creator opens the panel the
	// shell is up by definition, because the shell is what they opened.
	IConvaiDIContainer& Container = FConvaiDIContainerManager::Get();
	const auto Resolved = Container.Resolve<IPageFactoryManager>();
	if (Resolved.IsFailure() || !Resolved.GetValue().IsValid())
	{
		CPM_LOG(Warning, TEXT("Convai editor shell unavailable; the Pak Manager page was not registered."));
		return false;
	}

	// The SDK declares the route; this supplies what fills it. See docs/adr/0007.
	Resolved.GetValue()->RegisterFactory(MakeShared<FCPM_PakManagerPageFactory>());
	bShellPageRegistered = true;
	return true;
}

void FConvaiPakManagerModule::RegisterMenuEntry()
{
	// The entry point lives HERE rather than in the SDK's header bar, which builds its navigation
	// from a hard-coded list. Keeping it on this side means every line that knows this plugin exists
	// is in this plugin, and the SDK carries a route it never has to reason about.
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
		LOCTEXT("OpenPakManagerTooltip", "Publish this project's chunks to Convai."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FConvaiPakManagerModule::OpenPakManager)));
}

void FConvaiPakManagerModule::OpenPakManager()
{
	if (!EnsureShellPageRegistered())
	{
		return;
	}

	IConvaiDIContainer& Container = FConvaiDIContainerManager::Get();
	const auto Navigation = Container.Resolve<INavigationService>();
	if (Navigation.IsFailure() || !Navigation.GetValue().IsValid())
	{
		CPM_LOG(Warning, TEXT("Convai editor navigation unavailable; cannot open the Pak Manager."));
		return;
	}

	Navigation.GetValue()->Navigate(ConvaiEditor::Route::E::PakManager);
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
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FConvaiPakManagerModule, ConvaiPakManager)
