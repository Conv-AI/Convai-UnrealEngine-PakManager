// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Interface/WorkflowInterface.h"
#include "Interface/WorkflowListenerInterface.h"
#include "Interface/JobInterface.h"
#include "Workflow.generated.h"

class UWorkflowContext;

DECLARE_DELEGATE_TwoParams(FWorkflowEventCallback, EWorkflowEventType, const FWorkflowStatusInfo&);

UCLASS()
class CONVAIJOBSYSTEM_API UWorkflow : public UObject, public IWorkflowInterface
{
	GENERATED_BODY()

public:
	void SetHandle(const FWorkflowHandle& InHandle) { Handle = InHandle; }
	void SetEventCallback(const FWorkflowEventCallback& Callback) { EventCallback = Callback; }

	// IWorkflowInterface
	virtual bool IInitializeFromJobs(const FWorkflowRequestFromJobs& Request) override;
	virtual bool IInitializeFromJobDefinitions(const FWorkflowRequestFromJobDefinitions& Request) override;
	virtual bool IStartWorkflow() override;
	virtual void ICancelWorkflow(bool bForce = false) override;

	virtual FWorkflowHandle IGetHandle() const override { return Handle; }
	virtual FWorkflowStatusInfo IGetStatusInfo() const override;
	virtual UWorkflowContext* IGetContext() const override { return Context; }

	virtual void IOnJobCompleted(const FJobCompletionInfo& CompletionInfo) override;
	virtual void IReportJobProgress(const FJobProgressInfo& ProgressInfo) override;

	virtual void IAddListener(const TScriptInterface<IWorkflowListenerInterface>& Listener) override;
	virtual void IRemoveListener(const TScriptInterface<IWorkflowListenerInterface>& Listener) override;

	bool IsRunning() const { return StatusInfo.Status == EWorkflowStatus::Running || StatusInfo.Status == EWorkflowStatus::Cancelling; }
	bool IsInitialized() const { return StatusInfo.Status == EWorkflowStatus::Initialized; }

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
	void UpdateComputedFields() const;

	FJobConfig GetEffectiveJobConfig() const;
	bool ShouldRetry(EJobResult Result) const;

	bool InitializeWorkflowInternal(
		const TArray<TScriptInterface<IJobInterface>>& Jobs,
		const FWorkflowRequestOptions& Options);

private:
	FWorkflowHandle Handle;
	FWorkflowEventCallback EventCallback;

	UPROPERTY()
	TObjectPtr<UWorkflowContext> Context;

	UPROPERTY()
	TArray<TScriptInterface<IJobInterface>> JobQueue;

	UPROPERTY()
	TArray<TScriptInterface<IWorkflowListenerInterface>> Listeners;

	UPROPERTY()
	TObjectPtr<UObject> CurrentJobObject;

	mutable FWorkflowStatusInfo StatusInfo;
	FWorkflowConfig WorkflowConfig;
	FJobConfig CurrentJobConfig;
	double JobStartTime = 0.0;
	double WorkflowStartTime = 0.0;

	FTimerHandle JobTimeoutHandle;
	FTimerHandle WorkflowTimeoutHandle;
	FTimerHandle RetryHandle;
};
