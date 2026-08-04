// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Factories/IPageFactory.h"

/**
 * Supplies the Pak Manager page to the Convai editor shell.
 *
 * The Pak Manager stays its own plugin and visits the shell rather than moving into it: the SDK
 * ships to every Convai developer, and this tool serves only creators who generated a project with
 * the Modding Tool. See docs/adr/0007.
 *
 * The SDK declares the route; this supplies what fills it. Without this plugin installed the route
 * exists and nothing answers for it, which is the intended shape - the SDK carries a route it never
 * has to reason about.
 */
class FCPM_PakManagerPageFactory : public FPageFactoryBase
{
public:
	FCPM_PakManagerPageFactory();

	virtual TConvaiResult<TSharedRef<SWidget>> CreatePage() override;
	virtual FName GetFactoryType() const override { return TEXT("FCPM_PakManagerPageFactory"); }
};
