// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Type/JS_Definations.h"
#include "WorkflowInterface.h"
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
 * Lifecycle:
 * 1. IPreInitialize() - Called when job is created from definition (configuration phase)
 * 2. IInitialize() - Called when job is added to workflow (workflow binding phase)
 * 3. IExecute() - Called to run the job
 * 4. ICancel() - Called if workflow is canceled
 * 
 * Contract:
 * - The job MUST call NotifyJobCompleted() from WorkflowBlueprintLibrary when Execute finishes
 * - Jobs can be synchronous or asynchronous internally
 * - Jobs should respect cancellation requests
 */
class CONVAIJOBSYSTEM_API IJobInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Job")
	void IPreInitialize(const FJobDefinition& Definition);
	virtual void IPreInitialize_Implementation(const FJobDefinition& Definition) {}

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Job")
	void IInitialize(const TScriptInterface<IWorkflowInterface>& Workflow);
	virtual void IInitialize_Implementation(const TScriptInterface<IWorkflowInterface>& Workflow) {}

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Job")
	void IExecute();
	virtual void IExecute_Implementation() {}

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
