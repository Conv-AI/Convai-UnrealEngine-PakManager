// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Avatar/CPM_AvatarBlueprint.h"
#include "Components/SkeletalMeshComponent.h"
#include "ConvaiChatbotComponent.h"
#include "ConvaiFaceSync.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

using namespace ConvaiPakManager::Avatar;

namespace
{
	/**
	 * A blueprint in the transient package, held against garbage collection.
	 *
	 * Fixtures are built in code rather than checked in as .uasset content: what these rules act on
	 * is an SCS, and an SCS assembled here says in the test itself what shape is being exercised.
	 * Transient is also what proves PrepareAvatarBlueprint never saves - a transient package cannot
	 * be written, so a save attempt would fail the test rather than pass unnoticed.
	 */
	struct FScratchBlueprint
	{
		UBlueprint* Blueprint = nullptr;

		explicit FScratchBlueprint(UClass* ParentClass = AActor::StaticClass())
		{
			Blueprint = FKismetEditorUtilities::CreateBlueprint(
				ParentClass, GetTransientPackage(),
				MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("BP_CPMTest")),
				BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
			if (Blueprint)
			{
				// CompileBlueprint can collect garbage, and nothing else references a transient asset.
				Blueprint->AddToRoot();
			}
		}

		~FScratchBlueprint()
		{
			if (Blueprint)
			{
				Blueprint->RemoveFromRoot();
			}
		}

		USimpleConstructionScript* SCS() const { return Blueprint->SimpleConstructionScript; }

		USCS_Node* AddNode(UClass* ComponentClass, const TCHAR* Name) const
		{
			USCS_Node* Node = SCS()->CreateNode(ComponentClass, FName(Name));
			SCS()->AddNode(Node);
			return Node;
		}

		/** A skeletal mesh node whose skeleton is named `SkeletonName`, so its path carries that name. */
		USCS_Node* AddSkeletalNode(const TCHAR* Name, const TCHAR* SkeletonName) const
		{
			USCS_Node* Node = AddNode(USkeletalMeshComponent::StaticClass(), Name);
			USkeleton* Skeleton = NewObject<USkeleton>(
				GetTransientPackage(),
				MakeUniqueObjectName(GetTransientPackage(), USkeleton::StaticClass(), SkeletonName));
			USkeletalMesh* Mesh = NewObject<USkeletalMesh>(GetTransientPackage());
			Mesh->SetSkeleton(Skeleton);
			Cast<USkeletalMeshComponent>(Node->ComponentTemplate)->SetSkeletalMeshAsset(Mesh);
			return Node;
		}

		/** Nodes survive a compile but their identity is the variable name, so look them up by it. */
		USkeletalMeshComponent* FindMesh(const TCHAR* Name) const
		{
			for (USCS_Node* Node : SCS()->GetAllNodes())
			{
				if (Node && Node->GetVariableName() == FName(Name))
				{
					return Cast<USkeletalMeshComponent>(Node->ComponentTemplate);
				}
			}
			return nullptr;
		}

		USCS_Node* FindNodeOf(const UClass* ComponentClass) const
		{
			for (USCS_Node* Node : SCS()->GetAllNodes())
			{
				if (Node && Node->ComponentClass && Node->ComponentClass->IsChildOf(ComponentClass))
				{
					return Node;
				}
			}
			return nullptr;
		}

		int32 CountNodesOf(const UClass* ComponentClass) const
		{
			int32 Count = 0;
			for (const USCS_Node* Node : SCS()->GetAllNodes())
			{
				if (Node && Node->ComponentClass && Node->ComponentClass->IsChildOf(ComponentClass))
				{
					++Count;
				}
			}
			return Count;
		}

		int32 NodeCount() const { return SCS()->GetAllNodes().Num(); }
	};

	/** The same class PrepareAvatarBlueprint adds, resolved the same way, so the test cannot drift. */
	UClass* ChatbotBPClass()
	{
		FString Root = TEXT("/ConvAI/");
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ConvAI")))
		{
			Root = Plugin->GetMountedAssetPath();
		}
		const FString Path = Root + TEXT("ConvaiConveniencePack/ConvaiBPComponent/BP_ConvaiChatbotComponent.BP_ConvaiChatbotComponent");
		const UBlueprint* Blueprint = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *Path));
		return Blueprint ? Blueprint->GeneratedClass : nullptr;
	}

	/** A stand-in for the MetaHuman importer's Face_AnimBP_C, which this project has no copy of. */
	UClass* ImporterFaceAnimClass()
	{
		const TCHAR* Name = TEXT("Face_AnimBP_C");
		if (UClass* Existing = FindObject<UClass>(GetTransientPackage(), Name))
		{
			return Existing;
		}
		UBlueprintGeneratedClass* StandIn = NewObject<UBlueprintGeneratedClass>(GetTransientPackage(), Name);
		StandIn->SetSuperStruct(UAnimInstance::StaticClass());
		StandIn->Bind();
		StandIn->StaticLink(true);
		return StandIn;
	}

	bool ChangesMention(const TArray<FString>& Changes, const TCHAR* Fragment)
	{
		return Changes.ContainsByPredicate([Fragment](const FString& Change) { return Change.Contains(Fragment); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMAvatarAddsBothConvaiComponentsOnce,
	"ConvaiPakManager.Avatar.AddsBothConvaiComponentsOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMAvatarAddsBothConvaiComponentsOnce::RunTest(const FString&)
{
	UClass* BPChatbot = ChatbotBPClass();
	if (!TestNotNull(TEXT("BP_ConvaiChatbotComponent is mounted"), BPChatbot))
	{
		return false;
	}

	FScratchBlueprint Scratch;
	FString Error;
	TArray<FString> Changes;

	TestTrue(TEXT("a bare Actor blueprint is prepared"), PrepareAvatarBlueprint(Scratch.Blueprint, Error, Changes));
	TestEqual(TEXT("both components are reported"), Changes.Num(), 2);
	TestEqual(TEXT("one BP chatbot component"), Scratch.CountNodesOf(BPChatbot), 1);
	TestEqual(TEXT("one face sync component"), Scratch.CountNodesOf(UConvaiFaceSyncComponent::StaticClass()), 1);

	const int32 NodesAfterFirst = Scratch.NodeCount();
	TArray<FString> SecondChanges;
	TestTrue(TEXT("a second pass still succeeds"), PrepareAvatarBlueprint(Scratch.Blueprint, Error, SecondChanges));
	TestEqual(TEXT("and changes nothing"), SecondChanges.Num(), 0);
	TestEqual(TEXT("adding no node"), Scratch.NodeCount(), NodesAfterFirst);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMAvatarAttachesUnderTheExistingRoot,
	"ConvaiPakManager.Avatar.AttachesUnderTheExistingRoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMAvatarAttachesUnderTheExistingRoot::RunTest(const FString&)
{
	UClass* BPChatbot = ChatbotBPClass();
	if (!TestNotNull(TEXT("BP_ConvaiChatbotComponent is mounted"), BPChatbot))
	{
		return false;
	}

	// The shape a creator hands us: DefaultSceneRoot with their mesh under it. Both Convai
	// components are scene components, so adding either as a second root node would make it the
	// actor root and take the creator's hierarchy with it.
	FScratchBlueprint Scratch;
	USCS_Node* DefaultRoot = Scratch.SCS()->GetDefaultSceneRootNode();
	if (!TestNotNull(TEXT("the fixture is rooted on DefaultSceneRoot"), DefaultRoot))
	{
		return false;
	}
	DefaultRoot->AddChildNode(Scratch.SCS()->CreateNode(USkeletalMeshComponent::StaticClass(), TEXT("Body")));

	FString Error;
	TArray<FString> Changes;
	TestTrue(TEXT("prepared"), PrepareAvatarBlueprint(Scratch.Blueprint, Error, Changes));

	USCS_Node* Root = Scratch.SCS()->GetDefaultSceneRootNode();
	TestTrue(TEXT("DefaultSceneRoot is still a root node"), Scratch.SCS()->GetRootNodes().Contains(Root));
	TestTrue(TEXT("the creator's mesh is still under it"),
		Scratch.SCS()->FindParentNode(Scratch.FindNodeOf(USkeletalMeshComponent::StaticClass())) == Root);
	TestTrue(TEXT("the chatbot hangs off it"),
		Scratch.SCS()->FindParentNode(Scratch.FindNodeOf(BPChatbot)) == Root);
	TestTrue(TEXT("and so does face sync"),
		Scratch.SCS()->FindParentNode(Scratch.FindNodeOf(UConvaiFaceSyncComponent::StaticClass())) == Root);

	// A native root (ACharacter's capsule) is no SCS node, so there is nothing to attach to here:
	// new nodes are root nodes, and the SCS attaches those to the inherited root at construction.
	FScratchBlueprint Character(ACharacter::StaticClass());
	TArray<FString> CharacterChanges;
	TestTrue(TEXT("a Character blueprint is prepared"), PrepareAvatarBlueprint(Character.Blueprint, Error, CharacterChanges));
	USCS_Node* CharacterChatbot = Character.FindNodeOf(BPChatbot);
	if (TestNotNull(TEXT("the chatbot was added"), CharacterChatbot))
	{
		TestTrue(TEXT("as a root node, to attach to the native root"),
			Character.SCS()->GetRootNodes().Contains(CharacterChatbot));
	}
	TestFalse(TEXT("no DefaultSceneRoot is in play on a natively rooted actor"),
		Character.SCS()->GetRootNodes().Contains(Character.SCS()->GetDefaultSceneRootNode()));

	// A child of an Actor-based blueprint inherits a root that is a node in the *parent's* SCS.
	// Making our components children of it would file them in a blueprint the creator never picked.
	FScratchBlueprint Parent;
	FKismetEditorUtilities::CompileBlueprint(Parent.Blueprint);
	const int32 ParentNodesBefore = Parent.NodeCount();

	FScratchBlueprint Child(Parent.Blueprint->GeneratedClass);
	TArray<FString> ChildChanges;
	TestTrue(TEXT("a child blueprint is prepared"), PrepareAvatarBlueprint(Child.Blueprint, Error, ChildChanges));
	USCS_Node* ChildChatbot = Child.FindNodeOf(BPChatbot);
	if (TestNotNull(TEXT("the chatbot landed in the child's own SCS"), ChildChatbot))
	{
		TestTrue(TEXT("as a root node, to attach to the inherited root"),
			Child.SCS()->GetRootNodes().Contains(ChildChatbot));
	}
	TestEqual(TEXT("and the parent blueprint is untouched"), Parent.NodeCount(), ParentNodesBefore);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMAvatarRefusesRawChatbotComponent,
	"ConvaiPakManager.Avatar.RefusesRawChatbotComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMAvatarRefusesRawChatbotComponent::RunTest(const FString&)
{
	FScratchBlueprint Scratch;
	Scratch.AddNode(UConvaiChatbotComponent::StaticClass(), TEXT("Chatbot"));
	const int32 NodesBefore = Scratch.NodeCount();

	FString Error;
	TArray<FString> Changes;
	TestFalse(TEXT("the raw C++ component is refused"), PrepareAvatarBlueprint(Scratch.Blueprint, Error, Changes));
	TestTrue(TEXT("and the message names the replacement"), Error.Contains(TEXT("BP_ConvaiChatbotComponent")));
	TestEqual(TEXT("a refusal changes nothing, not even face sync"), Scratch.NodeCount(), NodesBefore);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMAvatarKeepsAnExistingChatbotComponent,
	"ConvaiPakManager.Avatar.KeepsAnExistingChatbotComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMAvatarKeepsAnExistingChatbotComponent::RunTest(const FString&)
{
	UClass* BPChatbot = ChatbotBPClass();
	if (!TestNotNull(TEXT("BP_ConvaiChatbotComponent is mounted"), BPChatbot))
	{
		return false;
	}

	FScratchBlueprint Scratch;
	Scratch.AddNode(BPChatbot, TEXT("MyChatbot"));

	FString Error;
	TArray<FString> Changes;
	TestTrue(TEXT("prepared"), PrepareAvatarBlueprint(Scratch.Blueprint, Error, Changes));
	TestEqual(TEXT("only face sync is added"), Changes.Num(), 1);
	TestTrue(TEXT("and it is face sync that was added"), ChangesMention(Changes, TEXT("FaceSync")));
	TestEqual(TEXT("the creator's chatbot component is not duplicated"), Scratch.CountNodesOf(BPChatbot), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMAvatarCountsInheritedComponents,
	"ConvaiPakManager.Avatar.CountsInheritedComponents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMAvatarCountsInheritedComponents::RunTest(const FString&)
{
	FScratchBlueprint Parent;
	FString Error;
	TArray<FString> Changes;
	TestTrue(TEXT("the parent is prepared"), PrepareAvatarBlueprint(Parent.Blueprint, Error, Changes));
	UClass* ParentClass = Parent.Blueprint->GeneratedClass;
	if (!TestNotNull(TEXT("the parent compiled to a class"), ParentClass))
	{
		return false;
	}

	FScratchBlueprint Child(ParentClass);
	TArray<FString> ChildChanges;
	TestTrue(TEXT("the child is prepared"), PrepareAvatarBlueprint(Child.Blueprint, Error, ChildChanges));
	TestEqual(TEXT("inherited components count as present"), ChildChanges.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMAvatarDetectsMetaHumanBySkeleton,
	"ConvaiPakManager.Avatar.DetectsMetaHumanBySkeleton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMAvatarDetectsMetaHumanBySkeleton::RunTest(const FString&)
{
	TestFalse(TEXT("a null blueprint is not a MetaHuman"), IsMetaHuman(nullptr));

	FScratchBlueprint Bare;
	TestFalse(TEXT("no skeletal mesh, no MetaHuman"), IsMetaHuman(Bare.Blueprint));

	FScratchBlueprint MetaHuman;
	MetaHuman.AddSkeletalNode(TEXT("Body"), TEXT("metahuman_base_skel"));
	TestTrue(TEXT("a body mesh on a MetaHuman skeleton"), IsMetaHuman(MetaHuman.Blueprint));

	FScratchBlueprint Mannequin;
	Mannequin.AddSkeletalNode(TEXT("Body"), TEXT("ue4_mannequin_skel"));
	TestFalse(TEXT("another skeleton is not a MetaHuman"), IsMetaHuman(Mannequin.Blueprint));

	FScratchBlueprint WrongName;
	WrongName.AddSkeletalNode(TEXT("Torso"), TEXT("metahuman_base_skel"));
	TestFalse(TEXT("the node must be the body"), IsMetaHuman(WrongName.Blueprint));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMAvatarWiresMetaHumanAnimBlueprints,
	"ConvaiPakManager.Avatar.WiresMetaHumanAnimBlueprints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMAvatarWiresMetaHumanAnimBlueprints::RunTest(const FString&)
{
	FScratchBlueprint MetaHuman;
	MetaHuman.AddSkeletalNode(TEXT("Body"), TEXT("metahuman_base_skel"));
	MetaHuman.AddSkeletalNode(TEXT("Face"), TEXT("metahuman_base_skel"));

	FString Error;
	TArray<FString> Changes;
	TestTrue(TEXT("a MetaHuman is prepared"), PrepareAvatarBlueprint(MetaHuman.Blueprint, Error, Changes));

	const USkeletalMeshComponent* Body = MetaHuman.FindMesh(TEXT("Body"));
	const USkeletalMeshComponent* Face = MetaHuman.FindMesh(TEXT("Face"));
	if (!TestNotNull(TEXT("body mesh survived the compile"), Body) || !TestNotNull(TEXT("face mesh survived the compile"), Face))
	{
		return false;
	}
	TestEqual(TEXT("Convai's body anim BP is wired"),
		Body->AnimClass ? Body->AnimClass->GetName() : FString(), FString(TEXT("Convai_MetaHuman_BodyAnim_C")));
	TestEqual(TEXT("Convai's face anim BP is wired"),
		Face->AnimClass ? Face->AnimClass->GetName() : FString(), FString(TEXT("Convai_MetaHuman_FaceAnim_C")));
	TestTrue(TEXT("both assignments are reported"),
		ChangesMention(Changes, TEXT("Convai_MetaHuman_BodyAnim_C")) && ChangesMention(Changes, TEXT("Convai_MetaHuman_FaceAnim_C")));

	// A creator's own anim blueprint outranks ours, on a MetaHuman as much as anywhere else.
	FScratchBlueprint Authored;
	Authored.AddSkeletalNode(TEXT("Body"), TEXT("metahuman_base_skel"));
	Authored.AddSkeletalNode(TEXT("Face"), TEXT("metahuman_base_skel"));
	Authored.FindMesh(TEXT("Body"))->SetAnimInstanceClass(UAnimInstance::StaticClass());
	Authored.FindMesh(TEXT("Face"))->SetAnimInstanceClass(UAnimInstance::StaticClass());
	TArray<FString> AuthoredChanges;
	TestTrue(TEXT("prepared"), PrepareAvatarBlueprint(Authored.Blueprint, Error, AuthoredChanges));
	TestEqual(TEXT("the creator's anim BP is kept"),
		Authored.FindMesh(TEXT("Body"))->AnimClass->GetName(), FString(TEXT("AnimInstance")));
	TestEqual(TEXT("on the face as well, where ours would otherwise land"),
		Authored.FindMesh(TEXT("Face"))->AnimClass->GetName(), FString(TEXT("AnimInstance")));

	// Face_AnimBP_C is what the MetaHuman importer leaves behind, and the one anim BP we replace.
	// The class does not exist in this project, so stand one in under exactly the name the rule reads.
	FScratchBlueprint Imported;
	Imported.AddSkeletalNode(TEXT("Body"), TEXT("metahuman_base_skel"));
	Imported.AddSkeletalNode(TEXT("Face"), TEXT("metahuman_base_skel"));
	Imported.FindMesh(TEXT("Face"))->SetAnimInstanceClass(ImporterFaceAnimClass());
	TArray<FString> ImportedChanges;
	TestTrue(TEXT("prepared"), PrepareAvatarBlueprint(Imported.Blueprint, Error, ImportedChanges));
	TestEqual(TEXT("the importer's face anim BP is replaced"),
		Imported.FindMesh(TEXT("Face"))->AnimClass->GetName(), FString(TEXT("Convai_MetaHuman_FaceAnim_C")));

	// And a non-MetaHuman gets no anim blueprint at all.
	FScratchBlueprint Mannequin;
	Mannequin.AddSkeletalNode(TEXT("Body"), TEXT("ue4_mannequin_skel"));
	TArray<FString> MannequinChanges;
	TestTrue(TEXT("prepared"), PrepareAvatarBlueprint(Mannequin.Blueprint, Error, MannequinChanges));
	TestNull(TEXT("no anim BP on a non-MetaHuman body"), Mannequin.FindMesh(TEXT("Body"))->AnimClass.Get());

	return true;
}

#endif // WITH_AUTOMATION_TESTS
