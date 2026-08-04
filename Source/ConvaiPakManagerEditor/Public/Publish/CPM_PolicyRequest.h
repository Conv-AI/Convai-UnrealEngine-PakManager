// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/StrongObjectPtr.h"

#include "CPM_PolicyRequest.generated.h"

class UCPM_GetGithubRepoFileProxy;

/**
 * One in-flight read of the Publish Policy.
 *
 * Exists because the fetch proxy answers through a DYNAMIC delegate, which can only bind a UFUNCTION
 * on a UObject - so a caller wanting a lambda needs an object to be that UFUNCTION. Keeping it here
 * rather than on the subsystem also means two Chunks resolving their policy at once cannot overwrite
 * each other's pending callback, which a single set of handlers on the subsystem would.
 *
 * Roots itself for the length of the request and unroots on the way out, so nothing else has to keep
 * it alive.
 */
UCLASS()
class CONVAIPAKMANAGEREDITOR_API UCPM_PolicyRequest : public UObject
{
	GENERATED_BODY()

public:
	/** Called once, with the file's contents on success or an empty string on failure. */
	DECLARE_DELEGATE_TwoParams(FOnPolicyFetched, bool /*bSucceeded*/, const FString& /*Contents*/);

	/** Starts a read. The request keeps itself alive until it answers. */
	static void Start(const FString& Repository, const FString& Ref, const FString& Path, FOnPolicyFetched OnFetched);

private:
	UFUNCTION()
	void HandleSuccess(const FString& ResponseString);

	UFUNCTION()
	void HandleFailure(const FString& ResponseString);

	void Finish(bool bSucceeded, const FString& Contents);

	UPROPERTY()
	TObjectPtr<UCPM_GetGithubRepoFileProxy> Proxy;

	FOnPolicyFetched Callback;

	/** Guards against a proxy that broadcasts both outcomes, which would unroot this twice. */
	bool bFinished = false;
};
