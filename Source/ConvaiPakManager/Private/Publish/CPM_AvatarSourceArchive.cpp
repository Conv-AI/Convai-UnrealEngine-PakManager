// Copyright Convai. All Rights Reserved.

#include "Publish/CPM_AvatarSourceArchive.h"
#include "Dom/JsonObject.h"
#include "FileUtilities/ZipArchiveReader.h"
#include "FileUtilities/ZipArchiveWriter.h"
#include "HAL/PlatformFileManager.h"
#include "Hash/Blake3.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "PluginDescriptor.h"
#include "Serialization/JsonSerializer.h"
#include "Utility/CPM_Log.h"
#include "Windows/WindowsHWrapper.h"

namespace
{
	constexpr int64 MaximumFileBytes = 512ll * 1024 * 1024;
	constexpr int64 MaximumTotalBytes = 16ll * 1024 * 1024 * 1024;
	constexpr int32 MaximumFiles = 50000;
	const FDateTime ArchiveTimestamp(2000, 1, 1);

	bool Fail(FString& Error, const TCHAR* Reason)
	{
		Error = Reason;
		return false;
	}

	bool IsIdentifier(const FString& Value)
	{
		if (Value.IsEmpty() || Value.Len() > 128 || !FChar::IsAlpha(Value[0])) { return false; }
		for (const TCHAR C : Value)
		{
			if (C > 127 || (!FChar::IsAlnum(C) && C != '_')) { return false; }
		}
		return true;
	}

	bool IsIdentity(const FString& Value)
	{
		if (Value.IsEmpty() || Value.Len() > 256) { return false; }
		for (const TCHAR C : Value)
		{
			if (!FChar::IsAlnum(C) && C != '.' && C != '_' && C != '-' && C != '+') { return false; }
		}
		return true;
	}

	bool IsHash(const FString& Hash)
	{
		if (Hash.Len() != 64) { return false; }
		for (const TCHAR C : Hash) { if (!FChar::IsHexDigit(C)) { return false; } }
		return true;
	}

	FString HashBytes(const TArray<uint8>& Bytes)
	{
		return LexToString(FBlake3::HashBuffer(Bytes.GetData(), Bytes.Num()));
	}

	bool IsSegment(const FString& Segment)
	{
		if (Segment.IsEmpty() || Segment == TEXT(".") || Segment == TEXT("..")
			|| Segment.EndsWith(TEXT(".")) || Segment.EndsWith(TEXT(" "))) { return false; }
		for (const TCHAR C : Segment)
		{
			if (C < 32 || C == 127 || FString(TEXT("\\/:*?\"<>|")).Contains(FString::Chr(C))) { return false; }
		}
		FString Base;
		FString Suffix;
		Segment.Split(TEXT("."), &Base, &Suffix);
		if (Base.IsEmpty()) { Base = Segment; }
		Base.ToUpperInline();
		return Base != TEXT("CON") && Base != TEXT("PRN") && Base != TEXT("AUX") && Base != TEXT("NUL")
			&& !(Base.Len() == 4 && (Base.StartsWith(TEXT("COM")) || Base.StartsWith(TEXT("LPT")))
				&& Base[3] >= '1' && Base[3] <= '9');
	}

	bool IsRelativePath(const FString& Path)
	{
		if (Path.IsEmpty() || Path.Len() > 1024 || Path.StartsWith(TEXT("/")) || Path.EndsWith(TEXT("/"))) { return false; }
		TArray<FString> Parts;
		Path.ParseIntoArray(Parts, TEXT("/"), false);
		for (const FString& Part : Parts) { if (!IsSegment(Part)) { return false; } }
		return true;
	}

	bool IsAscii(const FString& Value)
	{
		for (const TCHAR C : Value) { if (C > 127) { return false; } }
		return true;
	}

	bool NormalizeAbsolute(FString& Path)
	{
		FPaths::NormalizeFilename(Path);
		while (Path.Len() > 3 && Path.EndsWith(TEXT("/"))) { Path.LeftChopInline(1); }
		return Path.Len() > 3 && FChar::IsAlpha(Path[0]) && Path[1] == ':' && Path[2] == '/'
			&& IsRelativePath(Path.Mid(3));
	}

	FString NativePath(FString Path)
	{
		Path.ReplaceInline(TEXT("/"), TEXT("\\"));
		return TEXT("\\\\?\\") + Path;
	}

	bool CheckPath(const FString& Path, bool bAllowMissing, FString& Error,
		const FString& AllowedLink = FString(), const FString& AllowedParentLink = FString())
	{
		TArray<FString> Parts;
		Path.Mid(3).ParseIntoArray(Parts, TEXT("/"), false);
		FString Current = Path.Left(3);
		int32 Links = 0;
		for (int32 Index = 0; Index < Parts.Num(); ++Index)
		{
			Current = FPaths::Combine(Current, Parts[Index]);
			const DWORD Attributes = GetFileAttributesW(*NativePath(Current));
			if (Attributes == INVALID_FILE_ATTRIBUTES)
			{
				const DWORD Code = GetLastError();
				if (bAllowMissing && Index == Parts.Num() - 1 && Code == ERROR_FILE_NOT_FOUND) { return true; }
				return Fail(Error, TEXT("A source or output path is missing or inaccessible."));
			}
			if ((Attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
			{
				if ((!Current.Equals(AllowedLink, ESearchCase::IgnoreCase)
					&& !Current.Equals(AllowedParentLink, ESearchCase::IgnoreCase)) || ++Links > 1)
				{
					return Fail(Error, TEXT("Filesystem links are only allowed at the verified selected-avatar root."));
				}
			}
			if (Index < Parts.Num() - 1 && (Attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				return Fail(Error, TEXT("A source or output parent is not a directory."));
			}
		}
		return true;
	}

	bool GetFinalPath(HANDLE Handle, FString& Out)
	{
		const DWORD Length = GetFinalPathNameByHandleW(Handle, nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
		if (Length == 0 || Length > 32768) { return false; }
		TArray<TCHAR> Buffer;
		Buffer.SetNumZeroed(Length + 1);
		const DWORD Written = GetFinalPathNameByHandleW(Handle, Buffer.GetData(), Buffer.Num(), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
		if (Written == 0 || Written >= static_cast<DWORD>(Buffer.Num())) { return false; }
		Out = Buffer.GetData();
		Out.RemoveFromStart(TEXT("\\\\?\\"));
		return NormalizeAbsolute(Out);
	}

	bool CheckContentRoot(const FString& Selected, const FString& Physical, const FString& PluginName, FString& Error)
	{
		const FString Parent = FPaths::GetPath(Selected);
		const FString AllowedParent = FPaths::GetCleanFilename(Parent).Equals(PluginName, ESearchCase::IgnoreCase) ? Parent : FString();
		if (!CheckPath(Physical, false, Error) || !CheckPath(Selected, false, Error, Selected, AllowedParent)) { return false; }
		HANDLE Handle = CreateFileW(*NativePath(Selected), FILE_READ_ATTRIBUTES,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
		if (Handle == INVALID_HANDLE_VALUE) { return Fail(Error, TEXT("Cannot open the selected content root.")); }
		ON_SCOPE_EXIT { CloseHandle(Handle); };
		BY_HANDLE_FILE_INFORMATION Info;
		FString Actual;
		if (!GetFileInformationByHandle(Handle, &Info) || (Info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0
			|| !GetFinalPath(Handle, Actual) || !Actual.Equals(Physical, ESearchCase::IgnoreCase))
		{
			return Fail(Error, TEXT("The selected content root does not resolve to the verified physical avatar content."));
		}
		return true;
	}

	bool ReadFileBytes(const FString& Path, int64 ExpectedSize, int64 MaximumSize, TArray<uint8>& Bytes,
		TFunctionRef<bool()> MayContinue, FString& Error)
	{
		if (!MayContinue() || !CheckPath(Path, false, Error)) { return false; }
		HANDLE Handle = CreateFileW(*NativePath(Path), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
		if (Handle == INVALID_HANDLE_VALUE) { return Fail(Error, TEXT("Cannot read a selected source file.")); }
		ON_SCOPE_EXIT { CloseHandle(Handle); };
		BY_HANDLE_FILE_INFORMATION Before;
		LARGE_INTEGER Size;
		FString Actual;
		if (!GetFileInformationByHandle(Handle, &Before) || !GetFileSizeEx(Handle, &Size)
			|| (Before.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) != 0
			|| Before.nNumberOfLinks != 1 || !GetFinalPath(Handle, Actual) || !Actual.Equals(Path, ESearchCase::IgnoreCase))
		{
			return Fail(Error, TEXT("Source files must be real single-link files inside the verified avatar root."));
		}
		if (Size.QuadPart <= 0 || Size.QuadPart > MaximumSize || (ExpectedSize >= 0 && Size.QuadPart != ExpectedSize))
		{
			return Fail(Error, TEXT("A source file changed size, is empty, or exceeds the supported single-file size limit."));
		}
		Bytes.SetNumUninitialized(static_cast<int32>(Size.QuadPart));
		for (int32 Offset = 0; Offset < Bytes.Num();)
		{
			if (!MayContinue()) { return false; }
			const DWORD Wanted = static_cast<DWORD>(FMath::Min(Bytes.Num() - Offset, 1024 * 1024));
			DWORD Read = 0;
			if (!ReadFile(Handle, Bytes.GetData() + Offset, Wanted, &Read, nullptr) || Read != Wanted)
			{
				return Fail(Error, TEXT("A selected source file could not be read completely."));
			}
			Offset += Read;
		}
		BY_HANDLE_FILE_INFORMATION After;
		if (!GetFileInformationByHandle(Handle, &After) || Before.nFileSizeHigh != After.nFileSizeHigh
			|| Before.nFileSizeLow != After.nFileSizeLow || CompareFileTime(&Before.ftLastWriteTime, &After.ftLastWriteTime) != 0)
		{
			return Fail(Error, TEXT("A selected source file changed during archive preparation."));
		}
		return MayContinue();
	}

	class FArchiveWriteHandle final : public IFileHandle
	{
	public:
		FArchiveWriteHandle(HANDLE InHandle, bool& InFailed) : Handle(InHandle), bFailed(InFailed) {}
		~FArchiveWriteHandle() override { Flush(true); CloseHandle(Handle); }
		int64 Tell() override
		{
			LARGE_INTEGER Distance = {};
			LARGE_INTEGER Position = {};
			return Check(SetFilePointerEx(Handle, Distance, &Position, FILE_CURRENT) != 0) ? Position.QuadPart : -1;
		}
		bool Seek(int64 Position) override { return SeekFrom(Position, FILE_BEGIN); }
		bool SeekFromEnd(int64 Position) override { return SeekFrom(Position, FILE_END); }
		bool Read(uint8*, int64) override { return Check(false); }
		bool ReadAt(uint8*, int64, int64) override { return Check(false); }
		bool Write(const uint8* Bytes, int64 Size) override
		{
			if (Size < 0) { return Check(false); }
			while (Size > 0)
			{
				const DWORD Wanted = static_cast<DWORD>(FMath::Min<int64>(Size, 1024 * 1024));
				DWORD Written = 0;
				if (!Check(WriteFile(Handle, Bytes, Wanted, &Written, nullptr) != 0 && Written > 0)) { return false; }
				Bytes += Written;
				Size -= Written;
			}
			return true;
		}
		bool Flush(bool = false) override { return Check(FlushFileBuffers(Handle) != 0); }
		bool Truncate(int64 Size) override { return Seek(Size) && Check(SetEndOfFile(Handle) != 0); }
	private:
		bool SeekFrom(int64 Position, DWORD Origin)
		{
			LARGE_INTEGER Distance;
			Distance.QuadPart = Position;
			return Check(SetFilePointerEx(Handle, Distance, nullptr, Origin) != 0);
		}
		bool Check(bool bResult) { bFailed |= !bResult; return bResult; }
		HANDLE Handle;
		bool& bFailed;
	};

	TSharedRef<FJsonValue> JsonObjectValue(TSharedRef<FJsonObject> Object)
	{
		return MakeShared<FJsonValueObject>(Object);
	}

	TArray<TSharedPtr<FJsonValue>> JsonStrings(TArray<FString> Strings)
	{
		Strings.Sort();
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const FString& String : Strings) { Values.Add(MakeShared<FJsonValueString>(String)); }
		return Values;
	}

	TArray<uint8> JsonBytes(const TSharedRef<FJsonObject>& Object)
	{
		FString Text;
		FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Text));
		FTCHARToUTF8 Utf8(*Text);
		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		return Bytes;
	}

	struct FArchiveEntry
	{
		FString Path;
		FString Hash;
		int64 Size;
	};
}

FCPM_AvatarSourceArchiveResult FCPM_AvatarSourceArchive::Create(
	const FCPM_AvatarSourceArchiveInput& Input, const FString& OutputDirectory, const FString& ArchiveName,
	TFunctionRef<bool()> IsCancelled, TFunctionRef<bool()> IsSnapshotValid)
{
	FCPM_AvatarSourceArchiveResult Result;
	const auto MayContinue = [&]()
	{
		if (IsCancelled()) { return Fail(Result.Error, TEXT("Source archive creation was cancelled.")); }
		if (!IsSnapshotValid()) { return Fail(Result.Error, TEXT("The monitored source snapshot changed or its validity is unknown.")); }
		return true;
	};
	if (!MayContinue()) { return Result; }
	FString PluginRoot = Input.PhysicalPluginRoot;
	FString SelectedContent = Input.SelectedContentRoot;
	FString OutputRoot = OutputDirectory;
	if (!NormalizeAbsolute(PluginRoot) || !NormalizeAbsolute(SelectedContent) || !NormalizeAbsolute(OutputRoot)
		|| !IsIdentifier(Input.PluginName) || !FPaths::GetCleanFilename(PluginRoot).Equals(Input.PluginName, ESearchCase::CaseSensitive)
		|| !Input.Mount.Equals(TEXT("/") + Input.PluginName + TEXT("/"), ESearchCase::CaseSensitive)
		|| ArchiveName.Len() > 255 || !IsSegment(ArchiveName) || !ArchiveName.EndsWith(TEXT(".zip"), ESearchCase::CaseSensitive)
		|| Input.MaxSingleFileBytes <= 0 || Input.MaxSingleFileBytes > MaximumFileBytes
		|| Input.Files.IsEmpty() || Input.Files.Num() > MaximumFiles || Input.Packages.IsEmpty() || Input.Packages.Num() > MaximumFiles
		|| Input.RequiredPlugins.Num() > 256 || Input.RequiredEngineModules.Num() > 1024
		|| !IsIdentity(Input.EngineVersion) || !IsIdentity(Input.EngineBuild)
		|| !IsIdentity(Input.SdkFingerprint) || !IsIdentity(Input.SdkProfile))
	{
		Result.Error = TEXT("Source archive identities, explicit file list, local roots or size limit are invalid.");
		return Result;
	}
	TArray<FString> EngineParts;
	Input.EngineVersion.ParseIntoArray(EngineParts, TEXT("."), false);
	if (EngineParts.Num() < 2 || EngineParts.Num() > 3)
	{
		Result.Error = TEXT("Source engine version must be an exact major.minor or major.minor.patch version.");
		return Result;
	}
	for (const FString& Part : EngineParts)
	{
		if (Part.IsEmpty()) { Result.Error = TEXT("Source engine version is invalid."); return Result; }
		for (TCHAR C : Part) { if (C < '0' || C > '9') { Result.Error = TEXT("Source engine version is invalid."); return Result; } }
	}
	const FString ContentRoot = FPaths::Combine(PluginRoot, TEXT("Content"));
	const FString OutputPath = FPaths::Combine(OutputRoot, ArchiveName);
	if (!CheckPath(PluginRoot, false, Result.Error) || !CheckContentRoot(SelectedContent, ContentRoot, Input.PluginName, Result.Error)
		|| !CheckPath(OutputRoot, false, Result.Error) || !CheckPath(OutputPath, true, Result.Error)) { return Result; }
	if ((GetFileAttributesW(*NativePath(OutputRoot)) & FILE_ATTRIBUTE_DIRECTORY) == 0
		|| OutputRoot.Equals(PluginRoot, ESearchCase::IgnoreCase) || OutputRoot.StartsWith(PluginRoot + TEXT("/"), ESearchCase::IgnoreCase))
	{
		Result.Error = TEXT("The archive output directory must be outside the selected avatar plugin.");
		return Result;
	}
	HANDLE OutputDirectoryHandle = CreateFileW(*NativePath(OutputRoot), FILE_READ_ATTRIBUTES, FILE_SHARE_READ,
		nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
	if (OutputDirectoryHandle == INVALID_HANDLE_VALUE)
	{
		Result.Error = TEXT("The owned archive output directory is busy or cannot be protected from replacement."); return Result;
	}
	ON_SCOPE_EXIT { CloseHandle(OutputDirectoryHandle); };
	FString ActualOutputRoot;
	if (!GetFinalPath(OutputDirectoryHandle, ActualOutputRoot) || !ActualOutputRoot.Equals(OutputRoot, ESearchCase::IgnoreCase))
	{
		Result.Error = TEXT("The archive output directory changed during validation."); return Result;
	}

	TSet<FString> Prerequisites;
	TSet<FString> NativeModules;
	for (const FString& Module : Input.RequiredEngineModules)
	{
		if (!IsIdentifier(Module) || NativeModules.Contains(Module.ToLower()))
		{
			Result.Error = TEXT("Engine module declarations must be explicit and unique."); return Result;
		}
		NativeModules.Add(Module.ToLower());
	}
	TArray<FCPM_AvatarSourcePrerequisite> Required = Input.RequiredPlugins;
	Required.Sort([](const auto& A, const auto& B) { return A.PluginName < B.PluginName; });
	for (const auto& Plugin : Required)
	{
		if (!IsIdentifier(Plugin.PluginName) || Plugin.PluginName.Equals(Input.PluginName, ESearchCase::IgnoreCase)
			|| !IsIdentity(Plugin.Version) || !IsIdentity(Plugin.Fingerprint) || Prerequisites.Contains(Plugin.PluginName.ToLower())
			|| Plugin.Modules.Num() > 256)
		{
			Result.Error = TEXT("Required plugin identities must be explicit and unique; dependency code is never bundled.");
			return Result;
		}
		Prerequisites.Add(Plugin.PluginName.ToLower());
		for (const FString& Module : Plugin.Modules)
		{
			if (!IsIdentifier(Module)) { Result.Error = TEXT("Required native module identity is invalid."); return Result; }
			NativeModules.Add(Module.ToLower());
		}
	}
	TMap<FString, const FCPM_AvatarSourcePackage*> Packages;
	for (const auto& Package : Input.Packages)
	{
		if (!Package.Name.StartsWith(Input.Mount, ESearchCase::CaseSensitive)
			|| Package.Name.Len() > 1024 || Package.Dependencies.Num() > 4096
			|| !FPackageName::IsValidTextForLongPackageName(Package.Name) || Packages.Contains(Package.Name.ToLower()))
		{
			Result.Error = TEXT("Package declarations must be unique and belong to the selected stable mount.");
			return Result;
		}
		Packages.Add(Package.Name.ToLower(), &Package);
	}
	FString BlueprintPackage;
	FString BlueprintObject;
	if (!Input.Blueprint.Split(TEXT("."), &BlueprintPackage, &BlueprintObject) || !Packages.Contains(BlueprintPackage.ToLower())
		|| !Packages[BlueprintPackage.ToLower()]->Name.Equals(BlueprintPackage, ESearchCase::CaseSensitive)
		|| !BlueprintObject.Equals(FPackageName::GetShortName(BlueprintPackage), ESearchCase::CaseSensitive))
	{
		Result.Error = TEXT("The source Blueprint must name an explicit package and its top-level object in the selected mount.");
		return Result;
	}
	int64 DependencyTextSize = 0;
	for (const auto& Package : Input.Packages)
	{
		TSet<FString> Seen;
		for (const FString& Dependency : Package.Dependencies)
		{
			DependencyTextSize += Dependency.Len();
			if (Dependency.Len() > 1024 || DependencyTextSize > 16 * 1024 * 1024
				|| !FPackageName::IsValidTextForLongPackageName(Dependency) || Seen.Contains(Dependency.ToLower()))
			{
				Result.Error = TEXT("Package dependencies must be valid and unique."); return Result;
			}
			Seen.Add(Dependency.ToLower());
			FString MountName;
			FString Rest;
			Dependency.Mid(1).Split(TEXT("/"), &MountName, &Rest);
			const auto* SelectedPackage = Packages.Find(Dependency.ToLower());
			const bool bSelected = SelectedPackage && (*SelectedPackage)->Name.Equals(Dependency, ESearchCase::CaseSensitive);
			const bool bNative = MountName.Equals(TEXT("Script"), ESearchCase::CaseSensitive) && NativeModules.Contains(Rest.ToLower());
			if (!bSelected && !bNative && !MountName.Equals(TEXT("Engine"), ESearchCase::CaseSensitive) && !Prerequisites.Contains(MountName.ToLower()))
			{
				Result.Error = TEXT("A dependency is outside the selected closure and declared engine/plugin prerequisites."); return Result;
			}
		}
	}

	TSet<FString> Names;
	TSet<FString> PrimaryPackages;
	TMap<FString, FString> PrimaryHashes;
	TArray<FCPM_AvatarSourceFile> Files = Input.Files;
	Files.Sort([](const auto& A, const auto& B) { return A.ContentRelativePath < B.ContentRelativePath; });
	int64 TotalBytes = 0;
	for (const auto& File : Files)
	{
		if (!IsAscii(File.ContentRelativePath))
		{
			Result.Error = TEXT("The supported source ZIP reader requires ASCII content filenames; no archive was written."); return Result;
		}
		const FString Extension = FPaths::GetExtension(File.ContentRelativePath, false);
		const FString PackageName = Input.Mount + FPaths::ChangeExtension(File.ContentRelativePath, TEXT(""));
		const bool bPrimary = Extension.Equals(TEXT("uasset"), ESearchCase::CaseSensitive) || Extension.Equals(TEXT("umap"), ESearchCase::CaseSensitive);
		if (!IsRelativePath(File.ContentRelativePath) || (!bPrimary && !Extension.Equals(TEXT("uexp"), ESearchCase::CaseSensitive)
			&& !Extension.Equals(TEXT("ubulk"), ESearchCase::CaseSensitive) && !Extension.Equals(TEXT("uptnl"), ESearchCase::CaseSensitive))
			|| File.Size <= 0 || File.Size > Input.MaxSingleFileBytes || !IsHash(File.Blake3)
			|| Names.Contains(File.ContentRelativePath.ToLower()) || !Packages.Contains(PackageName.ToLower())
			|| !Packages[PackageName.ToLower()]->Name.Equals(PackageName, ESearchCase::CaseSensitive) || (bPrimary && PrimaryPackages.Contains(PackageName.ToLower()))
			|| TotalBytes > MaximumTotalBytes - File.Size)
		{
			Result.Error = TEXT("Only unique declared content package files within the supported size limits may be archived.");
			return Result;
		}
		Names.Add(File.ContentRelativePath.ToLower());
		if (bPrimary)
		{
			PrimaryPackages.Add(PackageName.ToLower());
			PrimaryHashes.Add(PackageName.ToLower(), File.Blake3.ToLower());
		}
		TotalBytes += File.Size;
	}
	if (PrimaryPackages.Num() != Packages.Num())
	{
		Result.Error = TEXT("Every declared package requires exactly one primary source file."); return Result;
	}
	TArray<uint8> DescriptorBytes;
	const FString DescriptorPath = FPaths::Combine(PluginRoot, Input.PluginName + TEXT(".uplugin"));
	if (!ReadFileBytes(DescriptorPath, -1, 256 * 1024, DescriptorBytes, MayContinue, Result.Error)) { return Result; }
	const FString DescriptorHash = HashBytes(DescriptorBytes);
	FUTF8ToTCHAR DescriptorText(reinterpret_cast<const ANSICHAR*>(DescriptorBytes.GetData()), DescriptorBytes.Num());
	FPluginDescriptor Descriptor;
	FText DescriptorError;
	if (!Descriptor.Read(FString(DescriptorText.Length(), DescriptorText.Get()), DescriptorError) || !Descriptor.bCanContainContent
		|| !Descriptor.bNoCode || !Descriptor.Modules.IsEmpty() || !Descriptor.PreBuildSteps.HostPlatformToCommands.IsEmpty()
		|| !Descriptor.PostBuildSteps.HostPlatformToCommands.IsEmpty())
	{
		Result.Error = TEXT("Automatic source archives support verified content-only avatar plugins without build steps."); return Result;
	}
	for (const FPluginReferenceDescriptor& Plugin : Descriptor.Plugins)
	{
		if (Plugin.bEnabled && !Prerequisites.Contains(Plugin.Name.ToLower()))
		{
			Result.Error = TEXT("An enabled source plugin prerequisite is missing from the verified declaration."); return Result;
		}
	}

	TArray<TSharedPtr<FJsonValue>> DependencyJson;
	TArray<TSharedPtr<FJsonValue>> EnabledPlugins;
	for (const auto& Plugin : Required)
	{
		auto Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("name"), Plugin.PluginName);
		Item->SetStringField(TEXT("version"), Plugin.Version);
		Item->SetStringField(TEXT("fingerprint"), Plugin.Fingerprint);
		Item->SetBoolField(TEXT("bundled"), false);
		Item->SetArrayField(TEXT("modules"), JsonStrings(Plugin.Modules));
		DependencyJson.Add(JsonObjectValue(Item));
		auto Enabled = MakeShared<FJsonObject>();
		Enabled->SetStringField(TEXT("Name"), Plugin.PluginName);
		Enabled->SetBoolField(TEXT("Enabled"), true);
		EnabledPlugins.Add(JsonObjectValue(Enabled));
	}
	auto PluginJson = MakeShared<FJsonObject>();
	PluginJson->SetNumberField(TEXT("FileVersion"), 3);
	PluginJson->SetNumberField(TEXT("Version"), 1);
	PluginJson->SetStringField(TEXT("FriendlyName"), Input.PluginName);
	PluginJson->SetBoolField(TEXT("CanContainContent"), true);
	PluginJson->SetBoolField(TEXT("NoCode"), true);
	PluginJson->SetBoolField(TEXT("EnabledByDefault"), false);
	PluginJson->SetArrayField(TEXT("Plugins"), EnabledPlugins);
	auto AvatarEnabled = MakeShared<FJsonObject>();
	AvatarEnabled->SetStringField(TEXT("Name"), Input.PluginName);
	AvatarEnabled->SetBoolField(TEXT("Enabled"), true);
	EnabledPlugins.Add(JsonObjectValue(AvatarEnabled));
	auto ProjectJson = MakeShared<FJsonObject>();
	ProjectJson->SetNumberField(TEXT("FileVersion"), 3);
	ProjectJson->SetStringField(TEXT("EngineAssociation"), EngineParts[0] + TEXT(".") + EngineParts[1]);
	ProjectJson->SetBoolField(TEXT("DisableEnginePluginsByDefault"), true);
	ProjectJson->SetArrayField(TEXT("Plugins"), EnabledPlugins);
	auto Manifest = MakeShared<FJsonObject>();
	Manifest->SetNumberField(TEXT("formatVersion"), FormatVersion);
	Manifest->SetStringField(TEXT("hashAlgorithm"), TEXT("blake3"));
	auto Identity = MakeShared<FJsonObject>();
	Identity->SetStringField(TEXT("name"), Input.PluginName);
	Identity->SetStringField(TEXT("mount"), Input.Mount);
	Identity->SetStringField(TEXT("blueprint"), Input.Blueprint);
	Manifest->SetObjectField(TEXT("plugin"), Identity);
	auto Engine = MakeShared<FJsonObject>();
	Engine->SetStringField(TEXT("version"), Input.EngineVersion);
	Engine->SetStringField(TEXT("build"), Input.EngineBuild);
	Engine->SetArrayField(TEXT("modules"), JsonStrings(Input.RequiredEngineModules));
	Manifest->SetObjectField(TEXT("engine"), Engine);
	auto Sdk = MakeShared<FJsonObject>();
	Sdk->SetStringField(TEXT("fingerprint"), Input.SdkFingerprint);
	Sdk->SetStringField(TEXT("profile"), Input.SdkProfile);
	Manifest->SetObjectField(TEXT("sdk"), Sdk);
	Manifest->SetArrayField(TEXT("requiredPlugins"), DependencyJson);
	TArray<TSharedPtr<FJsonValue>> PackageJson;
	TArray<FString> PackageNames;
	Packages.GenerateKeyArray(PackageNames);
	PackageNames.Sort();
	for (const FString& Name : PackageNames)
	{
		auto Package = MakeShared<FJsonObject>();
		Package->SetStringField(TEXT("name"), Packages[Name]->Name);
		Package->SetStringField(TEXT("primaryFileBlake3"), PrimaryHashes[Name]);
		Package->SetArrayField(TEXT("dependencies"), JsonStrings(Packages[Name]->Dependencies));
		PackageJson.Add(JsonObjectValue(Package));
	}
	Manifest->SetArrayField(TEXT("packages"), PackageJson);

	const FString Temporary = FPaths::Combine(OutputRoot, TEXT(".avatar-source-") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".tmp"));
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	HANDLE Output = CreateFileW(*NativePath(Temporary), GENERIC_READ | GENERIC_WRITE | DELETE, FILE_SHARE_READ,
		nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (Output == INVALID_HANDLE_VALUE) { Result.Error = TEXT("Cannot exclusively create the temporary source archive."); return Result; }
	bool bPromoted = false;
	ON_SCOPE_EXIT
	{
		if (!bPromoted)
		{
			FILE_DISPOSITION_INFO DeleteInfo{true};
			if (!SetFileInformationByHandle(Output, FileDispositionInfo, &DeleteInfo, sizeof(DeleteInfo)))
			{
				CPM_LOG(Warning, TEXT("Could not remove the owned incomplete source archive; no pathname cleanup was attempted."));
			}
		}
		CloseHandle(Output);
	};
	HANDLE WriterHandle = nullptr;
	if (!DuplicateHandle(GetCurrentProcess(), Output, GetCurrentProcess(), &WriterHandle, 0, false, DUPLICATE_SAME_ACCESS))
	{
		Result.Error = TEXT("Cannot create the owned source ZIP writer handle."); return Result;
	}
	bool bWriteFailed = false;
	FString ManifestHash;
	TArray<FArchiveEntry> Entries;
	{
		FZipArchiveWriter Writer(new FArchiveWriteHandle(WriterHandle, bWriteFailed));
		const auto Add = [&](const FString& Path, const TArray<uint8>& Bytes)
		{
			Entries.Add({ Path, HashBytes(Bytes), Bytes.Num() });
			Writer.AddFile(Path, Bytes, ArchiveTimestamp);
		};
		Add(TEXT("AvatarSource.uproject"), JsonBytes(ProjectJson));
		const FString PluginPrefix = TEXT("Plugins/") + Input.PluginName + TEXT("/");
		Add(PluginPrefix + Input.PluginName + TEXT(".uplugin"), JsonBytes(PluginJson));
		for (const auto& File : Files)
		{
			TArray<uint8> Bytes;
			if (!ReadFileBytes(FPaths::Combine(ContentRoot, File.ContentRelativePath), File.Size, Input.MaxSingleFileBytes, Bytes, MayContinue, Result.Error)) { return Result; }
			if (!HashBytes(Bytes).Equals(File.Blake3, ESearchCase::IgnoreCase))
			{
				Result.Error = TEXT("A selected source file no longer matches the validated preparation snapshot."); return Result;
			}
			Add(PluginPrefix + TEXT("Content/") + File.ContentRelativePath, Bytes);
			if (bWriteFailed) { Result.Error = TEXT("Writing the temporary source archive failed."); return Result; }
		}
		TArray<TSharedPtr<FJsonValue>> FileJson;
		for (const auto& Entry : Entries)
		{
			auto File = MakeShared<FJsonObject>();
			File->SetStringField(TEXT("path"), Entry.Path);
			File->SetNumberField(TEXT("size"), static_cast<double>(Entry.Size));
			File->SetStringField(TEXT("blake3"), Entry.Hash);
			FileJson.Add(JsonObjectValue(File));
		}
		Manifest->SetArrayField(TEXT("files"), FileJson);
		const TArray<uint8> Bytes = JsonBytes(Manifest);
		ManifestHash = HashBytes(Bytes);
		Add(ManifestFilename, Bytes);
	}
	if (bWriteFailed) { Result.Error = TEXT("Writing or flushing the completed source archive failed."); return Result; }
	BY_HANDLE_FILE_INFORMATION ArchiveInfo;
	FString ActualTemporary;
	if (!GetFileInformationByHandle(Output, &ArchiveInfo) || ArchiveInfo.nNumberOfLinks != 1
		|| (ArchiveInfo.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0
		|| !GetFinalPath(Output, ActualTemporary) || !ActualTemporary.Equals(Temporary, ESearchCase::IgnoreCase))
	{
		Result.Error = TEXT("The temporary source archive changed before validation."); return Result;
	}
	{
		auto ReadHandle = PlatformFile.OpenRead(*Temporary, IPlatformFile::EOpenReadFlags::AllowWrite | IPlatformFile::EOpenReadFlags::AllowDelete);
		if (ReadHandle.HasError()) { Result.Error = TEXT("Cannot read the completed source archive."); return Result; }
		FZipArchiveReader Reader(ReadHandle.StealValue().Release());
		if (!Reader.IsValid() || Reader.GetFileNames().Num() != Entries.Num())
		{
			Result.Error = TEXT("The completed source archive failed structural validation."); return Result;
		}
		for (const auto& Entry : Entries)
		{
			TArray<uint8> Bytes;
			if (!MayContinue()) { return Result; }
			if (!Reader.TryReadFile(Entry.Path, Bytes) || Bytes.Num() != Entry.Size || HashBytes(Bytes) != Entry.Hash)
			{
				Result.Error = TEXT("The completed source archive failed its byte/hash validation."); return Result;
			}
		}
	}
	for (const auto& File : Files)
	{
		TArray<uint8> Bytes;
		if (!ReadFileBytes(FPaths::Combine(ContentRoot, File.ContentRelativePath), File.Size, Input.MaxSingleFileBytes, Bytes, MayContinue, Result.Error)) { return Result; }
		if (!HashBytes(Bytes).Equals(File.Blake3, ESearchCase::IgnoreCase))
		{
			Result.Error = TEXT("A selected source file changed before archive commit."); return Result;
		}
	}
	if (!ReadFileBytes(DescriptorPath, DescriptorBytes.Num(), 256 * 1024, DescriptorBytes, MayContinue, Result.Error)
		|| HashBytes(DescriptorBytes) != DescriptorHash
		|| !CheckContentRoot(SelectedContent, ContentRoot, Input.PluginName, Result.Error)
		|| !CheckPath(OutputRoot, false, Result.Error) || !CheckPath(OutputPath, true, Result.Error)
		|| !GetFinalPath(OutputDirectoryHandle, ActualOutputRoot) || !ActualOutputRoot.Equals(OutputRoot, ESearchCase::IgnoreCase)
		|| !MayContinue())
	{
		if (Result.Error.IsEmpty()) { Result.Error = TEXT("The source descriptor changed before archive commit."); }
		return Result;
	}
	const FString RenamePath = NativePath(OutputPath);
	TArray<uint8> RenameBytes;
	RenameBytes.SetNumZeroed(STRUCT_OFFSET(FILE_RENAME_INFO, FileName) + (RenamePath.Len() + 1) * sizeof(TCHAR));
	FILE_RENAME_INFO* Rename = reinterpret_cast<FILE_RENAME_INFO*>(RenameBytes.GetData());
	Rename->ReplaceIfExists = true;
	Rename->RootDirectory = nullptr;
	Rename->FileNameLength = RenamePath.Len() * sizeof(TCHAR);
	FMemory::Memcpy(Rename->FileName, *RenamePath, Rename->FileNameLength);
	if (!SetFileInformationByHandle(Output, FileRenameInfo, Rename, RenameBytes.Num()))
	{
		const DWORD Code = GetLastError();
		Result.Error = FString::Printf(TEXT("Cannot atomically install the validated source archive (Windows error %lu); the previous archive was preserved."), Code);
		return Result;
	}
	bPromoted = true;
	Result.bSuccess = true;
	Result.ArchivePath = OutputPath;
	Result.ManifestBlake3 = ManifestHash;
	Result.ContentFileCount = Files.Num();
	return Result;
}
