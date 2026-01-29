// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "JS_Definations.generated.h"

UENUM(BlueprintType)
enum class EJobResult : uint8
{
	Success		UMETA(DisplayName = "Success"),
	Failed		UMETA(DisplayName = "Failed"),
	Cancelled	UMETA(DisplayName = "Cancelled"),
	Timeout		UMETA(DisplayName = "Timeout")
};

UENUM(BlueprintType)
enum class EWorkflowStatus : uint8
{
	Idle					UMETA(DisplayName = "Idle"),
	Running					UMETA(DisplayName = "Running"),
	CancellationRequested	UMETA(DisplayName = "Cancellation Requested"),
	Completed				UMETA(DisplayName = "Completed"),
	Failed					UMETA(DisplayName = "Failed"),
	Timeout					UMETA(DisplayName = "Timeout"),
	Cancelled				UMETA(DisplayName = "Cancelled")
};

USTRUCT(BlueprintType)
struct CONVAIJOBSYSTEM_API FWorkflowConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow")
	float JobTimeoutSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow")
	float WorkflowTimeoutSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow")
	bool bContinueOnJobTimeout = false;
};
