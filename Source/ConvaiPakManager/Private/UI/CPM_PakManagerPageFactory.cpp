// Copyright 2025 Convai Inc. All Rights Reserved.

#include "UI/CPM_PakManagerPageFactory.h"

#include "UI/SCPM_PakManagerPage.h"

FCPM_PakManagerPageFactory::FCPM_PakManagerPageFactory()
	: FPageFactoryBase(ConvaiEditor::Route::E::PakManager)
{
}

TConvaiResult<TSharedRef<SWidget>> FCPM_PakManagerPageFactory::CreatePage()
{
	TSharedRef<SWidget> Page = SNew(SCPM_PakManagerPage);
	return TConvaiResult<TSharedRef<SWidget>>::Success(Page);
}
