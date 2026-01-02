// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CPM_Utils.h"
#include "IImageWrapper.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/Texture2D.h"
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
	static void GetAssetID(FString& AssetID, const int32& ChunkId);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static ECPM_AssetType GetAssetType(const int32& ChunkId);
	
	// Create asset utility functions
	UFUNCTION(BlueprintCallable, Category="Convai|PakManager")
	static bool SaveConvaiCreateAssetData(const FString& ResponseString, const int32& ChunkId);

	UFUNCTION(BlueprintCallable, Category="Convai|PakManager")
	static bool LoadConvaiCreateAssetData(FCPM_CreatedAssets& OutData, const int32& ChunkId);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FString GetCreateAssetDataFilePath(const int32& ChunkId);
	// END Create asset utility functions

	// Asset metadata utility functions
	UFUNCTION(BlueprintCallable, Category="Convai|PakManager")
	static bool SaveConvaiAssetMetadata(const FString& ResponseString, const int32& ChunkId);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Convai|PakManager")
	static void GetAssetMetaDataString(FString& MetaData, const int32& ChunkId);

	/** Get parsed asset metadata struct from chunk ID */
	UFUNCTION(BlueprintCallable, Category="Convai|PakManager")
	static bool CPM_GetAssetMetadata(FCPM_AssetMetadata& OutMetadata, const int32& ChunkId);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FString GetPakMetadataFilePath(const int32& ChunkId);
	// END Asset metadata utility functions

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FString CPM_GetCacheDirectory();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FString CPM_GetRawProjectZipPath();
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FString GetPackageDirectory();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FString GetPakFilePathFromChunkID(const ECPM_Platform Platform, const int32& ChunkID);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Convai|PakManager")
	static void GetModdingMetadata(FCPM_ModdingMetadata& OutData, const int32& ChunkId);

	UFUNCTION(BlueprintCallable, Category="Convai|PakManager")
	static bool CreateModdingMetadata(const FCPM_ModdingMetadata& InData, const int32& ChunkId);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FString GetModdingMetadataFilePath(const int32& ChunkId);
	
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager")
	static bool GetCreatedAssetsFromJSON(const FString& JsonString, FCPM_CreatedAssets& OutCreatedAssets);
		
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static bool ShouldCreateAsset(const int32& ChunkId);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ConvaiPakManagerLog"), Category = "Convai|PakManager")
	static void CPM_LogMessage(const FString& Message, ECPM_LogLevel Verbosity = ECPM_LogLevel::Log);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FString GetPythonScriptDirectory();
	
	/** Get the ui default directory path (PluginDir/Resources/UI/Defaults/) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FString CPM_GetUIDefaultsDirectory();

	/** Get a specific default JSON file path by filename (e.g., "InitialAssetInfo.json") */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FString CPM_GetUIDefaultsFilePath(const FString& Filename);

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

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static UTexture2D* CPM_LoadTexture2DFromDisk(const FString& FilePath, bool bGenerateMips = true);

	// Project Zipping utility functions
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static TArray<FString> GetProjectDirectoriesToZip();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static TArray<FString> GetProjectFilesToZip();
	// END Project Zipping utility functions

	UFUNCTION(BlueprintCallable, BlueprintPure,Category = "Convai|PakManager")
	static int32 GetPrimaryAssetLabelChunkId(const FString& AssetPath);
	
	UFUNCTION(BlueprintCallable, Category = "Convai|System|Environment")
	static bool CPM_SetSystemEnvVar(const FString& VarName, const FString& VarValue);

	UFUNCTION(BlueprintCallable, Category = "Convai|System|Environment")
	static bool CPM_GetSystemEnvVar(const FString& VarName, FString& OutVarValue);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|System|Environment")
	static int64 CPM_GetFileSize(const FString& FilePath);
	
	static FString CPM_GetPluginDirectory();
	static bool Texture2DToPixels(UTexture2D* Texture2D, int32& Width, int32& Height, TArray<FColor>& Pixels);
	static bool Texture2DToBytes(UTexture2D* Texture2D, const EImageFormat ImageFormat, TArray<uint8>& ByteArray, const int32 CompressionQuality);
	static bool PixelsToBytes(const int32 Width, const int32 Height, const TArray<FColor>& Pixels, const EImageFormat ImageFormat, TArray<uint8>& ByteArray, const int32 CompressionQuality);
	static bool ExtractAssetListFromResponseString(const FString& ResponseString, FCPM_AssetResponse& AssetResponse);
};
