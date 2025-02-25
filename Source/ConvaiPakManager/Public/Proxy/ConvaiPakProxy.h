// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Net/OnlineBlueprintCallProxyBase.h"
#include "Utility/CPM_Utils.h"
#include "ConvaiPakProxy.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPakUploadDelegate, const FString&, ResponseString, float, Progress);


UCLASS()
class UConvaiCreatePakAssetProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FPakUploadDelegate OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FPakUploadDelegate OnFailure;

	UPROPERTY(BlueprintAssignable)
	FPakUploadDelegate OnProgress;
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Create Pak Asset"), Category = "Convai|PakManager")
	static UConvaiCreatePakAssetProxy* CreatePakAssetProxy(const FConvaiCreatePakAssetParams& Params);

	virtual void Activate() override;
	
private:
	FString URL;
	FConvaiCreatePakAssetParams AssociatedParams;

	bool PrepareMultipartFormData(TArray<uint8>& DataToSend, const FString& Boundary) const;
	void OnHttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnHttpRequestProgress(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived);
};