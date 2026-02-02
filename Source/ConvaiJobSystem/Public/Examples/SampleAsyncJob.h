// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/NoExportTypes.h"
#include "Interface/JobInterface.h"
#include "SampleAsyncJob.generated.h"

/**
 * Sample job demonstrating async execution with progress reporting.
 * Simulates a task that takes time to complete (e.g., downloading, processing).
 */
UCLASS(Blueprintable, BlueprintType)
class CONVAIJOBSYSTEM_API USampleAsyncJob : public UObject, public IJobInterface
{
	GENERATED_BODY()

public:
	// Duration of the simulated async task in seconds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample Job")
	float TaskDurationSeconds = 3.0f;

	// Number of progress updates during execution
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample Job")
	int32 ProgressSteps = 5;

	// Job configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample Job")
	FJobConfig JobConfig;

	// IJobInterface
	virtual void IExecute_Implementation(const TScriptInterface<IWorkflowInterface>& Workflow) override;
	virtual void ICancel_Implementation(bool bForce) override;
	virtual FJobConfig IGetJobConfig_Implementation() const override { return JobConfig; }

private:
	void SimulateProgress();
	void CompleteJob();

	UPROPERTY()
	TScriptInterface<IWorkflowInterface> CachedWorkflow;

	FTimerHandle ProgressTimerHandle;
	int32 CurrentStep = 0;
	bool bIsCancelled = false;
};

/**
 * Sample job that randomly fails to demonstrate retry functionality.
 * Configurable failure rate allows testing different retry scenarios.
 */
UCLASS(Blueprintable, BlueprintType)
class CONVAIJOBSYSTEM_API USampleFailingJob : public UObject, public IJobInterface
{
	GENERATED_BODY()

public:
	// Probability of failure (0.0 = never fail, 1.0 = always fail)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample Job", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FailureProbability = 0.7f;

	// Simulated execution time before success/failure
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample Job")
	float ExecutionTimeSeconds = 0.5f;

	// Job configuration with retry settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample Job")
	FJobConfig JobConfig;

	USampleFailingJob();

	// IJobInterface
	virtual void IExecute_Implementation(const TScriptInterface<IWorkflowInterface>& Workflow) override;
	virtual void ICancel_Implementation(bool bForce) override;
	virtual FJobConfig IGetJobConfig_Implementation() const override { return JobConfig; }

private:
	void AttemptCompletion();

	UPROPERTY()
	TScriptInterface<IWorkflowInterface> CachedWorkflow;

	FTimerHandle ExecutionTimerHandle;
	bool bIsCancelled = false;
};

/**
 * Sample job demonstrating conditional execution.
 * Only executes if a specific key exists in the workflow context.
 */
UCLASS(Blueprintable, BlueprintType)
class CONVAIJOBSYSTEM_API USampleConditionalJob : public UObject, public IJobInterface
{
	GENERATED_BODY()

public:
	// The context key that must exist for this job to execute
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample Job")
	FGameplayTag RequiredContextKey;

	// Job configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample Job")
	FJobConfig JobConfig;

	// IJobInterface
	virtual void IExecute_Implementation(const TScriptInterface<IWorkflowInterface>& Workflow) override;
	virtual bool IShouldSkip_Implementation(UWorkflowContext* Context) const override;
	virtual FJobConfig IGetJobConfig_Implementation() const override { return JobConfig; }
};

/**
 * Sample job that writes data to the workflow context.
 * Demonstrates data sharing between jobs.
 */
UCLASS(Blueprintable, BlueprintType)
class CONVAIJOBSYSTEM_API USampleContextWriterJob : public UObject, public IJobInterface
{
	GENERATED_BODY()

public:
	// Key-value pairs to write to context
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample Job")
	TMap<FGameplayTag, FString> DataToWrite;

	// Job configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample Job")
	FJobConfig JobConfig;

	// IJobInterface
	virtual void IExecute_Implementation(const TScriptInterface<IWorkflowInterface>& Workflow) override;
	virtual FJobConfig IGetJobConfig_Implementation() const override { return JobConfig; }
};
