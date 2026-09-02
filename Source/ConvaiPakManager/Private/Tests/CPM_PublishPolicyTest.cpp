// Copyright 2025 Convai Inc. All Rights Reserved.

#include "CPM_PakManagerSettings.h"
#include "Misc/AutomationTest.h"
#include "Publish/CPM_PublishTypes.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	/** The policy Convai publishes today, verbatim. */
	const TCHAR* LivePolicyJson = TEXT(R"({
		"unreal-engine": {
			"windows": { "should-package": true, "configuration": "Shipping" },
			"linux":   { "should-package": true, "configuration": "Shipping" }
		},
		"raw-project-upload": true
	})");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMPublishPolicyReadsTheLivePolicy,
	"ConvaiPakManager.Publish.Policy.ReadsTheLivePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMPublishPolicyReadsTheLivePolicy::RunTest(const FString&)
{
	FCPM_PublishPolicy Policy;
	FString Error;

	TestTrue(TEXT("parses"), Policy.ParseFromJson(LivePolicyJson, Error));
	TestTrue(TEXT("packages Windows"), Policy.Windows.bShouldPackage);
	TestEqual(TEXT("at the configuration it names"), Policy.Windows.Configuration, FString(TEXT("Shipping")));
	TestTrue(TEXT("packages Linux"), Policy.Linux.bShouldPackage);
	TestTrue(TEXT("uploads the raw project"), Policy.bUploadRawProject);
	TestEqual(TEXT("asks for two platforms"), Policy.PlatformsToPackage().Num(), 2);

	return true;
}

/**
 * A platform the policy does not mention is simply not packaged. Absent and false mean the same
 * thing, so this must not be an error - it is how a Windows-only policy is expressed.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMPublishPolicyTreatsAnAbsentPlatformAsOff,
	"ConvaiPakManager.Publish.Policy.TreatsAnAbsentPlatformAsOff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMPublishPolicyTreatsAnAbsentPlatformAsOff::RunTest(const FString&)
{
	FCPM_PublishPolicy Policy;
	FString Error;

	const bool bParsed = Policy.ParseFromJson(
		TEXT(R"({"unreal-engine":{"windows":{"should-package":true,"configuration":"Shipping"}}})"), Error);

	TestTrue(TEXT("parses"), bParsed);
	TestTrue(TEXT("packages Windows"), Policy.Windows.bShouldPackage);
	TestFalse(TEXT("does not package Linux"), Policy.Linux.bShouldPackage);
	TestEqual(TEXT("asks for one platform"), Policy.PlatformsToPackage().Num(), 1);

	return true;
}

/**
 * Refused rather than defaulted to Shipping. Guessing a build configuration publishes a Pak built
 * differently from what Convai asked for, and nothing downstream can tell.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMPublishPolicyRefusesAPlatformWithNoConfiguration,
	"ConvaiPakManager.Publish.Policy.RefusesAPlatformWithNoConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMPublishPolicyRefusesAPlatformWithNoConfiguration::RunTest(const FString&)
{
	FCPM_PublishPolicy Policy;
	FString Error;

	TestFalse(TEXT("refuses"),
		Policy.ParseFromJson(TEXT(R"({"unreal-engine":{"windows":{"should-package":true}}})"), Error));
	TestTrue(TEXT("says which platform"), Error.Contains(TEXT("Windows")));

	return true;
}

/** A Publish that would produce nothing is a misread policy, not an instruction. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMPublishPolicyRefusesAPolicyThatProducesNothing,
	"ConvaiPakManager.Publish.Policy.RefusesAPolicyThatProducesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMPublishPolicyRefusesAPolicyThatProducesNothing::RunTest(const FString&)
{
	FCPM_PublishPolicy Policy;
	FString Error;

	TestFalse(TEXT("refuses"),
		Policy.ParseFromJson(TEXT(R"({"unreal-engine":{},"raw-project-upload":false})"), Error));

	return true;
}

/**
 * A failed read must leave the caller's policy untouched.
 *
 * A half-applied policy is the dangerous outcome: it would publish some Versions and silently omit
 * others, which surfaces later as a product failing to load an Asset rather than here.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMPublishPolicyLeavesTheOldPolicyIntactOnFailure,
	"ConvaiPakManager.Publish.Policy.LeavesTheOldPolicyIntactOnFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMPublishPolicyLeavesTheOldPolicyIntactOnFailure::RunTest(const FString&)
{
	FCPM_PublishPolicy Policy;
	FString Error;
	TestTrue(TEXT("reads the live policy first"), Policy.ParseFromJson(LivePolicyJson, Error));

	TestFalse(TEXT("refuses the malformed one"), Policy.ParseFromJson(TEXT("{ not json"), Error));

	TestTrue(TEXT("Windows survives"), Policy.Windows.bShouldPackage);
	TestTrue(TEXT("Linux survives"), Policy.Linux.bShouldPackage);
	TestTrue(TEXT("the raw project flag survives"), Policy.bUploadRawProject);

	return true;
}

/**
 * What the creator's Upload Raw Project Archive setting is allowed to do to a Policy.
 *
 * The whole truth table, four rows of it. The row that matters is the last one: a creator who has
 * turned the upload ON cannot thereby add an archive to an Asset whose Policy asked for none -
 * which Versions an Asset carries is Convai's decision, and a setting that could add to it would
 * publish a Version nothing on the server expects.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMPublishPolicyArchiveUploadOnlySubtracts,
	"ConvaiPakManager.Publish.Policy.ArchiveUploadOnlySubtracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMPublishPolicyArchiveUploadOnlySubtracts::RunTest(const FString&)
{
	UCPM_PakManagerSettings* Settings = GetMutableDefault<UCPM_PakManagerSettings>();
	const bool bRestore = Settings->bUploadRawProjectArchive;

	for (const bool bUpload : { false, true })
	{
		Settings->bUploadRawProjectArchive = bUpload;
		for (const bool bPolicy : { false, true })
		{
			TestEqual(
				*FString::Printf(TEXT("upload=%d policy=%d"), bUpload, bPolicy),
				Settings->ShouldArchiveRawProject(bPolicy), bPolicy && bUpload);
		}
	}

	Settings->bUploadRawProjectArchive = bRestore;
	return true;
}

/**
 * A policy typed into a project's settings is held to what a fetched one is held to.
 *
 * The fields skip the parser, so without this they would be the one way into a Publish that never
 * met the two checks the parser exists for.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMPublishPolicyValidatesATypedOverride,
	"ConvaiPakManager.Publish.Policy.ValidatesATypedOverride",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMPublishPolicyValidatesATypedOverride::RunTest(const FString&)
{
	FString Error;

	FCPM_PublishPolicy Nothing;
	TestFalse(TEXT("a policy that would produce nothing is refused"), Nothing.Validate(Error));

	FCPM_PublishPolicy NoConfiguration;
	NoConfiguration.Windows.bShouldPackage = true;
	TestFalse(TEXT("packaging Windows with no configuration is refused"), NoConfiguration.Validate(Error));
	TestTrue(TEXT("and says which platform"), Error.Contains(TEXT("Windows")));

	FCPM_PublishPolicy RawOnly;
	RawOnly.bUploadRawProject = true;
	TestTrue(TEXT("the raw project alone is a policy"), RawOnly.Validate(Error));

	FCPM_PublishPolicy Windows;
	Windows.Windows.bShouldPackage = true;
	Windows.Windows.Configuration = TEXT("Shipping");
	TestTrue(TEXT("one platform with a configuration is a policy"), Windows.Validate(Error));
	TestEqual(TEXT("and asks for that one platform"), Windows.PlatformsToPackage().Num(), 1);

	return true;
}

#endif  // WITH_AUTOMATION_TESTS
