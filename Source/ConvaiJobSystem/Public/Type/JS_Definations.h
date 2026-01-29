// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "JS_Definations.generated.h"

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

/**
 * Configuration for workflow execution
 */
USTRUCT(BlueprintType)
struct CONVAIJOBSYSTEM_API FWorkflowConfig
{
	GENERATED_BODY()

	/** Timeout for individual jobs in seconds. 0 = no timeout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow")
	float JobTimeoutSeconds = 0.0f;

	/** Timeout for entire workflow in seconds. 0 = no timeout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow")
	float WorkflowTimeoutSeconds = 0.0f;

	/** If true, automatically force-fail timed out jobs and continue. If false, fail workflow on timeout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow")
	bool bContinueOnJobTimeout = false;
};