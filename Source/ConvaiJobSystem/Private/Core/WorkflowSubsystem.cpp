// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Core/WorkflowSubsystem.h"
#include "Core/WorkflowContext.h"
#include "Interface/JobInterface.h"
#include "Interface/WorkflowListenerInterface.h"
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
		ResetState(false);
	}
	Listeners.Empty();
	Context = nullptr;
	UE_LOG(LogWorkflow, Log, TEXT("WorkflowSubsystem deinitialized"));
	Super::Deinitialize();
}

void UWorkflowSubsystem::AddListener(const TScriptInterface<IWorkflowListenerInterface>& Listener)
{
	if (Listener.GetObject() && !Listeners.Contains(Listener))
	{
		Listeners.Add(Listener);
	}
}

void UWorkflowSubsystem::RemoveListener(const TScriptInterface<IWorkflowListenerInterface>& Listener)
{
	Listeners.Remove(Listener);
}

void UWorkflowSubsystem::NotifyListenersWorkflowStatus(const FWorkflowStatusEvent& Event)
{
	for (const auto& Listener : Listeners)
	{
		if (UObject* ListenerObj = Listener.GetObject())
		{
			IWorkflowListenerInterface::Execute_OnWorkflowStatusChanged(ListenerObj, Event);
		}
	}
	
	OnWorkflowStatusChangedDelegate.Broadcast(Event);
}

void UWorkflowSubsystem::NotifyListenersWorkflowProgress(const FWorkflowProgressEvent& Event)
{
	for (const auto& Listener : Listeners)
	{
		if (UObject* ListenerObj = Listener.GetObject())
		{
			IWorkflowListenerInterface::Execute_OnWorkflowProgressChanged(ListenerObj, Event);
		}
	}
	
	OnWorkflowProgressChangedDelegate.Broadcast(Event);
}

void UWorkflowSubsystem::NotifyListenersJobStatus(const FJobStatusEvent& Event)
{
	for (const auto& Listener : Listeners)
	{
		if (UObject* ListenerObj = Listener.GetObject())
		{
			IWorkflowListenerInterface::Execute_OnJobStatusChanged(ListenerObj, Event);
		}
	}
	
	OnJobStatusChangedDelegate.Broadcast(Event);
}

void UWorkflowSubsystem::SetStatus(const EWorkflowStatus NewStatus, const FString& Message)
{
	if (Status == NewStatus) return;
	
	Status = NewStatus;
	LastErrorMessage = Message;
	
	FWorkflowStatusEvent Event;
	Event.Status = NewStatus;
	Event.Message = Message;
	Event.ElapsedTime = GetWorkflowElapsedTime();
	
	NotifyListenersWorkflowStatus(Event);
}

void UWorkflowSubsystem::OnJobCompleted(const TScriptInterface<IJobInterface>& Job, const EJobResult Result, const FString& ErrorMessage)
{
	if (!IsRunning())
	{
		UE_LOG(LogWorkflow, Verbose, TEXT("OnJobCompleted ignored - workflow not running"));
		return;
	}
	
	ClearJobTimeoutTimer();

	if (UObject* JobObject = Job.GetObject(); JobObject != CurrentJob)
	{
		UE_LOG(LogWorkflow, Warning, TEXT("OnJobCompleted: unexpected job"));
		return;
	}
	
	BroadcastJobStatus(Result, ErrorMessage);
	
	if (Result == EJobResult::Success)
	{
		AdvanceToNextJob();
		return;
	}
	
	if (Result == EJobResult::Cancelled)
	{
		FinishWorkflow(EWorkflowStatus::Cancelled);
		return;
	}
	
	if (ShouldRetry(Result, CurrentJobConfig))
	{
		if (CurrentJobConfig.RetryDelaySeconds > 0.0f)
		{
			ScheduleRetry(CurrentJobConfig.RetryDelaySeconds);
		}
		else
		{
			RetryCurrentJob();
		}
		return;
	}
	
	if (CurrentJobConfig.bContinueWorkflowOnFailure)
	{
		UE_LOG(LogWorkflow, Log, TEXT("Job %d failed but continuing workflow"), CurrentJobIndex);
		AdvanceToNextJob();
	}
	else
	{
		const EWorkflowStatus FinalStatus = (Result == EJobResult::Timeout) ? EWorkflowStatus::Timeout : EWorkflowStatus::Failed;
		FinishWorkflow(FinalStatus, ErrorMessage);
	}
}

void UWorkflowSubsystem::ReportJobProgress(const TScriptInterface<IJobInterface>& Job, float Progress)
{
	if (!IsRunning() || Job.GetObject() != CurrentJob) return;
	
	CurrentJobProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
	BroadcastWorkflowProgress();
}

void UWorkflowSubsystem::BroadcastWorkflowProgress()
{
	FWorkflowProgressEvent Event;
	Event.Progress = GetProgress();
	Event.CurrentJobIndex = CurrentJobIndex;
	Event.TotalJobs = JobQueue.Num();
	Event.CurrentJobName = CurrentJobName;
	Event.CurrentJobProgress = CurrentJobProgress;
	
	NotifyListenersWorkflowProgress(Event);
}

bool UWorkflowSubsystem::ExecuteWorkflow(const FWorkflowConfig& Config, const TArray<TScriptInterface<IJobInterface>>& Jobs)
{
	if (IsRunning())
	{
		UE_LOG(LogWorkflow, Warning, TEXT("Cannot start workflow - already running"));
		return false;
	}
	
	if (Jobs.Num() == 0)
	{
		UE_LOG(LogWorkflow, Warning, TEXT("Cannot start workflow - no jobs provided"));
		return false;
	}
	
	for (int32 i = 0; i < Jobs.Num(); i++)
	{
		const UObject* JobObject = Jobs[i].GetObject();
		if (!JobObject || !JobObject->GetClass()->ImplementsInterface(UJobInterface::StaticClass()))
		{
			UE_LOG(LogWorkflow, Error, TEXT("Job %d is invalid or does not implement IJobInterface"), i);
			return false;
		}
	}
	
	ResetState(false);
	WorkflowConfig = Config;
	JobQueue = Jobs;
	WorkflowStartTime = FPlatformTime::Seconds();
	
	UE_LOG(LogWorkflow, Log, TEXT("Starting workflow with %d jobs"), JobQueue.Num());
	
	SetStatus(EWorkflowStatus::Running);
	StartWorkflowTimeoutTimer();
	ExecuteCurrentJob();
	
	return true;
}

void UWorkflowSubsystem::CancelWorkflow(const bool bForce)
{
	if (!IsRunning()) return;
	
	if (bForce)
	{
		UE_LOG(LogWorkflow, Warning, TEXT("Force cancelling workflow"));
		CancelCurrentJob(true);
		ResetState(true);
		return;
	}
	
	if (bCancellationRequested)
	{
		UE_LOG(LogWorkflow, Log, TEXT("Cancellation already requested"));
		return;
	}
	
	bCancellationRequested = true;
	SetStatus(EWorkflowStatus::CancellationRequested);
	CancelCurrentJob();
}

void UWorkflowSubsystem::CancelCurrentJob(bool bForce)
{
	if (!CurrentJob) return;
	
	if (CurrentJob->GetClass()->ImplementsInterface(UJobInterface::StaticClass()))
	{
		IJobInterface::Execute_Cancel(CurrentJob, bForce);
	}
}

float UWorkflowSubsystem::GetProgress() const
{
	if (JobQueue.Num() == 0) return 0.0f;
	
	float BaseProgress = static_cast<float>(CurrentJobIndex) / static_cast<float>(JobQueue.Num());
	float JobContribution = CurrentJobProgress / static_cast<float>(JobQueue.Num());
	return BaseProgress + JobContribution;
}

float UWorkflowSubsystem::GetCurrentJobElapsedTime() const
{
	return (IsRunning() && CurrentJobStartTime > 0.0) 
		? static_cast<float>(FPlatformTime::Seconds() - CurrentJobStartTime) 
		: 0.0f;
}

float UWorkflowSubsystem::GetWorkflowElapsedTime() const
{
	return WorkflowStartTime > 0.0 
		? static_cast<float>(FPlatformTime::Seconds() - WorkflowStartTime) 
		: 0.0f;
}

void UWorkflowSubsystem::AdvanceToNextJob()
{
	CurrentJobIndex++;
	CurrentRetryCount = 0;
	CurrentJobProgress = 0.0f;
	CurrentJobName.Empty();
	CurrentJob = nullptr;
	
	if (CurrentJobIndex >= JobQueue.Num())
	{
		FinishWorkflow(EWorkflowStatus::Completed);
	}
	else if (bCancellationRequested)
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
		CurrentJobIndex + 1, JobQueue.Num(), *CurrentJobName);
	
	BroadcastJobStatus(EJobResult::Skipped);
	AdvanceToNextJob();
}

void UWorkflowSubsystem::RetryCurrentJob()
{
	CurrentRetryCount++;
	CurrentJobProgress = 0.0f;
	UE_LOG(LogWorkflow, Log, TEXT("Retrying job %d: %s (attempt %d/%d)"), 
		CurrentJobIndex, *CurrentJobName, CurrentRetryCount + 1, CurrentJobConfig.MaxRetries + 1);
	
	BroadcastJobStatus(EJobResult::Retrying);
	ExecuteCurrentJob();
}

void UWorkflowSubsystem::ScheduleRetry(float DelaySeconds)
{
	FTimerManager* TimerManager = GetTimerManager();
	if (!TimerManager)
	{
		RetryCurrentJob();
		return;
	}
	
	UE_LOG(LogWorkflow, Log, TEXT("Scheduling retry for job %d in %.1fs"), CurrentJobIndex, DelaySeconds);
	TimerManager->SetTimer(RetryTimerHandle, this, &UWorkflowSubsystem::HandleRetryTimer, DelaySeconds, false);
}

void UWorkflowSubsystem::HandleRetryTimer()
{
	if (!IsRunning()) return;
	RetryCurrentJob();
}

void UWorkflowSubsystem::ExecuteCurrentJob()
{
	if (CurrentJobIndex >= JobQueue.Num())
	{
		UE_LOG(LogWorkflow, Error, TEXT("ExecuteCurrentJob: invalid index"));
		return;
	}
	
	if (bCancellationRequested)
	{
		FinishWorkflow(EWorkflowStatus::Cancelled);
		return;
	}
	
	const TScriptInterface<IJobInterface>& JobInterface = JobQueue[CurrentJobIndex];
	CurrentJob = JobInterface.GetObject();
	CurrentJobStartTime = FPlatformTime::Seconds();
	CurrentJobProgress = 0.0f;
	CurrentJobConfig = GetCurrentJobConfig();
	
	CurrentJobName = CurrentJobConfig.Name.IsEmpty() 
		? (CurrentJob ? CurrentJob->GetClass()->GetName() : TEXT("Unknown"))
		: CurrentJobConfig.Name;
	
	if (!IJobInterface::Execute_ShouldExecute(CurrentJob, Context))
	{
		SkipCurrentJob();
		return;
	}
	
	UE_LOG(LogWorkflow, Log, TEXT("Executing job %d/%d: %s (attempt %d)"), 
		CurrentJobIndex + 1, JobQueue.Num(), *CurrentJobName, CurrentRetryCount + 1);
	
	if (CurrentJobConfig.TimeoutSeconds > 0.0f)
	{
		StartJobTimeoutTimer(CurrentJobConfig.TimeoutSeconds);
	}
	
	TScriptInterface<IWorkflowManagerInterface> ManagerInterface;
	ManagerInterface.SetObject(this);
	ManagerInterface.SetInterface(Cast<IWorkflowManagerInterface>(this));
	IJobInterface::Execute_Execute(JobInterface.GetObject(), ManagerInterface);
}

void UWorkflowSubsystem::FinishWorkflow(const EWorkflowStatus FinalStatus, const FString& ErrorMessage)
{
	UE_LOG(LogWorkflow, Log, TEXT("Workflow finished: %s (%.2fs)"), 
		FinalStatus == EWorkflowStatus::Completed ? TEXT("Completed") :
		FinalStatus == EWorkflowStatus::Failed ? TEXT("Failed") : 
		FinalStatus == EWorkflowStatus::Timeout ? TEXT("Timeout") : TEXT("Cancelled"),
		GetWorkflowElapsedTime());
	
	CleanupInternalState();
	SetStatus(FinalStatus, ErrorMessage);
}

void UWorkflowSubsystem::CleanupInternalState()
{
	ClearAllTimers();
	JobQueue.Empty();
	CurrentJob = nullptr;
	CurrentJobIndex = 0;
	CurrentRetryCount = 0;
	CurrentJobProgress = 0.0f;
	CurrentJobName.Empty();
	bCancellationRequested = false;
	CurrentJobStartTime = 0.0;
	WorkflowStartTime = 0.0;
	WorkflowConfig = FWorkflowConfig();
	CurrentJobConfig = FJobConfig();
}

void UWorkflowSubsystem::ResetState(const bool bBroadcastCancelled)
{
	const bool bWasRunning = IsRunning();
	
	CleanupInternalState();
	LastErrorMessage.Empty();
	
	if (Context)
	{
		Context->Clear();
	}
	
	if (bBroadcastCancelled && bWasRunning)
	{
		SetStatus(EWorkflowStatus::Cancelled, TEXT("Workflow was force cancelled"));
	}
	else
	{
		Status = EWorkflowStatus::Idle;
	}
}

FJobConfig UWorkflowSubsystem::GetCurrentJobConfig() const
{
	if (CurrentJobIndex >= JobQueue.Num()) 
	{
		return WorkflowConfig.DefaultJobConfig;
	}
	
	const TScriptInterface<IJobInterface>& JobInterface = JobQueue[CurrentJobIndex];

	if (const UObject* JobObject = JobInterface.GetObject())
	{
		const FJobConfig Config = IJobInterface::Execute_GetJobConfig(JobObject);
		return Config.bUseWorkflowConfig ? WorkflowConfig.DefaultJobConfig : Config;
	}
	
	return WorkflowConfig.DefaultJobConfig;
}

bool UWorkflowSubsystem::ShouldRetry(const EJobResult Result, const FJobConfig& Config) const
{
	if (CurrentRetryCount >= Config.MaxRetries) return false;
	if (Result == EJobResult::Timeout && Config.bRetryOnTimeout) return true;
	if (Result == EJobResult::Failed && Config.bRetryOnFailure) return true;
	return false;
}

void UWorkflowSubsystem::BroadcastJobStatus(const EJobResult Result, const FString& ErrorMessage)
{
	FJobStatusEvent Event;
	Event.JobIndex = CurrentJobIndex;
	Event.Result = Result;
	Event.Job = CurrentJob;
	Event.JobName = CurrentJobName;
	Event.RetryCount = CurrentRetryCount;
	Event.ErrorMessage = ErrorMessage;
	
	NotifyListenersJobStatus(Event);
}

void UWorkflowSubsystem::ClearAllTimers()
{
	ClearJobTimeoutTimer();
	ClearWorkflowTimeoutTimer();
	
	if (RetryTimerHandle.IsValid())
	{
		if (FTimerManager* TimerManager = GetTimerManager())
		{
			TimerManager->ClearTimer(RetryTimerHandle);
		}
		RetryTimerHandle.Invalidate();
	}
}

void UWorkflowSubsystem::StartJobTimeoutTimer(const float TimeoutSeconds)
{
	FTimerManager* TimerManager = GetTimerManager();
	if (!TimerManager) return;
	
	ClearJobTimeoutTimer();
	TimerManager->SetTimer(JobTimeoutTimerHandle, this, &UWorkflowSubsystem::HandleJobTimeout, TimeoutSeconds, false);
}

void UWorkflowSubsystem::ClearJobTimeoutTimer()
{
	if (JobTimeoutTimerHandle.IsValid())
	{
		if (FTimerManager* TimerManager = GetTimerManager())
		{
			TimerManager->ClearTimer(JobTimeoutTimerHandle);
		}
		JobTimeoutTimerHandle.Invalidate();
	}
}

void UWorkflowSubsystem::HandleJobTimeout()
{
	if (!IsRunning()) return;
	
	UE_LOG(LogWorkflow, Warning, TEXT("Job %d timed out: %s"), CurrentJobIndex, *CurrentJobName);
	CancelCurrentJob();
	BroadcastJobStatus(EJobResult::Timeout, TEXT("Job timed out"));
	
	if (ShouldRetry(EJobResult::Timeout, CurrentJobConfig))
	{
		if (CurrentJobConfig.RetryDelaySeconds > 0.0f)
		{
			ScheduleRetry(CurrentJobConfig.RetryDelaySeconds);
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

void UWorkflowSubsystem::StartWorkflowTimeoutTimer()
{
	if (WorkflowConfig.WorkflowTimeoutSeconds <= 0.0f) return;
	
	FTimerManager* TimerManager = GetTimerManager();
	if (!TimerManager) return;
	
	ClearWorkflowTimeoutTimer();
	TimerManager->SetTimer(WorkflowTimeoutTimerHandle, this, &UWorkflowSubsystem::HandleWorkflowTimeout, WorkflowConfig.WorkflowTimeoutSeconds, false);
}

void UWorkflowSubsystem::ClearWorkflowTimeoutTimer()
{
	if (WorkflowTimeoutTimerHandle.IsValid())
	{
		if (FTimerManager* TimerManager = GetTimerManager())
		{
			TimerManager->ClearTimer(WorkflowTimeoutTimerHandle);
		}
		WorkflowTimeoutTimerHandle.Invalidate();
	}
}

void UWorkflowSubsystem::HandleWorkflowTimeout()
{
	if (!IsRunning()) return;
	
	UE_LOG(LogWorkflow, Warning, TEXT("Workflow timed out at job %d/%d: %s"), CurrentJobIndex + 1, JobQueue.Num(), *CurrentJobName);
	CancelCurrentJob();
	FinishWorkflow(EWorkflowStatus::Timeout, TEXT("Workflow timed out"));
}
