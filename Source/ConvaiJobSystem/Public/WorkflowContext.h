// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "WorkflowContext.generated.h"

/**
 * Shared context object for workflow jobs.
 * Acts as a blackboard where jobs can read/write data using GameplayTags as keys.
 * Values are stored as JSON strings for flexibility.
 */
UCLASS(BlueprintType)
class CONVAIJOBSYSTEM_API UWorkflowContext : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Set a value in the context
	 * @param Key The GameplayTag key
	 * @param JsonValue The value as a JSON string
	 */
	UFUNCTION(BlueprintCallable, Category = "Workflow|Context")
	void SetValue(FGameplayTag Key, const FString& JsonValue);

	/**
	 * Get a value from the context
	 * @param Key The GameplayTag key
	 * @return The value as a JSON string, or empty string if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "Workflow|Context")
	FString GetValue(FGameplayTag Key) const;

	/**
	 * Check if a key exists in the context
	 * @param Key The GameplayTag key
	 * @return True if the key exists
	 */
	UFUNCTION(BlueprintCallable, Category = "Workflow|Context")
	bool HasKey(FGameplayTag Key) const;

	/**
	 * Remove a key from the context
	 * @param Key The GameplayTag key to remove
	 */
	UFUNCTION(BlueprintCallable, Category = "Workflow|Context")
	void RemoveKey(FGameplayTag Key);

	/**
	 * Clear all data from the context
	 */
	UFUNCTION(BlueprintCallable, Category = "Workflow|Context")
	void Clear();

	/**
	 * Get all keys currently in the context
	 * @return Array of all GameplayTag keys
	 */
	UFUNCTION(BlueprintCallable, Category = "Workflow|Context")
	TArray<FGameplayTag> GetAllKeys() const;

	/**
	 * Get the number of entries in the context
	 * @return Number of key-value pairs
	 */
	UFUNCTION(BlueprintPure, Category = "Workflow|Context")
	int32 GetCount() const;

private:
	/** Internal storage for context data */
	UPROPERTY()
	TMap<FGameplayTag, FString> Data;
};
