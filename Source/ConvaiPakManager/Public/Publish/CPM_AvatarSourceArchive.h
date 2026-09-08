// Copyright Convai. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FCPM_AvatarSourceFile
{
	FString ContentRelativePath;
	FString Blake3;
	int64 Size = 0;
};

struct FCPM_AvatarSourcePackage
{
	FString Name;
	TArray<FString> Dependencies;
};

struct FCPM_AvatarSourcePrerequisite
{
	FString PluginName;
	FString Version;
	FString Fingerprint;
	TArray<FString> Modules;
};

struct FCPM_AvatarSourceArchiveInput
{
	FString PhysicalPluginRoot;
	FString SelectedContentRoot;
	FString PluginName;
	FString Mount;
	FString Blueprint;
	FString EngineVersion;
	FString EngineBuild;
	TArray<FString> RequiredEngineModules = { TEXT("CoreUObject"), TEXT("Engine") };
	FString SdkFingerprint;
	FString SdkProfile;
	TArray<FCPM_AvatarSourceFile> Files;
	TArray<FCPM_AvatarSourcePackage> Packages;
	TArray<FCPM_AvatarSourcePrerequisite> RequiredPlugins;
	int64 MaxSingleFileBytes = 256ll * 1024 * 1024;
};

struct FCPM_AvatarSourceArchiveResult
{
	bool bSuccess = false;
	FString Error;
	FString ArchivePath;
	FString ManifestBlake3;
	int32 ContentFileCount = 0;
};

/** Content-only source export. Does not discover a closure, license dependencies or monitor authored inputs. */
class CONVAIPAKMANAGER_API FCPM_AvatarSourceArchive
{
public:
	static constexpr int32 FormatVersion = 1;
	static constexpr const TCHAR* ManifestFilename = TEXT("avatar-source-manifest.json");

	/**
	 * Synchronous file I/O; call off the game thread. Files and hashes must come from a validated
	 * preparation snapshot. IsSnapshotValid must observe the caller's monotonic input monitor;
	 * before/after hashes alone do not establish a coherent authoring/cook snapshot.
	 */
	static FCPM_AvatarSourceArchiveResult Create(
		const FCPM_AvatarSourceArchiveInput& Input,
		const FString& OutputDirectory,
		const FString& ArchiveName,
		TFunctionRef<bool()> IsCancelled,
		TFunctionRef<bool()> IsSnapshotValid);
};
