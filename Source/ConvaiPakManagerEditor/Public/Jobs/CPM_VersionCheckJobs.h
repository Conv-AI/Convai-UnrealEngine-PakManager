// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interface/JobInterface.h"
#include "CPM_VersionCheckJobs.generated.h"

class UCPM_GetGithubRepoFileProxy;

/**
 * Validates that the local ConvaiPakManager plugin version is not behind
 * the version published in the remote repository.
 *
 * Repo:   Conv-AI/Convai-UnrealEngine-PakManager (main)
 * File:   ConvaiPakManager.uplugin
 * Field:  "Version" (integer)
 *
 * Fails when: local Version < remote Version.
 */
UCLASS(BlueprintType)
class CONVAIPAKMANAGEREDITOR_API UCPM_PakManagerVersionCheckJob : public UObject, public IJobInterface
{
	GENERATED_BODY()

public:
	UCPM_PakManagerVersionCheckJob();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Version Check")
	FJobConfig JobConfig;

	// IJobInterface
	virtual void IPreInitialize_Implementation(const FJobDefinition& Definition) override;
	virtual void IInitialize_Implementation(const TScriptInterface<IWorkflowInterface>& Workflow) override;
	virtual void IExecute_Implementation() override;
	virtual void ICancel_Implementation(bool bForce) override;
	virtual FJobConfig IGetJobConfig_Implementation() const override { return JobConfig; }

private:
	UFUNCTION()
	void OnFileReceived(const FString& ResponseString);

	UFUNCTION()
	void OnFileFetchFailed(const FString& ResponseString);

	void NotifyCompletion(EJobResult Result, const FString& ErrorMessage = TEXT(""));

	UPROPERTY()
	TScriptInterface<IWorkflowInterface> CachedWorkflow;

	UPROPERTY()
	TObjectPtr<UCPM_GetGithubRepoFileProxy> ActiveProxy;

	bool bIsCancelled = false;
};

/**
 * Validates that the current Unreal Engine version matches the
 * target version required by the Convai Modding Tool.
 *
 * Repo:   Conv-AI/Convai-UnrealEngine-ModdingTool (main)
 * File:   Version.json
 * Field:  "target-ue-version" (string, e.g. "5.5")
 *
 * Fails when: current UE version != target-ue-version.
 */
UCLASS(BlueprintType)
class CONVAIPAKMANAGEREDITOR_API UCPM_ModdingToolUEVersionCheckJob : public UObject, public IJobInterface
{
	GENERATED_BODY()

public:
	UCPM_ModdingToolUEVersionCheckJob();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Version Check")
	FJobConfig JobConfig;

	// IJobInterface
	virtual void IPreInitialize_Implementation(const FJobDefinition& Definition) override;
	virtual void IInitialize_Implementation(const TScriptInterface<IWorkflowInterface>& Workflow) override;
	virtual void IExecute_Implementation() override;
	virtual void ICancel_Implementation(bool bForce) override;
	virtual FJobConfig IGetJobConfig_Implementation() const override { return JobConfig; }

private:
	UFUNCTION()
	void OnFileReceived(const FString& ResponseString);

	UFUNCTION()
	void OnFileFetchFailed(const FString& ResponseString);

	void NotifyCompletion(EJobResult Result, const FString& ErrorMessage = TEXT(""));

	UPROPERTY()
	TScriptInterface<IWorkflowInterface> CachedWorkflow;

	UPROPERTY()
	TObjectPtr<UCPM_GetGithubRepoFileProxy> ActiveProxy;

	bool bIsCancelled = false;
};
