// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Examples/SampleWorkflowRunner.h"
#include "Examples/SampleAsyncJob.h"
#include "Core/WorkflowSubsystem.h"
#include "Core/WorkflowContext.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorkflowDemo, Log, All);

ASampleWorkflowRunner::ASampleWorkflowRunner()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ASampleWorkflowRunner::BeginPlay()
{
	Super::BeginPlay();

	// Get the workflow subsystem as our workflow manager
	if (UWorkflowSubsystem* Subsystem = GEngine->GetEngineSubsystem<UWorkflowSubsystem>())
	{
		WorkflowManager.SetObject(Subsystem);
		WorkflowManager.SetInterface(Subsystem);
	}
}

void ASampleWorkflowRunner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clean up if workflow is running
	if (IsWorkflowRunning())
	{
		CancelWorkflow(true);
	}

	// Clear references
	CreatedJobs.Empty();
	WorkflowManager = nullptr;

	Super::EndPlay(EndPlayReason);
}

void ASampleWorkflowRunner::RunDemoWorkflow()
{
	if (!WorkflowManager.GetInterface())
	{
		UE_LOG(LogWorkflowDemo, Error, TEXT("No workflow manager available!"));
		return;
	}

	if (IsWorkflowRunning())
	{
		UE_LOG(LogWorkflowDemo, Warning, TEXT("Workflow already running!"));
		return;
	}

	UE_LOG(LogWorkflowDemo, Log, TEXT("========================================"));
	UE_LOG(LogWorkflowDemo, Log, TEXT("Starting Demo Workflow"));
	UE_LOG(LogWorkflowDemo, Log, TEXT("========================================"));

	// Clear previous jobs
	CreatedJobs.Empty();

	// Job 1: Context Writer - Writes data that other jobs can read
	USampleContextWriterJob* ContextWriterJob = NewObject<USampleContextWriterJob>(this);
	ContextWriterJob->JobConfig.Name = TEXT("Context Writer");
	// Note: DataToWrite uses FGameplayTag keys - for this demo we skip context writing
	// In real usage, you would define GameplayTags and use them here

	TScriptInterface<IJobInterface> Job1;
	Job1.SetObject(ContextWriterJob);
	Job1.SetInterface(ContextWriterJob);
	CreatedJobs.Add(Job1);

	// Job 2: Async Job - Demonstrates progress reporting
	USampleAsyncJob* AsyncJob1 = NewObject<USampleAsyncJob>(this);
	AsyncJob1->JobConfig.Name = TEXT("Data Processing");
	AsyncJob1->TaskDurationSeconds = 3.0f;
	AsyncJob1->ProgressSteps = 6;

	TScriptInterface<IJobInterface> Job2;
	Job2.SetObject(AsyncJob1);
	Job2.SetInterface(AsyncJob1);
	CreatedJobs.Add(Job2);

	// Job 3: Conditional Job - Only runs if context key exists
	// Note: Since RequiredContextKey needs a valid FGameplayTag, this job will execute
	// (IShouldSkip returns false when no key is set)
	USampleConditionalJob* ConditionalJob = NewObject<USampleConditionalJob>(this);
	ConditionalJob->JobConfig.Name = TEXT("Conditional Task");
	// ConditionalJob->RequiredContextKey = ... // Would need a valid FGameplayTag

	TScriptInterface<IJobInterface> Job3;
	Job3.SetObject(ConditionalJob);
	Job3.SetInterface(ConditionalJob);
	CreatedJobs.Add(Job3);

	// Job 4: Failing Job - Demonstrates retry system
	USampleFailingJob* FailingJob = NewObject<USampleFailingJob>(this);
	FailingJob->JobConfig.Name = TEXT("Unreliable Service Call");
	FailingJob->FailureProbability = 0.7f; // 70% chance of failure
	FailingJob->ExecutionTimeSeconds = 0.2f;
	// Retry config is set in constructor

	TScriptInterface<IJobInterface> Job4;
	Job4.SetObject(FailingJob);
	Job4.SetInterface(FailingJob);
	CreatedJobs.Add(Job4);

	// Job 5: Another Async Job - Final step
	USampleAsyncJob* AsyncJob2 = NewObject<USampleAsyncJob>(this);
	AsyncJob2->JobConfig.Name = TEXT("Finalization");
	AsyncJob2->TaskDurationSeconds = 2.0f;
	AsyncJob2->ProgressSteps = 4;

	TScriptInterface<IJobInterface> Job5;
	Job5.SetObject(AsyncJob2);
	Job5.SetInterface(AsyncJob2);
	CreatedJobs.Add(Job5);

	// Configure the workflow
	FWorkflowRequest Request;
	Request.Config.WorkflowTimeoutSeconds = WorkflowTimeoutSeconds;
	Request.Config.DefaultJobConfig.MaxRetries = DefaultMaxRetries;
	Request.Config.DefaultJobConfig.RetryDelaySeconds = DefaultRetryDelaySeconds;
	Request.Config.DefaultJobConfig.bRetryOnFailure = true;
	Request.Jobs = CreatedJobs;

	// Add ourselves as a listener
	TScriptInterface<IWorkflowListenerInterface> Listener;
	Listener.SetObject(this);
	Listener.SetInterface(this);
	Request.Listeners.Add(Listener);

	// Start the workflow
	const bool bStarted = WorkflowManager->IExecuteWorkflow(Request);
	
	if (bStarted)
	{
		UE_LOG(LogWorkflowDemo, Log, TEXT("Workflow started successfully with %d jobs"), CreatedJobs.Num());
	}
	else
	{
		UE_LOG(LogWorkflowDemo, Error, TEXT("Failed to start workflow!"));
	}
}

void ASampleWorkflowRunner::CancelWorkflow(bool bForce)
{
	if (WorkflowManager.GetInterface())
	{
		UE_LOG(LogWorkflowDemo, Log, TEXT("Cancelling workflow (force=%s)"), bForce ? TEXT("true") : TEXT("false"));
		WorkflowManager->ICancelWorkflow(bForce);
	}
}

bool ASampleWorkflowRunner::IsWorkflowRunning() const
{
	if (IWorkflowManagerInterface* Interface = WorkflowManager.GetInterface())
	{
		const FWorkflowStatusInfo Status = Interface->IGetStatusInfo();
		return Status.Status == EWorkflowStatus::Running || Status.Status == EWorkflowStatus::Cancelling;
	}
	return false;
}

void ASampleWorkflowRunner::IOnWorkflowEvent_Implementation(EWorkflowEventType EventType, const FWorkflowStatusInfo& StatusInfo)
{
	LogWorkflowEvent(EventType, StatusInfo);
	OnWorkflowEventReceived.Broadcast(EventType, StatusInfo);
}

void ASampleWorkflowRunner::LogWorkflowEvent(EWorkflowEventType EventType, const FWorkflowStatusInfo& StatusInfo)
{
	const FString EventName = GetEventTypeName(EventType);
	const FString JobName = StatusInfo.CurrentJob.Name.IsEmpty() ? TEXT("N/A") : StatusInfo.CurrentJob.Name;

	switch (EventType)
	{
	case EWorkflowEventType::WorkflowStarted:
		UE_LOG(LogWorkflowDemo, Log, TEXT("[%s] Total jobs: %d"), *EventName, StatusInfo.TotalJobs);
		break;

	case EWorkflowEventType::WorkflowCompleted:
		UE_LOG(LogWorkflowDemo, Log, TEXT("[%s] Finished in %.2f seconds"), *EventName, StatusInfo.ElapsedTime);
		UE_LOG(LogWorkflowDemo, Log, TEXT("========================================"));
		break;

	case EWorkflowEventType::WorkflowFailed:
	case EWorkflowEventType::WorkflowTimeout:
	case EWorkflowEventType::WorkflowCancelled:
		UE_LOG(LogWorkflowDemo, Warning, TEXT("[%s] %s"), *EventName, *StatusInfo.ErrorMessage);
		break;

	case EWorkflowEventType::JobStarted:
		UE_LOG(LogWorkflowDemo, Log, TEXT("[%s] Job %d/%d: %s"), 
			*EventName, 
			StatusInfo.CurrentJob.Index + 1, 
			StatusInfo.TotalJobs, 
			*JobName);
		break;

	case EWorkflowEventType::JobCompleted:
		UE_LOG(LogWorkflowDemo, Log, TEXT("[%s] %s completed in %.2fs"), 
			*EventName, 
			*JobName, 
			StatusInfo.CurrentJob.ElapsedTime);
		break;

	case EWorkflowEventType::JobFailed:
		UE_LOG(LogWorkflowDemo, Warning, TEXT("[%s] %s failed: %s"), 
			*EventName, 
			*JobName, 
			*StatusInfo.CurrentJob.ErrorMessage);
		break;

	case EWorkflowEventType::JobRetrying:
		UE_LOG(LogWorkflowDemo, Log, TEXT("[%s] %s retry attempt %d"), 
			*EventName, 
			*JobName, 
			StatusInfo.CurrentJob.RetryCount + 1);
		break;

	case EWorkflowEventType::JobSkipped:
		UE_LOG(LogWorkflowDemo, Log, TEXT("[%s] %s skipped (condition not met)"), 
			*EventName, 
			*JobName);
		break;

	case EWorkflowEventType::ProgressUpdated:
		UE_LOG(LogWorkflowDemo, Verbose, TEXT("[%s] %s: %.0f%% - %s"), 
			*EventName, 
			*JobName, 
			StatusInfo.CurrentJob.Progress * 100.0f,
			*StatusInfo.CurrentJob.ProgressText.ToString());
		break;

	default:
		UE_LOG(LogWorkflowDemo, Log, TEXT("[%s]"), *EventName);
		break;
	}
}

FString ASampleWorkflowRunner::GetEventTypeName(EWorkflowEventType EventType) const
{
	switch (EventType)
	{
	case EWorkflowEventType::WorkflowStarted: return TEXT("WORKFLOW_STARTED");
	case EWorkflowEventType::WorkflowCompleted: return TEXT("WORKFLOW_COMPLETED");
	case EWorkflowEventType::WorkflowFailed: return TEXT("WORKFLOW_FAILED");
	case EWorkflowEventType::WorkflowCancelled: return TEXT("WORKFLOW_CANCELLED");
	case EWorkflowEventType::WorkflowTimeout: return TEXT("WORKFLOW_TIMEOUT");
	case EWorkflowEventType::JobStarted: return TEXT("JOB_STARTED");
	case EWorkflowEventType::JobCompleted: return TEXT("JOB_COMPLETED");
	case EWorkflowEventType::JobFailed: return TEXT("JOB_FAILED");
	case EWorkflowEventType::JobTimeout: return TEXT("JOB_TIMEOUT");
	case EWorkflowEventType::JobSkipped: return TEXT("JOB_SKIPPED");
	case EWorkflowEventType::JobRetrying: return TEXT("JOB_RETRYING");
	case EWorkflowEventType::ProgressUpdated: return TEXT("PROGRESS");
	default: return TEXT("UNKNOWN");
	}
}
