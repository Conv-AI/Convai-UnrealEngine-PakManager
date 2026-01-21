// Copyright 2022 Convai Inc. All Rights Reserved.

#include "SlateWidgets/Core/SCPM_Label.h"
#include "SlateWidgets/CPM_SlateStyle.h"
#include "Widgets/Text/STextBlock.h"

void SCPM_Label::Construct(const FArguments& InArgs)
{
	CurrentStyle = InArgs._TextStyle;
	ColorOverride = InArgs._ColorOverride;

	ChildSlot
	[
		SAssignNew(TextBlock, STextBlock)
		.Text(InArgs._Text)
		.Font(GetFont())
		.ColorAndOpacity(this, &SCPM_Label::GetColor)
		.Justification(InArgs._Justification)
	];
}

void SCPM_Label::SetText(const FText& InText)
{
	if (TextBlock.IsValid())
	{
		TextBlock->SetText(InText);
	}
}

void SCPM_Label::SetColor(const FLinearColor& InColor)
{
	ColorOverride = InColor;
}

FSlateFontInfo SCPM_Label::GetFont() const
{
	switch (CurrentStyle)
	{
	case ECPM_TextStyle::Header:
		return CPMStyle::HeaderFont();
	case ECPM_TextStyle::Caption:
		return CPMStyle::CaptionFont();
	case ECPM_TextStyle::Button:
		return CPMStyle::ButtonFont();
	case ECPM_TextStyle::Body:
	default:
		return CPMStyle::BodyFont();
	}
}

FSlateColor SCPM_Label::GetColor() const
{
	if (ColorOverride.IsSet())
	{
		return FSlateColor(ColorOverride.GetValue());
	}
	
	switch (CurrentStyle)
	{
	case ECPM_TextStyle::Caption:
		return FSlateColor(CPMStyle::HintTextColor());
	case ECPM_TextStyle::Header:
	case ECPM_TextStyle::Body:
	case ECPM_TextStyle::Button:
	default:
		return FSlateColor(CPMStyle::TextColor());
	}
}

