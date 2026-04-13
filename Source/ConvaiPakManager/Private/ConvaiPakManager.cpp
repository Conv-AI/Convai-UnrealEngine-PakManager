// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConvaiPakManager.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

#define LOCTEXT_NAMESPACE "FConvaiPakManagerModule"

void FConvaiPakManagerModule::StartupModule()
{
	ConvaiPakManagerSettings = NewObject<UCPM_Settings>(GetTransientPackage(), "ConvaiPakManagerSettings", RF_Standalone);
	ConvaiPakManagerSettings->AddToRoot();

#if WITH_EDITOR
	// Register settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "ConvaiPakManager",
			LOCTEXT("RuntimeSettingsName", "ConvaiPakManager"),
			LOCTEXT("RuntimeSettingsDescription", "Configure Convai Pak Manager settings"),
			ConvaiPakManagerSettings);
	}
#endif
}

void FConvaiPakManagerModule::ShutdownModule()
{
#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "ConvaiPakManager");
	}
#endif

	if (ConvaiPakManagerSettings)
	{
		if (!GExitPurge)
		{
			ConvaiPakManagerSettings->RemoveFromRoot();
		}

		ConvaiPakManagerSettings = nullptr;
	}
}

UCPM_Settings* FConvaiPakManagerModule::GetConvaiPakManagerSettings() const
{
	check(ConvaiPakManagerSettings);
	return ConvaiPakManagerSettings;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FConvaiPakManagerModule, ConvaiPakManager)
