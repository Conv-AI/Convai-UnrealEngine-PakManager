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
	CONVAIPAKMANAGEREDITOR_API FLinearColor Primary();
	
	/** Primary color when hovered */
	CONVAIPAKMANAGEREDITOR_API FLinearColor PrimaryHover();
	
	/** Primary color when pressed */
	CONVAIPAKMANAGEREDITOR_API FLinearColor PrimaryPressed();
	
	/** Secondary/neutral color */
	CONVAIPAKMANAGEREDITOR_API FLinearColor Secondary();
	
	/** Secondary color when hovered */
	CONVAIPAKMANAGEREDITOR_API FLinearColor SecondaryHover();
	
	/** Danger/destructive action color */
	CONVAIPAKMANAGEREDITOR_API FLinearColor Danger();
	
	/** Danger color when hovered */
	CONVAIPAKMANAGEREDITOR_API FLinearColor DangerHover();

	/** Warning/caution color */
	CONVAIPAKMANAGEREDITOR_API FLinearColor Warning();

	/** Warning color when hovered */
	CONVAIPAKMANAGEREDITOR_API FLinearColor WarningHover();

	/** Informational color */
	CONVAIPAKMANAGEREDITOR_API FLinearColor Info();

	/** Info color when hovered */
	CONVAIPAKMANAGEREDITOR_API FLinearColor InfoHover();

	/** Success/positive color */
	CONVAIPAKMANAGEREDITOR_API FLinearColor Success();

	/** Success color when hovered */
	CONVAIPAKMANAGEREDITOR_API FLinearColor SuccessHover();
	
	/** Input field background color */
	CONVAIPAKMANAGEREDITOR_API FLinearColor InputBackground();
	
	/** Input field border color */
	CONVAIPAKMANAGEREDITOR_API FLinearColor InputBorder();
	
	/** Input field border color when focused */
	CONVAIPAKMANAGEREDITOR_API FLinearColor InputBorderFocused();
	
	/** Main text color */
	CONVAIPAKMANAGEREDITOR_API FLinearColor TextColor();
	
	/** Hint/placeholder text color */
	CONVAIPAKMANAGEREDITOR_API FLinearColor HintTextColor();
	
	/** Text color for primary buttons */
	CONVAIPAKMANAGEREDITOR_API FLinearColor ButtonTextColor();
	
	/** Widget background color */
	CONVAIPAKMANAGEREDITOR_API FLinearColor Background();
	
	/** Row/item background color */
	CONVAIPAKMANAGEREDITOR_API FLinearColor RowBackground();
	
	/** Row/item background color when hovered */
	CONVAIPAKMANAGEREDITOR_API FLinearColor RowBackgroundHover();

	//~ Sizes & Spacing
	
	/** Standard input field height */
	CONVAIPAKMANAGEREDITOR_API float InputHeight();
	
	/** Standard button height */
	CONVAIPAKMANAGEREDITOR_API float ButtonHeight();
	
	/** Icon button size (square) */
	CONVAIPAKMANAGEREDITOR_API float IconButtonSize();
	
	/** Spacing between rows */
	CONVAIPAKMANAGEREDITOR_API float RowSpacing();
	
	/** Standard content padding */
	CONVAIPAKMANAGEREDITOR_API FMargin ContentPadding();
	
	/** Input field padding */
	CONVAIPAKMANAGEREDITOR_API FMargin InputPadding();
	
	/** Button padding */
	CONVAIPAKMANAGEREDITOR_API FMargin ButtonPadding();
	
	/** Border radius for rounded elements */
	CONVAIPAKMANAGEREDITOR_API float BorderRadius();

	//~ Fonts
	
	/** Font for headers */
	CONVAIPAKMANAGEREDITOR_API FSlateFontInfo HeaderFont();
	
	/** Font for body text */
	CONVAIPAKMANAGEREDITOR_API FSlateFontInfo BodyFont();
	
	/** Font for captions/small text */
	CONVAIPAKMANAGEREDITOR_API FSlateFontInfo CaptionFont();
	
	/** Font for button text */
	CONVAIPAKMANAGEREDITOR_API FSlateFontInfo ButtonFont();

	//~ Brush Helpers
	
	/** Get a rounded box brush with specified color */
	CONVAIPAKMANAGEREDITOR_API const FSlateBrush* GetRoundedBoxBrush();
	
	/** Create an editable text box style */
	CONVAIPAKMANAGEREDITOR_API FEditableTextBoxStyle GetEditableTextBoxStyle();
	
	/** Create a button style for the specified variant */
	CONVAIPAKMANAGEREDITOR_API FButtonStyle GetButtonStyle(bool bIsPrimary = true);
}

