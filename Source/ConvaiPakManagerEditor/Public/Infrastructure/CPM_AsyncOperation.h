// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Async/Async.h"
#include "Infrastructure/CPM_Result.h"
#include "Infrastructure/CPM_CancellationToken.h"
#include "Utility/CPM_Log.h"

/**
 * State of an async operation
 */
enum class ECPM_AsyncState : uint8
{
	NotStarted,
	Running,
	Succeeded,
	Failed,
	Cancelled
};

/**
 * Progress reporter interface for async operations
 */
class ICPM_ProgressReporter
{
public:
	virtual ~ICPM_ProgressReporter() = default;
	
	/** Report progress (0.0 to 1.0) */
	virtual void ReportProgress(float Progress, const FString& Message = FString()) = 0;
};

/**
 * Simple progress reporter that broadcasts to a delegate
 */
class FCPM_DelegateProgressReporter : public ICPM_ProgressReporter
{
public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnProgress, float /* Progress */, const FString& /* Message */);
	
	virtual void ReportProgress(float Progress, const FString& Message = FString()) override
	{
		// Marshal to game thread
		AsyncTask(ENamedThreads::GameThread, [this, Progress, Message]()
		{
			OnProgressDelegate.Broadcast(Progress, Message);
		});
	}

	FOnProgress& OnProgress() { return OnProgressDelegate; }

private:
	FOnProgress OnProgressDelegate;
};

/**
 * Async operation wrapper with cancellation and progress support.
 * Executes work on a background thread and reports results on the game thread.
 */
template <typename TResult>
class FCPM_AsyncOperation : public TSharedFromThis<FCPM_AsyncOperation<TResult>>
{
public:
	/** Delegate for completion callbacks */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnComplete, const TCPM_Result<TResult>& /* Result */);

	/** Work function signature */
	using FWorkFunction = TFunction<TCPM_Result<TResult>(
		TSharedPtr<FCPM_CancellationToken> /* Token */, 
		TSharedPtr<ICPM_ProgressReporter> /* Progress */)>;

	/**
	 * Create an async operation
	 * @param WorkFunction The work to execute
	 * @param CancellationToken Optional cancellation token
	 */
	FCPM_AsyncOperation(FWorkFunction InWorkFunction, TSharedPtr<FCPM_CancellationToken> InCancellationToken = nullptr)
		: WorkFunction(MoveTemp(InWorkFunction))
		, State(ECPM_AsyncState::NotStarted)
	{
		if (InCancellationToken.IsValid())
		{
			CancellationToken = InCancellationToken;
		}
		else
		{
			// Create our own token source
			OwnedTokenSource = MakeShared<FCPM_CancellationTokenSource>();
			CancellationToken = OwnedTokenSource->GetToken();
		}

		ProgressReporter = MakeShared<FCPM_DelegateProgressReporter>();
	}

	~FCPM_AsyncOperation()
	{
		if (IsRunning())
		{
			Cancel();
		}
	}

	/** Start the async operation */
	void Start()
	{
		FScopeLock Lock(&StateMutex);

		if (State != ECPM_AsyncState::NotStarted)
		{
			return;
		}

		if (!WorkFunction)
		{
			CPM_LOG(Error, TEXT("AsyncOperation: No work function provided"));
			State = ECPM_AsyncState::Failed;
			Result = TCPM_Result<TResult>::Failure(TEXT("No work function provided"));
			BroadcastCompletion();
			return;
		}

		State = ECPM_AsyncState::Running;

		// Execute on thread pool
		TWeakPtr<FCPM_AsyncOperation<TResult>> WeakSelf = this->AsShared();
		Async(EAsyncExecution::ThreadPool, [WeakSelf]()
		{
			if (TSharedPtr<FCPM_AsyncOperation<TResult>> This = WeakSelf.Pin())
			{
				This->ExecuteWork();
			}
		});
	}

	/** Cancel the operation */
	void Cancel()
	{
		if (OwnedTokenSource.IsValid())
		{
			OwnedTokenSource->Cancel();
		}

		FScopeLock Lock(&StateMutex);
		if (State == ECPM_AsyncState::Running || State == ECPM_AsyncState::NotStarted)
		{
			State = ECPM_AsyncState::Cancelled;
			Result = TCPM_Result<TResult>::Failure(TEXT("Operation cancelled"));
			BroadcastCompletion();
		}
	}

	/** Check if running */
	bool IsRunning() const
	{
		FScopeLock Lock(&StateMutex);
		return State == ECPM_AsyncState::Running;
	}

	/** Check if cancelled */
	bool IsCancelled() const
	{
		FScopeLock Lock(&StateMutex);
		return State == ECPM_AsyncState::Cancelled;
	}

	/** Check if complete (success, failure, or cancelled) */
	bool IsComplete() const
	{
		FScopeLock Lock(&StateMutex);
		return State == ECPM_AsyncState::Succeeded ||
		       State == ECPM_AsyncState::Failed ||
		       State == ECPM_AsyncState::Cancelled;
	}

	/** Get current state */
	ECPM_AsyncState GetState() const
	{
		FScopeLock Lock(&StateMutex);
		return State;
	}

	/** Register a completion callback (called on game thread) */
	FDelegateHandle OnComplete(TFunction<void(const TCPM_Result<TResult>&)> Callback)
	{
		FScopeLock Lock(&CompletionMutex);

		// If already complete, invoke immediately on game thread
		if (IsComplete())
		{
			TCPM_Result<TResult> CurrentResult = Result;
			AsyncTask(ENamedThreads::GameThread, [Callback = MoveTemp(Callback), CurrentResult]()
			{
				Callback(CurrentResult);
			});
			return FDelegateHandle();
		}

		return OnCompleteDelegate.AddLambda(MoveTemp(Callback));
	}

	/** Get progress delegate for binding */
	FCPM_DelegateProgressReporter::FOnProgress& OnProgress()
	{
		return StaticCastSharedPtr<FCPM_DelegateProgressReporter>(ProgressReporter)->OnProgress();
	}

	/** Get the result (blocks until complete) */
	TCPM_Result<TResult> GetResult() const
	{
		// Simple spin wait with timeout
		const double StartTime = FPlatformTime::Seconds();
		const double TimeoutSeconds = 60.0;

		while (!IsComplete())
		{
			if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds)
			{
				return TCPM_Result<TResult>::Failure(TEXT("Operation timed out"));
			}
			FPlatformProcess::Sleep(0.01f);
		}

		FScopeLock Lock(&ResultMutex);
		return Result;
	}

	/** Get cancellation token */
	TSharedPtr<FCPM_CancellationToken> GetCancellationToken() const
	{
		return CancellationToken;
	}

private:
	void ExecuteWork()
	{
		TCPM_Result<TResult> WorkResult;

		// Check cancellation before starting
		if (CancellationToken.IsValid() && CancellationToken->IsCancellationRequested())
		{
			WorkResult = TCPM_Result<TResult>::Failure(TEXT("Operation cancelled before execution"));
		}
		else
		{
			// Execute the work
			WorkResult = WorkFunction(CancellationToken, ProgressReporter);
		}

		// Complete the operation
		CompleteWith(WorkResult);
	}

	void CompleteWith(const TCPM_Result<TResult>& InResult)
	{
		{
			FScopeLock Lock(&StateMutex);
			if (State != ECPM_AsyncState::Running)
			{
				return; // Already completed or cancelled
			}

			// Determine final state
			if (CancellationToken.IsValid() && CancellationToken->IsCancellationRequested())
			{
				State = ECPM_AsyncState::Cancelled;
			}
			else if (InResult.IsSuccess())
			{
				State = ECPM_AsyncState::Succeeded;
			}
			else
			{
				State = ECPM_AsyncState::Failed;
			}
		}

		// Store result
		{
			FScopeLock Lock(&ResultMutex);
			Result = InResult;
		}

		// Broadcast completion
		BroadcastCompletion();
	}

	void BroadcastCompletion()
	{
		TCPM_Result<TResult> FinalResult;
		{
			FScopeLock Lock(&ResultMutex);
			FinalResult = Result;
		}

		TWeakPtr<FCPM_AsyncOperation<TResult>> WeakSelf = this->AsShared();

		AsyncTask(ENamedThreads::GameThread, [WeakSelf, FinalResult]()
		{
			if (TSharedPtr<FCPM_AsyncOperation<TResult>> This = WeakSelf.Pin())
			{
				FScopeLock Lock(&This->CompletionMutex);
				This->OnCompleteDelegate.Broadcast(FinalResult);
			}
		});
	}

	FWorkFunction WorkFunction;
	TSharedPtr<FCPM_CancellationToken> CancellationToken;
	TSharedPtr<FCPM_CancellationTokenSource> OwnedTokenSource;
	TSharedPtr<ICPM_ProgressReporter> ProgressReporter;
	
	ECPM_AsyncState State;
	mutable FCriticalSection StateMutex;
	
	TCPM_Result<TResult> Result;
	mutable FCriticalSection ResultMutex;
	
	FOnComplete OnCompleteDelegate;
	mutable FCriticalSection CompletionMutex;
};

/**
 * Helper to create and start an async operation
 */
template <typename TResult>
TSharedPtr<FCPM_AsyncOperation<TResult>> CPM_RunAsync(
	typename FCPM_AsyncOperation<TResult>::FWorkFunction WorkFunction,
	TSharedPtr<FCPM_CancellationToken> CancellationToken = nullptr)
{
	auto Operation = MakeShared<FCPM_AsyncOperation<TResult>>(MoveTemp(WorkFunction), CancellationToken);
	Operation->Start();
	return Operation;
}
