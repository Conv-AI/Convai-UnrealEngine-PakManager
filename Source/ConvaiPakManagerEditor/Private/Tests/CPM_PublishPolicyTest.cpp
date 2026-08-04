// Copyright 2025 Convai Inc. All Rights Reserved.

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

#endif  // WITH_AUTOMATION_TESTS
