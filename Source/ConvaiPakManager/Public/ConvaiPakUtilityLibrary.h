// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ConvaiPakUtilityLibrary.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogConvaiPakManager, Log, All);

UCLASS()
class CONVAIPAKMANAGER_API UConvaiPakUtilityLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static bool IsFileValid(const FString& FilePath);

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager")
	static bool LoadFileToByteArray(const FString& FilePath, TArray<uint8>& FileData);
	
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager")
	static FString OpenFileDialog(const TArray<FString>& Extensions);

	UFUNCTION(BlueprintPure, Category = "Convai|Utilities")
	static FString GetProjectName();

	UFUNCTION(BlueprintCallable, Category = "Convai|Pak Utilities")
	static bool ValidatePakFile(const FString& PakFilePath);
};
