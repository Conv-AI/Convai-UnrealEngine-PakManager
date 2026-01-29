// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Utility/CPM_Utils.h"
#include "Types/CPM_WidgetTypes.h"

class SCPM_KeyValueList;
class SCPM_Button;
class SImage;

/**
 * Main Pak Manager Window - Slate UI for managing pak file creation and uploads
 */
class CONVAIPAKMANAGEREDITOR_API SCPM_PakManagerWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCPM_PakManagerWindow) {}
	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs);

	/** Get the currently selected asset type */
	ECPM_AssetType GetSelectedAssetType() const;

	/** Get all asset info as key-value pairs */
	TArray<FCPM_KeyValuePair> GetAssetInfoPairs() const;

	/** Get a specific value by key from the asset info */
	FString GetAssetInfoValue(const FString& Key) const;

	/** Get asset info as JSON string */
	FString GetAssetInfoAsJson() const;

private:
	// UI Elements
	TSharedPtr<SCPM_KeyValueList> AssetInfoList;
	TSharedPtr<SImage> ThumbnailImage;
	TSharedPtr<SCPM_Button> CaptureThumbnailButton;
	TSharedPtr<SCPM_Button> PackageButton;
	TSharedPtr<SCPM_Button> CreateButton;
	TSharedPtr<class STextBlock> StatusText;

	// Current state
	FSlateBrush ThumbnailBrush;

	// UI Building helpers
	TSharedRef<SWidget> BuildHeaderSection();
	TSharedRef<SWidget> BuildAssetInfoSection();
	TSharedRef<SWidget> BuildThumbnailSection();
	TSharedRef<SWidget> BuildActionButtonsSection();
	TSharedRef<SWidget> BuildStatusSection();

	// Config loading
	TArray<FCPM_KeyValuePair> LoadAssetConfigFromJson();
	void PopulateAutoValues(TArray<FCPM_KeyValuePair>& Pairs);

	// Event handlers
	void HandleAssetInfoChanged(const TArray<FCPM_KeyValuePair>& Pairs);
	FReply HandleCaptureThumbnailClicked();
	FReply HandlePackageClicked();
	FReply HandleCreateClicked();

	// Helpers
	void UpdateStatus(const FString& Message, bool bIsError = false);
};
