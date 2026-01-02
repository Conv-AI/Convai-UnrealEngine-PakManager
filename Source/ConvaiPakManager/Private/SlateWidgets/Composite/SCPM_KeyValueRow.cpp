// Copyright 2022 Convai Inc. All Rights Reserved.

#include "SlateWidgets/Composite/SCPM_KeyValueRow.h"
#include "SlateWidgets/Core/SCPM_EditableTextBox.h"
#include "SlateWidgets/Core/SCPM_IconButton.h"
#include "SlateWidgets/Core/SCPM_ComboBox.h"
#include "SlateWidgets/CPM_SlateStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

void SCPM_KeyValueRow::Construct(const FArguments& InArgs)
{
	RowIndex = InArgs._RowIndex;
	CurrentPair = InArgs._Pair;
	OnRemoveRequestedCallback = InArgs._OnRemoveRequested;
	OnKeyValueChangedCallback = InArgs._OnKeyValueChanged;

	// Determine if remove button should be visible
	const bool bShowRemove = InArgs._ShowRemoveButton && !CurrentPair.bCannotRemove;

	// Create the value widget - either text input or dropdown
	TSharedPtr<SWidget> ValueWidget;
	
	if (CurrentPair.bUseDropdownForValue && CurrentPair.ValueOptions.Num() > 0)
	{
		// Use dropdown for value
		SAssignNew(ValueComboBox, SCPM_ComboBox)
			.Options(CurrentPair.ValueOptions)
			.SelectedOption(CurrentPair.Value)
			.IsEnabled(!CurrentPair.bValueReadOnly)
			.OnSelectionChanged(this, &SCPM_KeyValueRow::HandleValueDropdownChanged);
		
		ValueWidget = ValueComboBox;
	}
	else
	{
		// Use text input for value
		SAssignNew(ValueInput, SCPM_EditableTextBox)
			.Text(FText::FromString(CurrentPair.Value))
			.HintText(InArgs._ValueHintText)
			.IsReadOnly(CurrentPair.bValueReadOnly)
			.OnTextChanged(this, &SCPM_KeyValueRow::HandleValueChanged);
		
		ValueWidget = ValueInput;
	}

	ChildSlot
	[
		SNew(SHorizontalBox)
		
		// Key Input
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(0, 0, 4, 0)
		[
			SAssignNew(KeyInput, SCPM_EditableTextBox)
			.Text(FText::FromString(CurrentPair.Key))
			.HintText(InArgs._KeyHintText)
			.IsReadOnly(CurrentPair.bKeyReadOnly)
			.OnTextChanged(this, &SCPM_KeyValueRow::HandleKeyChanged)
		]
		
		// Value Widget (text input or dropdown)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(4, 0, 4, 0)
		[
			ValueWidget.ToSharedRef()
		]
		
		// Remove Button (conditionally shown)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4, 0, 0, 0)
		[
			SNew(SBox)
			.Visibility(bShowRemove ? EVisibility::Visible : EVisibility::Collapsed)
			[
				SNew(SCPM_IconButton)
				.IconText(SCPM_IconButton::GetRemoveIcon())
				.ButtonStyle(ECPM_ButtonStyle::Danger)
				.ToolTipText(NSLOCTEXT("CPM", "RemoveRowTooltip", "Remove this entry"))
				.OnClicked(this, &SCPM_KeyValueRow::HandleRemoveClicked)
			]
		]
	];

	// Set initial visibility based on hidden flag
	SetVisibility(CurrentPair.bIsHidden ? EVisibility::Collapsed : EVisibility::Visible);
}

FString SCPM_KeyValueRow::GetKey() const
{
	if (KeyInput.IsValid())
	{
		return KeyInput->GetText().ToString();
	}
	return CurrentPair.Key;
}

FString SCPM_KeyValueRow::GetValue() const
{
	// Check if using dropdown
	if (ValueComboBox.IsValid())
	{
		return ValueComboBox->GetSelectedOption();
	}
	
	// Using text input
	if (ValueInput.IsValid())
	{
		return ValueInput->GetText().ToString();
	}
	
	return CurrentPair.Value;
}

FCPM_KeyValuePair SCPM_KeyValueRow::GetKeyValuePair() const
{
	// Create a copy and update key/value while preserving control flags
	FCPM_KeyValuePair Result = CurrentPair;
	Result.Key = GetKey();
	Result.Value = GetValue();
	return Result;
}

void SCPM_KeyValueRow::SetKey(const FString& InKey)
{
	CurrentPair.Key = InKey;
	if (KeyInput.IsValid())
	{
		KeyInput->SetText(FText::FromString(InKey));
	}
}

void SCPM_KeyValueRow::SetValue(const FString& InValue)
{
	CurrentPair.Value = InValue;
	
	if (ValueComboBox.IsValid())
	{
		ValueComboBox->SetSelectedOption(InValue);
	}
	else if (ValueInput.IsValid())
	{
		ValueInput->SetText(FText::FromString(InValue));
	}
}

void SCPM_KeyValueRow::SetKeyValuePair(const FCPM_KeyValuePair& InPair)
{
	// Update key and value
	SetKey(InPair.Key);
	SetValue(InPair.Value);
	
	// Update visibility if changed
	if (CurrentPair.bIsHidden != InPair.bIsHidden)
	{
		CurrentPair.bIsHidden = InPair.bIsHidden;
		SetIsHidden(InPair.bIsHidden);
	}
}

void SCPM_KeyValueRow::SetIsHidden(bool bHidden)
{
	CurrentPair.bIsHidden = bHidden;
	SetVisibility(bHidden ? EVisibility::Collapsed : EVisibility::Visible);
}

void SCPM_KeyValueRow::HandleKeyChanged(const FText& NewText)
{
	CurrentPair.Key = NewText.ToString();
	NotifyChanged();
}

void SCPM_KeyValueRow::HandleValueChanged(const FText& NewText)
{
	CurrentPair.Value = NewText.ToString();
	NotifyChanged();
}

void SCPM_KeyValueRow::HandleValueDropdownChanged(const FString& NewValue)
{
	CurrentPair.Value = NewValue;
	NotifyChanged();
}

FReply SCPM_KeyValueRow::HandleRemoveClicked()
{
	OnRemoveRequestedCallback.ExecuteIfBound(RowIndex);
	return FReply::Handled();
}

void SCPM_KeyValueRow::NotifyChanged()
{
	OnKeyValueChangedCallback.ExecuteIfBound(RowIndex, CurrentPair.Key, CurrentPair.Value);
}
