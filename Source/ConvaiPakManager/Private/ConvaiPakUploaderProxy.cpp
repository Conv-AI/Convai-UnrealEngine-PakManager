// Fill out your copyright notice in the Description page of Project Settings.


#include "ConvaiPakUploaderProxy.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	const FString PakUploadURL = TEXT("");
}

UConvaiPakUploaderProxy* UConvaiPakUploaderProxy::UploadPakFile(const FString& FilePath)
{
	UConvaiPakUploaderProxy* Proxy = NewObject<UConvaiPakUploaderProxy>();
	Proxy->LocalFilePath = FilePath;
	Proxy->UploadDestinationURL = PakUploadURL; 
	return Proxy;
}

void UConvaiPakUploaderProxy::Activate()
{
	if (LocalFilePath.IsEmpty() || UploadDestinationURL.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid file path or URL"));
		OnFailure.Broadcast(false, 0.f);
		return;
	}
	
	// Prepare multipart form data
	TArray<uint8> DataToSend;
	const FString Boundary = TEXT("ConvaiPakManagerPluginBoundary") + FString::FromInt(FDateTime::Now().GetTicks());
	if (!PrepareMultipartFormData(DataToSend, Boundary))
	{
		OnFailure.Broadcast(false, 0.f);
		return;
	}

	// Create and configure HTTP request
	const TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(UploadDestinationURL);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary));
	HttpRequest->SetHeader(TEXT("Content-Length"), FString::FromInt(DataToSend.Num()));
	HttpRequest->SetContent(DataToSend);

	// Bind delegates
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UConvaiPakUploaderProxy::OnHttpRequestComplete);
	HttpRequest->OnRequestProgress().BindUObject(this, &UConvaiPakUploaderProxy::OnHttpRequestProgress);

	HttpRequest->ProcessRequest();
}

bool UConvaiPakUploaderProxy::PrepareMultipartFormData(TArray<uint8>& DataToSend, const FString& Boundary) const
{
	// Validate file
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*LocalFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("File not found: %s"), *LocalFilePath);
		return false;
	}

	// Load file data
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *LocalFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load file: %s"), *LocalFilePath);
		return false;
	}

	// Add header for the file
	const FString Header = FString::Printf(
		TEXT("--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\nContent-Type: application/octet-stream\r\n\r\n"),
		*Boundary, *FPaths::GetCleanFilename(LocalFilePath));
	DataToSend.Append(reinterpret_cast<uint8*>(TCHAR_TO_UTF8(*Header)), Header.Len());

	// Add file data
	DataToSend.Append(FileData);

	// Add closing boundary
	const FString Footer = FString::Printf(TEXT("\r\n--%s--\r\n"), *Boundary);
	DataToSend.Append(reinterpret_cast<uint8*>(TCHAR_TO_UTF8(*Footer)), Footer.Len());

	return true;
}


void UConvaiPakUploaderProxy::OnHttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() != 200)
	{
		UE_LOG(LogTemp, Error, TEXT("HTTP request failed: %s"), *Response->GetContentAsString());
		OnFailure.Broadcast(false, 0.f);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("HTTP request succeeded: %s"), *Response->GetContentAsString());
	OnSuccess.Broadcast(true, 100.f);
}

void UConvaiPakUploaderProxy::OnHttpRequestProgress(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
{
	const int32 TotalBytes = Request->GetContentLength();
	const float Progress = TotalBytes > 0 ? static_cast<float>(BytesSent) / TotalBytes : 0.0f;

	UE_LOG(LogTemp, Log, TEXT("Upload progress: %.2f%%"), Progress * 100);
	OnProgress.Broadcast(false, Progress);
}
