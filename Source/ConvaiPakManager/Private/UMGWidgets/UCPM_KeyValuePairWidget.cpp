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

	// If we need to update properties at runtime, we'd rebuild here
	// For now, most properties are only set at construction
}

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

void UCPM_KeyValuePairWidget::AddEmptyPair()
{
	if (SlateWidget.IsValid())
	{
		SlateWidget->AddPair(FCPM_KeyValuePair());
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
	TMap<FString, FString> Result;
	
	TArray<FCPM_KeyValuePair> Pairs = GetValidPairs();
	for (const FCPM_KeyValuePair& Pair : Pairs)
	{
		Result.Add(Pair.Key, Pair.Value);
	}
	
	return Result;
}

void UCPM_KeyValuePairWidget::SetPairsFromMap(const TMap<FString, FString>& InMap)
{
	TArray<FCPM_KeyValuePair> Pairs;
	Pairs.Reserve(InMap.Num());
	
	for (const TPair<FString, FString>& MapPair : InMap)
	{
		Pairs.Add(FCPM_KeyValuePair(MapPair.Key, MapPair.Value));
	}
	
	SetPairs(Pairs);
}

void UCPM_KeyValuePairWidget::HandleListChanged(const TArray<FCPM_KeyValuePair>& Pairs)
{
	OnPairsChanged.Broadcast(Pairs);
}

#undef LOCTEXT_NAMESPACE

