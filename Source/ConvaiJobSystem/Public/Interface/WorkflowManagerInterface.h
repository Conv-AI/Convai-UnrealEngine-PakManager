// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Type/JS_Definations.h"
#include "WorkflowManagerInterface.generated.h"

class IWorkflowInterface;
class IWorkflowListenerInterface;

UINTERFACE(MinimalAPI)
class UWorkflowManagerInterface : public UInterface
{
	GENERATED_BODY()
};

class CONVAIJOBSYSTEM_API IWorkflowManagerInterface
{
	GENERATED_BODY()

public:
	virtual FWorkflowHandle ICreateWorkflowFromJobs(const FCreateWorkflowFromJobsParams& Params) = 0;
	virtual FWorkflowHandle ICreateWorkflowFromJobDefinitions(const FCreateWorkflowFromJobDefinitionsParams& Params) = 0;
	virtual bool IStartWorkflow(const FWorkflowHandle& Handle) = 0;

	virtual bool ICancelWorkflow(const FWorkflowHandle& Handle, bool bForce = false) = 0;
	virtual bool ICancelAllWorkflows(bool bForce = false) = 0;

	virtual TScriptInterface<IWorkflowInterface> IGetWorkflow(const FWorkflowHandle& Handle) const = 0;
	virtual TArray<FWorkflowHandle> IGetAllWorkflowHandles() const = 0;
};
