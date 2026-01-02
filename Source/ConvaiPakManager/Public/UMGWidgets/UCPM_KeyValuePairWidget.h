// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Types/CPM_WidgetTypes.h"
#include "UCPM_KeyValuePairWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPM_OnKeyValuePairsChanged, const TArray<FCPM_KeyValuePair>&, Pairs);

/**
 * A UMG widget wrapper for the Key-Value Pair List Slate widget.
 * Allows adding/removing/editing key-value pairs dynamically.
 * Can be used in Editor Utility Widgets or in-game UI.
 */
UCLASS()
class CONVAIPAKMANAGER_API UCPM_KeyValuePairWidget : public UWidget
{
	GENERATED_BODY()

public:
	UCPM_KeyValuePairWidget(const FObjectInitializer& ObjectInitializer);

	//~ UWidget Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

	//~ Properties

	/** Initial pairs to populate when the widget is created */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|PakManager|KeyValuePairWidget")
	TArray<FCPM_KeyValuePair> InitialPairs;

	/** Hint text displayed in empty Key input fields */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|PakManager|KeyValuePairWidget|Appearance")
	FText KeyHintText;

	/** Hint text displayed in empty Value input fields */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|PakManager|KeyValuePairWidget|Appearance")
	FText ValueHintText;

	/** Text displayed on the Add button */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|PakManager|KeyValuePairWidget|Appearance")
	FText AddButtonText;

	/** Whether to show the Add button */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|PakManager|KeyValuePairWidget|Appearance")
	bool bShowAddButton;

	/** Maximum height before the list becomes scrollable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|PakManager|KeyValuePairWidget|Appearance", meta = (ClampMin = "50.0"))
	float MaxHeight;

	//~ Events

	/** Called whenever the list changes (add/remove/edit) */
	UPROPERTY(BlueprintAssignable, Category = "Convai|PakManager|KeyValuePairWidget|Events")
	FCPM_OnKeyValuePairsChanged OnPairsChanged;

	//~ Basic Getters

	/** Get all key-value pairs (including empty ones) */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	TArray<FCPM_KeyValuePair> GetAllPairs() const;

	/** Get only valid pairs (where key is not empty) */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	TArray<FCPM_KeyValuePair> GetValidPairs() const;

	/** Get the number of pairs */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	int32 GetPairCount() const;

	/** Convert pairs to a TMap for easy lookup */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	TMap<FString, FString> GetPairsAsMap() const;

	//~ Basic Setters

	/** Set all pairs (replaces existing) */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	void SetPairs(const TArray<FCPM_KeyValuePair>& InPairs);

	/** Add a pair using the full struct (allows setting all control options) */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	void AddPair(const FCPM_KeyValuePair& Pair);

	/** Remove the pair at the specified index */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	void RemovePairAt(int32 Index);

	/** Clear all pairs */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	void ClearAllPairs();

	//~ Index-Based Access

	/** Get pair at the specified index. Returns empty pair if invalid. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	FCPM_KeyValuePair GetPairByIndex(int32 Index) const;

	/** Set pair at the specified index. Returns true if successful. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	bool SetPairByIndex(int32 Index, const FCPM_KeyValuePair& InPair);

	//~ Key-Based Lookup

	/** Check if a key exists in the list */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	bool HasKey(const FString& Key) const;

	/** Find the index of a key. Returns -1 if not found. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	int32 FindKeyIndex(const FString& Key) const;

	/** Get the value for a specific key. Returns empty string if not found. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	FString GetValueForKey(const FString& Key) const;

	/** Get the full pair for a specific key. Returns empty pair if not found. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	FCPM_KeyValuePair GetPairByKey(const FString& Key) const;

	/** Set/update the value for a specific key. Returns true if key was found. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	bool SetValueForKey(const FString& Key, const FString& NewValue);

	/** Set/update the full pair for a specific key. Returns true if key was found. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	bool SetPairByKey(const FString& Key, const FCPM_KeyValuePair& InPair);

	/** Remove the pair with the specified key. Returns true if found and removed. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	bool RemoveByKey(const FString& Key);

	/** Add or update a pair. If key exists, updates the pair. If not, adds new pair. */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	void AddOrUpdatePair(const FCPM_KeyValuePair& InPair);

protected:
	//~ UWidget Interface
	virtual void SynchronizeProperties() override;

private:
	/** The underlying Slate widget */
	TSharedPtr<class SCPM_KeyValueList> SlateWidget;

	/** Handle list changes from the Slate widget */
	void HandleListChanged(const TArray<FCPM_KeyValuePair>& Pairs);
};

