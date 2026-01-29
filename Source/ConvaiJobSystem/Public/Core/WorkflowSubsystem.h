// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Interface/WorkflowManagerInterface.h"
#include "Interface/JobInterface.h"
#include "WorkflowSubsystem.generated.h"

class UWorkflowContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorkflowCompleted, EWorkflowStatus, Status, const FString&,ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnJobStatusChanged, int32, JobIndex, EJobResult, Result, UObject*, Job);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCancellationRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnJobTimedOut, int32, JobIndex, UObject*, Job);

/**
 * Default workflow manager provided by the module.
 * Users can create their own by implementing IWorkflowManagerInterface.
 */
UCLASS()
class CONVAIJOBSYSTEM_API UWorkflowSubsystem : public UEngineSubsystem, public IWorkflowManagerInterface
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// IWorkflowManagerInterface
	virtual UWorkflowContext* GetContext() const override;
	virtual void OnJobCompleted(UObject* Job, EJobResult Result, const FString& ErrorMessage = TEXT("")) override;
	virtual bool IsCancellationRequested() const override;
	// ~IWorkflowManagerInterface
	
	UFUNCTION(BlueprintCallable, Category = "Workflow")
	bool ExecuteWorkflow(const FWorkflowConfig& Config, const TArray<TScriptInterface<IJobInterface>>& Jobs);

	UFUNCTION(BlueprintCallable, Category = "Workflow")
	void CancelWorkflow(bool bForce = false);

	UFUNCTION(BlueprintPure, Category = "Workflow")
	EWorkflowStatus GetStatus() const { return Status; }

	UFUNCTION(BlueprintPure, Category = "Workflow")
	bool IsRunning() const { return Status == EWorkflowStatus::Running; }

	UFUNCTION(BlueprintPure, Category = "Workflow")
	int32 GetCurrentJobIndex() const { return IsRunning() ? CurrentJobIndex : -1; }

	UFUNCTION(BlueprintPure, Category = "Workflow")
	int32 GetTotalJobCount() const { return JobQueue.Num(); }

	UFUNCTION(BlueprintPure, Category = "Workflow")
	float GetProgress() const;

	UFUNCTION(BlueprintPure, Category = "Workflow")
	float GetCurrentJobElapsedTime() const;

	UFUNCTION(BlueprintPure, Category = "Workflow")
	float GetWorkflowElapsedTime() const;

	UPROPERTY(BlueprintAssignable, Category = "Workflow|Events")
	FOnWorkflowCompleted OnWorkflowCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Workflow|Events")
	FOnJobStatusChanged OnJobStatusChanged;

	/** Jobs can bind to this for immediate cancellation notification */
	UPROPERTY(BlueprintAssignable, Category = "Workflow|Events")
	FOnCancellationRequested OnCancellationRequested;

	UPROPERTY(BlueprintAssignable, Category = "Workflow|Events")
	FOnJobTimedOut OnJobTimedOut;

protected:
	void ExecuteCurrentJob();
	void AdvanceToNextJob();
	void FinishWorkflow(EWorkflowStatus FinalStatus, const FString& ErrorMessage = TEXT(""));
	void CleanupInternalState();
	void ResetState(bool bBroadcastCancelled);
	void ClearAllTimers();
	void StartJobTimeoutTimer();
	void ClearJobTimeoutTimer();
	void HandleJobTimeout();
	void StartWorkflowTimeoutTimer();
	void ClearWorkflowTimeoutTimer();
	void HandleWorkflowTimeout();

private:
	UPROPERTY()
	TObjectPtr<UWorkflowContext> Context;

	UPROPERTY()
	TArray<TScriptInterface<IJobInterface>> JobQueue;

	UPROPERTY()
	TObjectPtr<UObject> CurrentJob;

	int32 CurrentJobIndex = 0;
	EWorkflowStatus Status = EWorkflowStatus::Idle;
	bool bCancellationRequested = false;
	FString LastErrorMessage;
	FWorkflowConfig CurrentConfig;
	double CurrentJobStartTime = 0.0;
	double WorkflowStartTime = 0.0;
	FTimerHandle JobTimeoutTimerHandle;
	FTimerHandle WorkflowTimeoutTimerHandle;
};
