// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Types/CPM_WidgetTypes.h"

/**
 * A reusable alert banner with a message, close button,
 * and an optional configurable action button (e.g., "Retry", "Reload").
 * Supports Error, Warning, Info, and Success severity types.
 *
 * Usage:
 *   SNew(SCPM_AlertBanner)
 *   .AlertType(ECPM_AlertType::Error)
 *   .Message(FText::FromString("Something went wrong"))
 *   .ActionButtonText(FText::FromString("Retry"))
 *   .OnClose(FSimpleDelegate::CreateLambda([](){ ... }))
 *   .OnActionClicked(FSimpleDelegate::CreateLambda([](){ ... }))
 */
class CONVAIPAKMANAGEREDITOR_API SCPM_AlertBanner : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCPM_AlertBanner)
		: _AlertType(ECPM_AlertType::Error)
		, _Message(FText::GetEmpty())
		, _ActionButtonText(FText::GetEmpty())
	{}
		/** The alert severity type — controls colors and icon */
		SLATE_ARGUMENT(ECPM_AlertType, AlertType)

		/** The message to display */
		SLATE_ATTRIBUTE(FText, Message)

		/** Text for the optional action button. Leave empty to hide it. */
		SLATE_ARGUMENT(FText, ActionButtonText)

		/** Called when the close (X) button is clicked */
		SLATE_EVENT(FSimpleDelegate, OnClose)

		/** Called when the action button is clicked */
		SLATE_EVENT(FSimpleDelegate, OnActionClicked)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Update the message at runtime */
	void SetMessage(const FText& InMessage);

	/** Update the action button text at runtime (empty hides the button) */
	void SetActionButtonText(const FText& InText);

	/** Show the banner */
	void Show();

	/** Hide the banner */
	void Hide();

private:
	ECPM_AlertType CurrentAlertType;

	TSharedPtr<class STextBlock> MessageTextBlock;
	TSharedPtr<class STextBlock> ActionTextBlock;
	TSharedPtr<class SWidget> ActionButtonContainer;

	FSimpleDelegate OnCloseCallback;
	FSimpleDelegate OnActionClickedCallback;

	FLinearColor GetAccentColor() const;
	FLinearColor GetAccentHoverColor() const;
	FLinearColor GetBackgroundTint() const;
	FText GetIconText() const;

	FReply HandleClose();
	FReply HandleActionClicked();
};
