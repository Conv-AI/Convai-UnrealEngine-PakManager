// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Interface/WorkflowManagerInterface.h"
#include "WorkflowSubsystem.generated.h"

class UWorkflow;

UCLASS()
class CONVAIJOBSYSTEM_API UWorkflowSubsystem : public UEngineSubsystem, public IWorkflowManagerInterface
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// IWorkflowManagerInterface
	UFUNCTION(BlueprintCallable, Category = "Workflow")
	virtual FWorkflowHandle ICreateWorkflowFromJobs(const FCreateWorkflowFromJobsParams& Params) override;

	UFUNCTION(BlueprintCallable, Category = "Workflow")
	virtual FWorkflowHandle ICreateWorkflowFromJobDefinitions(const FCreateWorkflowFromJobDefinitionsParams& Params) override;

	UFUNCTION(BlueprintCallable, Category = "Workflow")
	virtual bool IStartWorkflow(const FWorkflowHandle& Handle) override;

	UFUNCTION(BlueprintCallable, Category = "Workflow")
	virtual bool ICancelWorkflow(const FWorkflowHandle& Handle, bool bForce = false) override;

	UFUNCTION(BlueprintCallable, Category = "Workflow")
	virtual bool ICancelAllWorkflows(bool bForce = false) override;

	UFUNCTION(BlueprintCallable, Category = "Workflow")
	virtual TScriptInterface<IWorkflowInterface> IGetWorkflow(const FWorkflowHandle& Handle) const override;

	UFUNCTION(BlueprintCallable, Category = "Workflow")
	virtual TArray<FWorkflowHandle> IGetAllWorkflowHandles() const override;

	UFUNCTION(BlueprintPure, Category = "Workflow")
	int32 GetActiveWorkflowCount() const;

	UFUNCTION(BlueprintCallable, Category = "Workflow")
	bool RemoveWorkflow(const FWorkflowHandle& Handle);

	UFUNCTION(BlueprintCallable, Category = "Workflow")
	void RemoveCompletedWorkflows();

private:
	UPROPERTY()
	TMap<FWorkflowHandle, TObjectPtr<UWorkflow>> Workflows;
};
