// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Types/CPM_WidgetTypes.h"

DECLARE_DELEGATE_OneParam(FOnKeyValueListChanged, const TArray<FCPM_KeyValuePair>& /*Pairs*/);

/**
 * A complete key-value pair list widget with add/remove functionality.
 * Contains a scrollable list of KeyValueRows and an Add button.
 */
class CONVAIPAKMANAGER_API SCPM_KeyValueList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCPM_KeyValueList)
		: _KeyHintText(NSLOCTEXT("CPM", "KeyHint", "Key"))
		, _ValueHintText(NSLOCTEXT("CPM", "ValueHint", "Value"))
		, _AddButtonText(NSLOCTEXT("CPM", "AddPair", "+ Add New Pair"))
		, _ShowAddButton(true)
		, _MaxHeight(300.0f)
	{}
		/** Initial pairs to populate */
		SLATE_ARGUMENT(TArray<FCPM_KeyValuePair>, InitialPairs)
		
		/** Hint text for key inputs */
		SLATE_ARGUMENT(FText, KeyHintText)
		
		/** Hint text for value inputs */
		SLATE_ARGUMENT(FText, ValueHintText)
		
		/** Text for the add button */
		SLATE_ARGUMENT(FText, AddButtonText)
		
		/** Whether to show the add button */
		SLATE_ARGUMENT(bool, ShowAddButton)
		
		/** Maximum height before scrolling */
		SLATE_ARGUMENT(float, MaxHeight)
		
		/** Called when the list changes (add/remove/edit) */
		SLATE_EVENT(FOnKeyValueListChanged, OnListChanged)
		
	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs);

	//~ Basic Getters

	/** Get all key-value pairs */
	TArray<FCPM_KeyValuePair> GetAllPairs() const;

	/** Get only non-empty pairs (where key is not empty) */
	TArray<FCPM_KeyValuePair> GetValidPairs() const;

	/** Get the number of pairs */
	int32 GetPairCount() const;

	/** Convert pairs to a TMap for easy lookup (only valid pairs) */
	TMap<FString, FString> GetPairsAsMap() const;

	//~ Basic Setters

	/** Set all pairs (replaces existing) */
	void SetPairs(const TArray<FCPM_KeyValuePair>& InPairs);

	/** Add a new pair */
	void AddPair(const FCPM_KeyValuePair& InPair = FCPM_KeyValuePair());

	/** Remove a pair at the specified index */
	void RemovePairAt(int32 Index);

	/** Clear all pairs */
	void ClearAllPairs();

	//~ Index-Based Access

	/** Get pair at the specified index. Returns empty pair if invalid. */
	FCPM_KeyValuePair GetPairByIndex(int32 Index) const;

	/** Set pair at the specified index. Returns true if successful. */
	bool SetPairByIndex(int32 Index, const FCPM_KeyValuePair& InPair);

	//~ Key-Based Lookup

	/** Check if a key exists in the list */
	bool HasKey(const FString& Key) const;

	/** Find the index of a key. Returns INDEX_NONE (-1) if not found. */
	int32 FindKeyIndex(const FString& Key) const;

	/** Get the value for a specific key. Returns empty string if not found. */
	FString GetValueForKey(const FString& Key) const;

	/** Get the full pair for a specific key. Returns empty pair if not found. */
	FCPM_KeyValuePair GetPairByKey(const FString& Key) const;

	/** Set/update the value for a specific key. Returns true if key was found. */
	bool SetValueForKey(const FString& Key, const FString& NewValue);

	/** Set/update the full pair for a specific key. Returns true if key was found. */
	bool SetPairByKey(const FString& Key, const FCPM_KeyValuePair& InPair);

	/** Remove the pair with the specified key. Returns true if found and removed. */
	bool RemoveByKey(const FString& Key);

	/** Add or update a pair. If key exists, updates the pair. If not, adds new pair. */
	void AddOrUpdatePair(const FCPM_KeyValuePair& InPair);

	//~ JSON Export

	/** 
	 * Get pairs as a JSON object { key1: value1, key2: value2 }
	 * @param KeysToIgnore - Array of keys to exclude from the output
	 * @return JSON object containing key-value pairs (skips empty values)
	 */
	TSharedPtr<FJsonObject> GetPairsAsJsonObject(const TArray<FString>& KeysToIgnore = TArray<FString>()) const;

	/**
	 * Get pairs as a JSON string { "key1": "value1", "key2": "value2" }
	 * @param KeysToIgnore - Array of keys to exclude from the output
	 * @return JSON string containing key-value pairs (skips empty values)
	 */
	FString GetPairsAsJsonString(const TArray<FString>& KeysToIgnore = TArray<FString>()) const;

private:
	/** Container for row widgets */
	TSharedPtr<class SVerticalBox> RowsContainer;
	
	/** Scroll box for the rows */
	TSharedPtr<class SScrollBox> ScrollBox;
	
	/** Array of row widgets */
	TArray<TSharedPtr<class SCPM_KeyValueRow>> RowWidgets;
	
	/** Stored arguments for creating new rows */
	FText KeyHintText;
	FText ValueHintText;
	
	/** Callback for list changes */
	FOnKeyValueListChanged OnListChangedCallback;

	/** Create a new row widget */
	TSharedRef<class SCPM_KeyValueRow> CreateRowWidget(int32 Index, const FCPM_KeyValuePair& Pair);

	/** Rebuild the rows container */
	void RebuildRows();

	/** Update row indices after add/remove */
	void UpdateRowIndices();

	/** Handle row removal request */
	void HandleRowRemoveRequested(int32 RowIndex);

	/** Handle row value change */
	void HandleRowValueChanged(int32 RowIndex, const FString& Key, const FString& Value) const;

	/** Handle add button click */
	FReply HandleAddClicked();

	/** Notify listeners of list changes */
	void NotifyListChanged() const;
};

