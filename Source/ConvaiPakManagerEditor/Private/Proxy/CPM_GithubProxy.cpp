// Convai


#include "Proxy/CPM_GithubProxy.h"

UCPM_GetGithubRepoFileProxy* UCPM_GetGithubRepoFileProxy::GetGithubRepoFileProxy(
	const FString& GithubRepoName, const FString& BranchName, const FString& FileName)
{
	UCPM_GetGithubRepoFileProxy* Proxy = NewObject<UCPM_GetGithubRepoFileProxy>();
	Proxy->URL = FString::Printf(TEXT("https://raw.githubusercontent.com/%s/%s/%s"), *GithubRepoName, * BranchName, *FileName);	
	return Proxy;
}

bool UCPM_GetGithubRepoFileProxy::ConfigureRequest(TSharedRef<IConvaihttpRequest> Request, const TCHAR* Verb)
{
	if (!Super::ConfigureRequest(Request, ConvaiHttpConstants::GET))
	{
		return false;
	}
	
	// Request->OnRequestProgress().BindLambda(
	// [&](CONVAI_HTTP_REQUEST_PTR InRequest, CONVAI_HTTP_DOWN_PROGRESS_TYPE BytesSent, CONVAI_HTTP_DOWN_PROGRESS_TYPE BytesReceived)
	// {
	// 	const uint64 TotalBytes = InRequest->GetResponse()->GetContentLength();
	// 	const float UploadProgress = TotalBytes > 0 ? static_cast<float>(BytesSent) / static_cast<float>(TotalBytes) : 0.0f;
	//
	// 	OnProgress.Broadcast(ResponseString, UploadProgress);
	// });
	
	return true;
}

void UCPM_GetGithubRepoFileProxy::HandleSuccess()
{
	Super::HandleSuccess();
	OnSuccess.Broadcast(ResponseString);
}

void UCPM_GetGithubRepoFileProxy::HandleFailure()
{
	Super::HandleFailure();
	OnFailure.Broadcast(ResponseString);
}

