// Copyright Convai. All Rights Reserved.

#include "Publish/CPM_AvatarCookInputs.h"
#include "AssetRegistry/AssetBundleData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "CoreGlobals.h"
#include "Engine/Blueprint.h"
#include "Engine/PrimaryAssetLabel.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Hash/Blake3.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PackageTools.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	struct FCookFixture
	{
		FString Mount = TEXT("/ConvaiCook_") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT("/");
		FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
			/ TEXT("Automation/ConvaiPakManager/SelectedCook") / Mount.Mid(1).LeftChop(1)) + TEXT("/");
		TArray<TStrongObjectPtr<UObject>> Assets;
		TSet<FName> OwnedPackages;

		FCookFixture()
		{
			IFileManager::Get().MakeDirectory(*Directory, true);
			FPackageName::RegisterMountPoint(Mount, Directory);
		}

		~FCookFixture()
		{
			for (const FName Name : OwnedPackages)
			{
				if (UPackage* Package = FindPackage(nullptr, *Name.ToString()))
				{
					Package->SetDirtyFlag(false);
					ForEachObjectWithPackage(Package, [](UObject* Object)
					{
						if (Object->IsAsset()) { FAssetRegistryModule::AssetDeleted(Object); }
						Object->ClearFlags(RF_Public | RF_Standalone);
						Object->MarkAsGarbage();
						return true;
					}, true);
					Package->MarkAsGarbage();
				}
			}
			Assets.Reset();
			FPackageName::UnRegisterMountPoint(Mount, Directory);
			const FString Allowed = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
				/ TEXT("Automation/ConvaiPakManager/SelectedCook")) + TEXT("/");
			if (Directory.StartsWith(Allowed, ESearchCase::IgnoreCase))
			{
				IFileManager::Get().DeleteDirectory(*Directory, false, true);
			}
		}

		static FString Filename(const FString& Package)
		{
			return FPackageName::LongPackageNameToFilename(Package, FPackageName::GetAssetPackageExtension());
		}

		bool Save(UObject* Asset)
		{
			if (!Asset) { return false; }
			Assets.Emplace(Asset);
			UPackage* Package = Asset->GetPackage();
			OwnedPackages.Add(Package->GetFName());
			const FString Path = Filename(Package->GetName());
			if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true)) { return false; }
			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Public | RF_Standalone;
			Args.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Package, Asset, *Path, Args)) { return false; }
			FAssetRegistryModule::AssetCreated(Asset);
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().ScanFilesSynchronous({ Path }, true);
			return true;
		}

		UTexture2D* Texture(const FString& Relative)
		{
			UPackage* Package = CreatePackage(*(Mount + Relative));
			auto* Asset = NewObject<UTexture2D>(Package, *FPackageName::GetShortName(Relative), RF_Public | RF_Standalone);
			const FColor Pixel = FColor::Red;
			Asset->Source.Init(1, 1, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(&Pixel));
			return Save(Asset) ? Asset : nullptr;
		}

		UBlueprint* Blueprint(const FString& Relative)
		{
			UPackage* Package = CreatePackage(*(Mount + Relative));
			UBlueprint* Asset = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package,
				FName(*FPackageName::GetShortName(Relative)), BPTYPE_Normal);
			if (!Asset) { return nullptr; }
			FKismetEditorUtilities::CompileBlueprint(Asset);
			return Save(Asset) ? Asset : nullptr;
		}

		FString Hash(const FString& Package) const
		{
			TArray<uint8> Bytes;
			return FFileHelper::LoadFileToArray(Bytes, *Filename(Package))
				? LexToString(FBlake3::HashBuffer(Bytes.GetData(), Bytes.Num())) : FString();
		}

		bool Unload(FText& Error)
		{
			TArray<UPackage*> Packages;
			for (const FName Name : OwnedPackages)
			{
				if (UPackage* Package = FindPackage(nullptr, *Name.ToString())) { Packages.Add(Package); }
			}
			Assets.Reset();
			UPackageTools::FUnloadPackageParams Params(Packages);
			Params.bResetTransBuffer = false;
			const bool bSuccess = UPackageTools::UnloadPackages(Params);
			Error = Params.OutErrorMessage;
			return bSuccess;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCPMAvatarCookInputsSavedBundleTest,
	"ConvaiPakManager.Publish.SelectedCook.SavesFreshExplicitBundle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCPMAvatarCookInputsSavedBundleTest::RunTest(const FString&)
{
	FCookFixture Fixture;
	UTexture2D* A = Fixture.Texture(TEXT("AvatarA/Existing"));
	UTexture2D* B = Fixture.Texture(TEXT("AvatarB/Unselected"));
	UBlueprint* Blueprint = Fixture.Blueprint(TEXT("AvatarA/BP_Avatar"));
	if (!TestNotNull(TEXT("Saved selected texture"), A) || !TestNotNull(TEXT("Saved unselected texture"), B)
		|| !TestNotNull(TEXT("Saved selected Blueprint"), Blueprint)
		|| !TestNotNull(TEXT("Compiled generated class"), Blueprint->GeneratedClass.Get())) { return false; }
	const FString AName = A->GetPackage()->GetName();
	const FString BName = B->GetPackage()->GetName();
	const FString BPName = Blueprint->GetPackage()->GetName();
	const FString AHash = Fixture.Hash(AName), BHash = Fixture.Hash(BName), BPHash = Fixture.Hash(BPName);
	TestFalse(TEXT("Source hash was read"), AHash.IsEmpty() || BHash.IsEmpty() || BPHash.IsEmpty());
	const FSoftObjectPath APath(A), BPath(B), BPClassPath(Blueprint->GeneratedClass.Get());
	const FString FirstPackage = Fixture.Mount + TEXT("Jobs/First/PAL_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Fixture.OwnedPackages.Add(FName(*FirstPackage));
	const bool bSilentBefore = GIsSilent, bUnattendedBefore = GIsRunningUnattendedScript;
	const auto First = FCPM_AvatarCookInputs::WriteLabel(FirstPackage, { APath }, { BPClassPath }, 37);
	if (!TestTrue(*First.Error, First.bSuccess)) { return false; }
	TestEqual(TEXT("Both real asset kinds are serialized"), First.ExplicitObjectCount, 2);
	TestEqual(TEXT("Silent scope is restored"), GIsSilent, bSilentBefore);
	TestEqual(TEXT("Unattended scope is restored"), GIsRunningUnattendedScript, bUnattendedBefore);
	const FString FirstHash = Fixture.Hash(FirstPackage);
	UTexture2D* Added = Fixture.Texture(TEXT("AvatarA/AddedAfterFirstLabel"));
	if (!TestNotNull(TEXT("Saved asset added after the first label"), Added)) { return false; }
	const FSoftObjectPath AddedPath(Added);
	const FString SecondPackage = Fixture.Mount + TEXT("Jobs/Second/PAL_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Fixture.OwnedPackages.Add(FName(*SecondPackage));
	const auto Second = FCPM_AvatarCookInputs::WriteLabel(SecondPackage, { APath, AddedPath }, { BPClassPath }, 37);
	if (!TestTrue(*Second.Error, Second.bSuccess)) { return false; }
	TestEqual(TEXT("Fresh job includes the newly saved asset on its first save"), Second.ExplicitObjectCount, 3);
	TestFalse(TEXT("Existing labels cannot be replaced"), FCPM_AvatarCookInputs::WriteLabel(
		FirstPackage, { AddedPath }, {}, 38).bSuccess);
	TestEqual(TEXT("First job label was not refreshed"), Fixture.Hash(FirstPackage), FirstHash);
	TestEqual(TEXT("Selected texture remains unchanged"), Fixture.Hash(AName), AHash);
	TestEqual(TEXT("Unselected texture remains unchanged"), Fixture.Hash(BName), BHash);
	TestEqual(TEXT("Selected Blueprint remains unchanged"), Fixture.Hash(BPName), BPHash);
	FText UnloadError;
	const bool bUnloaded = Fixture.Unload(UnloadError);
	if (!TestTrue(*UnloadError.ToString(), bUnloaded)) { return false; }
	IAssetRegistry::FLoadPackageRegistryData Disk;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get()
		.LoadPackageRegistryData(Second.LabelFilename, Disk);
	TSet<FTopLevelAssetPath> Paths;
	if (Disk.Data.Num() == 1 && Disk.Data[0].TaggedAssetBundles)
	{
		for (const FAssetBundleEntry& Entry : Disk.Data[0].TaggedAssetBundles->Bundles)
		{
			TestEqual(TEXT("Only Explicit metadata is persisted"), Entry.BundleName, FName(TEXT("Explicit")));
			Paths.Append(Entry.AssetPaths);
		}
	}
	TestEqual(TEXT("Disk bundle contains exactly selected objects"), Paths.Num(), 3);
	TestTrue(TEXT("Disk bundle contains existing selected asset"), Paths.Contains(APath.GetAssetPath()));
	TestTrue(TEXT("Disk bundle contains newly added selected asset"), Paths.Contains(AddedPath.GetAssetPath()));
	TestTrue(TEXT("Disk bundle uses the native generated-class path"), Paths.Contains(BPClassPath.GetAssetPath()));
	TestFalse(TEXT("Unselected avatar is absent from disk bundle"), Paths.Contains(BPath.GetAssetPath()));
	UPrimaryAssetLabel* Reloaded = LoadObject<UPrimaryAssetLabel>(nullptr, *Second.LabelObjectPath);
	if (!TestNotNull(TEXT("Reloads the saved job label"), Reloaded)) { return false; }
	TestEqual(TEXT("Persistent nonzero chunk is retained"), Reloaded->Rules.ChunkId, 37);
	TestEqual(TEXT("Explicit closure uses AlwaysCook"), Reloaded->Rules.CookRule, EPrimaryAssetCookRule::AlwaysCook);
	TestFalse(TEXT("No broad directory labeling"), Reloaded->bLabelAssetsInMyDirectory);
	TestFalse(TEXT("No recursive management"), Reloaded->Rules.bApplyRecursively);
	TestFalse(TEXT("Helper label is not runtime content"), Reloaded->bIsRuntimeLabel);
	TestTrue(TEXT("No linked collection"), Reloaded->AssetCollection.CollectionName.IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCPMAvatarCookInputsRefusesHostTest,
	"ConvaiPakManager.Publish.SelectedCook.RefusesHostAndUnverifiedSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCPMAvatarCookInputsRefusesHostTest::RunTest(const FString&)
{
	FCPM_AvatarCookInput Input;
	Input.HostDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	Input.ChildProjectFile = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	Input.JobId = FGuid::NewGuid();
	Input.SelectedMount = TEXT("/UnverifiedAvatar/");
	Input.SelectedPackages = { TEXT("/UnverifiedAvatar/BP_Avatar") };
	Input.ChunkId = 37;
	const auto Host = FCPM_AvatarCookInputs::Prepare(Input, [] { return false; }, [] { return true; });
	TestFalse(TEXT("Ordinary host cannot create a child cook label"), Host.bSuccess);
	TestTrue(TEXT("Host refusal reports project identity"), Host.Error.Contains(TEXT("persistent uploader")));
	const auto Cancelled = FCPM_AvatarCookInputs::Prepare(Input, [] { return true; }, [] { return true; });
	TestFalse(TEXT("Cancellation cannot create a label"), Cancelled.bSuccess);
	TestTrue(TEXT("Cancellation is explicit"), Cancelled.Error.Contains(TEXT("cancelled")));
	const auto Changed = FCPM_AvatarCookInputs::Prepare(Input, [] { return false; }, [] { return false; });
	TestFalse(TEXT("An unknown or changed source snapshot cannot create a label"), Changed.bSuccess);
	TestTrue(TEXT("Snapshot refusal is explicit"), Changed.Error.Contains(TEXT("snapshot")));
	const FString JobPackage = TEXT("/Game/ConvaiAvatarStudioJobs/Job_")
		+ Input.JobId.ToString(EGuidFormats::Digits) + TEXT("/PAL_Selected_") + Input.JobId.ToString(EGuidFormats::Digits);
	TestFalse(TEXT("No host label was saved"), FPackageName::DoesPackageExist(JobPackage));
	TestNull(TEXT("No host label package was created"), FindPackage(nullptr, *JobPackage));
	return true;
}

#endif
