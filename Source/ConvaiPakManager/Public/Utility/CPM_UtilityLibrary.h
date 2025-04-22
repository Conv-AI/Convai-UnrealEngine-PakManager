// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CPM_Utils.h"
#include "IImageWrapper.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CPM_UtilityLibrary.generated.h"

struct FCPM_CreatedAssets;

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
	static void GetAssetID(FString& AssetID);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static ECPM_AssetType GetAssetType();
	
	// Create asset utility functions
	UFUNCTION(BlueprintCallable, Category="Convai|PakManager")
	static bool SaveConvaiCreateAssetData(const FString& ResponseString);

	UFUNCTION(BlueprintCallable, Category="Convai|PakManager")
	static bool LoadConvaiCreateAssetData(FCPM_CreatedAssets& OutData);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FString GetCreateAssetDataFilePath();
	// END Create asset utility functions

	// Asset metadata utility functions
	UFUNCTION(BlueprintCallable, Category="Convai|PakManager")
	static bool SaveConvaiAssetMetadata(const FString& ResponseString);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Convai|PakManager")
	static void GetAssetMetaDataString(FString& MetaData);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FString GetPakMetadataFilePath();
	// END Asset metadata utility functions

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FString GetPackageDirectory();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FString GetPakFilePathFromChunkID(const FString& ChunkID);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Convai|PakManager")
	static void GetModdingMetadata(FCPM_ModdingMetadata& OutData);
	
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager")
	static bool GetCreatedAssetsFromJSON(const FString& JsonString, FCPM_CreatedAssets& OutCreatedAssets);
		
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static bool ShouldCreateAsset();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ConvaiPakManagerLog"), Category = "Convai|PakManager")
	static void CPM_LogMessage(const FString& Message, ECPM_LogLevel Verbosity = ECPM_LogLevel::Log);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FString GetPythonScriptDirectory();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static UClass* CPM_LoadClassByPath(const FString& ClassPath);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static UObject* CPM_LoadAssetByPath(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FAssetData CPM_LoadAssetDataByPath(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager")
	static bool CPM_DeleteFileByPath(const FString& FilePath);

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager")
	static bool CPM_DeleteDirectory(const FString& DirectoryPath);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static bool CPM_IsThumbnailValid(UTexture2D* Texture, float MinValidRatio = 0.01f, int32 SampleStep = 1);
	
	static bool Texture2DToPixels(UTexture2D* Texture2D, int32& Width, int32& Height, TArray<FColor>& Pixels);
	static bool Texture2DToBytes(UTexture2D* Texture2D, const EImageFormat ImageFormat, TArray<uint8>& ByteArray, const int32 CompressionQuality);
	static bool PixelsToBytes(const int32 Width, const int32 Height, const TArray<FColor>& Pixels, const EImageFormat ImageFormat, TArray<uint8>& ByteArray, const int32 CompressionQuality);
	static bool ExtractAssetListFromResponseString(const FString& ResponseString, FCPM_AssetResponse& AssetResponse);
};
