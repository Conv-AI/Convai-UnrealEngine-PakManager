// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"

/**
 * Centralized styling for all CPM Slate widgets.
 * Provides consistent colors, fonts, and sizes across the widget library.
 */
namespace CPMStyle
{
	//~ Colors
	
	/** Primary action color (buttons, highlights) */
	CONVAIPAKMANAGER_API FLinearColor Primary();
	
	/** Primary color when hovered */
	CONVAIPAKMANAGER_API FLinearColor PrimaryHover();
	
	/** Primary color when pressed */
	CONVAIPAKMANAGER_API FLinearColor PrimaryPressed();
	
	/** Secondary/neutral color */
	CONVAIPAKMANAGER_API FLinearColor Secondary();
	
	/** Secondary color when hovered */
	CONVAIPAKMANAGER_API FLinearColor SecondaryHover();
	
	/** Danger/destructive action color */
	CONVAIPAKMANAGER_API FLinearColor Danger();
	
	/** Danger color when hovered */
	CONVAIPAKMANAGER_API FLinearColor DangerHover();
	
	/** Input field background color */
	CONVAIPAKMANAGER_API FLinearColor InputBackground();
	
	/** Input field border color */
	CONVAIPAKMANAGER_API FLinearColor InputBorder();
	
	/** Input field border color when focused */
	CONVAIPAKMANAGER_API FLinearColor InputBorderFocused();
	
	/** Main text color */
	CONVAIPAKMANAGER_API FLinearColor TextColor();
	
	/** Hint/placeholder text color */
	CONVAIPAKMANAGER_API FLinearColor HintTextColor();
	
	/** Text color for primary buttons */
	CONVAIPAKMANAGER_API FLinearColor ButtonTextColor();
	
	/** Widget background color */
	CONVAIPAKMANAGER_API FLinearColor Background();
	
	/** Row/item background color */
	CONVAIPAKMANAGER_API FLinearColor RowBackground();
	
	/** Row/item background color when hovered */
	CONVAIPAKMANAGER_API FLinearColor RowBackgroundHover();

	//~ Sizes & Spacing
	
	/** Standard input field height */
	CONVAIPAKMANAGER_API float InputHeight();
	
	/** Standard button height */
	CONVAIPAKMANAGER_API float ButtonHeight();
	
	/** Icon button size (square) */
	CONVAIPAKMANAGER_API float IconButtonSize();
	
	/** Spacing between rows */
	CONVAIPAKMANAGER_API float RowSpacing();
	
	/** Standard content padding */
	CONVAIPAKMANAGER_API FMargin ContentPadding();
	
	/** Input field padding */
	CONVAIPAKMANAGER_API FMargin InputPadding();
	
	/** Button padding */
	CONVAIPAKMANAGER_API FMargin ButtonPadding();
	
	/** Border radius for rounded elements */
	CONVAIPAKMANAGER_API float BorderRadius();

	//~ Fonts
	
	/** Font for headers */
	CONVAIPAKMANAGER_API FSlateFontInfo HeaderFont();
	
	/** Font for body text */
	CONVAIPAKMANAGER_API FSlateFontInfo BodyFont();
	
	/** Font for captions/small text */
	CONVAIPAKMANAGER_API FSlateFontInfo CaptionFont();
	
	/** Font for button text */
	CONVAIPAKMANAGER_API FSlateFontInfo ButtonFont();

	//~ Brush Helpers
	
	/** Get a rounded box brush with specified color */
	CONVAIPAKMANAGER_API const FSlateBrush* GetRoundedBoxBrush();
	
	/** Create an editable text box style */
	CONVAIPAKMANAGER_API FEditableTextBoxStyle GetEditableTextBoxStyle();
	
	/** Create a button style for the specified variant */
	CONVAIPAKMANAGER_API FButtonStyle GetButtonStyle(bool bIsPrimary = true);
}

