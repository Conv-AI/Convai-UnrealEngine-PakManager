// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AdvancedCopyCustomization.h"
#include "CPM_DependencyCopyAPI.generated.h"

/**
 * Specifies whether to copy or move assets during the dependency copy operation.
 * Note: Move is only valid for non-Engine assets.
 */
UENUM(BlueprintType)
enum class ECPM_DependencyCopyOp : uint8
{
	/** Copy assets to destination (leaves originals in place) */
	Copy,
	/** Move assets to destination (only valid for non-Engine assets) */
	Move
};

/**
 * Policy for handling Engine module dependencies.
 */
UENUM(BlueprintType)
enum class ECPM_EngineDependencyPolicy : uint8
{
	/** Copy engine assets into destination (do NOT modify Engine content) */
	CopyIntoDestination,
	/** Skip engine assets entirely (caller accepts external engine deps) */
	Skip,
	/** Fail if any engine asset is required */
	Fail
};

/**
 * Options for controlling the dependency copy operation.
 */
USTRUCT(BlueprintType)
struct CONVAIPAKMANAGEREDITOR_API FCPM_DependencyCopyOptions
{
	GENERATED_BODY()

	// Initializes sensible defaults for automated dependency copy.
	FCPM_DependencyCopyOptions()
		: Operation(ECPM_DependencyCopyOp::Copy)                              // Copy assets (not move)
		, EnginePolicy(ECPM_EngineDependencyPolicy::CopyIntoDestination)      // Keep PAK self-contained
		, bIncludeSoftDependencies(true)                                      // Include soft refs
		, bIncludeHardDependencies(true)                                      // Include hard refs
		, bIncludeSearchableNameDependencies(false)                           // Skip name-only deps
		, bCopyIfAlreadyInDestination(false)                                  // Avoid re-copying
		, DestinationSubdir(TEXT(""))                                         // Example: "Imported/MyFeature"
		, bFixupRedirectors(true)                                             // Fix redirectors after copy/move
		, bSaveAfterCopy(true)                                                // Save touched packages
		, bSuppressUI(true)                                                   // Automation-friendly (no dialogs)
		, bOverwriteExisting(true)                                            // Replace existing destination assets
		, ExcludedPaths()                                                     // Example: {"/Game/ThirdParty"}
		, ExcludedModules()                                                   // Example: {"Engine", "MyPlugin"}
		, ExcludedPackages()                                                  // Example: {"/Game/UI/WBP_Debug"}
	{
	}

	/** Copy vs Move (move only allowed for non-engine assets) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dependency Copy")
	ECPM_DependencyCopyOp Operation;

	/** How to handle Engine content dependencies */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dependency Copy")
	ECPM_EngineDependencyPolicy EnginePolicy;

	/** Include soft references (AssetRegistry "soft" deps) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dependency Copy")
	bool bIncludeSoftDependencies;

	/** Include hard package references */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dependency Copy")
	bool bIncludeHardDependencies;

	/** Include searchable-name deps (optional; usually safe to keep off) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dependency Copy")
	bool bIncludeSearchableNameDependencies;

	/** Include assets already under destination root (usually false to avoid duplicates) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dependency Copy")
	bool bCopyIfAlreadyInDestination;

	/** Optional subfolder under destination root (empty = place directly under root). Example: "Imported/MyFeature" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dependency Copy")
	FString DestinationSubdir;

	/** Fix redirectors after copy/move (recommended when moving/renaming is involved) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dependency Copy")
	bool bFixupRedirectors;

	/** Save packages when done (recommended for automation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dependency Copy")
	bool bSaveAfterCopy;

	/** Suppress Advanced Copy UI (recommended for commandlets/CI) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dependency Copy")
	bool bSuppressUI;

	/** Overwrite existing destination assets (true = replace, false = keep existing) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dependency Copy")
	bool bOverwriteExisting;

	/** Skip dependencies under these path prefixes. Example: "/Game/ThirdParty" skips "/Game/ThirdParty/..." */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dependency Copy|Exclusions")
	TArray<FString> ExcludedPaths;

	/** Skip dependencies under these mount points/modules. Example: "Engine" skips "/Engine/..." */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dependency Copy|Exclusions")
	TArray<FString> ExcludedModules;

	/** Skip these exact packages (exact match). Example: "/Game/UI/WBP_Debug" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dependency Copy|Exclusions")
	TArray<FName> ExcludedPackages;
};

/**
 * Information about a single dependency item in the copy plan.
 */
USTRUCT(BlueprintType)
struct CONVAIPAKMANAGEREDITOR_API FCPM_DependencyCopyItem
{
	GENERATED_BODY()

	/** Source package name (e.g., /Game/MyAsset) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency Copy")
	FName SourcePackage;

	/** Destination package name (e.g., /MyPlugin/MyAsset) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency Copy")
	FName DestPackage;

	/** True if this is an Engine/non-project asset */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency Copy")
	bool bIsEngineAsset = false;

	/** True if this item was included in the copy plan */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency Copy")
	bool bPlanned = false;

	/** True if the copy/move operation succeeded */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency Copy")
	bool bCopiedOrMoved = false;

	/** True if this item was skipped (e.g., Engine asset with Skip policy) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency Copy")
	bool bSkipped = false;

	/** Error message if the operation failed */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency Copy")
	FString Error;
};

/**
 * Report returned after a dependency copy operation.
 */
USTRUCT(BlueprintType)
struct CONVAIPAKMANAGEREDITOR_API FCPM_DependencyCopyReport
{
	GENERATED_BODY()

	/** True if the overall operation succeeded */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency Copy")
	bool bSuccess = false;

	/** Detailed information about each item processed */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency Copy")
	TArray<FCPM_DependencyCopyItem> Items;

	/** Packages that were skipped (e.g., Engine assets with Skip policy) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency Copy")
	TArray<FName> SkippedPackages;

	/** Packages that failed to copy */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency Copy")
	TArray<FName> FailedPackages;

	/** Map from old package name to new package name for remapped packages */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency Copy")
	TMap<FName, FName> Remap;

	/** Error message if the operation failed at a high level */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency Copy")
	FString ErrorMessage;

	/** Number of packages successfully copied */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency Copy")
	int32 CopiedCount = 0;

	/** Number of packages skipped */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency Copy")
	int32 SkippedCount = 0;

	/** Number of packages that failed */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency Copy")
	int32 FailedCount = 0;

	/** Total number of Engine dependencies found */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency Copy")
	int32 EngineDependencyCount = 0;

	/** Total number of Game/Project dependencies found */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency Copy")
	int32 GameDependencyCount = 0;
};

/**
 * Custom AdvancedCopyCustomization that allows copying Engine assets.
 * Standard UAdvancedCopyCustomization excludes Engine content by default.
 */
UCLASS()
class CONVAIPAKMANAGEREDITOR_API UCPM_DependencyCopyCustomization : public UAdvancedCopyCustomization
{
	GENERATED_BODY()

public:
	UCPM_DependencyCopyCustomization(const FObjectInitializer &ObjectInitializer);

	/** Configure this customization for a specific copy operation */
	void Configure(const FString &InDestinationRoot, const FCPM_DependencyCopyOptions &InOptions);

	/** Override to allow Engine assets (we handle them specially) */
	virtual FARFilter GetARFilter() const override;

	/** Transform destination paths to map to our target location */
	virtual void TransformDestinationPaths(TMap<FString, FString> &OutPackagesAndDestinations) const override;

	/** Get the destination root configured for this operation */
	const FString &GetDestinationRoot() const { return DestinationRoot; }

	/** Get the options configured for this operation */
	const FCPM_DependencyCopyOptions &GetOptions() const { return Options; }

protected:
	/** The destination root path (e.g., "/MyPlugin/") */
	FString DestinationRoot;

	/** Options for this copy operation */
	FCPM_DependencyCopyOptions Options;
};

/**
 * API for copying packages with their dependencies into a target module.
 * Handles both Game and Engine module dependencies correctly.
 */
class CONVAIPAKMANAGEREDITOR_API FCPM_DependencyCopyAPI
{
public:
	/**
	 * Copies/moves SourcePackage and all discovered dependencies into DestinationRoot.
	 *
	 * @param SourcePackage     The package to copy (e.g., /Game/MyAsset)
	 * @param DestinationRoot   Target mount point (e.g., "/MyPlugin/" or "/MyPlugin/Content/")
	 * @param Options           Options controlling the copy behavior
	 * @return                  Report detailing what was copied, skipped, or failed
	 */
	static FCPM_DependencyCopyReport CopyPackageWithDependencies(
		const FName &SourcePackage,
		const FString &DestinationRoot,
		const FCPM_DependencyCopyOptions &Options);

	/**
	 * Copies/moves multiple packages and all their dependencies into DestinationRoot.
	 *
	 * @param SourcePackages    Array of packages to copy
	 * @param DestinationRoot   Target mount point
	 * @param Options           Options controlling the copy behavior
	 * @return                  Report detailing what was copied, skipped, or failed
	 */
	static FCPM_DependencyCopyReport CopyPackagesWithDependencies(
		const TArray<FName> &SourcePackages,
		const FString &DestinationRoot,
		const FCPM_DependencyCopyOptions &Options);

	/**
	 * Checks if a package is an Engine or non-project plugin asset.
	 *
	 * @param PackageName   The package to check
	 * @return              True if this is an Engine asset or from a non-project plugin
	 */
	static bool IsEnginePackage(const FName &PackageName);

	/**
	 * Checks if a package path is under the destination root.
	 *
	 * @param PackageName       The package to check
	 * @param DestinationRoot   The destination root path
	 * @return                  True if the package is already under the destination
	 */
	static bool IsPackageUnderDestination(const FName &PackageName, const FString &DestinationRoot);

	/**
	 * Checks if a package should be excluded based on the options.
	 * Checks against ExcludedPaths, ExcludedModules, ExcludedPackages, and ExcludedPlugins.
	 *
	 * @param PackageName   The package to check
	 * @param Options       The copy options containing exclusion lists
	 * @return              True if the package should be excluded
	 */
	static bool ShouldExcludePackage(const FName& PackageName, const FCPM_DependencyCopyOptions& Options);

	/**
	 * Generates the destination package path for a source package.
	 *
	 * @param SourcePackage     The source package name
	 * @param DestinationRoot   The destination root path
	 * @param DestinationSubdir Optional subdirectory under destination
	 * @return                  The computed destination package name
	 */
	static FName MakeDestinationPackage(
		const FName &SourcePackage,
		const FString &DestinationRoot,
		const FString &DestinationSubdir = FString());

private:
	/**
	 * Gathers all dependencies for the given packages.
	 */
	static void GatherDependencies(
		const TArray<FName> &RootPackages,
		const FCPM_DependencyCopyOptions &Options,
		TSet<FName> &OutAllPackages,
		TSet<FName> &OutEnginePackages,
		TSet<FName> &OutGamePackages);

	/**
	 * Recursively gathers dependencies for a single package.
	 */
	static void RecursiveGatherDependencies(
		const FName &PackageName,
		const FCPM_DependencyCopyOptions &Options,
		TSet<FName> &VisitedPackages,
		TSet<FName> &OutEnginePackages,
		TSet<FName> &OutGamePackages);

	/**
	 * Builds the copy plan mapping source packages to destination packages.
	 */
	static bool BuildCopyPlan(
		const TSet<FName> &AllPackages,
		const TSet<FName> &EnginePackages,
		const FString &DestinationRoot,
		const FCPM_DependencyCopyOptions &Options,
		TMap<FName, FName> &OutSourceToDest,
		FCPM_DependencyCopyReport &InOutReport);

	/**
	 * Executes the copy using Advanced Copy (handles reference remapping).
	 */
	static bool ExecuteAdvancedCopy(
		const TMap<FName, FName> &SourceToDest,
		const FCPM_DependencyCopyOptions &Options,
		FCPM_DependencyCopyReport &InOutReport);

	/**
	 * Duplicates an asset manually (used for Engine assets that AdvancedCopy rejects).
	 */
	static bool DuplicateAssetManually(
		const FName &SourcePackage,
		const FName &DestPackage,
		FString &OutError);

	/**
	 * Updates references in copied packages to point to other copied packages.
	 * @deprecated Use FixupAllHardReferences instead
	 */
	static bool FixupReferencesInCopiedPackages(
		const TMap<FName, FName>& SourceToDest,
		const TArray<FName>& CopiedPackages,
		FString& OutError);

	/**
	 * Comprehensive reference fixup that handles both hard and soft object references.
	 * Loads all source and destination packages, builds object mappings, and uses
	 * FArchiveReplaceObjectRef to replace all references in copied packages.
	 * 
	 * @param SourceToDest  Map of source package names to destination package names
	 * @param OutError      Error message if operation fails
	 * @return              True if successful
	 */
	static bool FixupAllHardReferences(
		const TMap<FName, FName>& SourceToDest,
		FString& OutError);
};
