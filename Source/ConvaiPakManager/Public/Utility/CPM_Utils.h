// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CPM_Utils.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct FCPM_CreatePakAssetParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Convai|PakManager")
	TArray<FString> Tags;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Convai|PakManager")
	FString MetaData;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Convai|PakManager")
	FString Version;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Convai|PakManager")
	FString Entity_Type;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Convai|PakManager")
	UTexture2D* Thumbnail;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Convai|PakManager")
	FString Visiblity;

	FCPM_CreatePakAssetParams()
		: MetaData(TEXT("")),
		  Version(TEXT("")),
		  Entity_Type(TEXT("")),
		  Thumbnail(nullptr),
		  Visiblity(TEXT(""))
	{
	}
};

USTRUCT(BlueprintType)
struct FCPM_EntityData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString SceneName;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString SceneDescription;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString SceneMetaData;
};

USTRUCT(BlueprintType)
struct FCPM_AssetMetadata
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString Version;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString SceneId;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString EntityId;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString RootPath;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString AssetType;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString LevelName;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FCPM_EntityData EntityData;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString ContentPath;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString ProjectName;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString BlueprintClass;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString BlueprintClassPath;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString AssetName;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString AssetDescription;
};

USTRUCT(BlueprintType)
struct FCPM_AssetDetails
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString AssetId;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString GCPFileName;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString FileName;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FCPM_AssetMetadata Metadata;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString MetadataString;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	TArray<FString> Versions;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString ThumbnailGCPPath;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	TArray<FString> Tags;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString UploadedOn;
};

USTRUCT(BlueprintType)
struct FCPM_SceneDetails
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString SceneId;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString BuildId;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString OwnerId;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString SceneName;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString SceneDescription;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString SceneThumbnail;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString Visibility;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString CreatedOn;
};

USTRUCT(BlueprintType)
struct FCPM_UploadUrls
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	TMap<FString, FString> UploadURLsMap;
};

USTRUCT(BlueprintType)
struct FCPM_Asset
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FCPM_AssetDetails Asset;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FCPM_SceneDetails Scene;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FCPM_UploadUrls UploadUrls;
};

USTRUCT(BlueprintType)
struct FCPM_CreatedAssets
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	FString TransactionID;

	UPROPERTY(BlueprintReadWrite, Category = "Convai|PakManager")
	TArray<FCPM_Asset> Assets;
};

UENUM(BlueprintType)
enum class ECPM_LogLevel : uint8
{
	Log UMETA(DisplayName = "Log"),
	Warning UMETA(DisplayName = "Warning"),
	Error UMETA(DisplayName = "Error"),
};

USTRUCT(BlueprintType)
struct FCPM_AssetData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset Data")
	FString asset_id;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset Data")
	FString gcp_file_name;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset Data")
	FString file_name;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset Data")
	TArray<FString> tags;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset Data")
	FString metadata;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset Data")
	FString uploaded_on;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset Data")
	FString signed_url;
};

// Main response struct containing the transaction ID and array of assets
USTRUCT(BlueprintType)
struct FCPM_AssetResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset Response")
	FString transactionID;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset Response")
	TArray<FCPM_AssetData> assets;
};

USTRUCT(BlueprintType)
struct FCPM_ModdingMetadata
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset Response")
	FString ProjectName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset Response")
	FString PluginName;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset Response")
    FString AssetType;
};

UENUM(BlueprintType)
enum class ECPM_AssetManagerStatus : uint8
{
	Max					UMETA(DisplayName = "None"),
	
	Packaging_Begin     UMETA(DisplayName = "Packaging Started"),
	Packaging_Success   UMETA(DisplayName = "Packaging Completed"),
	Packaging_Failed    UMETA(DisplayName = "Packaging Failed"),

	Create_Begin        UMETA(DisplayName = "Creating Asset"),
	Create_Success      UMETA(DisplayName = "Created Asset"),
	Create_Failed       UMETA(DisplayName = "Create Asset Failed"),

	Update_Begin        UMETA(DisplayName = "Updating Asset"),
	Update_Success      UMETA(DisplayName = "Updated Asset"),
	Update_Failed       UMETA(DisplayName = "Update Asset Failed"),

	UploadPak_Begin        UMETA(DisplayName = "Uploading Asset"),
	UploadPak_Success      UMETA(DisplayName = "Uploaded Asset"),
	UploadPak_Failed       UMETA(DisplayName = "Upload Asset Failed"),
	
	Delete_Begin        UMETA(DisplayName = "Deleting Asset"),
	Delete_Success      UMETA(DisplayName = "Deleted Asset"),
	Delete_Failed       UMETA(DisplayName = "Delete Asset Failed")
};

UENUM(BlueprintType)
enum class ECPM_AssetType : uint8
{
	Max					UMETA(DisplayName = "None"),
	Avatar				UMETA(DisplayName = "Avatar"),
	Scene				UMETA(DisplayName = "Scene"),
};

UENUM(BlueprintType)
enum class ECPM_CustomScalabilityLevel : uint8
{
	Low         UMETA(DisplayName = "Low"),
	Medium      UMETA(DisplayName = "Medium"),
	High        UMETA(DisplayName = "High"),
	Epic        UMETA(DisplayName = "Epic"),
	Cinematic   UMETA(DisplayName = "Cinematic")
};

UENUM(BlueprintType)
enum class ECPM_Platform : uint8
{
	Windows         UMETA(DisplayName = "Windows"),
	Linux			UMETA(DisplayName = "Linux"),
};