// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "ConvaiPakManager.generated.h"

UCLASS(config = Engine, defaultconfig)
class CONVAIPAKMANAGER_API UCPM_Settings : public UObject
{
	GENERATED_BODY()

public:
	UCPM_Settings(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{}
	
	UPROPERTY(Config, EditAnywhere, Category = "Convai")
	TMap<FString, FString> CustomPrams;
};

class FConvaiPakManagerModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	static inline FConvaiPakManagerModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FConvaiPakManagerModule>("ConvaiPakManager");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("ConvaiPakManager");
	}

	virtual bool IsGameModule() const override
	{
		return true;
	}

	/** Getter for internal settings object to support runtime configuration changes */
	UCPM_Settings* GetConvaiPakManagerSettings() const;

protected:
	/** Module settings */
	UCPM_Settings* ConvaiPakManagerSettings;
};
