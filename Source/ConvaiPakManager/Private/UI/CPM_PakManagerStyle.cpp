// Copyright 2025 Convai Inc. All Rights Reserved.

#include "UI/CPM_PakManagerStyle.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"

const FLinearColor FCPM_PakManagerStyle::FPalette::Canvas = FLinearColor(FColor::FromHex(TEXT("#111512")));
const FLinearColor FCPM_PakManagerStyle::FPalette::Panel = FLinearColor(FColor::FromHex(TEXT("#1A211C")));
const FLinearColor FCPM_PakManagerStyle::FPalette::Hover = FLinearColor(FColor::FromHex(TEXT("#263229")));
const FLinearColor FCPM_PakManagerStyle::FPalette::Border = FLinearColor(FColor::FromHex(TEXT("#344139")));
const FLinearColor FCPM_PakManagerStyle::FPalette::TextPrimary = FLinearColor(FColor::FromHex(TEXT("#F1F5F1")));
const FLinearColor FCPM_PakManagerStyle::FPalette::TextSecondary = FLinearColor(FColor::FromHex(TEXT("#AEB8AF")));
const FLinearColor FCPM_PakManagerStyle::FPalette::GreenPrimary = FLinearColor(FColor::FromHex(TEXT("#2FAE62")));
const FLinearColor FCPM_PakManagerStyle::FPalette::GreenBright = FLinearColor(FColor::FromHex(TEXT("#78D99B")));
const FLinearColor FCPM_PakManagerStyle::FPalette::GreenDeep = FLinearColor(FColor::FromHex(TEXT("#165C34")));
const FLinearColor FCPM_PakManagerStyle::FPalette::Warning = FLinearColor(FColor::FromHex(TEXT("#DCA94A")));
const FLinearColor FCPM_PakManagerStyle::FPalette::Error = FLinearColor(FColor::FromHex(TEXT("#E26060")));

TSharedPtr<FSlateStyleSet> FCPM_PakManagerStyle::StyleInstance;

void FCPM_PakManagerStyle::Initialize()
{
	if (StyleInstance.IsValid())
	{
		return;
	}
	StyleInstance = Create();
	FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
}

void FCPM_PakManagerStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		StyleInstance.Reset();
	}
}

const ISlateStyle& FCPM_PakManagerStyle::Get()
{
	return *StyleInstance;
}

FName FCPM_PakManagerStyle::GetStyleSetName()
{
	static const FName StyleSetName(TEXT("CPM_PakManagerStyle"));
	return StyleSetName;
}

TSharedRef<FSlateStyleSet> FCPM_PakManagerStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShared<FSlateStyleSet>(GetStyleSetName());

	Style->Set("CPM.Canvas", new FSlateColorBrush(FPalette::Canvas));
	Style->Set("CPM.Panel", new FSlateColorBrush(FPalette::Panel));
	Style->Set("CPM.Hover", new FSlateColorBrush(FPalette::Hover));
	Style->Set("CPM.Border", new FSlateColorBrush(FPalette::Border));
	Style->Set("CPM.ThumbnailFrame", new FSlateRoundedBoxBrush(FPalette::Panel, 4.0f, FPalette::Border, 1.0f));

	const FButtonStyle& BaseButton = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button");

	// Near-black (Canvas) text on the green fills; the greens carry the emphasis, not the text.
	FButtonStyle PrimaryButton = BaseButton;
	PrimaryButton
		.SetNormal(FSlateRoundedBoxBrush(FPalette::GreenPrimary, 4.0f))
		.SetHovered(FSlateRoundedBoxBrush(FPalette::GreenBright, 4.0f))
		.SetPressed(FSlateRoundedBoxBrush(FPalette::GreenDeep, 4.0f))
		.SetNormalForeground(FPalette::Canvas)
		.SetHoveredForeground(FPalette::Canvas)
		.SetPressedForeground(FPalette::Canvas);
	Style->Set("CPM.Button.Primary", PrimaryButton);

	FButtonStyle SecondaryButton = BaseButton;
	SecondaryButton
		.SetNormal(FSlateRoundedBoxBrush(FPalette::Panel, 4.0f, FPalette::Border, 1.0f))
		.SetHovered(FSlateRoundedBoxBrush(FPalette::Hover, 4.0f, FPalette::Border, 1.0f))
		.SetPressed(FSlateRoundedBoxBrush(FPalette::Hover, 4.0f, FPalette::Border, 1.0f))
		.SetNormalForeground(FPalette::TextPrimary)
		.SetHoveredForeground(FPalette::TextPrimary)
		.SetPressedForeground(FPalette::TextPrimary);
	Style->Set("CPM.Button.Secondary", SecondaryButton);

	FButtonStyle DangerButton = BaseButton;
	DangerButton
		.SetNormal(FSlateRoundedBoxBrush(FPalette::Panel, 4.0f, FPalette::Error, 1.0f))
		.SetHovered(FSlateRoundedBoxBrush(FPalette::Hover, 4.0f, FPalette::Error, 1.0f))
		.SetPressed(FSlateRoundedBoxBrush(FPalette::Hover, 4.0f, FPalette::Error, 1.0f))
		.SetNormalForeground(FPalette::Error)
		.SetHoveredForeground(FPalette::Error)
		.SetPressedForeground(FPalette::Error);
	Style->Set("CPM.Button.Danger", DangerButton);

	const FTextBlockStyle& BaseText = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");

	FTextBlockStyle TitleText = BaseText;
	TitleText.SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 13)).SetColorAndOpacity(FPalette::TextPrimary);
	Style->Set("CPM.Text.Title", TitleText);

	FTextBlockStyle SectionText = BaseText;
	SectionText.SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 10)).SetColorAndOpacity(FPalette::TextPrimary);
	Style->Set("CPM.Text.Section", SectionText);

	FTextBlockStyle SecondaryText = BaseText;
	SecondaryText.SetColorAndOpacity(FPalette::TextSecondary);
	Style->Set("CPM.Text.Secondary", SecondaryText);

	FTextBlockStyle HintText = BaseText;
	HintText.SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 9)).SetColorAndOpacity(FPalette::TextSecondary);
	Style->Set("CPM.Text.Hint", HintText);

	// Borrowed editor icon, tinted to the brand - this set authors no image assets of its own.
	FSlateBrush* TabIcon = new FSlateBrush(*FAppStyle::Get().GetBrush("Icons.Package"));
	TabIcon->TintColor = FSlateColor(FPalette::GreenPrimary);
	Style->Set("CPM.TabIcon", TabIcon);

	return Style;
}
