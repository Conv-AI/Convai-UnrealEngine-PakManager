// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "UI/CPM_PakManagerViewModels.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class ITableRow;
class STableViewBase;
template <typename ItemType> class SListView;

DECLARE_DELEGATE_OneParam(FOnCPMAssetSelected, TSharedPtr<FCPM_AssetViewModel>);

/**
 * Sidebar list of the project's Chunks: badge dot, name, type - with search. Shown only in
 * multi-Chunk projects; the root panel decides that, this widget just lists what it is given.
 *
 * Selection is a request, not a fact: the row click goes out through OnAssetSelected and the root
 * panel answers with SetSelection once the unsaved-edits guard has had its say.
 */
class SCPM_AssetListPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCPM_AssetListPanel)
		: _Project(nullptr)
	{}
		SLATE_ARGUMENT(FCPM_ProjectViewModel*, Project)
		SLATE_EVENT(FOnCPMAssetSelected, OnAssetSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Re-filters and redraws after the project view model changed underneath. */
	void RefreshList();

	/** Moves the visual selection without going through the selection guard. */
	void SetSelection(TSharedPtr<FCPM_AssetViewModel> AssetVM);

private:
	TSharedRef<ITableRow> GenerateRow(TSharedPtr<FCPM_AssetViewModel> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void HandleSelectionChanged(TSharedPtr<FCPM_AssetViewModel> Item, ESelectInfo::Type SelectInfo);
	void HandleSearchChanged(const FText& NewText);

	void ApplyFilter();

	FCPM_ProjectViewModel* Project = nullptr;
	FOnCPMAssetSelected OnAssetSelected;

	/** What the list shows: Project->Assets minus rows the search filters out. */
	TArray<TSharedPtr<FCPM_AssetViewModel>> Filtered;
	FString SearchString;

	TSharedPtr<SListView<TSharedPtr<FCPM_AssetViewModel>>> ListView;
};
