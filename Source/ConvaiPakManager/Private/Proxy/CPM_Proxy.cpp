// Fill out your copyright notice in the Description page of Project Settings.


#include "Proxy/CPM_Proxy.h"
#include "HttpModule.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Utility/CPM_UtilityLibrary.h"

namespace
{
	const FString CreatePakAssetURL = TEXT("http://35.239.167.143:8089/assets/upload");
}

UCPM_CreatePakAssetProxy* UCPM_CreatePakAssetProxy::CreatePakAssetProxy(const FCPM_CreatePakAssetParams& Params)
{
	UCPM_CreatePakAssetProxy* Proxy = NewObject<UCPM_CreatePakAssetProxy>();
	Proxy->M_Params = Params;
	Proxy->URL = CreatePakAssetURL;
	return Proxy;
}

bool UCPM_CreatePakAssetProxy::ConfigureRequest(TSharedRef<IHttpRequest> Request, const TCHAR* Verb)
{
	if (!Super::ConfigureRequest(Request, ConvaiHttpConstants::POST))
	{
		return false;
	} 
         
	return true;
}

bool UCPM_CreatePakAssetProxy::AddContentToRequest(TArray<uint8>& DataToSend, const FString& Boundary)
{
	if (URL.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid file path or URL"));
		OnFailure.Broadcast(FCPM_CreatedAssets(), 0.f);
		return false;
	}
	
	TArray<TSharedPtr<FJsonValue>> JsonTagsArray;
	for (const FString& Tag : M_Params.Tags)
	{
		JsonTagsArray.Add(MakeShareable(new FJsonValueString(Tag)));
	}
	FString TagsJson;
	TSharedRef<TJsonWriter<>> TagsWriter = TJsonWriterFactory<>::Create(&TagsJson);
	FJsonSerializer::Serialize(JsonTagsArray, TagsWriter);

	// Append tags JSON array directly to form data
	FString TagsField = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"tags\"\r\n\r\n%s"), *Boundary, *TagsJson);
	DataToSend.Append((uint8*)TCHAR_TO_UTF8(*TagsField), TagsField.Len());

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

	if (!M_Params.Entity_Type.IsEmpty())
	{
		const FString EntityTypeField = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"entity_type\"\r\n\r\n%s"),
			*Boundary, *M_Params.Entity_Type);
		DataToSend.Append(reinterpret_cast<uint8*>(TCHAR_TO_UTF8(*EntityTypeField)), EntityTypeField.Len());
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

void UCPM_CreatePakAssetProxy::HandleSuccess()
{
	Super::HandleSuccess();
	FCPM_CreatedAssets CreatedAssets;
	if (UCPM_UtilityLibrary::GetCreatedAssetsFromJSON(ResponseString, CreatedAssets))
	{
		OnSuccess.Broadcast(CreatedAssets, 100.f);
		return;
	}
	
	UE_LOG(LogTemp, Error, TEXT("Parsing failed"));
	OnFailure.Broadcast(CreatedAssets, 0.f);
}

void UCPM_CreatePakAssetProxy::HandleFailure()
{
	Super::HandleFailure();
	OnFailure.Broadcast(FCPM_CreatedAssets(), 0.f);
}



UCPM_UploadPakAssetProxy* UCPM_UploadPakAssetProxy::UploadPakAssetProxy(const FString& UploadURL, const FString& PakFilePath)
{
	UCPM_UploadPakAssetProxy* Proxy = NewObject<UCPM_UploadPakAssetProxy>();
	Proxy->URL = UploadURL;
	Proxy->M_PakFilePath = PakFilePath;
	return Proxy;
}

void UCPM_UploadPakAssetProxy::Activate()
{
	if (URL.IsEmpty() || M_PakFilePath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid file URL or path"));
		OnFailure.Broadcast(0.f);
		return;
	}
	
	TArray<uint8> FileContent;
	if (!FFileHelper::LoadFileToArray(FileContent, *M_PakFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load file: %s"), *M_PakFilePath);
		return;
	}

	const TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetVerb("PUT");
	HttpRequest->SetURL(URL);
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
	HttpRequest->SetHeader(TEXT("access-control-allow-origin"), TEXT("*"));
	HttpRequest->SetHeader(TEXT("x-goog-content-length-range"), TEXT("0,2621440000"));
	HttpRequest->SetContent(FileContent);

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[&](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			if (!Response)
			{
				if (bWasSuccessful)
				{
					UE_LOG(LogTemp, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
				}

				OnFailure.Broadcast(0.f);
				return;
			}
			if (!bWasSuccessful || Response->GetResponseCode() < 200 || Response->GetResponseCode() > 299)
			{
				UE_LOG(LogTemp, Warning, TEXT("HTTP request failed with code %d, and with response:%s"), Response->GetResponseCode(), *Response->GetContentAsString());
				OnFailure.Broadcast(0.f);
				return;
			}
			
			OnSuccess.Broadcast(100.f);
		});

	HttpRequest->OnRequestProgress().BindLambda(
	[&](FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
	{
		int32 TotalBytes = Request->GetContentLength();
		float UploadProgress = TotalBytes > 0 ? (float)BytesSent / (float)TotalBytes : 0.0f;

		OnProgress.Broadcast(UploadProgress);
	});
	
	HttpRequest->ProcessRequest();
}
