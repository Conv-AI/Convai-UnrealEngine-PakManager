// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Examples/SampleWorkflowRunner.h"
#include "Examples/SampleAsyncJob.h"
#include "Core/WorkflowSubsystem.h"
#include "Core/WorkflowContext.h"
#include "Interface/WorkflowInterface.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorkflowDemo, Log, All);

ASampleWorkflowRunner::ASampleWorkflowRunner()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ASampleWorkflowRunner::BeginPlay()
{
	Super::BeginPlay();

	WorkflowManager = GEngine->GetEngineSubsystem<UWorkflowSubsystem>();
}

void ASampleWorkflowRunner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (WorkflowManager && CurrentWorkflowHandle.IsValid())
	{
		CancelWorkflow(true);
	}

	CreatedJobs.Empty();
	WorkflowManager = nullptr;

	Super::EndPlay(EndPlayReason);
}

FWorkflowHandle ASampleWorkflowRunner::RunDemoWorkflow()
{
	if (!WorkflowManager)
	{
		UE_LOG(LogWorkflowDemo, Error, TEXT("No workflow manager available!"));
		return FWorkflowHandle::Invalid();
	}

	UE_LOG(LogWorkflowDemo, Log, TEXT("========================================"));
	UE_LOG(LogWorkflowDemo, Log, TEXT("Starting Demo Workflow"));
	UE_LOG(LogWorkflowDemo, Log, TEXT("========================================"));

	CreatedJobs.Empty();

	USampleContextWriterJob* ContextWriterJob = NewObject<USampleContextWriterJob>(this);
	ContextWriterJob->JobConfig.Name = TEXT("Context Writer");

	TScriptInterface<IJobInterface> Job1;
	Job1.SetObject(ContextWriterJob);
	Job1.SetInterface(ContextWriterJob);
	CreatedJobs.Add(Job1);

	USampleAsyncJob* AsyncJob1 = NewObject<USampleAsyncJob>(this);
	AsyncJob1->JobConfig.Name = TEXT("Data Processing");
	AsyncJob1->TaskDurationSeconds = 3.0f;
	AsyncJob1->ProgressSteps = 6;

	TScriptInterface<IJobInterface> Job2;
	Job2.SetObject(AsyncJob1);
	Job2.SetInterface(AsyncJob1);
	CreatedJobs.Add(Job2);

	USampleConditionalJob* ConditionalJob = NewObject<USampleConditionalJob>(this);
	ConditionalJob->JobConfig.Name = TEXT("Conditional Task");

	TScriptInterface<IJobInterface> Job3;
	Job3.SetObject(ConditionalJob);
	Job3.SetInterface(ConditionalJob);
	CreatedJobs.Add(Job3);

	USampleFailingJob* FailingJob = NewObject<USampleFailingJob>(this);
	FailingJob->JobConfig.Name = TEXT("Unreliable Service Call");
	FailingJob->FailureProbability = 0.7f;
	FailingJob->ExecutionTimeSeconds = 0.2f;

	TScriptInterface<IJobInterface> Job4;
	Job4.SetObject(FailingJob);
	Job4.SetInterface(FailingJob);
	CreatedJobs.Add(Job4);

	USampleAsyncJob* AsyncJob2 = NewObject<USampleAsyncJob>(this);
	AsyncJob2->JobConfig.Name = TEXT("Finalization");
	AsyncJob2->TaskDurationSeconds = 2.0f;
	AsyncJob2->ProgressSteps = 4;

	TScriptInterface<IJobInterface> Job5;
	Job5.SetObject(AsyncJob2);
	Job5.SetInterface(AsyncJob2);
	CreatedJobs.Add(Job5);

	FWorkflowRequestFromJobs Request;
	Request.Options.WorkflowConfig.WorkflowTimeoutSeconds = WorkflowTimeoutSeconds;
	Request.Options.WorkflowConfig.DefaultJobConfig.MaxRetries = DefaultMaxRetries;
	Request.Options.WorkflowConfig.DefaultJobConfig.RetryDelaySeconds = DefaultRetryDelaySeconds;
	Request.Options.WorkflowConfig.DefaultJobConfig.bRetryOnFailure = true;
	Request.Jobs = CreatedJobs;

	TScriptInterface<IWorkflowListenerInterface> Listener;
	Listener.SetObject(this);
	Listener.SetInterface(this);
	Request.Options.Listeners.Add(Listener);

	FCreateWorkflowFromJobsParams Params;
	Params.Request = Request;
	Params.bStartImmediately = true;

	CurrentWorkflowHandle = WorkflowManager->ICreateWorkflowFromJobs(Params);

	if (CurrentWorkflowHandle.IsValid())
	{
		UE_LOG(LogWorkflowDemo, Log, TEXT("Workflow [%s] started successfully with %d jobs"), 
			*CurrentWorkflowHandle.Id.ToString(), CreatedJobs.Num());
	}
	else
	{
		UE_LOG(LogWorkflowDemo, Error, TEXT("Failed to start workflow!"));
	}

	return CurrentWorkflowHandle;
}

void ASampleWorkflowRunner::CancelWorkflow(bool bForce)
{
	if (WorkflowManager && CurrentWorkflowHandle.IsValid())
	{
		UE_LOG(LogWorkflowDemo, Log, TEXT("Cancelling workflow [%s] (force=%s)"), 
			*CurrentWorkflowHandle.Id.ToString(), bForce ? TEXT("true") : TEXT("false"));
		WorkflowManager->ICancelWorkflow(CurrentWorkflowHandle, bForce);
	}
}

void ASampleWorkflowRunner::CancelAllWorkflows(bool bForce)
{
	if (WorkflowManager)
	{
		UE_LOG(LogWorkflowDemo, Log, TEXT("Cancelling all workflows (force=%s)"), bForce ? TEXT("true") : TEXT("false"));
		WorkflowManager->ICancelAllWorkflows(bForce);
	}
}

bool ASampleWorkflowRunner::IsWorkflowRunning() const
{
	if (WorkflowManager && CurrentWorkflowHandle.IsValid())
	{
		if (TScriptInterface<IWorkflowInterface> Workflow = WorkflowManager->IGetWorkflow(CurrentWorkflowHandle))
		{
			const FWorkflowStatusInfo Status = Workflow->IGetStatusInfo();
			return Status.Status == EWorkflowStatus::Running || Status.Status == EWorkflowStatus::Cancelling;
		}
	}
	return false;
}

int32 ASampleWorkflowRunner::GetActiveWorkflowCount() const
{
	if (WorkflowManager)
	{
		return WorkflowManager->GetActiveWorkflowCount();
	}
	return 0;
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
	const FString WorkflowId = StatusInfo.Handle.IsValid() ? StatusInfo.Handle.Id.ToString().Left(8) : TEXT("N/A");

	switch (EventType)
	{
	case EWorkflowEventType::WorkflowStarted:
		UE_LOG(LogWorkflowDemo, Log, TEXT("[%s][%s] Total jobs: %d"), *WorkflowId, *EventName, StatusInfo.TotalJobs);
		break;

	case EWorkflowEventType::WorkflowCompleted:
		UE_LOG(LogWorkflowDemo, Log, TEXT("[%s][%s] Finished in %.2f seconds"), *WorkflowId, *EventName, StatusInfo.ElapsedTime);
		UE_LOG(LogWorkflowDemo, Log, TEXT("========================================"));
		break;

	case EWorkflowEventType::WorkflowFailed:
	case EWorkflowEventType::WorkflowTimeout:
	case EWorkflowEventType::WorkflowCancelled:
		UE_LOG(LogWorkflowDemo, Warning, TEXT("[%s][%s] %s"), *WorkflowId, *EventName, *StatusInfo.ErrorMessage);
		break;

	case EWorkflowEventType::JobStarted:
		UE_LOG(LogWorkflowDemo, Log, TEXT("[%s][%s] Job %d/%d: %s"), 
			*WorkflowId,
			*EventName, 
			StatusInfo.CurrentJob.Index + 1, 
			StatusInfo.TotalJobs, 
			*JobName);
		break;

	case EWorkflowEventType::JobCompleted:
		UE_LOG(LogWorkflowDemo, Log, TEXT("[%s][%s] %s completed in %.2fs"), 
			*WorkflowId,
			*EventName, 
			*JobName, 
			StatusInfo.CurrentJob.ElapsedTime);
		break;

	case EWorkflowEventType::JobFailed:
		UE_LOG(LogWorkflowDemo, Warning, TEXT("[%s][%s] %s failed: %s"), 
			*WorkflowId,
			*EventName, 
			*JobName, 
			*StatusInfo.CurrentJob.ErrorMessage);
		break;

	case EWorkflowEventType::JobRetrying:
		UE_LOG(LogWorkflowDemo, Log, TEXT("[%s][%s] %s retry attempt %d"), 
			*WorkflowId,
			*EventName, 
			*JobName, 
			StatusInfo.CurrentJob.RetryCount + 1);
		break;

	case EWorkflowEventType::JobSkipped:
		UE_LOG(LogWorkflowDemo, Log, TEXT("[%s][%s] %s skipped (condition not met)"), 
			*WorkflowId,
			*EventName, 
			*JobName);
		break;

	case EWorkflowEventType::ProgressUpdated:
		UE_LOG(LogWorkflowDemo, Verbose, TEXT("[%s][%s] %s: %.0f%% - %s"), 
			*WorkflowId,
			*EventName, 
			*JobName, 
			StatusInfo.CurrentJob.Progress * 100.0f,
			*StatusInfo.CurrentJob.ProgressText.ToString());
		break;

	default:
		UE_LOG(LogWorkflowDemo, Log, TEXT("[%s][%s]"), *WorkflowId, *EventName);
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
