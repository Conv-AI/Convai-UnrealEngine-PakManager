// Fill out your copyright notice in the Description page of Project Settings.


#include "Proxy/CPM_Proxy.h"
#include "Chunk/CPM_Chunk.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Texture2D.h"
#include "Utility/CPM_Log.h"
#include "Utility/CPM_UtilityLibrary.h"
#include "ConvaiUtils.h"

namespace
{
    static FString CreatePakAssetURL() { return UConvaiURL::GetFullURL(TEXT("assets/upload"), false); }
    static FString UpdatePakAssetURL() { return UConvaiURL::GetFullURL(TEXT("assets/update"), false); }
    static FString DeletePakAssetURL() { return UConvaiURL::GetFullURL(TEXT("assets/delete"), false); }
    static FString GetPakAssetURL()    { return UConvaiURL::GetFullURL(TEXT("assets/get"), false); }
}


bool UCPM_CreateUpdatePakAssetBaseProxy::ConfigureRequest(TSharedRef<CONVAI_HTTP_REQUEST_INTERFACE> Request, const TCHAR* Verb)
{
	if (!Super::ConfigureRequest(Request, ConvaiHttpConstants::POST))
	{
		return false;
	} 
         
	return true;
}

bool UCPM_CreateUpdatePakAssetBaseProxy::AddContentToRequest(CONVAI_HTTP_PAYLOAD_ARRAY_TYPE& DataToSend, const FString& Boundary)
{
	if (URL.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid file path or URL"));
		return false;
	}

	if (M_Params.Tags.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> JsonTagsArray;
		for (const FString& Tag : M_Params.Tags)
		{
			JsonTagsArray.Add(MakeShareable(new FJsonValueString(Tag)));
		}
		FString TagsJson;
		const TSharedRef<TJsonWriter<>> TagsWriter = TJsonWriterFactory<>::Create(&TagsJson);
		FJsonSerializer::Serialize(JsonTagsArray, TagsWriter);

		// Append tags JSON array directly to form data
		const FString TagsField = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"tags\"\r\n\r\n%s"), *Boundary, *TagsJson);
		DataToSend.Append(reinterpret_cast<uint8*>(TCHAR_TO_UTF8(*TagsField)), TagsField.Len());
	}
	
	if (!M_Params.MetaData.IsEmpty())
	{
		const FString MetaDataField = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"metadata\"\r\n\r\n%s"),
			*Boundary, *M_Params.MetaData);
		DataToSend.Append(reinterpret_cast<uint8*>(TCHAR_TO_UTF8(*MetaDataField)), MetaDataField.Len());
	}

	if (!M_Params.Version.IsEmpty())
	{
		const FString VersionField = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"version\"\r\n\r\n%s"),
			*Boundary, *M_Params.Version);
		DataToSend.Append(reinterpret_cast<uint8*>(TCHAR_TO_UTF8(*VersionField)), VersionField.Len());
	}

	if (M_bUpdateAsset)
	{
		if (!M_AssetId.IsEmpty())
		{
			const FString AssetIDField = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"asset_id\"\r\n\r\n%s"),
				*Boundary, *M_AssetId);
			DataToSend.Append(reinterpret_cast<uint8*>(TCHAR_TO_UTF8(*AssetIDField)), AssetIDField.Len());
		}
		else
		{
			UCPM_UtilityLibrary::CPM_LogMessage(TEXT("Empty Asset"), ECPM_LogLevel::Error);
		}
	}
	else
	{
		if (!M_Params.Entity_Type.IsEmpty())
		{
			const FString EntityTypeField = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"entity_type\"\r\n\r\n%s"),
				*Boundary, *M_Params.Entity_Type);
			DataToSend.Append(reinterpret_cast<uint8*>(TCHAR_TO_UTF8(*EntityTypeField)), EntityTypeField.Len());
		}
		else
		{
			UCPM_UtilityLibrary::CPM_LogMessage(TEXT("Empty Type"), ECPM_LogLevel::Error);
		}
	}

	if (!M_Params.Visiblity.IsEmpty())
	{
		const FString VisiblityField = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"visibility\"\r\n\r\n%s"),
			*Boundary, *M_Params.Visiblity);
		DataToSend.Append(reinterpret_cast<uint8*>(TCHAR_TO_UTF8(*VisiblityField)), VisiblityField.Len());
	}
	
	if(M_Params.Thumbnail)
	{
		TArray<uint8> CompressedData;
		const FString TextureName = FString::Printf(TEXT("%s.png"), *M_Params.Thumbnail->GetName());
		const FString MimeType = TEXT("application/octet-stream");

		if (UCPM_UtilityLibrary::Texture2DToBytes(M_Params.Thumbnail, EImageFormat::PNG, CompressedData, 0))
		{
			const FString TextureHeader = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"thumbnail\"; filename=\"%s\"\r\nContent-Type: %s\r\n\r\n"), *Boundary, *TextureName, *MimeType);
			DataToSend.Append(reinterpret_cast<uint8*>(TCHAR_TO_UTF8(*TextureHeader)), TextureHeader.Len());
			DataToSend.Append(CompressedData);
		}
	}
	
	return true;
}


UCPM_CreatePakAssetProxy* UCPM_CreatePakAssetProxy::CreatePakAssetProxy(
	const FCPM_CreatePakAssetParams& Params, const int32 ChunkId, const FString& EnvironmentSlug)
{
	UCPM_CreatePakAssetProxy* Proxy = NewObject<UCPM_CreatePakAssetProxy>();
	Proxy->M_Params = Params;
	Proxy->URL = CreatePakAssetURL();
	Proxy->RecordChunkId = ChunkId;
	Proxy->RecordEnvironmentSlug = EnvironmentSlug;
	return Proxy;
}

void UCPM_CreatePakAssetProxy::HandleSuccess()
{
	Super::HandleSuccess();
	FCPM_CreatedAssets CreatedAssets;

	if (UCPM_UtilityLibrary::GetCreatedAssetsFromJSON(ResponseString, CreatedAssets))
	{
		// Recorded HERE rather than by the last Job of the publish, which is what holds the window
		// in which the Asset exists on Convai and nothing here names it to well under a second
		// instead of a whole multi-gigabyte upload.
		const FString PakMetaData = CreatedAssets.Assets.IsValidIndex(0) ? CreatedAssets.Assets[0].Asset.MetadataString : FString();
		if (!PakMetaData.IsEmpty())
		{
			// An empty echo used to be written anyway, leaving a zero-byte document that every later
			// compose refuses to parse.
			ConvaiPakManager::Chunk::WritePakMetadata(RecordChunkId, RecordEnvironmentSlug, PakMetaData);
		}

		if (ConvaiPakManager::Chunk::WriteCreateAssetData(RecordChunkId, RecordEnvironmentSlug, ResponseString))
		{
			OnSuccess.Broadcast(CreatedAssets);
			return;
		}
	}

	OnFailure.Broadcast(CreatedAssets);
}

void UCPM_CreatePakAssetProxy::HandleFailure()
{
	Super::HandleFailure();
	OnFailure.Broadcast(FCPM_CreatedAssets());
}


UCPM_UpdatePakAssetProxy* UCPM_UpdatePakAssetProxy::UpdatePakAssetProxy(const FString& AssetID,
	const FCPM_CreatePakAssetParams& UpdateParams)
{
	UCPM_UpdatePakAssetProxy* Proxy = NewObject<UCPM_UpdatePakAssetProxy>();
	Proxy->M_Params = UpdateParams;
	Proxy->URL = UpdatePakAssetURL();
	Proxy->M_AssetId = AssetID;
	Proxy->M_bUpdateAsset = true;
	return Proxy;
}

void UCPM_UpdatePakAssetProxy::HandleSuccess()
{
	Super::HandleSuccess();

	FString MintedUrl;
	if (UCPM_UtilityLibrary::GetMintedUploadUrl(ResponseString, MintedUrl))
	{
		OnSuccess.Broadcast(MintedUrl);
		return;
	}

	UCPM_UtilityLibrary::CPM_LogMessage(
		FString::Printf(TEXT("assets/update minted no upload URL. The server said: %s"), *ResponseString),
		ECPM_LogLevel::Error);
	OnFailure.Broadcast(ResponseString);
}

void UCPM_UpdatePakAssetProxy::HandleFailure()
{
	Super::HandleFailure();
	OnFailure.Broadcast(ResponseString);
}



UCPM_UploadPakAssetProxy* UCPM_UploadPakAssetProxy::UploadPakAssetProxy(const FString& UploadURL, const FString& PakFilePath, UCPM_UploadPakAssetProxy*& OutProxy)
{
	UCPM_UploadPakAssetProxy* Proxy = NewObject<UCPM_UploadPakAssetProxy>();
	Proxy->URL = UploadURL;
	Proxy->M_PakFilePath = PakFilePath;
	OutProxy = Proxy;
	return Proxy;
}

bool UCPM_UploadPakAssetProxy::ConfigureRequest(TSharedRef<CONVAI_HTTP_REQUEST_INTERFACE> Request, const TCHAR* Verb)
{
	if (!Super::ConfigureRequest(Request, ConvaiHttpConstants::PUT))
	{
		return false;
	}

	// Store the request reference for cancellation
	ActiveHttpRequest = Request;
	bIsInProgress = true;

	Request->SetHeader(TEXT("access-control-allow-origin"), TEXT("*"));
	Request->SetHeader(TEXT("x-goog-content-length-range"), TEXT("0,10485760000"));
	
	TWeakObjectPtr<UCPM_UploadPakAssetProxy> WeakThis(this);
	#if !USE_CONVAI_HTTP && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4))
	Request->OnRequestProgress64().BindLambda(
#else
	Request->OnRequestProgress().BindLambda(
#endif
	[WeakThis](CONVAI_HTTP_REQUEST_PTR InRequest, CONVAI_HTTP_DOWN_PROGRESS_TYPE BytesSent, CONVAI_HTTP_DOWN_PROGRESS_TYPE BytesReceived)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		
		uint64 TotalBytes = InRequest->GetContentLength();
		float UploadProgress = TotalBytes > 0 ? (float)BytesSent / (float)TotalBytes : 0.0f;
	
		WeakThis->OnProgress.Broadcast(UploadProgress);
	});
		
	return true;
}

void UCPM_UploadPakAssetProxy::CancelRequest()
{
	if (ActiveHttpRequest.IsValid() && bIsInProgress)
	{
		ActiveHttpRequest->CancelRequest();
		bIsInProgress = false;
		ActiveHttpRequest.Reset();
		
		OnCancelled.Broadcast();
	}
}

bool UCPM_UploadPakAssetProxy::IsRequestInProgress() const
{
	return bIsInProgress;
}

bool UCPM_UploadPakAssetProxy::AddContentToRequest(CONVAI_HTTP_PAYLOAD_ARRAY_TYPE& DataToSend, const FString& Boundary)
{
	if (URL.IsEmpty() || M_PakFilePath.IsEmpty())
	{
		UCPM_UtilityLibrary::CPM_LogMessage(TEXT("Invalid file URL or path"), ECPM_LogLevel::Error);
		OnFailure.Broadcast(0.f);
		return false;
	}
	
	if (!FFileHelper::LoadFileToArray(DataToSend, *M_PakFilePath))
	{
		UCPM_UtilityLibrary::CPM_LogMessage(FString::Printf(TEXT("Failed to load file: %s"), *M_PakFilePath), ECPM_LogLevel::Error);
		return false;
	}

	return true;
}

void UCPM_UploadPakAssetProxy::HandleSuccess()
{
	bIsInProgress = false;
	ActiveHttpRequest.Reset();
	
	Super::HandleSuccess();
	OnSuccess.Broadcast(100.f);
}

void UCPM_UploadPakAssetProxy::HandleFailure()
{
	bIsInProgress = false;
	ActiveHttpRequest.Reset();
	
	Super::HandleFailure();
	OnFailure.Broadcast(0.f);
}


// Get asset
UCPM_GetAssetProxy* UCPM_GetAssetProxy::GetAssetProxy(
	const FString& AssetID, const int32 ChunkId, const FString& EnvironmentSlug)
{
	UCPM_GetAssetProxy* Proxy = NewObject<UCPM_GetAssetProxy>();
	Proxy->URL = GetPakAssetURL();
	Proxy->AssociatedAssetId = AssetID;
	Proxy->RecordChunkId = ChunkId;
	Proxy->RecordEnvironmentSlug = EnvironmentSlug;
	return Proxy;
}

bool UCPM_GetAssetProxy::ConfigureRequest(TSharedRef<CONVAI_HTTP_REQUEST_INTERFACE> Request, const TCHAR* Verb)
{
	return Super::ConfigureRequest(Request, ConvaiHttpConstants::POST);
}

bool UCPM_GetAssetProxy::AddContentToRequestAsString(TSharedPtr<FJsonObject>& ObjectToSend)
{
	Super::AddContentToRequestAsString(ObjectToSend);

	// Not validated here: returning false from this override does not stop the request, it only
	// sends it without a body. The caller is the one that refuses an empty id.
	ObjectToSend->SetStringField(TEXT("asset_id"), AssociatedAssetId);
	return true;
}

void UCPM_GetAssetProxy::HandleSuccess()
{
	Super::HandleSuccess();

	FString Document;
	if (!UCPM_UtilityLibrary::GetAssetMetadataFromJSON(ResponseString, Document))
	{
		// Left exactly as it was. An answer this cannot read is not evidence the Asset changed, and
		// overwriting the cache with nothing would make every later compose refuse to parse it.
		//
		// The body is NOT logged: an assets/get response carries a signed URL per Version, and those
		// are credentials that outlive the log file a creator pastes into a support ticket.
		CPM_LOG(Warning, TEXT("assets/get carried no metadata document for asset %s (%d bytes); ")
			TEXT("chunk %d keeps the document it had."),
			*AssociatedAssetId, ResponseString.Len(), RecordChunkId);
		OnFailure.Broadcast(ResponseString);
		return;
	}

	ConvaiPakManager::Chunk::WritePakMetadata(RecordChunkId, RecordEnvironmentSlug, Document);
	OnSuccess.Broadcast(Document);
}

void UCPM_GetAssetProxy::HandleFailure()
{
	Super::HandleFailure();
	OnFailure.Broadcast(ResponseString);
}
// END Get asset

// Delete asset
UCPM_DeleteAssetProxy* UCPM_DeleteAssetProxy::DeleteAssetProxy(const FString& AssetID, const FString& Version)
{
	UCPM_DeleteAssetProxy* Proxy = NewObject<UCPM_DeleteAssetProxy>();
	Proxy->URL = DeletePakAssetURL();
	Proxy->AssociatedAssetIdD = AssetID;
	Proxy->AssociatedVersion = Version;
	return Proxy;
}

bool UCPM_DeleteAssetProxy::ConfigureRequest(TSharedRef<CONVAI_HTTP_REQUEST_INTERFACE> Request, const TCHAR* Verb)
{
	if (!Super::ConfigureRequest(Request, ConvaiHttpConstants::POST))
	{
		return false;
	}

	return true; 
}

bool UCPM_DeleteAssetProxy::AddContentToRequestAsString(TSharedPtr<FJsonObject>& ObjectToSend)
{
	Super::AddContentToRequestAsString(ObjectToSend);

	if (!UConvaiFormValidation::ValidateInputText(AssociatedAssetIdD))
	{
		HandleFailure();
		return false;
	}

	ObjectToSend->SetStringField(TEXT("asset_id"), AssociatedAssetIdD);

	if (!AssociatedVersion.IsEmpty())
	{
		ObjectToSend->SetStringField(TEXT("version"), AssociatedVersion);
	}
	
	return true;
}

void UCPM_DeleteAssetProxy::HandleSuccess()
{
	Super::HandleSuccess();
	OnSuccess.Broadcast(ResponseString);
}

void UCPM_DeleteAssetProxy::HandleFailure()
{
	Super::HandleFailure();
	OnFailure.Broadcast(TEXT("Http req failed"));
}
// END Delete asset