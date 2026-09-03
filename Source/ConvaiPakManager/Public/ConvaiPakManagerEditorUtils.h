// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Utility/CPM_Utils.h"
#include "ConvaiPakManagerEditorUtils.generated.h"

struct FCPM_PackageParam;

//Used to callback into calling code when a UAT task completes. First param is the result type, second param is the runtime in sec.
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnUatTaskResultCallack, const FString&, Result, double, Runtime);

UCLASS()
class CONVAIPAKMANAGER_API UConvaiPakManagerEditorUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManagerEditor")
	static void CPM_PackageProject(const FCPM_PackageParam& PackageParam, FOnUatTaskResultCallack OnPackagingCompleted);

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager")
	static bool CPM_TakeViewportScreenshot(const FString& FilePath);

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager")
	static bool CPM_CreateZip(const FString& ZipFilePath, const TArray<FString>& Files, const TArray<FString>& Directories);

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager")
	static void CPM_CreateZipAsync(const FString& ZipFilePath, const TArray<FString>& Files, const TArray<FString>& Directories, FOnUatTaskResultCallack OnZippingCompleted);

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager")
	static AActor* SpawnAndSnapActorToView(UClass* ActorClass);

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager")
	static bool GetPackageDependencies(const FName& PackageName, const TArray<FString>& FilterPaths, TSet<FName>& AllDependencies, TSet<FString>& ExternalObjectsPaths, TSet<FName>& ExcludedDependencies);

	static void RecursiveGetDependencies(const FName& PackageName, TSet<FName>& AllDependencies, TSet<FString>& ExternalObjectsPaths, TSet<FName>& ExcludedDependencies, const TFunction<bool(FName)>& ShouldExcludeFromDependenciesSearch);
};
