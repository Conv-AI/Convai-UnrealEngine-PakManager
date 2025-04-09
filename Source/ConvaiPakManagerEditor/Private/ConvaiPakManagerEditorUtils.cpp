// Fill out your copyright notice in the Description page of Project Settings.


#include "ConvaiPakManagerEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "Editor.h"                  
#include "LevelEditor.h"              
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "PlayInEditorDataTypes.h"    
#endif

void UConvaiPakManagerEditorUtils::CPM_MarkAssetDirty(UObject* Asset)
{
	if (!Asset)
	{
		UE_LOG(LogTemp, Warning, TEXT("MarkAssetDirty: Asset is null."));
		return;
	}

	UPackage* Package = Asset->GetOutermost();
	if (Package)
	{
		Package->SetDirtyFlag(true);
		FAssetRegistryModule::AssetCreated(Asset);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("MarkAssetDirty: Could not get package for asset '%s'."), *Asset->GetName());
	}
}

#if WITH_EDITOR
static TSharedPtr<IAssetViewport> GetActiveAssetViewport()
{
	if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		return LevelEditorModule.GetFirstActiveViewport();
	}
	return nullptr;
}
#endif

void UConvaiPakManagerEditorUtils::CPM_TogglePlayMode()
{
#if WITH_EDITOR
	
	if (!GEditor->PlayWorld)
	{
		FRequestPlaySessionParams PlayParams;
		if (const TSharedPtr<IAssetViewport> ActiveViewport = GetActiveAssetViewport(); ActiveViewport.IsValid())
		{
			const TWeakPtr<IAssetViewport> WeakViewport(ActiveViewport);
			PlayParams.DestinationSlateViewport = WeakViewport;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("No valid active viewport found. Launching PIE in a new editor window."));
		}
        
		GEditor->RequestPlaySession(PlayParams);
	}
	else
	{
		GEditor->RequestEndPlayMap();
	}
#endif
}