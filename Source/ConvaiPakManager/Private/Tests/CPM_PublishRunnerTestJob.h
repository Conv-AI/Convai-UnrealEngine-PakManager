// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Jobs/CPM_PublishJobs.h"

#include "CPM_PublishRunnerTestJob.generated.h"

/**
 * A Publish Job that does nothing but record what was asked of it.
 *
 * Derives from UCPM_PublishJobBase rather than standing alone, so the tests exercise the real
 * Report and two-part-cancel plumbing every Publish Job inherits - the half of the contract most
 * likely to break when what sits underneath it is swapped.
 *
 * UHT only processes headers, so this cannot live in the .cpp that uses it.
 */
UCLASS()
class UCPM_PublishRunnerTestJob : public UCPM_PublishJobBase
{
	GENERATED_BODY()

public:
	/** False leaves the Job in flight forever, which is what lets a cancel or a timeout be the thing under test. */
	bool bReportOnExecute = true;

	bool bShouldSucceed = true;

	FString FailError = TEXT("deliberate test failure");

	/** The contract every real Publish Job keeps: report completion from inside a cancel. */
	bool bReportCancelledOnCancel = true;

	/** >= 0 reports this much progress before completing. */
	float ProgressToReport = -1.0f;
	FString ProgressStep = TEXT("working");

	/** > 0 gives this Job a deadline. Named apart from the accessor so a test can set it. */
	float TimeoutOverride = 0.0f;

	int32 ExecuteCount = 0;
	int32 CancelCount = 0;

	/** Reports from outside, for a test that has to hold a Job in flight and then release it. */
	void FinishNow(const bool bSucceeded, const FString& Error = FString())
	{
		Report(bSucceeded ? ECPM_PublishResult::Success : ECPM_PublishResult::Failed, Error);
	}

	virtual FString Name() const override { return TEXT("Fake"); }

	virtual float TimeoutSeconds() const override { return TimeoutOverride; }

	virtual void Execute() override
	{
		++ExecuteCount;

		if (ProgressToReport >= 0.0f)
		{
			ReportProgress(ProgressStep, ProgressToReport);
		}

		if (!bReportOnExecute)
		{
			return;
		}

		Report(bShouldSucceed ? ECPM_PublishResult::Success : ECPM_PublishResult::Failed,
			bShouldSucceed ? FString() : FailError);
	}

	virtual void Cancel(const bool bForce) override
	{
		++CancelCount;
		bCancelled = true;

		if (bReportCancelledOnCancel)
		{
			Report(ECPM_PublishResult::Cancelled, TEXT("cancelled"));
		}
	}
};

/**
 * Advances the core ticker by a small delta on the calling (game) thread.
 *
 * Safe here: automation tests run on the game thread, which is also where the engine pumps the core
 * ticker, so this cannot overlap with the engine's own Tick.
 *
 * `inline`, at namespace scope, rather than a copy in an anonymous namespace: a unity build merges
 * the anonymous namespaces of the files it concatenates, which makes two identical bodies a
 * redefinition error.
 */
inline void PumpTicker()
{
	FTSTicker::GetCoreTicker().Tick(0.01f);
}
