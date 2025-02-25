// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Net/OnlineBlueprintCallProxyBase.h"
#include "ConvaiPakUploaderProxy.generated.h"

// Delegates for success, failure, and progress updates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPakUploadDelegate, bool, bSuccess, float, Progress);

UCLASS()
class CONVAIPAKMANAGER_API UConvaiPakUploaderProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FPakUploadDelegate OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FPakUploadDelegate OnFailure;

	UPROPERTY(BlueprintAssignable)
	FPakUploadDelegate OnProgress;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Upload Pak File"), Category = "Convai|PakManager")
	static UConvaiPakUploaderProxy* UploadPakFile(const FString& FilePath);

	virtual void Activate() override;
	
private:
	FString LocalFilePath;
	FString UploadDestinationURL;

	bool PrepareMultipartFormData(TArray<uint8>& DataToSend, const FString& Boundary) const;
	void OnHttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <= 3
	void OnHttpRequestProgress(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived);
#else
	void OnHttpRequestProgress64(FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived);
#endif
};
