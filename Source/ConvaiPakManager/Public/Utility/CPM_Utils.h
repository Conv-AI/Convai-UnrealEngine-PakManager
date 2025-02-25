// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CPM_Utils.generated.h"

USTRUCT(BlueprintType)
struct FConvaiCreatePakAssetParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Avatar")
	TArray<FString> Tags;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Avatar")
	FString MetaData;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Avatar")
	FString Version;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Avatar")
	FString Entity_Type;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Avatar")
	UTexture2D* Thumbnail;

	FConvaiCreatePakAssetParams()
		: MetaData(TEXT("")),
		  Version(TEXT("")),
		  Entity_Type(TEXT("")),
		  Thumbnail(nullptr)
	{
	}
};

