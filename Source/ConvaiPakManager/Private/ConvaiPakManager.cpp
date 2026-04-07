// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConvaiPakManager.h"

#include "ISettingsModule.h"

#define LOCTEXT_NAMESPACE "FConvaiPakManagerModule"

void FConvaiPakManagerModule::StartupModule()
{
	ConvaiPakManagerSettings = NewObject<UCPM_Settings>(GetTransientPackage(), "ConvaiPakManagerSettings", RF_Standalone);
	ConvaiPakManagerSettings->AddToRoot();

	// Register settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "ConvaiPakManager",
			LOCTEXT("RuntimeSettingsName", "ConvaiPakManager"),
			LOCTEXT("RuntimeSettingsDescription", "Configure Convai Pak Manager settings"),
			ConvaiPakManagerSettings);
	}
}

void FConvaiPakManagerModule::ShutdownModule()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "ConvaiPakManager");
	}

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