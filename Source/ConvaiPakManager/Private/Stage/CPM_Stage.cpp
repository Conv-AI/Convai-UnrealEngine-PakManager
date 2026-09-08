// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Stage/CPM_Stage.h"

#include "Dom/JsonObject.h"
#include "Misc/EnumRange.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Utility/CPM_Log.h"

bool FCPM_StageFacts::HasCustomLighting() const
{
	// Static lights are left out: they need a build the studio never runs, so a level lit only by
	// them arrives dark - and must not be the reason the studio's own lighting stands down.
	return DynamicLights.Num() > 0 || bSkyLight || bSkyAtmosphere || bHeightFog || bVolumetricCloud;
}

bool FCPM_StageFacts::HasCustomCamera() const
{
	return !CameraActorName.IsNone();
}

namespace ConvaiPakManager::Stage
{
namespace
{
	FCPM_StageIssue Issue(const ECPM_StageSeverity Severity, FString Reason, const FCPM_StageActor* Actor = nullptr)
	{
		FCPM_StageIssue Result;
		Result.Severity = Severity;
		Result.Reason = MoveTemp(Reason);
		if (Actor)
		{
			Result.Target = Actor->Path;
			Result.ActorGuid = Actor->Guid;
		}
		return Result;
	}

	FString Labels(const TArray<FCPM_StageActor>& Actors)
	{
		TArray<FString> Names;
		Names.Reserve(Actors.Num());
		for (const FCPM_StageActor& Actor : Actors)
		{
			Names.Add(Actor.Label);
		}
		return FString::Join(Names, TEXT(", "));
	}

	/** The one fact each limit is measured against. Walkthrough S4's limit-to-fact table. */
	double Measure(const FCPM_StageFacts& Facts, const ECPM_StageLimit Limit)
	{
		switch (Limit)
		{
		case ECPM_StageLimit::DynamicLights: return Facts.DynamicLights.Num();
		case ECPM_StageLimit::ShadowLights: return Facts.ShadowCastingLights.Num();
		// Components, not instances: an instanced mesh is one thing to draw with, however many it draws.
		case ECPM_StageLimit::StaticMeshes: return Facts.StaticMeshComponents;
		case ECPM_StageLimit::Triangles: return Facts.Triangles;
		case ECPM_StageLimit::Textures: return Facts.TexturesCount;
		case ECPM_StageLimit::TextureMaxSize: return Facts.TexturesMaxBuiltPx;
		case ECPM_StageLimit::TextureMemoryMb: return Facts.TexturesMemoryMb;
		case ECPM_StageLimit::ConvaiObjects: return Facts.ConvaiObjects;
		case ECPM_StageLimit::Actors: return Facts.Actors;
		default: return 0.0;
		}
	}

	FCPM_StageIssue LimitIssue(
		const FCPM_StageFacts& Facts, const ECPM_StageLimit Limit, const FCPM_StageLimitRule& Rule, const double Measured)
	{
		FCPM_StageIssue Result;
		Result.Severity = Rule.Severity;
		Result.Measured = Measured;
		Result.Limit = Rule.Max;

		switch (Limit)
		{
		case ECPM_StageLimit::TextureMaxSize:
			Result.Target = Facts.LargestTexture;
			Result.Reason = FString::Printf(TEXT("Texture too large: %s - %.0f px, max %.0f px"),
				*Facts.LargestTexture.GetAssetName(), Measured, Rule.Max);
			if (!Facts.LargestTextureContext.IsEmpty())
			{
				Result.Reason += FString::Printf(TEXT(" (%s)"), *Facts.LargestTextureContext);
			}
			break;

		case ECPM_StageLimit::DynamicLights:
			if (Facts.DynamicLights.Num() > 0)
			{
				Result.Target = Facts.DynamicLights[0].Path;
				Result.ActorGuid = Facts.DynamicLights[0].Guid;
			}
			Result.Reason = FString::Printf(TEXT("Too many dynamic lights: %s - %.0f, max %.0f"),
				*Labels(Facts.DynamicLights), Measured, Rule.Max);
			break;

		case ECPM_StageLimit::ShadowLights:
			if (Facts.ShadowCastingLights.Num() > 0)
			{
				Result.Target = Facts.ShadowCastingLights[0].Path;
				Result.ActorGuid = Facts.ShadowCastingLights[0].Guid;
			}
			Result.Reason = FString::Printf(TEXT("Shadow-casting light%s: %s - %.0f, max %.0f"),
				Facts.ShadowCastingLights.Num() == 1 ? TEXT("") : TEXT("s"),
				*Labels(Facts.ShadowCastingLights), Measured, Rule.Max);
			break;

		case ECPM_StageLimit::StaticMeshes:
			Result.Reason = FString::Printf(TEXT("Static meshes: %.0f components (%d instances), max %.0f"),
				Measured, Facts.StaticMeshInstances, Rule.Max);
			break;

		case ECPM_StageLimit::Triangles:
			Result.Reason = FString::Printf(TEXT("Triangles: %.0f, max %.0f"), Measured, Rule.Max);
			break;

		case ECPM_StageLimit::Textures:
			Result.Reason = FString::Printf(TEXT("Textures: %.0f including the avatar's, max %.0f"), Measured, Rule.Max);
			break;

		case ECPM_StageLimit::TextureMemoryMb:
			Result.Reason = FString::Printf(TEXT("Texture memory: %.0f MB, max %.0f MB"), Measured, Rule.Max);
			break;

		case ECPM_StageLimit::ConvaiObjects:
			Result.Reason = FString::Printf(TEXT("Convai objects: %.0f, max %.0f"), Measured, Rule.Max);
			break;

		case ECPM_StageLimit::Actors:
			Result.Reason = FString::Printf(TEXT("Actors: %.0f, max %.0f"), Measured, Rule.Max);
			break;

		default:
			break;
		}

		return Result;
	}

	/** {x,y,z} and {pitch,yaw,roll} both, because the studio's importer reads an object, not an array. */
	TSharedPtr<FJsonObject> Triple(
		const TCHAR* KeyA, const double A, const TCHAR* KeyB, const double B, const TCHAR* KeyC, const double C)
	{
		const TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(KeyA, A);
		Object->SetNumberField(KeyB, B);
		Object->SetNumberField(KeyC, C);
		return Object;
	}
}

TArray<FCPM_StageIssue> Evaluate(const FCPM_StageFacts& Facts, const FCPM_StageLimits& Limits)
{
	TArray<FCPM_StageIssue> Issues;
	const FString Level = FPackageName::GetShortName(Facts.SourceLevel);

	// The placed avatar is the transform the studio spawns at, so exactly one is the whole question -
	// and the Info line is how the creator learns that the spot they see is the spot that ships.
	if (Facts.AvatarActorNames.Num() == 0)
	{
		Issues.Add(Issue(ECPM_StageSeverity::Error,
			FString::Printf(TEXT("Avatar not placed: %s is not in %s"), *Facts.AvatarName, *Level)));
	}
	else if (Facts.AvatarActorNames.Num() > 1)
	{
		Issues.Add(Issue(ECPM_StageSeverity::Error,
			FString::Printf(TEXT("More than one %s placed (%d) - keep one"),
				*Facts.AvatarName, Facts.AvatarActorNames.Num())));
	}
	else
	{
		Issues.Add(Issue(ECPM_StageSeverity::Info,
			FString::Printf(TEXT("%s found at (%.0f, %.0f, %.0f) - Avatar Studio will spawn it here."),
				*Facts.AvatarName, Facts.AvatarLocation.X, Facts.AvatarLocation.Y, Facts.AvatarLocation.Z)));
	}

	if (Facts.Cameras.Num() > 1 && Facts.CameraActorName.IsNone())
	{
		Issues.Add(Issue(ECPM_StageSeverity::Error,
			FString::Printf(TEXT("Which camera? %s - tag one Convai.Stage.Camera"), *Labels(Facts.Cameras))));
	}

	// AutoActivateForPlayer is private and hard-cuts in BeginPlay, so the copy cannot clear it for
	// the creator: the level itself has to change.
	for (const FCPM_StageActor& Camera : Facts.AutoActivatingCameras)
	{
		Issues.Add(Issue(ECPM_StageSeverity::Error,
			FString::Printf(TEXT("%s auto-activates for the player - set it to Disabled"), *Camera.Label), &Camera));
	}

	for (const FCPM_StageActor& Camera : Facts.AspectConstrainedCameras)
	{
		Issues.Add(Issue(ECPM_StageSeverity::Warning,
			FString::Printf(TEXT("%s constrains aspect ratio - untick Constrain Aspect Ratio"), *Camera.Label), &Camera));
	}

	for (const FCPM_StageActor& Referencer : Facts.AvatarReferencers)
	{
		Issues.Add(Issue(ECPM_StageSeverity::Error,
			FString::Printf(TEXT("%s references the placed avatar - reference it through Convai actions/objects instead"),
				*Referencer.Label), &Referencer));
	}

	if (!Facts.bNavMeshBounds)
	{
		Issues.Add(Issue(ECPM_StageSeverity::Error,
			TEXT("No NavMesh: add a NavMeshBoundsVolume over the stage floor - Move To and Follow need one")));
	}

	if (Facts.bDirty)
	{
		Issues.Add(Issue(ECPM_StageSeverity::Warning,
			FString::Printf(TEXT("%s has unsaved changes - the upload copies what is on disk"), *Level)));
	}

	if (Facts.Sublevels > 0)
	{
		Issues.Add(Issue(ECPM_StageSeverity::Error,
			FString::Printf(TEXT("%s has %d sublevel%s - Avatar Studio streams only the persistent level; ")
				TEXT("move their actors into it (Levels panel -> Move Selected Actors to Level) and remove them"),
				*Level, Facts.Sublevels, Facts.Sublevels == 1 ? TEXT("") : TEXT("s"))));
	}

	for (const FCPM_StageActor& Light : Facts.StaticLights)
	{
		Issues.Add(Issue(ECPM_StageSeverity::Warning,
			FString::Printf(TEXT("Static light %s will not render in Avatar Studio - set Mobility to Movable"),
				*Light.Label), &Light));
	}

	for (const FCPM_StageActor& Attached : Facts.AttachedToAvatar)
	{
		Issues.Add(Issue(ECPM_StageSeverity::Warning,
			FString::Printf(TEXT("%s is attached to the avatar and will be left in the stage at its world position ")
				TEXT("- attach it inside the avatar blueprint instead"), *Attached.Label), &Attached));
	}

	for (const FString& Entry : Facts.Unsupported)
	{
		Issues.Add(Issue(ECPM_StageSeverity::Info,
			FString::Printf(TEXT("%s will not be used by Avatar Studio"), *Entry)));
	}

	if (Facts.bHasLevelBlueprint)
	{
		Issues.Add(Issue(ECPM_StageSeverity::Info,
			FString::Printf(TEXT("%s has a Level Blueprint; it will run in Avatar Studio"), *Level)));
	}

	for (const ECPM_StageLimit Limit : TEnumRange<ECPM_StageLimit>())
	{
		const FCPM_StageLimitRule* Rule = Limits.Rules.Find(Limit);
		if (!Rule)
		{
			continue;
		}

		const double Measured = Measure(Facts, Limit);
		if (Measured > Rule->Max)
		{
			Issues.Add(LimitIssue(Facts, Limit, *Rule, Measured));
		}
	}

	return Issues;
}

bool WriteStageRecordAt(const FString& Path, const FCPM_StageFacts& Facts, const FString& LevelPackage)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schema"), 1);
	Root->SetStringField(TEXT("level"), LevelPackage);

	const TSharedPtr<FJsonObject> Transform = MakeShared<FJsonObject>();
	Transform->SetObjectField(TEXT("location"),
		Triple(TEXT("x"), Facts.AvatarLocation.X, TEXT("y"), Facts.AvatarLocation.Y, TEXT("z"), Facts.AvatarLocation.Z));
	Transform->SetObjectField(TEXT("rotation"),
		Triple(TEXT("pitch"), Facts.AvatarRotation.Pitch, TEXT("yaw"), Facts.AvatarRotation.Yaw,
			TEXT("roll"), Facts.AvatarRotation.Roll));
	Transform->SetObjectField(TEXT("scale"),
		Triple(TEXT("x"), Facts.AvatarScale.X, TEXT("y"), Facts.AvatarScale.Y, TEXT("z"), Facts.AvatarScale.Z));
	Root->SetObjectField(TEXT("avatar_transform"), Transform);

	Root->SetBoolField(TEXT("has_custom_lighting"), Facts.HasCustomLighting());
	Root->SetBoolField(TEXT("has_custom_camera"), Facts.HasCustomCamera());

	FString Serialised;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Serialised);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		return false;
	}

	return FFileHelper::SaveStringToFile(Serialised, *Path);
}

TSharedPtr<FJsonObject> ReadStageRecordAt(const FString& Path)
{
	FString Contents;
	if (!FFileHelper::LoadFileToString(Contents, *Path))
	{
		return nullptr;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Contents);
	TSharedPtr<FJsonObject> Record;
	if (!FJsonSerializer::Deserialize(Reader, Record) || !Record.IsValid())
	{
		// Never a refusal: this file is rewritten from the facts on every run, so a damaged one costs
		// the Asset its stage key and nothing else.
		CPM_LOG(Warning, TEXT("%s is not valid JSON, so the Asset publishes without its stage."), *Path);
		return nullptr;
	}

	return Record;
}
}
