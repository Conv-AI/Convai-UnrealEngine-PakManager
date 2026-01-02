// Copyright 2022 Convai Inc. All Rights Reserved.

#include "UMGWidgets/UCPM_KeyValuePairWidget.h"
#include "SlateWidgets/Widgets/SCPM_KeyValueList.h"

#define LOCTEXT_NAMESPACE "CPMWidgets"

UCPM_KeyValuePairWidget::UCPM_KeyValuePairWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, KeyHintText(LOCTEXT("DefaultKeyHint", "Key"))
	, ValueHintText(LOCTEXT("DefaultValueHint", "Value"))
	, AddButtonText(LOCTEXT("DefaultAddButton", "+ Add New Pair"))
	, bShowAddButton(true)
	, MaxHeight(300.0f)
{
}

void UCPM_KeyValuePairWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	SlateWidget.Reset();
}

TSharedRef<SWidget> UCPM_KeyValuePairWidget::RebuildWidget()
{
	// Create the delegate binding
	FOnKeyValueListChanged ListChangedDelegate;
	ListChangedDelegate.BindUObject(this, &UCPM_KeyValuePairWidget::HandleListChanged);
	
	SlateWidget = SNew(SCPM_KeyValueList)
		.InitialPairs(InitialPairs)
		.KeyHintText(KeyHintText)
		.ValueHintText(ValueHintText)
		.AddButtonText(AddButtonText)
		.ShowAddButton(bShowAddButton)
		.MaxHeight(MaxHeight)
		.OnListChanged(ListChangedDelegate);

	return SlateWidget.ToSharedRef();
}

#if WITH_EDITOR
const FText UCPM_KeyValuePairWidget::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Convai|PakManager");
}
#endif

void UCPM_KeyValuePairWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
}

//~ Basic Getters

TArray<FCPM_KeyValuePair> UCPM_KeyValuePairWidget::GetAllPairs() const
{
	if (SlateWidget.IsValid())
	{
		return SlateWidget->GetAllPairs();
	}
	return TArray<FCPM_KeyValuePair>();
}

TArray<FCPM_KeyValuePair> UCPM_KeyValuePairWidget::GetValidPairs() const
{
	if (SlateWidget.IsValid())
	{
		return SlateWidget->GetValidPairs();
	}
	return TArray<FCPM_KeyValuePair>();
}

int32 UCPM_KeyValuePairWidget::GetPairCount() const
{
	if (SlateWidget.IsValid())
	{
		return SlateWidget->GetPairCount();
	}
	return 0;
}

TMap<FString, FString> UCPM_KeyValuePairWidget::GetPairsAsMap() const
{
	if (SlateWidget.IsValid())
	{
		return SlateWidget->GetPairsAsMap();
	}
	return TMap<FString, FString>();
}

//~ Basic Setters

void UCPM_KeyValuePairWidget::SetPairs(const TArray<FCPM_KeyValuePair>& InPairs)
{
	if (SlateWidget.IsValid())
	{
		SlateWidget->SetPairs(InPairs);
	}
	else
	{
		// Store for when widget is built
		InitialPairs = InPairs;
	}
}

void UCPM_KeyValuePairWidget::AddPair(const FCPM_KeyValuePair& Pair)
{
	if (SlateWidget.IsValid())
	{
		SlateWidget->AddPair(Pair);
	}
}

void UCPM_KeyValuePairWidget::RemovePairAt(int32 Index)
{
	if (SlateWidget.IsValid())
	{
		SlateWidget->RemovePairAt(Index);
	}
}

void UCPM_KeyValuePairWidget::ClearAllPairs()
{
	if (SlateWidget.IsValid())
	{
		SlateWidget->ClearAllPairs();
	}
}

//~ Index-Based Access

FCPM_KeyValuePair UCPM_KeyValuePairWidget::GetPairByIndex(int32 Index) const
{
	if (SlateWidget.IsValid())
	{
		return SlateWidget->GetPairByIndex(Index);
	}
	return FCPM_KeyValuePair();
}

bool UCPM_KeyValuePairWidget::SetPairByIndex(int32 Index, const FCPM_KeyValuePair& InPair)
{
	if (SlateWidget.IsValid())
	{
		return SlateWidget->SetPairByIndex(Index, InPair);
	}
	return false;
}

//~ Key-Based Lookup

bool UCPM_KeyValuePairWidget::HasKey(const FString& Key) const
{
	if (SlateWidget.IsValid())
	{
		return SlateWidget->HasKey(Key);
	}
	return false;
}

int32 UCPM_KeyValuePairWidget::FindKeyIndex(const FString& Key) const
{
	if (SlateWidget.IsValid())
	{
		return SlateWidget->FindKeyIndex(Key);
	}
	return INDEX_NONE;
}

FString UCPM_KeyValuePairWidget::GetValueForKey(const FString& Key) const
{
	if (SlateWidget.IsValid())
	{
		return SlateWidget->GetValueForKey(Key);
	}
	return FString();
}

FCPM_KeyValuePair UCPM_KeyValuePairWidget::GetPairByKey(const FString& Key) const
{
	if (SlateWidget.IsValid())
	{
		return SlateWidget->GetPairByKey(Key);
	}
	return FCPM_KeyValuePair();
}

bool UCPM_KeyValuePairWidget::SetValueForKey(const FString& Key, const FString& NewValue)
{
	if (SlateWidget.IsValid())
	{
		return SlateWidget->SetValueForKey(Key, NewValue);
	}
	return false;
}

bool UCPM_KeyValuePairWidget::SetPairByKey(const FString& Key, const FCPM_KeyValuePair& InPair)
{
	if (SlateWidget.IsValid())
	{
		return SlateWidget->SetPairByKey(Key, InPair);
	}
	return false;
}

bool UCPM_KeyValuePairWidget::RemoveByKey(const FString& Key)
{
	if (SlateWidget.IsValid())
	{
		return SlateWidget->RemoveByKey(Key);
	}
	return false;
}

void UCPM_KeyValuePairWidget::AddOrUpdatePair(const FCPM_KeyValuePair& InPair)
{
	if (SlateWidget.IsValid())
	{
		SlateWidget->AddOrUpdatePair(InPair);
	}
}

//~ JSON Export

FString UCPM_KeyValuePairWidget::GetPairsAsJsonString(const TArray<FString>& KeysToIgnore)
{
	if (SlateWidget.IsValid())
	{
		return SlateWidget->GetPairsAsJsonString(KeysToIgnore);
	}
	return TEXT("{}");
}

//~ Event Handler

void UCPM_KeyValuePairWidget::HandleListChanged(const TArray<FCPM_KeyValuePair>& Pairs)
{
	OnPairsChanged.Broadcast(Pairs);
}

#undef LOCTEXT_NAMESPACE
