// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Type/JS_Definations.h"
#include "WorkflowListenerInterface.generated.h"

UINTERFACE(BlueprintType, Blueprintable)
class UWorkflowListenerInterface : public UInterface
{
	GENERATED_BODY()
};

class CONVAIJOBSYSTEM_API IWorkflowListenerInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Workflow")
	void OnWorkflowEvent(EWorkflowEventType EventType, const FWorkflowStatusInfo& StatusInfo);
};
