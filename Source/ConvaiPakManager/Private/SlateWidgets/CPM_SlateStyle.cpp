// Copyright 2022 Convai Inc. All Rights Reserved.

#include "SlateWidgets/CPM_SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"

namespace CPMStyle
{
	//~ Colors - Modern dark theme inspired by professional UI

	FLinearColor Primary()
	{
		return FLinearColor(0.224f, 0.486f, 0.855f, 1.0f); // #3A7CDA - Professional blue
	}

	FLinearColor PrimaryHover()
	{
		return FLinearColor(0.294f, 0.557f, 0.925f, 1.0f); // Lighter blue on hover
	}

	FLinearColor PrimaryPressed()
	{
		return FLinearColor(0.176f, 0.396f, 0.745f, 1.0f); // Darker blue when pressed
	}

	FLinearColor Secondary()
	{
		return FLinearColor(0.18f, 0.2f, 0.22f, 1.0f); // Dark gray
	}

	FLinearColor SecondaryHover()
	{
		return FLinearColor(0.24f, 0.26f, 0.28f, 1.0f); // Lighter gray on hover
	}

	FLinearColor Danger()
	{
		return FLinearColor(0.835f, 0.263f, 0.263f, 1.0f); // #D54343 - Red
	}

	FLinearColor DangerHover()
	{
		return FLinearColor(0.906f, 0.333f, 0.333f, 1.0f); // Lighter red on hover
	}

	FLinearColor InputBackground()
	{
		return FLinearColor(0.08f, 0.09f, 0.10f, 1.0f); // Very dark input bg
	}

	FLinearColor InputBorder()
	{
		return FLinearColor(0.25f, 0.27f, 0.29f, 1.0f); // Subtle border
	}

	FLinearColor InputBorderFocused()
	{
		return Primary(); // Blue border when focused
	}

	FLinearColor TextColor()
	{
		return FLinearColor(0.92f, 0.93f, 0.94f, 1.0f); // Off-white text
	}

	FLinearColor HintTextColor()
	{
		return FLinearColor(0.5f, 0.52f, 0.54f, 1.0f); // Dim gray for hints
	}

	FLinearColor ButtonTextColor()
	{
		return FLinearColor(1.0f, 1.0f, 1.0f, 1.0f); // Pure white for buttons
	}

	FLinearColor Background()
	{
		return FLinearColor(0.12f, 0.13f, 0.14f, 1.0f); // Dark background
	}

	FLinearColor RowBackground()
	{
		return FLinearColor(0.14f, 0.15f, 0.16f, 1.0f); // Slightly lighter for rows
	}

	FLinearColor RowBackgroundHover()
	{
		return FLinearColor(0.18f, 0.19f, 0.20f, 1.0f); // Highlighted row
	}

	//~ Sizes & Spacing

	float InputHeight()
	{
		return 32.0f;
	}

	float ButtonHeight()
	{
		return 32.0f;
	}

	float IconButtonSize()
	{
		return 24.0f;
	}

	float RowSpacing()
	{
		return 8.0f;
	}

	FMargin ContentPadding()
	{
		return FMargin(12.0f);
	}

	FMargin InputPadding()
	{
		return FMargin(8.0f, 6.0f);
	}

	FMargin ButtonPadding()
	{
		return FMargin(16.0f, 6.0f);
	}

	float BorderRadius()
	{
		return 4.0f;
	}

	//~ Fonts

	FSlateFontInfo HeaderFont()
	{
		return FCoreStyle::GetDefaultFontStyle("Bold", 14);
	}

	FSlateFontInfo BodyFont()
	{
		return FCoreStyle::GetDefaultFontStyle("Regular", 10);
	}

	FSlateFontInfo CaptionFont()
	{
		return FCoreStyle::GetDefaultFontStyle("Regular", 8);
	}

	FSlateFontInfo ButtonFont()
	{
		return FCoreStyle::GetDefaultFontStyle("Bold", 10);
	}

	//~ Brush Helpers

	const FSlateBrush* GetRoundedBoxBrush()
	{
		// Use the engine's built-in white brush as base
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	FEditableTextBoxStyle GetEditableTextBoxStyle()
	{
		FEditableTextBoxStyle Style = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
		
		// Background
		Style.BackgroundImageNormal.TintColor = FSlateColor(InputBackground());
		Style.BackgroundImageHovered.TintColor = FSlateColor(InputBackground());
		Style.BackgroundImageFocused.TintColor = FSlateColor(InputBackground());
		Style.BackgroundImageReadOnly.TintColor = FSlateColor(InputBackground() * 0.8f);
		
		// Text colors
		Style.ForegroundColor = FSlateColor(TextColor());
		
		// Padding
		Style.Padding = InputPadding();
		
		// Note: Font is set directly on the SEditableTextBox widget, not on the style
		
		return Style;
	}

	FButtonStyle GetButtonStyle(bool bIsPrimary)
	{
		FButtonStyle Style = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
		
		if (bIsPrimary)
		{
			Style.Normal.TintColor = FSlateColor(Primary());
			Style.Hovered.TintColor = FSlateColor(PrimaryHover());
			Style.Pressed.TintColor = FSlateColor(PrimaryPressed());
		}
		else
		{
			Style.Normal.TintColor = FSlateColor(Secondary());
			Style.Hovered.TintColor = FSlateColor(SecondaryHover());
			Style.Pressed.TintColor = FSlateColor(Secondary() * 0.8f);
		}
		
		Style.Disabled.TintColor = FSlateColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.5f));
		
		return Style;
	}
}

