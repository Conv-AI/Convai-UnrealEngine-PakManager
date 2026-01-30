// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Utility/WorkflowBlueprintLibrary.h"
#include "Core/WorkflowContext.h"
#include "Interface/WorkflowManagerInterface.h"
#include "Interface/JobInterface.h"
#include "Interface/WorkflowListenerInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorkflowBP, Log, All);

UWorkflowContext* UWorkflowBlueprintLibrary::GetWorkflowContext(const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager)
{
	if (IWorkflowManagerInterface* Interface = WorkflowManager.GetInterface())
	{
		return Interface->GetContext();
	}
	return nullptr;
}

void UWorkflowBlueprintLibrary::NotifyJobCompleted(
	const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager,
	const TScriptInterface<IJobInterface>& Job,
	EJobResult Result,
	const FString& ErrorMessage)
{
	if (IWorkflowManagerInterface* Interface = WorkflowManager.GetInterface())
	{
		Interface->OnJobCompleted(Job, Result, ErrorMessage);
	}
	else
	{
		UE_LOG(LogWorkflowBP, Error, TEXT("NotifyJobCompleted called with invalid WorkflowManager"));
	}
}

bool UWorkflowBlueprintLibrary::IsWorkflowManagerValid(const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager)
{
	return WorkflowManager.GetInterface() != nullptr;
}

void UWorkflowBlueprintLibrary::ReportJobProgress(
	const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager,
	const TScriptInterface<IJobInterface>& Job,
	float Progress)
{
	if (IWorkflowManagerInterface* Interface = WorkflowManager.GetInterface())
	{
		Interface->ReportJobProgress(Job, Progress);
	}
}

void UWorkflowBlueprintLibrary::AddWorkflowListener(
	const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager,
	const TScriptInterface<IWorkflowListenerInterface>& Listener)
{
	if (IWorkflowManagerInterface* Interface = WorkflowManager.GetInterface())
	{
		Interface->AddListener(Listener);
	}
}

void UWorkflowBlueprintLibrary::RemoveWorkflowListener(
	const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager,
	const TScriptInterface<IWorkflowListenerInterface>& Listener)
{
	if (IWorkflowManagerInterface* Interface = WorkflowManager.GetInterface())
	{
		Interface->RemoveListener(Listener);
	}
}

FWorkflowStatusInfo UWorkflowBlueprintLibrary::GetWorkflowStatusInfo(const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager)
{
	if (IWorkflowManagerInterface* Interface = WorkflowManager.GetInterface())
	{
		return Interface->GetStatusInfo();
	}
	return FWorkflowStatusInfo();
}
