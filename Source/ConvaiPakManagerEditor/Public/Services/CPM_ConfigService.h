// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/ICPM_Service.h"
#include "Infrastructure/CPM_Result.h"
#include "Infrastructure/CPM_AsyncOperation.h"
#include "Dom/JsonObject.h"

/**
 * Platform-specific packaging configuration
 */
struct FCPM_PlatformConfig
{
	/** Whether to package for this platform */
	bool bShouldPackage = false;
	
	/** Build configuration (e.g., "Shipping", "Development") */
	FString Configuration = TEXT("Shipping");
};

/**
 * Packaging configuration loaded from GitHub (or local fallback)
 */
struct FCPM_PackagingConfig
{
	/** Windows platform config */
	FCPM_PlatformConfig Windows;
	
	/** Linux platform config */
	FCPM_PlatformConfig Linux;
	
	/** Whether to upload raw project files */
	bool bRawProjectUpload = true;
	
	/** Whether config is valid */
	bool bIsValid = false;
	
	/** Whether this config was loaded from fallback (local file) */
	bool bIsFromFallback = false;
	
	/** Raw JSON for additional fields */
	TSharedPtr<FJsonObject> RawJson;
	
	/** Get platforms that should be packaged */
	TArray<FString> GetPackagingPlatforms() const
	{
		TArray<FString> Platforms;
		if (Windows.bShouldPackage)
		{
			Platforms.Add(TEXT("Windows"));
		}
		if (Linux.bShouldPackage)
		{
			Platforms.Add(TEXT("Linux"));
		}
		return Platforms;
	}
};

/**
 * Config Service - Handles fetching and caching of remote configuration
 */
class CONVAIPAKMANAGEREDITOR_API FCPM_ConfigService : public ICPM_Service
{
public:
	FCPM_ConfigService();
	virtual ~FCPM_ConfigService();

	//~ ICPM_Service Interface
	virtual void Initialize() override;
	virtual void Shutdown() override;
	virtual FName GetServiceName() const override { return TEXT("ConfigService"); }

	/**
	 * Fetch the packaging config from GitHub
	 * Uses cached version if available and not expired
	 */
	TSharedPtr<FCPM_AsyncOperation<FCPM_PackagingConfig>> FetchPackagingConfig(bool bForceRefresh = false);

	/**
	 * Get cached config if available (synchronous)
	 */
	TCPM_Result<FCPM_PackagingConfig> GetCachedConfig() const;

	/**
	 * Clear the cached config
	 */
	void ClearCache();

	/**
	 * Check if we have a valid cached config
	 */
	bool HasValidCache() const;

private:
	/** Parse config JSON into FCPM_PackagingConfig */
	static FCPM_PackagingConfig ParseConfigJson(const FString& JsonString, bool bIsFromFallback);
	
	/** Load config from local fallback file */
	TCPM_Result<FCPM_PackagingConfig> LoadFallbackConfig() const;
	
	// Cache
	TOptional<FCPM_PackagingConfig> CachedConfig;
	FDateTime CacheTime;
	double CacheValiditySeconds = 300.0; // 5 minutes
	mutable FCriticalSection CacheMutex;

	// GitHub config source
	static constexpr const TCHAR* GithubRepoName = TEXT("Conv-AI/Convai-UnrealEngine-ModdingTool");
	static constexpr const TCHAR* GithubBranchName = TEXT("main");
	static constexpr const TCHAR* GithubFileName = TEXT("resources/asset_uploader_config.json");
	static constexpr const TCHAR* LocalFallbackFileName = TEXT("asset_uploader_config.json");
};

/**
 * Global accessor for the config service
 */
class CONVAIPAKMANAGEREDITOR_API FCPM_ConfigServiceManager
{
public:
	static TSharedPtr<FCPM_ConfigService> Get();
	static void Initialize();
	static void Shutdown();

private:
	static TSharedPtr<FCPM_ConfigService> Instance;
};
