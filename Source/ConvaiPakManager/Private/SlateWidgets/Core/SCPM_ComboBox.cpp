// Copyright 2022 Convai Inc. All Rights Reserved.

#include "SlateWidgets/Core/SCPM_ComboBox.h"
#include "SlateWidgets/CPM_SlateStyle.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"

void SCPM_ComboBox::Construct(const FArguments& InArgs)
{
	Options = InArgs._Options;
	OnSelectionChangedCallback = InArgs._OnSelectionChanged;
	
	RebuildOptionsSource();
	
	// Find the initially selected item
	FString InitialSelection = InArgs._SelectedOption;
	if (!InitialSelection.IsEmpty())
	{
		for (const TSharedPtr<FString>& Option : OptionsSource)
		{
			if (Option.IsValid() && *Option == InitialSelection)
			{
				SelectedItem = Option;
				break;
			}
		}
	}
	
	// Default to first item if no selection
	if (!SelectedItem.IsValid() && OptionsSource.Num() > 0)
	{
		SelectedItem = OptionsSource[0];
	}

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredHeight(CPMStyle::InputHeight())
		[
			SAssignNew(ComboBox, STextComboBox)
			.OptionsSource(&OptionsSource)
			.InitiallySelectedItem(SelectedItem)
			.OnSelectionChanged(this, &SCPM_ComboBox::HandleSelectionChanged)
			.Font(CPMStyle::BodyFont())
		]
	];
}

FString SCPM_ComboBox::GetSelectedOption() const
{
	if (SelectedItem.IsValid())
	{
		return *SelectedItem;
	}
	return FString();
}

void SCPM_ComboBox::SetSelectedOption(const FString& InOption)
{
	for (const TSharedPtr<FString>& Option : OptionsSource)
	{
		if (Option.IsValid() && *Option == InOption)
		{
			SelectedItem = Option;
			if (ComboBox.IsValid())
			{
				ComboBox->SetSelectedItem(SelectedItem);
			}
			break;
		}
	}
}

void SCPM_ComboBox::SetOptions(const TArray<FString>& InOptions)
{
	Options = InOptions;
	RebuildOptionsSource();
	
	// Refresh the combo box
	if (ComboBox.IsValid())
	{
		ComboBox->RefreshOptions();
		
		// Re-select current item if still valid, otherwise select first
		bool bFoundCurrent = false;
		if (SelectedItem.IsValid())
		{
			for (const TSharedPtr<FString>& Option : OptionsSource)
			{
				if (Option.IsValid() && *Option == *SelectedItem)
				{
					SelectedItem = Option;
					ComboBox->SetSelectedItem(SelectedItem);
					bFoundCurrent = true;
					break;
				}
			}
		}
		
		if (!bFoundCurrent && OptionsSource.Num() > 0)
		{
			SelectedItem = OptionsSource[0];
			ComboBox->SetSelectedItem(SelectedItem);
		}
	}
}

void SCPM_ComboBox::RebuildOptionsSource()
{
	OptionsSource.Empty();
	OptionsSource.Reserve(Options.Num());
	
	for (const FString& Option : Options)
	{
		OptionsSource.Add(MakeShared<FString>(Option));
	}
}

void SCPM_ComboBox::HandleSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedItem = NewSelection;
	
	if (NewSelection.IsValid())
	{
		OnSelectionChangedCallback.ExecuteIfBound(*NewSelection);
	}
}

TSharedRef<SWidget> SCPM_ComboBox::GenerateOptionWidget(TSharedPtr<FString> InOption)
{
	return SNew(STextBlock)
		.Text(FText::FromString(InOption.IsValid() ? *InOption : FString()))
		.Font(CPMStyle::BodyFont())
		.ColorAndOpacity(CPMStyle::TextColor());
}

