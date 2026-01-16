// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Utility/CPM_Utils.h"
#include "CPM_DependencyCopyAPI.h"
#include "ConvaiPakManagerEditorUtils.generated.h"

struct FCPM_PackageParam;

//Used to callback into calling code when a UAT task completes. First param is the result type, second param is the runtime in sec.
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnUatTaskResultCallack, const FString&, Result, double, Runtime);

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
	static void CPM_PackageProject(const FCPM_PackageParam& PackageParam, FOnUatTaskResultCallack OnPackagingCompleted);

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManagerEditor")
	static void CPM_ToggleLiveCoding(const bool Enable = false);

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManagerEditor")
	static void CPM_ShowPluginContent(const bool bEnable = false);

	UFUNCTION(BlueprintCallable, Category = "Convai|PakManagerEditor")
	static void CPM_SetEngineScalability(ECPM_CustomScalabilityLevel Level);
	
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

	// ==================================================================================
	// Dependency Copy API - Blueprint Wrappers
	// ==================================================================================

	/**
	 * Copies a package and all its dependencies into a destination module/plugin.
	 * Handles both Game and Engine dependencies correctly.
	 * 
	 * @param SourcePackage     The package to copy (e.g., /Game/MyFolder/MyAsset)
	 * @param DestinationRoot   Target mount point (e.g., "/MyPlugin/" or "/MyPlugin/Content/")
	 * @param Options           Options controlling the copy behavior
	 * @param OutReport         Report detailing what was copied, skipped, or failed
	 * @return                  True if the operation succeeded (no failures)
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|DependencyCopy", meta = (AutoCreateRefTerm = "Options"))
	static bool CopyPackageWithDependencies(
		const FName& SourcePackage,
		const FString& DestinationRoot,
		const FCPM_DependencyCopyOptions& Options,
		FCPM_DependencyCopyReport& OutReport
	);

	/**
	 * Copies multiple packages and all their dependencies into a destination module/plugin.
	 * 
	 * @param SourcePackages    Array of packages to copy
	 * @param DestinationRoot   Target mount point
	 * @param Options           Options controlling the copy behavior
	 * @param OutReport         Report detailing what was copied, skipped, or failed
	 * @return                  True if the operation succeeded (no failures)
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|DependencyCopy", meta = (AutoCreateRefTerm = "Options"))
	static bool CopyPackagesWithDependencies(
		const TArray<FName>& SourcePackages,
		const FString& DestinationRoot,
		const FCPM_DependencyCopyOptions& Options,
		FCPM_DependencyCopyReport& OutReport
	);

	/**
	 * Checks if a package is an Engine asset or from a non-project plugin.
	 * 
	 * @param PackageName   The package to check
	 * @return              True if this is an Engine/non-project asset
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager|DependencyCopy")
	static bool IsEnginePackage(const FName& PackageName);

	/**
	 * Generates the destination package path for a source package.
	 * Useful for previewing where packages will be copied.
	 * 
	 * @param SourcePackage     The source package name
	 * @param DestinationRoot   The destination root path
	 * @param DestinationSubdir Optional subdirectory under destination
	 * @return                  The computed destination package name
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager|DependencyCopy")
	static FName GetDestinationPackagePath(
		const FName& SourcePackage,
		const FString& DestinationRoot,
		const FString& DestinationSubdir
	);

	/**
	 * Creates default options for dependency copy with common settings.
	 * 
	 * @return Default options configured for typical use case
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|PakManager|DependencyCopy")
	static FCPM_DependencyCopyOptions MakeDefaultDependencyCopyOptions();

	/**
	 * Analyzes dependencies for a package without copying.
	 * Returns counts of Engine vs Game dependencies.
	 * 
	 * @param PackageName           The package to analyze
	 * @param Options               Options for dependency gathering
	 * @param OutTotalDependencies  Total number of dependencies
	 * @param OutEngineDependencies Number of Engine dependencies
	 * @param OutGameDependencies   Number of Game dependencies
	 * @param OutDependencyList     List of all dependency package names
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|DependencyCopy", meta = (AutoCreateRefTerm = "Options"))
	static void AnalyzePackageDependencies(
		const FName& PackageName,
		const FCPM_DependencyCopyOptions& Options,
		int32& OutTotalDependencies,
		int32& OutEngineDependencies,
		int32& OutGameDependencies,
		TArray<FName>& OutDependencyList
	);
};
