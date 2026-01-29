// Copyright 2022 Convai Inc. All Rights Reserved.

#include "Services/CPM_ConfigService.h"
#include "Utility/CPM_Log.h"
#include "Utility/CPM_UtilityLibrary.h"
#include "Proxy/CPM_GithubProxy.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// Static instance
TSharedPtr<FCPM_ConfigService> FCPM_ConfigServiceManager::Instance = nullptr;

TSharedPtr<FCPM_ConfigService> FCPM_ConfigServiceManager::Get()
{
	if (!Instance.IsValid())
	{
		Initialize();
	}
	return Instance;
}

void FCPM_ConfigServiceManager::Initialize()
{
	if (!Instance.IsValid())
	{
		Instance = MakeShared<FCPM_ConfigService>();
		Instance->Initialize();
		CPM_LOG(Log, TEXT("ConfigService initialized"));
	}
}

void FCPM_ConfigServiceManager::Shutdown()
{
	if (Instance.IsValid())
	{
		Instance->Shutdown();
		Instance.Reset();
		CPM_LOG(Log, TEXT("ConfigService shutdown"));
	}
}

FCPM_ConfigService::FCPM_ConfigService()
{
}

FCPM_ConfigService::~FCPM_ConfigService()
{
	Shutdown();
}

void FCPM_ConfigService::Initialize()
{
	CPM_LOG(Log, TEXT("FCPM_ConfigService::Initialize"));
}

void FCPM_ConfigService::Shutdown()
{
	ClearCache();
	CPM_LOG(Log, TEXT("FCPM_ConfigService::Shutdown"));
}

bool FCPM_ConfigService::HasValidCache() const
{
	FScopeLock Lock(&CacheMutex);
	
	if (!CachedConfig.IsSet())
	{
		return false;
	}
	
	double ElapsedSeconds = (FDateTime::UtcNow() - CacheTime).GetTotalSeconds();
	return ElapsedSeconds < CacheValiditySeconds;
}

TCPM_Result<FCPM_PackagingConfig> FCPM_ConfigService::GetCachedConfig() const
{
	FScopeLock Lock(&CacheMutex);
	
	if (!CachedConfig.IsSet())
	{
		return TCPM_Result<FCPM_PackagingConfig>::Failure(TEXT("No cached config available"));
	}
	
	return TCPM_Result<FCPM_PackagingConfig>::Success(CachedConfig.GetValue());
}

void FCPM_ConfigService::ClearCache()
{
	FScopeLock Lock(&CacheMutex);
	CachedConfig.Reset();
}

FCPM_PackagingConfig FCPM_ConfigService::ParseConfigJson(const FString& JsonString, bool bIsFromFallback)
{
	FCPM_PackagingConfig Config;
	Config.bIsFromFallback = bIsFromFallback;
	
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		CPM_LOG(Error, TEXT("Failed to parse config JSON"));
		return Config;
	}
	
	Config.RawJson = JsonObject;
	Config.bIsValid = true;
	
	// Parse unreal-engine section
	const TSharedPtr<FJsonObject>* UnrealEngineObj;
	if (JsonObject->TryGetObjectField(TEXT("unreal-engine"), UnrealEngineObj))
	{
		// Parse Windows config
		const TSharedPtr<FJsonObject>* WindowsObj;
		if ((*UnrealEngineObj)->TryGetObjectField(TEXT("windows"), WindowsObj))
		{
			(*WindowsObj)->TryGetBoolField(TEXT("should-package"), Config.Windows.bShouldPackage);
			(*WindowsObj)->TryGetStringField(TEXT("configuration"), Config.Windows.Configuration);
		}
		
		// Parse Linux config
		const TSharedPtr<FJsonObject>* LinuxObj;
		if ((*UnrealEngineObj)->TryGetObjectField(TEXT("linux"), LinuxObj))
		{
			(*LinuxObj)->TryGetBoolField(TEXT("should-package"), Config.Linux.bShouldPackage);
			(*LinuxObj)->TryGetStringField(TEXT("configuration"), Config.Linux.Configuration);
		}
	}
	
	// Parse raw-project-upload
	JsonObject->TryGetBoolField(TEXT("raw-project-upload"), Config.bRawProjectUpload);
	
	CPM_LOG(Log, TEXT("Parsed config - Windows: %s (%s), Linux: %s (%s), RawUpload: %s, FromFallback: %s"),
		Config.Windows.bShouldPackage ? TEXT("Yes") : TEXT("No"),
		*Config.Windows.Configuration,
		Config.Linux.bShouldPackage ? TEXT("Yes") : TEXT("No"),
		*Config.Linux.Configuration,
		Config.bRawProjectUpload ? TEXT("Yes") : TEXT("No"),
		bIsFromFallback ? TEXT("Yes") : TEXT("No"));
	
	return Config;
}

TCPM_Result<FCPM_PackagingConfig> FCPM_ConfigService::LoadFallbackConfig() const
{
	const FString FallbackPath = UCPM_UtilityLibrary::CPM_GetConfigDefaultsFilePath(LocalFallbackFileName);
	
	CPM_LOG(Log, TEXT("Loading fallback config from: %s"), *FallbackPath);
	
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FallbackPath))
	{
		return TCPM_Result<FCPM_PackagingConfig>::Failure(
			FString::Printf(TEXT("Failed to load fallback config from: %s"), *FallbackPath));
	}
	
	FCPM_PackagingConfig Config = ParseConfigJson(JsonString, true);
	if (!Config.bIsValid)
	{
		return TCPM_Result<FCPM_PackagingConfig>::Failure(TEXT("Failed to parse fallback config JSON"));
	}
	
	return TCPM_Result<FCPM_PackagingConfig>::Success(Config);
}

TSharedPtr<FCPM_AsyncOperation<FCPM_PackagingConfig>> FCPM_ConfigService::FetchPackagingConfig(bool bForceRefresh)
{
	// Check cache first
	if (!bForceRefresh && HasValidCache())
	{
		CPM_LOG(Log, TEXT("Using cached packaging config"));
		
		auto CachedResult = GetCachedConfig();
		
		// Return immediately with cached value
		auto Operation = MakeShared<FCPM_AsyncOperation<FCPM_PackagingConfig>>(
			[CachedResult](TSharedPtr<FCPM_CancellationToken> Token, TSharedPtr<ICPM_ProgressReporter> Progress) -> TCPM_Result<FCPM_PackagingConfig>
			{
				return CachedResult;
			}
		);
		Operation->Start();
		return Operation;
	}

	// Capture for lambda - use raw pointer since we're using the static manager
	TWeakPtr<FCPM_ConfigService> WeakSelf = FCPM_ConfigServiceManager::Get();

	auto Operation = MakeShared<FCPM_AsyncOperation<FCPM_PackagingConfig>>(
		[WeakSelf](TSharedPtr<FCPM_CancellationToken> CancelToken, TSharedPtr<ICPM_ProgressReporter> Progress) -> TCPM_Result<FCPM_PackagingConfig>
		{
			CPM_LOG(Log, TEXT("Fetching packaging config from GitHub: %s/%s/%s"), 
				GithubRepoName, GithubBranchName, GithubFileName);

			// We need to perform the HTTP request on the game thread since UObjects must be created there
			TSharedPtr<TPromise<TCPM_Result<FCPM_PackagingConfig>>> Promise = 
				MakeShared<TPromise<TCPM_Result<FCPM_PackagingConfig>>>();
			TFuture<TCPM_Result<FCPM_PackagingConfig>> Future = Promise->GetFuture();

			// Schedule GitHub proxy request on game thread
			AsyncTask(ENamedThreads::GameThread, [Promise, WeakSelf, CancelToken]()
			{
				if (CancelToken.IsValid() && CancelToken->IsCancellationRequested())
				{
					Promise->SetValue(TCPM_Result<FCPM_PackagingConfig>::Failure(TEXT("Cancelled")));
					return;
				}

				// Create the GitHub proxy
				UCPM_GetGithubRepoFileProxy* GithubProxy = UCPM_GetGithubRepoFileProxy::GetGithubRepoFileProxy(
					GithubRepoName,
					GithubBranchName,
					GithubFileName);

				if (!GithubProxy)
				{
					CPM_LOG(Error, TEXT("Failed to create GitHub proxy"));
					
					// Try fallback
					if (TSharedPtr<FCPM_ConfigService> This = WeakSelf.Pin())
					{
						auto FallbackResult = This->LoadFallbackConfig();
						if (FallbackResult.IsSuccess())
						{
							// Cache the fallback result
							FScopeLock Lock(&This->CacheMutex);
							This->CachedConfig = FallbackResult.GetValue();
							This->CacheTime = FDateTime::UtcNow();
						}
						Promise->SetValue(FallbackResult);
					}
					else
					{
						Promise->SetValue(TCPM_Result<FCPM_PackagingConfig>::Failure(TEXT("Service unavailable")));
					}
					return;
				}

				// Bind success handler
				GithubProxy->OnSuccess.AddLambda([Promise, WeakSelf](const FString& ResponseString)
				{
					CPM_LOG(Log, TEXT("GitHub config fetch successful"));
					
					FCPM_PackagingConfig Config = FCPM_ConfigService::ParseConfigJson(ResponseString, false);
					
					if (!Config.bIsValid)
					{
						CPM_LOG(Warning, TEXT("Failed to parse GitHub config, trying fallback"));
						
						// Try fallback
						if (TSharedPtr<FCPM_ConfigService> This = WeakSelf.Pin())
						{
							auto FallbackResult = This->LoadFallbackConfig();
							if (FallbackResult.IsSuccess())
							{
								FScopeLock Lock(&This->CacheMutex);
								This->CachedConfig = FallbackResult.GetValue();
								This->CacheTime = FDateTime::UtcNow();
							}
							Promise->SetValue(FallbackResult);
						}
						else
						{
							Promise->SetValue(TCPM_Result<FCPM_PackagingConfig>::Failure(TEXT("Service unavailable")));
						}
						return;
					}
					
					// Cache the result
					if (TSharedPtr<FCPM_ConfigService> This = WeakSelf.Pin())
					{
						FScopeLock Lock(&This->CacheMutex);
						This->CachedConfig = Config;
						This->CacheTime = FDateTime::UtcNow();
					}
					
					Promise->SetValue(TCPM_Result<FCPM_PackagingConfig>::Success(Config));
				});

				// Bind failure handler
				GithubProxy->OnFailure.AddLambda([Promise, WeakSelf](const FString& ErrorString)
				{
					CPM_LOG(Warning, TEXT("GitHub config fetch failed: %s, trying fallback"), *ErrorString);
					
					// Try fallback on failure
					if (TSharedPtr<FCPM_ConfigService> This = WeakSelf.Pin())
					{
						auto FallbackResult = This->LoadFallbackConfig();
						if (FallbackResult.IsSuccess())
						{
							FScopeLock Lock(&This->CacheMutex);
							This->CachedConfig = FallbackResult.GetValue();
							This->CacheTime = FDateTime::UtcNow();
						}
						Promise->SetValue(FallbackResult);
					}
					else
					{
						Promise->SetValue(TCPM_Result<FCPM_PackagingConfig>::Failure(
							FString::Printf(TEXT("GitHub failed: %s, fallback unavailable"), *ErrorString)));
					}
				});

				// Activate the proxy to start the request
				GithubProxy->Activate();
			});

			// Wait for the result
			Future.Wait();
			return Future.Get();
		}
	);

	Operation->Start();
	return Operation;
}
