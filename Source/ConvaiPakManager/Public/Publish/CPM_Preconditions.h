// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Utility/CPM_Utils.h"

/**
 * What must already be true before a Publish may start. See CONTEXT.md for the concept and
 * docs/adr/0014 for why these refuse where the compatibility banner only warns.
 *
 * Everything that DECIDES here is pure and is handed the facts, so the deciding is testable without
 * a level, a toolchain or a network. Reading those facts off the host is the only part that touches
 * the world, and it is one function.
 */
namespace ConvaiPakManager::Preconditions
{
/** What this host holds for Linux cross-compilation, read the way UnrealBuildTool reads it. */
struct CONVAIPAKMANAGER_API FLinuxToolchain
{
	/** The SDK directory that resolved, or empty when none did. */
	FString FoundAt;

	/** ToolchainVersion.txt's first line, or empty when that directory holds no readable one. */
	FString FoundVersion;

	/** What this engine asks for, from its own Linux_SDK.json. Empty when that file is unreadable. */
	FString ExpectedVersion;

	/** Whether a Linux Pak can be cooked on this machine. */
	bool bUsable = false;
};

/**
 * The ordinal a toolchain name opens with - 26 for "v26_clang-20.1.8-rockylinux8".
 *
 * INDEX_NONE for anything not in that shape. UnrealBuildTool treats such a name as unusable rather
 * than as version zero, and so does everything here.
 */
CONVAIPAKMANAGER_API int32 ToolchainOrdinal(const FString& ToolchainName);

/**
 * Whether Found sits inside the range this engine accepts.
 *
 * A bound this cannot read is not applied, so a range that is entirely unreadable accepts any Found
 * that IS readable: the engine's own config file moving in a future version is not grounds for
 * refusing a creator whose toolchain is sitting right there. A Found it cannot read is refused,
 * because that is the case UnrealBuildTool also refuses.
 */
CONVAIPAKMANAGER_API bool ToolchainWithinRange(const FString& Found, const FString& Min, const FString& Max);

/** Reads the host. The .cpp records which places UnrealBuildTool looks in, and in what order. */
CONVAIPAKMANAGER_API FLinuxToolchain InspectLinuxToolchain();

/** Why this host cannot package Linux, or empty when it can. */
CONVAIPAKMANAGER_API FString WhyLinuxCannotPackage(const FLinuxToolchain& Toolchain);

/**
 * Why this project cannot have an Asset record created or updated from it, or empty when it can.
 *
 * Names EVERY missing thing rather than the first, so a creator fixes one level once instead of
 * discovering the next requirement on the next click.
 */
CONVAIPAKMANAGER_API FString WhyAssetRecordCannotBeWritten(
	ECPM_AssetType AssetType, int32 SpawnPointCount, bool bHasNavMeshBounds);
}
