// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Types/CPM_WidgetTypes.h"

/**
 * A styled button with consistent CPM styling.
 * Supports Primary, Secondary, Danger, and Ghost variants.
 */
class CONVAIPAKMANAGER_API SCPM_Button : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCPM_Button)
		: _Text()
		, _ButtonStyle(ECPM_ButtonStyle::Primary)
		, _IsEnabled(true)
	{}
		/** The button text */
		SLATE_ATTRIBUTE(FText, Text)
		
		/** The button style variant */
		SLATE_ARGUMENT(ECPM_ButtonStyle, ButtonStyle)
		
		/** Whether the button is enabled */
		SLATE_ATTRIBUTE(bool, IsEnabled)
		
		/** Called when the button is clicked */
		SLATE_EVENT(FOnClicked, OnClicked)
		
		/** Optional: Custom content instead of text */
		SLATE_DEFAULT_SLOT(FArguments, Content)
		
	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs);

	/** Set the button text */
	void SetText(const FText& InText);

	/** Set whether the button is enabled */
	void SetEnabled(bool bInEnabled);

private:
	/** The actual button widget */
	TSharedPtr<class SButton> Button;
	
	/** Text block for displaying button text */
	TSharedPtr<class STextBlock> TextBlock;
	
	/** Current button style */
	ECPM_ButtonStyle CurrentStyle;
	
	/** Stored click callback */
	FOnClicked OnClickedCallback;

	/** Get colors based on button style */
	FLinearColor GetNormalColor() const;
	FLinearColor GetHoverColor() const;
	FLinearColor GetPressedColor() const;
	FLinearColor GetTextColor() const;

	/** Handle button click */
	FReply HandleClicked();
};

