// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Core/WorkflowSubsystem.h"
#include "Core/WorkflowContext.h"
#include "Interface/JobInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorkflow, Log, All);

void UWorkflowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	// Create the shared context
	Context = NewObject<UWorkflowContext>(this);
	
	UE_LOG(LogWorkflow, Log, TEXT("WorkflowSubsystem initialized"));
}

void UWorkflowSubsystem::Deinitialize()
{
	// Cancel any running workflow
	if (IsRunning())
	{
		CancelWorkflow();
	}
	
	Context = nullptr;
	JobQueue.Empty();
	
	UE_LOG(LogWorkflow, Log, TEXT("WorkflowSubsystem deinitialized"));
	
	Super::Deinitialize();
}

UWorkflowContext* UWorkflowSubsystem::GetContext() const
{
	return Context;
}

void UWorkflowSubsystem::OnJobCompleted(UObject* Job, const EJobResult Result, const FString& ErrorMessage)
{
	// Validate this is the current job
	if (Job != CurrentJob)
	{
		UE_LOG(LogWorkflow, Warning, TEXT("NotifyJobCompleted called with unexpected job object"));
		return;
	}
	
	if (Status != EWorkflowStatus::Running)
	{
		UE_LOG(LogWorkflow, Warning, TEXT("NotifyJobCompleted called but workflow is not running"));
		return;
	}
	
	UE_LOG(LogWorkflow, Log, TEXT("Job %d completed with result: %s"), 
		CurrentJobIndex, 
		Result == EJobResult::Success ? TEXT("Success") : 
		Result == EJobResult::Failed ? TEXT("Failed") : TEXT("Cancelled"));
	
	// Broadcast job completion
	OnJobStatusChanged.Broadcast(CurrentJobIndex, Result, Job);
	
	// Handle result
	if (Result == EJobResult::Success)
	{
		// Move to next job
		CurrentJobIndex++;
		CurrentJob = nullptr;
		
		if (CurrentJobIndex < JobQueue.Num())
		{
			// Execute next job
			ExecuteCurrentJob();
		}
		else
		{
			// All jobs completed successfully
			FinishWorkflow(EWorkflowStatus::Completed);
		}
	}
	else if (Result == EJobResult::Failed)
	{
		LastErrorMessage = ErrorMessage;
		FinishWorkflow(EWorkflowStatus::Failed, ErrorMessage);
	}
	else // Cancelled
	{
		FinishWorkflow(EWorkflowStatus::Cancelled);
	}
}

bool UWorkflowSubsystem::IsCancellationRequested() const
{
	return bCancellationRequested;
}

bool UWorkflowSubsystem::ExecuteWorkflow(const TArray<TScriptInterface<IJobInterface>>& Jobs)
{
	// Check if already running
	if (IsRunning())
	{
		UE_LOG(LogWorkflow, Warning, TEXT("Cannot start workflow - already running"));
		return false;
	}
	
	// Validate jobs
	if (Jobs.Num() == 0)
	{
		UE_LOG(LogWorkflow, Warning, TEXT("Cannot start workflow - no jobs provided"));
		return false;
	}
	
	// Validate all jobs implement the interface
	for (int32 i = 0; i < Jobs.Num(); i++)
	{
		UObject* JobObject = Jobs[i].GetObject();
		if (!JobObject)
		{
			UE_LOG(LogWorkflow, Error, TEXT("Job at index %d is null"), i);
			return false;
		}
		
		// Check if object implements IJobInterface (works for both C++ and Blueprint)
		if (!JobObject->GetClass()->ImplementsInterface(UJobInterface::StaticClass()))
		{
			UE_LOG(LogWorkflow, Error, TEXT("Job at index %d (%s) does not implement IJobInterface"), 
				i, *JobObject->GetClass()->GetName());
			return false;
		}
	}
	
	// Reset state
	ResetState();
	
	// Store jobs
	JobQueue = Jobs;
	Status = EWorkflowStatus::Running;
	
	UE_LOG(LogWorkflow, Log, TEXT("Starting workflow with %d jobs"), JobQueue.Num());
	
	// Start first job
	ExecuteCurrentJob();
	
	return true;
}

void UWorkflowSubsystem::CancelWorkflow()
{
	if (!IsRunning())
	{
		return;
	}
	
	UE_LOG(LogWorkflow, Log, TEXT("Cancellation requested"));
	bCancellationRequested = true;
	
	// Note: The current job should check IsCancellationRequested() and complete
	// with EJobResult::Canceled. If it doesn't respond, we don't force-kill it.
}

float UWorkflowSubsystem::GetProgress() const
{
	if (JobQueue.Num() == 0)
	{
		return 0.0f;
	}
	
	// Simple progress: completed jobs / total jobs
	return static_cast<float>(CurrentJobIndex) / static_cast<float>(JobQueue.Num());
}

void UWorkflowSubsystem::ExecuteCurrentJob()
{
	if (CurrentJobIndex >= JobQueue.Num())
	{
		UE_LOG(LogWorkflow, Error, TEXT("ExecuteCurrentJob called with invalid index"));
		return;
	}
	
	// Check for cancellation before starting next job
	if (bCancellationRequested)
	{
		FinishWorkflow(EWorkflowStatus::Cancelled);
		return;
	}
	
	// Get job interface
	const TScriptInterface<IJobInterface>& JobInterface = JobQueue[CurrentJobIndex];
	CurrentJob = JobInterface.GetObject();
	
	UE_LOG(LogWorkflow, Log, TEXT("Executing job %d of %d"), CurrentJobIndex + 1, JobQueue.Num());
	
	// Execute the job, passing ourselves as the workflow manager interface
	TScriptInterface<IWorkflowManagerInterface> ManagerInterface;
	ManagerInterface.SetObject(this);
	ManagerInterface.SetInterface(Cast<IWorkflowManagerInterface>(this));
	
	IJobInterface::Execute_Execute(JobInterface.GetObject(), ManagerInterface);
}

void UWorkflowSubsystem::FinishWorkflow(EWorkflowStatus FinalStatus, const FString& ErrorMessage)
{
	UE_LOG(LogWorkflow, Log, TEXT("Workflow finished with status: %s%s"), 
		FinalStatus == EWorkflowStatus::Completed ? TEXT("Completed") :
		FinalStatus == EWorkflowStatus::Failed ? TEXT("Failed") : TEXT("Cancelled"),
		ErrorMessage.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" - %s"), *ErrorMessage));
	
	Status = FinalStatus;
	CurrentJob = nullptr;
	
	// Broadcast completion
	OnWorkflowCompleted.Broadcast(FinalStatus, ErrorMessage);
}

void UWorkflowSubsystem::ResetState()
{
	JobQueue.Empty();
	CurrentJob = nullptr;
	CurrentJobIndex = 0;
	bCancellationRequested = false;
	LastErrorMessage.Empty();
	
	// Clear context for new workflow
	if (Context)
	{
		Context->Clear();
	}
}
