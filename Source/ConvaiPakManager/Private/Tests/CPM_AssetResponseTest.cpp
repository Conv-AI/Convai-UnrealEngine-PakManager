// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Utility/CPM_UtilityLibrary.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	/**
	 * An assets/get answer, trimmed from one captured off the live backend.
	 *
	 * The shape is the point: the record's fields sit directly on `assets[N]`, where a create
	 * response nests them under `assets[N].asset`. Reading one with the other's parser silently
	 * yields nothing.
	 */
	const TCHAR* GetResponse = TEXT(R"({
		"transactionID": "5f7bbc68-9ad0-42da-b513-6e1b52371023",
		"assets": [{
			"asset_id": "35934fb7-7074-4356-bb9b-7d6e55d68684",
			"entity_type": "avatar",
			"tags": ["Pak", "Avatar", "Raw"],
			"versions": ["ue-5.5-Raw", "ue-5.8-Windows"],
			"metadata": {
				"asset_name": "Avatar",
				"asset_type": "avatar",
				"root_path": "/BK6BQLDWRGECQYJEBJ6N/",
				"Raw_PakSize": 3776337790,
				"Windows_PakSize": 568396083,
				"entity_data": { "gender": "male", "avatar_name": "BP_Irene" }
			},
			"version_urls": { "ue-5.8-Windows": "https://storage.googleapis.com/signed?X-Goog-Signature=secret" }
		}]
	})");
}

/**
 * The document a refresh writes into a Chunk's cache, and nothing else from the answer.
 *
 * The rest of an assets/get response is signed URLs - credentials - and per-Version detail nothing
 * here records. Taking only the document is what keeps them out of the creator's project.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMAssetResponseReadsTheDocumentAndNothingElse,
	"ConvaiPakManager.Publish.Response.ReadsTheDocumentAndNothingElse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMAssetResponseReadsTheDocumentAndNothingElse::RunTest(const FString&)
{
	FString Document;
	TestTrue(TEXT("a real assets/get answer parses"),
		UCPM_UtilityLibrary::GetAssetMetadataFromJSON(GetResponse, Document));

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Document);
	TestTrue(TEXT("and what it yields is a document the composer can read"),
		FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());

	if (!Root.IsValid())
	{
		return false;
	}

	FString AssetName;
	Root->TryGetStringField(TEXT("asset_name"), AssetName);
	TestEqual(TEXT("carrying the server's own fields"), AssetName, FString(TEXT("Avatar")));

	// Larger than int32 holds - a raw project archive is routinely gigabytes.
	TestEqual(TEXT("and a size that does not fit an int32"),
		static_cast<int64>(Root->GetNumberField(TEXT("Raw_PakSize"))), static_cast<int64>(3776337790));

	TestFalse(TEXT("no signed URL follows the document into the project"),
		Document.Contains(TEXT("X-Goog-Signature")));
	TestFalse(TEXT("nor the envelope around it"), Document.Contains(TEXT("transactionID")));

	// The create parser reads assets[N].asset; this envelope has no such key. Pinned because the
	// two are one field name apart and the failure is silent.
	FCPM_CreatedAssets AsCreate;
	UCPM_UtilityLibrary::GetCreatedAssetsFromJSON(GetResponse, AsCreate);
	TestTrue(TEXT("the create parser finds no document in a get answer"),
		AsCreate.Assets.IsEmpty() || AsCreate.Assets[0].Asset.MetadataString.IsEmpty());

	FString Nothing;
	TestFalse(TEXT("an answer with no assets is refused"),
		UCPM_UtilityLibrary::GetAssetMetadataFromJSON(TEXT(R"({"assets":[]})"), Nothing));
	TestFalse(TEXT("so is one that is not JSON"),
		UCPM_UtilityLibrary::GetAssetMetadataFromJSON(TEXT("{ not json"), Nothing));

	return true;
}

#endif
