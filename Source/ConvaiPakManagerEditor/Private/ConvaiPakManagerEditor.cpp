// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiPakManagerEditor.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "Chunk/CPM_Chunk.h"
#include "Utility/CPM_Log.h"

#define LOCTEXT_NAMESPACE "FConvaiPakManagerEditorModule"

void FConvaiPakManagerEditorModule::StartupModule()
{
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
