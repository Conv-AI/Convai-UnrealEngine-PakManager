// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

DECLARE_DELEGATE_OneParam(FOnCPMComboBoxSelectionChanged, const FString& /*SelectedOption*/);

/**
 * A styled combo box (dropdown) with consistent CPM styling.
 * Allows selection from a list of predefined string options.
 */
class CONVAIPAKMANAGER_API SCPM_ComboBox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCPM_ComboBox)
		: _Options()
		, _SelectedOption()
	{}
		/** The list of options to display */
		SLATE_ARGUMENT(TArray<FString>, Options)
		
		/** The initially selected option */
		SLATE_ARGUMENT(FString, SelectedOption)
		
		/** Called when the selection changes */
		SLATE_EVENT(FOnCPMComboBoxSelectionChanged, OnSelectionChanged)
		
	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs);

	/** Get the currently selected option */
	FString GetSelectedOption() const;

	/** Set the selected option */
	void SetSelectedOption(const FString& InOption);

	/** Set the available options */
	void SetOptions(const TArray<FString>& InOptions);

	/** Get the available options */
	const TArray<FString>& GetOptions() const { return Options; }

private:
	/** The combo box widget */
	TSharedPtr<class STextComboBox> ComboBox;
	
	/** Available options */
	TArray<FString> Options;
	
	/** Options as shared pointers (required by STextComboBox) */
	TArray<TSharedPtr<FString>> OptionsSource;
	
	/** Currently selected item */
	TSharedPtr<FString> SelectedItem;
	
	/** Selection changed callback */
	FOnCPMComboBoxSelectionChanged OnSelectionChangedCallback;

	/** Rebuild options source from Options array */
	void RebuildOptionsSource();

	/** Handle selection change */
	void HandleSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

	/** Generate row widget for dropdown */
	TSharedRef<SWidget> GenerateOptionWidget(TSharedPtr<FString> InOption);
};

