// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "JobTypes.generated.h"

/**
 * Result of a job execution
 */
UENUM(BlueprintType)
enum class EJobResult : uint8
{
	/** Job completed successfully */
	Success		UMETA(DisplayName = "Success"),
	
	/** Job failed during execution */
	Failed		UMETA(DisplayName = "Failed"),
	
	/** Job was cancelled before completion */
	Cancelled	UMETA(DisplayName = "Cancelled")
};

/**
 * Current status of a workflow
 */
UENUM(BlueprintType)
enum class EWorkflowStatus : uint8
{
	/** Workflow is idle, not started */
	Idle		UMETA(DisplayName = "Idle"),
	
	/** Workflow is currently executing jobs */
	Running		UMETA(DisplayName = "Running"),
	
	/** Workflow completed successfully */
	Completed	UMETA(DisplayName = "Completed"),
	
	/** Workflow failed due to job failure */
	Failed		UMETA(DisplayName = "Failed"),
	
	/** Workflow was cancelled */
	Cancelled	UMETA(DisplayName = "Cancelled")
};
