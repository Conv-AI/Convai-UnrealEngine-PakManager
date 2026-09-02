// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Publish/CPM_PublishTypes.h"

#include "Misc/EngineVersion.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	/** Reads one platform's section. What it reads is judged by Validate, not here. */
	void ReadPlatform(const TSharedPtr<FJsonObject>& Engine, const TCHAR* Key, FCPM_PlatformPolicy& OutPolicy)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Engine->TryGetObjectField(Key, Object) || !Object || !Object->IsValid())
		{
			// A platform the policy does not mention is not packaged. Absent and false mean the same
			// thing here, which is why this is not an error.
			OutPolicy = FCPM_PlatformPolicy();
			return;
		}

		// Cleared first: the field carries a default for a policy typed into project settings, and a
		// JSON policy that names no configuration must be refused rather than inherit it.
		OutPolicy = FCPM_PlatformPolicy();
		OutPolicy.Configuration.Reset();

		(*Object)->TryGetBoolField(TEXT("should-package"), OutPolicy.bShouldPackage);
		(*Object)->TryGetStringField(TEXT("configuration"), OutPolicy.Configuration);
	}
}

FString FCPM_PakArtifact::VersionSlotFor(const ECPM_Platform Platform)
{
	// Named on the wire rather than by engine and platform: one archive of the creator's project
	// serves every engine version, which is the whole point of holding it.
	if (Platform == ECPM_Platform::Raw)
	{
		return TEXT("raw");
	}

	const TCHAR* Name =
		Platform == ECPM_Platform::Windows ? TEXT("Windows") :
		Platform == ECPM_Platform::Linux ? TEXT("Linux") : nullptr;
	if (!Name)
	{
		return FString();
	}

	const FEngineVersion& Engine = FEngineVersion::Current();
	return FString::Printf(TEXT("ue-%d.%d-%s"), Engine.GetMajor(), Engine.GetMinor(), Name);
}

bool FCPM_PublishPolicy::ParseFromJson(const FString& Json, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("the publish policy is not valid JSON");
		return false;
	}

	const TSharedPtr<FJsonObject>* Engine = nullptr;
	if (!Root->TryGetObjectField(TEXT("unreal-engine"), Engine) || !Engine || !Engine->IsValid())
	{
		OutError = TEXT("the publish policy has no \"unreal-engine\" section");
		return false;
	}

	FCPM_PublishPolicy Parsed;
	ReadPlatform(*Engine, TEXT("windows"), Parsed.Windows);
	ReadPlatform(*Engine, TEXT("linux"), Parsed.Linux);
	Root->TryGetBoolField(TEXT("raw-project-upload"), Parsed.bUploadRawProject);

	if (!Parsed.Validate(OutError))
	{
		return false;
	}

	// Assigned only once everything parsed, so a failed read leaves the caller's policy alone.
	*this = Parsed;
	return true;
}

bool FCPM_PublishPolicy::Validate(FString& OutError) const
{
	// Refused rather than defaulted to Shipping: guessing a build configuration means publishing a
	// Pak built differently from what was asked for, and nothing downstream can tell.
	if (Windows.bShouldPackage && Windows.Configuration.IsEmpty())
	{
		OutError = TEXT("the publish policy asks to package Windows but names no configuration");
		return false;
	}
	if (Linux.bShouldPackage && Linux.Configuration.IsEmpty())
	{
		OutError = TEXT("the publish policy asks to package Linux but names no configuration");
		return false;
	}

	if (!Windows.bShouldPackage && !Linux.bShouldPackage && !bUploadRawProject)
	{
		// A Publish that would produce nothing is a policy the reader misunderstood, not an
		// instruction. Better caught here than as an Asset with no Versions.
		OutError = TEXT("the publish policy asks for no platforms and no raw project");
		return false;
	}

	return true;
}

FCPM_PublishPolicy FCPM_PublishPolicy::Defaults()
{
	FCPM_PublishPolicy Policy;
	Policy.Windows.bShouldPackage = true;
	Policy.Windows.Configuration = TEXT("Shipping");
	Policy.Linux.bShouldPackage = true;
	Policy.Linux.Configuration = TEXT("Shipping");
	Policy.bUploadRawProject = true;
	return Policy;
}

TArray<ECPM_Platform> FCPM_PublishPolicy::PlatformsToPackage() const
{
	TArray<ECPM_Platform> Platforms;
	if (Windows.bShouldPackage)
	{
		Platforms.Add(ECPM_Platform::Windows);
	}
	if (Linux.bShouldPackage)
	{
		Platforms.Add(ECPM_Platform::Linux);
	}
	return Platforms;
}

FCPM_PublishPolicy FCPM_PublishPolicy::WithPlatforms(const TArray<ECPM_Platform>& Selection) const
{
	// What the Policy asks for today, so a platform it never named is built the way the ones it did
	// name are built. Empty only when the Policy asks for no platform at all.
	FString InheritedConfiguration;
	for (const ECPM_Platform Asked : PlatformsToPackage())
	{
		if (const FCPM_PlatformPolicy* AskedPolicy = Find(Asked))
		{
			if (!AskedPolicy->Configuration.IsEmpty())
			{
				InheritedConfiguration = AskedPolicy->Configuration;
				break;
			}
		}
	}

	FCPM_PublishPolicy Selected = *this;
	for (const ECPM_Platform Platform : { ECPM_Platform::Windows, ECPM_Platform::Linux })
	{
		FCPM_PlatformPolicy& Chosen = (Platform == ECPM_Platform::Windows) ? Selected.Windows : Selected.Linux;
		Chosen.bShouldPackage = Selection.Contains(Platform);

		// Validate refuses a platform that packages without one, and a policy parsed from JSON
		// leaves it empty for every platform it did not ask for.
		if (Chosen.bShouldPackage && Chosen.Configuration.IsEmpty())
		{
			Chosen.Configuration = InheritedConfiguration.IsEmpty() ? TEXT("Shipping") : InheritedConfiguration;
		}
	}

	return Selected;
}

const FCPM_PlatformPolicy* FCPM_PublishPolicy::Find(const ECPM_Platform Platform) const
{
	switch (Platform)
	{
	case ECPM_Platform::Windows:
		return &Windows;
	case ECPM_Platform::Linux:
		return &Linux;
	default:
		return nullptr;
	}
}

bool FCPM_ChunkStatus::IsBusy() const
{
	switch (Status)
	{
	case ECPM_AssetManagerStatus::Packaging_Begin:
	case ECPM_AssetManagerStatus::Archiving_Begin:
	case ECPM_AssetManagerStatus::Create_Begin:
	case ECPM_AssetManagerStatus::Update_Begin:
	case ECPM_AssetManagerStatus::UploadPak_Begin:
	case ECPM_AssetManagerStatus::Delete_Begin:
		return true;
	default:
		return false;
	}
}
