// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "ConvaiPakManagerEUWBase.generated.h"


UCLASS(Abstract)
class CONVAIPAKMANAGEREDITOR_API UConvaiPakManagerEUWBase : public UEditorUtilityWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Convai|PakManager")
	void LoadUI();
	void LoadUI_Implementation(){}
};
