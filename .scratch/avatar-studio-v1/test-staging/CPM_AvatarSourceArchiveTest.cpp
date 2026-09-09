// Copyright Convai. All Rights Reserved.

#include "Publish/CPM_AvatarSourceArchive.h"
#include "Dom/JsonObject.h"
#include "FileUtilities/ZipArchiveReader.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Hash/Blake3.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <winioctl.h>
#include "Windows/HideWindowsPlatformTypes.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	FString Hash(const TArray<uint8>& Bytes)
	{
		return LexToString(FBlake3::HashBuffer(Bytes.GetData(), Bytes.Num()));
	}

	bool CreateFixtureJunction(const FString& Link, FString Target)
	{
		Target.ReplaceInline(TEXT("/"), TEXT("\\"));
		const FString Substitute = TEXT("\\??\\") + Target;
		struct FJunctionData
		{
			DWORD Tag;
			WORD DataLength;
			WORD Reserved;
			WORD SubstituteOffset;
			WORD SubstituteLength;
			WORD PrintOffset;
			WORD PrintLength;
			WCHAR Path[1];
		};
		static_assert(STRUCT_OFFSET(FJunctionData, Path) == 16 && sizeof(WCHAR) == sizeof(TCHAR));
		TArray<uint8> Bytes;
		Bytes.SetNumZeroed(STRUCT_OFFSET(FJunctionData, Path) + (Substitute.Len() + Target.Len() + 2) * sizeof(WCHAR));
		if (Bytes.Num() > MAXIMUM_REPARSE_DATA_BUFFER_SIZE) { return false; }
		auto* Data = reinterpret_cast<FJunctionData*>(Bytes.GetData());
		Data->Tag = IO_REPARSE_TAG_MOUNT_POINT;
		Data->DataLength = static_cast<WORD>(Bytes.Num() - 8);
		Data->SubstituteLength = static_cast<WORD>(Substitute.Len() * sizeof(WCHAR));
		Data->PrintOffset = static_cast<WORD>(Data->SubstituteLength + sizeof(WCHAR));
		Data->PrintLength = static_cast<WORD>(Target.Len() * sizeof(WCHAR));
		FMemory::Memcpy(Data->Path, *Substitute, Data->SubstituteLength);
		FMemory::Memcpy(reinterpret_cast<uint8*>(Data->Path) + Data->PrintOffset, *Target, Data->PrintLength);
		if (!CreateDirectoryW(*Link, nullptr)) { return false; }
		HANDLE Directory = CreateFileW(*Link, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
			FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
		DWORD Returned = 0;
		const bool bCreated = Directory != INVALID_HANDLE_VALUE && DeviceIoControl(Directory, FSCTL_SET_REPARSE_POINT,
			Bytes.GetData(), Bytes.Num(), nullptr, 0, &Returned, nullptr);
		if (Directory != INVALID_HANDLE_VALUE) { CloseHandle(Directory); }
		if (!bCreated) { RemoveDirectoryW(*Link); }
		return bCreated;
	}

	struct FArchiveFixture
	{
		FAutomationTestBase& Test;
		FString Root;
		FString Content;
		FString Output;
		FCPM_AvatarSourceArchiveInput Input;
		TArray<FString> Links;
		bool bReady = false;

		explicit FArchiveFixture(FAutomationTestBase& InTest) : Test(InTest)
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),
				TEXT("Automation/ConvaiPakManager/SelectedSource"), FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			Input.PhysicalPluginRoot = FPaths::Combine(Root, TEXT("Plugins/AvatarAlpha"));
			Content = FPaths::Combine(Input.PhysicalPluginRoot, TEXT("Content"));
			Output = FPaths::Combine(Root, TEXT("Output"));
			if (!IFileManager::Get().MakeDirectory(*FPaths::Combine(Content, TEXT("Data")), true)
				|| !IFileManager::Get().MakeDirectory(*Output, true)) { Test.AddError(TEXT("Cannot create owned archive fixture.")); return; }
			Input.SelectedContentRoot = Content;
			Input.PluginName = TEXT("AvatarAlpha");
			Input.Mount = TEXT("/AvatarAlpha/");
			Input.Blueprint = TEXT("/AvatarAlpha/BP_Avatar.BP_Avatar");
			Input.EngineVersion = TEXT("5.8.0");
			Input.EngineBuild = TEXT("UE-5.8-test");
			Input.SdkFingerprint = TEXT("sdk-test-fingerprint");
			Input.SdkProfile = TEXT("avatar-studio-v1");
			Input.RequiredPlugins.Add({ TEXT("Convai"), TEXT("test-version"), TEXT("test-fingerprint"), { TEXT("Convai") } });
			Input.Packages.Add({ TEXT("/AvatarAlpha/BP_Avatar"), { TEXT("/AvatarAlpha/Data/Geometry"), TEXT("/Script/Convai"), TEXT("/Script/Engine") } });
			Input.Packages.Add({ TEXT("/AvatarAlpha/Data/Geometry"), { TEXT("/Engine/EngineResources/DefaultTexture") } });
			const FString Descriptor = TEXT("{\"FileVersion\":3,\"Version\":1,\"CanContainContent\":true,\"NoCode\":true,\"PrivateSecret\":\"EXCLUDED_SECRET_SENTINEL\",\"Plugins\":[{\"Name\":\"Convai\",\"Enabled\":true}]}");
			bReady = FFileHelper::SaveStringToFile(Descriptor, *FPaths::Combine(Input.PhysicalPluginRoot, TEXT("AvatarAlpha.uplugin")),
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
				&& AddFile(TEXT("BP_Avatar.uasset"), { 1, 2, 3, 4 })
				&& AddFile(TEXT("Data/Geometry.uasset"), { 5, 6, 7, 8 })
				&& AddFile(TEXT("Data/Geometry.ubulk"), { 9, 10, 11, 12 });
			for (const FString& Excluded : { FString(TEXT("Config/DefaultEngine.ini")), FString(TEXT("Plugins/OtherAvatar/Content/Other.uasset")),
				FString(TEXT("Saved/Jobs/session.json")), FString(TEXT("Plugins/AvatarAlpha/Content/cache.json")) })
			{
				const FString Path = FPaths::Combine(Root, Excluded);
				bReady &= IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true)
					&& FFileHelper::SaveStringToFile(TEXT("EXCLUDED_SECRET_SENTINEL"), *Path);
			}
			Test.TestTrue(TEXT("Writes archive byte fixtures and excluded host files"), bReady);
		}

		~FArchiveFixture()
		{
			bool bSafe = Root.StartsWith(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),
				TEXT("Automation/ConvaiPakManager/SelectedSource"))) + TEXT("/"), ESearchCase::IgnoreCase);
			for (int32 Index = Links.Num() - 1; Index >= 0; --Index)
			{
				bSafe &= Links[Index].StartsWith(Root + TEXT("/"), ESearchCase::IgnoreCase) && RemoveDirectoryW(*Links[Index]);
			}
			if (bSafe) { IFileManager::Get().DeleteDirectory(*Root, false, true); }
			else { Test.AddError(TEXT("Owned fixture link cleanup failed; recursive cleanup was refused.")); }
		}

		bool AddFile(const FString& Relative, const TArray<uint8>& Bytes)
		{
			if (!FFileHelper::SaveArrayToFile(Bytes, *FPaths::Combine(Content, Relative))) { return false; }
			Input.Files.Add({ Relative, Hash(Bytes), Bytes.Num() });
			return true;
		}

		bool AddJunction(const FString& Link, const FString& Target)
		{
			const bool bCreated = CreateFixtureJunction(Link, Target);
			if (bCreated) { Links.Add(Link); }
			return Test.TestTrue(TEXT("Creates a real owned NTFS junction"), bCreated);
		}

		FString ArchivePath() const { return FPaths::Combine(Output, TEXT("avatar.zip")); }
		FCPM_AvatarSourceArchiveResult Create() const
		{
			return FCPM_AvatarSourceArchive::Create(Input, Output, TEXT("avatar.zip"), [] { return false; }, [] { return true; });
		}
		bool HasTemporary() const
		{
			TArray<FString> Names;
			IFileManager::Get().FindFiles(Names, *FPaths::Combine(Output, TEXT(".avatar-source-*.tmp")), true, false);
			return !Names.IsEmpty();
		}
		TArray<uint8> ReadArchive() const
		{
			TArray<uint8> Bytes;
			Test.TestTrue(TEXT("Reads the completed fixture archive"), FFileHelper::LoadFileToArray(Bytes, *ArchivePath()));
			return Bytes;
		}
		bool ExpectFailure(const FCPM_AvatarSourceArchiveResult& Result, const TArray<uint8>& Previous)
		{
			Test.TestFalse(TEXT("Refuses the unverified archive"), Result.bSuccess);
			Test.TestFalse(TEXT("Returns a concrete refusal reason"), Result.Error.IsEmpty());
			Test.TestTrue(TEXT("Failure exposes no successful archive binding"), Result.ArchivePath.IsEmpty() && Result.ManifestBlake3.IsEmpty());
			Test.TestTrue(TEXT("Preserves every byte of the previous good archive"), ReadArchive() == Previous);
			return Test.TestFalse(TEXT("Removes only its incomplete temporary archive"), HasTemporary());
		}
	};

	TSharedPtr<FJsonObject> ReadJson(FAutomationTestBase& Test, FZipArchiveReader& Reader, const FString& Name)
	{
		TArray<uint8> Bytes;
		if (!Test.TestTrue(TEXT("Reads generated JSON from the real ZIP"), Reader.TryReadFile(Name, Bytes))) { return nullptr; }
		FUTF8ToTCHAR Utf8(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
		const FString Text(Utf8.Length(), Utf8.Get());
		Test.TestFalse(TEXT("Generated metadata excludes source descriptor secrets"), Text.Contains(TEXT("EXCLUDED_SECRET_SENTINEL")));
		TSharedPtr<FJsonObject> Object;
		Test.TestTrue(TEXT("Generated metadata is JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object));
		return Object;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCPMSelectedSourceArchiveContents,
	"ConvaiPakManager.Publish.SelectedSource.SanitizedContentAndManifest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCPMSelectedSourceArchiveContents::RunTest(const FString&)
{
	FArchiveFixture Fixture(*this);
	if (!Fixture.bReady) { return false; }
	const auto Result = Fixture.Create();
	if (!TestTrue(FString(TEXT("Creates selected source ZIP: ")) + Result.Error, Result.bSuccess)) { return false; }
	TestEqual(TEXT("Counts only explicitly selected content files"), Result.ContentFileCount, 3);
	FZipArchiveReader Reader(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*Result.ArchivePath));
	if (!TestTrue(TEXT("The real ZIP reader accepts the archive"), Reader.IsValid())) { return false; }
	TestEqual(TEXT("Archive contains three content files, two generated descriptors and one manifest"), Reader.GetFileNames().Num(), 6);
	for (const auto& File : Fixture.Input.Files)
	{
		TArray<uint8> Bytes;
		TestTrue(TEXT("Selected package bytes are present"), Reader.TryReadFile(TEXT("Plugins/AvatarAlpha/Content/") + File.ContentRelativePath, Bytes));
		TestEqual(TEXT("Archived bytes have the prepared source hash"), Hash(Bytes), File.Blake3);
	}
	const auto Manifest = ReadJson(*this, Reader, FCPM_AvatarSourceArchive::ManifestFilename);
	const auto Project = ReadJson(*this, Reader, TEXT("AvatarSource.uproject"));
	const auto Plugin = ReadJson(*this, Reader, TEXT("Plugins/AvatarAlpha/AvatarAlpha.uplugin"));
	if (!Manifest || !Project || !Plugin) { return false; }
	TestEqual(TEXT("Manifest format is version one"), Manifest->GetNumberField(TEXT("formatVersion")), 1.0);
	TestEqual(TEXT("Manifest keeps the stable Blueprint"), Manifest->GetObjectField(TEXT("plugin"))->GetStringField(TEXT("blueprint")), Fixture.Input.Blueprint);
	TestEqual(TEXT("Manifest records exact SDK fingerprint"), Manifest->GetObjectField(TEXT("sdk"))->GetStringField(TEXT("fingerprint")), Fixture.Input.SdkFingerprint);
	TestFalse(TEXT("Code prerequisites are explicitly not bundled"), Manifest->GetArrayField(TEXT("requiredPlugins"))[0]->AsObject()->GetBoolField(TEXT("bundled")));
	TestEqual(TEXT("Generated project enables only the avatar and its declared prerequisite"), Project->GetArrayField(TEXT("Plugins")).Num(), 2);
	TestTrue(TEXT("Generated plugin remains content only"), Plugin->GetBoolField(TEXT("NoCode")));
	TestFalse(TEXT("Generated plugin does not implicitly enable itself elsewhere"), Plugin->GetBoolField(TEXT("EnabledByDefault")));
	TestFalse(TEXT("Generated descriptor has no source build steps"), Plugin->HasField(TEXT("PreBuildSteps")));
	TestEqual(TEXT("Manifest records each archived entry except itself"), Manifest->GetArrayField(TEXT("files")).Num(), 5);
	for (const auto& Value : Manifest->GetArrayField(TEXT("files")))
	{
		const auto File = Value->AsObject();
		TArray<uint8> Bytes;
		TestTrue(TEXT("Every manifest entry exists in the archive"), Reader.TryReadFile(File->GetStringField(TEXT("path")), Bytes));
		TestEqual(TEXT("Manifest size matches archived bytes"), File->GetNumberField(TEXT("size")), static_cast<double>(Bytes.Num()));
		TestEqual(TEXT("Manifest hash covers the exact archived bytes"), File->GetStringField(TEXT("blake3")), Hash(Bytes));
	}
	for (const auto& Value : Manifest->GetArrayField(TEXT("packages")))
	{
		const auto Package = Value->AsObject();
		const FString Relative = Package->GetStringField(TEXT("name")).RightChop(Fixture.Input.Mount.Len()) + TEXT(".uasset");
		const auto* File = Fixture.Input.Files.FindByPredicate([&](const auto& Candidate) { return Candidate.ContentRelativePath == Relative; });
		TestTrue(TEXT("Each package hash binds its selected primary source file"), File && File->Blake3 == Package->GetStringField(TEXT("primaryFileBlake3")));
	}
	TArray<uint8> ManifestBytes;
	Reader.TryReadFile(FCPM_AvatarSourceArchive::ManifestFilename, ManifestBytes);
	TestEqual(TEXT("Result binds the exact completed manifest"), Result.ManifestBlake3, Hash(ManifestBytes));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCPMSelectedSourceArchiveRefusals,
	"ConvaiPakManager.Publish.SelectedSource.RejectsUnsafeDeclarations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCPMSelectedSourceArchiveRefusals::RunTest(const FString&)
{
	FArchiveFixture Fixture(*this);
	if (!Fixture.bReady || !TestTrue(TEXT("Creates prior good archive"), Fixture.Create().bSuccess)) { return false; }
	const auto Previous = Fixture.ReadArchive();
	const auto Valid = Fixture.Input;
	for (const FString& Path : { FString(TEXT("../BP_Avatar.uasset")), FString(TEXT("/BP_Avatar.uasset")),
		FString(TEXT("Data\\Geometry.uasset")), FString(TEXT("Data//Geometry.uasset")), FString(TEXT("CON.uasset")),
		FString(TEXT("BP_Avatar.uasset:secret")), FString(TEXT("BP_Avatar.uasset.")), FString(TEXT("Config.ini")),
		FString(TEXT("bp_avatar.uasset")), FString(TEXT("BP_Avatar.UASSET")) })
	{
		Fixture.Input = Valid;
		Fixture.Input.Files[0].ContentRelativePath = Path;
		Fixture.ExpectFailure(Fixture.Create(), Previous);
	}
	Fixture.Input = Valid;
	Fixture.Input.Files.Add(Valid.Files[0]);
	Fixture.Input.Files.Last().ContentRelativePath.ToLowerInline();
	Fixture.ExpectFailure(Fixture.Create(), Previous);
	Fixture.Input = Valid;
	Fixture.Input.Blueprint = TEXT("/avataralpha/BP_Avatar.BP_Avatar");
	Fixture.ExpectFailure(Fixture.Create(), Previous);
	Fixture.Input = Valid;
	Fixture.Input.Blueprint = TEXT("/AvatarAlpha/BP_Avatar.bp_avatar");
	Fixture.ExpectFailure(Fixture.Create(), Previous);
	Fixture.Input = Valid;
	Fixture.Input.Mount = TEXT("/avataralpha/");
	Fixture.ExpectFailure(Fixture.Create(), Previous);
	Fixture.Input = Valid;
	Fixture.Input.Packages[0].Dependencies[0] = TEXT("/AvatarAlpha/Data/geometry");
	Fixture.ExpectFailure(Fixture.Create(), Previous);
	Fixture.Input = Valid;
	Fixture.Input.Packages[0].Dependencies.Add(TEXT("/Game/HostSecret"));
	Fixture.ExpectFailure(Fixture.Create(), Previous);
	Fixture.Input = Valid;
	Fixture.Input.RequiredPlugins.Reset();
	Fixture.ExpectFailure(Fixture.Create(), Previous);
	Fixture.Input = Valid;
	Fixture.Input.MaxSingleFileBytes = 3;
	Fixture.ExpectFailure(Fixture.Create(), Previous);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCPMSelectedSourceArchiveRecovery,
	"ConvaiPakManager.Publish.SelectedSource.ChangeCancellationAndRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCPMSelectedSourceArchiveRecovery::RunTest(const FString&)
{
	FArchiveFixture Fixture(*this);
	if (!Fixture.bReady || !TestTrue(TEXT("Creates prior good archive"), Fixture.Create().bSuccess)) { return false; }
	const auto Previous = Fixture.ReadArchive();
	TestTrue(TEXT("Changes source bytes without changing the expected size"),
		FFileHelper::SaveArrayToFile(TArray<uint8> { 4, 3, 2, 1 }, *FPaths::Combine(Fixture.Content, TEXT("BP_Avatar.uasset"))));
	Fixture.ExpectFailure(Fixture.Create(), Previous);
	TestTrue(TEXT("Restores the expected fixture bytes"),
		FFileHelper::SaveArrayToFile(TArray<uint8> { 1, 2, 3, 4 }, *FPaths::Combine(Fixture.Content, TEXT("BP_Avatar.uasset"))));
	bool bCancelledAfterWriteStarted = false;
	Fixture.ExpectFailure(FCPM_AvatarSourceArchive::Create(Fixture.Input, Fixture.Output, TEXT("avatar.zip"), [&]
	{
		bCancelledAfterWriteStarted |= Fixture.HasTemporary();
		return bCancelledAfterWriteStarted;
	}, [] { return true; }), Previous);
	TestTrue(TEXT("Cancellation really occurred after temporary archive creation"), bCancelledAfterWriteStarted);
	bool bSnapshotChangedAfterWriteStarted = false;
	Fixture.ExpectFailure(FCPM_AvatarSourceArchive::Create(Fixture.Input, Fixture.Output, TEXT("avatar.zip"), [] { return false; }, [&]
	{
		bSnapshotChangedAfterWriteStarted |= Fixture.HasTemporary();
		return !bSnapshotChangedAfterWriteStarted;
	}), Previous);
	TestTrue(TEXT("Snapshot invalidation really occurred during archive preparation"), bSnapshotChangedAfterWriteStarted);
	TestTrue(TEXT("A new valid attempt succeeds after both failures"), Fixture.Create().bSuccess);
	TestFalse(TEXT("Recovery leaves no temporary archive"), Fixture.HasTemporary());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCPMSelectedSourceArchiveLinks,
	"ConvaiPakManager.Publish.SelectedSource.VerifiedRootAndNestedLinkRefusal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCPMSelectedSourceArchiveLinks::RunTest(const FString&)
{
	FArchiveFixture Fixture(*this);
	if (!Fixture.bReady) { return false; }
	const FString LinkedRoot = FPaths::Combine(Fixture.Root, TEXT("LinkedContent"));
	if (!Fixture.AddJunction(LinkedRoot, Fixture.Content)) { return false; }
	Fixture.Input.SelectedContentRoot = LinkedRoot;
	if (!TestTrue(TEXT("Dereferences the one verified selected content root"), Fixture.Create().bSuccess)) { return false; }
	const auto Previous = Fixture.ReadArchive();
	const FString WrongRoot = FPaths::Combine(Fixture.Root, TEXT("WrongContent"));
	if (!Fixture.AddJunction(WrongRoot, Fixture.Output)) { return false; }
	Fixture.Input.SelectedContentRoot = WrongRoot;
	Fixture.ExpectFailure(Fixture.Create(), Previous);
	Fixture.Input.SelectedContentRoot = LinkedRoot;
	const FString NestedLink = FPaths::Combine(Fixture.Content, TEXT("Nested"));
	if (!Fixture.AddJunction(NestedLink, FPaths::Combine(Fixture.Content, TEXT("Data")))) { return false; }
	Fixture.Input.Files[1].ContentRelativePath = TEXT("Nested/Geometry.uasset");
	Fixture.Input.Files[2].ContentRelativePath = TEXT("Nested/Geometry.ubulk");
	Fixture.Input.Packages[1].Name = TEXT("/AvatarAlpha/Nested/Geometry");
	Fixture.Input.Packages[0].Dependencies[0] = Fixture.Input.Packages[1].Name;
	Fixture.ExpectFailure(Fixture.Create(), Previous);
	Fixture.Input.Files[1].ContentRelativePath = TEXT("Data/Geometry.uasset");
	Fixture.Input.Files[2].ContentRelativePath = TEXT("Data/Geometry.ubulk");
	Fixture.Input.Packages[1].Name = TEXT("/AvatarAlpha/Data/Geometry");
	Fixture.Input.Packages[0].Dependencies[0] = Fixture.Input.Packages[1].Name;
	const FString HardLink = FPaths::Combine(Fixture.Root, TEXT("alias.uasset"));
	if (!TestTrue(TEXT("Creates an actual owned hard-link alias"), CreateHardLinkW(*HardLink,
		*FPaths::Combine(Fixture.Content, TEXT("BP_Avatar.uasset")), nullptr))) { return false; }
	Fixture.ExpectFailure(Fixture.Create(), Previous);
	TestTrue(TEXT("Removes the fixture hard-link alias"), DeleteFileW(*HardLink));
	return true;
}

#endif
