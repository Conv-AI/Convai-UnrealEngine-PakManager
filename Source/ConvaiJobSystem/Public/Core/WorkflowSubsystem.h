// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Interface/WorkflowManagerInterface.h"
#include "Interface/JobInterface.h"
#include "WorkflowSubsystem.generated.h"

class UWorkflowContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorkflowCompleted, EWorkflowStatus, Status, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnJobStatusChanged, int32, JobIndex, EJobResult, Result, UObject*, Job);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCancellationRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnJobTimedOut, int32, JobIndex, UObject*, Job);

/**
 * Engine subsystem that provides workflow management capabilities.
 * This is the default workflow manager provided by the module.
 * Users can also create their own managers by implementing IWorkflowManagerInterface.
 * 
 * Usage:
 *   UWorkflowSubsystem* WS = GEngine->GetEngineSubsystem<UWorkflowSubsystem>();
 *   WS->ExecuteWorkflow(ArrayOfJobs);
 * 
 * Cancellation:
 *   - CancelWorkflow() requests graceful cancellation, jobs should respond
 *   - CancelWorkflow(true) forces immediate reset if a job is stuck
 * 
 * Timeouts:
 *   - Use ExecuteWorkflowWithConfig() with FWorkflowConfig for timeout support
 *   - JobTimeoutSeconds: Max time for any single job
 *   - WorkflowTimeoutSeconds: Max time for entire workflow
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
	// ~IWorkflowManagerInterface
	
	/**
	 * Execute a workflow with configuration options.
	 * 
	 * @param Jobs Array of objects implementing IJobInterface
	 * @param Config Workflow configuration (timeouts, etc.)
	 * @return True if workflow started successfully, false if already running
	 */
	UFUNCTION(BlueprintCallable, Category = "Workflow")
	bool ExecuteWorkflow(const FWorkflowConfig& Config, const TArray<TScriptInterface<IJobInterface>>& Jobs);

	/**
	 * Cancel the workflow.
	 * 
	 * @param bForce If false (default): Graceful cancellation - sets flag and broadcasts 
	 *               OnCancellationRequested. Jobs should check IsCancellationRequested() 
	 *               and complete with Cancelled result.
	 *               If true: Force immediate reset - does not wait for current job.
	 *               The stuck job may continue running but its completion will be ignored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Workflow")
	void CancelWorkflow(bool bForce = false);

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

	/**
	 * Get how long the current job has been running.
	 * @return Elapsed time in seconds, or 0 if no job running
	 */
	UFUNCTION(BlueprintPure, Category = "Workflow")
	float GetCurrentJobElapsedTime() const;

	/**
	 * Get how long the workflow has been running.
	 * @return Elapsed time in seconds, or 0 if not running
	 */
	UFUNCTION(BlueprintPure, Category = "Workflow")
	float GetWorkflowElapsedTime() const;

	/** Called when the workflow completes (success, failure, or cancelled) */
	UPROPERTY(BlueprintAssignable, Category = "Workflow|Events")
	FOnWorkflowCompleted OnWorkflowCompleted;

	/** Called when each job completes */
	UPROPERTY(BlueprintAssignable, Category = "Workflow|Events")
	FOnJobStatusChanged OnJobStatusChanged;

	/** 
	 * Called when cancellation is requested. 
	 * Jobs can bind to this for immediate notification instead of polling IsCancellationRequested().
	 */
	UPROPERTY(BlueprintAssignable, Category = "Workflow|Events")
	FOnCancellationRequested OnCancellationRequested;

	/** Called when a job times out (before it's force-failed) */
	UPROPERTY(BlueprintAssignable, Category = "Workflow|Events")
	FOnJobTimedOut OnJobTimedOut;

protected:
	/** Execute the job at CurrentJobIndex */
	void ExecuteCurrentJob();

	/** Advance to the next job or finish if done */
	void AdvanceToNextJob();

	/** Finish the workflow with the given status */
	void FinishWorkflow(EWorkflowStatus FinalStatus, const FString& ErrorMessage = TEXT(""));

	/** Clean up internal state (timers, job queue, indices) but preserve Context and Status */
	void CleanupInternalState();

	/** Full reset including Context. If bBroadcastCancelled, fires OnWorkflowCompleted with Cancelled status. */
	void ResetState(bool bBroadcastCancelled);

	/** Clear both job and workflow timeout timers */
	void ClearAllTimers();

	/** Start the job timeout timer */
	void StartJobTimeoutTimer();

	/** Clear the job timeout timer */
	void ClearJobTimeoutTimer();

	/** Called when job timeout timer fires */
	void HandleJobTimeout();

	/** Start the workflow timeout timer */
	void StartWorkflowTimeoutTimer();

	/** Clear the workflow timeout timer */
	void ClearWorkflowTimeoutTimer();

	/** Called when workflow timeout timer fires */
	void HandleWorkflowTimeout();

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

	/** Current workflow configuration */
	FWorkflowConfig CurrentConfig;

	/** Time when current job started */
	double CurrentJobStartTime = 0.0;

	/** Time when workflow started */
	double WorkflowStartTime = 0.0;

	/** Timer handle for job timeout */
	FTimerHandle JobTimeoutTimerHandle;

	/** Timer handle for workflow timeout */
	FTimerHandle WorkflowTimeoutTimerHandle;
};
