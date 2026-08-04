// Fill out your copyright notice in the Description page of Project Settings.


#include "ConvaiPakEditorSubsystem.h"

#include "CPM_PakManagerSettings.h"
#include "Chunk/CPM_Chunk.h"
#include "Core/WorkflowManagerSubsystem.h"
#include "EditorUtilityLibrary.h"
#include "Jobs/CPM_PublishJobs.h"
#include "Misc/FileHelper.h"
#include "Proxy/CPM_Proxy.h"
#include "Publish/CPM_PolicyRequest.h"
#include "Utility/CPM_Log.h"
#include "Utility/CPM_UtilityLibrary.h"

void UConvaiPakEditorSubsystem::Deinitialize()
{
	// Workflows outlive this subsystem otherwise, and their jobs hold raw callbacks back into it.
	for (const TPair<int32, FWorkflowHandle>& Active : ActivePublishes)
	{
		if (UWorkflowManagerSubsystem* Manager = UWorkflowManagerSubsystem::Get())
		{
			Manager->ICancelWorkflow(Active.Value, /*bForce=*/true);
		}
	}
	ActivePublishes.Reset();

	Super::Deinitialize();
}

void UConvaiPakEditorSubsystem::GetSelectedAssetPackageName(FString& PackageName)
{
	TArray<FAssetData> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssetData();
	if (SelectedAssets.Num() > 0)
	{
		PackageName = SelectedAssets[0].PackageName.ToString();
	}
}

TArray<int32> UConvaiPakEditorSubsystem::GetChunkIds() const
{
	TArray<int32> Ids;
	for (const FCPM_Chunk& Chunk : ConvaiPakManager::Chunk::Discover())
	{
		Ids.Add(Chunk.Id);
	}
	return Ids;
}

FCPM_ChunkStatus UConvaiPakEditorSubsystem::GetChunkStatus(const int32 ChunkId) const
{
	if (const FCPM_ChunkStatus* Status = StatusByChunk.Find(ChunkId))
	{
		return *Status;
	}

	FCPM_ChunkStatus Idle;
	Idle.ChunkId = ChunkId;
	return Idle;
}

FString UConvaiPakEditorSubsystem::GetAssetId(const int32 ChunkId) const
{
	FString AssetId;
	FString Contents;
	if (!FFileHelper::LoadFileToString(Contents, *ConvaiPakManager::Chunk::GetCreateAssetDataPath(ChunkId)))
	{
		return AssetId;
	}

	FCPM_CreatedAssets Created;
	if (UCPM_UtilityLibrary::GetCreatedAssetsFromJSON(Contents, Created) && !Created.Assets.IsEmpty())
	{
		AssetId = Created.Assets[0].Asset.AssetId;
	}
	return AssetId;
}

bool UConvaiPakEditorSubsystem::CanAddAnotherChunk() const
{
	return !UCPM_PakManagerSettings::Get().IsAtChunkLimit(GetChunkIds().Num());
}

void UConvaiPakEditorSubsystem::SetStatus(
	const int32 ChunkId,
	const ECPM_AssetManagerStatus Status,
	const FString& Message,
	const float Progress,
	const FString& StepName)
{
	FCPM_ChunkStatus& Stored = StatusByChunk.FindOrAdd(ChunkId);
	Stored.ChunkId = ChunkId;
	Stored.Status = Status;
	Stored.Message = Message;
	Stored.Progress = Progress;
	Stored.StepName = StepName;

	OnChunkStatusChanged.Broadcast(Stored);
	OnChunkStatusChangedEvent.Broadcast(Stored);
}

void UConvaiPakEditorSubsystem::ResolvePolicy(
	const int32 ChunkId,
	TFunction<void(bool, const FCPM_PublishPolicy&, const FString&)> OnResolved)
{
	const UCPM_PakManagerSettings& Settings = UCPM_PakManagerSettings::Get();

	if (!Settings.PolicyOverrideFile.IsEmpty())
	{
		FString Contents;
		if (!FFileHelper::LoadFileToString(Contents, *Settings.PolicyOverrideFile))
		{
			OnResolved(false, FCPM_PublishPolicy(),
				FString::Printf(TEXT("could not read the publish policy override at %s"), *Settings.PolicyOverrideFile));
			return;
		}

		FCPM_PublishPolicy Policy;
		FString Error;
		const bool bParsed = Policy.ParseFromJson(Contents, Error);
		OnResolved(bParsed, Policy, Error);
		return;
	}

	UCPM_PolicyRequest::Start(
		Settings.PolicyRepository, Settings.PolicyRef, Settings.PolicyPath,
		UCPM_PolicyRequest::FOnPolicyFetched::CreateLambda(
			[OnResolved](const bool bSucceeded, const FString& Contents)
			{
				if (!bSucceeded)
				{
					// Refused rather than fallen back to a cached or compiled-in policy. The policy
					// exists so Convai can change what a Publish produces, so a stale copy is wrong
					// exactly when it matters - and publishing from one yields an Asset missing a
					// Version, which nothing here would notice. See docs/adr/0004.
					OnResolved(false, FCPM_PublishPolicy(), TEXT("the publish policy could not be fetched"));
					return;
				}

				FCPM_PublishPolicy Policy;
				FString Error;
				const bool bParsed = Policy.ParseFromJson(Contents, Error);
				OnResolved(bParsed, Policy, Error);
			}));
}

FWorkflowHandle UConvaiPakEditorSubsystem::Publish(const int32 ChunkId)
{
	if (!GetChunkIds().Contains(ChunkId))
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Create_Failed,
			FString::Printf(TEXT("no Primary Asset Label in this project declares chunk %d"), ChunkId));
		return FWorkflowHandle::Invalid();
	}

	if (ActivePublishes.Contains(ChunkId))
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Create_Failed, TEXT("this chunk is already publishing"));
		return FWorkflowHandle::Invalid();
	}

	SetStatus(ChunkId, ECPM_AssetManagerStatus::Packaging_Begin, FString(), 0.0f, TEXT("Reading publish policy"));

	// Deliberately NOT a Job of the publish queue. The policy decides which Jobs exist, and a queue's
	// shape may depend only on what the caller knew before building it - so it is resolved first and
	// the queue is built from the answer. See docs/adr/0004 and the Job System's docs/adr/0009.
	TWeakObjectPtr<UConvaiPakEditorSubsystem> WeakThis(this);
	ResolvePolicy(ChunkId, [WeakThis, ChunkId](const bool bSucceeded, const FCPM_PublishPolicy& Policy, const FString& Error)
	{
		UConvaiPakEditorSubsystem* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}

		if (!bSucceeded)
		{
			Self->SetStatus(ChunkId, ECPM_AssetManagerStatus::Packaging_Failed, Error);
			return;
		}

		Self->StartPublishWorkflow(ChunkId, Policy);
	});

	// The handle is not known until the policy answers, so a caller that needs one watches the
	// status instead. Cancel is by Chunk, which is what a UI has to hand anyway.
	return FWorkflowHandle::Invalid();
}

FWorkflowHandle UConvaiPakEditorSubsystem::StartPublishWorkflow(const int32 ChunkId, const FCPM_PublishPolicy& Policy)
{
	UWorkflowManagerSubsystem* Manager = UWorkflowManagerSubsystem::Get();
	if (!Manager)
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Packaging_Failed, TEXT("the job system is unavailable"));
		return FWorkflowHandle::Invalid();
	}

	const bool bHasPaks = !Policy.PlatformsToPackage().IsEmpty();

	FWorkflowRequest Request;

	if (bHasPaks)
	{
		Request.Jobs.Add(NewObject<UCPM_PackagePaksJob>(this));
	}
	if (Policy.bUploadRawProject)
	{
		Request.Jobs.Add(NewObject<UCPM_ArchiveRawProjectJob>(this));
	}

	Request.Jobs.Add(NewObject<UCPM_CreateAssetJob>(this));

	UCPM_UploadArtifactsJob* Upload = NewObject<UCPM_UploadArtifactsJob>(this);
	// Configured before it joins the queue: IDeclareIO is asked once, at queue build, and must
	// already know whether to require Paks and a raw archive.
	Upload->Configure(bHasPaks, Policy.bUploadRawProject);
	Request.Jobs.Add(Upload);

	Request.Jobs.Add(NewObject<UCPM_PersistChunkStateJob>(this));

	FCPM_PublishRequest PublishRequest;
	PublishRequest.ChunkId = ChunkId;
	PublishRequest.Policy = Policy;
	Request.Inputs.Add(FInstancedStruct::Make(PublishRequest));

	TWeakObjectPtr<UConvaiPakEditorSubsystem> WeakThis(this);
	Request.OnProgressNative.BindLambda([WeakThis, ChunkId](const FWorkflowStatusInfo& Info)
	{
		if (UConvaiPakEditorSubsystem* Self = WeakThis.Get())
		{
			Self->HandleWorkflowProgress(ChunkId, Info);
		}
	});
	Request.OnFinishedNative.BindLambda([WeakThis, ChunkId](const FWorkflowStatusInfo& Info, const FWorkflowResult&)
	{
		if (UConvaiPakEditorSubsystem* Self = WeakThis.Get())
		{
			Self->HandleWorkflowFinished(ChunkId, Info);
		}
	});

	const FWorkflowHandle Handle = Manager->ICreateWorkflow(Request);
	if (!Handle.IsValid())
	{
		// The queue was rejected before anything ran - a Job requiring something no earlier Job
		// produces. Worth surfacing as-is rather than as a generic failure: it is a wiring bug, and
		// the job system has already logged which type went unsatisfied.
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Packaging_Failed,
			TEXT("the publish steps did not fit together; see the log for which value went unsatisfied"));
		return FWorkflowHandle::Invalid();
	}

	ActivePublishes.Add(ChunkId, Handle);
	return Handle;
}

void UConvaiPakEditorSubsystem::HandleWorkflowProgress(const int32 ChunkId, const FWorkflowStatusInfo& Info)
{
	// Taken from the running Job itself rather than mapped from its display name, so improving the
	// wording cannot silently change what the UI thinks is happening.
	ECPM_AssetManagerStatus Phase = ECPM_AssetManagerStatus::Max;
	if (const UCPM_PublishJobBase* Job = Cast<UCPM_PublishJobBase>(Info.CurrentJob.JobObject.GetObject()))
	{
		Phase = Job->GetPhaseStatus();
	}

	SetStatus(ChunkId, Phase, FString(), Info.Progress, Info.CurrentJob.ProgressText.ToString());
}

void UConvaiPakEditorSubsystem::HandleWorkflowFinished(const int32 ChunkId, const FWorkflowStatusInfo& Info)
{
	ActivePublishes.Remove(ChunkId);

	switch (Info.Status)
	{
	case EWorkflowStatus::Completed:
		SetStatus(ChunkId, ECPM_AssetManagerStatus::UploadPak_Success, FString(), 1.0f);
		break;

	case EWorkflowStatus::Cancelled:
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Publish_Cancelled, FString(), Info.Progress);
		break;

	default:
		SetStatus(ChunkId, ECPM_AssetManagerStatus::UploadPak_Failed,
			Info.ErrorMessage.IsEmpty() ? TEXT("publishing failed") : Info.ErrorMessage, Info.Progress);
		break;
	}
}

bool UConvaiPakEditorSubsystem::CancelPublish(const int32 ChunkId)
{
	const FWorkflowHandle* Handle = ActivePublishes.Find(ChunkId);
	if (!Handle)
	{
		return false;
	}

	UWorkflowManagerSubsystem* Manager = UWorkflowManagerSubsystem::Get();
	// Not forced: the running Job is allowed to finish reporting, so an upload in flight unhooks its
	// request rather than being abandoned mid-transfer.
	return Manager && Manager->ICancelWorkflow(*Handle, /*bForce=*/false);
}

bool UConvaiPakEditorSubsystem::DeleteAsset(const int32 ChunkId, const FString& Version)
{
	const FString AssetId = GetAssetId(ChunkId);
	if (AssetId.IsEmpty())
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Failed, TEXT("this chunk has no published asset"));
		return false;
	}

	if (DeletingChunkId != INDEX_NONE)
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Failed, TEXT("another delete is already in flight"));
		return false;
	}

	DeleteProxy = UCPM_DeleteAssetProxy::DeleteAssetProxy(AssetId, Version);
	if (!DeleteProxy)
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Failed, TEXT("could not build the delete request"));
		return false;
	}

	DeletingChunkId = ChunkId;
	SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Begin);

	DeleteProxy->OnSuccess.AddDynamic(this, &UConvaiPakEditorSubsystem::HandleDeleteSucceeded);
	DeleteProxy->OnFailure.AddDynamic(this, &UConvaiPakEditorSubsystem::HandleDeleteFailed);
	DeleteProxy->Activate();
	return true;
}

void UConvaiPakEditorSubsystem::HandleDeleteSucceeded(const FString& ResponseString)
{
	const int32 ChunkId = DeletingChunkId;
	DeletingChunkId = INDEX_NONE;
	DeleteProxy = nullptr;

	if (ChunkId != INDEX_NONE)
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Success);
	}
}

void UConvaiPakEditorSubsystem::HandleDeleteFailed(const FString& ResponseString)
{
	const int32 ChunkId = DeletingChunkId;
	DeletingChunkId = INDEX_NONE;
	DeleteProxy = nullptr;

	if (ChunkId != INDEX_NONE)
	{
		SetStatus(ChunkId, ECPM_AssetManagerStatus::Delete_Failed, TEXT("the server refused to delete the asset"));
	}
}
