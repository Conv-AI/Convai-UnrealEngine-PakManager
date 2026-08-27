// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/**
 * The Pak Manager's own style set: the Convai black/dark-green palette over stock editor styling.
 *
 * Small on purpose. Fonts, control heights and most widget styles come from FAppStyle so the panel
 * reads as a native editor tool; this set only recolors what carries the brand - surfaces, the
 * primary/secondary/danger buttons, status colors and the focus accent. See docs/adr/0009 and
 * .scratch/slate-ui-rebuild/design.md.
 */
class FCPM_PakManagerStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static const ISlateStyle& Get();
	static FName GetStyleSetName();

	/** Palette tokens from the UI spec. Widgets take colors from here, never from literals. */
	struct FPalette
	{
		static const FLinearColor Canvas;        // #111512 tab background
		static const FLinearColor Panel;         // #1A211C raised grouped areas
		static const FLinearColor Hover;         // #263229 hover/selection surface
		static const FLinearColor Border;        // #344139 quiet separators
		static const FLinearColor TextPrimary;   // #F1F5F1
		static const FLinearColor TextSecondary; // #AEB8AF
		static const FLinearColor GreenPrimary;  // #2FAE62 primary action, active
		static const FLinearColor GreenBright;   // #78D99B focus, success emphasis
		static const FLinearColor GreenDeep;     // #165C34 pressed/low-emphasis
		static const FLinearColor Warning;       // #DCA94A missing/stale pak
		static const FLinearColor Error;         // #E26060 validation/destructive
	};

	// Registered style keys, so callers and the set cannot drift apart on spelling.
	// Brushes: "CPM.Canvas", "CPM.Panel", "CPM.Hover", "CPM.Border", "CPM.ThumbnailFrame"
	// Buttons: "CPM.Button.Primary", "CPM.Button.Secondary", "CPM.Button.Danger"
	// Text:    "CPM.Text.Title", "CPM.Text.Section", "CPM.Text.Secondary", "CPM.Text.Hint"
	// Icon:    "CPM.TabIcon"

private:
	static TSharedRef<FSlateStyleSet> Create();

	static TSharedPtr<FSlateStyleSet> StyleInstance;
};
