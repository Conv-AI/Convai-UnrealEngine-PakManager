// Copyright 2022 Convai Inc. All Rights Reserved.

#include "SlateWidgets/Core/SCPM_IconButton.h"
#include "SlateWidgets/CPM_SlateStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"

void SCPM_IconButton::Construct(const FArguments& InArgs)
{
	CurrentStyle = InArgs._ButtonStyle;
	OnClickedCallback = InArgs._OnClicked;

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(CPMStyle::IconButtonSize())
		.HeightOverride(CPMStyle::IconButtonSize())
		[
			SAssignNew(Button, SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.OnClicked(this, &SCPM_IconButton::HandleClicked)
			.IsEnabled(InArgs._IsEnabled)
			.ToolTipText(InArgs._ToolTipText)
			.ContentPadding(FMargin(0))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(InArgs._IconText)
				.Font(CPMStyle::BodyFont())
				.ColorAndOpacity(this, &SCPM_IconButton::GetIconColor)
				.Justification(ETextJustify::Center)
			]
		]
	];
}

void SCPM_IconButton::SetEnabled(bool bInEnabled)
{
	if (Button.IsValid())
	{
		Button->SetEnabled(bInEnabled);
	}
}

FSlateColor SCPM_IconButton::GetIconColor() const
{
	// Check if button is hovered for visual feedback
	const bool bIsHovered = Button.IsValid() && Button->IsHovered();
	
	switch (CurrentStyle)
	{
	case ECPM_ButtonStyle::Danger:
		return bIsHovered ? FSlateColor(CPMStyle::DangerHover()) : FSlateColor(CPMStyle::Danger());
	case ECPM_ButtonStyle::Primary:
		return bIsHovered ? FSlateColor(CPMStyle::PrimaryHover()) : FSlateColor(CPMStyle::Primary());
	case ECPM_ButtonStyle::Ghost:
	case ECPM_ButtonStyle::Secondary:
	default:
		return bIsHovered ? FSlateColor(CPMStyle::TextColor()) : FSlateColor(CPMStyle::HintTextColor());
	}
}

FReply SCPM_IconButton::HandleClicked()
{
	if (OnClickedCallback.IsBound())
	{
		return OnClickedCallback.Execute();
	}
	return FReply::Handled();
}

