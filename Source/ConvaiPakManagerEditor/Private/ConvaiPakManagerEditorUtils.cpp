// Fill out your copyright notice in the Description page of Project Settings.


#include "ConvaiPakManagerEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "Editor.h"                  
#include "LevelEditor.h"              
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "PlayInEditorDataTypes.h"    
#include "IUATHelperModule.h"
#include "Misc/Paths.h"
#include "Logging/LogMacros.h"

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

void PackageProjectUsingUATHelper(const FString& ProjectFilePath, const FString& OutputDirectory, 
                                  const FString& Platform = TEXT("Win64"), 
                                  const FString& Configuration = TEXT("Shipping"))
{
#if WITH_EDITOR
    if (ProjectFilePath.IsEmpty() || OutputDirectory.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Project file or output directory is empty."));
        return;
    }

    // Retrieve the current executable path to pass to UAT (similar to the "-unrealexe" flag in PackageProject.bat).
    const FString UnrealExe = FPlatformProcess::ExecutablePath();
    const FString ExtraParams = FString::Printf(TEXT(" -unrealexe=\"%s\""), *UnrealExe);
    
    // Construct the BuildCookRun command line.
    const FString CommandLine = FString::Printf(
        TEXT("BuildCookRun -project=\"%s\" -noP4 -platform=%s -clientconfig=%s -serverconfig=%s -cook -build -stage -pak -archive -archivedirectory=\"%s\"%s"),
        *ProjectFilePath,
        *Platform,
        *Configuration,
        *Configuration,
        *OutputDirectory,
        *ExtraParams
    );

    UE_LOG(LogTemp, Log, TEXT("Packaging command line: %s"), *CommandLine);

    IUATHelperModule::Get().CreateUatTask(
        CommandLine,
        FText::FromString(Platform),                   // Display name (target platform)
        FText::FromString(TEXT("Packaging Project")),  // Full task name for notifications
        FText::FromString(TEXT("Packaging")),          // Short task name
        nullptr,                                       // Task icon (optional)
        [](FString Result, double Runtime)
        {
            // Callback when packaging task completes.
            UE_LOG(LogTemp, Log, TEXT("Packaging result: %s, runtime: %f seconds"), *Result, Runtime);
        }
    );
#else
    UE_LOG(LogTemp, Warning, TEXT("Packaging can only be executed in the Editor."));
#endif
}

void UConvaiPakManagerEditorUtils::CPM_PackageProject()
{
	PackageProjectUsingUATHelper(FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()),  FPaths::Combine(FPaths::ProjectDir(), TEXT("PackagedApp")));
}
