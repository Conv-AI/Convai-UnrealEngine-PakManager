// Fill out your copyright notice in the Description page of Project Settings.


#include "ConvaiPakManagerEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "Editor.h"                  
#include "ILiveCodingModule.h"
#include "LevelEditor.h"              
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "PlayInEditorDataTypes.h"    
#include "IUATHelperModule.h"
#include "Async/Async.h"
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

void UConvaiPakManagerEditorUtils::CPM_PackageProject(FOnPackagingCompleted OnPackagingCompleted)
{
	const FString ProjectFilePath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	const FString OutputDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("PackagedApp"));
	const FString Platform = TEXT("Win64");
	const FString Configuration = TEXT("Shipping");

	#if WITH_EDITOR
    if (ProjectFilePath.IsEmpty() || OutputDirectory.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Project file or output directory is empty."));
        return;
    }

    const FString UnrealExe = FPlatformProcess::ExecutablePath();
    const FString ExtraParams = FString::Printf(TEXT(" -unrealexe=\"%s\""), *UnrealExe);

    FString CommandLine = FString::Printf(
        TEXT("BuildCookRun -project=\"%s\" -noP4 -platform=%s -clientconfig=%s -serverconfig=%s -cook -build -stage -pak -archive -archivedirectory=\"%s\"%s"),
        *ProjectFilePath,
        *Platform,
        *Configuration,
        *Configuration,
        *OutputDirectory,
        *ExtraParams
    );

    UE_LOG(LogTemp, Log, TEXT("Packaging command line: %s"), *CommandLine);

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    // In UE 5.3, use the new signature:
    IUATHelperModule::Get().CreateUatTask(
        CommandLine,
        FText::FromString(Platform),                   // PlatformDisplayName
        FText::FromString(TEXT("Packaging Project")),  // TaskName
        FText::FromString(TEXT("Packaging")),          // TaskShortName
        nullptr,                                       // TaskIcon (optionally supply a valid FSlateBrush*)
        /*OptionalAnalyticsParamArray=*/ nullptr,       // Analytics parameter added in UE 5.3
        [=](FString Result, double Runtime)
        {
        	AsyncTask(ENamedThreads::Type::GameThread, [=]()
			{
				(void)OnPackagingCompleted.ExecuteIfBound(Result, Runtime);	
			});
        },
        FString()                                      // ResultLocation (default empty)
    );
#else
    IUATHelperModule::Get().CreateUatTask(
        CommandLine,
        FText::FromString(Platform),                   // PlatformDisplayName
        FText::FromString(TEXT("Packaging Project")),  // TaskName
        FText::FromString(TEXT("Packaging")),          // TaskShortName
        nullptr,                                       // TaskIcon
        [=](FString Result, double Runtime)
        {
        	AsyncTask(ENamedThreads::Type::GameThread, [=]()
        	{
        		(void)OnPackagingCompleted.ExecuteIfBound(Result, Runtime);	
        	});        	
        },
        FString()                                      // ResultLocation
    );
#endif

#else
    UE_LOG(LogTemp, Warning, TEXT("Packaging can only be executed in the Editor."));
#endif
}

void UConvaiPakManagerEditorUtils::CPM_ToggleLiveCoding(const bool Enable)
{
	if (ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME))
	{
		LiveCoding->EnableByDefault(Enable);

		if (LiveCoding->IsEnabledByDefault() && !LiveCoding->IsEnabledForSession())
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::FromString(TEXT("NoEnableLiveCodingAfterHotReloadLive Coding cannot be enabled while hot-reloaded modules are active. Please close the editor and build from your IDE before restarting.")));
		}
	}
}
