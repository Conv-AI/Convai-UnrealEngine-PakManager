// Copyright Convai. All Rights Reserved.

#include "Jobs/CPM_PublishJobs.h"
#include "Jobs/CPM_PublishRunner.h"
#include "Utility/CPM_UtilityLibrary.h"
#include "AssetRegistry/AssetBundleData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerSettings.h"
#include "Engine/PrimaryAssetLabel.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "Hash/Blake3.h"
#include "ILiveCodingModule.h"
#include "Interfaces/IPluginManager.h"
#include "IPlatformFilePak.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "TargetReceipt.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

#if WITH_AUTOMATION_TESTS && PLATFORM_WINDOWS

namespace
{
	constexpr const TCHAR* FixtureName = TEXT("AvatarStudioV0Packaging");
	constexpr const TCHAR* FixtureMount = TEXT("/Game/ConvaiV0Packaging");
	constexpr const TCHAR* McpSection = TEXT("/Script/ModelContextProtocolEngine.ModelContextProtocolSettings");
	constexpr const TCHAR* CookMcpArgument = TEXT("-AdditionalCookerOptions=-ini:EditorPerProjectUserSettings:")
		TEXT("[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False");

	FString Absolute(FString Path)
	{
		Path = FPaths::ConvertRelativePathToFull(Path);
		FPaths::NormalizeFilename(Path);
		FPaths::CollapseRelativeDirectories(Path);
		return Path;
	}

	bool LocalAbsolute(const FString& Path)
	{
		return Path.Len() > 3 && FChar::IsAlpha(Path[0]) && Path[1] == TEXT(':')
			&& (Path[2] == TEXT('/') || Path[2] == TEXT('\\')) && !Path.Mid(2).Contains(TEXT(":"));
	}

	bool Inside(const FString& Path, const FString& Directory)
	{
		return Absolute(Path).StartsWith(Absolute(Directory) + TEXT("/"), ESearchCase::IgnoreCase);
	}

	bool PlainAncestors(const FString& Directory)
	{
		FString Path = Absolute(Directory);
		while (Path.Len() > 3)
		{
			const DWORD Attributes = GetFileAttributesW(*Path);
			if (Attributes == INVALID_FILE_ATTRIBUTES || !(Attributes & FILE_ATTRIBUTE_DIRECTORY)
				|| (Attributes & FILE_ATTRIBUTE_REPARSE_POINT)) { return false; }
			Path = FPaths::GetPath(Path);
		}
		return true;
	}

	bool PlainFile(const FString& Path)
	{
		HANDLE Handle = CreateFileW(*Path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
		if (Handle == INVALID_HANDLE_VALUE) { return false; }
		BY_HANDLE_FILE_INFORMATION Info{};
		const bool bPlain = GetFileInformationByHandle(Handle, &Info)
			&& !(Info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
			&& Info.nNumberOfLinks == 1;
		CloseHandle(Handle);
		return bPlain;
	}

	bool PlainTree(const FString& Root)
	{
		if (!PlainAncestors(Root)) { return false; }
		TArray<FString> Pending{ Root };
		int32 Count = 0;
		while (!Pending.IsEmpty())
		{
			const FString Directory = Pending.Pop();
			if (!FPlatformFileManager::Get().GetPlatformFile().IterateDirectory(*Directory,
				[&](const TCHAR* Path, bool bDirectory)
				{
					const DWORD Attributes = GetFileAttributesW(Path);
					if (++Count > 200000 || Attributes == INVALID_FILE_ATTRIBUTES
						|| (Attributes & FILE_ATTRIBUTE_REPARSE_POINT)) { return false; }
					if (bDirectory) { Pending.Add(Path); }
					return true;
				})) { return false; }
		}
		return true;
	}

	struct FIniSnapshot
	{
		FString Path;
		TArray<uint8> Bytes;
		bool bExisted = false;

		bool Capture(const FString& InPath)
		{
			Path = Absolute(InPath);
			bExisted = IFileManager::Get().FileExists(*Path);
			return !bExisted || (PlainFile(Path) && IFileManager::Get().FileSize(*Path) <= 1024 * 1024
				&& FFileHelper::LoadFileToArray(Bytes, *Path));
		}

		bool Unchanged() const
		{
			if (bExisted != IFileManager::Get().FileExists(*Path)) { return false; }
			if (!bExisted) { return true; }
			TArray<uint8> Current;
			return PlainFile(Path) && IFileManager::Get().FileSize(*Path) == Bytes.Num()
				&& FFileHelper::LoadFileToArray(Current, *Path) && Current == Bytes;
		}
	};

	class FPackageCommandAudit final : public FOutputDevice
	{
	public:
		FPackageCommandAudit() { GLog->AddOutputDevice(this); }
		virtual ~FPackageCommandAudit() override { if (GLog) { GLog->RemoveOutputDevice(this); } }
		virtual bool CanBeUsedOnAnyThread() const override { return true; }
		virtual bool CanBeUsedOnMultipleThreads() const override { return true; }
		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type, const FName& Category) override
		{
			if (Category != FName(TEXT("ConvaiPakManagerLog"))
				|| !FStringView(Message).StartsWith(TEXT("Packaging Win64 (Development): "))) { return; }
			FScopeLock Lock(&Mutex);
			++Count;
			if (Count == 1) { Command = FString(Message).Left(16384); }
		}
		void Read(int32& OutCount, FString& OutCommand)
		{
			FScopeLock Lock(&Mutex);
			OutCount = Count;
			OutCommand = Command;
		}
	private:
		FCriticalSection Mutex;
		int32 Count = 0;
		FString Command;
	};

	bool CheckOwnership(FAutomationTestBase& Test, FString& OutProject)
	{
		FString Requested, Host;
		if (!FParse::Value(FCommandLine::Get(), TEXT("AvatarStudioV0PackagingProject="), Requested)
			|| !FParse::Value(FCommandLine::Get(), TEXT("AvatarStudioV0PackagingHost="), Host)
			|| !LocalAbsolute(Requested) || !LocalAbsolute(Host))
		{
			Test.AddError(TEXT("Real V0 packaging requires absolute owned project and host arguments."));
			return false;
		}
		OutProject = Absolute(FPaths::GetProjectFilePath());
		const FString Directory = FPaths::GetPath(OutProject);
		const FString Id = FPaths::GetCleanFilename(Directory);
		FGuid ParsedId;
		const FString Expected = Absolute(Host / TEXT("Saved/ConvaiAvatarStudio/Development/V0Packaging")
			/ Id / (FString(FixtureName) + TEXT(".uproject")));
		const FString Marker = Directory / TEXT("Saved/ConvaiAvatarStudio/Development/owned-v0-packaging-project.txt");
		FString MarkerText;
		if (!FGuid::ParseExact(Id, EGuidFormats::Digits, ParsedId) || !ParsedId.IsValid()
			|| !OutProject.Equals(Absolute(Requested), ESearchCase::IgnoreCase)
			|| !OutProject.Equals(Expected, ESearchCase::IgnoreCase)
			|| !PlainFile(OutProject) || !PlainAncestors(FPaths::GetPath(Marker)) || !PlainFile(Marker)
			|| IFileManager::Get().FileSize(*Marker) > 4096 || !FFileHelper::LoadFileToString(MarkerText, *Marker)
			|| !OutProject.Equals(MarkerText.TrimStartAndEnd(), ESearchCase::IgnoreCase)
			|| !PlainTree(Directory))
		{
			Test.AddError(TEXT("Refusing V0 package mutations: project/marker/physical GUID fixture ownership is unverified."));
			return false;
		}
		if (IsRunningCommandlet() || !GIsEditor || !FSlateApplication::IsInitialized()
			|| !FParse::Param(FCommandLine::Get(), TEXT("NoLiveCoding")))
		{
			Test.AddError(TEXT("The owned fixture requires a normal Slate editor launched with -NoLiveCoding."));
			return false;
		}
		if (ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
			LiveCoding && LiveCoding->IsEnabledForSession())
		{
			Test.AddError(TEXT("Live Coding is enabled; refusing to start the packaging fixture."));
			return false;
		}
		for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
		{
			if (!Inside(Plugin->GetBaseDir(), FPaths::EnginePluginsDir())
				&& !Inside(Plugin->GetBaseDir(), Directory / TEXT("Plugins")))
			{
				Test.AddError(TEXT("An enabled non-engine plugin is outside the physical owned test project."));
				return false;
			}
		}
		FTargetReceipt EditorReceipt;
		if (!PlainFile(Directory / TEXT("Source/AvatarStudioV0Packaging.Target.cs"))
			|| !PlainFile(Directory / TEXT("Source/AvatarStudioV0PackagingEditor.Target.cs"))
			|| !EditorReceipt.Read(Directory / TEXT("Binaries/Win64/AvatarStudioV0PackagingEditor.target"))
			|| EditorReceipt.TargetType != EBuildTargetType::Editor
			|| EditorReceipt.TargetName != TEXT("AvatarStudioV0PackagingEditor")
			|| !Absolute(EditorReceipt.ProjectFile).Equals(OutProject, ESearchCase::IgnoreCase)
			|| !EditorReceipt.LaunchesCurrentExecutable())
		{
			Test.AddError(TEXT("The owned plain project's real Editor build and matching Game target are required."));
			return false;
		}
		for (const TCHAR* Relative : { TEXT("PackagedApp"), TEXT("Saved/Cooked"), TEXT("Saved/StagedBuilds"),
			TEXT("Content/ConvaiV0Packaging"), TEXT("Binaries/Win64/AvatarStudioV0Packaging.target") })
		{
			if (GetFileAttributesW(*(Directory / Relative)) != INVALID_FILE_ATTRIBUTES)
			{
				Test.AddError(FString::Printf(TEXT("Use a fresh owned project: %s already exists."), Relative));
				return false;
			}
		}
		return true;
	}

	bool CheckConfiguration(FAutomationTestBase& Test)
	{
		const UAssetManagerSettings* Settings = GetDefault<UAssetManagerSettings>();
		if (!Settings || Settings->PrimaryAssetTypesToScan.Num() != 1 || !Settings->DirectoriesToExclude.IsEmpty()
			|| !Settings->PrimaryAssetRules.IsEmpty() || !Settings->CustomPrimaryAssetRules.IsEmpty())
		{
			Test.AddError(TEXT("Use the isolated single PrimaryAssetLabel scan profile with no overrides/exclusions."));
			return false;
		}
		const FPrimaryAssetTypeInfo& Type = Settings->PrimaryAssetTypesToScan[0];
		if (Type.PrimaryAssetType != FName(TEXT("PrimaryAssetLabel")) || Type.bHasBlueprintClasses || Type.bIsEditorOnly
			|| Type.GetAssetBaseClass().ToSoftObjectPath() != FSoftObjectPath(UPrimaryAssetLabel::StaticClass())
			|| Type.GetDirectories().Num() != 1 || Type.GetDirectories()[0].Path != FixtureMount
			|| !Type.GetSpecificAssets().IsEmpty() || Type.Rules.CookRule != EPrimaryAssetCookRule::AlwaysCook)
		{
			Test.AddError(TEXT("The native label scan must cook only /Game/ConvaiV0Packaging with AlwaysCook."));
			return false;
		}
		const TCHAR* Packaging = TEXT("/Script/UnrealEd.ProjectPackagingSettings");
		for (const auto& Expected : TArray<TPair<FString, bool>>{
			{ TEXT("bCookAll"), false }, { TEXT("bCookMapsOnly"), false }, { TEXT("bGenerateChunks"), true },
			{ TEXT("bGenerateNoChunks"), false }, { TEXT("UsePakFile"), true },
			{ TEXT("bUseIoStore"), false }, { TEXT("bUseZenStore"), false } })
		{
			bool Value = !Expected.Value;
			if (!GConfig->GetBool(Packaging, *Expected.Key, Value, GGameIni) || Value != Expected.Value)
			{
				Test.AddError(TEXT("The owned chunk/Pak/no-IoStore/no-Zen packaging profile is missing: ") + Expected.Key);
				return false;
			}
		}
		for (const TCHAR* Key : { TEXT("DirectoriesToAlwaysCook"), TEXT("DirectoriesToNeverCook") })
		{
			TArray<FString> Paths;
			GConfig->GetArray(Packaging, Key, Paths, GGameIni);
			if (!Paths.IsEmpty()) { Test.AddError(TEXT("Fixture packaging directory overrides must be empty.")); return false; }
		}
		TArray<FString> Maps;
		GConfig->GetArray(Packaging, TEXT("MapsToCook"), Maps, GGameIni);
		FString Map;
		if (Maps.Num() != 1 || !FParse::Value(*Maps[0], TEXT("FilePath="), Map) || Map != TEXT("/Engine/Maps/Entry"))
		{
			Test.AddError(TEXT("The owned packaging profile must cook only /Engine/Maps/Entry."));
			return false;
		}
		return true;
	}

	struct FPackagingState
	{
		FAutomationTestBase& Test;
		FString Project;
		FString Root = FString(FixtureMount) + TEXT("/") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		TArray<TStrongObjectPtr<UObject>> Assets;
		TArray<FString> LabelNames;
		TArray<FString> TextureNames;
		TArray<FString> TextureHashes;
		TArray<FIniSnapshot> IniSnapshots;
		TStrongObjectPtr<UCPM_PublishRunner> Runner;
		TUniquePtr<FPackageCommandAudit> Audit;
		ECPM_PublishResult Result = ECPM_PublishResult::Failed;
		FString Error;
		int32 FinishedCount = 0;
		int32 McpPort = 0;
		bool bMcpAutoStart = false;
		bool bDeadlineReported = false;
		double StartedAt = 0;

		explicit FPackagingState(FAutomationTestBase& InTest) : Test(InTest) {}

		static FString Filename(const FString& Package)
		{
			return FPackageName::LongPackageNameToFilename(Package, FPackageName::GetAssetPackageExtension());
		}

		static FString Hash(const FString& Package)
		{
			const FString Path = Filename(Package);
			TArray<uint8> Bytes;
			return PlainFile(Path) && IFileManager::Get().FileSize(*Path) <= 1024 * 1024
				&& FFileHelper::LoadFileToArray(Bytes, *Path)
				? LexToString(FBlake3::HashBuffer(Bytes.GetData(), Bytes.Num())) : FString();
		}

		bool Save(UObject* Asset)
		{
			if (!Asset) { return false; }
			Assets.Emplace(Asset);
			FAssetRegistryModule::AssetCreated(Asset);
			const FString Path = Filename(Asset->GetPackage()->GetName());
			if (IFileManager::Get().FileExists(*Path) || !Inside(Path, FPaths::ProjectContentDir())
				|| !IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true)) { return false; }
			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Public | RF_Standalone;
			Args.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Asset->GetPackage(), Asset, *Path, Args)) { return false; }
			IAssetRegistry::GetChecked().ScanFilesSynchronous({ Path }, true);
			return !Asset->GetPackage()->IsDirty();
		}

		bool ReadBundle(const FString& Package, TSet<FTopLevelAssetPath>& Paths)
		{
			Paths.Reset();
			IAssetRegistry::FLoadPackageRegistryData Disk;
			IAssetRegistry::GetChecked().LoadPackageRegistryData(Filename(Package), Disk);
			if (Disk.Data.Num() != 1 || !Disk.Data[0].TaggedAssetBundles) { return false; }
			for (const FAssetBundleEntry& Bundle : Disk.Data[0].TaggedAssetBundles->Bundles)
			{
				if (Bundle.BundleName == UPrimaryAssetLabel::DirectoryBundle)
				{
					Paths.Append(Bundle.AssetPaths);
					return true;
				}
			}
			return false;
		}

		bool Initialize()
		{
			if (!CheckConfiguration(Test)
				|| !Test.TestTrue(TEXT("Native Asset Manager is initialized before fixture mutation"), UAssetManager::IsInitialized())) { return false; }
			FARFilter Filter;
			Filter.ClassPaths.Add(UPrimaryAssetLabel::StaticClass()->GetClassPathName());
			Filter.bRecursiveClasses = true;
			TArray<FAssetData> Existing;
			IAssetRegistry::GetChecked().GetAssets(Filter, Existing);
			if (!Existing.IsEmpty())
			{
				Test.AddError(TEXT("The plain fixture must contain no pre-existing PrimaryAssetLabels: V0 saves all of them."));
				return false;
			}
			if (!GConfig->GetBool(McpSection, TEXT("bAutoStartServer"), bMcpAutoStart, GEditorPerProjectIni)
				|| !bMcpAutoStart || !GConfig->GetInt(McpSection, TEXT("ServerPortNumber"), McpPort, GEditorPerProjectIni)
				|| McpPort <= 0 || McpPort > 65535 || !FModuleManager::Get().IsModuleLoaded(TEXT("ModelContextProtocol")))
			{
				Test.AddError(TEXT("Enable MCP at an owned unused port in this fixture editor before testing the cook-only override."));
				return false;
			}
			const FString Directory = FPaths::GetPath(Project);
			for (const FString& Path : { Directory / TEXT("Config/DefaultGame.ini"),
				Directory / TEXT("Config/DefaultEditorPerProjectUserSettings.ini"), Absolute(GEditorPerProjectIni) })
			{
				if (!Inside(Path, Directory) || !IniSnapshots.Emplace_GetRef().Capture(Path))
				{
					Test.AddError(TEXT("Cannot snapshot owned fixture INI files before packaging."));
					return false;
				}
			}
			for (int32 Avatar = 0; Avatar != 2; ++Avatar)
			{
				const FString AvatarPath = Root / (Avatar == 0 ? TEXT("AvatarA") : TEXT("AvatarB"));
				const FString LabelName = AvatarPath / (Avatar == 0 ? TEXT("PAL_A") : TEXT("PAL_B"));
				UPrimaryAssetLabel* Label = NewObject<UPrimaryAssetLabel>(CreatePackage(*LabelName),
					*FPackageName::GetShortName(LabelName), RF_Public | RF_Standalone);
				Label->bLabelAssetsInMyDirectory = true;
				Label->bIsRuntimeLabel = false;
				Label->Rules.ChunkId = 371 + Avatar;
				Label->Rules.Priority = 1;
				Label->Rules.CookRule = EPrimaryAssetCookRule::AlwaysCook;
				Label->Rules.bApplyRecursively = false;
				if (!Test.TestTrue(TEXT("Save the real directory label before copying content"), Save(Label))) { return false; }
				LabelNames.Add(LabelName);
				for (const TCHAR* AssetName : { TEXT("T_Existing"), TEXT("T_AddedAfterLabel") })
				{
					const FString Name = AvatarPath / AssetName;
					auto* Texture = NewObject<UTexture2D>(CreatePackage(*Name), AssetName, RF_Public | RF_Standalone);
					const FColor Pixel = Avatar == 0 ? FColor::Red : FColor::Blue;
					Texture->Source.Init(1, 1, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(&Pixel));
					Texture->SRGB = false;
					if (!Test.TestTrue(TEXT("Save native texture added after label serialization"), Save(Texture))) { return false; }
					TextureNames.Add(Name);
					TextureHashes.Add(Hash(Name));
					if (!Test.TestFalse(TEXT("Saved texture hash is available"), TextureHashes.Last().IsEmpty())) { return false; }
				}
				TSet<FTopLevelAssetPath> Disk;
				if (!Test.TestTrue(TEXT("Read the label's real saved directory bundle"), ReadBundle(LabelName, Disk))) { return false; }
				for (int32 Index = Avatar * 2; Index != Avatar * 2 + 2; ++Index)
				{
					const FString& Name = TextureNames[Index];
					if (!Test.TestFalse(TEXT("The persisted label is stale before V0 packaging"),
						Disk.Contains(FTopLevelAssetPath(FName(*Name), FName(*FPackageName::GetShortName(Name)))))) { return false; }
				}
			}
			Existing.Reset();
			IAssetRegistry::GetChecked().GetAssets(Filter, Existing);
			if (!Test.TestEqual(TEXT("Exactly the two owned labels will be resaved by V0"), Existing.Num(), 2)) { return false; }
			for (const FAssetData& Label : Existing)
			{
				if (!LabelNames.Contains(Label.PackageName.ToString()) || !PlainFile(Filename(Label.PackageName.ToString())))
				{
					Test.AddError(TEXT("Unexpected label discovered before V0's broad resave; refusing packaging."));
					return false;
				}
			}
			Test.AddInfo(FString::Printf(TEXT("Owned V0 fixture ready: %s; editor PID %u; MCP configured port %d. ")
				TEXT("Coordinator must independently probe the real listener; this is not a listener assertion."),
				*Root, FPlatformProcess::GetCurrentProcessId(), McpPort));
			return true;
		}

		void VerifyPak(int32 Avatar)
		{
			const FString Path = UCPM_UtilityLibrary::GetPakFilePathFromChunkID(ECPM_Platform::Windows, FString::FromInt(371 + Avatar));
			Test.AddInfo(TEXT("Real retained Pak: ") + Path);
			if (!Test.TestTrue(TEXT("Fresh requested and unrequested chunk Paks exist"), PlainFile(Path))) { return; }
			TRefCountPtr<FPakFile> Pak = new FPakFile(&FPlatformFileManager::Get().GetPlatformFile(), *Path, false);
			if (!Test.TestTrue(TEXT("Native reader opens the real cooked Pak index"), Pak->IsValid())) { return; }
			Test.TestEqual(TEXT("Native Pak filename retains the nonzero chunk identity"), Pak->PakGetPakchunkIndex(), 371 + Avatar);
			for (int32 Index = 0; Index != TextureNames.Num(); ++Index)
			{
				const FString Entry = FString(TEXT("../../../")) + FixtureName + TEXT("/Content/")
					+ TextureNames[Index].RightChop(6) + TEXT(".uasset");
				FPakEntry Info;
				const bool bFound = Pak->Find(Entry, &Info) == FPakFile::EFindResult::Found;
				Test.TestEqual(*(TEXT("Exact real cooked entry: ") + Entry), bFound, Index / 2 == Avatar);
				if (bFound) { Test.TestTrue(TEXT("Cooked asset has a nonempty payload"), Info.Size > 0); }
			}
		}

		void Verify()
		{
			Test.TestEqual(TEXT("The real package runner resolves once"), FinishedCount, 1);
			Test.TestTrue(*(TEXT("Real V0 UAT package result: ") + Error), Result == ECPM_PublishResult::Success);
			int32 Commands = 0;
			FString Command;
			Audit->Read(Commands, Command);
			Test.TestEqual(TEXT("Exactly one real V0 UAT packaging invocation"), Commands, 1);
			Test.TestTrue(TEXT("Actual UAT command preserves the cook-only MCP override"), Command.Contains(CookMcpArgument, ESearchCase::CaseSensitive));
			Test.TestTrue(TEXT("Actual UAT command builds the Game target without skipping its build"),
				Command.Contains(TEXT(" -build ")) && !Command.Contains(TEXT(" -skipbuild "))
				&& Command.Contains(TEXT("-target=AvatarStudioV0Packaging "))
				&& Command.Contains(TEXT("-clientconfig=Development ")));
			Test.AddInfo(TEXT("Actual V0 packaging argument evidence: ") + Command);
			for (const FIniSnapshot& Snapshot : IniSnapshots)
			{
				Test.TestTrue(TEXT("Packaging preserves exact owned editor/project INI bytes"), Snapshot.Unchanged());
			}
			bool CurrentAutoStart = false;
			int32 CurrentPort = 0;
			Test.TestTrue(TEXT("Effective original-editor MCP remains enabled at the same port"),
				GConfig->GetBool(McpSection, TEXT("bAutoStartServer"), CurrentAutoStart, GEditorPerProjectIni)
				&& GConfig->GetInt(McpSection, TEXT("ServerPortNumber"), CurrentPort, GEditorPerProjectIni)
				&& CurrentAutoStart == bMcpAutoStart && CurrentPort == McpPort);
			for (int32 Avatar = 0; Avatar != 2; ++Avatar)
			{
				TSet<FTopLevelAssetPath> Disk;
				if (Test.TestTrue(TEXT("Read the refreshed bundle from its actual saved package"), ReadBundle(LabelNames[Avatar], Disk)))
				{
					for (int32 Index = 0; Index != TextureNames.Num(); ++Index)
					{
						const FString& Name = TextureNames[Index];
						const bool bContains = Disk.Contains(FTopLevelAssetPath(FName(*Name), FName(*FPackageName::GetShortName(Name))));
						Test.TestEqual(TEXT("Both saved directory bundles refreshed, preserving their separate avatar sets"), bContains, Index / 2 == Avatar);
					}
				}
			}
			for (int32 Index = 0; Index != TextureNames.Num(); ++Index)
			{
				Test.TestEqual(TEXT("V0 packaging does not modify saved authored texture bytes"), Hash(TextureNames[Index]), TextureHashes[Index]);
			}
			if (Result != ECPM_PublishResult::Success) { return; }
			const FCPM_PublishContext& Context = Runner->GetContext();
			Test.TestEqual(TEXT("Only the requested Windows artifact is returned to the local runner"), Context.Paks.Num(), 1);
			if (Context.Paks.Num() == 1)
			{
				Test.TestEqual(TEXT("Real package retains the engine Windows version slot"), Context.Paks[0].VersionSlot,
					FCPM_PakArtifact::VersionSlotFor(ECPM_Platform::Windows));
			}
			Test.TestTrue(TEXT("Local package fixture neither archives nor creates a cloud asset"),
				!Context.bHasRawArchive && Context.Published.AssetId.IsEmpty());
			FTargetReceipt GameReceipt;
			const bool bGameBuilt = GameReceipt.Read(FPaths::GetPath(Project) / TEXT("Binaries/Win64/AvatarStudioV0Packaging.target"));
			Test.TestTrue(TEXT("UAT produced an actual matching Development Win64 Game build and executable"),
				bGameBuilt && GameReceipt.TargetType == EBuildTargetType::Game
				&& GameReceipt.Configuration == EBuildConfiguration::Development && GameReceipt.Platform == TEXT("Win64")
				&& GameReceipt.TargetName == FixtureName && Absolute(GameReceipt.ProjectFile).Equals(Project, ESearchCase::IgnoreCase)
				&& PlainFile(GameReceipt.Launch));
			VerifyPak(0);
			VerifyPak(1);
		}
	};

	class FRunV0Packaging final : public IAutomationLatentCommand
	{
	public:
		explicit FRunV0Packaging(TSharedRef<FPackagingState> InState) : State(MoveTemp(InState)) {}
		virtual bool Update() override
		{
			if (!State->Runner.IsValid())
			{
				IAssetRegistry& Registry = IAssetRegistry::GetChecked();
				if (Registry.IsLoadingAssets() || Registry.IsGathering())
				{
					if (FPlatformTime::Seconds() < RegistryDeadline) { return false; }
					State->Test.AddError(TEXT("Asset Registry did not settle before the fixture's mutation gate."));
					return true;
				}
				if (!State->Initialize()) { return true; }
				State->Runner.Reset(NewObject<UCPM_PublishRunner>());
				State->Audit = MakeUnique<FPackageCommandAudit>();
				FCPM_PublishContext Context;
				Context.Request.ChunkId = 371;
				Context.Request.Policy.Windows.bShouldPackage = true;
				Context.Request.Policy.Windows.Configuration = TEXT("Development");
				Context.Request.bReuseExistingPaks = false;
				Context.Request.EnvironmentSlug = TEXT("owned-local-fixture-no-backend");
				FCPM_OnPublishFinished Finished;
				Finished.BindLambda([Weak = TWeakPtr<FPackagingState>(State)](ECPM_PublishResult Result, const FString& Error, float)
				{
					if (TSharedPtr<FPackagingState> Pinned = Weak.Pin())
					{
						++Pinned->FinishedCount;
						Pinned->Result = Result;
						Pinned->Error = Error;
					}
				});
				State->StartedAt = FPlatformTime::Seconds();
				State->Runner->Start({ NewObject<UCPM_PackagePaksJob>() }, Context, FCPM_OnPublishProgress(), MoveTemp(Finished));
				return false;
			}
			if (State->FinishedCount == 0)
			{
				if (!State->bDeadlineReported && FPlatformTime::Seconds() - State->StartedAt > 1800.0)
				{
					State->bDeadlineReported = true;
					State->Test.AddError(TEXT("Real V0 package exceeded 30 minutes. Still retaining its fixture while UAT is outstanding; ")
						TEXT("coordinator must inspect or stop only its owned process tree. V0 Cancel is not drain proof."));
				}
				return false;
			}
			State->Verify();
			State->Test.AddInfo(TEXT("Owned project and real source/build/cook/Pak artifacts retained for independent inspection."));
			return true;
		}
	private:
		TSharedRef<FPackagingState> State;
		double RegistryDeadline = FPlatformTime::Seconds() + 120.0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCPMV0RealAllLabelPackagingTest,
	"ConvaiPakManager.Integration.V0.RealAllLabelPackaging",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCPMV0RealAllLabelPackagingTest::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("AvatarStudioV0Packaging")))
	{
		AddWarning(TEXT("Real V0 UAT integration was not run; requires the explicit owned-project packaging opt-in."));
		return true;
	}
	TSharedRef<FPackagingState> State = MakeShared<FPackagingState>(*this);
	if (!CheckOwnership(*this, State->Project)) { return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FRunV0Packaging(State));
	return true;
}

#endif
