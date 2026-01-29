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
	// Silent reset during shutdown - don't broadcast
	if (IsRunning())
	{
		ResetState(false);
	}
	
	Context = nullptr;
	
	UE_LOG(LogWorkflow, Log, TEXT("WorkflowSubsystem deinitialized"));
	
	Super::Deinitialize();
}

UWorkflowContext* UWorkflowSubsystem::GetContext() const
{
	return Context;
}

void UWorkflowSubsystem::OnJobCompleted(UObject* Job, const EJobResult Result, const FString& ErrorMessage)
{
	// Check if workflow is still running first - a timed-out job may complete after workflow finished
	if (!IsRunning())
	{
		UE_LOG(LogWorkflow, Verbose, TEXT("OnJobCompleted ignored - workflow is not running (job: %s completed after timeout/cancel)"),
			Job ? *Job->GetName() : TEXT("null"));
		return;
	}
	
	ClearJobTimeoutTimer();
	
	if (Job != CurrentJob)
	{
		UE_LOG(LogWorkflow, Warning, TEXT("OnJobCompleted called with unexpected job object. Expected: %s, Got: %s"),
			CurrentJob ? *CurrentJob->GetName() : TEXT("null"),
			Job ? *Job->GetName() : TEXT("null"));
		return;
	}
	
	UE_LOG(LogWorkflow, Log, TEXT("Job %d completed with result: %s"), 
		CurrentJobIndex, 
		Result == EJobResult::Success ? TEXT("Success") : 
		Result == EJobResult::Failed ? TEXT("Failed") : TEXT("Cancelled"));
	
	OnJobStatusChanged.Broadcast(CurrentJobIndex, Result, Job);
	
	switch (Result)
	{
	case EJobResult::Success:
		AdvanceToNextJob();
		break;
		
	case EJobResult::Failed:
		LastErrorMessage = ErrorMessage;
		FinishWorkflow(EWorkflowStatus::Failed, ErrorMessage);
		break;
		
	case EJobResult::Cancelled:
		FinishWorkflow(EWorkflowStatus::Cancelled);
		break;
	}
}

bool UWorkflowSubsystem::IsCancellationRequested() const
{
	return bCancellationRequested;
}

bool UWorkflowSubsystem::ExecuteWorkflow(const FWorkflowConfig& Config, const TArray<TScriptInterface<IJobInterface>>& Jobs)
{
	if (IsRunning())
	{
		UE_LOG(LogWorkflow, Warning, TEXT("Cannot start workflow - already running. Call CancelWorkflow(true) to force reset."));
		return false;
	}
	
	if (Jobs.Num() == 0)
	{
		UE_LOG(LogWorkflow, Warning, TEXT("Cannot start workflow - no jobs provided"));
		return false;
	}
	
	// Validate all jobs
	for (int32 i = 0; i < Jobs.Num(); i++)
	{
		UObject* JobObject = Jobs[i].GetObject();
		if (!JobObject)
		{
			UE_LOG(LogWorkflow, Error, TEXT("Job at index %d is null"), i);
			return false;
		}
		
		if (!JobObject->GetClass()->ImplementsInterface(UJobInterface::StaticClass()))
		{
			UE_LOG(LogWorkflow, Error, TEXT("Job at index %d (%s) does not implement IJobInterface"), 
				i, *JobObject->GetClass()->GetName());
			return false;
		}
	}
	
	ResetState(false);
	
	CurrentConfig = Config;
	JobQueue = Jobs;
	Status = EWorkflowStatus::Running;
	WorkflowStartTime = FPlatformTime::Seconds();
	
	UE_LOG(LogWorkflow, Log, TEXT("Starting workflow with %d jobs (JobTimeout: %.1fs, WorkflowTimeout: %.1fs)"), 
		JobQueue.Num(), Config.JobTimeoutSeconds, Config.WorkflowTimeoutSeconds);
	
	StartWorkflowTimeoutTimer();
	ExecuteCurrentJob();
	
	return true;
}

void UWorkflowSubsystem::CancelWorkflow(const bool bForce)
{
	if (!IsRunning())
	{
		UE_LOG(LogWorkflow, Log, TEXT("CancelWorkflow called but workflow is not running"));
		return;
	}
	
	if (bForce)
	{
		UE_LOG(LogWorkflow, Warning, TEXT("Force cancelling workflow - immediately resetting state"));
		ResetState(true);
	}
	else
	{
		if (bCancellationRequested)
		{
			UE_LOG(LogWorkflow, Log, TEXT("Cancellation already requested. Call CancelWorkflow(true) to force reset."));
			return;
		}
		
		UE_LOG(LogWorkflow, Log, TEXT("Graceful cancellation requested - notifying listeners"));
		bCancellationRequested = true;
		OnCancellationRequested.Broadcast();
	}
}

float UWorkflowSubsystem::GetProgress() const
{
	if (JobQueue.Num() == 0)
	{
		return 0.0f;
	}
	return static_cast<float>(CurrentJobIndex) / static_cast<float>(JobQueue.Num());
}

float UWorkflowSubsystem::GetCurrentJobElapsedTime() const
{
	if (!IsRunning() || CurrentJobStartTime <= 0.0)
	{
		return 0.0f;
	}
	return static_cast<float>(FPlatformTime::Seconds() - CurrentJobStartTime);
}

float UWorkflowSubsystem::GetWorkflowElapsedTime() const
{
	if (WorkflowStartTime <= 0.0)
	{
		return 0.0f;
	}
	return static_cast<float>(FPlatformTime::Seconds() - WorkflowStartTime);
}

void UWorkflowSubsystem::AdvanceToNextJob()
{
	CurrentJobIndex++;
	CurrentJob = nullptr;
	
	if (CurrentJobIndex < JobQueue.Num())
	{
		if (bCancellationRequested)
		{
			FinishWorkflow(EWorkflowStatus::Cancelled);
		}
		else
		{
			ExecuteCurrentJob();
		}
	}
	else
	{
		FinishWorkflow(EWorkflowStatus::Completed);
	}
}

void UWorkflowSubsystem::ExecuteCurrentJob()
{
	if (CurrentJobIndex >= JobQueue.Num())
	{
		UE_LOG(LogWorkflow, Error, TEXT("ExecuteCurrentJob called with invalid index"));
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
	
	UE_LOG(LogWorkflow, Log, TEXT("Executing job %d of %d: %s"), 
		CurrentJobIndex + 1, 
		JobQueue.Num(),
		CurrentJob ? *CurrentJob->GetClass()->GetName() : TEXT("Unknown"));
	
	StartJobTimeoutTimer();
	
	TScriptInterface<IWorkflowManagerInterface> ManagerInterface;
	ManagerInterface.SetObject(this);
	ManagerInterface.SetInterface(Cast<IWorkflowManagerInterface>(this));
	
	IJobInterface::Execute_Execute(JobInterface.GetObject(), ManagerInterface);
}

void UWorkflowSubsystem::FinishWorkflow(const EWorkflowStatus FinalStatus, const FString& ErrorMessage)
{
	const float TotalTime = GetWorkflowElapsedTime();
	
	UE_LOG(LogWorkflow, Log, TEXT("Workflow finished with status: %s (%.2fs)%s"), 
		FinalStatus == EWorkflowStatus::Completed ? TEXT("Completed") :
		FinalStatus == EWorkflowStatus::Failed ? TEXT("Failed") : TEXT("Cancelled"),
		TotalTime,
		ErrorMessage.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" - %s"), *ErrorMessage));
	
	// Clean up internal state (Context preserved so users can read job outputs)
	CleanupInternalState();
	
	Status = FinalStatus;
	LastErrorMessage = ErrorMessage;
	
	OnWorkflowCompleted.Broadcast(FinalStatus, ErrorMessage);
}

void UWorkflowSubsystem::CleanupInternalState()
{
	ClearAllTimers();
	JobQueue.Empty();
	CurrentJob = nullptr;
	CurrentJobIndex = 0;
	bCancellationRequested = false;
	CurrentJobStartTime = 0.0;
	WorkflowStartTime = 0.0;
	CurrentConfig = FWorkflowConfig();
}

void UWorkflowSubsystem::ResetState(const bool bBroadcastCancelled)
{
	const bool bWasRunning = IsRunning();
	
	CleanupInternalState();
	Status = EWorkflowStatus::Idle;
	LastErrorMessage.Empty();
	
	// Clear context for fresh start
	if (Context)
	{
		Context->Clear();
	}
	
	if (bBroadcastCancelled && bWasRunning)
	{
		OnWorkflowCompleted.Broadcast(EWorkflowStatus::Cancelled, TEXT("Workflow was force cancelled"));
	}
}

void UWorkflowSubsystem::ClearAllTimers()
{
	ClearJobTimeoutTimer();
	ClearWorkflowTimeoutTimer();
}

void UWorkflowSubsystem::StartJobTimeoutTimer()
{
	if (CurrentConfig.JobTimeoutSeconds <= 0.0f)
	{
		return;
	}
	
	FTimerManager* TimerManager = GetTimerManager();
	if (!TimerManager)
	{
		UE_LOG(LogWorkflow, Warning, TEXT("Cannot start job timeout timer - no valid world"));
		return;
	}
	
	ClearJobTimeoutTimer();
	
	TimerManager->SetTimer(
		JobTimeoutTimerHandle,
		this,
		&UWorkflowSubsystem::HandleJobTimeout,
		CurrentConfig.JobTimeoutSeconds,
		false
	);
	
	UE_LOG(LogWorkflow, Verbose, TEXT("Job timeout timer started: %.1fs"), CurrentConfig.JobTimeoutSeconds);
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
	if (!IsRunning())
	{
		return;
	}
	
	const float ElapsedTime = GetCurrentJobElapsedTime();
	
	UE_LOG(LogWorkflow, Warning, TEXT("Job %d timed out after %.2fs (limit: %.1fs)"), 
		CurrentJobIndex, ElapsedTime, CurrentConfig.JobTimeoutSeconds);
	
	OnJobTimedOut.Broadcast(CurrentJobIndex, CurrentJob);
	
	if (CurrentConfig.bContinueOnJobTimeout)
	{
		UE_LOG(LogWorkflow, Log, TEXT("Continuing workflow after job timeout (bContinueOnJobTimeout=true)"));
		OnJobStatusChanged.Broadcast(CurrentJobIndex, EJobResult::Failed, CurrentJob);
		AdvanceToNextJob();
	}
	else
	{
		FinishWorkflow(EWorkflowStatus::Failed, 
			FString::Printf(TEXT("Job %d timed out after %.1f seconds"), CurrentJobIndex, ElapsedTime));
	}
}

void UWorkflowSubsystem::StartWorkflowTimeoutTimer()
{
	if (CurrentConfig.WorkflowTimeoutSeconds <= 0.0f)
	{
		return;
	}
	
	FTimerManager* TimerManager = GetTimerManager();
	if (!TimerManager)
	{
		UE_LOG(LogWorkflow, Warning, TEXT("Cannot start workflow timeout timer - no valid world"));
		return;
	}
	
	ClearWorkflowTimeoutTimer();
	
	TimerManager->SetTimer(
		WorkflowTimeoutTimerHandle,
		this,
		&UWorkflowSubsystem::HandleWorkflowTimeout,
		CurrentConfig.WorkflowTimeoutSeconds,
		false
	);
	
	UE_LOG(LogWorkflow, Verbose, TEXT("Workflow timeout timer started: %.1fs"), CurrentConfig.WorkflowTimeoutSeconds);
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
	if (!IsRunning())
	{
		return;
	}
	
	const float ElapsedTime = GetWorkflowElapsedTime();
	
	UE_LOG(LogWorkflow, Warning, TEXT("Workflow timed out after %.2fs (limit: %.1fs) at job %d/%d"), 
		ElapsedTime, CurrentConfig.WorkflowTimeoutSeconds, CurrentJobIndex + 1, JobQueue.Num());
	
	FinishWorkflow(EWorkflowStatus::Failed, 
		FString::Printf(TEXT("Workflow timed out after %.1f seconds"), ElapsedTime));
}
