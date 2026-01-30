// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Type/JS_Definations.h"
#include "WorkflowBlueprintLibrary.generated.h"

class IWorkflowListenerInterface;
class IJobInterface;
class UWorkflowContext;
class IWorkflowManagerInterface;

UCLASS()
class CONVAIJOBSYSTEM_API UWorkflowBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Workflow", meta = (DisplayName = "Get Workflow Context"))
	static UWorkflowContext* GetWorkflowContext(const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager);

	UFUNCTION(BlueprintCallable, Category = "Workflow", meta = (DisplayName = "Notify Job Completed"))
	static void NotifyJobCompleted(
		const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager,
		const TScriptInterface<IJobInterface>& Job,
		EJobResult Result,
		const FString& ErrorMessage = TEXT(""));

	UFUNCTION(BlueprintPure, Category = "Workflow", meta = (DisplayName = "Is Workflow Manager Valid"))
	static bool IsWorkflowManagerValid(const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager);

	UFUNCTION(BlueprintCallable, Category = "Workflow", meta = (DisplayName = "Report Job Progress"))
	static void ReportJobProgress(
		const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager,
		const TScriptInterface<IJobInterface>& Job,
		float Progress);

	UFUNCTION(BlueprintCallable, Category = "Workflow", meta = (DisplayName = "Add Workflow Listener"))
	static void AddWorkflowListener(
		const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager,
		const TScriptInterface<IWorkflowListenerInterface>& Listener);

	UFUNCTION(BlueprintCallable, Category = "Workflow", meta = (DisplayName = "Remove Workflow Listener"))
	static void RemoveWorkflowListener(
		const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager,
		const TScriptInterface<IWorkflowListenerInterface>& Listener);

	UFUNCTION(BlueprintPure, Category = "Workflow", meta = (DisplayName = "Get Workflow Status Info"))
	static FWorkflowStatusInfo GetWorkflowStatusInfo(const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager);
};
