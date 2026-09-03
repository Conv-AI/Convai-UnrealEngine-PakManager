// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "CPM_Compatibility.generated.h"

/**
 * What the Pak Manager knows about whether this install is the one Convai targets.
 *
 * Fail open throughout: an unread version leaves both flags false, because "we could not ask" must
 * never render as "your engine is wrong" - that sends a creator to reinstall an engine that was
 * fine. Unknown never reads as a mismatch.
 */
USTRUCT(BlueprintType)
struct CONVAIPAKMANAGER_API FCPM_CompatibilityStatus
{
	GENERATED_BODY()

	/** Whether anything has answered this session. False means unknown, never "compatible". */
	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	bool bChecked = false;

	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	FString InstalledToolVersion;

	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	FString LatestToolVersion;

	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	bool bToolOutdated = false;

	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	FString EngineVersion;

	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	FString TargetEngineVersion;

	UPROPERTY(BlueprintReadOnly, Category = "Convai|PakManager")
	bool bEngineMismatch = false;
};

namespace ConvaiPakManager::Compatibility
{
/** Where the two pins are read from. Constants, not settings - values that never change. */
inline const TCHAR* ToolRepository = TEXT("Conv-AI/Convai-UnrealEngine-PakManager");

// ponytail: reads main's .uplugin rather than the releases API, so a creator may be told about a
// version the Modding Tool cannot install yet if main runs ahead; upgrade path is a JSON proxy
// against api.github.com/repos/<repo>/releases/latest.
inline const TCHAR* ToolVersionFile = TEXT("ConvaiPakManager.uplugin");

inline const TCHAR* ModdingToolRepository = TEXT("Conv-AI/Convai-UnrealEngine-ModdingTool");
inline const TCHAR* TargetEngineFile = TEXT("Version.json");
inline const TCHAR* SourceRef = TEXT("main");

/** The "VersionName" a `.uplugin` declares, or empty if it declares none or is not JSON. */
CONVAIPAKMANAGER_API FString ParsePluginVersionName(const FString& UpluginJson);

/** The "target-ue-version" the Modding Tool declares, or empty if it declares none. */
CONVAIPAKMANAGER_API FString ParseTargetEngineVersion(const FString& VersionJson);

/**
 * Whether Latest is a later version than Installed.
 *
 * False for anything either side cannot parse, so an unreadable pin says "up to date" rather than
 * nagging a creator about an upgrade nobody can name.
 */
CONVAIPAKMANAGER_API bool IsNewerVersion(const FString& Installed, const FString& Latest);

/**
 * Whether the running engine is the one the Modding Tool targets, compared on Major.Minor.
 *
 * Patch is not compared: a Pak cooked by 5.8.1 loads in the 5.8 the Modding Tool ships, and
 * warning on a hotfix would warn everybody. An empty or unreadable target matches everything.
 */
CONVAIPAKMANAGER_API bool EngineMatchesTarget(const FString& Engine, const FString& Target);

/** This install's version, from the plugin descriptor. Empty if the plugin is not found. */
CONVAIPAKMANAGER_API FString InstalledToolVersion();
}
