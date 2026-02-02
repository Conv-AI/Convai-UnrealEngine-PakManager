// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Core/WorkflowManagerSubsystem.h"
#include "Core/Workflow.h"
#include "Interface/WorkflowInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorkflowManager, Log, All);

void UWorkflowManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogWorkflowManager, Log, TEXT("WorkflowSubsystem initialized"));
}

void UWorkflowManagerSubsystem::Deinitialize()
{
	ICancelAllWorkflows(true);
	Workflows.Empty();
	UE_LOG(LogWorkflowManager, Log, TEXT("WorkflowSubsystem deinitialized"));
	Super::Deinitialize();
}

FWorkflowHandle UWorkflowManagerSubsystem::ICreateWorkflowFromJobs(const FCreateWorkflowFromJobsParams& Params)
{
	FWorkflowHandle Handle = FWorkflowHandle::Generate();

	UWorkflow* Workflow = NewObject<UWorkflow>(this);
	Workflow->SetHandle(Handle);

	if (!Workflow->IInitializeFromJobs(Params.Request))
	{
		UE_LOG(LogWorkflowManager, Error, TEXT("Failed to initialize workflow from jobs"));
		return FWorkflowHandle::Invalid();
	}

	Workflows.Add(Handle, Workflow);
	UE_LOG(LogWorkflowManager, Log, TEXT("Created workflow [%s] with %d jobs"), *Handle.Id.ToString(), Params.Request.Jobs.Num());

	if (Params.bStartImmediately)
	{
		Workflow->IStartWorkflow();
	}

	return Handle;
}

FWorkflowHandle UWorkflowManagerSubsystem::ICreateWorkflowFromJobDefinitions(const FCreateWorkflowFromJobDefinitionsParams& Params)
{
	const FWorkflowHandle Handle = FWorkflowHandle::Generate();

	UWorkflow* Workflow = NewObject<UWorkflow>(this);
	Workflow->SetHandle(Handle);

	if (!Workflow->IInitializeFromJobDefinitions(Params.Request))
	{
		UE_LOG(LogWorkflowManager, Error, TEXT("Failed to initialize workflow from job definitions"));
		return FWorkflowHandle::Invalid();
	}

	Workflows.Add(Handle, Workflow);
	UE_LOG(LogWorkflowManager, Log, TEXT("Created workflow [%s] with %d job definitions"), *Handle.Id.ToString(), Params.Request.JobDefinitions.Num());

	if (Params.bStartImmediately)
	{
		Workflow->IStartWorkflow();
	}

	return Handle;
}

bool UWorkflowManagerSubsystem::IStartWorkflow(const FWorkflowHandle& Handle)
{
	if (const TObjectPtr<UWorkflow>* WorkflowPtr = Workflows.Find(Handle))
	{
		if (UWorkflow* Workflow = *WorkflowPtr; Workflow && Workflow->IsInitialized())
		{
			return Workflow->IStartWorkflow();
		}
		UE_LOG(LogWorkflowManager, Warning, TEXT("Workflow [%s] is not in initialized state"), *Handle.Id.ToString());
		return false;
	}

	UE_LOG(LogWorkflowManager, Warning, TEXT("Workflow [%s] not found"), *Handle.Id.ToString());
	return false;
}

bool UWorkflowManagerSubsystem::ICancelWorkflow(const FWorkflowHandle& Handle, bool bForce)
{
	if (const TObjectPtr<UWorkflow>* WorkflowPtr = Workflows.Find(Handle))
	{
		if (UWorkflow* Workflow = *WorkflowPtr)
		{
			Workflow->ICancelWorkflow(bForce);
			return true;
		}
	}

	UE_LOG(LogWorkflowManager, Warning, TEXT("Workflow [%s] not found"), *Handle.Id.ToString());
	return false;
}

bool UWorkflowManagerSubsystem::ICancelAllWorkflows(bool bForce)
{
	bool bAnyCancelled = false;
	for (auto& Pair : Workflows)
	{
		if (UWorkflow* Workflow = Pair.Value)
		{
			if (Workflow->IsRunning() || Workflow->IsInitialized())
			{
				Workflow->ICancelWorkflow(bForce);
				bAnyCancelled = true;
			}
		}
	}
	return bAnyCancelled;
}

TScriptInterface<IWorkflowInterface> UWorkflowManagerSubsystem::IGetWorkflow(const FWorkflowHandle& Handle) const
{
	if (const TObjectPtr<UWorkflow>* WorkflowPtr = Workflows.Find(Handle))
	{
		if (UWorkflow* Workflow = *WorkflowPtr)
		{
			TScriptInterface<IWorkflowInterface> Interface;
			Interface.SetObject(Workflow);
			Interface.SetInterface(Cast<IWorkflowInterface>(Workflow));
			return Interface;
		}
	}
	return TScriptInterface<IWorkflowInterface>();
}

TArray<FWorkflowHandle> UWorkflowManagerSubsystem::IGetAllWorkflowHandles() const
{
	TArray<FWorkflowHandle> Handles;
	Workflows.GetKeys(Handles);
	return Handles;
}

int32 UWorkflowManagerSubsystem::GetActiveWorkflowCount() const
{
	int32 Count = 0;
	for (const auto& Pair : Workflows)
	{
		if (const UWorkflow* Workflow = Pair.Value)
		{
			if (Workflow->IsRunning())
			{
				Count++;
			}
		}
	}
	return Count;
}

bool UWorkflowManagerSubsystem::RemoveWorkflow(const FWorkflowHandle& Handle)
{
	if (const TObjectPtr<UWorkflow>* WorkflowPtr = Workflows.Find(Handle))
	{
		if (const UWorkflow* Workflow = *WorkflowPtr; Workflow && Workflow->IsRunning())
		{
			UE_LOG(LogWorkflowManager, Warning, TEXT("Cannot remove running workflow [%s]"), *Handle.Id.ToString());
			return false;
		}
	}

	return Workflows.Remove(Handle) > 0;
}

void UWorkflowManagerSubsystem::RemoveCompletedWorkflows()
{
	TArray<FWorkflowHandle> ToRemove;

	for (const auto& Pair : Workflows)
	{
		if (const UWorkflow* Workflow = Pair.Value)
		{
			if (const FWorkflowStatusInfo StatusInfo = Workflow->IGetStatusInfo(); StatusInfo.Status == EWorkflowStatus::Completed ||
				StatusInfo.Status == EWorkflowStatus::Failed ||
				StatusInfo.Status == EWorkflowStatus::Cancelled ||
				StatusInfo.Status == EWorkflowStatus::Timeout)
			{
				ToRemove.Add(Pair.Key);
			}
		}
	}

	for (const FWorkflowHandle& Handle : ToRemove)
	{
		Workflows.Remove(Handle);
	}

	if (ToRemove.Num() > 0)
	{
		UE_LOG(LogWorkflowManager, Log, TEXT("Removed %d completed workflows"), ToRemove.Num());
	}
}
