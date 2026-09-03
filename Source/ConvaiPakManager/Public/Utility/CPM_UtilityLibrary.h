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

	/** True when a Pak is on disk at this path and mounts - what "already packaged" means everywhere. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager")
	static bool IsPakUsable(const FString& PakFilePath);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static ECPM_AssetType GetAssetType();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FString CPM_GetCacheDirectory();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FString CPM_GetRawProjectZipPath();
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FString GetPackageDirectory();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static FString GetPakFilePathFromChunkID(const ECPM_Platform Platform, const FString& ChunkID);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Convai|PakManager")
	static void GetModdingMetadata(FCPM_ModdingMetadata& OutData);

	/** What the Modding Tool decided about one Chunk. The overload above asks for the sole Chunk's. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Convai|PakManager", meta = (DisplayName = "Get Modding Metadata For Chunk"))
	static void GetModdingMetadataForChunk(int32 ChunkId, FCPM_ModdingMetadata& OutData);
	
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager")
	static bool GetCreatedAssetsFromJSON(const FString& JsonString, FCPM_CreatedAssets& OutCreatedAssets);

	/**
	 * The upload URL an assets/upload or assets/update response minted, from either shape it comes in.
	 *
	 * One call names one Version and is answered with one URL, but the key it arrives under names
	 * the artefact ("scene_asset"), not the Version - so the key is no use to a caller and only the
	 * value is returned.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager")
	static bool GetMintedUploadUrl(const FString& ResponseString, FString& OutUrl);

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

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static UTexture2D* CPM_LoadTexture2DFromDisk(const FString& FilePath, bool bGenerateMips = true);

	// Project Zipping utility functions
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static TArray<FString> GetProjectDirectoriesToZip();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	static TArray<FString> GetProjectFilesToZip();
	// END Project Zipping utility functions

	UFUNCTION(BlueprintCallable, Category = "Convai|System|Environment")
	static bool CPM_SetSystemEnvVar(const FString& VarName, const FString& VarValue);

	UFUNCTION(BlueprintCallable, Category = "Convai|System|Environment")
	static bool CPM_GetSystemEnvVar(const FString& VarName, FString& OutVarValue);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|System|Environment")
	static int64 CPM_GetFileSize(const FString& FilePath);
	
	static bool Texture2DToPixels(UTexture2D* Texture2D, int32& Width, int32& Height, TArray<FColor>& Pixels);
	static bool Texture2DToBytes(UTexture2D* Texture2D, const EImageFormat ImageFormat, TArray<uint8>& ByteArray, const int32 CompressionQuality);
	static bool PixelsToBytes(const int32 Width, const int32 Height, const TArray<FColor>& Pixels, const EImageFormat ImageFormat, TArray<uint8>& ByteArray, const int32 CompressionQuality);
	static bool ExtractAssetListFromResponseString(const FString& ResponseString, FCPM_AssetResponse& AssetResponse);
};
