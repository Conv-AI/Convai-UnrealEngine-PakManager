// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Publish/CPM_PublishTypes.h"
#include "UObject/Object.h"

#include "CPM_PublishRunner.generated.h"

class UCPM_PublishJobBase;

/** Where a run has got to: which Job, how far across the whole queue, and what it calls what it is doing. */
DECLARE_DELEGATE_FourParams(FCPM_OnPublishProgress, UCPM_PublishJobBase* /*Job*/, int32 /*JobIndex*/,
	float /*Overall*/, const FString& /*Step*/);

/** Once per run, whatever the outcome. Progress is where it had got to, which a cancel needs. */
DECLARE_DELEGATE_ThreeParams(FCPM_OnPublishFinished, ECPM_PublishResult /*Result*/, const FString& /*Error*/,
	float /*Progress*/);

/**
 * Runs the Jobs of one Publish, in order, one at a time.
 *
 * Sequential, per-Job deadline, two-part cancel - the three things a Publish actually needs from an
 * orchestrator, and the whole of what it used from the one it used to borrow. See docs/adr/0012.
 *
 * The first Job is executed a tick after Start rather than inside it, which is load-bearing: a queue
 * whose Jobs all complete synchronously - one packaging Job reusing the Pak already on disk - would
 * otherwise report the run finished before the caller had anywhere to register it.
 */
UCLASS()
class CONVAIPAKMANAGER_API UCPM_PublishRunner : public UObject
{
	GENERATED_BODY()

public:
	void Start(const TArray<UCPM_PublishJobBase*>& InJobs, const FCPM_PublishContext& InContext,
		FCPM_OnPublishProgress OnProgress, FCPM_OnPublishFinished OnFinished);

	/**
	 * Asks the running Job to stop. The run resolves as Cancelled when it reports, and forced or not
	 * it is the Job that decides how much of what is in flight it abandons.
	 */
	void Cancel(bool bForce);

	/** Called by the running Job. A report from any other Job, or after the run finished, is ignored. */
	void ReportJobFinished(UCPM_PublishJobBase* Job, ECPM_PublishResult Result, const FString& Error);
	void ReportJobProgress(UCPM_PublishJobBase* Job, const FString& Step, float Percent);

	/** The run's shared state. Jobs are handed a pointer to it and write their outputs into it. */
	FCPM_PublishContext& GetContext() { return Context; }

private:
	void ExecuteCurrent();
	void ArmTimeout(const UCPM_PublishJobBase& Job);
	void HandleTimeout(float Seconds);
	void Finish(ECPM_PublishResult Result, const FString& Error);
	void ClearTimers();

	UPROPERTY()
	TArray<TObjectPtr<UCPM_PublishJobBase>> Jobs;

	UPROPERTY()
	FCPM_PublishContext Context;

	int32 CurrentIndex = INDEX_NONE;

	/** 0..1 across the queue, kept so the finish can say how far a cancelled run had got. */
	float Progress = 0.0f;

	bool bFinished = false;

	/** Set by Cancel, so a Job that answers Failed while stopping still resolves the run as Cancelled. */
	bool bCancelling = false;

	FTSTicker::FDelegateHandle StartTicker;
	FTSTicker::FDelegateHandle TimeoutTicker;

	FCPM_OnPublishProgress ProgressDelegate;
	FCPM_OnPublishFinished FinishedDelegate;
};
