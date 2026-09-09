// Copyright Convai. All Rights Reserved.

#include "Publish/CPM_AvatarCookInputs.h"

#include "AssetRegistry/AssetBundleData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintSupport.h"
#include "CoreGlobals.h"
#include "Engine/AssetManager.h"
#include "Engine/Blueprint.h"
#include "Engine/PrimaryAssetLabel.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Windows/WindowsHWrapper.h"

namespace
{
	bool IsPluginMount(const FString& Mount)
	{
		if (Mount.Len() < 3 || !Mount.StartsWith(TEXT("/")) || !Mount.EndsWith(TEXT("/"))
			|| Mount.Equals(TEXT("/Game/"), ESearchCase::IgnoreCase) || Mount.Equals(TEXT("/Engine/"), ESearchCase::IgnoreCase)
			|| Mount.Equals(TEXT("/Script/"), ESearchCase::IgnoreCase)) { return false; }
		for (const TCHAR C : Mount.Mid(1).LeftChop(1))
		{
			if (!FChar::IsAlnum(C) && C != '_') { return false; }
		}
		return FPackageName::MountPointExists(Mount);
	}

	bool PlainLocalPath(FString Path, bool bAllowMissing)
	{
		FPaths::NormalizeDirectoryName(Path);
		if (Path.Len() < 4 || Path.Len() > 1024 || !FChar::IsAlpha(Path[0]) || Path[1] != ':' || Path[2] != '/') { return false; }
		TArray<FString> Parts;
		Path.Mid(3).ParseIntoArray(Parts, TEXT("/"), false);
		for (const FString& Part : Parts)
		{
			if (Part.IsEmpty() || Part == TEXT(".") || Part == TEXT("..") || Part.Contains(TEXT(":"))) { return false; }
		}
		FString Current = Path.Left(3);
		for (const FString& Part : Parts)
		{
			Current = FPaths::Combine(Current, Part);
			const DWORD Attributes = GetFileAttributesW(*Current);
			if (Attributes == INVALID_FILE_ATTRIBUTES)
			{
				const DWORD Code = GetLastError();
				return bAllowMissing && (Code == ERROR_FILE_NOT_FOUND || Code == ERROR_PATH_NOT_FOUND);
			}
			if ((Attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) { return false; }
		}
		return true;
	}

	bool SameLocalPath(FString A, FString B)
	{
		FPaths::NormalizeFilename(A);
		FPaths::NormalizeFilename(B);
		return A.Equals(B, ESearchCase::IgnoreCase);
	}
}

FCPM_AvatarCookInputResult FCPM_AvatarCookInputs::Prepare(const FCPM_AvatarCookInput& Input,
	TFunctionRef<bool()> IsCancelled, TFunctionRef<bool()> IsSnapshotValid)
{
	FCPM_AvatarCookInputResult Result;
	if (!IsInGameThread()) { Result.Error = TEXT("Cook input preparation requires the child game thread."); return Result; }
	const auto MayContinue = [&]
	{
		if (IsCancelled()) { Result.Error = TEXT("Cook input preparation was cancelled."); return false; }
		if (!IsSnapshotValid()) { Result.Error = TEXT("The selected source snapshot changed or is unverified."); return false; }
		return true;
	};
	if (!MayContinue()) { return Result; }
	const FString ExpectedChild = FPaths::Combine(Input.HostDirectory,
		TEXT("Saved/ConvaiAvatarStudio/Uploader/AvatarStudioUploader.uproject"));
	const FString CurrentProject = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	if (!PlainLocalPath(Input.HostDirectory, false) || !PlainLocalPath(Input.ChildProjectFile, false)
		|| !SameLocalPath(ExpectedChild, Input.ChildProjectFile) || !SameLocalPath(CurrentProject, Input.ChildProjectFile))
	{
		Result.Error = TEXT("Cook inputs can only be prepared inside this host's verified persistent uploader project."); return Result;
	}
	if (!Input.JobId.IsValid() || Input.ChunkId <= 0 || !IsPluginMount(Input.SelectedMount)
		|| Input.SelectedPackages.IsEmpty() || Input.SelectedPackages.Num() + Input.PrerequisitePackages.Num() > 50000)
	{
		Result.Error = TEXT("Cook job identity, stable chunk, selected mount or explicit package closure is invalid."); return Result;
	}
	const FString Job = Input.JobId.ToString(EGuidFormats::Digits);
	const FString LabelName = TEXT("PAL_Selected_") + Job;
	const FString LabelPackage = TEXT("/Game/ConvaiAvatarStudioJobs/Job_") + Job + TEXT("/") + LabelName;
	const FString LabelFilename = FPackageName::LongPackageNameToFilename(LabelPackage, FPackageName::GetAssetPackageExtension());
	const FString ExpectedFilename = FPaths::Combine(FPaths::GetPath(Input.ChildProjectFile), TEXT("Content/ConvaiAvatarStudioJobs"),
		TEXT("Job_") + Job, LabelName + TEXT(".uasset"));
	if (!SameLocalPath(FPaths::ConvertRelativePathToFull(LabelFilename), ExpectedFilename)
		|| !PlainLocalPath(FPaths::GetPath(ExpectedFilename), true))
	{
		Result.Error = TEXT("The job's /Game package does not map to real child-owned Content."); return Result;
	}
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	if (!UAssetManager::IsInitialized() || Registry.IsLoadingAssets() || Registry.IsGathering())
	{
		Result.Error = TEXT("Cook input preparation requires a settled registry and initialized Asset Manager."); return Result;
	}
	TSet<FName> Closure;
	TArray<FString> Packages = Input.SelectedPackages;
	Packages.Append(Input.PrerequisitePackages);
	for (int32 Index = 0; Index < Packages.Num(); ++Index)
	{
		const FString& Name = Packages[Index];
		const bool bSelected = Name.StartsWith(Input.SelectedMount, ESearchCase::CaseSensitive);
		if (!FPackageName::IsValidLongPackageName(Name, true) || Name.Len() > 1024
			|| Name.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase) || Name.StartsWith(TEXT("/Script/"), ESearchCase::IgnoreCase)
			|| bSelected != (Index < Input.SelectedPackages.Num()) || Closure.Contains(FName(*Name)))
		{
			Result.Error = TEXT("Cook packages must be unique, correctly classified and outside host /Game content."); return Result;
		}
		Closure.Add(FName(*Name));
	}
	TArray<FSoftObjectPath> Assets;
	TArray<FSoftObjectPath> Blueprints;
	for (const FString& Name : Packages)
	{
		if (!MayContinue()) { return Result; }
		UPackage* Loaded = FindPackage(nullptr, *Name);
		if ((Loaded && Loaded->IsDirty()) || !FPackageName::DoesPackageExist(Name))
		{
			Result.Error = TEXT("Every selected/prerequisite package must have a clean saved source."); return Result;
		}
		TArray<FName> Dependencies;
		if (!Registry.GetDependencies(FName(*Name), Dependencies, UE::AssetRegistry::EDependencyCategory::Package))
		{
			Result.Error = TEXT("A cook package's dependency closure is unknown."); return Result;
		}
		for (const FName Dependency : Dependencies)
		{
			if (!Dependency.ToString().StartsWith(TEXT("/Script/"), ESearchCase::CaseSensitive) && !Closure.Contains(Dependency))
			{
				Result.Error = TEXT("A source dependency is outside the explicit selected/prerequisite cook closure."); return Result;
			}
		}
		TArray<FAssetData> PackageAssets;
		if (!Registry.GetAssetsByPackageName(FName(*Name), PackageAssets, true) || PackageAssets.IsEmpty())
		{
			Result.Error = TEXT("A saved cook package has no verified on-disk asset paths."); return Result;
		}
		for (const FAssetData& Asset : PackageAssets)
		{
			if (!Asset.PackageName.ToString().Equals(Name, ESearchCase::CaseSensitive))
			{
				Result.Error = TEXT("A cook package's declared path does not match its saved registry identity."); return Result;
			}
			if (Asset.IsRedirector() || Asset.IsInstanceOf<UPrimaryAssetLabel>())
			{
				Result.Error = TEXT("Redirectors and linked labels cannot be part of the selected cook input bundle."); return Result;
			}
			FString GeneratedClass;
			if (Asset.GetTagValue(FBlueprintTags::GeneratedClassPath, GeneratedClass))
			{
				const FSoftObjectPath Path(FPackageName::ExportTextPathToObjectPath(GeneratedClass));
				if (!Path.IsValid() || !Path.GetLongPackageName().Equals(Name, ESearchCase::CaseSensitive)
					|| !Path.GetAssetName().EndsWith(TEXT("_C"), ESearchCase::CaseSensitive))
				{
					Result.Error = TEXT("A Blueprint's saved generated-class path is invalid."); return Result;
				}
				Blueprints.AddUnique(Path);
			}
			else if (Asset.IsInstanceOf<UBlueprint>())
			{
				Result.Error = TEXT("A Blueprint has no saved generated-class path."); return Result;
			}
			else { Assets.AddUnique(Asset.GetSoftObjectPath()); }
		}
	}
	if (!MayContinue()) { return Result; }
	Result = WriteLabel(LabelPackage, Assets, Blueprints, Input.ChunkId);
	if (Result.bSuccess && !MayContinue()) { Result.bSuccess = false; }
	return Result;
}

FCPM_AvatarCookInputResult FCPM_AvatarCookInputs::WriteLabel(const FString& LabelPackage,
	const TArray<FSoftObjectPath>& Assets, const TArray<FSoftObjectPath>& Blueprints, int32 ChunkId)
{
	FCPM_AvatarCookInputResult Result;
	if (!IsInGameThread() || !UAssetManager::IsInitialized() || ChunkId <= 0 || Assets.Num() + Blueprints.Num() == 0
		|| !FPackageName::IsValidLongPackageName(LabelPackage) || FindPackage(nullptr, *LabelPackage) || FPackageName::DoesPackageExist(LabelPackage))
	{
		Result.Error = TEXT("A cook label requires a fresh owned package, initialized Asset Manager and explicit inputs."); return Result;
	}
	const FPrimaryAssetId LabelId(UPrimaryAssetLabel::StaticClass()->GetFName(), FPackageName::GetShortFName(LabelPackage));
	if (!UAssetManager::Get().GetPrimaryAssetPath(LabelId).IsNull())
	{
		Result.Error = TEXT("This job label's primary identity is already registered; the job must use a fresh identity."); return Result;
	}
	TGuardValue<bool> Silent(GIsSilent, true);
	TGuardValue<bool> Unattended(GIsRunningUnattendedScript, true);
	const FString Filename = FPackageName::LongPackageNameToFilename(LabelPackage, FPackageName::GetAssetPackageExtension());
	if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true))
	{
		Result.Error = TEXT("Cannot create the child-owned job label directory."); return Result;
	}
	UPackage* Package = CreatePackage(*LabelPackage);
	auto* Label = NewObject<UPrimaryAssetLabel>(Package, *FPackageName::GetShortName(LabelPackage), RF_Public | RF_Standalone);
	Label->bLabelAssetsInMyDirectory = false;
	Label->bIsRuntimeLabel = false;
	Label->bIncludeRedirectors = false;
	Label->Rules.ChunkId = ChunkId;
	Label->Rules.Priority = 1;
	Label->Rules.bApplyRecursively = false;
	Label->Rules.CookRule = EPrimaryAssetCookRule::AlwaysCook;
	for (const FSoftObjectPath& Path : Assets) { Label->ExplicitAssets.Add(TSoftObjectPtr<UObject>(Path)); }
	for (const FSoftObjectPath& Path : Blueprints) { Label->ExplicitBlueprints.Add(TSoftClassPtr<UObject>(Path)); }
	Package->MarkPackageDirty();
	FSavePackageArgs Save;
	Save.TopLevelFlags = RF_Public | RF_Standalone;
	Save.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, Label, *Filename, Save))
	{
		Result.Error = TEXT("The child-owned cook label could not be saved; the job must not cook."); return Result;
	}
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	IAssetRegistry::FLoadPackageRegistryData Disk;
	Registry.LoadPackageRegistryData(Filename, Disk);
	TSet<FTopLevelAssetPath> Expected;
	for (const auto& Path : Assets) { Expected.Add(Path.GetAssetPath()); }
	for (const auto& Path : Blueprints) { Expected.Add(Path.GetAssetPath()); }
	TSet<FTopLevelAssetPath> Actual;
	if (Disk.Data.Num() == 1 && Disk.Data[0].TaggedAssetBundles)
	{
		for (const FAssetBundleEntry& Entry : Disk.Data[0].TaggedAssetBundles->Bundles)
		{
			if (Entry.BundleName != FName(TEXT("Explicit")))
			{
				Result.Error = TEXT("The saved job label contains an unexpected non-explicit bundle."); return Result;
			}
			Actual.Append(Entry.AssetPaths);
		}
	}
	if (Actual.Num() != Expected.Num() || !Actual.Includes(Expected))
	{
		Result.Error = TEXT("The job label's on-disk explicit bundle does not match the validated source closure."); return Result;
	}
	Result.bSuccess = true;
	Result.LabelObjectPath = Label->GetPathName();
	Result.LabelFilename = Filename;
	Result.ExplicitObjectCount = Actual.Num();
	return Result;
}
