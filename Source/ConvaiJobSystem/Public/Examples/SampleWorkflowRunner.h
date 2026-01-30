// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/WorkflowListenerInterface.h"
#include "Interface/WorkflowManagerInterface.h"
#include "SampleWorkflowRunner.generated.h"

class USampleAsyncJob;
class USampleFailingJob;
class USampleConditionalJob;
class USampleContextWriterJob;

/**
 * Sample actor demonstrating complete workflow system usage.
 * 
 * Features demonstrated:
 * - Creating and configuring jobs
 * - Setting up workflow configuration
 * - Listening to workflow events
 * - Handling progress updates
 * - Retry behavior
 * - Conditional job execution
 * - Context data sharing between jobs
 * 
 * Usage:
 * 1. Place this actor in your level
 * 2. Call RunDemoWorkflow() from Blueprint or C++
 * 3. Watch the output log for workflow progress
 */
UCLASS(Blueprintable, BlueprintType)
class CONVAIJOBSYSTEM_API ASampleWorkflowRunner : public AActor, public IWorkflowListenerInterface
{
	GENERATED_BODY()

public:
	ASampleWorkflowRunner();

	// Workflow configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow Demo")
	float WorkflowTimeoutSeconds = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow Demo")
	int32 DefaultMaxRetries = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow Demo")
	float DefaultRetryDelaySeconds = 1.0f;

	// Demo settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow Demo")
	bool bEnableConditionalJob = true;

	/**
	 * Runs a demonstration workflow showing all system capabilities.
	 * 
	 * The workflow includes:
	 * 1. Context Writer Job - Writes data to context
	 * 2. Async Job - Shows progress reporting
	 * 3. Conditional Job - Only runs if context key exists
	 * 4. Failing Job - Demonstrates retry system
	 * 5. Another Async Job - Final cleanup
	 */
	UFUNCTION(BlueprintCallable, Category = "Workflow Demo")
	void RunDemoWorkflow();

	/**
	 * Cancels the currently running workflow.
	 */
	UFUNCTION(BlueprintCallable, Category = "Workflow Demo")
	void CancelWorkflow(bool bForce = false);

	/**
	 * Returns true if a workflow is currently running.
	 */
	UFUNCTION(BlueprintPure, Category = "Workflow Demo")
	bool IsWorkflowRunning() const;

	// IWorkflowListenerInterface
	virtual void IOnWorkflowEvent_Implementation(EWorkflowEventType EventType, const FWorkflowStatusInfo& StatusInfo) override;

	// Events for Blueprint binding
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorkflowEventReceived, EWorkflowEventType, EventType, const FWorkflowStatusInfo&, StatusInfo);

	UPROPERTY(BlueprintAssignable, Category = "Workflow Demo|Events")
	FOnWorkflowEventReceived OnWorkflowEventReceived;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void LogWorkflowEvent(EWorkflowEventType EventType, const FWorkflowStatusInfo& StatusInfo);
	FString GetEventTypeName(EWorkflowEventType EventType) const;

	UPROPERTY()
	TScriptInterface<IWorkflowManagerInterface> WorkflowManager;

	UPROPERTY()
	TArray<TScriptInterface<IJobInterface>> CreatedJobs;
};
