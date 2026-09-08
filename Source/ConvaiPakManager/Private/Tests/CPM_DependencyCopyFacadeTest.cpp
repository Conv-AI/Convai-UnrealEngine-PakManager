// Copyright Convai. All Rights Reserved.

#include "CPM_DependencyCopyAPI.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Hash/Blake3.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PackageTools.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	struct FCopyFacadeFixture
	{
		FString Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		FString Mount = TEXT("/ConvaiCopyFacade_") + Id + TEXT("/");
		FString Parent = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
			/ TEXT("Automation/ConvaiPakManager/CopyFacade"));
		FString Directory = Parent / Id + TEXT("/");
		TArray<TStrongObjectPtr<UObject>> Assets;
		TSet<FName> OwnedPackages;
		UClass* ReferenceClass = nullptr;
		FObjectPropertyBase* HardProperty = nullptr;
		FStructProperty* SoftProperty = nullptr;
		bool bMounted = false;

		bool Initialize()
		{
			FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("ConvaiAvatarPreparationEditor"));
			// Reuse the existing native test asset without exporting a private SDK fixture class.
			ReferenceClass = FindObject<UClass>(nullptr,
				TEXT("/Script/ConvaiAvatarPreparationEditor.ConvaiPreparationReferenceFixture"));
			if (!ReferenceClass) { return false; }
			HardProperty = FindFProperty<FObjectPropertyBase>(ReferenceClass, TEXT("HardReference"));
			SoftProperty = FindFProperty<FStructProperty>(ReferenceClass, TEXT("SoftReference"));
			if (!HardProperty || !SoftProperty || SoftProperty->Struct != TBaseStructure<FSoftObjectPath>::Get()
				|| IFileManager::Get().DirectoryExists(*Directory)
				|| !IFileManager::Get().MakeDirectory(*Directory, true)) { return false; }
			FPackageName::RegisterMountPoint(Mount, Directory);
			bMounted = true;
			return true;
		}

		~FCopyFacadeFixture()
		{
			if (!bMounted) { return; }
			Assets.Reset();
			for (const FName Name : OwnedPackages)
			{
				if (!Name.ToString().StartsWith(Mount, ESearchCase::CaseSensitive)) { continue; }
				if (UPackage* Package = FindPackage(nullptr, *Name.ToString()))
				{
					Package->SetDirtyFlag(false);
					ForEachObjectWithPackage(Package, [](UObject* Object)
					{
						if (Object->IsAsset()) { FAssetRegistryModule::AssetDeleted(Object); }
						Object->ClearFlags(RF_Public | RF_Standalone);
						Object->MarkAsGarbage();
						return true;
					});
					Package->MarkAsGarbage();
				}
			}
			FPackageName::UnRegisterMountPoint(Mount, Directory);
			if (FPaths::IsSamePath(FPaths::GetPath(Directory.LeftChop(1)), Parent)
				&& FPaths::GetCleanFilename(Directory.LeftChop(1)).Equals(Id, ESearchCase::CaseSensitive))
			{
				IFileManager::Get().DeleteDirectory(*Directory, false, true);
			}
		}

		static FString Filename(FName Package)
		{
			return FPackageName::LongPackageNameToFilename(Package.ToString(), FPackageName::GetAssetPackageExtension());
		}

		static FString ObjectPath(FName Package)
		{
			return Package.ToString() + TEXT(".") + FPackageName::GetShortName(Package);
		}

		bool Save(UObject* Asset)
		{
			if (!Asset) { return false; }
			UPackage* Package = Asset->GetPackage();
			Assets.Emplace(Asset);
			OwnedPackages.Add(Package->GetFName());
			const FString Path = Filename(Package->GetFName());
			if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true)) { return false; }
			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Public | RF_Standalone;
			Args.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Package, Asset, *Path, Args)) { return false; }
			FAssetRegistryModule::AssetCreated(Asset);
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get()
				.ScanFilesSynchronous({ Path }, true);
			return true;
		}

		UTexture2D* Texture(FName Name, FColor Pixel)
		{
			UPackage* Package = CreatePackage(*Name.ToString());
			UTexture2D* Asset = NewObject<UTexture2D>(Package, *FPackageName::GetShortName(Name), RF_Public | RF_Standalone);
			Asset->Source.Init(1, 1, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(&Pixel));
			return Save(Asset) ? Asset : nullptr;
		}

		UObject* References(FName Name, UObject* Hard, UObject* Soft)
		{
			UPackage* Package = CreatePackage(*Name.ToString());
			UObject* Asset = NewObject<UObject>(Package, ReferenceClass,
				*FPackageName::GetShortName(Name), RF_Public | RF_Standalone);
			HardProperty->SetObjectPropertyValue_InContainer(Asset, Hard);
			*SoftProperty->ContainerPtrToValuePtr<FSoftObjectPath>(Asset) = FSoftObjectPath(Soft);
			return Save(Asset) ? Asset : nullptr;
		}

		FString Hash(FName Name) const
		{
			TArray<uint8> Bytes;
			return FFileHelper::LoadFileToArray(Bytes, *Filename(Name))
				? LexToString(FBlake3::HashBuffer(Bytes.GetData(), Bytes.Num())) : FString();
		}

		bool Unload(FAutomationTestBase& Test)
		{
			TArray<UPackage*> Packages;
			TArray<TWeakObjectPtr<UObject>> Objects;
			for (FName Name : OwnedPackages)
			{
				if (UPackage* Package = FindPackage(nullptr, *Name.ToString()))
				{
					if (!Test.TestFalse(TEXT("fixture was saved before native unload"), Package->IsDirty())) { return false; }
					Packages.Add(Package);
					ForEachObjectWithPackage(Package, [&Objects](UObject* Object)
					{
						Objects.Emplace(Object);
						return true;
					});
				}
			}
			Assets.Reset();
			UPackageTools::FUnloadPackageParams Params(Packages);
			Params.bResetTransBuffer = false;
			if (!Test.TestTrue(TEXT("native unload succeeds without resetting undo"), UPackageTools::UnloadPackages(Params)))
			{
				Test.AddError(Params.OutErrorMessage.ToString());
				return false;
			}
			bool bAbsent = true;
			for (FName Name : OwnedPackages)
			{
				bAbsent &= Test.TestNull(TEXT("owned package is absent before fresh disk load"), FindPackage(nullptr, *Name.ToString()));
			}
			for (const TWeakObjectPtr<UObject>& Object : Objects)
			{
				bAbsent &= Test.TestFalse(TEXT("previous nested fixture object is gone"), Object.IsValid());
			}
			return bAbsent;
		}

		bool CheckReferences(FAutomationTestBase& Test, FName Root, FName Hard, FName Soft)
		{
			UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPath(Root));
			UObject* HardAsset = LoadObject<UObject>(nullptr, *ObjectPath(Hard));
			UObject* SoftAsset = LoadObject<UObject>(nullptr, *ObjectPath(Soft));
			if (!Test.TestNotNull(TEXT("saved referencer loads"), Asset)
				|| !Test.TestNotNull(TEXT("saved hard target loads"), HardAsset)
				|| !Test.TestNotNull(TEXT("saved soft target loads"), SoftAsset)
				|| !Test.TestTrue(TEXT("referencer retains its native fixture class"), Asset->IsA(ReferenceClass))) { return false; }
			Assets.Emplace(Asset);
			Assets.Emplace(HardAsset);
			Assets.Emplace(SoftAsset);
			const FSoftObjectPath& Path = *SoftProperty->ContainerPtrToValuePtr<FSoftObjectPath>(Asset);
			bool bCorrect = Test.TestTrue(TEXT("saved hard reference resolves to the expected object"),
				HardProperty->GetObjectPropertyValue_InContainer(Asset) == HardAsset);
			bCorrect &= Test.TestTrue(TEXT("saved soft reference retains the exact expected path"),
				Path.ToString().Equals(ObjectPath(Soft), ESearchCase::CaseSensitive));
			bCorrect &= Test.TestTrue(TEXT("saved soft reference resolves to the expected object"), Path.TryLoad() == SoftAsset);
			return bCorrect;
		}

		static bool CheckPixel(FAutomationTestBase& Test, FName Name, FColor Expected)
		{
			UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath(Name));
			if (!Test.TestNotNull(TEXT("saved texture loads"), Texture)) { return false; }
			TArray64<uint8> Pixels;
			if (!Test.TestTrue(TEXT("native texture source mip can be read"), Texture->Source.GetMipData(Pixels, 0))
				|| !Test.TestEqual(TEXT("fixture has one real source pixel"), Pixels.Num(), int64(sizeof(FColor)))) { return false; }
			return Test.TestTrue(TEXT("saved texture payload retains the expected pixel"),
				FMemory::Memcmp(Pixels.GetData(), &Expected, sizeof(Expected)) == 0);
		}
	};

	bool RunFacadeCopy(FAutomationTestBase& Test, bool bExistingDestination)
	{
		FCopyFacadeFixture Fixture;
		if (!Test.TestTrue(TEXT("fresh owned mount and native reference fixture are available"), Fixture.Initialize())) { return false; }
		const FName Root(*(Fixture.Mount + TEXT("Original/Root")));
		const FName Hard(*(Fixture.Mount + TEXT("Original/Hard")));
		const FName Soft(*(Fixture.Mount + TEXT("Original/Soft")));
		const FName Unrelated(*(Fixture.Mount + TEXT("Unrelated")));
		const FString Destination = Fixture.Mount + TEXT("Copied/");
		const auto DestinationName = [&Fixture, &Destination](const TCHAR* Name)
		{
			return FName(*(Destination + Fixture.Mount.Mid(1) + TEXT("Original/") + Name));
		};
		const FName CopiedRoot = DestinationName(TEXT("Root"));
		const FName CopiedHard = DestinationName(TEXT("Hard"));
		const FName CopiedSoft = DestinationName(TEXT("Soft"));
		UTexture2D* HardAsset = Fixture.Texture(Hard, FColor::Red);
		UTexture2D* SoftAsset = Fixture.Texture(Soft, FColor::Yellow);
		if (!Test.TestNotNull(TEXT("saved original hard target"), HardAsset)
			|| !Test.TestNotNull(TEXT("saved original soft target"), SoftAsset)
			|| !Test.TestNotNull(TEXT("saved original hard/soft graph"), Fixture.References(Root, HardAsset, SoftAsset))
			|| !Test.TestNotNull(TEXT("saved unrelated fixture asset"), Fixture.Texture(Unrelated, FColor::White))) { return false; }

		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FName> Dependencies;
		if (!Test.TestTrue(TEXT("native registry resolves saved root dependencies"), Registry.GetDependencies(Root, Dependencies))
			|| !Test.TestTrue(TEXT("native registry sees the hard package"), Dependencies.Contains(Hard))
			|| !Test.TestTrue(TEXT("native registry sees the distinct soft package"), Dependencies.Contains(Soft))) { return false; }

		TMap<FName, FString> Originals;
		for (FName Name : { Root, Hard, Soft, Unrelated })
		{
			const FString Hash = Fixture.Hash(Name);
			if (!Test.TestFalse(TEXT("original hash is available"), Hash.IsEmpty())) { return false; }
			Originals.Add(Name, Hash);
		}
		for (FName Name : { CopiedRoot, CopiedHard, CopiedSoft }) { Fixture.OwnedPackages.Add(Name); }
		if (bExistingDestination)
		{
			if (!Test.TestNotNull(TEXT("existing hard destination has different payload"), Fixture.Texture(CopiedHard, FColor::Blue))
				|| !Test.TestNotNull(TEXT("existing soft destination has different payload"), Fixture.Texture(CopiedSoft, FColor::Green))
				|| !Test.TestNotNull(TEXT("existing destination still references original targets"),
					Fixture.References(CopiedRoot, HardAsset, SoftAsset))) { return false; }
		}
		HardAsset = nullptr;
		SoftAsset = nullptr;
		const FCPM_DependencyCopyOptions Options;
		if (!Test.TestFalse(TEXT("CPM's default remains non-strict"), Options.bStrictValidation)) { return false; }
		const FCPM_DependencyCopyReport Report = FCPM_DependencyCopyAPI::CopyPackageWithDependencies(Root, Destination, Options);
		if (!Test.TestTrue(TEXT("the default CPM facade accepts the saved graph"), Report.bSuccess))
		{
			Test.AddError(Report.ErrorMessage);
			return false;
		}
		Test.TestTrue(TEXT("legacy fixup reports completion"), Report.bReferencesFixedUp);
		Test.TestEqual(TEXT("three legacy copied-or-already-present items"), Report.CopiedCount, 3);
		Test.TestEqual(TEXT("no skipped packages"), Report.SkippedCount, 0);
		Test.TestEqual(TEXT("no failed packages"), Report.FailedCount, 0);
		Test.TestTrue(TEXT("no skipped or failed package entries"), Report.SkippedPackages.IsEmpty() && Report.FailedPackages.IsEmpty());
		Test.TestEqual(TEXT("the native closure has three game packages"), Report.GameDependencyCount, 3);
		Test.TestEqual(TEXT("the native closure has no engine content"), Report.EngineDependencyCount, 0);
		Test.TestEqual(TEXT("three detailed mapped items"), Report.Items.Num(), 3);
		Test.TestEqual(TEXT("three package remaps"), Report.Remap.Num(), 3);
		for (const TPair<FName, FName>& Expected : TMap<FName, FName>{{ Root, CopiedRoot }, { Hard, CopiedHard }, { Soft, CopiedSoft }})
		{
			const FName* Actual = Report.Remap.Find(Expected.Key);
			Test.TestTrue(TEXT("facade returns the legacy mount-preserving destination"), Actual && *Actual == Expected.Value);
			const FCPM_DependencyCopyItem* Item = Report.Items.FindByPredicate(
				[&Expected](const FCPM_DependencyCopyItem& Candidate) { return Candidate.SourcePackage == Expected.Key; });
			Test.TestTrue(TEXT("facade preserves the successful per-package report"), Item && Item->DestPackage == Expected.Value
				&& Item->bPlanned && Item->bCopiedOrMoved && !Item->bSkipped && !Item->bIsEngineAsset && Item->Error.IsEmpty());
		}
		if (!Fixture.Unload(Test)
			|| !Fixture.CheckReferences(Test, CopiedRoot, CopiedHard, CopiedSoft)
			|| !Fixture.CheckReferences(Test, Root, Hard, Soft)) { return false; }
		Fixture.CheckPixel(Test, CopiedHard, bExistingDestination ? FColor::Blue : FColor::Red);
		Fixture.CheckPixel(Test, CopiedSoft, bExistingDestination ? FColor::Green : FColor::Yellow);
		for (const TPair<FName, FString>& Original : Originals)
		{
			Test.TestEqual(TEXT("original and unrelated package bytes are unchanged"), Fixture.Hash(Original.Key), Original.Value);
		}
		if (bExistingDestination)
		{
			Test.AddInfo(TEXT("V0 bOverwriteExisting=true retains existing game payloads and marks them copied; this is not incremental refresh."));
			TMap<FName, FString> BeforeNoOp;
			TArray<FString> SavedFiles;
			for (FName Name : { CopiedRoot, CopiedHard, CopiedSoft })
			{
				const FString Hash = Fixture.Hash(Name);
				if (!Test.TestFalse(TEXT("existing destination hash is available"), Hash.IsEmpty())) { return false; }
				BeforeNoOp.Add(Name, Hash);
				SavedFiles.Add(FCopyFacadeFixture::Filename(Name));
			}
			Registry.ScanFilesSynchronous(SavedFiles, true);
			const FCPM_DependencyCopyReport NoOp = FCPM_DependencyCopyAPI::CopyPackageWithDependencies(CopiedRoot, Destination, Options);
			Test.TestTrue(TEXT("an already-under-destination graph is accepted"), NoOp.bSuccess);
			Test.TestEqual(TEXT("already-under-destination graph is not copied"), NoOp.CopiedCount, 0);
			Test.TestEqual(TEXT("all three existing destinations are reported skipped"), NoOp.SkippedCount, 3);
			Test.TestTrue(TEXT("already-under-destination graph has no remap"), NoOp.Remap.IsEmpty());
			for (const TPair<FName, FString>& Before : BeforeNoOp)
			{
				Test.TestEqual(TEXT("default already-under-destination no-op preserves saved bytes"), Fixture.Hash(Before.Key), Before.Value);
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCPMDependencyCopyFacadeFresh,
	"ConvaiPakManager.Preparation.DefaultFacade.SavesHardAndSoftReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCPMDependencyCopyFacadeFresh::RunTest(const FString&)
{
	return RunFacadeCopy(*this, false);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCPMDependencyCopyFacadeExisting,
	"ConvaiPakManager.Preparation.DefaultFacade.PreservesExistingDestinationBehavior",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCPMDependencyCopyFacadeExisting::RunTest(const FString&)
{
	return RunFacadeCopy(*this, true);
}

#endif
