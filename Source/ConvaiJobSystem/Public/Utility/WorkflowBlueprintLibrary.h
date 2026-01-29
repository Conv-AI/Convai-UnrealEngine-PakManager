// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Interface/WorkflowManagerInterface.h"
#include "WorkflowBlueprintLibrary.generated.h"

class UWorkflowContext;

/**
 * Blueprint Function Library for workflow operations.
 * Provides Blueprint-callable wrappers for IWorkflowManagerInterface functions.
 */
UCLASS()
class CONVAIJOBSYSTEM_API UWorkflowBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Get the workflow context (blackboard) from a workflow manager.
	 * Use this to read/write shared data between jobs.
	 * 
	 * @param WorkflowManager The workflow manager interface
	 * @return The workflow context, or nullptr if invalid
	 */
	UFUNCTION(BlueprintCallable, Category = "Workflow", meta = (DisplayName = "Get Workflow Context"))
	static UWorkflowContext* GetWorkflowContext(const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager);

	/**
	 * Notify the workflow manager that a job has completed.
	 * MUST be called by every job when it finishes (success or failure).
	 * 
	 * @param WorkflowManager The workflow manager interface
	 * @param Job The job object that completed (pass 'self')
	 * @param Result The result of the job execution
	 * @param ErrorMessage Optional error message if failed
	 */
	UFUNCTION(BlueprintCallable, Category = "Workflow", meta = (DisplayName = "Notify Job Completed"))
	static void NotifyJobCompleted(
		const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager,
		UObject* Job,
		EJobResult Result,
		const FString& ErrorMessage = TEXT(""));

	/**
	 * Check if workflow cancellation has been requested.
	 * Long-running jobs should check this periodically and abort if true.
	 * 
	 * @param WorkflowManager The workflow manager interface
	 * @return True if cancellation was requested
	 */
	UFUNCTION(BlueprintPure, Category = "Workflow", meta = (DisplayName = "Is Cancellation Requested"))
	static bool IsCancellationRequested(const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager);

	/**
	 * Check if a workflow manager interface is valid.
	 * 
	 * @param WorkflowManager The workflow manager interface to check
	 * @return True if the interface is valid and can be used
	 */
	UFUNCTION(BlueprintPure, Category = "Workflow", meta = (DisplayName = "Is Workflow Manager Valid"))
	static bool IsWorkflowManagerValid(const TScriptInterface<IWorkflowManagerInterface>& WorkflowManager);
};
