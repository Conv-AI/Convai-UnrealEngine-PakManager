// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Chunk/CPM_Chunk.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	TSharedRef<FJsonObject> Parse(const TCHAR* Json)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Root);
		return Root.IsValid() ? Root.ToSharedRef() : MakeShared<FJsonObject>();
	}

	FString Field(const TSharedRef<FJsonObject>& Root, const TCHAR* Name)
	{
		FString Value;
		Root->TryGetStringField(Name, Value);
		return Value;
	}

	FString EntityField(const TSharedRef<FJsonObject>& Root, const TCHAR* Name)
	{
		const TSharedPtr<FJsonObject>* Entity = nullptr;
		return Root->TryGetObjectField(TEXT("entity_data"), Entity) && Entity && Entity->IsValid()
			? Field((*Entity).ToSharedRef(), Name)
			: FString();
	}

	bool HasEntityObject(const TSharedRef<FJsonObject>& Root, const TCHAR* Name)
	{
		const TSharedPtr<FJsonObject>* Entity = nullptr;
		if (!Root->TryGetObjectField(TEXT("entity_data"), Entity) || !Entity || !Entity->IsValid())
		{
			return false;
		}
		const TSharedPtr<FJsonObject>* Nested = nullptr;
		return (*Entity)->TryGetObjectField(Name, Nested);
	}
}

/** Every key the create-asset endpoint requires of a Scene, with the values it requires. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMAssetMetadataFillsASceneDocument,
	"ConvaiPakManager.Publish.Metadata.FillsASceneDocument",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMAssetMetadataFillsASceneDocument::RunTest(const FString&)
{
	// What SetEntryPoint leaves behind for a Scene, before anything fills in the rest.
	const TSharedRef<FJsonObject> Root = Parse(TEXT(R"({
		"root_path": "/AGXRDJZ56VYMFHVMTVPE/",
		"level_name": "/AGXRDJZ56VYMFHVMTVPE/Maps/Landing",
		"blueprint_class": "None",
		"blueprint_class_path": "",
		"asset_name": "Dev_CPM_53",
		"asset_description": ""
	})"));
	ConvaiPakManager::Chunk::FillRequiredMetadataFields(
		Root, TEXT("Dev_CPM_53"), TEXT("AGXRDJZ56VYMFHVMTVPE"), TEXT("Scene"));

	TestEqual(TEXT("project_name"), Field(Root, TEXT("project_name")), FString(TEXT("Dev_CPM_53")));
	TestEqual(TEXT("plugin_name"), Field(Root, TEXT("plugin_name")), FString(TEXT("AGXRDJZ56VYMFHVMTVPE")));
	TestEqual(TEXT("asset_type is lowercased"), Field(Root, TEXT("asset_type")), FString(TEXT("scene")));
	TestEqual(TEXT("content_path points into the plugin"), Field(Root, TEXT("content_path")),
		FString(TEXT("../../../Dev_CPM_53/Plugins/AGXRDJZ56VYMFHVMTVPE/Content/")));
	TestEqual(TEXT("root_path"), Field(Root, TEXT("root_path")), FString(TEXT("/AGXRDJZ56VYMFHVMTVPE/")));
	TestEqual(TEXT("level_name keeps its full package path"), Field(Root, TEXT("level_name")),
		FString(TEXT("/AGXRDJZ56VYMFHVMTVPE/Maps/Landing")));
	TestEqual(TEXT("blueprint_class"), Field(Root, TEXT("blueprint_class")), FString(TEXT("None")));
	TestEqual(TEXT("blueprint_class_path"), Field(Root, TEXT("blueprint_class_path")), FString());
	TestEqual(TEXT("asset_name"), Field(Root, TEXT("asset_name")), FString(TEXT("Dev_CPM_53")));

	TestEqual(TEXT("scene_name is the level's leaf"), EntityField(Root, TEXT("scene_name")), FString(TEXT("Landing")));
	TestEqual(TEXT("scene_description"), EntityField(Root, TEXT("scene_description")), FString(TEXT("Pak scene")));
	TestTrue(TEXT("scene_metadata is an object"), HasEntityObject(Root, TEXT("scene_metadata")));

	return true;
}

/** The Avatar half of the same schema, keyed off the blueprint rather than the level. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMAssetMetadataFillsAnAvatarDocument,
	"ConvaiPakManager.Publish.Metadata.FillsAnAvatarDocument",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMAssetMetadataFillsAnAvatarDocument::RunTest(const FString&)
{
	const TSharedRef<FJsonObject> Root = Parse(TEXT(R"({
		"root_path": "/AGXRDJZ56VYMFHVMTVPE/",
		"level_name": "",
		"blueprint_class": "/Script/Engine.BlueprintGeneratedClass'/AGXRDJZ56VYMFHVMTVPE/BP/Nova.Nova_C'",
		"blueprint_class_path": "/AGXRDJZ56VYMFHVMTVPE/BP/Nova"
	})"));
	ConvaiPakManager::Chunk::FillRequiredMetadataFields(
		Root, TEXT("Dev_CPM_53"), TEXT("AGXRDJZ56VYMFHVMTVPE"), TEXT("Avatar"));

	TestEqual(TEXT("asset_type is lowercased"), Field(Root, TEXT("asset_type")), FString(TEXT("avatar")));
	TestEqual(TEXT("asset_name falls back to the project"), Field(Root, TEXT("asset_name")), FString(TEXT("Dev_CPM_53")));
	TestEqual(TEXT("the blueprint class survives"), Field(Root, TEXT("blueprint_class")),
		FString(TEXT("/Script/Engine.BlueprintGeneratedClass'/AGXRDJZ56VYMFHVMTVPE/BP/Nova.Nova_C'")));

	TestEqual(TEXT("avatar_name is the blueprint's leaf"), EntityField(Root, TEXT("avatar_name")), FString(TEXT("Nova")));
	TestEqual(TEXT("gender defaults"), EntityField(Root, TEXT("gender")), FString(TEXT("male")));
	TestTrue(TEXT("avatar_config is an object"), HasEntityObject(Root, TEXT("avatar_config")));

	return true;
}

/**
 * The document a live project has on disk today: pointed at the project's own Content, carrying a
 * level by short name, and with no entity_data at all. This is the create-asset failure.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMAssetMetadataRepairsAnOlderDocument,
	"ConvaiPakManager.Publish.Metadata.RepairsAnOlderDocument",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMAssetMetadataRepairsAnOlderDocument::RunTest(const FString&)
{
	const TSharedRef<FJsonObject> Root = Parse(TEXT(R"({
		"root_path": "/JBILN5CDNI4TRYELD6CS/",
		"level_name": "Map",
		"blueprint_class": "",
		"blueprint_class_path": "",
		"content_path": "../../../Dev_CPM_58/Content/",
		"project_name": "Dev_CPM_58",
		"plugin_name": "JBILN5CDNI4TRYELD6CS",
		"asset_type": "scene",
		"asset_name": "Test",
		"asset_description": "Test new slate UI"
	})"));
	ConvaiPakManager::Chunk::FillRequiredMetadataFields(
		Root, TEXT("Dev_CPM_58"), TEXT("JBILN5CDNI4TRYELD6CS"), TEXT("Scene"));

	TestEqual(TEXT("the wrong content_path is replaced, not kept"), Field(Root, TEXT("content_path")),
		FString(TEXT("../../../Dev_CPM_58/Plugins/JBILN5CDNI4TRYELD6CS/Content/")));
	TestEqual(TEXT("an empty blueprint_class becomes None"), Field(Root, TEXT("blueprint_class")), FString(TEXT("None")));
	TestEqual(TEXT("what the creator typed is kept"), Field(Root, TEXT("asset_description")),
		FString(TEXT("Test new slate UI")));
	TestEqual(TEXT("a short level name still names the scene"), EntityField(Root, TEXT("scene_name")),
		FString(TEXT("Map")));
	TestTrue(TEXT("scene_metadata is an object"), HasEntityObject(Root, TEXT("scene_metadata")));

	return true;
}

/** Nothing the server put in entity_data may be lost to a second fill. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMAssetMetadataKeepsWhatItDoesNotOwn,
	"ConvaiPakManager.Publish.Metadata.KeepsWhatItDoesNotOwn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMAssetMetadataKeepsWhatItDoesNotOwn::RunTest(const FString&)
{
	const TSharedRef<FJsonObject> Root = Parse(TEXT(R"({
		"blueprint_class_path": "/PLUGIN/BP/Nova",
		"entity_data": { "gender": "female", "avatar_config": { "voice": "wren" }, "server_only": "keep me" }
	})"));
	ConvaiPakManager::Chunk::FillRequiredMetadataFields(Root, TEXT("Proj"), TEXT("PLUGIN"), TEXT("avatar"));

	TestEqual(TEXT("a gender the server chose is kept"), EntityField(Root, TEXT("gender")), FString(TEXT("female")));
	TestEqual(TEXT("a field nothing here models is kept"), EntityField(Root, TEXT("server_only")), FString(TEXT("keep me")));

	const TSharedPtr<FJsonObject>* Entity = nullptr;
	Root->TryGetObjectField(TEXT("entity_data"), Entity);
	const TSharedPtr<FJsonObject>* Config = nullptr;
	TestTrue(TEXT("avatar_config is not flattened back to empty"),
		Entity && (*Entity)->TryGetObjectField(TEXT("avatar_config"), Config) && Config
			&& (*Config)->HasField(TEXT("voice")));

	return true;
}

/**
 * Compose refuses on a document it cannot read, and a refusal leaves the cache exactly as the server
 * last echoed it. Both halves matter together: the publish job fails on the false, and there is no
 * half-written document for a later run to send in place of what the creator typed.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMAssetMetadataRefusesADocumentItCannotRead,
	"ConvaiPakManager.Publish.Metadata.RefusesADocumentItCannotRead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMAssetMetadataRefusesADocumentItCannotRead::RunTest(const FString&)
{
	const FString Directory = FPaths::Combine(
		FPaths::ProjectIntermediateDir(), TEXT("CPM_Tests"), TEXT("RefusesADocumentItCannotRead"));
	IFileManager::Get().DeleteDirectory(*Directory, false, true);
	IFileManager::Get().MakeDirectory(*Directory, true);

	const FString MetadataPath = FPaths::Combine(Directory, TEXT("PakMetaData_3.json"));
	const FString DraftPath = FPaths::Combine(Directory, TEXT("Draft_3.json"));
	const FString Echo = TEXT("{\"asset_name\":\"what the server last heard\"}");

	auto Compose = [&MetadataPath, &DraftPath]
	{
		return ConvaiPakManager::Chunk::ComposePakMetadataAt(
			MetadataPath, DraftPath, TEXT("Proj"), TEXT("PLUGIN"), TEXT("avatar"));
	};
	auto ReadMetadata = [&MetadataPath]
	{
		FString Contents;
		FFileHelper::LoadFileToString(Contents, *MetadataPath);
		return Contents;
	};

	AddExpectedError(TEXT("is not valid JSON"), EAutomationExpectedErrorFlags::Contains, 2);

	FFileHelper::SaveStringToFile(Echo, *MetadataPath);
	FFileHelper::SaveStringToFile(TEXT("{ not json"), *DraftPath);
	TestFalse(TEXT("a Draft that will not parse refuses"), Compose());
	TestEqual(TEXT("and the server's echo is left byte for byte"), ReadMetadata(), Echo);

	FFileHelper::SaveStringToFile(TEXT("} not json either"), *MetadataPath);
	FFileHelper::SaveStringToFile(TEXT("{\"asset_name\":\"Nova\"}"), *DraftPath);
	TestFalse(TEXT("so does a cache that will not parse"), Compose());
	TestEqual(TEXT("and it is left alone too"), ReadMetadata(), FString(TEXT("} not json either")));

	// With both readable it composes - so the two refusals above are refusals, not a function that
	// cannot succeed.
	FFileHelper::SaveStringToFile(Echo, *MetadataPath);
	TestTrue(TEXT("two readable documents compose"), Compose());
	TestEqual(TEXT("and what the creator typed wins the name"),
		Field(Parse(*ReadMetadata()), TEXT("asset_name")), FString(TEXT("Nova")));

	IFileManager::Get().DeleteDirectory(*Directory, false, true);
	return true;
}

#endif
