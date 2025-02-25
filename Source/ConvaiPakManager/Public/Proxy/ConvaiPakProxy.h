// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "RestAPI/ConvaiAPIBase.h"
#include "Utility/CPM_Utils.h"
#include "ConvaiPakProxy.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPakUploadDelegate, const FString&, ResponseString, float, Progress);


UCLASS()
class UConvaiCreatePakAssetProxy : public UConvaiAPIBaseProxy
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

protected:
	virtual bool ConfigureRequest(TSharedRef<IHttpRequest> Request, const TCHAR* Verb) override;
	virtual bool AddContentToRequest(TArray<uint8>& DataToSend, const FString& Boundary)  override;
	virtual bool AddContentToRequestAsString(TSharedPtr<FJsonObject>& ObjectToSend) override { return false; }
	virtual void HandleSuccess() override;
	virtual void HandleFailure() override;

private:
	FConvaiCreatePakAssetParams AssociatedParams;
};