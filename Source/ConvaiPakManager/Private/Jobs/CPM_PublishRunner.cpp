// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Jobs/CPM_PublishRunner.h"

#include "Jobs/CPM_PublishJobs.h"
#include "Utility/CPM_Log.h"

namespace
{
	const TCHAR* ResultName(const ECPM_PublishResult Result)
	{
		switch (Result)
		{
		case ECPM_PublishResult::Success:   return TEXT("succeeded");
		case ECPM_PublishResult::Cancelled: return TEXT("was cancelled");
		default:                            return TEXT("failed");
		}
	}
}

void UCPM_PublishRunner::Start(const TArray<UCPM_PublishJobBase*>& InJobs, const FCPM_PublishContext& InContext,
	FCPM_OnPublishProgress OnProgress, FCPM_OnPublishFinished OnFinished)
{
	Context = InContext;
	ProgressDelegate = MoveTemp(OnProgress);
	FinishedDelegate = MoveTemp(OnFinished);

	Jobs.Reset(InJobs.Num());
	for (UCPM_PublishJobBase* Job : InJobs)
	{
		Jobs.Add(Job);
		Job->Initialize(this, &Context);
	}

	CurrentIndex = 0;

	CPM_LOG(Display, TEXT("Publishing chunk %d to %s: %d step(s)."),
		Context.Request.ChunkId, *Context.Request.EnvironmentSlug, Jobs.Num());

	// A tick, never inside this call. See the class comment: a synchronous queue that finished here
	// would report a run over to a caller that has not yet been handed one to cancel.
	StartTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		StartTicker.Reset();
		ExecuteCurrent();
		return false;
	}));
}

void UCPM_PublishRunner::ExecuteCurrent()
{
	if (bFinished)
	{
		return;
	}

	if (!Jobs.IsValidIndex(CurrentIndex))
	{
		Finish(ECPM_PublishResult::Success, FString());
		return;
	}

	UCPM_PublishJobBase* Job = Jobs[CurrentIndex];
	CPM_LOG(Display, TEXT("Chunk %d, step %d of %d: %s."),
		Context.Request.ChunkId, CurrentIndex + 1, Jobs.Num(), *Job->Name());

	ArmTimeout(*Job);
	Job->Execute();
}

void UCPM_PublishRunner::ArmTimeout(const UCPM_PublishJobBase& Job)
{
	const float Seconds = Job.TimeoutSeconds();
	if (Seconds <= 0.0f)
	{
		return;
	}

	TimeoutTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this, Seconds](float)
	{
		TimeoutTicker.Reset();
		HandleTimeout(Seconds);
		return false;
	}), Seconds);
}

void UCPM_PublishRunner::HandleTimeout(const float Seconds)
{
	if (bFinished || !Jobs.IsValidIndex(CurrentIndex))
	{
		return;
	}

	UCPM_PublishJobBase* Job = Jobs[CurrentIndex];

	// Finished FIRST, then the Job is told to stop. The other order loses the outcome: a Job reports
	// Cancelled from inside Cancel, and taking that at face value would tell the creator they stopped
	// a publish that had actually run out of time.
	Finish(ECPM_PublishResult::Failed,
		FString::Printf(TEXT("%s timed out after %gs"), *Job->Name(), Seconds));

	Job->Cancel(/*bForce=*/true);
}

void UCPM_PublishRunner::Cancel(const bool bForce)
{
	if (bFinished)
	{
		return;
	}

	bCancelling = true;

	if (StartTicker.IsValid())
	{
		// Cancelled in the tick between Start and the first Job. Nothing has run, so there is nothing
		// to ask to stop and nobody to report.
		FTSTicker::GetCoreTicker().RemoveTicker(StartTicker);
		StartTicker.Reset();
		Finish(ECPM_PublishResult::Cancelled, TEXT("cancelled"));
		return;
	}

	// ponytail: no time-box on the Job answering. Every Publish Job reports from inside Cancel, so
	// the run resolves on this stack; one that stopped doing so would leave the run in flight for the
	// session. Give the runner a cancel deadline if a Job ever needs one.
	if (Jobs.IsValidIndex(CurrentIndex))
	{
		Jobs[CurrentIndex]->Cancel(bForce);
	}
}

void UCPM_PublishRunner::ReportJobFinished(UCPM_PublishJobBase* Job, const ECPM_PublishResult Result, const FString& Error)
{
	if (bFinished || !Jobs.IsValidIndex(CurrentIndex) || Jobs[CurrentIndex] != Job)
	{
		return;
	}

	ClearTimers();

	CPM_LOG(Log, TEXT("Chunk %d: %s %s%s%s."), Context.Request.ChunkId, *Job->Name(), ResultName(Result),
		Error.IsEmpty() ? TEXT("") : TEXT(" - "), *Error);

	// Asked first, so a Job that answers a cancel with Failed - or succeeds in the moment between the
	// creator clicking and the request unhooking - still resolves the run as the cancel it was.
	if (bCancelling || Result == ECPM_PublishResult::Cancelled)
	{
		Finish(ECPM_PublishResult::Cancelled, Error);
		return;
	}

	if (Result == ECPM_PublishResult::Failed)
	{
		Finish(ECPM_PublishResult::Failed, Error);
		return;
	}

	++CurrentIndex;
	ExecuteCurrent();
}

void UCPM_PublishRunner::ReportJobProgress(UCPM_PublishJobBase* Job, const FString& Step, const float Percent)
{
	if (bFinished || !Jobs.IsValidIndex(CurrentIndex) || Jobs[CurrentIndex] != Job)
	{
		return;
	}

	Progress = (static_cast<float>(CurrentIndex) + FMath::Clamp(Percent, 0.0f, 1.0f))
		/ static_cast<float>(Jobs.Num());

	ProgressDelegate.ExecuteIfBound(Job, CurrentIndex, Progress, Step);
}

void UCPM_PublishRunner::Finish(const ECPM_PublishResult Result, const FString& Error)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	// The one line that says how a Publish ended. Error carries the reason, and until now it reached
	// only the UI, which the creator has usually navigated away from by the time they ask why.
	if (Result == ECPM_PublishResult::Success)
	{
		CPM_LOG(Display, TEXT("Publishing chunk %d succeeded."), Context.Request.ChunkId);
	}
	else if (Result == ECPM_PublishResult::Cancelled)
	{
		// Display, not Error: the creator asked for this. Logging it as an error would also fail any
		// automation test that exercises the cancel path.
		CPM_LOG(Display, TEXT("Publishing chunk %d was cancelled."), Context.Request.ChunkId);
	}
	else
	{
		CPM_LOG(Error, TEXT("Publishing chunk %d failed: %s"),
			Context.Request.ChunkId, Error.IsEmpty() ? TEXT("no reason given") : *Error);
	}

	ClearTimers();
	FinishedDelegate.ExecuteIfBound(Result, Error, Progress);
}

void UCPM_PublishRunner::ClearTimers()
{
	if (StartTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(StartTicker);
		StartTicker.Reset();
	}

	if (TimeoutTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TimeoutTicker);
		TimeoutTicker.Reset();
	}
}
