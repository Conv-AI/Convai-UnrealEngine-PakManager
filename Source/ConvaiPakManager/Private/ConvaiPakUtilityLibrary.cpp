// Fill out your copyright notice in the Description page of Project Settings.

#include "ConvaiPakUtilityLibrary.h"

#include "DesktopPlatformModule.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"

bool UConvaiPakUtilityLibrary::IsFileValid(const FString& FilePath)
{
	return FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath);
}

bool UConvaiPakUtilityLibrary::LoadFileToByteArray(const FString& FilePath, TArray<uint8>& FileData)
{
	if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load file: %s"), *FilePath);
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
		UE_LOG(LogTemp, Warning, TEXT("Extensions array is empty"));
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
