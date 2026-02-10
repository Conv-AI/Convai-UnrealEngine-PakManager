// Copyright 2022 Convai Inc. All Rights Reserved.

#include "SlateWidgets/Composite/SCPM_AlertBanner.h"
#include "SlateWidgets/CPM_SlateStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"

void SCPM_AlertBanner::Construct(const FArguments& InArgs)
{
	CurrentAlertType = InArgs._AlertType;
	OnCloseCallback = InArgs._OnClose;
	OnActionClickedCallback = InArgs._OnActionClicked;

	const FText ActionText = InArgs._ActionButtonText;
	const bool bShowAction = !ActionText.IsEmpty();

	static FButtonStyle ActionStyle = CPMStyle::GetButtonStyle(true);
	ActionStyle.Normal.TintColor = FSlateColor(GetAccentColor());
	ActionStyle.Hovered.TintColor = FSlateColor(GetAccentHoverColor());
	ActionStyle.Pressed.TintColor = FSlateColor(GetAccentColor() * 0.8f);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(GetBackgroundTint())
		.Padding(CPMStyle::ContentPadding())
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				.Padding(0, 0, 8.0f, 0)
				[
					SNew(STextBlock)
					.Text(GetIconText())
					.Font(CPMStyle::HeaderFont())
					.ColorAndOpacity(GetAccentColor())
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SAssignNew(MessageTextBlock, STextBlock)
					.Text(InArgs._Message)
					.Font(CPMStyle::BodyFont())
					.ColorAndOpacity(CPMStyle::TextColor())
					.AutoWrapText(true)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				.Padding(8.0f, 0, 0, 0)
				[
					SNew(SBox)
					.WidthOverride(CPMStyle::IconButtonSize())
					.HeightOverride(CPMStyle::IconButtonSize())
					[
						SNew(SButton)
						.ButtonStyle(FCoreStyle::Get(), "NoBorder")
						.OnClicked(this, &SCPM_AlertBanner::HandleClose)
						.ContentPadding(FMargin(0))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("\u2715")))
							.Font(CPMStyle::BodyFont())
							.ColorAndOpacity(CPMStyle::HintTextColor())
						]
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(0, CPMStyle::RowSpacing(), 0, 0)
			[
				SAssignNew(ActionButtonContainer, SBox)
				.Visibility(bShowAction ? EVisibility::Visible : EVisibility::Collapsed)
				.MinDesiredHeight(CPMStyle::ButtonHeight())
				[
					SNew(SButton)
					.ButtonStyle(&ActionStyle)
					.OnClicked(this, &SCPM_AlertBanner::HandleActionClicked)
					.ContentPadding(CPMStyle::ButtonPadding())
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SAssignNew(ActionTextBlock, STextBlock)
						.Text(ActionText)
						.Font(CPMStyle::ButtonFont())
						.ColorAndOpacity(CPMStyle::ButtonTextColor())
					]
				]
			]
		]
	];
}

void SCPM_AlertBanner::SetMessage(const FText& InMessage)
{
	if (MessageTextBlock.IsValid())
	{
		MessageTextBlock->SetText(InMessage);
	}
}

void SCPM_AlertBanner::SetActionButtonText(const FText& InText)
{
	if (ActionTextBlock.IsValid())
	{
		ActionTextBlock->SetText(InText);
	}
	if (ActionButtonContainer.IsValid())
	{
		ActionButtonContainer->SetVisibility(
			InText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible
		);
	}
}

void SCPM_AlertBanner::Show()
{
	SetVisibility(EVisibility::Visible);
}

void SCPM_AlertBanner::Hide()
{
	SetVisibility(EVisibility::Collapsed);
}

FLinearColor SCPM_AlertBanner::GetAccentColor() const
{
	switch (CurrentAlertType)
	{
	case ECPM_AlertType::Error:		return CPMStyle::Danger();
	case ECPM_AlertType::Warning:	return CPMStyle::Warning();
	case ECPM_AlertType::Info:		return CPMStyle::Info();
	case ECPM_AlertType::Success:	return CPMStyle::Success();
	default:						return CPMStyle::Danger();
	}
}

FLinearColor SCPM_AlertBanner::GetAccentHoverColor() const
{
	switch (CurrentAlertType)
	{
	case ECPM_AlertType::Error:		return CPMStyle::DangerHover();
	case ECPM_AlertType::Warning:	return CPMStyle::WarningHover();
	case ECPM_AlertType::Info:		return CPMStyle::InfoHover();
	case ECPM_AlertType::Success:	return CPMStyle::SuccessHover();
	default:						return CPMStyle::DangerHover();
	}
}

FLinearColor SCPM_AlertBanner::GetBackgroundTint() const
{
	switch (CurrentAlertType)
	{
	case ECPM_AlertType::Error:		return FLinearColor(0.15f, 0.04f, 0.04f, 1.0f);
	case ECPM_AlertType::Warning:	return FLinearColor(0.15f, 0.12f, 0.04f, 1.0f);
	case ECPM_AlertType::Info:		return FLinearColor(0.04f, 0.08f, 0.15f, 1.0f);
	case ECPM_AlertType::Success:	return FLinearColor(0.04f, 0.12f, 0.05f, 1.0f);
	default:						return FLinearColor(0.15f, 0.04f, 0.04f, 1.0f);
	}
}

FText SCPM_AlertBanner::GetIconText() const
{
	switch (CurrentAlertType)
	{
	case ECPM_AlertType::Error:		return FText::FromString(TEXT("\u26A0"));  // ⚠
	case ECPM_AlertType::Warning:	return FText::FromString(TEXT("\u26A0"));  // ⚠
	case ECPM_AlertType::Info:		return FText::FromString(TEXT("\u2139"));  // ℹ
	case ECPM_AlertType::Success:	return FText::FromString(TEXT("\u2713"));  // ✓
	default:						return FText::FromString(TEXT("\u26A0"));
	}
}

FReply SCPM_AlertBanner::HandleClose()
{
	OnCloseCallback.ExecuteIfBound();
	return FReply::Handled();
}

FReply SCPM_AlertBanner::HandleActionClicked()
{
	OnActionClickedCallback.ExecuteIfBound();
	return FReply::Handled();
}
