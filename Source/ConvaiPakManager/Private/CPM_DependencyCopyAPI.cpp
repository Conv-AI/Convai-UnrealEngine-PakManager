// Copyright 2022 Convai Inc. All Rights Reserved.

#include "CPM_DependencyCopyAPI.h"
#include "ConvaiDependencyCopy.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(CPM_DependencyCopyAPI)

namespace
{
	ConvaiAvatarPreparation::FDependencyCopyOptions ToShared(const FCPM_DependencyCopyOptions& Options)
	{
		ConvaiAvatarPreparation::FDependencyCopyOptions Shared;
		Shared.Operation = static_cast<ConvaiAvatarPreparation::EDependencyCopyOp>(Options.Operation);
		Shared.EnginePolicy = static_cast<ConvaiAvatarPreparation::EEngineDependencyPolicy>(Options.EnginePolicy);
		Shared.bIncludeSoftDependencies = Options.bIncludeSoftDependencies;
		Shared.bIncludeHardDependencies = Options.bIncludeHardDependencies;
		Shared.bIncludeSearchableNameDependencies = Options.bIncludeSearchableNameDependencies;
		Shared.bCopyIfAlreadyInDestination = Options.bCopyIfAlreadyInDestination;
		Shared.DestinationSubdir = Options.DestinationSubdir;
		Shared.bFixupRedirectors = Options.bFixupRedirectors;
		Shared.bSaveAfterCopy = Options.bSaveAfterCopy;
		Shared.bSuppressUI = Options.bSuppressUI;
		Shared.bOverwriteExisting = Options.bOverwriteExisting;
		Shared.ExcludedPaths = Options.ExcludedPaths;
		Shared.ExcludedModules = Options.ExcludedModules;
		Shared.ExcludedPackages = Options.ExcludedPackages;
		Shared.AdditionalPackagesToFixup = Options.AdditionalPackagesToFixup;
		Shared.bStrictValidation = Options.bStrictValidation;
		return Shared;
	}

	FCPM_DependencyCopyReport FromShared(const ConvaiAvatarPreparation::FDependencyCopyReport& Shared)
	{
		FCPM_DependencyCopyReport Report;
		Report.bSuccess = Shared.bSuccess;
		Report.bReferencesFixedUp = Shared.bReferencesFixedUp;
		Report.SkippedPackages = Shared.SkippedPackages;
		Report.FailedPackages = Shared.FailedPackages;
		Report.Remap = Shared.Remap;
		Report.ErrorMessage = Shared.ErrorMessage;
		Report.CopiedCount = Shared.CopiedCount;
		Report.SkippedCount = Shared.SkippedCount;
		Report.FailedCount = Shared.FailedCount;
		Report.EngineDependencyCount = Shared.EngineDependencyCount;
		Report.GameDependencyCount = Shared.GameDependencyCount;
		for (const ConvaiAvatarPreparation::FDependencyCopyItem& SharedItem : Shared.Items)
		{
			FCPM_DependencyCopyItem& Item = Report.Items.AddDefaulted_GetRef();
			Item.SourcePackage = SharedItem.SourcePackage;
			Item.DestPackage = SharedItem.DestPackage;
			Item.bIsEngineAsset = SharedItem.bIsEngineAsset;
			Item.bPlanned = SharedItem.bPlanned;
			Item.bCopiedOrMoved = SharedItem.bCopiedOrMoved;
			Item.bSkipped = SharedItem.bSkipped;
			Item.Error = SharedItem.Error;
		}
		return Report;
	}
}

FCPM_DependencyCopyReport FCPM_DependencyCopyAPI::CopyPackageWithDependencies(
	const FName& SourcePackage, const FString& DestinationRoot, const FCPM_DependencyCopyOptions& Options)
{
	return FromShared(ConvaiAvatarPreparation::FDependencyCopy::CopyPackageWithDependencies(
		SourcePackage, DestinationRoot, ToShared(Options)));
}

FCPM_DependencyCopyReport FCPM_DependencyCopyAPI::CopyPackagesWithDependencies(
	const TArray<FName>& SourcePackages, const FString& DestinationRoot, const FCPM_DependencyCopyOptions& Options)
{
	return FromShared(ConvaiAvatarPreparation::FDependencyCopy::CopyPackagesWithDependencies(
		SourcePackages, DestinationRoot, ToShared(Options)));
}

bool FCPM_DependencyCopyAPI::IsEnginePackage(const FName& PackageName)
{
	return ConvaiAvatarPreparation::FDependencyCopy::IsEnginePackage(PackageName);
}

bool FCPM_DependencyCopyAPI::IsPackageUnderDestination(const FName& PackageName, const FString& DestinationRoot)
{
	return ConvaiAvatarPreparation::FDependencyCopy::IsPackageUnderDestination(PackageName, DestinationRoot);
}

bool FCPM_DependencyCopyAPI::ShouldExcludePackage(const FName& PackageName, const FCPM_DependencyCopyOptions& Options)
{
	return ConvaiAvatarPreparation::FDependencyCopy::ShouldExcludePackage(PackageName, ToShared(Options));
}

FName FCPM_DependencyCopyAPI::MakeDestinationPackage(
	const FName& SourcePackage, const FString& DestinationRoot, const FString& DestinationSubdir)
{
	return ConvaiAvatarPreparation::FDependencyCopy::MakeDestinationPackage(SourcePackage, DestinationRoot, DestinationSubdir);
}
