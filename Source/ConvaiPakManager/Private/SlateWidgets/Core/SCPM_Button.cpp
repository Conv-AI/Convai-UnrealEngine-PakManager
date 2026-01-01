// Copyright 2022 Convai Inc. All Rights Reserved.

#include "SlateWidgets/Core/SCPM_Button.h"
#include "SlateWidgets/CPM_SlateStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"

void SCPM_Button::Construct(const FArguments& InArgs)
{
	CurrentStyle = InArgs._ButtonStyle;
	OnClickedCallback = InArgs._OnClicked;

	// Create button content - either custom content or text
	TSharedRef<SWidget> ButtonContent = InArgs._Content.Widget;
	
	if (ButtonContent == SNullWidget::NullWidget)
	{
		// No custom content, use text
		SAssignNew(TextBlock, STextBlock)
			.Text(InArgs._Text)
			.Font(CPMStyle::ButtonFont())
			.ColorAndOpacity(GetTextColor())
			.Justification(ETextJustify::Center);
			
		ButtonContent = TextBlock.ToSharedRef();
	}

	// Use static styles to ensure they persist beyond this function scope
	static FButtonStyle PrimaryStyle = CPMStyle::GetButtonStyle(true);
	static FButtonStyle SecondaryStyle = CPMStyle::GetButtonStyle(false);
	
	const FButtonStyle* StylePtr = (CurrentStyle == ECPM_ButtonStyle::Primary) ? &PrimaryStyle : &SecondaryStyle;

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredHeight(CPMStyle::ButtonHeight())
		[
			SAssignNew(Button, SButton)
			.ButtonStyle(StylePtr)
			.OnClicked(this, &SCPM_Button::HandleClicked)
			.IsEnabled(InArgs._IsEnabled)
			.ContentPadding(CPMStyle::ButtonPadding())
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				ButtonContent
			]
		]
	];
}

void SCPM_Button::SetText(const FText& InText)
{
	if (TextBlock.IsValid())
	{
		TextBlock->SetText(InText);
	}
}

void SCPM_Button::SetEnabled(bool bInEnabled)
{
	if (Button.IsValid())
	{
		Button->SetEnabled(bInEnabled);
	}
}

FLinearColor SCPM_Button::GetNormalColor() const
{
	switch (CurrentStyle)
	{
	case ECPM_ButtonStyle::Primary:
		return CPMStyle::Primary();
	case ECPM_ButtonStyle::Secondary:
		return CPMStyle::Secondary();
	case ECPM_ButtonStyle::Danger:
		return CPMStyle::Danger();
	case ECPM_ButtonStyle::Ghost:
		return FLinearColor::Transparent;
	default:
		return CPMStyle::Primary();
	}
}

FLinearColor SCPM_Button::GetHoverColor() const
{
	switch (CurrentStyle)
	{
	case ECPM_ButtonStyle::Primary:
		return CPMStyle::PrimaryHover();
	case ECPM_ButtonStyle::Secondary:
		return CPMStyle::SecondaryHover();
	case ECPM_ButtonStyle::Danger:
		return CPMStyle::DangerHover();
	case ECPM_ButtonStyle::Ghost:
		return CPMStyle::Secondary();
	default:
		return CPMStyle::PrimaryHover();
	}
}

FLinearColor SCPM_Button::GetPressedColor() const
{
	switch (CurrentStyle)
	{
	case ECPM_ButtonStyle::Primary:
		return CPMStyle::PrimaryPressed();
	case ECPM_ButtonStyle::Secondary:
		return CPMStyle::Secondary() * 0.8f;
	case ECPM_ButtonStyle::Danger:
		return CPMStyle::Danger() * 0.8f;
	case ECPM_ButtonStyle::Ghost:
		return CPMStyle::Secondary() * 0.8f;
	default:
		return CPMStyle::PrimaryPressed();
	}
}

FLinearColor SCPM_Button::GetTextColor() const
{
	if (CurrentStyle == ECPM_ButtonStyle::Ghost)
	{
		return CPMStyle::TextColor();
	}
	return CPMStyle::ButtonTextColor();
}

FReply SCPM_Button::HandleClicked()
{
	if (OnClickedCallback.IsBound())
	{
		return OnClickedCallback.Execute();
	}
	return FReply::Handled();
}

