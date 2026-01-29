// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "JobTypes.h"
#include "WorkflowManagerInterface.generated.h"

class UWorkflowContext;

UINTERFACE(BlueprintType, MinimalAPI)
class UWorkflowManagerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface that workflow managers implement.
 * Jobs receive this interface to interact with the workflow system.
 */
class CONVAIJOBSYSTEM_API IWorkflowManagerInterface
{
	GENERATED_BODY()

public:
	/**
	 * Get the shared workflow context (blackboard)
	 * @return The workflow context object
	 */
	virtual UWorkflowContext* GetContext() const = 0;

	/**
	 * Called by a job to signal completion
	 * @param Job The job that completed (for identification)
	 * @param Result The result of the job execution
	 * @param ErrorMessage Optional error message if failed
	 */
	virtual void OnJobCompleted(UObject* Job, EJobResult Result, const FString& ErrorMessage = TEXT("")) = 0;

	/**
	 * Check if cancellation has been requested
	 * Jobs should check this periodically and abort if true
	 * @return True if cancellation was requested
	 */
	virtual bool IsCancellationRequested() const = 0;
};
