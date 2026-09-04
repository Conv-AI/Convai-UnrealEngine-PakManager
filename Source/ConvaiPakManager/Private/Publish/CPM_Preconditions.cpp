// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Publish/CPM_Preconditions.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ConvaiPakManager::Preconditions
{
namespace
{
	/**
	 * The names UnrealBuildTool uses, spelled here because none of them is reachable from C++.
	 * Engine/Source/Programs/UnrealBuildTool/Platform/Linux/LinuxPlatformSDK.cs is the original.
	 */
	const TCHAR* MultiArchRootVar = TEXT("LINUX_MULTIARCH_ROOT");
	const TCHAR* AutoSdkRootVar = TEXT("UE_SDKS_ROOT");
	const TCHAR* TargetPlatformName = TEXT("Linux_x64");
	const TCHAR* VersionFileName = TEXT("ToolchainVersion.txt");
	const TCHAR* SdkConfigFile = TEXT("Config/Linux/Linux_SDK.json");

	/** "HostWin64" - the shape GetInTreeSDKRoot builds from the host it is running on. */
	FString HostDirectoryName()
	{
		return FString(TEXT("Host")) + FPlatformProcess::GetBinariesSubdirectory();
	}

	FString FirstLineOf(const FString& FilePath)
	{
		FString Contents;
		if (!FFileHelper::LoadFileToString(Contents, *FilePath))
		{
			return FString();
		}

		int32 Break = INDEX_NONE;
		const FString First = Contents.FindChar(TEXT('\n'), Break) ? Contents.Left(Break) : Contents;
		return First.TrimStartAndEnd();
	}

	void ReadAcceptedRange(FString& OutMain, FString& OutMin, FString& OutMax)
	{
		FString Contents;
		if (!FFileHelper::LoadFileToString(Contents, *(FPaths::EngineDir() / SdkConfigFile)))
		{
			return;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Contents);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return;
		}

		Root->TryGetStringField(TEXT("MainVersion"), OutMain);
		Root->TryGetStringField(TEXT("MinVersion"), OutMin);
		Root->TryGetStringField(TEXT("MaxVersion"), OutMax);
	}
}

int32 ToolchainOrdinal(const FString& ToolchainName)
{
	const FString Trimmed = ToolchainName.TrimStartAndEnd();
	if (Trimmed.Len() < 2 || (Trimmed[0] != TEXT('v') && Trimmed[0] != TEXT('V')))
	{
		return INDEX_NONE;
	}

	int32 End = 1;
	while (End < Trimmed.Len() && FChar::IsDigit(Trimmed[End]))
	{
		++End;
	}
	if (End == 1)
	{
		return INDEX_NONE;
	}

	return FCString::Atoi(*Trimmed.Mid(1, End - 1));
}

bool ToolchainWithinRange(const FString& Found, const FString& Min, const FString& Max)
{
	const int32 Have = ToolchainOrdinal(Found);
	if (Have == INDEX_NONE)
	{
		return false;
	}

	const int32 Low = ToolchainOrdinal(Min);
	const int32 High = ToolchainOrdinal(Max);
	return (Low == INDEX_NONE || Have >= Low) && (High == INDEX_NONE || Have <= High);
}

FLinuxToolchain InspectLinuxToolchain()
{
	FLinuxToolchain Out;

	FString Min;
	FString Max;
	ReadAcceptedRange(Out.ExpectedVersion, Min, Max);

	// The order is UnrealBuildTool's, and the first branch is deliberately not a "try the next one".
	// GetSDKLocation returns LINUX_MULTIARCH_ROOT whenever it is set and never looks further, so a
	// variable pointing somewhere useless is a build that fails - and reporting that is the job.
	FString SdkDirectory = FPlatformMisc::GetEnvironmentVariable(MultiArchRootVar).TrimStartAndEnd();
	if (SdkDirectory.IsEmpty() && !Out.ExpectedVersion.IsEmpty())
	{
		const FString InTree = FPaths::EngineDir() / TEXT("Extras/ThirdPartyNotUE/SDKs")
			/ HostDirectoryName() / TargetPlatformName / Out.ExpectedVersion;
		if (IFileManager::Get().DirectoryExists(*InTree))
		{
			SdkDirectory = InTree;
		}
	}
	if (SdkDirectory.IsEmpty() && !Out.ExpectedVersion.IsEmpty())
	{
		// Not something GetSDKLocation consults: an AutoSDK is activated earlier, by machinery that
		// sets LINUX_MULTIARCH_ROOT itself from OutputEnvVars.txt, which cannot be modelled from
		// here. Looked at anyway, because refusing a creator whose AutoSDK will activate fine is the
		// worse of the two mistakes this check can make.
		const FString AutoSdkRoot = FPlatformMisc::GetEnvironmentVariable(AutoSdkRootVar).TrimStartAndEnd();
		const FString AutoSdk = AutoSdkRoot.IsEmpty()
			? FString()
			: AutoSdkRoot / HostDirectoryName() / TargetPlatformName / Out.ExpectedVersion;
		if (!AutoSdk.IsEmpty() && IFileManager::Get().DirectoryExists(*AutoSdk))
		{
			SdkDirectory = AutoSdk;
		}
	}

	if (SdkDirectory.IsEmpty())
	{
		return Out;
	}

	Out.FoundAt = SdkDirectory;
	// The directory name is NOT the version. UnrealBuildTool reads this file and so does this, which
	// is the whole difference between checking a toolchain and checking a folder's spelling.
	Out.FoundVersion = FirstLineOf(SdkDirectory / VersionFileName);
	Out.bUsable = ToolchainWithinRange(Out.FoundVersion, Min, Max);
	return Out;
}

FString WhyLinuxCannotPackage(const FLinuxToolchain& Toolchain)
{
	if (Toolchain.bUsable)
	{
		return FString();
	}

	const FString Wanted = Toolchain.ExpectedVersion.IsEmpty()
		? FString(TEXT("the Linux cross-compile toolchain"))
		: FString::Printf(TEXT("the Linux cross-compile toolchain %s"), *Toolchain.ExpectedVersion);

	// Every one of these ends the same way. A creator who installs the toolchain into a running
	// editor still has a stale environment block, and packaging inherits this process's - so the
	// restart is not boilerplate, it is the step without which the next attempt fails identically.
	if (Toolchain.FoundAt.IsEmpty())
	{
		return FString::Printf(
			TEXT("this publish packages Linux, but %s is not installed. Install it with the Convai ")
			TEXT("Modding Tool, then restart the editor."), *Wanted);
	}

	if (Toolchain.FoundVersion.IsEmpty())
	{
		return FString::Printf(
			TEXT("this publish packages Linux, but %s holds no readable %s, so the engine cannot use ")
			TEXT("it. Install %s with the Convai Modding Tool, then restart the editor."),
			*Toolchain.FoundAt, VersionFileName, *Wanted);
	}

	return FString::Printf(
		TEXT("this publish packages Linux, but %s holds %s and this engine needs %s. Install it with ")
		TEXT("the Convai Modding Tool, then restart the editor."),
		*Toolchain.FoundAt, *Toolchain.FoundVersion, *Wanted);
}

FString WhyAssetRecordCannotBeWritten(
	const ECPM_AssetType AssetType, const int32 SpawnPointCount, const bool bHasNavMeshBounds)
{
	TArray<FString> Missing;

	if (SpawnPointCount <= 0)
	{
		Missing.Add(TEXT("this level has no spawn point, so a Convai product has nowhere to put the ")
			TEXT("player - use Set from viewport to place one"));
	}

	if (AssetType == ECPM_AssetType::Scene && !bHasNavMeshBounds)
	{
		Missing.Add(TEXT("a Scene needs a Nav Mesh Bounds Volume before characters can walk in it - ")
			TEXT("use Add nav mesh bounds to place one"));
	}

	return FString::Join(Missing, TEXT("; and "));
}
}
