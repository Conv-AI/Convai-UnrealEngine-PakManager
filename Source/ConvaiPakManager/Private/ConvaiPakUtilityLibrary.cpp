// Fill out your copyright notice in the Description page of Project Settings.

#include "ConvaiPakUtilityLibrary.h"
#include "DesktopPlatformModule.h"
#include "IPlatformFilePak.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"

DEFINE_LOG_CATEGORY(LogConvaiPakManager);

bool UConvaiPakUtilityLibrary::IsFileValid(const FString& FilePath)
{
	return FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath);
}

bool UConvaiPakUtilityLibrary::LoadFileToByteArray(const FString& FilePath, TArray<uint8>& FileData)
{
	if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
	{
		UE_LOG(LogConvaiPakManager, Error, TEXT("Failed to load file: %s"), *FilePath);
		return false;
	}
	return true;
}

FString UConvaiPakUtilityLibrary::OpenFileDialog(const TArray<FString>& Extensions)
{
	FString FilePath;
	FString FileTypes = TEXT("Supported Files (");
	
	// Validate input
	if (Extensions.Num() == 0)
	{
		UE_LOG(LogConvaiPakManager, Warning, TEXT("Extensions array is empty"));
		FileTypes = TEXT("All Files (*.*)|*.*");
	}
	else
	{
		// Construct file filter string
		for (int32 Index = 0; Index < Extensions.Num(); ++Index)
		{
			if (Index > 0)
			{
				FileTypes += TEXT(";");
			}
			FileTypes += TEXT("*.") + Extensions[Index];
		}
	
		FileTypes += TEXT(")|");
		for (int32 Index = 0; Index < Extensions.Num(); ++Index)
		{
			if (Index > 0)
			{
				FileTypes += TEXT(";");
			}
			FileTypes += TEXT("*.") + Extensions[Index];
		}
	}	
	
	// Get the Desktop Platform module
	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
	{
		// File Dialog Options
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		TArray<FString> OutFiles;
		const bool bOpened = DesktopPlatform->OpenFileDialog(
			ParentWindowHandle,
			TEXT("Choose a file"),             // Dialog title
			FPaths::ProjectDir(),					// Default path
			TEXT(""),								// Default file
			FileTypes,								// File types
			EFileDialogFlags::None,
			OutFiles
		);

		if (bOpened && OutFiles.Num() > 0)
		{
			FilePath = OutFiles[0]; 
		}
	}

	return FilePath;
}

FString UConvaiPakUtilityLibrary::GetProjectName()
{
	return FApp::GetProjectName();
}

bool UConvaiPakUtilityLibrary::ValidatePakFile(const FString& PakFilePath)
{
	if (!FPaths::FileExists(PakFilePath))
	{
		UE_LOG(LogConvaiPakManager, Error, TEXT("Pak file does not exist: %s"), *PakFilePath);
		return false;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FPakPlatformFile* PakPlatformFile = new FPakPlatformFile();

	if (!PakPlatformFile->Initialize(&PlatformFile, TEXT("")))
	{
		UE_LOG(LogConvaiPakManager, Error, TEXT("Failed to initialize PakPlatformFile."));
		delete PakPlatformFile;
		return false;
	}

	if (!PakPlatformFile->Mount(*PakFilePath, 0, *FPaths::ProjectContentDir()))
	{
		UE_LOG(LogConvaiPakManager, Error, TEXT("Failed to mount Pak file: %s"), *PakFilePath);
		delete PakPlatformFile;
		return false;
	}

	PakPlatformFile->Unmount(*PakFilePath);
	
	delete PakPlatformFile;
	return true;
}