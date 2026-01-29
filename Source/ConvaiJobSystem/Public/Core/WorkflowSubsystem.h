// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Type/JobTypes.h"
#include "Interface/WorkflowManagerInterface.h"
#include "Interface/JobInterface.h"
#include "WorkflowSubsystem.generated.h"

class UWorkflowContext;

// Delegate for workflow completion
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorkflowCompleted, EWorkflowStatus, Status, const FString&, ErrorMessage);

// Delegate for individual job completion
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnJobStatusChanged, int32, JobIndex, EJobResult, Result, UObject*, Job);

/**
 * Engine subsystem that provides workflow management capabilities.
 * This is the default workflow manager provided by the module.
 * Users can also create their own managers by implementing IWorkflowManagerInterface.
 * 
 * Usage:
 *   UWorkflowSubsystem* WS = GEngine->GetEngineSubsystem<UWorkflowSubsystem>();
 *   WS->ExecuteWorkflow(ArrayOfJobs);
 */
UCLASS()
class CONVAIJOBSYSTEM_API UWorkflowSubsystem : public UEngineSubsystem, public IWorkflowManagerInterface
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	// IWorkflowManagerInterface implementation
	virtual UWorkflowContext* GetContext() const override;
	virtual void OnJobCompleted(UObject* Job, EJobResult Result, const FString& ErrorMessage = TEXT("")) override;
	virtual bool IsCancellationRequested() const override;

	/**
	 * Execute a workflow with the given jobs in sequential order.
	 * Jobs are executed one at a time, in array order.
	 * 
	 * @param Jobs Array of objects implementing IJobInterface
	 * @return True if workflow started successfully, false if already running
	 */
	UFUNCTION(BlueprintCallable, Category = "Workflow")
	bool ExecuteWorkflow(const TArray<TScriptInterface<IJobInterface>>& Jobs);

	/**
	 * Cancel the currently running workflow.
	 * Sets cancellation flag - jobs should check IsCancellationRequested().
	 */
	UFUNCTION(BlueprintCallable, Category = "Workflow")
	void CancelWorkflow();

	/**
	 * Get the current workflow status.
	 * @return Current status of the workflow
	 */
	UFUNCTION(BlueprintPure, Category = "Workflow")
	EWorkflowStatus GetStatus() const { return Status; }

	/**
	 * Check if a workflow is currently running.
	 * @return True if running
	 */
	UFUNCTION(BlueprintPure, Category = "Workflow")
	bool IsRunning() const { return Status == EWorkflowStatus::Running; }

	/**
	 * Get current job index (0-based).
	 * @return Index of currently executing job, or -1 if not running
	 */
	UFUNCTION(BlueprintPure, Category = "Workflow")
	int32 GetCurrentJobIndex() const { return IsRunning() ? CurrentJobIndex : -1; }

	/**
	 * Get total number of jobs in current workflow.
	 * @return Total job count, or 0 if not running
	 */
	UFUNCTION(BlueprintPure, Category = "Workflow")
	int32 GetTotalJobCount() const { return JobQueue.Num(); }

	/**
	 * Get workflow progress as a percentage.
	 * @return Progress from 0.0 to 1.0
	 */
	UFUNCTION(BlueprintPure, Category = "Workflow")
	float GetProgress() const;

	/** Called when the workflow completes (success, failure, or cancelled) */
	UPROPERTY(BlueprintAssignable, Category = "Workflow")
	FOnWorkflowCompleted OnWorkflowCompleted;

	/** Called when each job completes */
	UPROPERTY(BlueprintAssignable, Category = "Workflow")
	FOnJobStatusChanged OnJobStatusChanged;

protected:
	/** Execute the job at CurrentJobIndex */
	void ExecuteCurrentJob();

	/** Finish the workflow with the given status */
	void FinishWorkflow(EWorkflowStatus FinalStatus, const FString& ErrorMessage = TEXT(""));

	/** Reset internal state for a new workflow */
	void ResetState();

private:
	/** Shared context for jobs to read/write data */
	UPROPERTY()
	TObjectPtr<UWorkflowContext> Context;

	/** Queue of jobs to execute */
	UPROPERTY()
	TArray<TScriptInterface<IJobInterface>> JobQueue;

	/** Currently executing job (kept alive) */
	UPROPERTY()
	TObjectPtr<UObject> CurrentJob;

	/** Index of current job being executed */
	int32 CurrentJobIndex = 0;

	/** Current workflow status */
	EWorkflowStatus Status = EWorkflowStatus::Idle;

	/** Flag set when cancellation is requested */
	bool bCancellationRequested = false;

	/** Error message from failed job */
	FString LastErrorMessage;
};
