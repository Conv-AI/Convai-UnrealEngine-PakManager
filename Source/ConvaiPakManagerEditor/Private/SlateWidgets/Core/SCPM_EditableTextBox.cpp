// Copyright 2022 Convai Inc. All Rights Reserved.

#include "SlateWidgets/Core/SCPM_EditableTextBox.h"
#include "SlateWidgets/CPM_SlateStyle.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"

void SCPM_EditableTextBox::Construct(const FArguments& InArgs)
{
	OnTextChangedCallback = InArgs._OnTextChanged;
	OnTextCommittedCallback = InArgs._OnTextCommitted;

	// Use a static style to ensure it persists beyond this function scope
	static FEditableTextBoxStyle BoxStyle = CPMStyle::GetEditableTextBoxStyle();

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredHeight(CPMStyle::InputHeight())
		[
			SAssignNew(TextBox, SEditableTextBox)
			.Style(&BoxStyle)
			.Text(InArgs._Text)
			.HintText(InArgs._HintText)
			.Font(CPMStyle::BodyFont())
			.IsPassword(InArgs._IsPassword)
			.IsReadOnly(InArgs._IsReadOnly)
			.OnTextChanged(this, &SCPM_EditableTextBox::HandleTextChanged)
			.OnTextCommitted(this, &SCPM_EditableTextBox::HandleTextCommitted)
		]
	];
}

FText SCPM_EditableTextBox::GetText() const
{
	if (TextBox.IsValid())
	{
		return TextBox->GetText();
	}
	return FText::GetEmpty();
}

void SCPM_EditableTextBox::SetText(const FText& InText)
{
	if (TextBox.IsValid())
	{
		TextBox->SetText(InText);
	}
}

void SCPM_EditableTextBox::SetHintText(const FText& InHintText)
{
	if (TextBox.IsValid())
	{
		TextBox->SetHintText(InHintText);
	}
}

void SCPM_EditableTextBox::SetIsReadOnly(bool bInIsReadOnly)
{
	if (TextBox.IsValid())
	{
		TextBox->SetIsReadOnly(bInIsReadOnly);
	}
}

void SCPM_EditableTextBox::SetFocus()
{
	if (TextBox.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(TextBox, EFocusCause::SetDirectly);
	}
}

void SCPM_EditableTextBox::ClearText()
{
	SetText(FText::GetEmpty());
}

void SCPM_EditableTextBox::HandleTextChanged(const FText& NewText)
{
	OnTextChangedCallback.ExecuteIfBound(NewText);
}

void SCPM_EditableTextBox::HandleTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	OnTextCommittedCallback.ExecuteIfBound(NewText, CommitType);
}

