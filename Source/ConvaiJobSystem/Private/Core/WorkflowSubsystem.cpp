// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Core/WorkflowSubsystem.h"
#include "Core/WorkflowContext.h"
#include "Interface/JobInterface.h"
#include "Interface/WorkflowListenerInterface.h"
#include "Utility/WorkflowBlueprintLibrary.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorkflow, Log, All);

namespace 
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

void UWorkflowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Context = NewObject<UWorkflowContext>(this);
	UE_LOG(LogWorkflow, Log, TEXT("WorkflowSubsystem initialized"));
}

void UWorkflowSubsystem::Deinitialize()
{
	if (IsRunning())
	{
		ClearTimer(JobTimeoutHandle);
		ClearTimer(WorkflowTimeoutHandle);
		ClearTimer(RetryHandle);
	}
	Listeners.Empty();
	Context = nullptr;
	UE_LOG(LogWorkflow, Log, TEXT("WorkflowSubsystem deinitialized"));
	Super::Deinitialize();
}

void UWorkflowSubsystem::IAddListener(const TScriptInterface<IWorkflowListenerInterface>& Listener)
{
	if (Listener.GetObject() && !Listeners.Contains(Listener))
	{
		Listeners.Add(Listener);
	}
}

void UWorkflowSubsystem::IRemoveListener(const TScriptInterface<IWorkflowListenerInterface>& Listener)
{
	Listeners.Remove(Listener);
}

void UWorkflowSubsystem::UpdateComputedFields()
{
	StatusInfo.ElapsedTime = CalculateElapsedTime(WorkflowStartTime);
	StatusInfo.Progress = CalculateProgress(StatusInfo.CurrentJob.Index, StatusInfo.TotalJobs, StatusInfo.CurrentJob.Progress);
	StatusInfo.CurrentJob.ElapsedTime = CalculateElapsedTime(JobStartTime);
}

FWorkflowStatusInfo UWorkflowSubsystem::IGetStatusInfo()
{
	UpdateComputedFields();
	return StatusInfo;
}

void UWorkflowSubsystem::BroadcastEvent(const EWorkflowEventType EventType)
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

bool UWorkflowSubsystem::IExecuteWorkflowFromJobDefinitions(const FWorkflowRequestFromJobDefinitions& Request)
{
	if (IsRunning())
	{
		UE_LOG(LogWorkflow, Warning, TEXT("Cannot start workflow - already running"));
		return false;
	}
	
	FWorkflowRequestFromJobs ConvertedRequest;
	ConvertedRequest.Options = Request.Options;
	
	if (!UWorkflowBlueprintLibrary::CreateJobsFromDefinitions(this, Request.JobDefinitions, ConvertedRequest.Jobs))
	{
		UE_LOG(LogWorkflow, Error, TEXT("Failed to create jobs from definitions"));
		return false;
	}
	
	return IExecuteWorkflowFromJobs(ConvertedRequest);
}

bool UWorkflowSubsystem::IExecuteWorkflowFromJobs(const FWorkflowRequestFromJobs& Request)
{
	if (IsRunning())
	{
		UE_LOG(LogWorkflow, Warning, TEXT("Cannot start workflow - already running"));
		return false;
	}
	
	if (Request.Jobs.Num() == 0)
	{
		UE_LOG(LogWorkflow, Warning, TEXT("Cannot start workflow - no jobs provided"));
		return false;
	}
	
	for (int32 i = 0; i < Request.Jobs.Num(); i++)
	{
		const UObject* JobObject = Request.Jobs[i].GetObject();
		if (!JobObject || !JobObject->GetClass()->ImplementsInterface(UJobInterface::StaticClass()))
		{
			UE_LOG(LogWorkflow, Error, TEXT("Job %d is invalid or does not implement IJobInterface"), i);
			return false;
		}
	}
	
	ResetState();
	
	for (const auto& Listener : Request.Options.Listeners)
	{
		IAddListener(Listener);
	}
	
	WorkflowConfig = Request.Options.WorkflowConfig;
	JobQueue = Request.Jobs;
	WorkflowStartTime = FPlatformTime::Seconds();
	
	StatusInfo.Status = EWorkflowStatus::Running;
	StatusInfo.TotalJobs = Request.Jobs.Num();
	StatusInfo.CurrentJob.Index = 0;
	
	UE_LOG(LogWorkflow, Log, TEXT("Starting workflow with %d jobs"), JobQueue.Num());
	BroadcastEvent(EWorkflowEventType::WorkflowStarted);
	
	if (WorkflowConfig.WorkflowTimeoutSeconds > 0.0f)
	{
		SetTimer(WorkflowTimeoutHandle, WorkflowConfig.WorkflowTimeoutSeconds, 
			FTimerDelegate::CreateUObject(this, &UWorkflowSubsystem::HandleWorkflowTimeout));
	}
	
	ExecuteCurrentJob();
	return true;
}

void UWorkflowSubsystem::ICancelWorkflow(const bool bForce)
{
	if (!IsRunning()) return;
	
	UE_LOG(LogWorkflow, Log, TEXT("Cancelling workflow (force=%s)"), bForce ? TEXT("true") : TEXT("false"));
	
	if (bForce)
	{
		if (CurrentJobObject && CurrentJobObject->GetClass()->ImplementsInterface(UJobInterface::StaticClass()))
		{
			IJobInterface::Execute_ICancel(CurrentJobObject, true);
		}
		FinishWorkflow(EWorkflowStatus::Cancelled, TEXT("Workflow was force cancelled"));
		return;
	}
	
	if (StatusInfo.Status == EWorkflowStatus::Cancelling) return;
	
	StatusInfo.Status = EWorkflowStatus::Cancelling;
	
	if (CurrentJobObject && CurrentJobObject->GetClass()->ImplementsInterface(UJobInterface::StaticClass()))
	{
		IJobInterface::Execute_ICancel(CurrentJobObject, false);
	}
}

void UWorkflowSubsystem::IOnJobCompleted(const FJobCompletionInfo& CompletionInfo)
{
	if (!IsRunning())
	{
		UE_LOG(LogWorkflow, Verbose, TEXT("OnJobCompleted ignored - workflow not running"));
		return;
	}
	
	ClearTimer(JobTimeoutHandle);
	
	if (CompletionInfo.Job.GetObject() != CurrentJobObject)
	{
		UE_LOG(LogWorkflow, Warning, TEXT("OnJobCompleted: unexpected job"));
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
				SetTimer(RetryHandle, CurrentJobConfig.RetryDelaySeconds, 
					FTimerDelegate::CreateUObject(this, &UWorkflowSubsystem::HandleRetryTimer));
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
				SetTimer(RetryHandle, CurrentJobConfig.RetryDelaySeconds, 
					FTimerDelegate::CreateUObject(this, &UWorkflowSubsystem::HandleRetryTimer));
			}
			else
			{
				RetryCurrentJob();
			}
		}
		else if (CurrentJobConfig.bContinueWorkflowOnFailure)
		{
			UE_LOG(LogWorkflow, Log, TEXT("Job %d failed but continuing workflow"), StatusInfo.CurrentJob.Index);
			AdvanceToNextJob();
		}
		else
		{
			FinishWorkflow(EWorkflowStatus::Failed, CompletionInfo.ErrorMessage);
		}
		break;
	}
}

void UWorkflowSubsystem::IReportJobProgress(const FJobProgressInfo& ProgressInfo)
{
	if (!IsRunning() || ProgressInfo.Job.GetObject() != CurrentJobObject) return;
	
	StatusInfo.CurrentJob.Progress = FMath::Clamp(ProgressInfo.Progress, 0.0f, 1.0f);
	StatusInfo.CurrentJob.ProgressText = ProgressInfo.ProgressText;
	BroadcastEvent(EWorkflowEventType::ProgressUpdated);
}

void UWorkflowSubsystem::ExecuteCurrentJob()
{
	if (StatusInfo.CurrentJob.Index >= JobQueue.Num())
	{
		UE_LOG(LogWorkflow, Error, TEXT("ExecuteCurrentJob: invalid index"));
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
	
	UE_LOG(LogWorkflow, Log, TEXT("Executing job %d/%d: %s (attempt %d)"), 
		StatusInfo.CurrentJob.Index + 1, StatusInfo.TotalJobs, *StatusInfo.CurrentJob.Name, StatusInfo.CurrentJob.RetryCount + 1);
	
	BroadcastEvent(EWorkflowEventType::JobStarted);
	
	if (CurrentJobConfig.TimeoutSeconds > 0.0f)
	{
		SetTimer(JobTimeoutHandle, CurrentJobConfig.TimeoutSeconds, 
			FTimerDelegate::CreateUObject(this, &UWorkflowSubsystem::HandleJobTimeout));
	}
	
	TScriptInterface<IWorkflowManagerInterface> ManagerInterface;
	ManagerInterface.SetObject(this);
	ManagerInterface.SetInterface(Cast<IWorkflowManagerInterface>(this));
	IJobInterface::Execute_IExecute(CurrentJobObject, ManagerInterface);
}

void UWorkflowSubsystem::AdvanceToNextJob()
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

void UWorkflowSubsystem::SkipCurrentJob()
{
	UE_LOG(LogWorkflow, Log, TEXT("Skipping job %d/%d: %s (condition not met)"), 
		StatusInfo.CurrentJob.Index + 1, StatusInfo.TotalJobs, *StatusInfo.CurrentJob.Name);
	
	StatusInfo.CurrentJob.Result = EJobResult::Skipped;
	BroadcastEvent(EWorkflowEventType::JobSkipped);
	AdvanceToNextJob();
}

void UWorkflowSubsystem::RetryCurrentJob()
{
	StatusInfo.CurrentJob.RetryCount++;
	StatusInfo.CurrentJob.Progress = 0.0f;
	
	UE_LOG(LogWorkflow, Log, TEXT("Retrying job %d: %s (attempt %d/%d)"), 
		StatusInfo.CurrentJob.Index, *StatusInfo.CurrentJob.Name, 
		StatusInfo.CurrentJob.RetryCount + 1, CurrentJobConfig.MaxRetries + 1);
	
	BroadcastEvent(EWorkflowEventType::JobRetrying);
	ExecuteCurrentJob();
}

void UWorkflowSubsystem::HandleRetryTimer()
{
	if (!IsRunning()) return;
	RetryCurrentJob();
}

void UWorkflowSubsystem::HandleJobTimeout()
{
	if (!IsRunning()) return;
	
	UE_LOG(LogWorkflow, Warning, TEXT("Job %d timed out: %s"), StatusInfo.CurrentJob.Index, *StatusInfo.CurrentJob.Name);
	
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
			SetTimer(RetryHandle, CurrentJobConfig.RetryDelaySeconds, 
				FTimerDelegate::CreateUObject(this, &UWorkflowSubsystem::HandleRetryTimer));
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

void UWorkflowSubsystem::HandleWorkflowTimeout()
{
	if (!IsRunning()) return;
	
	UE_LOG(LogWorkflow, Warning, TEXT("Workflow timed out at job %d/%d: %s"), 
		StatusInfo.CurrentJob.Index + 1, StatusInfo.TotalJobs, *StatusInfo.CurrentJob.Name);
	
	if (CurrentJobObject && CurrentJobObject->GetClass()->ImplementsInterface(UJobInterface::StaticClass()))
	{
		IJobInterface::Execute_ICancel(CurrentJobObject, false);
	}
	
	FinishWorkflow(EWorkflowStatus::Timeout, TEXT("Workflow timed out"));
}

void UWorkflowSubsystem::FinishWorkflow(const EWorkflowStatus FinalStatus, const FString& ErrorMessage)
{
	UE_LOG(LogWorkflow, Log, TEXT("Workflow finished: %s (%.2fs)"), 
		*UEnum::GetValueAsString(FinalStatus), CalculateElapsedTime(WorkflowStartTime));
	
	ClearTimer(JobTimeoutHandle);
	ClearTimer(WorkflowTimeoutHandle);
	ClearTimer(RetryHandle);
	
	StatusInfo.Status = FinalStatus;
	StatusInfo.ErrorMessage = ErrorMessage;
	
	BroadcastEvent(GetWorkflowFinishEvent(FinalStatus));
	
	JobQueue.Empty();
	CurrentJobObject = nullptr;
	JobStartTime = 0.0;
	WorkflowStartTime = 0.0;
	WorkflowConfig = FWorkflowConfig();
	CurrentJobConfig = FJobConfig();
}

void UWorkflowSubsystem::ResetState()
{
	ClearTimer(JobTimeoutHandle);
	ClearTimer(WorkflowTimeoutHandle);
	ClearTimer(RetryHandle);
	
	JobQueue.Empty();
	CurrentJobObject = nullptr;
	StatusInfo = FWorkflowStatusInfo();
	WorkflowConfig = FWorkflowConfig();
	CurrentJobConfig = FJobConfig();
	JobStartTime = 0.0;
	WorkflowStartTime = 0.0;
	
	if (Context)
	{
		Context->Clear();
	}
}

FJobConfig UWorkflowSubsystem::GetEffectiveJobConfig() const
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

bool UWorkflowSubsystem::ShouldRetry(const EJobResult Result) const
{
	if (StatusInfo.CurrentJob.RetryCount >= CurrentJobConfig.MaxRetries) return false;
	if (Result == EJobResult::Timeout && CurrentJobConfig.bRetryOnTimeout) return true;
	if (Result == EJobResult::Failed && CurrentJobConfig.bRetryOnFailure) return true;
	return false;
}
