// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Utility/WorkflowBlueprintLibrary.h"
#include "Core/WorkflowContext.h"
#include "Interface/WorkflowInterface.h"
#include "Interface/WorkflowListenerInterface.h"
#include "Interface/JobInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorkflowBP, Log, All);

UWorkflowContext* UWorkflowBlueprintLibrary::GetWorkflowContext(const TScriptInterface<IWorkflowInterface>& Workflow)
{
	if (IWorkflowInterface* Interface = Workflow.GetInterface())
	{
		return Interface->IGetContext();
	}
	return nullptr;
}

void UWorkflowBlueprintLibrary::NotifyJobCompleted(
	const TScriptInterface<IWorkflowInterface>& Workflow,
	const FJobCompletionInfo& CompletionInfo)
{
	if (IWorkflowInterface* Interface = Workflow.GetInterface())
	{
		Interface->IOnJobCompleted(CompletionInfo);
	}
	else
	{
		UE_LOG(LogWorkflowBP, Error, TEXT("NotifyJobCompleted called with invalid Workflow"));
	}
}

bool UWorkflowBlueprintLibrary::IsWorkflowValid(const TScriptInterface<IWorkflowInterface>& Workflow)
{
	return Workflow.GetInterface() != nullptr;
}

void UWorkflowBlueprintLibrary::ReportJobProgress(
	const TScriptInterface<IWorkflowInterface>& Workflow,
	const FJobProgressInfo& ProgressInfo)
{
	if (IWorkflowInterface* Interface = Workflow.GetInterface())
	{
		Interface->IReportJobProgress(ProgressInfo);
	}
}

void UWorkflowBlueprintLibrary::AddWorkflowListener(
	const TScriptInterface<IWorkflowInterface>& Workflow,
	const TScriptInterface<IWorkflowListenerInterface>& Listener)
{
	if (IWorkflowInterface* Interface = Workflow.GetInterface())
	{
		Interface->IAddListener(Listener);
	}
}

void UWorkflowBlueprintLibrary::RemoveWorkflowListener(
	const TScriptInterface<IWorkflowInterface>& Workflow,
	const TScriptInterface<IWorkflowListenerInterface>& Listener)
{
	if (IWorkflowInterface* Interface = Workflow.GetInterface())
	{
		Interface->IRemoveListener(Listener);
	}
}

FWorkflowStatusInfo UWorkflowBlueprintLibrary::GetWorkflowStatusInfo(const TScriptInterface<IWorkflowInterface>& Workflow)
{
	if (IWorkflowInterface* Interface = Workflow.GetInterface())
	{
		return Interface->IGetStatusInfo();
	}
	return FWorkflowStatusInfo();
}

bool UWorkflowBlueprintLibrary::CreateJobsFromDefinitions(
	UObject* Outer,
	const TArray<FJobDefinition>& JobDefinitions,
	TArray<TScriptInterface<IJobInterface>>& OutJobs)
{
	OutJobs.Empty();
	
	if (!Outer)
	{
		UE_LOG(LogWorkflowBP, Error, TEXT("CreateJobsFromDefinitions: Outer object is null"));
		return false;
	}
	
	if (JobDefinitions.Num() == 0)
	{
		UE_LOG(LogWorkflowBP, Warning, TEXT("CreateJobsFromDefinitions: No job definitions provided"));
		return false;
	}
	
	OutJobs.Reserve(JobDefinitions.Num());
	
	for (int32 i = 0; i < JobDefinitions.Num(); i++)
	{
		const FJobDefinition& Definition = JobDefinitions[i];
		
		if (!Definition.JobClass)
		{
			UE_LOG(LogWorkflowBP, Error, TEXT("CreateJobsFromDefinitions: Job definition %d has no class specified"), i);
			OutJobs.Empty();
			return false;
		}
		
		if (!Definition.JobClass->ImplementsInterface(UJobInterface::StaticClass()))
		{
			UE_LOG(LogWorkflowBP, Error, TEXT("CreateJobsFromDefinitions: Job class '%s' does not implement IJobInterface"), 
				*Definition.JobClass->GetName());
			OutJobs.Empty();
			return false;
		}
		
		UObject* JobObject = NewObject<UObject>(Outer, Definition.JobClass);
		if (!JobObject)
		{
			UE_LOG(LogWorkflowBP, Error, TEXT("CreateJobsFromDefinitions: Failed to create job object from class '%s'"), 
				*Definition.JobClass->GetName());
			OutJobs.Empty();
			return false;
		}

		IJobInterface::Execute_IPreInitialize(JobObject, Definition);
		
		TScriptInterface<IJobInterface> JobInterface;
		JobInterface.SetObject(JobObject);
		JobInterface.SetInterface(Cast<IJobInterface>(JobObject));
		
		OutJobs.Add(JobInterface);
		
		UE_LOG(LogWorkflowBP, Verbose, TEXT("Created job %d from class '%s'"), i, *Definition.JobClass->GetName());
	}
	
	return true;
}
