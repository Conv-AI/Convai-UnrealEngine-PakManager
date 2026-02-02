// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Type/JS_Definations.h"
#include "WorkflowBlueprintLibrary.generated.h"

class IWorkflowListenerInterface;
class IJobInterface;
class UWorkflowContext;
class IWorkflowInterface;
class UWorkflowManagerSubsystem;

UCLASS()
class CONVAIJOBSYSTEM_API UWorkflowBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Workflow", meta = (DisplayName = "Get Workflow Context"))
	static UWorkflowContext* GetWorkflowContext(const TScriptInterface<IWorkflowInterface>& Workflow);

	UFUNCTION(BlueprintCallable, Category = "Workflow", meta = (DisplayName = "Notify Job Completed"))
	static void NotifyJobCompleted(
		const TScriptInterface<IWorkflowInterface>& Workflow,
		const FJobCompletionInfo& CompletionInfo);

	UFUNCTION(BlueprintPure, Category = "Workflow", meta = (DisplayName = "Is Workflow Valid"))
	static bool IsWorkflowValid(const TScriptInterface<IWorkflowInterface>& Workflow);

	UFUNCTION(BlueprintCallable, Category = "Workflow", meta = (DisplayName = "Report Job Progress"))
	static void ReportJobProgress(
		const TScriptInterface<IWorkflowInterface>& Workflow,
		const FJobProgressInfo& ProgressInfo);

	UFUNCTION(BlueprintCallable, Category = "Workflow", meta = (DisplayName = "Add Workflow Listener"))
	static void AddWorkflowListener(
		const TScriptInterface<IWorkflowInterface>& Workflow,
		const TScriptInterface<IWorkflowListenerInterface>& Listener);

	UFUNCTION(BlueprintCallable, Category = "Workflow", meta = (DisplayName = "Remove Workflow Listener"))
	static void RemoveWorkflowListener(
		const TScriptInterface<IWorkflowInterface>& Workflow,
		const TScriptInterface<IWorkflowListenerInterface>& Listener);

	UFUNCTION(BlueprintPure, Category = "Workflow", meta = (DisplayName = "Get Workflow Status Info"))
	static FWorkflowStatusInfo GetWorkflowStatusInfo(const TScriptInterface<IWorkflowInterface>& Workflow);
	
	/** Creates job instances from definitions. Calls IPreInitialize() on each job. */
	UFUNCTION(BlueprintCallable, Category = "Workflow", meta = (DisplayName = "Create Jobs From Definitions"))
	static bool CreateJobsFromDefinitions( UObject* Outer, const TArray<FJobDefinition>& JobDefinitions,
		TArray<TScriptInterface<IJobInterface>>& OutJobs);

	// ---- Manager Convenience Functions ----

	/** Creates and optionally starts a workflow from job instances. */
	UFUNCTION(BlueprintCallable, Category = "Workflow|Manager", meta = (DisplayName = "Create Workflow From Jobs"))
	static FWorkflowHandle CreateWorkflowFromJobs(const FCreateWorkflowFromJobsParams& Params);

	/** Creates and optionally starts a workflow from job definitions. */
	UFUNCTION(BlueprintCallable, Category = "Workflow|Manager", meta = (DisplayName = "Create Workflow From Job Definitions"))
	static FWorkflowHandle CreateWorkflowFromJobDefinitions(const FCreateWorkflowFromJobDefinitionsParams& Params);

	/** Cancels a running workflow. */
	UFUNCTION(BlueprintCallable, Category = "Workflow|Manager", meta = (DisplayName = "Cancel Workflow"))
	static bool CancelWorkflow(const FWorkflowHandle& Handle, bool bForce = false);
	
	UFUNCTION(BlueprintCallable, Category = "Workflow|Manager", meta = (DisplayName = "Cancel All Workflow"))
	static bool CancelAllWorkflows(bool bForce = false);
};
