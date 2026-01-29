// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Core/WorkflowContext.h"

void UWorkflowContext::SetValue(FGameplayTag Key, const FString& JsonValue)
{
	if (!Key.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("WorkflowContext::SetValue - Invalid GameplayTag key"));
		return;
	}
	
	Data.Add(Key, JsonValue);
}

FString UWorkflowContext::GetValue(FGameplayTag Key) const
{
	if (const FString* Value = Data.Find(Key))
	{
		return *Value;
	}
	return FString();
}

bool UWorkflowContext::HasKey(FGameplayTag Key) const
{
	return Data.Contains(Key);
}

void UWorkflowContext::RemoveKey(FGameplayTag Key)
{
	Data.Remove(Key);
}

void UWorkflowContext::Clear()
{
	Data.Empty();
}

TArray<FGameplayTag> UWorkflowContext::GetAllKeys() const
{
	TArray<FGameplayTag> Keys;
	Data.GetKeys(Keys);
	return Keys;
}

int32 UWorkflowContext::GetCount() const
{
	return Data.Num();
}
