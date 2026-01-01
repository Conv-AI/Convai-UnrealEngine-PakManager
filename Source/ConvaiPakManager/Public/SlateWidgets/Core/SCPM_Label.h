// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Types/CPM_WidgetTypes.h"

/**
 * A styled text label with consistent CPM styling.
 * Supports Header, Body, Caption, and Button text styles.
 */
class CONVAIPAKMANAGER_API SCPM_Label : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCPM_Label)
		: _Text()
		, _TextStyle(ECPM_TextStyle::Body)
		, _ColorOverride()
		, _Justification(ETextJustify::Left)
	{}
		/** The text to display */
		SLATE_ATTRIBUTE(FText, Text)
		
		/** The text style variant */
		SLATE_ARGUMENT(ECPM_TextStyle, TextStyle)
		
		/** Optional color override */
		SLATE_ARGUMENT(TOptional<FLinearColor>, ColorOverride)
		
		/** Text justification */
		SLATE_ARGUMENT(ETextJustify::Type, Justification)
		
	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs);

	/** Set the text */
	void SetText(const FText& InText);

	/** Set the text color */
	void SetColor(const FLinearColor& InColor);

private:
	/** The text block widget */
	TSharedPtr<class STextBlock> TextBlock;
	
	/** Current text style */
	ECPM_TextStyle CurrentStyle;
	
	/** Optional color override */
	TOptional<FLinearColor> ColorOverride;

	/** Get the font based on text style */
	FSlateFontInfo GetFont() const;

	/** Get the color based on text style or override */
	FSlateColor GetColor() const;
};

