// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Types/CPM_WidgetTypes.h"

/**
 * A small icon-only button for actions like remove, add, etc.
 * Uses text characters as icons for simplicity (✕, +, etc.)
 */
class CONVAIPAKMANAGER_API SCPM_IconButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCPM_IconButton)
		: _IconText(FText::FromString(TEXT("✕")))
		, _ButtonStyle(ECPM_ButtonStyle::Ghost)
		, _ToolTipText()
		, _IsEnabled(true)
	{}
		/** The icon character to display (e.g., "✕", "+", "−") */
		SLATE_ATTRIBUTE(FText, IconText)
		
		/** The button style variant */
		SLATE_ARGUMENT(ECPM_ButtonStyle, ButtonStyle)
		
		/** Tooltip text */
		SLATE_ATTRIBUTE(FText, ToolTipText)
		
		/** Whether the button is enabled */
		SLATE_ATTRIBUTE(bool, IsEnabled)
		
		/** Called when the button is clicked */
		SLATE_EVENT(FOnClicked, OnClicked)
		
	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs);

	/** Set whether the button is enabled */
	void SetEnabled(bool bInEnabled);

	/** Common icon constants */
	static FText GetRemoveIcon() { return FText::FromString(TEXT("\u2715")); }  // ✕
	static FText GetAddIcon() { return FText::FromString(TEXT("+")); }           // +
	static FText GetMinusIcon() { return FText::FromString(TEXT("\u2212")); }    // −
	static FText GetEditIcon() { return FText::FromString(TEXT("\u270E")); }     // ✎
	static FText GetCheckIcon() { return FText::FromString(TEXT("\u2713")); }    // ✓

private:
	/** The actual button widget */
	TSharedPtr<class SButton> Button;
	
	/** Current button style */
	ECPM_ButtonStyle CurrentStyle;
	
	/** Stored click callback */
	FOnClicked OnClickedCallback;

	/** Get the icon/text color based on style */
	FSlateColor GetIconColor() const;

	/** Handle button click */
	FReply HandleClicked();
};

