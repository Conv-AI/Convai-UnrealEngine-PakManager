// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Publish/CPM_PolicyRequest.h"

#include "Proxy/CPM_GithubProxy.h"

void UCPM_PolicyRequest::Start(
	const FString& Repository,
	const FString& Ref,
	const FString& Path,
	FOnPolicyFetched OnFetched)
{
	UCPM_PolicyRequest* Request = NewObject<UCPM_PolicyRequest>();
	Request->Callback = OnFetched;

	Request->Proxy = UCPM_GetGithubRepoFileProxy::GetGithubRepoFileProxy(Repository, Ref, Path);
	if (!Request->Proxy)
	{
		OnFetched.ExecuteIfBound(false, FString());
		return;
	}

	// Rooted before Activate, not after: a request that answers synchronously would otherwise be
	// collectable between the two.
	Request->AddToRoot();

	Request->Proxy->OnSuccess.AddDynamic(Request, &UCPM_PolicyRequest::HandleSuccess);
	Request->Proxy->OnFailure.AddDynamic(Request, &UCPM_PolicyRequest::HandleFailure);
	Request->Proxy->Activate();
}

void UCPM_PolicyRequest::HandleSuccess(const FString& ResponseString)
{
	Finish(true, ResponseString);
}

void UCPM_PolicyRequest::HandleFailure(const FString& ResponseString)
{
	Finish(false, FString());
}

void UCPM_PolicyRequest::Finish(const bool bSucceeded, const FString& Contents)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	Proxy = nullptr;

	// Copied before unrooting: the callback may be the last thing referencing whatever it captured,
	// and this object becomes collectable the moment it is unrooted.
	const FOnPolicyFetched Answer = Callback;
	Callback.Unbind();
	RemoveFromRoot();

	Answer.ExecuteIfBound(bSucceeded, Contents);
}
