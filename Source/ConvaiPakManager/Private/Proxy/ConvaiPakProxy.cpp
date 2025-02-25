// Fill out your copyright notice in the Description page of Project Settings.


#include "Proxy/ConvaiPakProxy.h"

#include "ConvaiUtils.h"
#include "HttpModule.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Utility/ConvaiPakUtilityLibrary.h"

namespace
{
	const FString CreatePakAssetURL = TEXT("http://35.239.167.143:8089/assets/upload");
}

UConvaiCreatePakAssetProxy* UConvaiCreatePakAssetProxy::CreatePakAssetProxy(const FConvaiCreatePakAssetParams& Params)
{
	UConvaiCreatePakAssetProxy* Proxy = NewObject<UConvaiCreatePakAssetProxy>();
	Proxy->AssociatedParams = Params;
	Proxy->URL = CreatePakAssetURL;
	return Proxy;
}

bool UConvaiCreatePakAssetProxy::ConfigureRequest(TSharedRef<IHttpRequest> Request, const TCHAR* Verb)
{
	if (!Super::ConfigureRequest(Request, ConvaiHttpConstants::POST))
	{
		return false;
	} 
         
	return true;
}

bool UConvaiCreatePakAssetProxy::AddContentToRequest(TArray<uint8>& DataToSend, const FString& Boundary)
{
	if (URL.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid file path or URL"));
		OnFailure.Broadcast(FString(TEXT("Invalid file path or URL")), 0.f);
		return false;
	}
	
	TArray<TSharedPtr<FJsonValue>> JsonTagsArray;
	for (const FString& Tag : AssociatedParams.Tags)
	{
		JsonTagsArray.Add(MakeShareable(new FJsonValueString(Tag)));
	}
	FString TagsJson;
	TSharedRef<TJsonWriter<>> TagsWriter = TJsonWriterFactory<>::Create(&TagsJson);
	FJsonSerializer::Serialize(JsonTagsArray, TagsWriter);

	// Append tags JSON array directly to form data
	FString TagsField = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"tags\"\r\n\r\n%s"), *Boundary, *TagsJson);
	DataToSend.Append((uint8*)TCHAR_TO_UTF8(*TagsField), TagsField.Len());

	if (!AssociatedParams.MetaData.IsEmpty())
	{
		const FString MetaDataField = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"metadata\"\r\n\r\n%s"),
			*Boundary, *AssociatedParams.MetaData);
		DataToSend.Append(reinterpret_cast<uint8*>(TCHAR_TO_UTF8(*MetaDataField)), MetaDataField.Len());
	}

	if (!AssociatedParams.Version.IsEmpty())
	{
		const FString VersionField = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"version\"\r\n\r\n%s"),
			*Boundary, *AssociatedParams.Version);
		DataToSend.Append(reinterpret_cast<uint8*>(TCHAR_TO_UTF8(*VersionField)), VersionField.Len());
	}

	if (!AssociatedParams.Entity_Type.IsEmpty())
	{
		const FString EntityTypeField = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"entity_type\"\r\n\r\n%s"),
			*Boundary, *AssociatedParams.Entity_Type);
		DataToSend.Append(reinterpret_cast<uint8*>(TCHAR_TO_UTF8(*EntityTypeField)), EntityTypeField.Len());
	}

	if(AssociatedParams.Thumbnail)
	{
		TArray<uint8> CompressedData;
		const FString TextureName = FString::Printf(TEXT("%s.png"), *AssociatedParams.Thumbnail->GetName());
		const FString MimeType = TEXT("application/octet-stream");

		if (UConvaiPakUtilityLibrary::Texture2DToBytes(AssociatedParams.Thumbnail, EImageFormat::PNG, CompressedData, 0))
		{
			const FString TextureHeader = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"thumbnail\"; filename=\"%s\"\r\nContent-Type: %s\r\n\r\n"), *Boundary, *TextureName, *MimeType);
			DataToSend.Append(reinterpret_cast<uint8*>(TCHAR_TO_UTF8(*TextureHeader)), TextureHeader.Len());
			DataToSend.Append(CompressedData);
		}
	}
	
	return true;
}

void UConvaiCreatePakAssetProxy::HandleSuccess()
{
	Super::HandleSuccess();
	OnSuccess.Broadcast(ResponseString, 100.f);
}

void UConvaiCreatePakAssetProxy::HandleFailure()
{
	Super::HandleFailure();
	OnFailure.Broadcast(ResponseString, 0.f);
}

/*
void UConvaiCreatePakAssetProxy::Activate()
{
	const TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
	const FString AuthKey = AuthHeaderAndKey.Value;
	const FString AuthHeader = AuthHeaderAndKey.Key;

	if (URL.IsEmpty() || AuthKey.IsEmpty() || AuthHeader.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid file path or URL"));
		OnFailure.Broadcast(FString(TEXT("Invalid file path or URL")), 0.f);
		return;
	}

	TArray<uint8> DataToSend;
	const FString Boundary = TEXT("ConvaiPakManagerPluginBoundary") + FString::FromInt(FDateTime::Now().GetTicks());
	if (!PrepareMultipartFormData(DataToSend, Boundary))
	{
		UE_LOG(LogTemp, Error, TEXT("Prepare multipart form data failed"));
		OnFailure.Broadcast(FString(TEXT("Prepare multipart form data failed")), 0.f);
		return;
	}

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(URL);
	HttpRequest->SetVerb(POST);
	HttpRequest->SetHeader(AuthHeader, AuthKey);
	HttpRequest->SetHeader(TEXT("Content-Type"), FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary));
	HttpRequest->SetHeader(TEXT("Content-Length"), FString::FromInt(DataToSend.Num()));
	HttpRequest->SetContent(DataToSend);
	
	//---------------------------------------------------------------------------------------------------------------
	TArray<uint8> ContentData = HttpRequest->GetContent();
	FString ContentString = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(ContentData.GetData())));

	UE_LOG(LogTemp, Warning, TEXT("Request Content: %s"), *ContentString);

	FString DebugDataString = FString(UTF8_TO_TCHAR(DataToSend.GetData()));
	UE_LOG(LogTemp, Warning, TEXT("Prepared Request Data: %s"), *DebugDataString);
	//---------------------------------------------------------------------------------------------------------------
	
	UE_LOG(LogTemp, Error, TEXT("Content Length : %d : %d"),HttpRequest->GetContentLength(), DataToSend.Num());

	HttpRequest->OnProcessRequestComplete().BindUObject(this, &ThisClass::OnHttpRequestComplete);
	HttpRequest->OnRequestProgress().BindUObject(this, &ThisClass::OnHttpRequestProgress);

	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogTemp, Error, TEXT("ProcessRequest Failed"));
		OnFailure.Broadcast(FString(TEXT("ProcessRequest Failed")), 0.f);
	}
}

bool UConvaiCreatePakAssetProxy::PrepareMultipartFormData(TArray<uint8>& DataToSend, const FString& Boundary) const
{
	// if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*AssociatedParams.PakFilePath))
	// {
	// 	UE_LOG(LogTemp, Error, TEXT("File not found: %s"), *AssociatedParams.PakFilePath);
	// 	return false;
	// }
	//
	// TArray<uint8> FileData;
	// if (!FFileHelper::LoadFileToArray(FileData, *AssociatedParams.PakFilePath))
	// {
	// 	UE_LOG(LogTemp, Error, TEXT("Failed to load file: %s"), *AssociatedParams.PakFilePath);
	// 	return false;
	// }
	//
	// const FString Header = FString::Printf(
	// 	TEXT("--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\nContent-Type: application/octet-stream\r\n\r\n"),
	// 	*Boundary, *FPaths::GetCleanFilename(AssociatedParams.PakFilePath));
	// DataToSend.Append(reinterpret_cast<uint8*>(TCHAR_TO_UTF8(*Header)), Header.Len());
	// DataToSend.Append(FileData);
	//
	// // Add closing boundary
	// const FString Footer = FString::Printf(TEXT("\r\n--%s--\r\n"), *Boundary);
	// DataToSend.Append(reinterpret_cast<uint8*>(TCHAR_TO_UTF8(*Footer)), Footer.Len());

	// TArray<TSharedPtr<FJsonValue>> JsonTagsArray;
	// for (const FString& Tag : AssociatedParams.Tags)
	// {
	// 	JsonTagsArray.Add(MakeShareable(new FJsonValueString(Tag)));
	// }
	// FString TagsJson;
	// const TSharedRef<TJsonWriter<>> TagsWriter = TJsonWriterFactory<>::Create(&TagsJson);
	// FJsonSerializer::Serialize(JsonTagsArray, TagsWriter);
	//
	// const FString TagsField = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"tags\"\r\n\r\n%s"), *Boundary, *TagsJson);
	// DataToSend.Append((uint8*)(TCHAR_TO_UTF8(*TagsField)), TagsField.Len());
	TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
	FString AuthKey = AuthHeaderAndKey.Value;
	FString AuthHeader = AuthHeaderAndKey.Key;

	if (URL.IsEmpty() || AuthKey.IsEmpty() || AuthHeader.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid file path or URL"));
		OnFailure.Broadcast(FString(TEXT("Invalid file path or URL")), 0.f);
		return false;
	}
	
	FString ExpIdField = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"experience_session_id\"\r\n\r\n%s"), *Boundary, *AuthKey);
	DataToSend.Append((uint8*)TCHAR_TO_UTF8(*ExpIdField), ExpIdField.Len());
	
	TArray<TSharedPtr<FJsonValue>> JsonTagsArray;
	for (const FString& Tag : AssociatedParams.Tags)
	{
		JsonTagsArray.Add(MakeShareable(new FJsonValueString(Tag)));
	}
	FString TagsJson;
	TSharedRef<TJsonWriter<>> TagsWriter = TJsonWriterFactory<>::Create(&TagsJson);
	FJsonSerializer::Serialize(JsonTagsArray, TagsWriter);

	// Append tags JSON array directly to form data
	FString TagsField = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"tags\"\r\n\r\n%s"), *Boundary, *TagsJson);
	DataToSend.Append((uint8*)TCHAR_TO_UTF8(*TagsField), TagsField.Len());

	if (!AssociatedParams.MetaData.IsEmpty())
	{
		const FString MetaDataField = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"metadata\"\r\n\r\n%s"),
			*Boundary, *AssociatedParams.MetaData);
		DataToSend.Append(reinterpret_cast<uint8*>(TCHAR_TO_UTF8(*MetaDataField)), MetaDataField.Len());
	}

	if (!AssociatedParams.Version.IsEmpty())
	{
		const FString VersionField = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"version\"\r\n\r\n%s"),
			*Boundary, *AssociatedParams.Version);
		DataToSend.Append(reinterpret_cast<uint8*>(TCHAR_TO_UTF8(*VersionField)), VersionField.Len());
	}

	if (!AssociatedParams.Entity_Type.IsEmpty())
	{
		const FString EntityTypeField = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"entity_type\"\r\n\r\n%s"),
			*Boundary, *AssociatedParams.Entity_Type);
		DataToSend.Append(reinterpret_cast<uint8*>(TCHAR_TO_UTF8(*EntityTypeField)), EntityTypeField.Len());
	}

	if(AssociatedParams.Thumbnail)
	{
		TArray<uint8> CompressedData;
		const FString TextureName = FString::Printf(TEXT("%s.png"), *AssociatedParams.Thumbnail->GetName());
		const FString MimeType = TEXT("application/octet-stream");

		if (UConvaiPakUtilityLibrary::Texture2DToBytes(AssociatedParams.Thumbnail, EImageFormat::PNG, CompressedData, 0))
		{
			const FString TextureHeader = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"thumbnail\"; filename=\"%s\"\r\nContent-Type: %s\r\n\r\n"), *Boundary, *TextureName, *MimeType);
			DataToSend.Append(reinterpret_cast<uint8*>(TCHAR_TO_UTF8(*TextureHeader)), TextureHeader.Len());
			DataToSend.Append(CompressedData);
		}
	}

	// Add closing boundary
	FString ClosingBoundary = FString::Printf(TEXT("\r\n------%s--\r\n"), *Boundary);
	DataToSend.Append((uint8*)TCHAR_TO_UTF8(*ClosingBoundary), ClosingBoundary.Len());
	
	return true;
}

void UConvaiCreatePakAssetProxy::OnHttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response,
	bool bWasSuccessful)
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
		OnFailure.Broadcast(FString(), 0.f);
		return;
	}
	if (!bWasSuccessful || Response->GetResponseCode() < 200 || Response->GetResponseCode() > 299)
	{
		UE_LOG(LogTemp, Warning, TEXT("HTTP request failed with code %d, and with response:%s"), Response->GetResponseCode(), *Response->GetContentAsString());
		OnFailure.Broadcast(FString(), 0.f);
		return;
	}

	const FString ResponseString = Response->GetContentAsString();
	OnSuccess.Broadcast(ResponseString, 100.f);
}

void UConvaiCreatePakAssetProxy::OnHttpRequestProgress(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
{
	if (Request.IsValid())
	{
		OnProgress.Broadcast(FString(), BytesReceived);
	}
}
*/