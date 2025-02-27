// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CPM_Utils.generated.h"

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

	FCPM_CreatePakAssetParams()
		: MetaData(TEXT("")),
		  Version(TEXT("")),
		  Entity_Type(TEXT("")),
		  Thumbnail(nullptr)
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
