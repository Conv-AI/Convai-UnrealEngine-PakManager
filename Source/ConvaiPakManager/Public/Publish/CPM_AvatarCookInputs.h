// Copyright Convai. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

struct FCPM_AvatarCookInput
{
	FString HostDirectory;
	FString ChildProjectFile;
	FGuid JobId;
	FString SelectedMount;
	TArray<FString> SelectedPackages;
	TArray<FString> PrerequisitePackages;
	int32 ChunkId = INDEX_NONE;
};

struct FCPM_AvatarCookInputResult
{
	bool bSuccess = false;
	FString Error;
	FString LabelObjectPath;
	FString LabelFilename;
	int32 ExplicitObjectCount = 0;
};

/** Saves only a fresh child-owned cook label; never invokes V0's broad label refresh. */
class CONVAIPAKMANAGER_API FCPM_AvatarCookInputs
{
public:
	/** Caller must retain the child lease and validated source snapshot through the subsequent cook. */
	static FCPM_AvatarCookInputResult Prepare(const FCPM_AvatarCookInput& Input,
		TFunctionRef<bool()> IsCancelled, TFunctionRef<bool()> IsSnapshotValid);

private:
	static FCPM_AvatarCookInputResult WriteLabel(const FString& LabelPackage,
		const TArray<FSoftObjectPath>& Assets, const TArray<FSoftObjectPath>& Blueprints, int32 ChunkId);
#if WITH_AUTOMATION_TESTS
	friend class FCPMAvatarCookInputsSavedBundleTest;
#endif
};
