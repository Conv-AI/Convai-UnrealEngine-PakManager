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
	UFUNCTION(BlueprintCallable, Category = "Workflow")
	virtual bool ExecuteWorkflow(const FWorkflowConfig& Config, const TArray<TScriptInterface<IJobInterface>>& Jobs) override;
	
	UFUNCTION(BlueprintCallable, Category = "Workflow")
	virtual void CancelWorkflow(bool bForce = false) override;
	
	UFUNCTION(BlueprintPure, Category = "Workflow")
	virtual EWorkflowStatus GetStatus() const override { return Status; }
	
	UFUNCTION(BlueprintPure, Category = "Workflow")
	virtual bool IsRunning() const override { return Status == EWorkflowStatus::Running; }
	
	UFUNCTION(BlueprintPure, Category = "Workflow")
	virtual float GetProgress() const override;
	
	UFUNCTION(BlueprintCallable, Category = "Workflow")
	virtual UWorkflowContext* GetContext() const override;
	
	virtual void OnJobCompleted(UObject* Job, EJobResult Result, const FString& ErrorMessage = TEXT("")) override;
	
	UFUNCTION(BlueprintPure, Category = "Workflow")
	virtual bool IsCancellationRequested() const override { return bCancellationRequested; }

	UFUNCTION(BlueprintPure, Category = "Workflow")
	int32 GetCurrentJobIndex() const { return IsRunning() ? CurrentJobIndex : -1; }

	UFUNCTION(BlueprintPure, Category = "Workflow")
	int32 GetTotalJobCount() const { return JobQueue.Num(); }

	UFUNCTION(BlueprintPure, Category = "Workflow")
	float GetCurrentJobElapsedTime() const;

	UFUNCTION(BlueprintPure, Category = "Workflow")
	float GetWorkflowElapsedTime() const;

	UPROPERTY(BlueprintAssignable, Category = "Workflow|Events")
	FOnWorkflowCompleted OnWorkflowCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Workflow|Events")
	FOnJobStatusChanged OnJobStatusChanged;

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
