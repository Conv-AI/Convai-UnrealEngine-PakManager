// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Type/JS_Definations.h"
#include "WorkflowManagerInterface.h"
#include "JobInterface.generated.h"

UINTERFACE(BlueprintType, Blueprintable)
class UJobInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for workflow jobs.
 * Any class can implement this interface to become executable within a workflow.
 * 
 * Contract:
 * - Execute() will be called by the workflow manager
 * - The job MUST call NotifyJobCompleted() from WorkflowBlueprintLibrary when done
 * - Jobs can be synchronous or asynchronous internally
 * - Jobs should check IsCancellationRequested() for long operations
 */
class CONVAIJOBSYSTEM_API IJobInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Job")
	void Execute(const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Job")
	void Cancel(bool bForce = false);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Job")
	FJobConfig GetJobConfig() const;
};
