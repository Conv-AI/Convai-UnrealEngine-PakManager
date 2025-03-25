// Fill out your copyright notice in the Description page of Project Settings.


#include "Proxy/CPM_Proxy.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Utility/CPM_UtilityLibrary.h"
#include "ConvaiUtils.h"

namespace
{
	const FString CreatePakAssetURL = TEXT("http://35.239.167.143:8089/assets/upload");
	const FString UpdatePakAssetURL = TEXT("http://35.239.167.143:8089/assets/update");
	const FString GetAssetURL = TEXT("http://35.239.167.143:8089/assets/get");
	//const FString GetAssetURL = TEXT("https://beta.convai.com/assets/get");
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


UCPM_CreatePakAssetProxy* UCPM_CreatePakAssetProxy::CreatePakAssetProxy(const FCPM_CreatePakAssetParams& Params)
{
	UCPM_CreatePakAssetProxy* Proxy = NewObject<UCPM_CreatePakAssetProxy>();
	Proxy->M_Params = Params;
	Proxy->URL = CreatePakAssetURL;
	return Proxy;
}

void UCPM_CreatePakAssetProxy::HandleSuccess()
{
	Super::HandleSuccess();
	FCPM_CreatedAssets CreatedAssets;
	
	if (UCPM_UtilityLibrary::GetCreatedAssetsFromJSON(ResponseString, CreatedAssets))
	{
		if (UCPM_UtilityLibrary::SaveConvaiAssetData(ResponseString))
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
	Proxy->URL = UpdatePakAssetURL;
	Proxy->M_AssetId = AssetID;
	Proxy->M_bUpdateAsset = true;
	return Proxy;
}

void UCPM_UpdatePakAssetProxy::HandleSuccess()
{
	Super::HandleSuccess();

	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		const TSharedPtr<FJsonObject>* UploadUrlsObject;
		if (JsonObject->TryGetObjectField(TEXT("upload_urls"), UploadUrlsObject))
		{
			for (const auto& Pair : (*UploadUrlsObject)->Values)
			{
				const FString FirstUploadURL = Pair.Value->AsString();
				OnSuccess.Broadcast(FirstUploadURL);
				return;
			}
		}
	}

	OnFailure.Broadcast(ResponseString);
}

void UCPM_UpdatePakAssetProxy::HandleFailure()
{
	Super::HandleFailure();
	OnFailure.Broadcast(ResponseString);
}



UCPM_UploadPakAssetProxy* UCPM_UploadPakAssetProxy::UploadPakAssetProxy(const FString& UploadURL, const FString& PakFilePath)
{
	UCPM_UploadPakAssetProxy* Proxy = NewObject<UCPM_UploadPakAssetProxy>();
	Proxy->URL = UploadURL;
	Proxy->M_PakFilePath = PakFilePath;
	return Proxy;
}

bool UCPM_UploadPakAssetProxy::ConfigureRequest(TSharedRef<CONVAI_HTTP_REQUEST_INTERFACE> Request, const TCHAR* Verb)
{
	if (!Super::ConfigureRequest(Request, ConvaiHttpConstants::PUT))
	{
		return false;
	}

	Request->SetHeader(TEXT("access-control-allow-origin"), TEXT("*"));
	Request->SetHeader(TEXT("x-goog-content-length-range"), TEXT("0,10485760000"));
	
	Request->OnRequestProgress().BindLambda(
	[&](CONVAI_HTTP_REQUEST_PTR InRequest, uint64 BytesSent, uint64 BytesReceived)
	{
		uint64 TotalBytes = InRequest->GetContentLength();
		float UploadProgress = TotalBytes > 0 ? (float)BytesSent / (float)TotalBytes : 0.0f;
	
		OnProgress.Broadcast(UploadProgress);
	});
		
	return true;
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
	Super::HandleSuccess();
	OnSuccess.Broadcast(100.f);
}

void UCPM_UploadPakAssetProxy::HandleFailure()
{
	Super::HandleFailure();
	OnFailure.Broadcast(0.f);
}


UCPM_GetAssetMetaDataProxy* UCPM_GetAssetMetaDataProxy::GetAssetProxy(UObject* WorldContextObject , FString AssetID)
{
    UCPM_GetAssetMetaDataProxy* Proxy = NewObject<UCPM_GetAssetMetaDataProxy>();
	Proxy->URL = GetAssetURL;
    Proxy->AssociatedAssetIdD = AssetID;
    return Proxy;
}

bool UCPM_GetAssetMetaDataProxy::ConfigureRequest(TSharedRef<CONVAI_HTTP_REQUEST_INTERFACE> Request, const TCHAR* Verb)
{
    if (!Super::ConfigureRequest(Request, ConvaiHttpConstants::POST))
    {
        return false;
    }

    return true;
} 

bool UCPM_GetAssetMetaDataProxy::AddContentToRequestAsString(TSharedPtr<FJsonObject>& ObjectToSend) 
{
	const TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
	const FString AuthKey = AuthHeaderAndKey.Value;

	if (!UConvaiFormValidation::ValidateAuthKey(AuthKey) || !UConvaiFormValidation::ValidateInputText(AssociatedAssetIdD))
	{
		HandleFailure();
		return false;
	}
	
	ObjectToSend->SetStringField(TEXT("experience_session_id"), AuthKey);
	ObjectToSend->SetStringField(TEXT("asset_id"), AssociatedAssetIdD);
	
    return true;
}

void UCPM_GetAssetMetaDataProxy::HandleSuccess()
{
    Super::HandleSuccess();
    
    if (UCPM_UtilityLibrary::ExtractAssetListFromResponseString(ResponseString, AssetResponse))
    {
        OnSuccess.Broadcast(AssetResponse, ResponseString);
    }
    else
    {
    	UCPM_UtilityLibrary::CPM_LogMessage(TEXT("Failed to parse response"), ECPM_LogLevel::Error);
        HandleFailure();
    }
}

void UCPM_GetAssetMetaDataProxy::HandleFailure()
{
    Super::HandleFailure();
    OnFailure.Broadcast(FCPM_AssetResponse(), ResponseString);
}