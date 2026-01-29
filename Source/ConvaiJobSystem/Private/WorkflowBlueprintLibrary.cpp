// Copyright 2025 Convai Inc. All Rights Reserved.

#include "WorkflowBlueprintLibrary.h"
#include "WorkflowContext.h"

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
	UObject* Job,
	EJobResult Result,
	const FString& ErrorMessage)
{
	if (IWorkflowManagerInterface* Interface = WorkflowManager.GetInterface())
	{
		Interface->OnJobCompleted(Job, Result, ErrorMessage);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("NotifyJobCompleted called with invalid WorkflowManager"));
	}
}

bool UWorkflowBlueprintLibrary::IsCancellationRequested(const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager)
{
	if (IWorkflowManagerInterface* Interface = WorkflowManager.GetInterface())
	{
		return Interface->IsCancellationRequested();
	}
	return false;
}

bool UWorkflowBlueprintLibrary::IsWorkflowManagerValid(const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager)
{
	return WorkflowManager.GetInterface() != nullptr;
}
