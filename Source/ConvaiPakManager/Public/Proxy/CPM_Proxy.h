// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RestAPI/ConvaiAPIBase.h"
#include "Utility/CPM_Utils.h"
#include "CPM_Proxy.generated.h"

struct FCPM_CreatedAssets;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPM_AssetCreateDelegate, const FCPM_CreatedAssets&, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPM_AssetUploadDelegate, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPM_StringResponseDelegate, const FString&, ResponseString);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCPM_OnCancelledDelegate);

/* Create and update base proxy*/
UCLASS()
class CONVAIPAKMANAGER_API UCPM_CreateUpdatePakAssetBaseProxy : public UConvaiAPIBaseProxy
{
	GENERATED_BODY()
	
protected:
	virtual bool ConfigureRequest(TSharedRef<CONVAI_HTTP_REQUEST_INTERFACE> Request, const TCHAR* Verb) override;
	virtual bool AddContentToRequest(CONVAI_HTTP_PAYLOAD_ARRAY_TYPE& DataToSend, const FString& Boundary)  override;
	virtual bool AddContentToRequestAsString(TSharedPtr<FJsonObject>& ObjectToSend) override { return false; }
	
	FCPM_CreatePakAssetParams M_Params;
	bool M_bUpdateAsset = false;
	FString M_AssetId;
};

/* Create Proxy */
UCLASS()
class CONVAIPAKMANAGER_API UCPM_CreatePakAssetProxy : public UCPM_CreateUpdatePakAssetBaseProxy
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FCPM_AssetCreateDelegate OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FCPM_AssetCreateDelegate OnFailure;
	
	/**
	 * @param ChunkId          Which Chunk this Asset belongs to.
	 * @param EnvironmentSlug  The backend the request is going to, resolved now rather than when
	 *                         the response lands - by then the creator may have changed the URL.
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Create Pak Asset"), Category = "Convai|PakManager")
	static UCPM_CreatePakAssetProxy* CreatePakAssetProxy(const FCPM_CreatePakAssetParams& Params, int32 ChunkId, const FString& EnvironmentSlug);

	/**
	 * The server's answer, verbatim.
	 *
	 * What a Chunk records about its Asset must be what the server said, not a re-serialisation of
	 * the parsed struct - the record is the only copy of the AssetID in the creator's world, and a
	 * lossy round trip through our own parser is not a thing to discover later.
	 */
	const FString& GetResponseString() const { return ResponseString; }

protected:
	virtual void HandleSuccess() override;
	virtual void HandleFailure() override;

private:
	/** Where the AssetID is recorded the instant the create returns. See HandleSuccess. */
	int32 RecordChunkId = INDEX_NONE;
	FString RecordEnvironmentSlug;
};

/* Update Proxy */
UCLASS()
class CONVAIPAKMANAGER_API UCPM_UpdatePakAssetProxy : public UCPM_CreateUpdatePakAssetBaseProxy
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FCPM_StringResponseDelegate OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FCPM_StringResponseDelegate OnFailure;
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Update Pak Asset"), Category = "Convai|PakManager")
	static UCPM_UpdatePakAssetProxy* UpdatePakAssetProxy(const FString& AssetID, const FCPM_CreatePakAssetParams& Params);

protected:
	virtual void HandleSuccess() override;
	virtual void HandleFailure() override;
};


//-------------------------------------------Upload proxy-------------------------------------------

UCLASS(BlueprintType)
class CONVAIPAKMANAGER_API UCPM_UploadPakAssetProxy : public UConvaiAPIBaseProxy
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FCPM_AssetUploadDelegate OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FCPM_AssetUploadDelegate OnFailure;

	UPROPERTY(BlueprintAssignable)
	FCPM_AssetUploadDelegate OnProgress;

	UPROPERTY(BlueprintAssignable)
	FCPM_OnCancelledDelegate OnCancelled;
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Upload Pak Asset"), Category = "Convai|PakManager")
	static UCPM_UploadPakAssetProxy* UploadPakAssetProxy(const FString& UploadURL, const FString& PakFilePath, UCPM_UploadPakAssetProxy*& OutProxy);

	/** Cancel the ongoing upload request */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager")
	void CancelRequest();

	/** Check if the upload request is currently in progress */
	UFUNCTION(BlueprintPure, Category = "Convai|PakManager")
	bool IsRequestInProgress() const;

protected:
	virtual bool ConfigureRequest(TSharedRef<CONVAI_HTTP_REQUEST_INTERFACE> Request, const TCHAR* Verb) override;
	virtual bool AddContentToRequest(CONVAI_HTTP_PAYLOAD_ARRAY_TYPE& DataToSend, const FString& Boundary)  override;
	virtual bool AddContentToRequestAsString(TSharedPtr<FJsonObject>& ObjectToSend) override { return false; }
	virtual void HandleSuccess() override;
	virtual void HandleFailure() override;
	
private:
	FString M_PakFilePath;
	
	/** Stored reference to the active HTTP request for cancellation */
	TSharedPtr<CONVAI_HTTP_REQUEST_INTERFACE> ActiveHttpRequest;
	
	/** Flag to track if the request is currently in progress */
	bool bIsInProgress = false;
};


//---------------------------------------------Get Asset---------------------------------------------

/**
 * Reads an Asset back from the backend that holds it, and refreshes that Chunk's cached copy of the
 * server's document with what came back.
 *
 * The Pak Manager is not the only writer of an Asset - other tools edit the same records - so the
 * server, not the creator's project, is the record of what an Asset currently holds. What the
 * creator typed still wins at compose time; this only refreshes the half they did not type.
 *
 * A failure changes nothing. The cache keeps whatever it held, which is the last thing this backend
 * is known to have said, and every caller is free to carry on with it.
 */
UCLASS()
class CONVAIPAKMANAGER_API UCPM_GetAssetProxy : public UConvaiAPIBaseProxy
{
	GENERATED_BODY()
public:
	/** The server's metadata document for the Asset, verbatim. */
	UPROPERTY(BlueprintAssignable)
	FCPM_StringResponseDelegate OnSuccess;

	/** The body the server sent, or empty when there was none. */
	UPROPERTY(BlueprintAssignable)
	FCPM_StringResponseDelegate OnFailure;

	/**
	 * @param AssetID          Must be non-empty; the caller checks, because a proxy that refuses
	 *                         inside its own body-builder still sends the request.
	 * @param ChunkId          Whose cache the answer refreshes.
	 * @param EnvironmentSlug  The backend being asked, resolved now rather than when the response
	 *                         lands - by then the creator may have changed the URL.
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Get Pak Asset"), Category = "Convai|PakManager")
	static UCPM_GetAssetProxy* GetAssetProxy(const FString& AssetID, int32 ChunkId, const FString& EnvironmentSlug);

protected:
	virtual bool ConfigureRequest(TSharedRef<CONVAI_HTTP_REQUEST_INTERFACE> Request, const TCHAR* Verb) override;
	virtual bool AddContentToRequest(CONVAI_HTTP_PAYLOAD_ARRAY_TYPE& DataToSend, const FString& Boundary) override { return false; }
	virtual bool AddContentToRequestAsString(TSharedPtr<FJsonObject>& ObjectToSend) override;
	virtual void HandleSuccess() override;
	virtual void HandleFailure() override;

	FString AssociatedAssetId;
	int32 RecordChunkId = INDEX_NONE;
	FString RecordEnvironmentSlug;
};

//-------------------------------------------Delete Asset-------------------------------------------

/** Delete Asset*/
UCLASS()
class CONVAIPAKMANAGER_API UCPM_DeleteAssetProxy : public UConvaiAPIBaseProxy
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FCPM_StringResponseDelegate OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FCPM_StringResponseDelegate OnFailure;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Delete Asset"), Category = "Convai|PakManager")
	static UCPM_DeleteAssetProxy* DeleteAssetProxy(const FString& AssetID, const FString& Version);
		
protected:
	virtual bool ConfigureRequest(TSharedRef<CONVAI_HTTP_REQUEST_INTERFACE> Request, const TCHAR* Verb) override;
	virtual bool AddContentToRequest(CONVAI_HTTP_PAYLOAD_ARRAY_TYPE& DataToSend, const FString& Boundary)  override { return false; }
	virtual bool AddContentToRequestAsString(TSharedPtr<FJsonObject>& ObjectToSend) override;
	virtual void HandleSuccess() override;
	virtual void HandleFailure() override;
		
	FString AssociatedAssetIdD;
	FString AssociatedVersion;
};

//--------------------------------------------------------------------------------------------------