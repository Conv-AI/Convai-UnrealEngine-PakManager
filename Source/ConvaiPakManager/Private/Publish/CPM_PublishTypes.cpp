// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Publish/CPM_PublishTypes.h"

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

		(*Object)->TryGetBoolField(TEXT("should-package"), OutPolicy.bShouldPackage);
		(*Object)->TryGetStringField(TEXT("configuration"), OutPolicy.Configuration);
	}
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
