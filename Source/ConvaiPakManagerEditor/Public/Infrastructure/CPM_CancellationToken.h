// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeBool.h"

/**
 * Cancellation token for cooperative cancellation of async operations.
 * Thread-safe and can be shared across multiple operations.
 */
class CONVAIPAKMANAGEREDITOR_API FCPM_CancellationToken : public TSharedFromThis<FCPM_CancellationToken>
{
public:
	FCPM_CancellationToken() : bIsCancelled(false) {}

	/** Check if cancellation has been requested */
	bool IsCancellationRequested() const
	{
		return bIsCancelled;
	}

	/** Throws if cancelled - useful for checking at safe points */
	void ThrowIfCancellationRequested() const
	{
		checkf(!bIsCancelled, TEXT("Operation was cancelled"));
	}

	/** Register a callback to be called when cancellation is requested */
	FDelegateHandle OnCancelled(TFunction<void()> Callback)
	{
		FScopeLock Lock(&CallbackMutex);
		
		// If already cancelled, invoke immediately
		if (bIsCancelled)
		{
			Callback();
			return FDelegateHandle();
		}

		return OnCancelledDelegate.AddLambda(MoveTemp(Callback));
	}

	/** Unregister a cancellation callback */
	void RemoveOnCancelled(FDelegateHandle Handle)
	{
		FScopeLock Lock(&CallbackMutex);
		OnCancelledDelegate.Remove(Handle);
	}

private:
	friend class FCPM_CancellationTokenSource;

	/** Request cancellation */
	void RequestCancellation()
	{
		bool bWasCancelled = bIsCancelled.AtomicSet(true);
		
		if (!bWasCancelled)
		{
			// Notify all registered callbacks
			FScopeLock Lock(&CallbackMutex);
			OnCancelledDelegate.Broadcast();
		}
	}

	FThreadSafeBool bIsCancelled;
	
	DECLARE_MULTICAST_DELEGATE(FOnCancelledDelegate);
	FOnCancelledDelegate OnCancelledDelegate;
	mutable FCriticalSection CallbackMutex;
};

/**
 * Source that controls a cancellation token.
 * Only the owner of the source can request cancellation.
 */
class CONVAIPAKMANAGEREDITOR_API FCPM_CancellationTokenSource
{
public:
	FCPM_CancellationTokenSource()
	{
		Token = MakeShared<FCPM_CancellationToken>();
	}

	/** Get the token to share with operations */
	TSharedPtr<FCPM_CancellationToken> GetToken() const
	{
		return Token;
	}

	/** Request cancellation of all operations using this token */
	void Cancel()
	{
		if (Token.IsValid())
		{
			Token->RequestCancellation();
		}
	}

	/** Check if cancellation has been requested */
	bool IsCancellationRequested() const
	{
		return Token.IsValid() && Token->IsCancellationRequested();
	}

private:
	TSharedPtr<FCPM_CancellationToken> Token;
};
