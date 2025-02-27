// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "IImageWrapper.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CPM_UtilityLibrary.generated.h"

struct FCPM_CreatedAssets;
DECLARE_LOG_CATEGORY_EXTERN(LogConvaiPakManager, Log, All);

UCLASS()
class CONVAIPAKMANAGER_API UCPM_UtilityLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager")
	static FString OpenFileDialog(const TArray<FString>& Extensions);

	UFUNCTION(BlueprintPure, Category = "Convai|PakManager")
	static FString GetProjectName();

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager")
	static bool ValidatePakFile(const FString& PakFilePath);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FString ExtractAssetID();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static bool GetCreatedAssetsFromJSON(const FString& JsonString, FCPM_CreatedAssets& OutCreatedAssets);
	
	static bool Texture2DToPixels(UTexture2D* Texture2D, int32& Width, int32& Height, TArray<FColor>& Pixels);
	static bool Texture2DToBytes(UTexture2D* Texture2D, const EImageFormat ImageFormat, TArray<uint8>& ByteArray, const int32 CompressionQuality);
	static bool PixelsToBytes(const int32 Width, const int32 Height, const TArray<FColor>& Pixels, const EImageFormat ImageFormat, TArray<uint8>& ByteArray, const int32 CompressionQuality);
};
