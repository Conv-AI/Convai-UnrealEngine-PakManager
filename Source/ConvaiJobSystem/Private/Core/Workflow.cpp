// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Core/Workflow.h"
#include "Core/WorkflowContext.h"
#include "Interface/JobInterface.h"
#include "Interface/WorkflowListenerInterface.h"
#include "Utility/WorkflowBlueprintLibrary.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorkflowInstance, Log, All);

namespace WorkflowHelpers
{
	FTimerManager* GetTimerManager()
	{
		if (const UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr)
		{
			return &World->GetTimerManager();
		}
		return nullptr;
	}

	void ClearTimer(FTimerHandle& Handle)
	{
		if (Handle.IsValid())
		{
			if (FTimerManager* TimerManager = GetTimerManager())
			{
				TimerManager->ClearTimer(Handle);
			}
			Handle.Invalidate();
		}
	}

	void SetTimer(FTimerHandle& Handle, const float Seconds, const FTimerDelegate& Delegate)
	{
		ClearTimer(Handle);
		if (FTimerManager* TimerManager = GetTimerManager())
		{
			TimerManager->SetTimer(Handle, Delegate, Seconds, false);
		}
	}

	float CalculateElapsedTime(const double StartTime)
	{
		return StartTime > 0.0 ? static_cast<float>(FPlatformTime::Seconds() - StartTime) : 0.0f;
	}

	float CalculateProgress(const int32 CurrentIndex, const int32 TotalJobs, const float JobProgress)
	{
		if (TotalJobs == 0) return 0.0f;
		const float BaseProgress = static_cast<float>(CurrentIndex) / static_cast<float>(TotalJobs);
		const float JobContribution = JobProgress / static_cast<float>(TotalJobs);
		return BaseProgress + JobContribution;
	}

	EWorkflowEventType GetWorkflowFinishEvent(const EWorkflowStatus FinalStatus)
	{
		switch (FinalStatus)
		{
		case EWorkflowStatus::Completed: return EWorkflowEventType::WorkflowCompleted;
		case EWorkflowStatus::Failed: return EWorkflowEventType::WorkflowFailed;
		case EWorkflowStatus::Timeout: return EWorkflowEventType::WorkflowTimeout;
		case EWorkflowStatus::Cancelled: return EWorkflowEventType::WorkflowCancelled;
		default: return EWorkflowEventType::WorkflowFailed;
		}
	}
}

void UWorkflow::IAddListener(const TScriptInterface<IWorkflowListenerInterface>& Listener)
{
	if (Listener.GetObject() && !Listeners.Contains(Listener))
	{
		Listeners.Add(Listener);
	}
}

void UWorkflow::IRemoveListener(const TScriptInterface<IWorkflowListenerInterface>& Listener)
{
	Listeners.Remove(Listener);
}

void UWorkflow::UpdateComputedFields() const
{
	StatusInfo.Handle = Handle;
	StatusInfo.ElapsedTime = WorkflowHelpers::CalculateElapsedTime(WorkflowStartTime);
	StatusInfo.Progress = WorkflowHelpers::CalculateProgress(StatusInfo.CurrentJob.Index, StatusInfo.TotalJobs, StatusInfo.CurrentJob.Progress);
	StatusInfo.CurrentJob.ElapsedTime = WorkflowHelpers::CalculateElapsedTime(JobStartTime);
}

FWorkflowStatusInfo UWorkflow::IGetStatusInfo() const
{
	const_cast<UWorkflow*>(this)->UpdateComputedFields();
	return StatusInfo;
}

void UWorkflow::BroadcastEvent(const EWorkflowEventType EventType)
{
	UpdateComputedFields();

	for (const auto& Listener : Listeners)
	{
		if (UObject* ListenerObj = Listener.GetObject())
		{
			IWorkflowListenerInterface::Execute_IOnWorkflowEvent(ListenerObj, EventType, StatusInfo);
		}
	}

	OnWorkflowEvent.Broadcast(EventType, StatusInfo);
}

bool UWorkflow::IInitializeFromJobDefinitions(const FWorkflowRequestFromJobDefinitions& Request)
{
	if (IsRunning() || IsInitialized())
	{
		UE_LOG(LogWorkflowInstance, Warning, TEXT("[%s] Cannot initialize workflow - already initialized or running"), *Handle.Id.ToString());
		return false;
	}

	TArray<TScriptInterface<IJobInterface>> Jobs;
	if (!UWorkflowBlueprintLibrary::CreateJobsFromDefinitions(this, Request.JobDefinitions, Jobs))
	{
		UE_LOG(LogWorkflowInstance, Error, TEXT("[%s] Failed to create jobs from definitions"), *Handle.Id.ToString());
		return false;
	}

	return InitializeWorkflowInternal(Jobs, Request.Options);
}

bool UWorkflow::IInitializeFromJobs(const FWorkflowRequestFromJobs& Request)
{
	if (IsRunning() || IsInitialized())
	{
		UE_LOG(LogWorkflowInstance, Warning, TEXT("[%s] Cannot initialize workflow - already initialized or running"), *Handle.Id.ToString());
		return false;
	}

	return InitializeWorkflowInternal(Request.Jobs, Request.Options);
}

bool UWorkflow::InitializeWorkflowInternal(
	const TArray<TScriptInterface<IJobInterface>>& Jobs,
	const FWorkflowRequestOptions& Options)
{
	if (Jobs.Num() == 0)
	{
		UE_LOG(LogWorkflowInstance, Warning, TEXT("[%s] Cannot initialize workflow - no jobs provided"), *Handle.Id.ToString());
		return false;
	}

	for (int32 i = 0; i < Jobs.Num(); i++)
	{
		const UObject* JobObject = Jobs[i].GetObject();
		if (!JobObject || !JobObject->GetClass()->ImplementsInterface(UJobInterface::StaticClass()))
		{
			UE_LOG(LogWorkflowInstance, Error, TEXT("[%s] Job %d is invalid or does not implement IJobInterface"), *Handle.Id.ToString(), i);
			return false;
		}
	}

	ResetState();

	Context = NewObject<UWorkflowContext>(this);

	for (const auto& Listener : Options.Listeners)
	{
		IAddListener(Listener);
	}

	WorkflowConfig = Options.WorkflowConfig;
	JobQueue = Jobs;

	TScriptInterface<IWorkflowInterface> WorkflowInterface;
	WorkflowInterface.SetObject(this);
	WorkflowInterface.SetInterface(Cast<IWorkflowInterface>(this));

	// Bind all jobs to this workflow (IPreInitialize was already called during job creation)
	for (const TScriptInterface<IJobInterface>& Job : JobQueue)
	{
		if (UObject* JobObject = Job.GetObject())
		{
			IJobInterface::Execute_IInitialize(JobObject, WorkflowInterface);
		}
	}

	StatusInfo.Handle = Handle;
	StatusInfo.Status = EWorkflowStatus::Initialized;
	StatusInfo.TotalJobs = Jobs.Num();
	StatusInfo.CurrentJob.Index = 0;

	UE_LOG(LogWorkflowInstance, Log, TEXT("[%s] Workflow initialized with %d jobs"), *Handle.Id.ToString(), JobQueue.Num());

	return true;
}

bool UWorkflow::IStartWorkflow()
{
	if (IsRunning())
	{
		UE_LOG(LogWorkflowInstance, Warning, TEXT("[%s] Cannot start workflow - already running"), *Handle.Id.ToString());
		return false;
	}

	if (!IsInitialized())
	{
		UE_LOG(LogWorkflowInstance, Warning, TEXT("[%s] Cannot start workflow - not initialized"), *Handle.Id.ToString());
		return false;
	}

	WorkflowStartTime = FPlatformTime::Seconds();
	StatusInfo.Status = EWorkflowStatus::Running;

	UE_LOG(LogWorkflowInstance, Log, TEXT("[%s] Starting workflow execution"), *Handle.Id.ToString());
	BroadcastEvent(EWorkflowEventType::WorkflowStarted);

	if (WorkflowConfig.WorkflowTimeoutSeconds > 0.0f)
	{
		WorkflowHelpers::SetTimer(WorkflowTimeoutHandle, WorkflowConfig.WorkflowTimeoutSeconds,
			FTimerDelegate::CreateUObject(this, &UWorkflow::HandleWorkflowTimeout));
	}

	ExecuteCurrentJob();
	return true;
}

void UWorkflow::ICancelWorkflow(const bool bForce)
{
	if (!IsRunning() && !IsInitialized()) return;

	UE_LOG(LogWorkflowInstance, Log, TEXT("[%s] Cancelling workflow (force=%s)"), *Handle.Id.ToString(), bForce ? TEXT("true") : TEXT("false"));

	if (bForce || IsInitialized())
	{
		if (CurrentJobObject && CurrentJobObject->GetClass()->ImplementsInterface(UJobInterface::StaticClass()))
		{
			IJobInterface::Execute_ICancel(CurrentJobObject, true);
		}
		FinishWorkflow(EWorkflowStatus::Cancelled, TEXT("Workflow was cancelled"));
		return;
	}

	if (StatusInfo.Status == EWorkflowStatus::Cancelling) return;

	StatusInfo.Status = EWorkflowStatus::Cancelling;

	if (CurrentJobObject && CurrentJobObject->GetClass()->ImplementsInterface(UJobInterface::StaticClass()))
	{
		IJobInterface::Execute_ICancel(CurrentJobObject, false);
	}
}

void UWorkflow::IOnJobCompleted(const FJobCompletionInfo& CompletionInfo)
{
	if (!IsRunning())
	{
		UE_LOG(LogWorkflowInstance, Verbose, TEXT("[%s] OnJobCompleted ignored - workflow not running"), *Handle.Id.ToString());
		return;
	}

	WorkflowHelpers::ClearTimer(JobTimeoutHandle);

	if (CompletionInfo.Job.GetObject() != CurrentJobObject)
	{
		UE_LOG(LogWorkflowInstance, Warning, TEXT("[%s] OnJobCompleted: unexpected job"), *Handle.Id.ToString());
		return;
	}

	StatusInfo.CurrentJob.Result = CompletionInfo.Result;
	StatusInfo.CurrentJob.ErrorMessage = CompletionInfo.ErrorMessage;
	StatusInfo.ErrorMessage = CompletionInfo.ErrorMessage;

	switch (CompletionInfo.Result)
	{
	case EJobResult::Success:
		BroadcastEvent(EWorkflowEventType::JobCompleted);
		AdvanceToNextJob();
		break;

	case EJobResult::Cancelled:
		FinishWorkflow(EWorkflowStatus::Cancelled);
		break;

	case EJobResult::Timeout:
		BroadcastEvent(EWorkflowEventType::JobTimeout);
		if (ShouldRetry(CompletionInfo.Result))
		{
			if (CurrentJobConfig.RetryDelaySeconds > 0.0f)
			{
				WorkflowHelpers::SetTimer(RetryHandle, CurrentJobConfig.RetryDelaySeconds,
					FTimerDelegate::CreateUObject(this, &UWorkflow::HandleRetryTimer));
			}
			else
			{
				RetryCurrentJob();
			}
		}
		else if (CurrentJobConfig.bContinueWorkflowOnFailure)
		{
			AdvanceToNextJob();
		}
		else
		{
			FinishWorkflow(EWorkflowStatus::Timeout, CompletionInfo.ErrorMessage);
		}
		break;

	case EJobResult::Failed:
	default:
		BroadcastEvent(EWorkflowEventType::JobFailed);
		if (ShouldRetry(CompletionInfo.Result))
		{
			if (CurrentJobConfig.RetryDelaySeconds > 0.0f)
			{
				WorkflowHelpers::SetTimer(RetryHandle, CurrentJobConfig.RetryDelaySeconds,
					FTimerDelegate::CreateUObject(this, &UWorkflow::HandleRetryTimer));
			}
			else
			{
				RetryCurrentJob();
			}
		}
		else if (CurrentJobConfig.bContinueWorkflowOnFailure)
		{
			UE_LOG(LogWorkflowInstance, Log, TEXT("[%s] Job %d failed but continuing workflow"), *Handle.Id.ToString(), StatusInfo.CurrentJob.Index);
			AdvanceToNextJob();
		}
		else
		{
			FinishWorkflow(EWorkflowStatus::Failed, CompletionInfo.ErrorMessage);
		}
		break;
	}
}

void UWorkflow::IReportJobProgress(const FJobProgressInfo& ProgressInfo)
{
	if (!IsRunning() || ProgressInfo.Job.GetObject() != CurrentJobObject) return;

	StatusInfo.CurrentJob.Progress = FMath::Clamp(ProgressInfo.Progress, 0.0f, 1.0f);
	StatusInfo.CurrentJob.ProgressText = ProgressInfo.ProgressText;
	BroadcastEvent(EWorkflowEventType::ProgressUpdated);
}

void UWorkflow::ExecuteCurrentJob()
{
	if (StatusInfo.CurrentJob.Index >= JobQueue.Num())
	{
		UE_LOG(LogWorkflowInstance, Error, TEXT("[%s] ExecuteCurrentJob: invalid index"), *Handle.Id.ToString());
		return;
	}

	if (StatusInfo.Status == EWorkflowStatus::Cancelling)
	{
		FinishWorkflow(EWorkflowStatus::Cancelled);
		return;
	}

	const TScriptInterface<IJobInterface>& JobInterface = JobQueue[StatusInfo.CurrentJob.Index];
	CurrentJobObject = JobInterface.GetObject();
	JobStartTime = FPlatformTime::Seconds();

	StatusInfo.CurrentJob.JobObject = JobInterface;
	StatusInfo.CurrentJob.Progress = 0.0f;
	CurrentJobConfig = GetEffectiveJobConfig();

	StatusInfo.CurrentJob.Name = CurrentJobConfig.Name.IsEmpty()
		? (CurrentJobObject ? CurrentJobObject->GetClass()->GetName() : TEXT("Unknown"))
		: CurrentJobConfig.Name;
	StatusInfo.CurrentJob.Result = EJobResult::InProgress;

	if (IJobInterface::Execute_IShouldSkip(CurrentJobObject, Context))
	{
		SkipCurrentJob();
		return;
	}

	UE_LOG(LogWorkflowInstance, Log, TEXT("[%s] Executing job %d/%d: %s (attempt %d)"),
		*Handle.Id.ToString(), StatusInfo.CurrentJob.Index + 1, StatusInfo.TotalJobs, *StatusInfo.CurrentJob.Name, StatusInfo.CurrentJob.RetryCount + 1);

	BroadcastEvent(EWorkflowEventType::JobStarted);

	if (CurrentJobConfig.TimeoutSeconds > 0.0f)
	{
		WorkflowHelpers::SetTimer(JobTimeoutHandle, CurrentJobConfig.TimeoutSeconds,
			FTimerDelegate::CreateUObject(this, &UWorkflow::HandleJobTimeout));
	}

	IJobInterface::Execute_IExecute(CurrentJobObject);
}

void UWorkflow::AdvanceToNextJob()
{
	StatusInfo.CurrentJob.Index++;
	StatusInfo.CurrentJob.RetryCount = 0;
	StatusInfo.CurrentJob.Progress = 0.0f;
	StatusInfo.CurrentJob.Name.Empty();
	StatusInfo.CurrentJob.JobObject = nullptr;
	StatusInfo.CurrentJob.ErrorMessage.Empty();
	StatusInfo.CurrentJob.Result = EJobResult::Pending;
	CurrentJobObject = nullptr;

	if (StatusInfo.CurrentJob.Index >= JobQueue.Num())
	{
		FinishWorkflow(EWorkflowStatus::Completed);
	}
	else if (StatusInfo.Status == EWorkflowStatus::Cancelling)
	{
		FinishWorkflow(EWorkflowStatus::Cancelled);
	}
	else
	{
		ExecuteCurrentJob();
	}
}

void UWorkflow::SkipCurrentJob()
{
	UE_LOG(LogWorkflowInstance, Log, TEXT("[%s] Skipping job %d/%d: %s (condition not met)"),
		*Handle.Id.ToString(), StatusInfo.CurrentJob.Index + 1, StatusInfo.TotalJobs, *StatusInfo.CurrentJob.Name);

	StatusInfo.CurrentJob.Result = EJobResult::Skipped;
	BroadcastEvent(EWorkflowEventType::JobSkipped);
	AdvanceToNextJob();
}

void UWorkflow::RetryCurrentJob()
{
	StatusInfo.CurrentJob.RetryCount++;
	StatusInfo.CurrentJob.Progress = 0.0f;

	UE_LOG(LogWorkflowInstance, Log, TEXT("[%s] Retrying job %d: %s (attempt %d/%d)"),
		*Handle.Id.ToString(), StatusInfo.CurrentJob.Index, *StatusInfo.CurrentJob.Name,
		StatusInfo.CurrentJob.RetryCount + 1, CurrentJobConfig.MaxRetries + 1);

	BroadcastEvent(EWorkflowEventType::JobRetrying);
	ExecuteCurrentJob();
}

void UWorkflow::HandleRetryTimer()
{
	if (!IsRunning()) return;
	RetryCurrentJob();
}

void UWorkflow::HandleJobTimeout()
{
	if (!IsRunning()) return;

	UE_LOG(LogWorkflowInstance, Warning, TEXT("[%s] Job %d timed out: %s"), *Handle.Id.ToString(), StatusInfo.CurrentJob.Index, *StatusInfo.CurrentJob.Name);

	if (CurrentJobObject && CurrentJobObject->GetClass()->ImplementsInterface(UJobInterface::StaticClass()))
	{
		IJobInterface::Execute_ICancel(CurrentJobObject, false);
	}

	StatusInfo.CurrentJob.Result = EJobResult::Timeout;
	StatusInfo.ErrorMessage = TEXT("Job timed out");
	BroadcastEvent(EWorkflowEventType::JobTimeout);

	if (ShouldRetry(EJobResult::Timeout))
	{
		if (CurrentJobConfig.RetryDelaySeconds > 0.0f)
		{
			WorkflowHelpers::SetTimer(RetryHandle, CurrentJobConfig.RetryDelaySeconds,
				FTimerDelegate::CreateUObject(this, &UWorkflow::HandleRetryTimer));
		}
		else
		{
			RetryCurrentJob();
		}
	}
	else if (CurrentJobConfig.bContinueWorkflowOnFailure)
	{
		AdvanceToNextJob();
	}
	else
	{
		FinishWorkflow(EWorkflowStatus::Timeout, TEXT("Job timed out"));
	}
}

void UWorkflow::HandleWorkflowTimeout()
{
	if (!IsRunning()) return;

	UE_LOG(LogWorkflowInstance, Warning, TEXT("[%s] Workflow timed out at job %d/%d: %s"),
		*Handle.Id.ToString(), StatusInfo.CurrentJob.Index + 1, StatusInfo.TotalJobs, *StatusInfo.CurrentJob.Name);

	if (CurrentJobObject && CurrentJobObject->GetClass()->ImplementsInterface(UJobInterface::StaticClass()))
	{
		IJobInterface::Execute_ICancel(CurrentJobObject, false);
	}

	FinishWorkflow(EWorkflowStatus::Timeout, TEXT("Workflow timed out"));
}

void UWorkflow::FinishWorkflow(const EWorkflowStatus FinalStatus, const FString& ErrorMessage)
{
	UE_LOG(LogWorkflowInstance, Log, TEXT("[%s] Workflow finished: %s (%.2fs)"),
		*Handle.Id.ToString(), *UEnum::GetValueAsString(FinalStatus), WorkflowHelpers::CalculateElapsedTime(WorkflowStartTime));

	WorkflowHelpers::ClearTimer(JobTimeoutHandle);
	WorkflowHelpers::ClearTimer(WorkflowTimeoutHandle);
	WorkflowHelpers::ClearTimer(RetryHandle);

	StatusInfo.Status = FinalStatus;
	StatusInfo.ErrorMessage = ErrorMessage;

	BroadcastEvent(WorkflowHelpers::GetWorkflowFinishEvent(FinalStatus));

	CurrentJobObject = nullptr;
	JobStartTime = 0.0;
}

void UWorkflow::ResetState()
{
	WorkflowHelpers::ClearTimer(JobTimeoutHandle);
	WorkflowHelpers::ClearTimer(WorkflowTimeoutHandle);
	WorkflowHelpers::ClearTimer(RetryHandle);

	JobQueue.Empty();
	Listeners.Empty();
	CurrentJobObject = nullptr;
	StatusInfo = FWorkflowStatusInfo();
	StatusInfo.Handle = Handle;
	WorkflowConfig = FWorkflowConfig();
	CurrentJobConfig = FJobConfig();
	JobStartTime = 0.0;
	WorkflowStartTime = 0.0;

	if (Context)
	{
		Context->Clear();
	}
}

FJobConfig UWorkflow::GetEffectiveJobConfig() const
{
	if (StatusInfo.CurrentJob.Index >= JobQueue.Num())
	{
		return WorkflowConfig.DefaultJobConfig;
	}

	const TScriptInterface<IJobInterface>& JobInterface = JobQueue[StatusInfo.CurrentJob.Index];
	if (const UObject* JobObject = JobInterface.GetObject())
	{
		const FJobConfig Config = IJobInterface::Execute_IGetJobConfig(JobObject);
		if (Config.bOverrideWorkflowDefaults)
		{
			return Config;
		}

		FJobConfig EffectiveConfig = WorkflowConfig.DefaultJobConfig;
		EffectiveConfig.Name = Config.Name;
		EffectiveConfig.Description = Config.Description;
		return EffectiveConfig;
	}

	return WorkflowConfig.DefaultJobConfig;
}

bool UWorkflow::ShouldRetry(const EJobResult Result) const
{
	if (StatusInfo.CurrentJob.RetryCount >= CurrentJobConfig.MaxRetries) return false;
	if (Result == EJobResult::Timeout && CurrentJobConfig.bRetryOnTimeout) return true;
	if (Result == EJobResult::Failed && CurrentJobConfig.bRetryOnFailure) return true;
	return false;
}
