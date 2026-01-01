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

	//~ Blueprint Functions

	/** Get all key-value pairs (including empty ones) */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	TArray<FCPM_KeyValuePair> GetAllPairs() const;

	/** Get only valid pairs (where key is not empty) */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	TArray<FCPM_KeyValuePair> GetValidPairs() const;

	/** Set all pairs (replaces existing) */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	void SetPairs(const TArray<FCPM_KeyValuePair>& InPairs);

	/** Add a new empty pair */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	void AddEmptyPair();

	/** Add a pair using the full struct (allows setting all control options like locked keys, dropdowns, etc.) */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	void AddPair(const FCPM_KeyValuePair& Pair);

	/** Remove the pair at the specified index */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	void RemovePairAt(int32 Index);

	/** Clear all pairs */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	void ClearAllPairs();

	/** Get the number of pairs */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	int32 GetPairCount() const;

	/** Convert pairs to a TMap for easy lookup */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	TMap<FString, FString> GetPairsAsMap() const;

	/** Set pairs from a TMap */
	UFUNCTION(BlueprintCallable, Category = "Convai|PakManager|KeyValuePairWidget")
	void SetPairsFromMap(const TMap<FString, FString>& InMap);

protected:
	//~ UWidget Interface
	virtual void SynchronizeProperties() override;

private:
	/** The underlying Slate widget */
	TSharedPtr<class SCPM_KeyValueList> SlateWidget;

	/** Handle list changes from the Slate widget */
	void HandleListChanged(const TArray<FCPM_KeyValuePair>& Pairs);
};

