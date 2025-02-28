// Fill out your copyright notice in the Description page of Project Settings.


#include "ConvaiPakEditorSubsystem.h"
#include "EditorUtilityLibrary.h"

void UConvaiPakEditorSubsystem::GetSelectedAssetPackageName(FString& PackageName)
{
	TArray<FAssetData> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssetData();
	if (SelectedAssets.Num() > 0)
	{
		PackageName = SelectedAssets[0].PackageName.ToString();
	}
}
