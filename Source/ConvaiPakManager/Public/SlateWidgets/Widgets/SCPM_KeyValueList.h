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

	/** Get all key-value pairs */
	TArray<FCPM_KeyValuePair> GetAllPairs() const;

	/** Get only non-empty pairs (where key is not empty) */
	TArray<FCPM_KeyValuePair> GetValidPairs() const;

	/** Set all pairs (replaces existing) */
	void SetPairs(const TArray<FCPM_KeyValuePair>& InPairs);

	/** Add a new pair */
	void AddPair(const FCPM_KeyValuePair& InPair = FCPM_KeyValuePair());

	/** Remove a pair at the specified index */
	void RemovePairAt(int32 Index);

	/** Clear all pairs */
	void ClearAllPairs();

	/** Get the number of pairs */
	int32 GetPairCount() const;

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
	void HandleRowValueChanged(int32 RowIndex, const FString& Key, const FString& Value);

	/** Handle add button click */
	FReply HandleAddClicked();

	/** Notify listeners of list changes */
	void NotifyListChanged();
};

