// Copyright 2022 Convai Inc. All Rights Reserved.

#include "SlateWidgets/Composite/SCPM_KeyValueRow.h"
#include "SlateWidgets/Core/SCPM_EditableTextBox.h"
#include "SlateWidgets/Core/SCPM_IconButton.h"
#include "SlateWidgets/CPM_SlateStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

void SCPM_KeyValueRow::Construct(const FArguments& InArgs)
{
	RowIndex = InArgs._RowIndex;
	CurrentKey = InArgs._Key;
	CurrentValue = InArgs._Value;
	OnRemoveRequestedCallback = InArgs._OnRemoveRequested;
	OnKeyValueChangedCallback = InArgs._OnKeyValueChanged;

	ChildSlot
	[
		SNew(SHorizontalBox)
		
		// Key Input
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(0, 0, 4, 0)
		[
			SAssignNew(KeyInput, SCPM_EditableTextBox)
			.Text(FText::FromString(CurrentKey))
			.HintText(InArgs._KeyHintText)
			.OnTextChanged(this, &SCPM_KeyValueRow::HandleKeyChanged)
		]
		
		// Value Input
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(4, 0, 4, 0)
		[
			SAssignNew(ValueInput, SCPM_EditableTextBox)
			.Text(FText::FromString(CurrentValue))
			.HintText(InArgs._ValueHintText)
			.OnTextChanged(this, &SCPM_KeyValueRow::HandleValueChanged)
		]
		
		// Remove Button (conditionally shown)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4, 0, 0, 0)
		[
			SNew(SBox)
			.Visibility(InArgs._ShowRemoveButton ? EVisibility::Visible : EVisibility::Collapsed)
			[
				SNew(SCPM_IconButton)
				.IconText(SCPM_IconButton::GetRemoveIcon())
				.ButtonStyle(ECPM_ButtonStyle::Danger)
				.ToolTipText(NSLOCTEXT("CPM", "RemoveRowTooltip", "Remove this entry"))
				.OnClicked(this, &SCPM_KeyValueRow::HandleRemoveClicked)
			]
		]
	];
}

FString SCPM_KeyValueRow::GetKey() const
{
	if (KeyInput.IsValid())
	{
		return KeyInput->GetText().ToString();
	}
	return CurrentKey;
}

FString SCPM_KeyValueRow::GetValue() const
{
	if (ValueInput.IsValid())
	{
		return ValueInput->GetText().ToString();
	}
	return CurrentValue;
}

FCPM_KeyValuePair SCPM_KeyValueRow::GetKeyValuePair() const
{
	return FCPM_KeyValuePair(GetKey(), GetValue());
}

void SCPM_KeyValueRow::SetKey(const FString& InKey)
{
	CurrentKey = InKey;
	if (KeyInput.IsValid())
	{
		KeyInput->SetText(FText::FromString(InKey));
	}
}

void SCPM_KeyValueRow::SetValue(const FString& InValue)
{
	CurrentValue = InValue;
	if (ValueInput.IsValid())
	{
		ValueInput->SetText(FText::FromString(InValue));
	}
}

void SCPM_KeyValueRow::SetKeyValuePair(const FCPM_KeyValuePair& InPair)
{
	SetKey(InPair.Key);
	SetValue(InPair.Value);
}

void SCPM_KeyValueRow::HandleKeyChanged(const FText& NewText)
{
	CurrentKey = NewText.ToString();
	NotifyChanged();
}

void SCPM_KeyValueRow::HandleValueChanged(const FText& NewText)
{
	CurrentValue = NewText.ToString();
	NotifyChanged();
}

FReply SCPM_KeyValueRow::HandleRemoveClicked()
{
	OnRemoveRequestedCallback.ExecuteIfBound(RowIndex);
	return FReply::Handled();
}

void SCPM_KeyValueRow::NotifyChanged()
{
	OnKeyValueChangedCallback.ExecuteIfBound(RowIndex, CurrentKey, CurrentValue);
}

