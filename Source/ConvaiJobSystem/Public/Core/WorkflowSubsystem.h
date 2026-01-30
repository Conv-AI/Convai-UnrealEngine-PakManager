// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Interface/WorkflowManagerInterface.h"
#include "Interface/WorkflowListenerInterface.h"
#include "Interface/JobInterface.h"
#include "WorkflowSubsystem.generated.h"

class UWorkflowContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorkflowEvent, EWorkflowEventType, EventType, const FWorkflowStatusInfo&, StatusInfo);

UCLASS()
class CONVAIJOBSYSTEM_API UWorkflowSubsystem : public UEngineSubsystem, public IWorkflowManagerInterface
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	/*
	* EWorkflowEventType → WHAT happened (event trigger)
	* EWorkflowStatus → CURRENT workflow state
	* EJobResult → Job outcome/state (Pending → InProgress → terminal state)
	*/
	UPROPERTY(BlueprintAssignable, Category = "Workflow|Events")
	FOnWorkflowEvent OnWorkflowEvent;
	
	// IWorkflowManagerInterface
	UFUNCTION(BlueprintCallable, Category = "Workflow")
	virtual bool IExecuteWorkflow(const FWorkflowRequest& Request) override;
	
	UFUNCTION(BlueprintCallable, Category = "Workflow")
	virtual void ICancelWorkflow(bool bForce = false) override;
	
	UFUNCTION(BlueprintPure, Category = "Workflow")
	virtual FWorkflowStatusInfo IGetStatusInfo() override;
	
	UFUNCTION(BlueprintCallable, Category = "Workflow")
	virtual UWorkflowContext* IGetContext() override { return Context; }
	
	UFUNCTION(BlueprintCallable, Category = "Workflow")
	virtual void IAddListener(const TScriptInterface<IWorkflowListenerInterface>& Listener) override;
	
	UFUNCTION(BlueprintCallable, Category = "Workflow")
	virtual void IRemoveListener(const TScriptInterface<IWorkflowListenerInterface>& Listener) override;
	
	UFUNCTION(BlueprintPure, Category = "Workflow")
	bool IsRunning() const { return StatusInfo.Status == EWorkflowStatus::Running || StatusInfo.Status == EWorkflowStatus::Cancelling; }
	
	virtual void IOnJobCompleted(const FJobCompletionInfo& CompletionInfo) override;
	virtual void IReportJobProgress(const FJobProgressInfo& ProgressInfo) override;
	
protected:
	void ExecuteCurrentJob();
	void AdvanceToNextJob();
	void RetryCurrentJob();
	void SkipCurrentJob();
	void FinishWorkflow(EWorkflowStatus FinalStatus, const FString& ErrorMessage = TEXT(""));
	void ResetState();
	
	void HandleJobTimeout();
	void HandleWorkflowTimeout();
	void HandleRetryTimer();
	
	void BroadcastEvent(EWorkflowEventType EventType);
	void UpdateComputedFields();
	
	FJobConfig GetEffectiveJobConfig() const;
	bool ShouldRetry(EJobResult Result) const;

private:
	UPROPERTY()
	TObjectPtr<UWorkflowContext> Context;

	UPROPERTY()
	TArray<TScriptInterface<IJobInterface>> JobQueue;

	UPROPERTY()
	TArray<TScriptInterface<IWorkflowListenerInterface>> Listeners;

	UPROPERTY()
	TObjectPtr<UObject> CurrentJobObject;

	FWorkflowStatusInfo StatusInfo;
	FWorkflowConfig WorkflowConfig;
	FJobConfig CurrentJobConfig;
	double JobStartTime = 0.0;
	double WorkflowStartTime = 0.0;
	
	FTimerHandle JobTimeoutHandle;
	FTimerHandle WorkflowTimeoutHandle;
	FTimerHandle RetryHandle;
};
