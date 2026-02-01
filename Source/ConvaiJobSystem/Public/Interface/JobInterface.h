// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Type/JS_Definations.h"
#include "WorkflowManagerInterface.h"
#include "JobInterface.generated.h"

class UWorkflowContext;

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
	void IInitialize(const FJobConfig& Config);
	virtual void IInitialize_Implementation(const FJobConfig& Config) {}

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Job")
	void IExecute(const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager);
	virtual void IExecute_Implementation(const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager) {}

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Job")
	void ICancel(bool bForce = false);
	virtual void ICancel_Implementation(bool bForce) {}

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Job")
	FJobConfig IGetJobConfig() const;
	virtual FJobConfig IGetJobConfig_Implementation() const { return FJobConfig(); }

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Job")
	bool IShouldSkip(UWorkflowContext* Context) const;
	virtual bool IShouldSkip_Implementation(UWorkflowContext* Context) const { return false; }
};
