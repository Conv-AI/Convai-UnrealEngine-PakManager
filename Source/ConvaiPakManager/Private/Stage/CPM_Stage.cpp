// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Stage/CPM_Stage.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "ConvaiDefinitions.h"
#include "ConvaiObjectComponent.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "Engine/Brush.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/LevelStreaming.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CoreMisc.h"
#include "Misc/EnumRange.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "RenderUtils.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StaticMeshCompiler.h"
#include "TextureCompiler.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Utility/CPM_Log.h"
#include "Utility/CPM_UtilityLibrary.h"

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

int32 FCPM_StageReport::Count(const ECPM_StageSeverity Severity) const
{
	int32 Total = 0;
	for (const FCPM_StageIssue& Issue : Issues)
	{
		if (Issue.Severity == Severity)
		{
			++Total;
		}
	}
	return Total;
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

	FString Label(const AActor* Actor)
	{
		// The default argument writes a made-up label into an actor that has none, and a scan has to
		// leave the level exactly as it found it.
		const FString Authored = Actor->GetActorLabel(/*bCreateIfNone=*/false);
		return Authored.IsEmpty() ? Actor->GetName() : Authored;
	}

	FCPM_StageActor Handle(const AActor* Actor)
	{
		FCPM_StageActor Result;
		Result.Label = Label(Actor);
		Result.Path = FSoftObjectPath(Actor);
		Result.Guid = Actor->GetActorGuid();
		return Result;
	}

	/**
	 * What every product already ships, so the stage is only asked about what it adds. This mirrors
	 * ContentEveryProductShips in the subsystem, which is private to it and which this layer must not
	 * reach up to; the two have to agree.
	 */
	TArray<FString> ShippedRoots()
	{
		FCPM_ModdingMetadata Metadata;
		UCPM_UtilityLibrary::GetModdingMetadata(Metadata);
		return { TEXT("/ConvAI/"), TEXT("/ConvaiHTTP/"), TEXT("/Engine/EditorBlueprintResources/"),
			TEXT("/Engine/EditorResources/"), TEXT("/") + Metadata.PluginName + TEXT("/") };
	}

	/** Where a package name is judged against those roots, and where the answer is collected. */
	struct FRootCheck
	{
		FString WorldPackage;
		TArray<FString> Shipped;
		TArray<FString>& Out;

		void Note(const UObject* Object)
		{
			if (!Object)
			{
				return;
			}

			const FString Package = Object->GetOutermost()->GetName();
			// The level's own package IS the stage, and /Script/ is code: neither is content to gather.
			if (Package == WorldPackage || Package.StartsWith(TEXT("/Script/")))
			{
				return;
			}

			for (const FString& Root : Shipped)
			{
				if (Package.StartsWith(Root))
				{
					return;
				}
			}
			Out.AddUnique(Package);
		}
	};

	/**
	 * Whether one ancestry chain reaches a placed avatar. Child-actor parenting and attachment are
	 * unrelated relationships and the stage reads them apart: a child actor of the avatar was never
	 * placed, an actor attached to it was and is only warned about.
	 */
	bool ChainReachesAvatar(const AActor* Actor, const TSet<const AActor*>& Avatars, const bool bAttachment)
	{
		const AActor* Current = Actor;
		// A cycle cannot happen, but a scan is not where anyone wants to find out.
		for (int32 Step = 0; Current && Step < 64; ++Step)
		{
			Current = bAttachment ? Current->GetAttachParentActor() : Current->GetParentActor();
			if (Current && Avatars.Contains(Current))
			{
				return true;
			}
		}
		return false;
	}

	/** What the walk gathers, so the reads that stall on compilation all happen once, at the end. */
	struct FCollected
	{
		/** Unique, so the wait for compilation is one wait. */
		TArray<UStaticMesh*> Meshes;

		/** Per component with its instance count, so a mesh drawn twice is counted twice. */
		TArray<TPair<UStaticMesh*, int32>> MeshUses;

		TArray<USkeletalMesh*> SkeletalUses;

		TSet<UTexture*> Textures;

		/** Kept apart because only the count takes them in - see FCPM_StageFacts::TexturesMaxBuiltPx. */
		TSet<UTexture*> AvatarTextures;

		/** "M_Desk on SM_Desk", from the first use that found the texture. */
		TMap<UTexture*, FString> Context;
	};

	/** Roots is null for the avatar's own meshes: its content is the studio's question, not the stage's. */
	void CollectTextures(const AActor* Actor, const UMeshComponent* Mesh, FCollected& Collected, FRootCheck* Roots)
	{
		TSet<UTexture*>& Found = Roots ? Collected.Textures : Collected.AvatarTextures;

		TArray<UMaterialInterface*> Materials;
		// Not every override resets the array, and UPrimitiveComponent's own version writes nothing.
		Mesh->GetUsedMaterials(Materials);

		for (UMaterialInterface* Material : Materials)
		{
			if (!Material)
			{
				continue;
			}
			if (Roots)
			{
				Roots->Note(Material);
			}

			TArray<UTexture*> Used;
			// The overload taking quality and feature levels is deprecated with an empty body: it still
			// compiles, returns nothing, and would read every stage as textureless.
			Material->GetUsedTextures(Used);

			for (UTexture* Texture : Used)
			{
				if (!Texture)
				{
					continue;
				}

				bool bSeen = false;
				Found.Add(Texture, &bSeen);
				if (!bSeen && Roots)
				{
					Collected.Context.Add(Texture,
						FString::Printf(TEXT("%s on %s"), *Material->GetName(), *Label(Actor)));
				}
				if (Roots)
				{
					Roots->Note(Texture);
				}
			}
		}
	}

	void CollectComponents(AActor* Actor, FCPM_StageFacts& Facts, FCollected& Collected, FRootCheck& Roots)
	{
		TInlineComponentArray<UActorComponent*> Components(Actor);
		for (UActorComponent* Component : Components)
		{
			// Visualization components are the camera's proxy mesh and frustum, the billboards, the
			// arrows: they draw for the creator here and for nobody in the studio.
			if (!Component || Component->IsEditorOnly() || Component->IsVisualizationComponent())
			{
				continue;
			}

			if (const ULightComponent* Light = Cast<ULightComponent>(Component))
			{
				if (Light->bAffectsWorld)
				{
					// Static only: a Stationary light still renders without a build, so it is not one of
					// the lights that arrive dark.
					(Light->HasStaticLighting() ? Facts.StaticLights : Facts.DynamicLights).Add(Handle(Actor));
					if (Light->CastsDynamicShadow())
					{
						Facts.ShadowCastingLights.Add(Handle(Actor));
					}
				}
				continue;
			}

			// A sky light hangs off ULightComponentBase rather than ULightComponent, so the cast above
			// never sees one and the light counts never include one.
			if (const USkyLightComponent* Sky = Cast<USkyLightComponent>(Component))
			{
				if (Sky->bAffectsWorld)
				{
					Facts.bSkyLight = true;
				}
				continue;
			}

			UMeshComponent* Mesh = Cast<UMeshComponent>(Component);
			if (!Mesh)
			{
				continue;
			}

			if (const UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(Mesh))
			{
				++Facts.StaticMeshComponents;

				const UInstancedStaticMeshComponent* Instanced = Cast<UInstancedStaticMeshComponent>(Static);
				const int32 Instances = Instanced ? Instanced->GetInstanceCount() : 1;
				if (Instanced)
				{
					Facts.StaticMeshInstances += Instances;
				}

				UStaticMesh* Asset = Static->GetStaticMesh();
				if (Asset)
				{
					Collected.Meshes.AddUnique(Asset);
					Collected.MeshUses.Emplace(Asset, Instances);
					Roots.Note(Asset);
				}
			}
			else if (const USkeletalMeshComponent* Skeletal = Cast<USkeletalMeshComponent>(Mesh))
			{
				if (USkeletalMesh* Asset = Skeletal->GetSkeletalMeshAsset())
				{
					Collected.SkeletalUses.Add(Asset);
					Roots.Note(Asset);
				}
			}

			CollectTextures(Actor, Mesh, Collected, &Roots);
		}
	}

	/** The camera choice needs the actors themselves, which the handles in the facts do not carry. */
	struct FCameraPick
	{
		TArray<const AActor*> Actors;
		const AActor* Tagged = nullptr;
	};

	void CollectPlacedActor(AActor* Actor, FCPM_StageFacts& Facts, FCameraPick& Cameras)
	{
		// Cine cameras derive from ACameraActor, so this one test is both.
		if (const ACameraActor* Camera = Cast<ACameraActor>(Actor))
		{
			Facts.Cameras.Add(Handle(Actor));
			Cameras.Actors.Add(Actor);

			// Disabled reads as INDEX_NONE; 0 is Player 0, which does take the view.
			if (Camera->GetAutoActivatePlayerIndex() != INDEX_NONE)
			{
				Facts.AutoActivatingCameras.Add(Handle(Actor));
			}

			TInlineComponentArray<UCameraComponent*> Lenses(Actor);
			for (const UCameraComponent* Lens : Lenses)
			{
				if (Lens && Lens->bConstrainAspectRatio != 0)
				{
					Facts.AspectConstrainedCameras.Add(Handle(Actor));
					break;
				}
			}

			if (Actor->ActorHasTag(FName(TEXT("Convai.Stage.Camera"))))
			{
				Cameras.Tagged = Actor;
			}
		}

		if (Actor->IsA<ANavMeshBoundsVolume>())
		{
			Facts.bNavMeshBounds = true;
		}
		if (Actor->IsA<ASkyAtmosphere>())
		{
			Facts.bSkyAtmosphere = true;
		}
		if (Actor->IsA<AExponentialHeightFog>())
		{
			Facts.bHeightFog = true;
		}
		if (Actor->IsA<AVolumetricCloud>())
		{
			Facts.bVolumetricCloud = true;
		}

		// The avatar is already out of this walk, so every pawn left is one the studio has no player for.
		if (Actor->IsA<APlayerStart>() || Actor->IsA<APawn>())
		{
			Facts.Unsupported.Add(Label(Actor));
		}

		TInlineComponentArray<UConvaiObjectComponent*> Objects(Actor);
		for (const UConvaiObjectComponent* Object : Objects)
		{
			// Not HasValidObjectName: the runtime normalises the name before its own empty check, so a
			// whitespace-only name passes there and is refused at runtime.
			if (Object && Object->bConvaiEnabled &&
				!FConvaiObjectEntry::NormalizeMovementPointName(Object->ObjectEntry.Name).IsEmpty())
			{
				++Facts.ConvaiObjects;
				break;
			}
		}
	}

	void FindAvatarReferencers(
		const TArray<AActor*>& Placed, AActor* LevelScript, const TSet<const AActor*>& Avatars, FCPM_StageFacts& Facts)
	{
		if (Avatars.IsEmpty())
		{
			return;
		}

		// Hard references only - a soft or weak one is invisible to the finder, which is the documented
		// V0 limitation rather than a gap here.
		TArray<AActor*> Candidates = Placed;
		if (LevelScript)
		{
			Candidates.Add(LevelScript);
		}

		for (AActor* Actor : Candidates)
		{
			// A finder per candidate: its dedup set is private and outlives any reset of the out-array,
			// so a shared finder reports the avatar to the first referencer and to nobody after it.
			// ponytail: one finder pass per actor and per component, linear in the stage; fine for tens
			// of actors, batch it if a stage of thousands ever shows up.
			TArray<UObject*> Found;
			FReferenceFinder Finder(Found, nullptr, false, false, false, false);
			Finder.FindReferences(Actor);

			TInlineComponentArray<UActorComponent*> Components(Actor);
			for (UActorComponent* Component : Components)
			{
				if (Component)
				{
					Finder.FindReferences(Component);
				}
			}

			for (const UObject* Object : Found)
			{
				if (Avatars.Contains(Cast<AActor>(Object)))
				{
					Facts.AvatarReferencers.Add(Handle(Actor));
					break;
				}
			}
		}
	}

	bool HasAuthoredLevelBlueprint(ULevel* Level)
	{
		// bDontCreate: the default builds the blueprint and dirties the level, and a scan that dirtied
		// the level would make its own bDirty fact true.
		ULevelScriptBlueprint* Blueprint = Level->GetLevelScriptBlueprint(/*bDontCreate=*/true);
		if (!Blueprint)
		{
			return false;
		}

		TArray<TObjectPtr<UEdGraph>> Graphs = Blueprint->UbergraphPages;
		Graphs.Append(Blueprint->FunctionGraphs);

		for (const UEdGraph* Graph : Graphs)
		{
			if (!Graph)
			{
				continue;
			}
			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				// An untouched level blueprint still holds the events the editor placed in it, so what
				// counts is a node the creator put there.
				if (Node && !Node->IsAutomaticallyPlacedGhostNode())
				{
					return true;
				}
			}
		}
		return false;
	}

	bool HasUnsavedChanges(ULevel* Level, UPackage* WorldPackage)
	{
		// Only this level's own packages: another map left dirty in the editor is not this stage's
		// business, and an OFPA level dirties its external actor packages rather than the map.
		TSet<UPackage*> Own;
		Own.Add(WorldPackage);
		if (Level->MapBuildData)
		{
			Own.Add(Level->MapBuildData->GetOutermost());
		}
		Own.Append(Level->GetLoadedExternalObjectPackages());

		TArray<UPackage*> Dirty;
		FEditorFileUtils::GetDirtyWorldPackages(Dirty);
		for (UPackage* Package : Dirty)
		{
			if (Own.Contains(Package))
			{
				return true;
			}
		}
		return false;
	}

	void MeasureGeometry(FCollected& Collected, FCPM_StageFacts& Facts)
	{
		// GetNumTriangles finishes the mesh's compilation itself, one mesh at a time; finishing the set
		// first makes that one wait. Never FinishAllCompilation - this runs on a tick.
		FStaticMeshCompilingManager::Get().FinishCompilation(Collected.Meshes);

		for (const TPair<UStaticMesh*, int32>& Use : Collected.MeshUses)
		{
			Facts.Triangles += Use.Key->GetNumTriangles(0) * Use.Value;
		}

		for (const USkeletalMesh* Mesh : Collected.SkeletalUses)
		{
			if (const FSkeletalMeshRenderData* Data = Mesh->GetResourceForRendering())
			{
				if (Data->LODRenderData.IsValidIndex(0))
				{
					Facts.Triangles += Data->LODRenderData[0].GetTotalFaces();
				}
			}
		}
	}

	void MeasureTextures(const FCollected& Collected, FCPM_StageFacts& Facts)
	{
		TArray<UTexture*> Textures = Collected.Textures.Array();
		// A size or format read off a still-compiling texture is the placeholder's, not the texture's.
		// Only the stage's are waited on: the avatar's are never measured, so compiling them would be
		// a stall bought for a number nothing reads.
		FTextureCompilingManager::Get().FinishCompilation(Textures);

		// Shared textures count once, which is what the Pak carries.
		Facts.TexturesCount = Collected.Textures.Union(Collected.AvatarTextures).Num();

		// The size the cook produces, not the one the artist authored: a 4K source under a 1024 maximum
		// ships as a 1024, and that is the number the limit is about.
		ITargetPlatformManagerModule* Platforms = GetTargetPlatformManager();
		const ITargetPlatform* Windows = Platforms ? Platforms->FindTargetPlatform(TEXT("Windows")) : nullptr;

		SIZE_T Bytes = 0;
		for (UTexture* Texture : Textures)
		{
			int32 SizeX = 0;
			int32 SizeY = 0;
			if (Windows)
			{
				Texture->GetBuiltTextureSize(Windows, SizeX, SizeY);
			}

			UTexture2D* Flat = Cast<UTexture2D>(Texture);
			// Zeros are how an unknown built size shows up, the call having nothing to return.
			if (Flat && (SizeX == 0 || SizeY == 0))
			{
				SizeX = Flat->GetSizeX();
				SizeY = Flat->GetSizeY();
			}

			const int32 Edge = FMath::Max(SizeX, SizeY);
			if (Edge > Facts.TexturesMaxBuiltPx)
			{
				Facts.TexturesMaxBuiltPx = Edge;
				Facts.LargestTexture = FSoftObjectPath(Texture);
				const FString* Context = Collected.Context.Find(Texture);
				Facts.LargestTextureContext = Context ? *Context : FString();
			}

			if (Flat)
			{
				Bytes += CalcTextureSize(SizeX, SizeY, Flat->GetPixelFormat(), Flat->GetNumMips());
			}
		}

		Facts.TexturesMemoryMb = static_cast<double>(Bytes) / (1024.0 * 1024.0);
	}
}

FCPM_StageFacts Inspect(UWorld* World, UClass* AvatarClass)
{
	FCPM_StageFacts Facts;
	if (!World || !World->PersistentLevel)
	{
		return Facts;
	}

	ULevel* Level = World->PersistentLevel;
	UPackage* WorldPackage = World->GetOutermost();

	Facts.SourceLevel = WorldPackage->GetName();
	Facts.bPartitioned = World->IsPartitionedWorld();
	Facts.Sublevels = World->GetStreamingLevels().Num();
	Facts.bDirty = HasUnsavedChanges(Level, WorldPackage);

	if (AvatarClass)
	{
		// The creator named the blueprint, not the class the compiler generated from it.
		FString Name = AvatarClass->GetName();
		Name.RemoveFromEnd(TEXT("_C"));
		Facts.AvatarName = MoveTemp(Name);
	}

	TSet<const AActor*> Avatars;
	TArray<AActor*> AvatarActors;
	for (AActor* Actor : Level->Actors)
	{
		if (IsValid(Actor) && AvatarClass && Actor->GetClass()->IsChildOf(AvatarClass))
		{
			Avatars.Add(Actor);
			AvatarActors.Add(Actor);
			Facts.AvatarActorNames.Add(Actor->GetFName());
		}
	}

	// One placed avatar is the only case with a spawn point to record; how many there should be is
	// Evaluate's to say.
	if (AvatarActors.Num() == 1)
	{
		const FTransform& Transform = AvatarActors[0]->GetActorTransform();
		Facts.AvatarLocation = Transform.GetLocation();
		Facts.AvatarRotation = Transform.Rotator();
		Facts.AvatarScale = Transform.GetScale3D();
	}

	FCollected Collected;
	FRootCheck Roots{ Facts.SourceLevel, ShippedRoots(), Facts.OutsideRoots };
	FCameraPick Cameras;
	TArray<AActor*> Placed;
	const ABrush* DefaultBrush = Level->GetDefaultBrush();

	for (AActor* Actor : Level->Actors)
	{
		// The level script actor is read for references below and counted as an actor nowhere.
		if (!IsValid(Actor) || Avatars.Contains(Actor) || Actor->IsEditorOnly() || Actor == DefaultBrush ||
			Actor->IsA<AWorldSettings>() || Actor->IsA<ALevelScriptActor>() ||
			ChainReachesAvatar(Actor, Avatars, /*bAttachment=*/false))
		{
			continue;
		}

		// A child actor was spawned by its parent's component rather than placed, so it is none of the
		// stage's actors - but it renders, so its components are still the stage's cost.
		if (!Actor->IsChildActor())
		{
			++Facts.Actors;
			Placed.Add(Actor);
			Roots.Note(Actor->GetClass());
			CollectPlacedActor(Actor, Facts, Cameras);

			// Attached and still counted: it is placed in the level, and it is only its place in the
			// Outliner that the copy cannot keep.
			if (ChainReachesAvatar(Actor, Avatars, /*bAttachment=*/true))
			{
				Facts.AttachedToAvatar.Add(Handle(Actor));
			}
		}

		CollectComponents(Actor, Facts, Collected, Roots);
	}

	if (Cameras.Actors.Num() == 1)
	{
		Facts.CameraActorName = Cameras.Actors[0]->GetFName();
	}
	else if (Cameras.Tagged)
	{
		Facts.CameraActorName = Cameras.Tagged->GetFName();
	}

	// bChecked defaults to true and asserts; nothing a scan reads is worth a crash.
	if (const AWorldSettings* Settings = World->GetWorldSettings(/*bCheckStreamingPersistent=*/false, /*bChecked=*/false))
	{
		if (Settings->DefaultGameMode)
		{
			Facts.Unsupported.Add(FString::Printf(TEXT("Game mode %s"), *Settings->DefaultGameMode->GetName()));
		}
	}

	Facts.bHasLevelBlueprint = HasAuthoredLevelBlueprint(Level);
	FindAvatarReferencers(Placed, Level->GetLevelScriptActor(), Avatars, Facts);

	// The avatar's textures are the one fact it counts in: the studio spawns its own copy, but the Pak
	// still carries them.
	for (AActor* Avatar : AvatarActors)
	{
		TInlineComponentArray<UMeshComponent*> Meshes(Avatar);
		for (const UMeshComponent* Mesh : Meshes)
		{
			if (Mesh && !Mesh->IsEditorOnly() && !Mesh->IsVisualizationComponent())
			{
				CollectTextures(Avatar, Mesh, Collected, /*Roots=*/nullptr);
			}
		}
	}

	MeasureGeometry(Collected, Facts);
	MeasureTextures(Collected, Facts);

	return Facts;
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
