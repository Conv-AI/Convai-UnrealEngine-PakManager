// Copyright 2022 Convai Inc. All Rights Reserved.

#include "SlateWidgets/Widgets/SCPM_KeyValueList.h"
#include "SlateWidgets/Composite/SCPM_KeyValueRow.h"
#include "SlateWidgets/Core/SCPM_Button.h"
#include "SlateWidgets/CPM_SlateStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"

void SCPM_KeyValueList::Construct(const FArguments& InArgs)
{
	KeyHintText = InArgs._KeyHintText;
	ValueHintText = InArgs._ValueHintText;
	OnListChangedCallback = InArgs._OnListChanged;

	// Create initial row widgets from provided pairs
	for (int32 i = 0; i < InArgs._InitialPairs.Num(); ++i)
	{
		RowWidgets.Add(CreateRowWidget(i, InArgs._InitialPairs[i]));
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		
		// Scrollable rows area
		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(InArgs._MaxHeight)
		[
			SAssignNew(ScrollBox, SScrollBox)
			.ScrollBarAlwaysVisible(false)
			+ SScrollBox::Slot()
			[
				SAssignNew(RowsContainer, SVerticalBox)
			]
		]
		
		// Add button
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, CPMStyle::RowSpacing(), 0, 0)
		[
			SNew(SBox)
			.Visibility(InArgs._ShowAddButton ? EVisibility::Visible : EVisibility::Collapsed)
			[
				SNew(SCPM_Button)
				.Text(InArgs._AddButtonText)
				.ButtonStyle(ECPM_ButtonStyle::Secondary)
				.OnClicked(this, &SCPM_KeyValueList::HandleAddClicked)
			]
		]
	];

	// Build initial rows
	RebuildRows();
}

TArray<FCPM_KeyValuePair> SCPM_KeyValueList::GetAllPairs() const
{
	TArray<FCPM_KeyValuePair> Pairs;
	Pairs.Reserve(RowWidgets.Num());
	
	for (const TSharedPtr<SCPM_KeyValueRow>& Row : RowWidgets)
	{
		if (Row.IsValid())
		{
			Pairs.Add(Row->GetKeyValuePair());
		}
	}
	
	return Pairs;
}

TArray<FCPM_KeyValuePair> SCPM_KeyValueList::GetValidPairs() const
{
	TArray<FCPM_KeyValuePair> Pairs;
	
	for (const TSharedPtr<SCPM_KeyValueRow>& Row : RowWidgets)
	{
		if (Row.IsValid())
		{
			FCPM_KeyValuePair Pair = Row->GetKeyValuePair();
			if (Pair.IsValid())
			{
				Pairs.Add(Pair);
			}
		}
	}
	
	return Pairs;
}

void SCPM_KeyValueList::SetPairs(const TArray<FCPM_KeyValuePair>& InPairs)
{
	// Clear existing rows
	RowWidgets.Empty();
	
	// Create new row widgets
	for (int32 i = 0; i < InPairs.Num(); ++i)
	{
		RowWidgets.Add(CreateRowWidget(i, InPairs[i]));
	}
	
	RebuildRows();
	NotifyListChanged();
}

void SCPM_KeyValueList::AddPair(const FCPM_KeyValuePair& InPair)
{
	const int32 NewIndex = RowWidgets.Num();
	RowWidgets.Add(CreateRowWidget(NewIndex, InPair));
	
	RebuildRows();
	NotifyListChanged();
	
	// Scroll to the new row
	if (ScrollBox.IsValid())
	{
		ScrollBox->ScrollToEnd();
	}
}

void SCPM_KeyValueList::RemovePairAt(int32 Index)
{
	if (RowWidgets.IsValidIndex(Index))
	{
		RowWidgets.RemoveAt(Index);
		UpdateRowIndices();
		RebuildRows();
		NotifyListChanged();
	}
}

void SCPM_KeyValueList::ClearAllPairs()
{
	RowWidgets.Empty();
	RebuildRows();
	NotifyListChanged();
}

int32 SCPM_KeyValueList::GetPairCount() const
{
	return RowWidgets.Num();
}

TMap<FString, FString> SCPM_KeyValueList::GetPairsAsMap() const
{
	TMap<FString, FString> Result;
	
	for (const TSharedPtr<SCPM_KeyValueRow>& Row : RowWidgets)
	{
		if (Row.IsValid())
		{
			FCPM_KeyValuePair Pair = Row->GetKeyValuePair();
			if (Pair.IsValid())
			{
				Result.Add(Pair.Key, Pair.Value);
			}
		}
	}
	
	return Result;
}

//~ Index-Based Access

FCPM_KeyValuePair SCPM_KeyValueList::GetPairByIndex(int32 Index) const
{
	if (RowWidgets.IsValidIndex(Index) && RowWidgets[Index].IsValid())
	{
		return RowWidgets[Index]->GetKeyValuePair();
	}
	return FCPM_KeyValuePair();
}

bool SCPM_KeyValueList::SetPairByIndex(int32 Index, const FCPM_KeyValuePair& InPair)
{
	if (RowWidgets.IsValidIndex(Index) && RowWidgets[Index].IsValid())
	{
		RowWidgets[Index]->SetKeyValuePair(InPair);
		NotifyListChanged();
		return true;
	}
	return false;
}

//~ Key-Based Lookup

bool SCPM_KeyValueList::HasKey(const FString& Key) const
{
	return FindKeyIndex(Key) != INDEX_NONE;
}

int32 SCPM_KeyValueList::FindKeyIndex(const FString& Key) const
{
	for (int32 i = 0; i < RowWidgets.Num(); ++i)
	{
		if (RowWidgets[i].IsValid() && RowWidgets[i]->GetKey() == Key)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

FString SCPM_KeyValueList::GetValueForKey(const FString& Key) const
{
	const int32 Index = FindKeyIndex(Key);
	if (Index != INDEX_NONE && RowWidgets[Index].IsValid())
	{
		return RowWidgets[Index]->GetValue();
	}
	return FString();
}

FCPM_KeyValuePair SCPM_KeyValueList::GetPairByKey(const FString& Key) const
{
	const int32 Index = FindKeyIndex(Key);
	if (Index != INDEX_NONE && RowWidgets[Index].IsValid())
	{
		return RowWidgets[Index]->GetKeyValuePair();
	}
	return FCPM_KeyValuePair();
}

bool SCPM_KeyValueList::SetValueForKey(const FString& Key, const FString& NewValue)
{
	const int32 Index = FindKeyIndex(Key);
	if (Index != INDEX_NONE && RowWidgets[Index].IsValid())
	{
		RowWidgets[Index]->SetValue(NewValue);
		NotifyListChanged();
		return true;
	}
	return false;
}

bool SCPM_KeyValueList::SetPairByKey(const FString& Key, const FCPM_KeyValuePair& InPair)
{
	const int32 Index = FindKeyIndex(Key);
	if (Index != INDEX_NONE && RowWidgets[Index].IsValid())
	{
		RowWidgets[Index]->SetKeyValuePair(InPair);
		NotifyListChanged();
		return true;
	}
	return false;
}

bool SCPM_KeyValueList::RemoveByKey(const FString& Key)
{
	const int32 Index = FindKeyIndex(Key);
	if (Index != INDEX_NONE)
	{
		RemovePairAt(Index);
		return true;
	}
	return false;
}

void SCPM_KeyValueList::AddOrUpdatePair(const FCPM_KeyValuePair& InPair)
{
	const int32 Index = FindKeyIndex(InPair.Key);
	if (Index != INDEX_NONE && RowWidgets[Index].IsValid())
	{
		// Key exists, update the entire pair
		RowWidgets[Index]->SetKeyValuePair(InPair);
		NotifyListChanged();
	}
	else
	{
		// Key doesn't exist, add new pair
		AddPair(InPair);
	}
}

TSharedRef<SCPM_KeyValueRow> SCPM_KeyValueList::CreateRowWidget(int32 Index, const FCPM_KeyValuePair& Pair)
{
	return SNew(SCPM_KeyValueRow)
		.RowIndex(Index)
		.Pair(Pair)
		.KeyHintText(KeyHintText)
		.ValueHintText(ValueHintText)
		.ShowRemoveButton(true)
		.OnRemoveRequested(this, &SCPM_KeyValueList::HandleRowRemoveRequested)
		.OnKeyValueChanged(this, &SCPM_KeyValueList::HandleRowValueChanged);
}

void SCPM_KeyValueList::RebuildRows()
{
	if (!RowsContainer.IsValid())
	{
		return;
	}
	
	// Clear existing children
	RowsContainer->ClearChildren();
	
	// Add all row widgets
	for (int32 i = 0; i < RowWidgets.Num(); ++i)
	{
		if (RowWidgets[i].IsValid())
		{
			RowsContainer->AddSlot()
				.AutoHeight()
				.Padding(0, i > 0 ? CPMStyle::RowSpacing() : 0, 0, 0)
				[
					RowWidgets[i].ToSharedRef()
				];
		}
	}
}

void SCPM_KeyValueList::UpdateRowIndices()
{
	for (int32 i = 0; i < RowWidgets.Num(); ++i)
	{
		if (RowWidgets[i].IsValid())
		{
			RowWidgets[i]->SetRowIndex(i);
		}
	}
}

void SCPM_KeyValueList::HandleRowRemoveRequested(int32 RowIndex)
{
	RemovePairAt(RowIndex);
}

void SCPM_KeyValueList::HandleRowValueChanged(int32 RowIndex, const FString& Key, const FString& Value) const
{
	// Just notify - the row already stores the updated value
	NotifyListChanged();
}

FReply SCPM_KeyValueList::HandleAddClicked()
{
	AddPair(FCPM_KeyValuePair());
	return FReply::Handled();
}

void SCPM_KeyValueList::NotifyListChanged() const
{
	if (OnListChangedCallback.IsBound())
	{
		OnListChangedCallback.Execute(GetAllPairs());
	}
}

