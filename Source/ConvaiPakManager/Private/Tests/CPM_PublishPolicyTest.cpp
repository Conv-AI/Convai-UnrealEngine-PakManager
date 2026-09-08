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

	// Emptied deliberately: the field defaults to Shipping for the settings UI, so the refusal this
	// guards is only reachable by a policy that cleared it - which is what a JSON one without a
	// "configuration" key arrives as.
	FCPM_PublishPolicy NoConfiguration;
	NoConfiguration.Windows.bShouldPackage = true;
	NoConfiguration.Windows.Configuration.Reset();
	TestFalse(TEXT("packaging Windows with no configuration is refused"), NoConfiguration.Validate(Error));
	TestTrue(TEXT("and says which platform"), Error.Contains(TEXT("Windows")));

	FCPM_PublishPolicy Typed;
	Typed.Windows.bShouldPackage = true;
	TestTrue(TEXT("a platform ticked in the settings arrives with a configuration"), Typed.Validate(Error));
	TestEqual(TEXT("and it is the one Convai builds at"), Typed.Windows.Configuration, FString(TEXT("Shipping")));

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

/**
 * A package-only run always cooks.
 *
 * Regression: "Package now" finished instantly having built nothing, because the packaging Job ORed
 * the bUseExistingPakFile debug setting into its reuse decision and that setting is on in this
 * project. Producing a fresh Pak is the ONLY thing a package-only run does, so no route - the debug
 * setting or the per-run menu item - may turn it into a no-op.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMPublishPolicyPackageOnlyNeverReusesPaks,
	"ConvaiPakManager.Publish.Policy.PackageOnlyNeverReusesPaks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMPublishPolicyPackageOnlyNeverReusesPaks::RunTest(const FString&)
{
	using FOptions = FCPM_PublishOptions;

	// The bug, from both routes.
	TestFalse(TEXT("package-only ignores the debug setting"),
		FOptions::ShouldReuseExistingPaks(/*bPackageOnly=*/true, /*bRequestedThisRun=*/false, /*bDebugSettingOn=*/true));
	TestFalse(TEXT("package-only ignores an explicit request"),
		FOptions::ShouldReuseExistingPaks(/*bPackageOnly=*/true, /*bRequestedThisRun=*/true, /*bDebugSettingOn=*/false));

	// A publish still honours both, which is what the setting and the menu item are for.
	TestTrue(TEXT("a publish honours the debug setting"),
		FOptions::ShouldReuseExistingPaks(/*bPackageOnly=*/false, /*bRequestedThisRun=*/false, /*bDebugSettingOn=*/true));
	TestTrue(TEXT("a publish honours an explicit request"),
		FOptions::ShouldReuseExistingPaks(/*bPackageOnly=*/false, /*bRequestedThisRun=*/true, /*bDebugSettingOn=*/false));
	TestFalse(TEXT("and cooks when neither asks"),
		FOptions::ShouldReuseExistingPaks(/*bPackageOnly=*/false, /*bRequestedThisRun=*/false, /*bDebugSettingOn=*/false));

	return true;
}

/**
 * What a Platform Selection does to a Policy - the one thing allowed to ADD, not only subtract.
 *
 * The row that matters is the forced one: a Windows-only policy asked to publish Linux must produce
 * a Linux that actually packages AND carries a build configuration, because ParseFromJson leaves
 * the configuration empty for a platform the policy never named and Validate refuses a platform
 * that packages without one. Publishing that would fail deep inside the packaging Job with
 * "no policy for platform Linux", long after the creator chose.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMPublishPolicySelectionAddsAndRemovesPlatforms,
	"ConvaiPakManager.Publish.Policy.SelectionAddsAndRemovesPlatforms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMPublishPolicySelectionAddsAndRemovesPlatforms::RunTest(const FString&)
{
	// The shape Convai publishes today: Linux named and switched OFF. ReadPlatform clears the
	// configuration of a platform the JSON names, so this - not an absent platform, which keeps the
	// struct's default - is the case that would produce an unpackageable forced Linux.
	FCPM_PublishPolicy WindowsOnly;
	FString Error;
	TestTrue(TEXT("reads a policy that switches Linux off"), WindowsOnly.ParseFromJson(
		TEXT(R"({"unreal-engine":{
			"windows":{"should-package":true,"configuration":"Shipping"},
			"linux":{"should-package":false}
		}})"), Error));
	TestFalse(TEXT("which does not package Linux"), WindowsOnly.Linux.bShouldPackage);
	TestTrue(TEXT("and names no Linux configuration"), WindowsOnly.Linux.Configuration.IsEmpty());

	// Adding: the enterprise project Convai agreed to host a Linux build for.
	const FCPM_PublishPolicy Forced =
		WindowsOnly.WithPlatforms({ ECPM_Platform::Windows, ECPM_Platform::Linux });
	TestTrue(TEXT("forcing Linux packages Linux"), Forced.Linux.bShouldPackage);
	TestEqual(TEXT("at the configuration the policy uses for what it does ask for"),
		Forced.Linux.Configuration, FString(TEXT("Shipping")));
	TestTrue(TEXT("and the forced policy is one a publish can run from"), Forced.Validate(Error));
	TestEqual(TEXT("asking for both platforms"), Forced.PlatformsToPackage().Num(), 2);

	// Removing: once Linux is general, one creator sending Windows alone.
	const FCPM_PublishPolicy Narrowed =
		FCPM_PublishPolicy::Defaults().WithPlatforms({ ECPM_Platform::Windows });
	TestTrue(TEXT("keeps Windows"), Narrowed.Windows.bShouldPackage);
	TestFalse(TEXT("drops Linux"), Narrowed.Linux.bShouldPackage);

	// The Raw Project Archive is NOT the Selection's to touch - see CONTEXT.md.
	TestTrue(TEXT("the archive flag survives adding"), Forced.bUploadRawProject == WindowsOnly.bUploadRawProject);
	TestTrue(TEXT("and survives removing"), Narrowed.bUploadRawProject);

	// Selecting nothing is expressible, and is a policy a publish must refuse to package from.
	const FCPM_PublishPolicy Nothing = WindowsOnly.WithPlatforms({});
	TestEqual(TEXT("selecting nothing asks for no platform"), Nothing.PlatformsToPackage().Num(), 0);

	// A platform the JSON never mentions keeps the struct's default configuration, so forcing it
	// works by a different route than the case above. Both must end up packageable.
	FCPM_PublishPolicy NoLinuxSection;
	TestTrue(TEXT("reads a policy with no Linux section at all"), NoLinuxSection.ParseFromJson(
		TEXT(R"({"unreal-engine":{"windows":{"should-package":true,"configuration":"Test"}}})"), Error));
	const FCPM_PublishPolicy ForcedFromAbsent =
		NoLinuxSection.WithPlatforms({ ECPM_Platform::Windows, ECPM_Platform::Linux });
	TestTrue(TEXT("forcing an unmentioned platform still validates"), ForcedFromAbsent.Validate(Error));
	TestFalse(TEXT("and it carries some configuration"), ForcedFromAbsent.Linux.Configuration.IsEmpty());

	return true;
}

/**
 * The `stage-limits` section: read from the same document as the policy, ignored where absent.
 *
 * On its own literal rather than LivePolicyJson, which gains the section later - two tests sharing
 * that fixture would mean the "no limits" case quietly stops testing anything the day it does.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMPublishPolicyReadsStageLimits,
	"ConvaiPakManager.Publish.Policy.ReadsStageLimits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMPublishPolicyReadsStageLimits::RunTest(const FString&)
{
	const TCHAR* PolicyWithLimits = TEXT(R"({
		"unreal-engine": {
			"windows": { "should-package": true, "configuration": "Shipping" },
			"linux":   { "should-package": true, "configuration": "Shipping" }
		},
		"raw-project-upload": true,
		"stage-limits": {
			"dynamic-lights":    { "max": 4,      "severity": "error" },
			"shadow-lights":     { "max": 0,      "severity": "warning" },
			"static-meshes":     { "max": 20,     "severity": "error" },
			"triangles":         { "max": 500000, "severity": "error" },
			"textures":          { "max": 250,    "severity": "error" },
			"texture-max-size":  { "max": 2048,   "severity": "error" },
			"texture-memory-mb": { "max": 512,    "severity": "error" },
			"convai-objects":    { "max": 10,     "severity": "warning" },
			"actors":            { "max": 100,    "severity": "warning" },
			"fog-volumes":       { "max": 1,      "severity": "error" }
		}
	})");

	FCPM_StageLimits Limits;
	FString Error;
	TestTrue(TEXT("reads the section"), Limits.ParseFromJson(PolicyWithLimits, Error));
	TestEqual(TEXT("a limit the tool does not know is ignored"), Limits.Rules.Num(), 9);

	const FCPM_StageLimitRule* DynamicLights = Limits.Rules.Find(ECPM_StageLimit::DynamicLights);
	if (TestNotNull(TEXT("dynamic-lights is one of them"), DynamicLights))
	{
		TestEqual(TEXT("at the max the policy names"), DynamicLights->Max, 4.0);
		TestTrue(TEXT("and exceeding it refuses the publish"), DynamicLights->Severity == ECPM_StageSeverity::Error);
	}

	// A max of none is a limit like any other, not an absent one.
	const FCPM_StageLimitRule* ShadowLights = Limits.Rules.Find(ECPM_StageLimit::ShadowLights);
	if (TestNotNull(TEXT("shadow-lights is one of them"), ShadowLights))
	{
		TestEqual(TEXT("at a max of none"), ShadowLights->Max, 0.0);
		TestTrue(TEXT("which only warns"), ShadowLights->Severity == ECPM_StageSeverity::Warning);
	}

	const FCPM_StageLimitRule* ConvaiObjects = Limits.Rules.Find(ECPM_StageLimit::ConvaiObjects);
	if (TestNotNull(TEXT("convai-objects is one of them"), ConvaiObjects))
	{
		TestTrue(TEXT("and it only warns"), ConvaiObjects->Severity == ECPM_StageSeverity::Warning);
	}

	FCPM_PublishPolicy Policy;
	TestTrue(TEXT("the section is invisible to the policy read beside it"),
		Policy.ParseFromJson(PolicyWithLimits, Error));
	TestEqual(TEXT("which still asks for two platforms"), Policy.PlatformsToPackage().Num(), 2);

	FCPM_StageLimits NoSection;
	TestTrue(TEXT("a policy from before the section parses"), NoSection.ParseFromJson(
		TEXT(R"({"unreal-engine":{"windows":{"should-package":true,"configuration":"Shipping"}},"raw-project-upload":true})"),
		Error));
	TestTrue(TEXT("and yields no limits"), NoSection.Rules.IsEmpty());

	TestFalse(TEXT("a limit with no max is refused"),
		Limits.ParseFromJson(TEXT(R"({"stage-limits":{"textures":{"severity":"error"}}})"), Error));
	TestTrue(TEXT("saying which limit it could not read"), Error.Contains(TEXT("textures")));
	TestEqual(TEXT("and the limits read before it are still there"), Limits.Rules.Num(), 9);

	TestFalse(TEXT("a severity nobody defined is refused"),
		Limits.ParseFromJson(TEXT(R"({"stage-limits":{"actors":{"max":5,"severity":"blocking"}}})"), Error));
	TestTrue(TEXT("saying which limit that was"), Error.Contains(TEXT("actors")));

	TestFalse(TEXT("a section that is not an object is refused"),
		Limits.ParseFromJson(TEXT(R"({"stage-limits": 7})"), Error));
	TestFalse(TEXT("and so is a document that is not JSON"),
		Limits.ParseFromJson(TEXT("{ not json"), Error));

	TestEqual(TEXT("a failed read leaves the old limits alone"), Limits.Rules.Num(), 9);

	return true;
}

#endif  // WITH_AUTOMATION_TESTS
