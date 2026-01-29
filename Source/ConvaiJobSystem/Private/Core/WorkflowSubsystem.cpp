// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Core/WorkflowSubsystem.h"
#include "Core/WorkflowContext.h"
#include "Interface/JobInterface.h"
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
	Context = nullptr;
	UE_LOG(LogWorkflow, Log, TEXT("WorkflowSubsystem deinitialized"));
	Super::Deinitialize();
}

void UWorkflowSubsystem::OnJobCompleted(UObject* Job, const EJobResult Result, const FString& ErrorMessage)
{
	if (!IsRunning())
	{
		UE_LOG(LogWorkflow, Verbose, TEXT("OnJobCompleted ignored - workflow not running"));
		return;
	}
	
	ClearJobTimeoutTimer();
	
	if (Job != CurrentJob)
	{
		UE_LOG(LogWorkflow, Warning, TEXT("OnJobCompleted: unexpected job. Expected: %s, Got: %s"),
			CurrentJob ? *CurrentJob->GetName() : TEXT("null"),
			Job ? *Job->GetName() : TEXT("null"));
		return;
	}
	
	UE_LOG(LogWorkflow, Log, TEXT("Job %d completed: %s"), 
		CurrentJobIndex, 
		Result == EJobResult::Success ? TEXT("Success") : 
		Result == EJobResult::Failed ? TEXT("Failed") : TEXT("Cancelled"));
	
	OnJobStatusChanged.Broadcast(CurrentJobIndex, Result, Job);
	
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
	
	// Handle failure/timeout - check for retry
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
	
	// No retry - check if we should continue or fail workflow
	if (CurrentJobConfig.bContinueWorkflowOnFailure)
	{
		UE_LOG(LogWorkflow, Log, TEXT("Job %d failed but continuing workflow"), CurrentJobIndex);
		AdvanceToNextJob();
	}
	else
	{
		EWorkflowStatus FinalStatus = (Result == EJobResult::Timeout) ? EWorkflowStatus::Timeout : EWorkflowStatus::Failed;
		FinishWorkflow(FinalStatus, ErrorMessage);
	}
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
		if (!JobObject)
		{
			UE_LOG(LogWorkflow, Error, TEXT("Job at index %d is null"), i);
			return false;
		}
		
		if (!JobObject->GetClass()->ImplementsInterface(UJobInterface::StaticClass()))
		{
			UE_LOG(LogWorkflow, Error, TEXT("Job %d (%s) does not implement IJobInterface"), i, *JobObject->GetClass()->GetName());
			return false;
		}
	}
	
	ResetState(false);
	WorkflowConfig = Config;
	JobQueue = Jobs;
	WorkflowStartTime = FPlatformTime::Seconds();
	
	UE_LOG(LogWorkflow, Log, TEXT("Starting workflow with %d jobs"), JobQueue.Num());
	
	Status = EWorkflowStatus::Running;
	OnWorkflowStatusChanged.Broadcast(Status, TEXT(""));
	
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
		ResetState(true);
		return;
	}
	
	if (bCancellationRequested)
	{
		UE_LOG(LogWorkflow, Log, TEXT("Cancellation already requested"));
		return;
	}
	
	bCancellationRequested = true;
	Status = EWorkflowStatus::CancellationRequested;
	OnWorkflowStatusChanged.Broadcast(Status, TEXT(""));
}

float UWorkflowSubsystem::GetProgress() const
{
	return JobQueue.Num() > 0 ? static_cast<float>(CurrentJobIndex) / static_cast<float>(JobQueue.Num()) : 0.0f;
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

void UWorkflowSubsystem::RetryCurrentJob()
{
	CurrentRetryCount++;
	UE_LOG(LogWorkflow, Log, TEXT("Retrying job %d (attempt %d/%d)"), 
		CurrentJobIndex, CurrentRetryCount + 1, CurrentJobConfig.MaxRetries + 1);
	
	OnJobRetry.Broadcast(CurrentJobIndex, CurrentRetryCount);
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
	CurrentJobConfig = GetCurrentJobConfig();
	
	UE_LOG(LogWorkflow, Log, TEXT("Executing job %d/%d: %s"), 
		CurrentJobIndex + 1, JobQueue.Num(),
		CurrentJob ? *CurrentJob->GetClass()->GetName() : TEXT("Unknown"));
	
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
	Status = FinalStatus;
	LastErrorMessage = ErrorMessage;
	OnWorkflowStatusChanged.Broadcast(FinalStatus, ErrorMessage);
}

void UWorkflowSubsystem::CleanupInternalState()
{
	ClearAllTimers();
	JobQueue.Empty();
	CurrentJob = nullptr;
	CurrentJobIndex = 0;
	CurrentRetryCount = 0;
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
		Status = EWorkflowStatus::Cancelled;
		OnWorkflowStatusChanged.Broadcast(Status, TEXT("Workflow was force cancelled"));
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
	if (CurrentRetryCount >= Config.MaxRetries)
	{
		return false;
	}
	
	if (Result == EJobResult::Timeout && Config.bRetryOnTimeout)
	{
		return true;
	}
	
	if (Result == EJobResult::Failed && Config.bRetryOnFailure)
	{
		return true;
	}
	
	return false;
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
	
	UE_LOG(LogWorkflow, Warning, TEXT("Job %d timed out"), CurrentJobIndex);
	OnJobStatusChanged.Broadcast(CurrentJobIndex, EJobResult::Timeout, CurrentJob);
	
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
	
	UE_LOG(LogWorkflow, Warning, TEXT("Workflow timed out at job %d/%d"), CurrentJobIndex + 1, JobQueue.Num());
	FinishWorkflow(EWorkflowStatus::Timeout, TEXT("Workflow timed out"));
}
