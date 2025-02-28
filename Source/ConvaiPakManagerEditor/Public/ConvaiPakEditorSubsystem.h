// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "ConvaiPakEditorSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class CONVAIPAKMANAGEREDITOR_API UConvaiPakEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()
public:
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager")
	void GetSelectedAssetPackageName(FString& PackageName);
};
