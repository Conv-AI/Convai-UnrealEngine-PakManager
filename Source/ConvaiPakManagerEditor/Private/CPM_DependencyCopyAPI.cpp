// Copyright 2022 Convai Inc. All Rights Reserved.

#include "CPM_DependencyCopyAPI.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Interfaces/IPluginManager.h"
#include "ObjectTools.h"
#include "FileHelpers.h"
#include "Editor.h"
#include "Misc/MessageDialog.h"
#include "Logging/MessageLog.h"
#include "Serialization/ArchiveReplaceObjectRef.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CPM_DependencyCopyAPI)

#define LOCTEXT_NAMESPACE "CPM_DependencyCopyAPI"

//////////////////////////////////////////////////////////////////////////
// UCPM_DependencyCopyCustomization
//////////////////////////////////////////////////////////////////////////

UCPM_DependencyCopyCustomization::UCPM_DependencyCopyCustomization(const FObjectInitializer &ObjectInitializer)
	: Super(ObjectInitializer)
{
	// By default, we want to allow more assets than the base class
	// Clear the default exclusions - we'll handle Engine assets ourselves
	FilterForExcludingDependencies.PackagePaths.Reset();
	FilterForExcludingDependencies.bRecursivePaths = true;
	FilterForExcludingDependencies.bRecursiveClasses = true;

	// Still exclude World/Level/MapBuildData as those are special
	FilterForExcludingDependencies.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
	FilterForExcludingDependencies.ClassPaths.Add(ULevel::StaticClass()->GetClassPathName());
}

void UCPM_DependencyCopyCustomization::Configure(const FString &InDestinationRoot, const FCPM_DependencyCopyOptions &InOptions)
{
	DestinationRoot = InDestinationRoot;
	Options = InOptions;

	// Ensure destination root ends with /
	if (!DestinationRoot.EndsWith(TEXT("/")))
	{
		DestinationRoot += TEXT("/");
	}

	// Add user-specified excluded paths to the filter
	for (const FString &ExcludedPath : Options.ExcludedPaths)
	{
		if (!ExcludedPath.IsEmpty())
		{
			FilterForExcludingDependencies.PackagePaths.Add(FName(*ExcludedPath));
		}
	}
}

FARFilter UCPM_DependencyCopyCustomization::GetARFilter() const
{
	FARFilter Filter = FilterForExcludingDependencies;

	// If we're skipping Engine assets, add Engine paths to exclusion
	if (Options.EnginePolicy == ECPM_EngineDependencyPolicy::Skip)
	{
		Filter.PackagePaths.Add(TEXT("/Engine"));

		// Add non-project plugin paths
		for (TSharedRef<IPlugin> &Plugin : IPluginManager::Get().GetDiscoveredPlugins())
		{
			if (Plugin->GetType() != EPluginType::Project)
			{
				Filter.PackagePaths.Add(FName(*("/" + Plugin->GetName())));
			}
		}
	}

	return Filter;
}

void UCPM_DependencyCopyCustomization::TransformDestinationPaths(TMap<FString, FString> &OutPackagesAndDestinations) const
{
	// Transform all destination paths to be under our destination root
	for (auto &Pair : OutPackagesAndDestinations)
	{
		const FString &SourcePackage = Pair.Key;
		FString &DestPackage = Pair.Value;

		// Generate the new destination
		FName NewDest = FCPM_DependencyCopyAPI::MakeDestinationPackage(
			FName(*SourcePackage),
			DestinationRoot,
			Options.DestinationSubdir);

		DestPackage = NewDest.ToString();
	}
}

//////////////////////////////////////////////////////////////////////////
// FCPM_DependencyCopyAPI - Public Methods
//////////////////////////////////////////////////////////////////////////

FCPM_DependencyCopyReport FCPM_DependencyCopyAPI::CopyPackageWithDependencies(
	const FName &SourcePackage,
	const FString &DestinationRoot,
	const FCPM_DependencyCopyOptions &Options)
{
	TArray<FName> Packages;
	Packages.Add(SourcePackage);
	return CopyPackagesWithDependencies(Packages, DestinationRoot, Options);
}

FCPM_DependencyCopyReport FCPM_DependencyCopyAPI::CopyPackagesWithDependencies(
	const TArray<FName> &SourcePackages,
	const FString &DestinationRoot,
	const FCPM_DependencyCopyOptions &Options)
{
	FCPM_DependencyCopyReport Report;

	// Validate inputs
	if (SourcePackages.Num() == 0)
	{
		Report.ErrorMessage = TEXT("No source packages specified");
		return Report;
	}

	if (DestinationRoot.IsEmpty())
	{
		Report.ErrorMessage = TEXT("Destination root is empty");
		return Report;
	}

	// Normalize destination root
	FString NormalizedDestRoot = DestinationRoot;
	if (!NormalizedDestRoot.StartsWith(TEXT("/")))
	{
		NormalizedDestRoot = TEXT("/") + NormalizedDestRoot;
	}
	if (!NormalizedDestRoot.EndsWith(TEXT("/")))
	{
		NormalizedDestRoot += TEXT("/");
	}

	// Phase 1: Gather all dependencies
	FScopedSlowTask SlowTask(4.0f, LOCTEXT("CopyingDependencies", "Copying Package Dependencies..."));
	SlowTask.MakeDialog();

	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("GatheringDependencies", "Gathering dependencies..."));

	TSet<FName> AllPackages;
	TSet<FName> EnginePackages;
	TSet<FName> GamePackages;

	GatherDependencies(SourcePackages, Options, AllPackages, EnginePackages, GamePackages);

	Report.EngineDependencyCount = EnginePackages.Num();
	Report.GameDependencyCount = GamePackages.Num();

	UE_LOG(LogTemp, Log, TEXT("CPM_DependencyCopyAPI: Found %d total dependencies (%d Engine, %d Game)"),
		   AllPackages.Num(), EnginePackages.Num(), GamePackages.Num());

	// Phase 2: Build copy plan
	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("BuildingCopyPlan", "Building copy plan..."));

	TMap<FName, FName> SourceToDest;
	if (!BuildCopyPlan(AllPackages, EnginePackages, NormalizedDestRoot, Options, SourceToDest, Report))
	{
		// BuildCopyPlan sets the error message
		return Report;
	}

	// Phase 3: Execute the copy
	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("ExecutingCopy", "Executing copy..."));

	if (!ExecuteAdvancedCopy(SourceToDest, Options, Report))
	{
		// ExecuteAdvancedCopy sets the error message
		return Report;
	}

	// Phase 4: Finalize
	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("FinalizingCopy", "Finalizing..."));

	// Update final counts
	Report.CopiedCount = 0;
	Report.SkippedCount = Report.SkippedPackages.Num();
	Report.FailedCount = Report.FailedPackages.Num();

	for (const FCPM_DependencyCopyItem &Item : Report.Items)
	{
		if (Item.bCopiedOrMoved)
		{
			Report.CopiedCount++;
		}
	}

	Report.bSuccess = (Report.FailedCount == 0);

	UE_LOG(LogTemp, Log, TEXT("CPM_DependencyCopyAPI: Copy complete. Copied: %d, Skipped: %d, Failed: %d"),
		   Report.CopiedCount, Report.SkippedCount, Report.FailedCount);

	return Report;
}

bool FCPM_DependencyCopyAPI::IsEnginePackage(const FName &PackageName)
{
	const FString PackageStr = PackageName.ToString();

	// Check for /Engine/ path
	if (PackageStr.StartsWith(TEXT("/Engine/")))
	{
		return true;
	}

	// Check for /Script/ path (code/module references)
	if (PackageStr.StartsWith(TEXT("/Script/")))
	{
		return true;
	}

	// Check for non-project plugins
	for (TSharedRef<IPlugin> &Plugin : IPluginManager::Get().GetDiscoveredPlugins())
	{
		if (Plugin->GetType() != EPluginType::Project)
		{
			const FString PluginRoot = TEXT("/") + Plugin->GetName() + TEXT("/");
			if (PackageStr.StartsWith(PluginRoot))
			{
				return true;
			}
		}
	}

	return false;
}

bool FCPM_DependencyCopyAPI::IsPackageUnderDestination(const FName &PackageName, const FString &DestinationRoot)
{
	const FString PackageStr = PackageName.ToString();
	return PackageStr.StartsWith(DestinationRoot);
}

FName FCPM_DependencyCopyAPI::MakeDestinationPackage(
	const FName &SourcePackage,
	const FString &DestinationRoot,
	const FString &DestinationSubdir)
{
	FString SourceStr = SourcePackage.ToString();

	// Extract the relative path from the source
	// e.g., /Game/Characters/Hero -> Characters/Hero
	// e.g., /Engine/Content/Textures/Default -> Engine_Content/Textures/Default

	FString RelativePath;

	// Handle different source mount points
	if (SourceStr.StartsWith(TEXT("/Game/")))
	{
		RelativePath = SourceStr.RightChop(6); // Remove "/Game/"
	}
	else if (SourceStr.StartsWith(TEXT("/Engine/")))
	{
		// Prefix Engine content to avoid conflicts
		RelativePath = TEXT("EngineContent/") + SourceStr.RightChop(8); // Remove "/Engine/"
	}
	else
	{
		// For plugin content like /PluginName/Folder/Asset
		// Extract: PluginName/Folder/Asset
		int32 SecondSlash = SourceStr.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 1);
		if (SecondSlash != INDEX_NONE)
		{
			// Get plugin name and remaining path
			FString PluginName = SourceStr.Mid(1, SecondSlash - 1);
			FString RemainingPath = SourceStr.RightChop(SecondSlash + 1);
			RelativePath = PluginName + TEXT("/") + RemainingPath;
		}
		else
		{
			// Fallback: just use the package name
			RelativePath = FPackageName::GetShortName(SourcePackage);
		}
	}

	// Build the destination path
	FString DestPath = DestinationRoot;
	if (!DestPath.EndsWith(TEXT("/")))
	{
		DestPath += TEXT("/");
	}

	if (!DestinationSubdir.IsEmpty())
	{
		DestPath += DestinationSubdir;
		if (!DestPath.EndsWith(TEXT("/")))
		{
			DestPath += TEXT("/");
		}
	}

	DestPath += RelativePath;

	return FName(*DestPath);
}

//////////////////////////////////////////////////////////////////////////
// FCPM_DependencyCopyAPI - Private Methods
//////////////////////////////////////////////////////////////////////////

void FCPM_DependencyCopyAPI::GatherDependencies(
	const TArray<FName> &RootPackages,
	const FCPM_DependencyCopyOptions &Options,
	TSet<FName> &OutAllPackages,
	TSet<FName> &OutEnginePackages,
	TSet<FName> &OutGamePackages)
{
	TSet<FName> VisitedPackages;

	for (const FName &RootPackage : RootPackages)
	{
		// Add the root package itself
		if (IsEnginePackage(RootPackage))
		{
			OutEnginePackages.Add(RootPackage);
		}
		else
		{
			OutGamePackages.Add(RootPackage);
		}
		OutAllPackages.Add(RootPackage);
		VisitedPackages.Add(RootPackage);

		// Recursively gather dependencies
		RecursiveGatherDependencies(RootPackage, Options, VisitedPackages, OutEnginePackages, OutGamePackages);
	}

	// Copy all visited packages to output
	OutAllPackages = VisitedPackages;
}

void FCPM_DependencyCopyAPI::RecursiveGatherDependencies(
	const FName &PackageName,
	const FCPM_DependencyCopyOptions &Options,
	TSet<FName> &VisitedPackages,
	TSet<FName> &OutEnginePackages,
	TSet<FName> &OutGamePackages)
{
	FAssetRegistryModule &AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

	TArray<FName> Dependencies;

	// Build dependency query based on options
	UE::AssetRegistry::FDependencyQuery DependencyQuery;
	UE::AssetRegistry::EDependencyCategory Category = UE::AssetRegistry::EDependencyCategory::None;

	if (Options.bIncludeHardDependencies)
	{
		Category |= UE::AssetRegistry::EDependencyCategory::Package;
	}

	if (Options.bIncludeSoftDependencies)
	{
		DependencyQuery.Required = UE::AssetRegistry::EDependencyProperty::None;
	}
	else
	{
		// Exclude soft references if not wanted
		DependencyQuery.Required = UE::AssetRegistry::EDependencyProperty::Hard;
	}

	if (Options.bIncludeSearchableNameDependencies)
	{
		Category |= UE::AssetRegistry::EDependencyCategory::SearchableName;
	}

	// Get dependencies
	AssetRegistry.GetDependencies(PackageName, Dependencies, Category, DependencyQuery);

	for (const FName &Dependency : Dependencies)
	{
		const FString DependencyStr = Dependency.ToString();

		// Skip script packages
		if (DependencyStr.StartsWith(TEXT("/Script/")))
		{
			continue;
		}

		// Skip already visited
		if (VisitedPackages.Contains(Dependency))
		{
			continue;
		}

		// Check if package exists
		const bool bPackageExists = AssetRegistry.GetAssetPackageDataCopy(Dependency).IsSet();
		if (!bPackageExists)
		{
			continue;
		}

		// Check excluded paths
		bool bIsExcluded = false;
		for (const FString &ExcludedPath : Options.ExcludedPaths)
		{
			if (!ExcludedPath.IsEmpty() && DependencyStr.StartsWith(ExcludedPath))
			{
				bIsExcluded = true;
				break;
			}
		}
		if (bIsExcluded)
		{
			continue;
		}

		// Mark as visited
		VisitedPackages.Add(Dependency);

		// Classify as Engine or Game
		if (IsEnginePackage(Dependency))
		{
			OutEnginePackages.Add(Dependency);
		}
		else
		{
			OutGamePackages.Add(Dependency);
		}

		// Recurse
		RecursiveGatherDependencies(Dependency, Options, VisitedPackages, OutEnginePackages, OutGamePackages);
	}
}

bool FCPM_DependencyCopyAPI::BuildCopyPlan(
	const TSet<FName> &AllPackages,
	const TSet<FName> &EnginePackages,
	const FString &DestinationRoot,
	const FCPM_DependencyCopyOptions &Options,
	TMap<FName, FName> &OutSourceToDest,
	FCPM_DependencyCopyReport &InOutReport)
{
	for (const FName &Package : AllPackages)
	{
		FCPM_DependencyCopyItem Item;
		Item.SourcePackage = Package;
		Item.bIsEngineAsset = EnginePackages.Contains(Package);

		// Check if already at destination
		if (IsPackageUnderDestination(Package, DestinationRoot))
		{
			if (!Options.bCopyIfAlreadyInDestination)
			{
				Item.bSkipped = true;
				Item.DestPackage = Package;
				InOutReport.Items.Add(Item);
				InOutReport.SkippedPackages.Add(Package);
				continue;
			}
		}

		// Handle Engine packages based on policy
		if (Item.bIsEngineAsset)
		{
			switch (Options.EnginePolicy)
			{
			case ECPM_EngineDependencyPolicy::CopyIntoDestination:
				// Will be copied
				break;

			case ECPM_EngineDependencyPolicy::Skip:
				Item.bSkipped = true;
				Item.DestPackage = Package; // Keep original
				InOutReport.Items.Add(Item);
				InOutReport.SkippedPackages.Add(Package);
				continue;

			case ECPM_EngineDependencyPolicy::Fail:
				InOutReport.ErrorMessage = FString::Printf(
					TEXT("Engine dependency detected and policy is set to Fail: %s"),
					*Package.ToString());
				InOutReport.FailedPackages.Add(Package);
				return false;
			}
		}

		// Generate destination path
		Item.DestPackage = MakeDestinationPackage(Package, DestinationRoot, Options.DestinationSubdir);
		Item.bPlanned = true;

		OutSourceToDest.Add(Package, Item.DestPackage);
		InOutReport.Remap.Add(Package, Item.DestPackage);
		InOutReport.Items.Add(Item);
	}

	return true;
}

bool FCPM_DependencyCopyAPI::ExecuteAdvancedCopy(
	const TMap<FName, FName> &SourceToDest,
	const FCPM_DependencyCopyOptions &Options,
	FCPM_DependencyCopyReport &InOutReport)
{
	if (SourceToDest.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("CPM_DependencyCopyAPI: No packages to copy"));
		return true;
	}

	// Separate Engine and Game packages
	TMap<FString, FString> GamePackagesToCopy;
	TMap<FName, FName> EnginePackagesToCopy;

	for (const auto &Pair : SourceToDest)
	{
		if (IsEnginePackage(Pair.Key))
		{
			EnginePackagesToCopy.Add(Pair.Key, Pair.Value);
		}
		else
		{
			GamePackagesToCopy.Add(Pair.Key.ToString(), Pair.Value.ToString());
		}
	}

	bool bSuccess = true;

	// Copy Game packages using AdvancedCopy
	if (GamePackagesToCopy.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("CPM_DependencyCopyAPI: Copying %d game packages using AdvancedCopy"), GamePackagesToCopy.Num());

		IAssetTools &AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

		FDuplicatedObjects DuplicatedObjects;
		const bool bForceAutosave = Options.bSaveAfterCopy;
		const bool bCopyOverAllDestinationOverlaps = Options.bOverwriteExisting;

		bool bCopyResult = AssetTools.AdvancedCopyPackages(
			GamePackagesToCopy,
			bForceAutosave,
			bCopyOverAllDestinationOverlaps,
			&DuplicatedObjects,
			Options.bSuppressUI ? EMessageSeverity::Error : EMessageSeverity::Info);

		if (!bCopyResult)
		{
			UE_LOG(LogTemp, Warning, TEXT("CPM_DependencyCopyAPI: AdvancedCopyPackages returned false"));
			// Note: AdvancedCopy can return false but still have copied some packages
		}

		// Mark copied packages in the report
		for (const auto &Pair : GamePackagesToCopy)
		{
			FName SourceName(*Pair.Key);
			for (FCPM_DependencyCopyItem &Item : InOutReport.Items)
			{
				if (Item.SourcePackage == SourceName)
				{
					// Check if package exists at destination
					FString DestFilename;
					if (FPackageName::DoesPackageExist(Pair.Value, &DestFilename))
					{
						Item.bCopiedOrMoved = true;
					}
					else
					{
						Item.bCopiedOrMoved = false;
						Item.Error = TEXT("Package not found at destination after copy");
						InOutReport.FailedPackages.AddUnique(SourceName);
						bSuccess = false;
					}
					break;
				}
			}
		}
	}

	// Copy Engine packages manually (AdvancedCopy rejects Engine content)
	if (EnginePackagesToCopy.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("CPM_DependencyCopyAPI: Copying %d engine packages manually"), EnginePackagesToCopy.Num());

		FScopedSlowTask EngineSlowTask(static_cast<float>(EnginePackagesToCopy.Num()),
									   LOCTEXT("CopyingEnginePackages", "Copying Engine packages..."));
		EngineSlowTask.MakeDialog();

		TArray<FName> CopiedEnginePackages;

		for (const auto &Pair : EnginePackagesToCopy)
		{
			EngineSlowTask.EnterProgressFrame(1.0f, FText::Format(
														LOCTEXT("CopyingEnginePackage", "Copying {0}..."),
														FText::FromName(Pair.Key)));

			FString Error;
			bool bDuplicateSuccess = DuplicateAssetManually(Pair.Key, Pair.Value, Error);

			// Update report
			for (FCPM_DependencyCopyItem &Item : InOutReport.Items)
			{
				if (Item.SourcePackage == Pair.Key)
				{
					Item.bCopiedOrMoved = bDuplicateSuccess;
					if (!bDuplicateSuccess)
					{
						Item.Error = Error;
						InOutReport.FailedPackages.AddUnique(Pair.Key);
						bSuccess = false;
					}
					else
					{
						CopiedEnginePackages.Add(Pair.Value);
					}
					break;
				}
			}
		}

	}

	// Final comprehensive reference fixup pass for ALL copied packages
	// This is necessary because:
	// 1. AdvancedCopy only remaps game->game references, not game->engine
	// 2. Engine assets were duplicated with their original references intact
	UE_LOG(LogTemp, Log, TEXT("CPM_DependencyCopyAPI: Performing comprehensive reference fixup for all copied packages..."));
	
	FString FixupError;
	if (!FixupAllHardReferences(SourceToDest, FixupError))
	{
		UE_LOG(LogTemp, Warning, TEXT("CPM_DependencyCopyAPI: Reference fixup warning: %s"), *FixupError);
		// Don't fail the whole operation for fixup issues, but log it
	}

	return bSuccess;
}

bool FCPM_DependencyCopyAPI::DuplicateAssetManually(
	const FName &SourcePackage,
	const FName &DestPackage,
	FString &OutError)
{
	FAssetRegistryModule &AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

	// Get assets in the source package
	TArray<FAssetData> SourceAssets;
	AssetRegistry.GetAssetsByPackageName(SourcePackage, SourceAssets);

	if (SourceAssets.Num() == 0)
	{
		OutError = FString::Printf(TEXT("No assets found in package %s"), *SourcePackage.ToString());
		return false;
	}

	const FString DestPackageStr = DestPackage.ToString();
	const FString DestPackagePath = FPackageName::GetLongPackagePath(DestPackageStr);
	const FString DestAssetName = FPackageName::GetShortName(DestPackageStr);

	bool bAnySuccess = false;

	for (const FAssetData &SourceAssetData : SourceAssets)
	{
		// Load the source asset
		UObject *SourceObject = SourceAssetData.GetAsset();
		if (!SourceObject)
		{
			UE_LOG(LogTemp, Warning, TEXT("CPM_DependencyCopyAPI: Failed to load source asset %s"), *SourceAssetData.GetObjectPathString());
			continue;
		}

		// Create the destination package
		UPackage *DestinationPackage = CreatePackage(*DestPackageStr);
		if (!DestinationPackage)
		{
			OutError = FString::Printf(TEXT("Failed to create destination package %s"), *DestPackageStr);
			continue;
		}

		// Duplicate the object
		UObject *DuplicatedObject = StaticDuplicateObject(
			SourceObject,
			DestinationPackage,
			*DestAssetName,
			RF_Public | RF_Standalone);

		if (!DuplicatedObject)
		{
			OutError = FString::Printf(TEXT("Failed to duplicate object %s"), *SourceAssetData.GetObjectPathString());
			continue;
		}

		// Mark package as dirty
		DestinationPackage->MarkPackageDirty();

		// Notify asset registry
		FAssetRegistryModule::AssetCreated(DuplicatedObject);

		// Save the package
		const FString PackageFilename = FPackageName::LongPackageNameToFilename(DestPackageStr, FPackageName::GetAssetPackageExtension());

		// Ensure directory exists
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFilename), true);

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GError;
		SaveArgs.bForceByteSwapping = false;
		SaveArgs.bWarnOfLongFilename = false;
		SaveArgs.SaveFlags = SAVE_NoError;

		FSavePackageResultStruct SaveResult = UPackage::Save(DestinationPackage, DuplicatedObject, *PackageFilename, SaveArgs);

		if (SaveResult.Result == ESavePackageResult::Success)
		{
			bAnySuccess = true;
			UE_LOG(LogTemp, Log, TEXT("CPM_DependencyCopyAPI: Duplicated %s -> %s"),
				   *SourcePackage.ToString(), *DestPackage.ToString());
		}
		else
		{
			OutError = FString::Printf(TEXT("Failed to save package %s"), *DestPackageStr);
		}
	}

	return bAnySuccess;
}

bool FCPM_DependencyCopyAPI::FixupReferencesInCopiedPackages(
	const TMap<FName, FName>& SourceToDest,
	const TArray<FName>& CopiedPackages,
	FString& OutError)
{
	// This function is now deprecated in favor of FixupAllHardReferences
	// Keeping for backwards compatibility
	return true;
}

bool FCPM_DependencyCopyAPI::FixupAllHardReferences(
	const TMap<FName, FName>& SourceToDest,
	FString& OutError)
{
	if (SourceToDest.Num() == 0)
	{
		return true;
	}

	FScopedSlowTask SlowTask(3.0f, LOCTEXT("FixingReferences", "Fixing asset references..."));
	SlowTask.MakeDialog();

	// Step 1: Build mapping of old UObject* to new UObject*
	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("LoadingAssets", "Loading assets for reference fixup..."));

	TMap<UObject*, UObject*> OldToNewObjects;
	TArray<UPackage*> DestinationPackages;
	TMap<FSoftObjectPath, FSoftObjectPath> SoftPathRemap;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	for (const auto& Pair : SourceToDest)
	{
		const FName& SourcePackageName = Pair.Key;
		const FName& DestPackageName = Pair.Value;

		// Load source package to get its objects
		UPackage* SourcePackage = LoadPackage(nullptr, *SourcePackageName.ToString(), LOAD_None);
		if (!SourcePackage)
		{
			UE_LOG(LogTemp, Warning, TEXT("CPM_DependencyCopyAPI: Could not load source package %s for reference mapping"),
				*SourcePackageName.ToString());
			continue;
		}

		// Load destination package
		UPackage* DestPackage = LoadPackage(nullptr, *DestPackageName.ToString(), LOAD_None);
		if (!DestPackage)
		{
			UE_LOG(LogTemp, Warning, TEXT("CPM_DependencyCopyAPI: Could not load dest package %s for reference mapping"),
				*DestPackageName.ToString());
			continue;
		}

		DestinationPackages.AddUnique(DestPackage);

		// Get all top-level objects from both packages
		TArray<UObject*> SourceObjects;
		TArray<UObject*> DestObjects;
		
		// Find the main asset in each package
		ForEachObjectWithPackage(SourcePackage, [&SourceObjects](UObject* Object)
		{
			if (Object && Object->IsAsset())
			{
				SourceObjects.Add(Object);
			}
			return true;
		}, false);

		ForEachObjectWithPackage(DestPackage, [&DestObjects](UObject* Object)
		{
			if (Object && Object->IsAsset())
			{
				DestObjects.Add(Object);
			}
			return true;
		}, false);

		UE_LOG(LogTemp, Log, TEXT("CPM_DependencyCopyAPI: Package %s -> %s has %d source objects, %d dest objects"),
			*SourcePackageName.ToString(), *DestPackageName.ToString(), SourceObjects.Num(), DestObjects.Num());

		// Match source to dest assets by class and name
		for (UObject* SourceObject : SourceObjects)
		{
			if (!SourceObject)
			{
				continue;
			}

			const FString SourceName = SourceObject->GetName();
			const UClass* SourceClass = SourceObject->GetClass();
			
			for (UObject* DestObject : DestObjects)
			{
				if (DestObject && DestObject->GetClass() == SourceClass && DestObject->GetName() == SourceName)
				{
					// Add to object mapping
					OldToNewObjects.Add(SourceObject, DestObject);
					
					// Add to soft path mapping
					SoftPathRemap.Add(
						FSoftObjectPath(SourceObject),
						FSoftObjectPath(DestObject)
					);
					
					UE_LOG(LogTemp, Log, TEXT("CPM_DependencyCopyAPI: Mapped object %s -> %s"),
						*SourceObject->GetPathName(), *DestObject->GetPathName());
					break;
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("CPM_DependencyCopyAPI: Built reference map with %d object mappings across %d packages"),
		OldToNewObjects.Num(), DestinationPackages.Num());

	if (OldToNewObjects.Num() == 0)
	{
		OutError = TEXT("No object mappings could be built");
		return false;
	}

	// Step 2: Replace hard object references in all destination packages
	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("ReplacingReferences", "Replacing object references..."));

	int32 TotalReplacedCount = 0;

	for (UPackage* Package : DestinationPackages)
	{
		// Get all objects in this package
		TArray<UObject*> ObjectsInPackage;
		GetObjectsWithOuter(Package, ObjectsInPackage, true);

		for (UObject* Object : ObjectsInPackage)
		{
			// Skip null or invalid objects - NOTE: we want to process VALID objects
			if (!Object || !IsValid(Object))
			{
				continue;
			}

			// Replace hard object references
			FArchiveReplaceObjectRef<UObject> ReplaceAr(
				Object,
				OldToNewObjects,
				EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef
			);

			int32 ReplacedInThisObject = ReplaceAr.GetCount();
			if (ReplacedInThisObject > 0)
			{
				UE_LOG(LogTemp, Log, TEXT("CPM_DependencyCopyAPI: Replaced %d references in %s"),
					ReplacedInThisObject, *Object->GetPathName());
			}
			TotalReplacedCount += ReplacedInThisObject;
		}

		// Mark package as dirty so it gets saved
		Package->MarkPackageDirty();
	}

	UE_LOG(LogTemp, Log, TEXT("CPM_DependencyCopyAPI: Replaced %d object references"), TotalReplacedCount);

	// Step 3: Also fix soft object paths
	if (DestinationPackages.Num() > 0 && SoftPathRemap.Num() > 0)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTools.RenameReferencingSoftObjectPaths(DestinationPackages, SoftPathRemap);
	}

	// Step 4: Save all modified packages
	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("SavingPackages", "Saving modified packages..."));

	TArray<UPackage*> PackagesToSave;
	for (UPackage* Package : DestinationPackages)
	{
		if (Package->IsDirty())
		{
			PackagesToSave.Add(Package);
		}
	}

	if (PackagesToSave.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("CPM_DependencyCopyAPI: Saving %d modified packages..."), PackagesToSave.Num());

		for (UPackage* Package : PackagesToSave)
		{
			const FString PackageName = Package->GetName();
			const FString PackageFilename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

			// Get the main asset to save
			TArray<UObject*> ObjectsInPackage;
			GetObjectsWithOuter(Package, ObjectsInPackage, false);
			
			UObject* AssetToSave = nullptr;
			for (UObject* Obj : ObjectsInPackage)
			{
				if (Obj && Obj->IsAsset())
				{
					AssetToSave = Obj;
					break;
				}
			}

			if (!AssetToSave)
			{
				UE_LOG(LogTemp, Warning, TEXT("CPM_DependencyCopyAPI: No asset found in package %s"), *PackageName);
				continue;
			}

			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.Error = GError;
			SaveArgs.SaveFlags = SAVE_NoError;

			FSavePackageResultStruct SaveResult = UPackage::Save(Package, AssetToSave, *PackageFilename, SaveArgs);

			if (SaveResult.Result == ESavePackageResult::Success)
			{
				UE_LOG(LogTemp, Log, TEXT("CPM_DependencyCopyAPI: Saved package with updated references: %s"), *PackageName);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("CPM_DependencyCopyAPI: Failed to save package: %s"), *PackageName);
			}
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
