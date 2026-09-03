// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Avatar/CPM_AvatarBlueprint.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "ConvaiChatbotComponent.h"
#include "ConvaiFaceSync.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Utility/CPM_Log.h"

namespace
{
	/**
	 * Where the Convai SDK's content lives, e.g. `/ConvAI/`.
	 *
	 * Asked of the plugin rather than hardcoded because the SDK's own toolset hardcodes `/Convai/`
	 * and disagrees with the `.uplugin` it ships beside; package names are case-insensitive so both
	 * happen to load, which is exactly how a mismatch survives unnoticed.
	 */
	FString ConvaiContentRoot()
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ConvAI")))
		{
			return Plugin->GetMountedAssetPath();
		}
		return TEXT("/ConvAI/");
	}

	template <typename TBlueprint>
	UClass* LoadGeneratedClass(const FString& AssetPath)
	{
		if (const TBlueprint* Blueprint = Cast<TBlueprint>(StaticLoadObject(TBlueprint::StaticClass(), nullptr, *AssetPath)))
		{
			return Blueprint->GeneratedClass;
		}
		return LoadObject<UClass>(nullptr, *(AssetPath + TEXT("_C")));
	}

	bool SCSHasComponent(const USimpleConstructionScript* SCS, const UClass* ComponentClass)
	{
		if (!SCS)
		{
			return false;
		}
		for (const USCS_Node* Node : SCS->GetAllNodes())
		{
			if (Node && Node->ComponentClass && Node->ComponentClass->IsChildOf(ComponentClass))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Whether the blueprint carries a component of this class, however it got there.
	 *
	 * Three places have to be looked at, and only the first is obvious: a component a C++ parent
	 * declares never appears in any SCS, and a creator childing one of Convai's sample blueprints
	 * inherits theirs from a parent blueprint's SCS. Missing either would re-add a component that is
	 * already inherited, and the duplicate is what a creator would then have to debug.
	 */
	bool HasComponent(const UBlueprint* Blueprint, const UClass* ComponentClass)
	{
		if (!Blueprint || !ComponentClass)
		{
			return false;
		}
		if (SCSHasComponent(Blueprint->SimpleConstructionScript, ComponentClass))
		{
			return true;
		}

		UClass* Parent = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetSuperClass() : Blueprint->ParentClass.Get();
		while (Parent)
		{
			const UBlueprintGeneratedClass* ParentBPGC = Cast<UBlueprintGeneratedClass>(Parent);
			if (!ParentBPGC)
			{
				if (const AActor* CDO = Cast<AActor>(Parent->GetDefaultObject()))
				{
					for (const UActorComponent* Component : CDO->GetComponents())
					{
						if (Component && Component->IsA(ComponentClass))
						{
							return true;
						}
					}
				}
				break;
			}
			if (SCSHasComponent(ParentBPGC->SimpleConstructionScript, ComponentClass))
			{
				return true;
			}
			Parent = Parent->GetSuperClass();
		}
		return false;
	}

	USCS_Node* AddComponentNode(UBlueprint* Blueprint, UClass* ComponentClass, const TCHAR* NodeName)
	{
		USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
		SCS->Modify();

		USCS_Node* Node = SCS->CreateNode(ComponentClass, FName(NodeName));
		if (!Node)
		{
			return nullptr;
		}

		// Both Convai components are scene components (the chatbot derives from UAudioComponent), so
		// they have to go under the existing scene root. Added as a second root node instead,
		// AddNode's ValidateSceneRootNodes would promote ours to actor root, reparent the creator's
		// components under it and delete DefaultSceneRoot.
		//
		// GetSceneRootComponentTemplate never reports DefaultSceneRoot (SimpleConstructionScript.cpp,
		// the `RootNode != DefaultSceneRootNode` guard), so that case is picked up by hand.
		USCS_Node* RootNode = nullptr;
		SCS->GetSceneRootComponentTemplate(false, &RootNode);
		if (!RootNode && SCS->GetRootNodes().Contains(SCS->GetDefaultSceneRootNode()))
		{
			RootNode = SCS->GetDefaultSceneRootNode();
		}

		// The root node can belong to a parent blueprint's SCS - GetSceneRootComponentTemplate scans
		// the SCS stack parents-first and only excludes *this* SCS's DefaultSceneRoot. AddChildNode
		// files the node in whichever SCS owns the parent, so on an inherited root that would write
		// into a blueprint the creator never picked. Same guard the SCS editor uses
		// (SubobjectDataSubsystem.cpp, AttachSubobject); a root node attaches to the actor's root at
		// construction anyway, and SetParent just records the intent.
		if (RootNode && RootNode->GetSCS() == SCS)
		{
			RootNode->AddChildNode(Node);
		}
		else
		{
			SCS->AddNode(Node);
			if (RootNode)
			{
				Node->SetParent(RootNode);
			}
		}
		return Node;
	}

	/** Loaded on first need: a MetaHuman with only a body mesh must not fail over the face asset. */
	struct FLazyAnimClass
	{
		FString AssetPath;
		UClass* Class = nullptr;
		bool bTried = false;

		UClass* Get()
		{
			if (!bTried)
			{
				bTried = true;
				Class = LoadGeneratedClass<UAnimBlueprint>(AssetPath);
			}
			return Class;
		}
	};

	/**
	 * The Convai anim blueprint each MetaHuman mesh node wants, all resolved before any is assigned.
	 *
	 * Planning is separate from applying so a missing Convai animation asset refuses with the
	 * creator's blueprint untouched, like every other refusal here.
	 */
	bool PlanMetaHumanAnimBlueprints(UBlueprint* Blueprint, const FString& ContentRoot,
		TArray<TPair<USCS_Node*, UClass*>>& OutWanted, FString& OutError)
	{
		FLazyAnimClass BodyAnim{ ContentRoot + TEXT("MetaHumans/Animations/Convai_MetaHuman_BodyAnim.Convai_MetaHuman_BodyAnim") };
		FLazyAnimClass FaceAnim{ ContentRoot + TEXT("MetaHumans/Animations/Convai_MetaHuman_FaceAnim.Convai_MetaHuman_FaceAnim") };

		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			USkeletalMeshComponent* Mesh = Node ? Cast<USkeletalMeshComponent>(Node->ComponentTemplate) : nullptr;
			if (!Mesh)
			{
				continue;
			}

			const FString NodeName = Node->GetVariableName().ToString();
			UClass* const Existing = Mesh->AnimClass.Get();

			FLazyAnimClass* Wanted = nullptr;
			if (NodeName.Contains(TEXT("body")) && !Existing)
			{
				Wanted = &BodyAnim;
			}
			// Face_AnimBP_C is what the MetaHuman importer leaves behind, not something the creator
			// wrote, so it is the one anim blueprint here that may be replaced.
			else if (NodeName.Contains(TEXT("face"))
				&& (!Existing || Existing->GetName().Equals(TEXT("Face_AnimBP_C"), ESearchCase::CaseSensitive)))
			{
				Wanted = &FaceAnim;
			}
			if (!Wanted)
			{
				continue;
			}

			UClass* AnimClass = Wanted->Get();
			if (!AnimClass)
			{
				OutError = FString::Printf(
					TEXT("'%s' is a MetaHuman, but the Convai animation blueprint '%s' could not be loaded. ")
					TEXT("Reinstall the Convai plugin content and pick the asset again."),
					*Blueprint->GetName(), *Wanted->AssetPath);
				return false;
			}

			OutWanted.Emplace(Node, AnimClass);
		}
		return true;
	}
}

namespace ConvaiPakManager::Avatar
{
bool IsMetaHuman(const UBlueprint* Blueprint)
{
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		return false;
	}

	for (const USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (!Node || !Node->GetVariableName().ToString().Contains(TEXT("body")))
		{
			continue;
		}
		const USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(Node->ComponentTemplate);
		const USkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
		const USkeleton* Skeleton = MeshAsset ? MeshAsset->GetSkeleton() : nullptr;
		if (Skeleton && Skeleton->GetPathName().Contains(TEXT("metahuman")))
		{
			return true;
		}
	}
	return false;
}

bool PrepareAvatarBlueprint(UBlueprint* Blueprint, FString& OutError, TArray<FString>& OutChanges)
{
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		OutError = TEXT("The selected asset is not an Actor blueprint, so Convai's components cannot be added to it.");
		return false;
	}

	const FString ContentRoot = ConvaiContentRoot();
	UClass* ChatbotClass = LoadGeneratedClass<UBlueprint>(
		ContentRoot + TEXT("ConvaiConveniencePack/ConvaiBPComponent/BP_ConvaiChatbotComponent.BP_ConvaiChatbotComponent"));
	if (!ChatbotClass)
	{
		OutError = FString::Printf(
			TEXT("BP_ConvaiChatbotComponent could not be loaded from '%s'. The Convai plugin content is missing or not mounted."),
			*ContentRoot);
		return false;
	}

	// Everything that can refuse runs before anything is changed: a refusal has to leave the
	// creator's blueprint untouched.
	const bool bNeedsChatbot = !HasComponent(Blueprint, ChatbotClass);
	if (bNeedsChatbot && HasComponent(Blueprint, UConvaiChatbotComponent::StaticClass()))
	{
		OutError = FString::Printf(
			TEXT("%s uses the C++ Convai Chatbot Component; replace it with BP_ConvaiChatbotComponent, ")
			TEXT("which carries the action dispatch and movement helpers a published avatar needs."),
			*Blueprint->GetName());
		return false;
	}

	TArray<TPair<USCS_Node*, UClass*>> AnimAssignments;
	if (IsMetaHuman(Blueprint) && !PlanMetaHumanAnimBlueprints(Blueprint, ContentRoot, AnimAssignments, OutError))
	{
		return false;
	}

	if (bNeedsChatbot)
	{
		if (!AddComponentNode(Blueprint, ChatbotClass, TEXT("ConvaiChatbot")))
		{
			OutError = FString::Printf(TEXT("Could not add BP_ConvaiChatbotComponent to %s."), *Blueprint->GetName());
			return false;
		}
		OutChanges.Add(TEXT("added BP_ConvaiChatbotComponent"));
	}

	if (!HasComponent(Blueprint, UConvaiFaceSyncComponent::StaticClass()))
	{
		if (!AddComponentNode(Blueprint, UConvaiFaceSyncComponent::StaticClass(), TEXT("ConvaiFaceSync")))
		{
			OutError = FString::Printf(TEXT("Could not add ConvaiFaceSyncComponent to %s."), *Blueprint->GetName());
			return false;
		}
		OutChanges.Add(TEXT("added ConvaiFaceSyncComponent"));
	}

	for (const TPair<USCS_Node*, UClass*>& Assignment : AnimAssignments)
	{
		USkeletalMeshComponent* Mesh = CastChecked<USkeletalMeshComponent>(Assignment.Key->ComponentTemplate);
		Mesh->Modify();
		Mesh->SetAnimInstanceClass(Assignment.Value);
		OutChanges.Add(FString::Printf(TEXT("assigned %s to '%s'"),
			*Assignment.Value->GetName(), *Assignment.Key->GetVariableName().ToString()));
	}

	if (OutChanges.Num() > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		Blueprint->MarkPackageDirty();

		CPM_LOG(Log, TEXT("Prepared Avatar blueprint '%s': %s."),
			*Blueprint->GetName(), *FString::Join(OutChanges, TEXT(", ")));
	}
	return true;
}
}
