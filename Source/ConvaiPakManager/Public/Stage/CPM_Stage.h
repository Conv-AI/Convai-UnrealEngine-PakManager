// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Publish/CPM_PublishTypes.h"
#include "UObject/SoftObjectPath.h"
#include "CPM_Stage.generated.h"

class FJsonObject;

/** One actor of the stage, as an issue names it and a click finds it. */
USTRUCT()
struct CONVAIPAKMANAGER_API FCPM_StageActor
{
	GENERATED_BODY()

	/** The label the creator sees in the Outliner. */
	UPROPERTY()
	FString Label;

	UPROPERTY()
	FSoftObjectPath Path;

	UPROPERTY()
	FGuid Guid;
};

/**
 * What a scan read off the source level, with the avatar left out of every fact but TexturesCount:
 * the studio spawns its own, so the stage is judged without it.
 *
 * Filled by the half that reads the world, judged by Evaluate, and written down by
 * WriteStageRecordAt - which takes five things from it and nothing else. AvatarActorNames and
 * CameraActorName are handles for the copy step, never written anywhere.
 */
USTRUCT()
struct CONVAIPAKMANAGER_API FCPM_StageFacts
{
	GENERATED_BODY()

	/** Long package name of the level scanned. */
	UPROPERTY()
	FString SourceLevel;

	UPROPERTY()
	bool bPartitioned = false;

	UPROPERTY()
	int32 Sublevels = 0;

	/** The map, its built data or any external actor package has unsaved changes. */
	UPROPERTY()
	bool bDirty = false;

	/** The Entry Point as the creator names it, e.g. "BP_Receptionist". */
	UPROPERTY()
	FString AvatarName;

	UPROPERTY()
	TArray<FName> AvatarActorNames;

	/** Where the placed avatar stands, in level space. Meaningful only when exactly one is placed. */
	UPROPERTY()
	FVector AvatarLocation = FVector::ZeroVector;

	UPROPERTY()
	FRotator AvatarRotation = FRotator::ZeroRotator;

	UPROPERTY()
	FVector AvatarScale = FVector::OneVector;

	/** Placed stage actors after the skips: never the avatar, nor a child actor of anything. */
	UPROPERTY()
	int32 Actors = 0;

	/** Movable and Stationary lights. Static ones are apart because they do not render in the studio. */
	UPROPERTY()
	TArray<FCPM_StageActor> DynamicLights;

	UPROPERTY()
	TArray<FCPM_StageActor> StaticLights;

	UPROPERTY()
	TArray<FCPM_StageActor> ShadowCastingLights;

	UPROPERTY()
	bool bSkyLight = false;

	UPROPERTY()
	bool bSkyAtmosphere = false;

	UPROPERTY()
	bool bHeightFog = false;

	UPROPERTY()
	bool bVolumetricCloud = false;

	UPROPERTY()
	int32 StaticMeshComponents = 0;

	UPROPERTY()
	int32 StaticMeshInstances = 0;

	/** LOD0, summed. */
	UPROPERTY()
	int32 Triangles = 0;

	/** Every texture the stage's materials use, plus the avatar's - the one fact the avatar counts in. */
	UPROPERTY()
	int32 TexturesCount = 0;

	/** The largest built edge in pixels among them, and which texture it is. */
	UPROPERTY()
	int32 TexturesMaxBuiltPx = 0;

	UPROPERTY()
	FSoftObjectPath LargestTexture;

	/** Where the largest texture is used, e.g. "M_Desk on SM_Desk". Empty when unknown. */
	UPROPERTY()
	FString LargestTextureContext;

	UPROPERTY()
	double TexturesMemoryMb = 0.0;

	UPROPERTY()
	TArray<FCPM_StageActor> Cameras;

	/** The camera the studio is handed: the only one, or the one tagged Convai.Stage.Camera. None otherwise. */
	UPROPERTY()
	FName CameraActorName;

	UPROPERTY()
	TArray<FCPM_StageActor> AutoActivatingCameras;

	UPROPERTY()
	TArray<FCPM_StageActor> AspectConstrainedCameras;

	UPROPERTY()
	bool bNavMeshBounds = false;

	UPROPERTY()
	int32 ConvaiObjects = 0;

	/** Stage actors holding a hard reference to the placed avatar. */
	UPROPERTY()
	TArray<FCPM_StageActor> AvatarReferencers;

	/** Stage actors attached to the avatar in the Outliner. */
	UPROPERTY()
	TArray<FCPM_StageActor> AttachedToAvatar;

	/** Long package names the stage reaches outside the Modding Plugin. */
	UPROPERTY()
	TArray<FString> OutsideRoots;

	/** What the studio will not use, by label: player starts, foreign pawns, a game mode. */
	UPROPERTY()
	TArray<FString> Unsupported;

	UPROPERTY()
	bool bHasLevelBlueprint = false;

	/** The record's has_custom_lighting. Static lights alone do not count: they never render there. */
	bool HasCustomLighting() const;

	/** The record's has_custom_camera. */
	bool HasCustomCamera() const;
};

/** One line of the scan's issue list. */
USTRUCT()
struct CONVAIPAKMANAGER_API FCPM_StageIssue
{
	GENERATED_BODY()

	UPROPERTY()
	ECPM_StageSeverity Severity = ECPM_StageSeverity::Info;

	/** The asset or actor a click selects. Empty when there is no one thing to point at. */
	UPROPERTY()
	FSoftObjectPath Target;

	/** Set when Target is an actor, so the click can select it. */
	UPROPERTY()
	FGuid ActorGuid;

	/** The whole line the creator reads. */
	UPROPERTY()
	FString Reason;

	/** The value measured and the limit it broke. Both zero for a rule that has no number. */
	UPROPERTY()
	double Measured = 0.0;

	UPROPERTY()
	double Limit = 0.0;
};

/**
 * The stage a level becomes when it is published with an Avatar. See CONTEXT.md.
 *
 * Everything that DECIDES here is pure and is handed the facts, so the deciding is testable without
 * a level, a policy read or a project on disk. Reading the facts off the world is one separate
 * function, and it does not live in this slice.
 */
namespace ConvaiPakManager::Stage
{
/**
 * Judges the facts against the limits.
 *
 * One issue per limit the policy names, at the severity it names; a limit it does not name is not
 * checked. The rules the policy cannot express are fixed here with their severities. Nothing here
 * tells the creator the avatar is taken out: the studio spawns it where it was found, and that is
 * what the Info line says.
 */
CONVAIPAKMANAGER_API TArray<FCPM_StageIssue> Evaluate(const FCPM_StageFacts& Facts, const FCPM_StageLimits& Limits);

/**
 * Writes the record the studio reads - schema, level, avatar_transform, has_custom_lighting,
 * has_custom_camera - and nothing else. It is injected verbatim into the wire metadata, so the file
 * IS the wire object and a key without a reader in the studio has no business in it. False when the
 * file could not be written.
 */
CONVAIPAKMANAGER_API bool WriteStageRecordAt(const FString& Path, const FCPM_StageFacts& Facts, const FString& LevelPackage);

/** The record at Path, or null when there is none or it does not parse. */
CONVAIPAKMANAGER_API TSharedPtr<FJsonObject> ReadStageRecordAt(const FString& Path);
}
