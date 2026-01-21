// Convai

#pragma once

#include "CoreMinimal.h"
#include "RestAPI/ConvaiAPIBase.h"
#include "CPM_GithubProxy.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPM_OnGetGithubRepoFile, const FString&, ResponseString);

UCLASS()
class UCPM_GetGithubRepoFileProxy : public UConvaiAPIBaseProxy
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FCPM_OnGetGithubRepoFile OnSuccess;
	
	UPROPERTY(BlueprintAssignable)
	FCPM_OnGetGithubRepoFile OnFailure;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Get Github Repo file" , WorldContext = "WorldContextObject"), Category = "Convai|PakManager")
	static UCPM_GetGithubRepoFileProxy* GetGithubRepoFileProxy(const FString& GithubRepoName, const FString& BranchName, const FString& FileName);

protected:
	virtual bool ConfigureRequest(TSharedRef<CONVAI_HTTP_REQUEST_INTERFACE> Request, const TCHAR* Verb) override;
	virtual void HandleSuccess() override;
	virtual void HandleFailure() override;
};
