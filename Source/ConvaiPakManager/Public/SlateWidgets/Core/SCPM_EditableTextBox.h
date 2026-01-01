// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/**
 * A styled editable text box with consistent CPM styling.
 * Supports hint text, validation, and change callbacks.
 */
class CONVAIPAKMANAGER_API SCPM_EditableTextBox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCPM_EditableTextBox)
		: _Text()
		, _HintText()
		, _IsPassword(false)
		, _IsReadOnly(false)
	{}
		/** The text to display */
		SLATE_ATTRIBUTE(FText, Text)
		
		/** Hint text shown when the text box is empty */
		SLATE_ATTRIBUTE(FText, HintText)
		
		/** Whether to display as password (masked) */
		SLATE_ARGUMENT(bool, IsPassword)
		
		/** Whether the text box is read-only */
		SLATE_ARGUMENT(bool, IsReadOnly)
		
		/** Called when the text changes */
		SLATE_EVENT(FOnTextChanged, OnTextChanged)
		
		/** Called when the text is committed (Enter pressed or focus lost) */
		SLATE_EVENT(FOnTextCommitted, OnTextCommitted)
		
	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs);

	/** Get the current text */
	FText GetText() const;

	/** Set the text programmatically */
	void SetText(const FText& InText);

	/** Set the hint text */
	void SetHintText(const FText& InHintText);

	/** Set whether the text box is read-only */
	void SetIsReadOnly(bool bInIsReadOnly);

	/** Set focus to this text box */
	void SetFocus();

	/** Clear the text */
	void ClearText();

private:
	/** The actual editable text box widget */
	TSharedPtr<class SEditableTextBox> TextBox;
	
	/** Stored callbacks */
	FOnTextChanged OnTextChangedCallback;
	FOnTextCommitted OnTextCommittedCallback;

	/** Internal handler for text changes */
	void HandleTextChanged(const FText& NewText);

	/** Internal handler for text commits */
	void HandleTextCommitted(const FText& NewText, ETextCommit::Type CommitType);
};

