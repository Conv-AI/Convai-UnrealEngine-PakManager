// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Examples/SampleAsyncJob.h"
#include "Core/WorkflowContext.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSampleJob, Log, All);

//////////////////////////////////////////////////////////////////////////
// USampleAsyncJob
//////////////////////////////////////////////////////////////////////////

void USampleAsyncJob::IExecute_Implementation(const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager)
{
	CachedWorkflowManager = WorkflowManager;
	CurrentStep = 0;
	bIsCancelled = false;

	UE_LOG(LogSampleJob, Log, TEXT("SampleAsyncJob: Starting async task (%.1fs, %d steps)"), 
		TaskDurationSeconds, ProgressSteps);

	if (const UWorld* World = GetWorld())
	{
		const float StepInterval = TaskDurationSeconds / FMath::Max(1, ProgressSteps);
		World->GetTimerManager().SetTimer(
			ProgressTimerHandle, 
			this, 
			&USampleAsyncJob::SimulateProgress, 
			StepInterval, 
			true);
	}
	else
	{
		CompleteJob();
	}
}

void USampleAsyncJob::ICancel_Implementation(bool bForce)
{
	bIsCancelled = true;
	
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ProgressTimerHandle);
	}

	UE_LOG(LogSampleJob, Log, TEXT("SampleAsyncJob: Cancelled (force=%s)"), bForce ? TEXT("true") : TEXT("false"));

	if (CachedWorkflowManager.GetInterface())
	{
		FJobCompletionInfo Completion;
		Completion.Job = this;
		Completion.Result = EJobResult::Cancelled;
		CachedWorkflowManager->IOnJobCompleted(Completion);
	}
}

void USampleAsyncJob::SimulateProgress()
{
	if (bIsCancelled) return;

	CurrentStep++;
	const float Progress = static_cast<float>(CurrentStep) / static_cast<float>(ProgressSteps);

	UE_LOG(LogSampleJob, Verbose, TEXT("SampleAsyncJob: Progress %.0f%%"), Progress * 100.0f);

	if (CachedWorkflowManager.GetInterface())
	{
		FJobProgressInfo ProgressInfo;
		ProgressInfo.Job = this;
		ProgressInfo.Progress = Progress;
		ProgressInfo.ProgressText = FText::FromString(FString::Printf(TEXT("Processing step %d of %d..."), CurrentStep, ProgressSteps));
		CachedWorkflowManager->IReportJobProgress(ProgressInfo);
	}

	if (CurrentStep >= ProgressSteps)
	{
		if (const UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ProgressTimerHandle);
		}
		CompleteJob();
	}
}

void USampleAsyncJob::CompleteJob()
{
	UE_LOG(LogSampleJob, Log, TEXT("SampleAsyncJob: Completed successfully"));

	if (CachedWorkflowManager.GetInterface())
	{
		FJobCompletionInfo Completion;
		Completion.Job = this;
		Completion.Result = EJobResult::Success;
		CachedWorkflowManager->IOnJobCompleted(Completion);
	}
}

//////////////////////////////////////////////////////////////////////////
// USampleFailingJob
//////////////////////////////////////////////////////////////////////////

USampleFailingJob::USampleFailingJob()
{
	// Configure retry settings by default
	JobConfig.Name = TEXT("Failing Job");
	JobConfig.bOverrideWorkflowDefaults = true;
	JobConfig.MaxRetries = 5;
	JobConfig.RetryDelaySeconds = 1.0f;
	JobConfig.bRetryOnFailure = true;
}

void USampleFailingJob::IExecute_Implementation(const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager)
{
	CachedWorkflowManager = WorkflowManager;
	bIsCancelled = false;

	UE_LOG(LogSampleJob, Log, TEXT("SampleFailingJob: Executing (failure probability: %.0f%%)"), 
		FailureProbability * 100.0f);

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ExecutionTimerHandle,
			this,
			&USampleFailingJob::AttemptCompletion,
			ExecutionTimeSeconds,
			false);
	}
	else
	{
		AttemptCompletion();
	}
}

void USampleFailingJob::ICancel_Implementation(bool bForce)
{
	bIsCancelled = true;

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExecutionTimerHandle);
	}

	UE_LOG(LogSampleJob, Log, TEXT("SampleFailingJob: Cancelled"));

	if (CachedWorkflowManager.GetInterface())
	{
		FJobCompletionInfo Completion;
		Completion.Job = this;
		Completion.Result = EJobResult::Cancelled;
		CachedWorkflowManager->IOnJobCompleted(Completion);
	}
}

void USampleFailingJob::AttemptCompletion()
{
	if (bIsCancelled) return;

	const bool bShouldFail = FMath::FRand() < FailureProbability;

	FJobCompletionInfo Completion;
	Completion.Job = this;

	if (bShouldFail)
	{
		UE_LOG(LogSampleJob, Warning, TEXT("SampleFailingJob: Failed! (will retry if configured)"));
		Completion.Result = EJobResult::Failed;
		Completion.ErrorMessage = TEXT("Simulated random failure");
	}
	else
	{
		UE_LOG(LogSampleJob, Log, TEXT("SampleFailingJob: Succeeded!"));
		Completion.Result = EJobResult::Success;
	}

	if (CachedWorkflowManager.GetInterface())
	{
		CachedWorkflowManager->IOnJobCompleted(Completion);
	}
}

//////////////////////////////////////////////////////////////////////////
// USampleConditionalJob
//////////////////////////////////////////////////////////////////////////

void USampleConditionalJob::IExecute_Implementation(const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager)
{
	UE_LOG(LogSampleJob, Log, TEXT("SampleConditionalJob: Executing (required key was found)"));

	FJobCompletionInfo Completion;
	Completion.Job = this;
	Completion.Result = EJobResult::Success;
	WorkflowManager->IOnJobCompleted(Completion);
}

bool USampleConditionalJob::IShouldSkip_Implementation(UWorkflowContext* Context) const
{
	if (!Context)
	{
		UE_LOG(LogSampleJob, Warning, TEXT("SampleConditionalJob: No context, skipping"));
		return true;
	}

	if (!RequiredContextKey.IsValid())
	{
		UE_LOG(LogSampleJob, Log, TEXT("SampleConditionalJob: No required key set, executing"));
		return false;
	}

	const bool bHasKey = Context->HasKey(RequiredContextKey);
	
	if (!bHasKey)
	{
		UE_LOG(LogSampleJob, Log, TEXT("SampleConditionalJob: Key '%s' not found, skipping"), 
			*RequiredContextKey.ToString());
	}

	return !bHasKey;
}

//////////////////////////////////////////////////////////////////////////
// USampleContextWriterJob
//////////////////////////////////////////////////////////////////////////

void USampleContextWriterJob::IExecute_Implementation(const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager)
{
	if (UWorkflowContext* Context = WorkflowManager->IGetContext())
	{
		for (const auto& Pair : DataToWrite)
		{
			Context->SetValue(Pair.Key, Pair.Value);
			UE_LOG(LogSampleJob, Log, TEXT("SampleContextWriterJob: Set '%s' = '%s'"), 
				*Pair.Key.ToString(), *Pair.Value);
		}
	}

	FJobCompletionInfo Completion;
	Completion.Job = this;
	Completion.Result = EJobResult::Success;
	WorkflowManager->IOnJobCompleted(Completion);
}
