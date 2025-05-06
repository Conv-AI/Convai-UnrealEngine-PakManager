// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Utility/CPM_Utils.h"
#include "ConvaiPakManagerEditorUtils.generated.h"

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnPackagingCompleted, const FString&, Result, double, Runtime);

UCLASS()
class CONVAIPAKMANAGEREDITOR_API UConvaiPakManagerEditorUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Marks the given asset as dirty so it can be saved */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManagerEditor")
	static void CPM_MarkAssetDirty(UObject* Asset);

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManagerEditor", meta = (CallInEditor = "true"))
	static void CPM_TogglePlayMode();

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManagerEditor")
	static void CPM_PackageProject(FOnPackagingCompleted OnPackagingCompleted);

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManagerEditor")
	static void CPM_ToggleLiveCoding(const bool Enable = false);

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManagerEditor")
	static void CPM_ShowPluginContent(const bool bEnable = false);

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManagerEditor")
	static void CPM_SetEngineScalability(ECPM_CustomScalabilityLevel Level);
	
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager")
	static bool CPM_TakeViewportScreenshot(const FString& FilePath);
};
