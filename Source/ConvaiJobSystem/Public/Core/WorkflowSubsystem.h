// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Interface/WorkflowManagerInterface.h"
#include "Interface/JobInterface.h"
#include "WorkflowSubsystem.generated.h"

class UWorkflowContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorkflowStatusChanged, const FWorkflowStatusEvent&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJobStatusChanged, const FJobStatusEvent&, Event);

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
	virtual bool IsRunning() const override { return Status == EWorkflowStatus::Running || Status == EWorkflowStatus::CancellationRequested; }
	
	UFUNCTION(BlueprintPure, Category = "Workflow")
	virtual float GetProgress() const override;
	
	UFUNCTION(BlueprintCallable, Category = "Workflow")
	virtual UWorkflowContext* GetContext() const override { return Context; }
	
	virtual void OnJobCompleted(UObject* Job, EJobResult Result, const FString& ErrorMessage = TEXT("")) override;
	
	UFUNCTION(BlueprintPure, Category = "Workflow")
	virtual bool IsCancellationRequested() const override { return bCancellationRequested; }

	UFUNCTION(BlueprintPure, Category = "Workflow")
	int32 GetCurrentJobIndex() const { return IsRunning() ? CurrentJobIndex : -1; }

	UFUNCTION(BlueprintPure, Category = "Workflow")
	int32 GetTotalJobCount() const { return JobQueue.Num(); }

	UFUNCTION(BlueprintPure, Category = "Workflow")
	int32 GetCurrentJobRetryCount() const { return CurrentRetryCount; }

	UFUNCTION(BlueprintPure, Category = "Workflow")
	float GetCurrentJobElapsedTime() const;

	UFUNCTION(BlueprintPure, Category = "Workflow")
	float GetWorkflowElapsedTime() const;

	UPROPERTY(BlueprintAssignable, Category = "Workflow|Events")
	FOnWorkflowStatusChanged OnWorkflowStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Workflow|Events")
	FOnJobStatusChanged OnJobStatusChanged;

protected:
	void ExecuteCurrentJob();
	void CancelCurrentJob(bool bForce = false);
	void AdvanceToNextJob();
	void RetryCurrentJob();
	void ScheduleRetry(float DelaySeconds);
	void HandleRetryTimer();
	void FinishWorkflow(EWorkflowStatus FinalStatus, const FString& ErrorMessage = TEXT(""));
	void CleanupInternalState();
	void ResetState(bool bBroadcastCancelled);
	void ClearAllTimers();
	void StartJobTimeoutTimer(float TimeoutSeconds);
	void ClearJobTimeoutTimer();
	void HandleJobTimeout();
	void StartWorkflowTimeoutTimer();
	void ClearWorkflowTimeoutTimer();
	void HandleWorkflowTimeout();
	FJobConfig GetCurrentJobConfig() const;
	bool ShouldRetry(EJobResult Result, const FJobConfig& Config) const;
	void BroadcastJobStatus(EJobResult Result, const FString& ErrorMessage = TEXT(""));
	void SetStatus(EWorkflowStatus NewStatus, const FString& Message = TEXT(""));

private:
	UPROPERTY()
	TObjectPtr<UWorkflowContext> Context;

	UPROPERTY()
	TArray<TScriptInterface<IJobInterface>> JobQueue;

	UPROPERTY()
	TObjectPtr<UObject> CurrentJob;

	int32 CurrentJobIndex = 0;
	int32 CurrentRetryCount = 0;
	EWorkflowStatus Status = EWorkflowStatus::Idle;
	bool bCancellationRequested = false;
	FString LastErrorMessage;
	FWorkflowConfig WorkflowConfig;
	FJobConfig CurrentJobConfig;
	double CurrentJobStartTime = 0.0;
	double WorkflowStartTime = 0.0;
	FTimerHandle JobTimeoutTimerHandle;
	FTimerHandle WorkflowTimeoutTimerHandle;
	FTimerHandle RetryTimerHandle;
};
