// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RestAPI/ConvaiAPIBase.h"
#include "Utility/CPM_Utils.h"
#include "CPM_Proxy.generated.h"

struct FCPM_CreatedAssets;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCPM_AssetCreateDelegate, const FCPM_CreatedAssets&, Response, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPM_GetAssetsHttpResponseCallbackDelegate, const FCPM_AssetResponse&, AssetData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPM_AssetUploadDelegate, float, Progress);

UCLASS()
class UCPM_CreatePakAssetProxy : public UConvaiAPIBaseProxy
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FCPM_AssetCreateDelegate OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FCPM_AssetCreateDelegate OnFailure;

	UPROPERTY(BlueprintAssignable)
	FCPM_AssetCreateDelegate OnProgress;
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Create Pak Asset"), Category = "Convai|PakManager")
	static UCPM_CreatePakAssetProxy* CreatePakAssetProxy(const FCPM_CreatePakAssetParams& Params);

protected:
	virtual bool ConfigureRequest(TSharedRef<IConvaihttpRequest> Request, const TCHAR* Verb) override;
	virtual bool AddContentToRequest(TArray64<uint8>& DataToSend, const FString& Boundary)  override;
	virtual bool AddContentToRequestAsString(TSharedPtr<FJsonObject>& ObjectToSend) override { return false; }
	virtual void HandleSuccess() override;
	virtual void HandleFailure() override;

private:
	FCPM_CreatePakAssetParams M_Params;
};



UCLASS()
class UCPM_UploadPakAssetProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FCPM_AssetUploadDelegate OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FCPM_AssetUploadDelegate OnFailure;

	UPROPERTY(BlueprintAssignable)
	FCPM_AssetUploadDelegate OnProgress;
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Upload Pak Asset"), Category = "Convai|PakManager")
	static UCPM_UploadPakAssetProxy* UploadPakAssetProxy(const FString& UploadURL, const FString& PakFilePath);

	virtual void Activate() override;
	
private:
	FString URL;
	FString M_PakFilePath;
};




//-------------------------------------------Get Asset-------------------------------------------

/** Get Asset */
UCLASS()
class UCPM_GetAssetMetaDataProxy : public UConvaiAPIBaseProxy
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FCPM_GetAssetsHttpResponseCallbackDelegate OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FCPM_GetAssetsHttpResponseCallbackDelegate OnFailure;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Get Asset Metadata" , WorldContext = "WorldContextObject"), Category = "Convai|PakManager")
	static UCPM_GetAssetMetaDataProxy* GetAssetProxy(UObject* WorldContextObject, FString AssetID);

protected:
	virtual bool ConfigureRequest(TSharedRef<IConvaihttpRequest> Request, const TCHAR* Verb) override;
	virtual bool AddContentToRequest(TArray64<uint8>& DataToSend, const FString& Boundary)  override { return false; }
	virtual bool AddContentToRequestAsString(TSharedPtr<FJsonObject>& ObjectToSend) override;
	virtual void HandleSuccess() override;
	virtual void HandleFailure() override;

public:
	FString AssociatedAssetIdD;
	FCPM_AssetResponse AssetResponse;
};