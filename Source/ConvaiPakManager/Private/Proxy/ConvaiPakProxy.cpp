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

