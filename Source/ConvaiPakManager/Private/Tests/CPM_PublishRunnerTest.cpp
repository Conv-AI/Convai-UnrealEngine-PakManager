// Copyright 2025 Convai Inc. All Rights Reserved.

#include "CPM_PublishRunnerTestJob.h"
#include "Jobs/CPM_PublishRunner.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	/**
	 * What a caller of a Publish queue actually observes: the order the Jobs run in, one finish with
	 * one outcome, and progress scaled across the queue.
	 *
	 * Deliberately thin, and deliberately the ONLY thing these tests talk to. It is what let the same
	 * cases run against the machinery being replaced and the machinery replacing it: swapping the
	 * runner underneath moved these two methods and nothing else in the file.
	 */
	struct FPublishRunHarness
	{
		enum class EOutcome : uint8
		{
			Completed,
			Failed,
			Cancelled
		};

		int32 FinishedCount = 0;
		EOutcome Outcome = EOutcome::Completed;
		FString Error;

		int32 ProgressCount = 0;
		int32 LastProgressIndex = INDEX_NONE;
		float LastProgress = -1.0f;
		FString LastProgressText;

		void Start(const TArray<UCPM_PublishJobBase*>& Jobs)
		{
			Runner.Reset(NewObject<UCPM_PublishRunner>());

			FCPM_OnPublishProgress OnProgress;
			OnProgress.BindLambda([this](UCPM_PublishJobBase*, const int32 JobIndex, const float Overall, const FString& Step)
			{
				++ProgressCount;
				LastProgressIndex = JobIndex;
				LastProgress = Overall;
				LastProgressText = Step;
			});

			FCPM_OnPublishFinished OnFinished;
			OnFinished.BindLambda([this](const ECPM_PublishResult Result, const FString& InError, float)
			{
				++FinishedCount;
				Outcome = Result == ECPM_PublishResult::Success
					? EOutcome::Completed
					: (Result == ECPM_PublishResult::Cancelled ? EOutcome::Cancelled : EOutcome::Failed);
				Error = InError;
			});

			Runner->Start(Jobs, FCPM_PublishContext(), MoveTemp(OnProgress), MoveTemp(OnFinished));
		}

		void Cancel()
		{
			Runner->Cancel(/*bForce=*/false);
		}

		/** Strong, because nothing else holds the runner for the length of a test. */
		TStrongObjectPtr<UCPM_PublishRunner> Runner;
	};

	UCPM_PublishRunnerTestJob* MakeJob()
	{
		return NewObject<UCPM_PublishRunnerTestJob>();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMPublishRunnerRunsJobsInOrder,
	"ConvaiPakManager.Publish.Runner.RunsJobsInOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMPublishRunnerRunsJobsInOrder::RunTest(const FString&)
{
	UCPM_PublishRunnerTestJob* First = MakeJob();
	UCPM_PublishRunnerTestJob* Second = MakeJob();
	UCPM_PublishRunnerTestJob* Third = MakeJob();
	for (UCPM_PublishRunnerTestJob* Job : { First, Second, Third })
	{
		Job->bReportOnExecute = false;
	}

	FPublishRunHarness Harness;
	Harness.Start({ First, Second, Third });

	TestEqual(TEXT("nothing has run when Start returns"), First->ExecuteCount, 0);

	PumpTicker();
	TestEqual(TEXT("the first job runs on the next tick"), First->ExecuteCount, 1);
	TestEqual(TEXT("the second waits"), Second->ExecuteCount, 0);

	First->FinishNow(true);
	TestEqual(TEXT("the second runs once the first reports"), Second->ExecuteCount, 1);
	TestEqual(TEXT("the third still waits"), Third->ExecuteCount, 0);
	TestEqual(TEXT("nothing has finished yet"), Harness.FinishedCount, 0);

	Second->FinishNow(true);
	TestEqual(TEXT("the third runs once the second reports"), Third->ExecuteCount, 1);

	Third->FinishNow(true);
	TestEqual(TEXT("the run finishes exactly once"), Harness.FinishedCount, 1);
	TestTrue(TEXT("and finishes Completed"), Harness.Outcome == FPublishRunHarness::EOutcome::Completed);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMPublishRunnerAFailureStopsTheQueue,
	"ConvaiPakManager.Publish.Runner.AFailureStopsTheQueue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMPublishRunnerAFailureStopsTheQueue::RunTest(const FString&)
{
	// The runner writes one Error for a failed run, which is the point of it.
	AddExpectedError(TEXT("Publishing chunk .* failed"), EAutomationExpectedErrorFlags::Contains, 1);

	UCPM_PublishRunnerTestJob* First = MakeJob();
	UCPM_PublishRunnerTestJob* Second = MakeJob();
	UCPM_PublishRunnerTestJob* Third = MakeJob();
	Second->bShouldSucceed = false;
	Second->FailError = TEXT("boom");

	FPublishRunHarness Harness;
	Harness.Start({ First, Second, Third });
	PumpTicker();

	TestEqual(TEXT("the failing job ran"), Second->ExecuteCount, 1);
	TestEqual(TEXT("the job after it never did"), Third->ExecuteCount, 0);
	TestEqual(TEXT("the run finishes exactly once"), Harness.FinishedCount, 1);
	TestTrue(TEXT("and finishes Failed"), Harness.Outcome == FPublishRunHarness::EOutcome::Failed);
	TestEqual(TEXT("carrying the job's own error"), Harness.Error, FString(TEXT("boom")));

	return true;
}

/**
 * Cancelled, never Failed: the two are different words to a creator, and the status the panel shows
 * is decided by this outcome alone.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMPublishRunnerCancelResolvesAsCancelled,
	"ConvaiPakManager.Publish.Runner.CancelResolvesAsCancelled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMPublishRunnerCancelResolvesAsCancelled::RunTest(const FString&)
{
	UCPM_PublishRunnerTestJob* InFlight = MakeJob();
	UCPM_PublishRunnerTestJob* Later = MakeJob();
	InFlight->bReportOnExecute = false;

	FPublishRunHarness Harness;
	Harness.Start({ InFlight, Later });
	PumpTicker();

	Harness.Cancel();

	TestEqual(TEXT("the running job was asked to cancel once"), InFlight->CancelCount, 1);
	TestEqual(TEXT("the run finishes exactly once"), Harness.FinishedCount, 1);
	TestTrue(TEXT("and finishes Cancelled"), Harness.Outcome == FPublishRunHarness::EOutcome::Cancelled);
	TestEqual(TEXT("the next job never ran"), Later->ExecuteCount, 0);

	// A creator clicking Cancel twice, and the request that was already in flight answering late.
	Harness.Cancel();
	InFlight->FinishNow(true);
	PumpTicker();

	TestEqual(TEXT("a second cancel and a late report change nothing"), Harness.FinishedCount, 1);
	TestTrue(TEXT("the outcome is still Cancelled"), Harness.Outcome == FPublishRunHarness::EOutcome::Cancelled);
	TestEqual(TEXT("and the next job still never ran"), Later->ExecuteCount, 0);

	return true;
}

/**
 * The finished-during-start case, and the reason the runner defers its first Execute by a tick.
 *
 * A queue whose every Job completes synchronously - one packaging Job reusing the Pak already on
 * disk - used to run to completion inside the call that created it, so the caller was told the run
 * had finished before it had anything to register it under.
 *
 * It is what deleted the pair of subsystem members that existed only to survive that.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMPublishRunnerASynchronousJobFinishesOnce,
	"ConvaiPakManager.Publish.Runner.ASynchronousJobFinishesOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMPublishRunnerASynchronousJobFinishesOnce::RunTest(const FString&)
{
	UCPM_PublishRunnerTestJob* Synchronous = MakeJob();

	FPublishRunHarness Harness;
	Harness.Start({ Synchronous });

	TestEqual(TEXT("Start returns before the run has finished"), Harness.FinishedCount, 0);

	PumpTicker();
	TestEqual(TEXT("and then it finishes exactly once"), Harness.FinishedCount, 1);
	TestTrue(TEXT("Completed"), Harness.Outcome == FPublishRunHarness::EOutcome::Completed);

	PumpTicker();
	TestEqual(TEXT("a later tick does not finish it again"), Harness.FinishedCount, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMPublishRunnerATimeoutFailsTheJob,
	"ConvaiPakManager.Publish.Runner.ATimeoutFailsTheJob",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMPublishRunnerATimeoutFailsTheJob::RunTest(const FString&)
{
	// The runner writes one Error for a failed run, which is the point of it.
	AddExpectedError(TEXT("Publishing chunk .* failed"), EAutomationExpectedErrorFlags::Contains, 1);

	UCPM_PublishRunnerTestJob* Stuck = MakeJob();
	Stuck->bReportOnExecute = false;
	Stuck->TimeoutOverride = 0.001f;

	FPublishRunHarness Harness;
	Harness.Start({ Stuck });

	// Two: one to execute the job and arm its deadline, one for the deadline to expire. A ticker
	// delegate added during a tick is not run until the next one.
	PumpTicker();
	PumpTicker();

	TestEqual(TEXT("the run finishes exactly once"), Harness.FinishedCount, 1);
	TestTrue(TEXT("Failed"), Harness.Outcome == FPublishRunHarness::EOutcome::Failed);
	TestTrue(TEXT("saying it timed out"), Harness.Error.Contains(TEXT("timed out")));
	TestEqual(TEXT("having asked the job to stop"), Stuck->CancelCount, 1);

	return true;
}

/**
 * Progress is the queue's, not the Job's. A creator watching a five-step publish must not see the
 * bar reach the end five times.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMPublishRunnerProgressIsOverall,
	"ConvaiPakManager.Publish.Runner.ProgressIsOverall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMPublishRunnerProgressIsOverall::RunTest(const FString&)
{
	UCPM_PublishRunnerTestJob* First = MakeJob();
	UCPM_PublishRunnerTestJob* Second = MakeJob();
	for (UCPM_PublishRunnerTestJob* Job : { First, Second })
	{
		Job->bReportOnExecute = false;
		Job->ProgressToReport = 0.5f;
	}

	FPublishRunHarness Harness;
	Harness.Start({ First, Second });
	PumpTicker();

	TestEqual(TEXT("the first job's half is the queue's quarter"), Harness.LastProgress, 0.25f, 0.001f);
	TestEqual(TEXT("reported against its position"), Harness.LastProgressIndex, 0);
	TestEqual(TEXT("with the job's own text"), Harness.LastProgressText, FString(TEXT("working")));

	First->FinishNow(true);

	TestEqual(TEXT("the second job's half is three quarters"), Harness.LastProgress, 0.75f, 0.001f);
	TestEqual(TEXT("reported against its position"), Harness.LastProgressIndex, 1);

	return true;
}

#endif
