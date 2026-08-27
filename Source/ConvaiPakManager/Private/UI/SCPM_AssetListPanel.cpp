// Copyright 2025 Convai Inc. All Rights Reserved.

#include "UI/SCPM_AssetListPanel.h"

#include "Brushes/SlateNoResource.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "UI/CPM_PakManagerStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SCPM_AssetListPanel"

namespace
{
	const FTextBlockStyle& SecondaryTextStyle()
	{
		return FCPM_PakManagerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CPM.Text.Secondary");
	}

	/** Stock editor row recolored: transparent at rest, the Hover surface for hover and selection. */
	const FTableRowStyle& AssetRowStyle()
	{
		static const FTableRowStyle Style = []
		{
			FTableRowStyle RowStyle = FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");
			const FSlateBrush* Hover = FCPM_PakManagerStyle::Get().GetBrush("CPM.Hover");
			RowStyle.SetEvenRowBackgroundBrush(FSlateNoResource())
				.SetOddRowBackgroundBrush(FSlateNoResource())
				.SetEvenRowBackgroundHoveredBrush(*Hover)
				.SetOddRowBackgroundHoveredBrush(*Hover)
				.SetActiveBrush(*Hover)
				.SetActiveHoveredBrush(*Hover)
				.SetInactiveBrush(*Hover)
				.SetInactiveHoveredBrush(*Hover);
			return RowStyle;
		}();
		return Style;
	}

	FSlateColor BadgeColor(FCPM_AssetViewModel::EBadge Badge)
	{
		using FPalette = FCPM_PakManagerStyle::FPalette;
		switch (Badge)
		{
		case FCPM_AssetViewModel::EBadge::ReadyToPublish:
			return FSlateColor(FPalette::GreenBright);
		case FCPM_AssetViewModel::EBadge::Publishing:
			return FSlateColor(FPalette::Warning);
		case FCPM_AssetViewModel::EBadge::Published:
			return FSlateColor(FPalette::GreenPrimary);
		case FCPM_AssetViewModel::EBadge::NeedsAttention:
			return FSlateColor(FPalette::Error);
		default:
			return FSlateColor(FPalette::TextSecondary);
		}
	}

	FText TypeText(ECPM_AssetType Type)
	{
		switch (Type)
		{
		case ECPM_AssetType::Scene:
			return LOCTEXT("SceneType", "Scene");
		case ECPM_AssetType::Avatar:
			return LOCTEXT("AvatarType", "Avatar");
		default:
			return LOCTEXT("UnknownType", "Asset");
		}
	}
}

void SCPM_AssetListPanel::Construct(const FArguments& InArgs)
{
	Project = InArgs._Project;
	OnAssetSelected = InArgs._OnAssetSelected;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCPM_PakManagerStyle::Get().GetBrush("CPM.Panel"))
		.Padding(8.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(4.0f, 4.0f, 4.0f, 8.0f)
			[
				SNew(STextBlock)
				.TextStyle(&FCPM_PakManagerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CPM.Text.Section"))
				.Text(LOCTEXT("AssetsHeader", "Assets"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SSearchBox)
				.HintText(LOCTEXT("SearchAssets", "Search assets..."))
				.OnTextChanged(this, &SCPM_AssetListPanel::HandleSearchChanged)
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SAssignNew(ListView, SListView<TSharedPtr<FCPM_AssetViewModel>>)
					.ListItemsSource(&Filtered)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow(this, &SCPM_AssetListPanel::GenerateRow)
					.OnSelectionChanged(this, &SCPM_AssetListPanel::HandleSelectionChanged)
				]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(12.0f)
				[
					SNew(STextBlock)
					.TextStyle(&SecondaryTextStyle())
					.AutoWrapText(true)
					.Justification(ETextJustify::Center)
					.Text_Lambda([this]
					{
						// Chunks come from Primary Asset Labels, not from this UI (design D4).
						return Project->Assets.IsEmpty()
							? LOCTEXT("NoAssets", "No assets in this project yet. Add a Primary Asset Label to the content you want to publish.")
							: LOCTEXT("NoSearchMatch", "No assets match the search.");
					})
					.Visibility_Lambda([this]
					{
						return Filtered.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
					})
				]
			]
		]
	];

	ApplyFilter();
}

void SCPM_AssetListPanel::RefreshList()
{
	ApplyFilter();
}

void SCPM_AssetListPanel::SetSelection(TSharedPtr<FCPM_AssetViewModel> AssetVM)
{
	if (!ListView.IsValid())
	{
		return;
	}

	if (AssetVM.IsValid())
	{
		ListView->SetSelection(AssetVM, ESelectInfo::Direct);
	}
	else
	{
		ListView->ClearSelection();
	}
}

TSharedRef<ITableRow> SCPM_AssetListPanel::GenerateRow(TSharedPtr<FCPM_AssetViewModel> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	using FPalette = FCPM_PakManagerStyle::FPalette;

	TSharedRef<STableRow<TSharedPtr<FCPM_AssetViewModel>>> RowWidget =
		SNew(STableRow<TSharedPtr<FCPM_AssetViewModel>>, OwnerTable)
		.Style(&AssetRowStyle())
		.Padding(FMargin(0.0f));

	// Raw pointer is safe: the lambda lives inside the row it points at.
	STableRow<TSharedPtr<FCPM_AssetViewModel>>* RowPtr = &RowWidget.Get();

	RowWidget->SetContent(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(3.0f)
			[
				SNew(SImage)
				.Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.ColorAndOpacity(FSlateColor(FPalette::GreenPrimary))
				.Visibility_Lambda([RowPtr]
				{
					// Hidden, not Collapsed, so the text does not shift when selection moves.
					return RowPtr->IsSelected() ? EVisibility::Visible : EVisibility::Hidden;
				})
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 6.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("\u25CF")))
			.ColorAndOpacity_Lambda([Item] { return BadgeColor(Item->Badge()); })
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 4.0f, 8.0f, 4.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text_Lambda([Item]
				{
					const FText Name = Item->Name.IsEmpty()
						? FText::FromString(FString::Printf(TEXT("Chunk %d"), Item->ChunkId))
						: FText::FromString(Item->Name);
					return Item->IsDirty() ? FText::Format(LOCTEXT("DirtyName", "{0} *"), Name) : Name;
				})
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.TextStyle(&SecondaryTextStyle())
				.Text_Lambda([Item]
				{
					return FText::Format(LOCTEXT("RowSecondary", "{0} - {1}"), TypeText(Item->AssetType), Item->BadgeText());
				})
			]
		]);

	return RowWidget;
}

void SCPM_AssetListPanel::HandleSelectionChanged(TSharedPtr<FCPM_AssetViewModel> Item, ESelectInfo::Type SelectInfo)
{
	// Direct means SetSelection - the root panel syncing the view, not the creator choosing.
	if (SelectInfo == ESelectInfo::Direct || !Item.IsValid())
	{
		return;
	}

	OnAssetSelected.ExecuteIfBound(Item);
}

void SCPM_AssetListPanel::HandleSearchChanged(const FText& NewText)
{
	SearchString = NewText.ToString();
	ApplyFilter();
}

void SCPM_AssetListPanel::ApplyFilter()
{
	Filtered.Reset();

	for (const TSharedPtr<FCPM_AssetViewModel>& AssetVM : Project->Assets)
	{
		if (SearchString.IsEmpty()
			|| AssetVM->Name.Contains(SearchString)
			|| TypeText(AssetVM->AssetType).ToString().Contains(SearchString))
		{
			Filtered.Add(AssetVM);
		}
	}

	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
