// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Stage/CPM_Stage.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_AUTOMATION_TESTS

using namespace ConvaiPakManager::Stage;

namespace
{
	FCPM_StageActor Actor(const TCHAR* Label)
	{
		FCPM_StageActor Result;
		Result.Label = Label;
		Result.Path = FSoftObjectPath(FString::Printf(TEXT("/Game/Office/L_Office.L_Office:PersistentLevel.%s"), Label));
		Result.Guid = FGuid::NewGuid();
		return Result;
	}

	/**
	 * The walkthrough's example level: one avatar, one camera, a nav mesh, and a 4096 px desk texture
	 * the live policy refuses. Actors is 7 and TexturesCount 22 (4 stage + 18 avatar), because the
	 * avatar is left out of every fact except the texture count.
	 */
	FCPM_StageFacts OfficeFacts()
	{
		FCPM_StageFacts Facts;
		Facts.SourceLevel = TEXT("/Game/Office/L_Office");
		Facts.AvatarName = TEXT("BP_Receptionist");
		Facts.AvatarActorNames = { TEXT("BP_Receptionist_C_1") };
		Facts.AvatarLocation = FVector(120.0, -40.0, 0.0);
		Facts.AvatarRotation = FRotator(0.0, 90.0, 0.0);
		Facts.AvatarScale = FVector(1.0, 1.0, 1.0);
		Facts.Actors = 7;
		Facts.DynamicLights = { Actor(TEXT("SpotLight_01")), Actor(TEXT("SpotLight_02")) };
		// The same actor in both lists, so a click on the shadow warning finds the light it names.
		Facts.ShadowCastingLights = { Facts.DynamicLights[1] };
		Facts.StaticMeshComponents = 3;
		Facts.Triangles = 23400;
		Facts.TexturesCount = 22;
		Facts.TexturesMaxBuiltPx = 4096;
		Facts.LargestTexture = FSoftObjectPath(TEXT("/Game/Office/Textures/T_Desk_D.T_Desk_D"));
		Facts.LargestTextureContext = TEXT("M_Desk on SM_Desk");
		Facts.TexturesMemoryMb = 96.0;
		Facts.Cameras = { Actor(TEXT("CAM_Front")) };
		Facts.CameraActorName = TEXT("CAM_Front");
		Facts.bNavMeshBounds = true;
		Facts.ConvaiObjects = 1;
		return Facts;
	}

	/** The nine limits as the walkthrough writes them, in the shape the publish policy carries. */
	const TCHAR* LiveLimitsJson = TEXT(R"({
		"stage-limits": {
			"dynamic-lights":    { "max": 4,      "severity": "error" },
			"shadow-lights":     { "max": 0,      "severity": "warning" },
			"static-meshes":     { "max": 20,     "severity": "error" },
			"triangles":         { "max": 500000, "severity": "error" },
			"textures":          { "max": 250,    "severity": "error" },
			"texture-max-size":  { "max": 2048,   "severity": "error" },
			"texture-memory-mb": { "max": 512,    "severity": "error" },
			"convai-objects":    { "max": 10,     "severity": "warning" },
			"actors":            { "max": 100,    "severity": "warning" }
		}
	})");

	FCPM_StageLimits Limits(const TCHAR* Json)
	{
		FCPM_StageLimits Parsed;
		FString Error;
		const bool bParsed = Parsed.ParseFromJson(Json, Error);
		ensureMsgf(bParsed, TEXT("this test's own limits JSON must parse: %s"), *Error);
		return Parsed;
	}

	int32 CountOf(const TArray<FCPM_StageIssue>& Issues, const ECPM_StageSeverity Severity)
	{
		int32 Count = 0;
		for (const FCPM_StageIssue& Issue : Issues)
		{
			Count += Issue.Severity == Severity ? 1 : 0;
		}
		return Count;
	}

	const FCPM_StageIssue* Find(const TArray<FCPM_StageIssue>& Issues, const TCHAR* Contains)
	{
		return Issues.FindByPredicate([Contains](const FCPM_StageIssue& Issue) { return Issue.Reason.Contains(Contains); });
	}

	/** D13: no line a creator reads may say their level lost anything. */
	bool AnyMentionsRemoval(const TArray<FCPM_StageIssue>& Issues)
	{
		return Issues.ContainsByPredicate([](const FCPM_StageIssue& Issue)
		{
			return Issue.Reason.Contains(TEXT("removed")) || Issue.Reason.Contains(TEXT("stripped"));
		});
	}

	TSharedPtr<FJsonObject> Nested(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Name)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		return Parent.IsValid() && Parent->TryGetObjectField(Name, Object) && Object ? *Object : nullptr;
	}

	/** A number the record does not carry reads as one nothing equals, so a missing key fails loudly. */
	double NumberIn(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
	{
		double Value = 0.0;
		return Object.IsValid() && Object->TryGetNumberField(Name, Value) ? Value : TNumericLimits<double>::Max();
	}

	FString RecordKeys(const TSharedPtr<FJsonObject>& Root)
	{
		TArray<FString> Keys;
		if (Root.IsValid())
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Root->Values)
			{
				Keys.Add(Field.Key);
			}
		}
		Keys.Sort();
		return FString::Join(Keys, TEXT(","));
	}
}

/**
 * The shape of an issue: the thing to click, the number measured, the number it broke, and a line
 * carrying both. Raising nothing and fixing the texture is what clears it.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMStageEvaluateNamesTheTextureOverTheLimit,
	"ConvaiPakManager.Stage.EvaluateNamesTheTextureOverTheLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMStageEvaluateNamesTheTextureOverTheLimit::RunTest(const FString&)
{
	FCPM_StageFacts Facts = OfficeFacts();
	const FCPM_StageLimits Rules = Limits(LiveLimitsJson);
	const TArray<FCPM_StageIssue> Issues = Evaluate(Facts, Rules);

	TestEqual(TEXT("the office level breaks one limit at error severity"),
		CountOf(Issues, ECPM_StageSeverity::Error), 1);

	const FCPM_StageIssue* Texture = Find(Issues, TEXT("T_Desk_D"));
	if (!TestNotNull(TEXT("the error is the one naming the texture"), Texture))
	{
		return false;
	}
	TestTrue(TEXT("it is the error"), Texture->Severity == ECPM_StageSeverity::Error);
	TestEqual(TEXT("a click on it opens the texture"), Texture->Target.ToString(), Facts.LargestTexture.ToString());
	TestEqual(TEXT("the measurement is the built edge in pixels"), Texture->Measured, 4096.0);
	TestEqual(TEXT("the limit is the one the policy names"), Texture->Limit, 2048.0);
	TestTrue(TEXT("the line carries both numbers and where the texture is used"),
		Texture->Reason.Contains(TEXT("4096")) && Texture->Reason.Contains(TEXT("2048"))
			&& Texture->Reason.Contains(TEXT("M_Desk on SM_Desk")));

	TestEqual(TEXT("the shadow light is the only warning"), CountOf(Issues, ECPM_StageSeverity::Warning), 1);
	const FCPM_StageIssue* Shadow = Find(Issues, TEXT("Shadow-casting"));
	if (!TestNotNull(TEXT("the shadow light is reported"), Shadow))
	{
		return false;
	}
	TestEqual(TEXT("the warning reads as the walkthrough writes it"), Shadow->Reason,
		FString(TEXT("Shadow-casting light: SpotLight_02 - 1, max 0")));
	TestTrue(TEXT("a click on it selects the light"), Shadow->ActorGuid == Facts.ShadowCastingLights[0].Guid);

	const FCPM_StageIssue* Spawn = Find(Issues, TEXT("found at"));
	if (!TestNotNull(TEXT("the scan says where the avatar was found"), Spawn))
	{
		return false;
	}
	TestEqual(TEXT("it says the studio will spawn the avatar there"), Spawn->Reason,
		FString(TEXT("BP_Receptionist found at (120, -40, 0) - Avatar Studio will spawn it here.")));
	TestFalse(TEXT("no line tells the creator their level lost something"), AnyMentionsRemoval(Issues));

	Facts.TexturesMaxBuiltPx = 2048;
	TestEqual(TEXT("fixing the texture and rescanning clears the error"),
		CountOf(Evaluate(Facts, Rules), ECPM_StageSeverity::Error), 0);

	return true;
}

/**
 * Which issues refuse an upload and which only say something. The policy owns the severity of a
 * limit; the rules it cannot express carry theirs here.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMStageEvaluateWarnsNotRefuses,
	"ConvaiPakManager.Stage.EvaluateWarnsNotRefuses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMStageEvaluateWarnsNotRefuses::RunTest(const FString&)
{
	const TArray<FCPM_StageIssue> AsWarning = Evaluate(OfficeFacts(), Limits(LiveLimitsJson));
	const FCPM_StageIssue* Lenient = Find(AsWarning, TEXT("Shadow-casting"));
	if (!TestNotNull(TEXT("the shadow light is reported"), Lenient))
	{
		return false;
	}
	TestTrue(TEXT("a limit configured warning never errors"), Lenient->Severity == ECPM_StageSeverity::Warning);
	TestEqual(TEXT("so the only error is the texture's"), CountOf(AsWarning, ECPM_StageSeverity::Error), 1);

	const TArray<FCPM_StageIssue> AsError = Evaluate(OfficeFacts(),
		Limits(TEXT(R"({"stage-limits":{"shadow-lights":{"max":0,"severity":"error"}}})")));
	const FCPM_StageIssue* Strict = Find(AsError, TEXT("Shadow-casting"));
	if (!TestNotNull(TEXT("the same light is reported under the stricter policy"), Strict))
	{
		return false;
	}
	TestTrue(TEXT("severity comes from the policy, not from the code"), Strict->Severity == ECPM_StageSeverity::Error);

	// The fixed rules, one at a time from a clean level, so each severity is pinned to its own rule.
	const FCPM_StageLimits None;
	TArray<FCPM_StageIssue> Everything;

	{
		FCPM_StageFacts Case = OfficeFacts();
		Case.bDirty = true;
		const TArray<FCPM_StageIssue> Issues = Evaluate(Case, None);
		Everything.Append(Issues);
		const FCPM_StageIssue* Dirty = Find(Issues, TEXT("unsaved changes"));
		if (TestNotNull(TEXT("an unsaved level is reported"), Dirty))
		{
			TestTrue(TEXT("an unsaved level warns"), Dirty->Severity == ECPM_StageSeverity::Warning);
		}
		TestEqual(TEXT("an unsaved level does not close the upload"), CountOf(Issues, ECPM_StageSeverity::Error), 0);
	}

	{
		FCPM_StageFacts Case = OfficeFacts();
		Case.AttachedToAvatar = { Actor(TEXT("BP_Hat")) };
		const TArray<FCPM_StageIssue> Issues = Evaluate(Case, None);
		Everything.Append(Issues);
		const FCPM_StageIssue* Attached = Find(Issues, TEXT("BP_Hat is attached to the avatar"));
		if (TestNotNull(TEXT("an actor attached to the avatar is reported"), Attached))
		{
			TestTrue(TEXT("it warns"), Attached->Severity == ECPM_StageSeverity::Warning);
		}
		TestEqual(TEXT("it does not close the upload"), CountOf(Issues, ECPM_StageSeverity::Error), 0);
	}

	{
		FCPM_StageFacts Case = OfficeFacts();
		Case.AspectConstrainedCameras = { Case.Cameras[0] };
		const TArray<FCPM_StageIssue> Issues = Evaluate(Case, None);
		Everything.Append(Issues);
		const FCPM_StageIssue* Aspect = Find(Issues, TEXT("constrains aspect ratio"));
		if (TestNotNull(TEXT("a constrained camera is reported"), Aspect))
		{
			TestTrue(TEXT("it warns"), Aspect->Severity == ECPM_StageSeverity::Warning);
			TestEqual(TEXT("it tells the creator which box to untick"), Aspect->Reason,
				FString(TEXT("CAM_Front constrains aspect ratio - untick Constrain Aspect Ratio")));
		}
		TestEqual(TEXT("it does not close the upload"), CountOf(Issues, ECPM_StageSeverity::Error), 0);
	}

	{
		FCPM_StageFacts Case = OfficeFacts();
		Case.AutoActivatingCameras = { Case.Cameras[0] };
		const TArray<FCPM_StageIssue> Issues = Evaluate(Case, None);
		Everything.Append(Issues);
		const FCPM_StageIssue* AutoActivate = Find(Issues, TEXT("auto-activates"));
		if (TestNotNull(TEXT("a camera that grabs the player is reported"), AutoActivate))
		{
			TestTrue(TEXT("it errors"), AutoActivate->Severity == ECPM_StageSeverity::Error);
		}
	}

	{
		FCPM_StageFacts Case = OfficeFacts();
		const FCPM_StageActor Bell = Actor(TEXT("BP_Bell"));
		Case.AvatarReferencers = { Bell };
		const TArray<FCPM_StageIssue> Issues = Evaluate(Case, None);
		Everything.Append(Issues);
		const FCPM_StageIssue* Reference = Find(Issues, TEXT("BP_Bell references the placed avatar"));
		if (TestNotNull(TEXT("a hard reference to the avatar is reported"), Reference))
		{
			TestTrue(TEXT("it errors"), Reference->Severity == ECPM_StageSeverity::Error);
			TestTrue(TEXT("a click on it selects the referencing actor"), Reference->ActorGuid == Bell.Guid);
		}
	}

	{
		FCPM_StageFacts Case = OfficeFacts();
		Case.bNavMeshBounds = false;
		const TArray<FCPM_StageIssue> Issues = Evaluate(Case, None);
		Everything.Append(Issues);
		const FCPM_StageIssue* NavMesh = Find(Issues, TEXT("NavMeshBoundsVolume"));
		if (TestNotNull(TEXT("a stage without nav mesh bounds is reported"), NavMesh))
		{
			TestTrue(TEXT("it errors"), NavMesh->Severity == ECPM_StageSeverity::Error);
		}
	}

	{
		FCPM_StageFacts Case = OfficeFacts();
		Case.Cameras = { Actor(TEXT("CAM_Front")), Actor(TEXT("CAM_Side")) };
		Case.CameraActorName = NAME_None;
		const TArray<FCPM_StageIssue> Untagged = Evaluate(Case, None);
		Everything.Append(Untagged);
		const FCPM_StageIssue* Which = Find(Untagged, TEXT("Which camera?"));
		if (TestNotNull(TEXT("two cameras and no tag is reported"), Which))
		{
			TestTrue(TEXT("it errors"), Which->Severity == ECPM_StageSeverity::Error);
			TestEqual(TEXT("it names both cameras and the tag to use"), Which->Reason,
				FString(TEXT("Which camera? CAM_Front, CAM_Side - tag one Convai.Stage.Camera")));
		}

		Case.CameraActorName = TEXT("CAM_Front");
		const TArray<FCPM_StageIssue> Tagged = Evaluate(Case, None);
		Everything.Append(Tagged);
		TestNull(TEXT("tagging one camera settles the question"), Find(Tagged, TEXT("Which camera?")));
	}

	{
		FCPM_StageFacts Case = OfficeFacts();
		Case.Unsupported = { TEXT("PlayerStart_0") };
		Case.bHasLevelBlueprint = true;
		const TArray<FCPM_StageIssue> Issues = Evaluate(Case, None);
		Everything.Append(Issues);
		const FCPM_StageIssue* Unsupported = Find(Issues, TEXT("PlayerStart_0"));
		if (TestNotNull(TEXT("an actor the studio will not use is reported"), Unsupported))
		{
			TestTrue(TEXT("it informs"), Unsupported->Severity == ECPM_StageSeverity::Info);
			TestEqual(TEXT("it says what will happen to it"), Unsupported->Reason,
				FString(TEXT("PlayerStart_0 will not be used by Avatar Studio")));
		}
		const FCPM_StageIssue* LevelBlueprint = Find(Issues, TEXT("Level Blueprint"));
		if (TestNotNull(TEXT("a Level Blueprint is reported"), LevelBlueprint))
		{
			TestTrue(TEXT("it informs"), LevelBlueprint->Severity == ECPM_StageSeverity::Info);
		}
		TestEqual(TEXT("neither closes the upload"), CountOf(Issues, ECPM_StageSeverity::Error), 0);
	}

	TestFalse(TEXT("no line of any of them tells the creator their level lost something"),
		AnyMentionsRemoval(Everything));

	return true;
}

/** A level Convai's policy refuses today passes tomorrow when the policy is raised, with no rebuild. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMStageRaisingALimitInConfigPasses,
	"ConvaiPakManager.Stage.RaisingALimitInConfigPasses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMStageRaisingALimitInConfigPasses::RunTest(const FString&)
{
	FCPM_StageFacts Facts = OfficeFacts();
	Facts.DynamicLights = {
		Actor(TEXT("SpotLight_01")), Actor(TEXT("SpotLight_02")), Actor(TEXT("SpotLight_03")),
		Actor(TEXT("SpotLight_04")), Actor(TEXT("SpotLight_05")) };
	Facts.ShadowCastingLights.Empty();
	Facts.TexturesMaxBuiltPx = 2048;

	const TArray<FCPM_StageIssue> AtFour = Evaluate(Facts,
		Limits(TEXT(R"({"stage-limits":{"dynamic-lights":{"max":4,"severity":"error"}}})")));
	TestEqual(TEXT("five lights break a limit of four"), CountOf(AtFour, ECPM_StageSeverity::Error), 1);

	const FCPM_StageIssue* Lights = Find(AtFour, TEXT("dynamic lights"));
	if (!TestNotNull(TEXT("the error names the lights"), Lights))
	{
		return false;
	}
	TestTrue(TEXT("it lists them and carries both numbers"),
		Lights->Reason.Contains(TEXT("SpotLight_05")) && Lights->Reason.Contains(TEXT("5"))
			&& Lights->Reason.Contains(TEXT("4")));
	TestEqual(TEXT("the measurement is the light count"), Lights->Measured, 5.0);
	TestEqual(TEXT("the limit is the one the policy names"), Lights->Limit, 4.0);

	const TArray<FCPM_StageIssue> AtFive = Evaluate(Facts,
		Limits(TEXT(R"({"stage-limits":{"dynamic-lights":{"max":5,"severity":"error"}}})")));
	TestEqual(TEXT("the same level passes when the policy is raised"),
		CountOf(AtFive, ECPM_StageSeverity::Error), 0);
	TestEqual(TEXT("and raises nothing else"), CountOf(AtFive, ECPM_StageSeverity::Warning), 0);

	const TArray<FCPM_StageIssue> Unnamed = Evaluate(Facts, Limits(TEXT("{}")));
	TestEqual(TEXT("a limit the policy does not name is not checked"),
		CountOf(Unnamed, ECPM_StageSeverity::Error), 0);
	TestEqual(TEXT("at any severity"), CountOf(Unnamed, ECPM_StageSeverity::Warning), 0);

	return true;
}

/** Exactly one placed avatar, because the studio spawns at the one transform the scan found. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMStageRefusesTwoAvatarsAndNone,
	"ConvaiPakManager.Stage.RefusesTwoAvatarsAndNone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMStageRefusesTwoAvatarsAndNone::RunTest(const FString&)
{
	const FCPM_StageLimits None;
	FCPM_StageFacts Facts = OfficeFacts();
	TArray<FCPM_StageIssue> Everything;

	Facts.AvatarActorNames.Empty();
	const TArray<FCPM_StageIssue> Missing = Evaluate(Facts, None);
	Everything.Append(Missing);
	const FCPM_StageIssue* NotPlaced = Find(Missing, TEXT("Avatar not placed"));
	if (TestNotNull(TEXT("an unplaced avatar is reported"), NotPlaced))
	{
		TestTrue(TEXT("it errors"), NotPlaced->Severity == ECPM_StageSeverity::Error);
		TestEqual(TEXT("it names the avatar and the level"), NotPlaced->Reason,
			FString(TEXT("Avatar not placed: BP_Receptionist is not in L_Office")));
	}
	TestNull(TEXT("and nothing claims a spawn spot"), Find(Missing, TEXT("found at")));

	Facts.AvatarActorNames = { TEXT("BP_Receptionist_C_1"), TEXT("BP_Receptionist_C_2") };
	const TArray<FCPM_StageIssue> Two = Evaluate(Facts, None);
	Everything.Append(Two);
	const FCPM_StageIssue* TooMany = Find(Two, TEXT("More than one"));
	if (TestNotNull(TEXT("two placed avatars are reported"), TooMany))
	{
		TestTrue(TEXT("it errors"), TooMany->Severity == ECPM_StageSeverity::Error);
		TestEqual(TEXT("it counts them and says what to do"), TooMany->Reason,
			FString(TEXT("More than one BP_Receptionist placed (2) - keep one")));
	}
	TestNull(TEXT("and no spawn spot is claimed while there are two"), Find(Two, TEXT("found at")));

	Facts.AvatarActorNames = { TEXT("BP_Receptionist_C_1") };
	const TArray<FCPM_StageIssue> One = Evaluate(Facts, None);
	Everything.Append(One);
	TestEqual(TEXT("one placed avatar refuses nothing"), CountOf(One, ECPM_StageSeverity::Error), 0);
	TestNotNull(TEXT("and the scan says where it will be spawned"), Find(One, TEXT("found at")));

	TestFalse(TEXT("no line of any of them tells the creator their level lost something"),
		AnyMentionsRemoval(Everything));

	return true;
}

/**
 * The two rules whose severity the walkthrough settles by hand, and the lighting flag they feed:
 * a static light is the creator's mistake to fix, a sublevel is content the studio never shows.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMStageEvaluateWarnsOnStaticLightAndSublevels,
	"ConvaiPakManager.Stage.EvaluateWarnsOnStaticLightAndSublevels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMStageEvaluateWarnsOnStaticLightAndSublevels::RunTest(const FString&)
{
	const FCPM_StageLimits None;

	FCPM_StageFacts StaticOnly = OfficeFacts();
	StaticOnly.DynamicLights.Empty();
	StaticOnly.ShadowCastingLights.Empty();
	StaticOnly.StaticLights = { Actor(TEXT("SpotLight_01")) };

	const TArray<FCPM_StageIssue> Issues = Evaluate(StaticOnly, None);
	const FCPM_StageIssue* Light = Find(Issues, TEXT("Static light"));
	if (TestNotNull(TEXT("a static light is reported"), Light))
	{
		TestTrue(TEXT("it warns"), Light->Severity == ECPM_StageSeverity::Warning);
		TestEqual(TEXT("it says what to change"), Light->Reason,
			FString(TEXT("Static light SpotLight_01 will not render in Avatar Studio - set Mobility to Movable")));
		TestTrue(TEXT("a click on it selects the light"), Light->ActorGuid == StaticOnly.StaticLights[0].Guid);
	}
	TestEqual(TEXT("a static light does not close the upload"), CountOf(Issues, ECPM_StageSeverity::Error), 0);

	TestFalse(TEXT("a level lit only by static lights must not switch the studio's lighting off"),
		StaticOnly.HasCustomLighting());
	StaticOnly.bSkyLight = true;
	TestTrue(TEXT("a sky light is lighting the studio must stand down for"), StaticOnly.HasCustomLighting());

	FCPM_StageFacts Streaming = OfficeFacts();
	Streaming.Sublevels = 2;
	const TArray<FCPM_StageIssue> WithSublevels = Evaluate(Streaming, None);
	const FCPM_StageIssue* Sublevels = Find(WithSublevels, TEXT("sublevels"));
	if (TestNotNull(TEXT("streaming sublevels are reported"), Sublevels))
	{
		TestTrue(TEXT("they error"), Sublevels->Severity == ECPM_StageSeverity::Error);
		TestTrue(TEXT("the line counts them and says only the persistent level streams"),
			Sublevels->Reason.Contains(TEXT("2 sublevels")) && Sublevels->Reason.Contains(TEXT("persistent level")));
	}

	Streaming.Sublevels = 0;
	const TArray<FCPM_StageIssue> WithoutSublevels = Evaluate(Streaming, None);
	TestNull(TEXT("a level with no sublevels is not asked about them"),
		Find(WithoutSublevels, TEXT("sublevel")));

	TestTrue(TEXT("a tagged camera is the one the studio is handed"), OfficeFacts().HasCustomCamera());
	FCPM_StageFacts NoCamera = OfficeFacts();
	NoCamera.CameraActorName = NAME_None;
	TestFalse(TEXT("and with none chosen the studio keeps its own"), NoCamera.HasCustomCamera());

	return true;
}

/**
 * The record is the wire object: five keys, vectors as objects because the studio imports objects,
 * and none of the handles or counts the scan carried to get there.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMStageRecordRoundTrips,
	"ConvaiPakManager.Stage.RecordRoundTrips",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMStageRecordRoundTrips::RunTest(const FString&)
{
	const FString Directory = FPaths::Combine(
		FPaths::ProjectIntermediateDir(), TEXT("CPM_Tests"), TEXT("CPM_StageTest"));
	IFileManager::Get().DeleteDirectory(*Directory, false, true);
	IFileManager::Get().MakeDirectory(*Directory, true);

	const FString Path = FPaths::Combine(Directory, TEXT("Stage_10.json"));
	const FString LevelPackage = TEXT("/JBILN5CDNI4TRYELD6CS/Stage/L_Office_Stage");

	TestTrue(TEXT("the record is written"), WriteStageRecordAt(Path, OfficeFacts(), LevelPackage));

	const TSharedPtr<FJsonObject> Record = ReadStageRecordAt(Path);
	if (!TestTrue(TEXT("and reads back"), Record.IsValid()))
	{
		return false;
	}

	TestEqual(TEXT("it carries the five keys the studio reads and nothing else"), RecordKeys(Record),
		FString(TEXT("avatar_transform,has_custom_camera,has_custom_lighting,level,schema")));
	TestEqual(TEXT("the schema is the first one"), Record->GetIntegerField(TEXT("schema")), 1);
	TestEqual(TEXT("the level is the copy's package, verbatim"), Record->GetStringField(TEXT("level")), LevelPackage);

	const TSharedPtr<FJsonObject> Transform = Nested(Record, TEXT("avatar_transform"));
	const TSharedPtr<FJsonObject> Location = Nested(Transform, TEXT("location"));
	TestEqual(TEXT("location x"), NumberIn(Location, TEXT("x")), 120.0);
	TestEqual(TEXT("location y"), NumberIn(Location, TEXT("y")), -40.0);
	TestEqual(TEXT("location z"), NumberIn(Location, TEXT("z")), 0.0);

	const TSharedPtr<FJsonObject> Rotation = Nested(Transform, TEXT("rotation"));
	TestEqual(TEXT("rotation pitch"), NumberIn(Rotation, TEXT("pitch")), 0.0);
	TestEqual(TEXT("rotation yaw"), NumberIn(Rotation, TEXT("yaw")), 90.0);
	TestEqual(TEXT("rotation roll"), NumberIn(Rotation, TEXT("roll")), 0.0);

	const TSharedPtr<FJsonObject> Scale = Nested(Transform, TEXT("scale"));
	TestEqual(TEXT("scale x"), NumberIn(Scale, TEXT("x")), 1.0);
	TestEqual(TEXT("scale y"), NumberIn(Scale, TEXT("y")), 1.0);
	TestEqual(TEXT("scale z"), NumberIn(Scale, TEXT("z")), 1.0);

	bool bFlag = false;
	TestTrue(TEXT("the office level lights itself"),
		Record->TryGetBoolField(TEXT("has_custom_lighting"), bFlag) && bFlag);
	TestTrue(TEXT("and hands the studio a camera"),
		Record->TryGetBoolField(TEXT("has_custom_camera"), bFlag) && bFlag);

	FString Written;
	FFileHelper::LoadFileToString(Written, *Path);
	TestFalse(TEXT("the actor handles and the counts never reach the record"),
		Written.Contains(TEXT("BP_Receptionist_C_1")) || Written.Contains(TEXT("CAM_Front"))
			|| Written.Contains(TEXT("actors")));

	FCPM_StageFacts Bare = OfficeFacts();
	Bare.DynamicLights.Empty();
	Bare.ShadowCastingLights.Empty();
	Bare.StaticLights = { Actor(TEXT("SpotLight_01")) };
	Bare.CameraActorName = NAME_None;
	TestTrue(TEXT("a stage with neither is written"), WriteStageRecordAt(Path, Bare, LevelPackage));

	const TSharedPtr<FJsonObject> BareRecord = ReadStageRecordAt(Path);
	if (TestTrue(TEXT("and reads back"), BareRecord.IsValid()))
	{
		TestTrue(TEXT("static lights alone are not custom lighting"),
			BareRecord->TryGetBoolField(TEXT("has_custom_lighting"), bFlag) && !bFlag);
		TestTrue(TEXT("and no chosen camera is not a custom camera"),
			BareRecord->TryGetBoolField(TEXT("has_custom_camera"), bFlag) && !bFlag);
	}

	TestFalse(TEXT("a record that is not there is not an answer"),
		ReadStageRecordAt(FPaths::Combine(Directory, TEXT("Stage_11.json"))).IsValid());

	// Logged as a warning, never a refusal: the next publish rewrites this file from the facts.
	FFileHelper::SaveStringToFile(FString(TEXT("{ not json")), *Path);
	TestFalse(TEXT("nor is one that does not parse"), ReadStageRecordAt(Path).IsValid());

	IFileManager::Get().DeleteDirectory(*Directory, false, true);
	return true;
}

#endif
