// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Utility/WorkflowBlueprintLibrary.h"
#include "Core/WorkflowContext.h"
#include "Interface/WorkflowManagerInterface.h"
#include "Interface/WorkflowListenerInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorkflowBP, Log, All);

UWorkflowContext* UWorkflowBlueprintLibrary::GetWorkflowContext(const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager)
{
	if (IWorkflowManagerInterface* Interface = WorkflowManager.GetInterface())
	{
		return Interface->IGetContext();
	}
	return nullptr;
}

void UWorkflowBlueprintLibrary::NotifyJobCompleted(
	const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager,
	const FJobCompletionInfo& CompletionInfo)
{
	if (IWorkflowManagerInterface* Interface = WorkflowManager.GetInterface())
	{
		Interface->IOnJobCompleted(CompletionInfo);
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
	const FJobProgressInfo& ProgressInfo)
{
	if (IWorkflowManagerInterface* Interface = WorkflowManager.GetInterface())
	{
		Interface->IReportJobProgress(ProgressInfo);
	}
}

void UWorkflowBlueprintLibrary::AddWorkflowListener(
	const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager,
	const TScriptInterface<IWorkflowListenerInterface>& Listener)
{
	if (IWorkflowManagerInterface* Interface = WorkflowManager.GetInterface())
	{
		Interface->IAddListener(Listener);
	}
}

void UWorkflowBlueprintLibrary::RemoveWorkflowListener(
	const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager,
	const TScriptInterface<IWorkflowListenerInterface>& Listener)
{
	if (IWorkflowManagerInterface* Interface = WorkflowManager.GetInterface())
	{
		Interface->IRemoveListener(Listener);
	}
}

FWorkflowStatusInfo UWorkflowBlueprintLibrary::GetWorkflowStatusInfo(const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager)
{
	if (IWorkflowManagerInterface* Interface = WorkflowManager.GetInterface())
	{
		return Interface->IGetStatusInfo();
	}
	return FWorkflowStatusInfo();
}
