// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Publish/CPM_Compatibility.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/EngineVersion.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ConvaiPakManager::Compatibility
{
namespace
{
	FString ReadStringField(const FString& Json, const TCHAR* Field)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return FString();
		}

		FString Value;
		Root->TryGetStringField(Field, Value);
		return Value;
	}

	/**
	 * FEngineVersion::Parse insists on Major.Minor.Patch, and the two pins are written differently -
	 * "2.3.11" in the `.uplugin`, "5.8" in Version.json. A missing patch is filled rather than read
	 * as unparsable, because every caller fails open and "5.8" would then match every engine.
	 */
	bool ParseVersion(const FString& Text, FEngineVersion& OutVersion)
	{
		FString Padded = Text.TrimStartAndEnd();
		if (Padded.IsEmpty())
		{
			return false;
		}

		int32 Components = 1;
		for (const TCHAR Character : Padded)
		{
			Components += Character == TEXT('.') ? 1 : 0;
		}
		for (; Components < 3; ++Components)
		{
			Padded += TEXT(".0");
		}

		return FEngineVersion::Parse(Padded, OutVersion);
	}
}

FString ParsePluginVersionName(const FString& UpluginJson)
{
	return ReadStringField(UpluginJson, TEXT("VersionName"));
}

FString ParseTargetEngineVersion(const FString& VersionJson)
{
	return ReadStringField(VersionJson, TEXT("target-ue-version"));
}

bool IsNewerVersion(const FString& Installed, const FString& Latest)
{
	FEngineVersion Have;
	FEngineVersion Available;
	if (!ParseVersion(Installed, Have) || !ParseVersion(Latest, Available))
	{
		return false;
	}

	return FEngineVersion::GetNewest(Have, Available, nullptr) == EVersionComparison::Second;
}

bool EngineMatchesTarget(const FString& Engine, const FString& Target)
{
	FEngineVersion Wanted;
	FEngineVersion Running;
	if (!ParseVersion(Target, Wanted) || !ParseVersion(Engine, Running))
	{
		return true;
	}

	return Running.GetMajor() == Wanted.GetMajor() && Running.GetMinor() == Wanted.GetMinor();
}

FString InstalledToolVersion()
{
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ConvaiPakManager")))
	{
		return Plugin->GetDescriptor().VersionName;
	}
	return FString();
}
}
