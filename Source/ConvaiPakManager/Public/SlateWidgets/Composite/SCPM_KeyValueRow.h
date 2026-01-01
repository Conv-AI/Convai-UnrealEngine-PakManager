// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Types/CPM_WidgetTypes.h"

DECLARE_DELEGATE_OneParam(FOnKeyValueRowRemoveRequested, int32 /*RowIndex*/);
DECLARE_DELEGATE_ThreeParams(FOnKeyValueRowChanged, int32 /*RowIndex*/, const FString& /*Key*/, const FString& /*Value*/);

/**
 * A single row containing Key input, Value input, and a Remove button.
 * Used as a building block for the KeyValueList.
 */
class CONVAIPAKMANAGER_API SCPM_KeyValueRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCPM_KeyValueRow)
		: _RowIndex(0)
		, _Key()
		, _Value()
		, _KeyHintText(NSLOCTEXT("CPM", "KeyHint", "Key"))
		, _ValueHintText(NSLOCTEXT("CPM", "ValueHint", "Value"))
		, _ShowRemoveButton(true)
	{}
		/** The index of this row in the list */
		SLATE_ARGUMENT(int32, RowIndex)
		
		/** Initial key value */
		SLATE_ARGUMENT(FString, Key)
		
		/** Initial value */
		SLATE_ARGUMENT(FString, Value)
		
		/** Hint text for the key input */
		SLATE_ARGUMENT(FText, KeyHintText)
		
		/** Hint text for the value input */
		SLATE_ARGUMENT(FText, ValueHintText)
		
		/** Whether to show the remove button */
		SLATE_ARGUMENT(bool, ShowRemoveButton)
		
		/** Called when the remove button is clicked */
		SLATE_EVENT(FOnKeyValueRowRemoveRequested, OnRemoveRequested)
		
		/** Called when key or value changes */
		SLATE_EVENT(FOnKeyValueRowChanged, OnKeyValueChanged)
		
	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs);

	/** Get the current key */
	FString GetKey() const;

	/** Get the current value */
	FString GetValue() const;

	/** Get both key and value as a pair */
	FCPM_KeyValuePair GetKeyValuePair() const;

	/** Set the key */
	void SetKey(const FString& InKey);

	/** Set the value */
	void SetValue(const FString& InValue);

	/** Set both key and value */
	void SetKeyValuePair(const FCPM_KeyValuePair& InPair);

	/** Get the row index */
	int32 GetRowIndex() const { return RowIndex; }

	/** Set the row index (used when reordering) */
	void SetRowIndex(int32 InRowIndex) { RowIndex = InRowIndex; }

private:
	/** The key input text box */
	TSharedPtr<class SCPM_EditableTextBox> KeyInput;
	
	/** The value input text box */
	TSharedPtr<class SCPM_EditableTextBox> ValueInput;
	
	/** Row index in the parent list */
	int32 RowIndex;
	
	/** Stored key value (for tracking changes) */
	FString CurrentKey;
	
	/** Stored value (for tracking changes) */
	FString CurrentValue;
	
	/** Callbacks */
	FOnKeyValueRowRemoveRequested OnRemoveRequestedCallback;
	FOnKeyValueRowChanged OnKeyValueChangedCallback;

	/** Handle key text change */
	void HandleKeyChanged(const FText& NewText);

	/** Handle value text change */
	void HandleValueChanged(const FText& NewText);

	/** Handle remove button click */
	FReply HandleRemoveClicked();

	/** Notify parent of changes */
	void NotifyChanged();
};

