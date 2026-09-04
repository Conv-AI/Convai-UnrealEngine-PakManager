# Drop "(fixed by project)" from the Asset type chip

Status: `needs-triage` — done and seen in the editor: the chip reads "Avatar".

The Asset type chip reads "Scene (fixed by project)" / "Avatar (fixed by project)". Show just
"Scene" / "Avatar".

[SCPM_AssetDetailPanel.cpp:516-518](../../../Source/ConvaiPakManager/Private/UI/SCPM_AssetDetailPanel.cpp#L516).

The parenthetical was explaining why the chip is not editable. If that still needs saying, it
belongs in the tooltip, not in the label.

## Done when

The chip reads "Scene" or "Avatar".

## Done — 2026-09-04

`SCPM_AssetDetailPanel.cpp` — the chip is now `LOCTEXT("SceneChip", "Scene")` /
`LOCTEXT("AvatarChip", "Avatar")`, and the sentence it used to carry moved to `.ToolTipText`:
"Decided by the Convai Modding Tool when this project was generated; it cannot be changed here."

Seen in the editor: the panel draws **Avatar** with nothing after it
(`Saved/VibeUE/Captures/m04-panel-front-on-1-2.png`). The tooltip itself was not captured — a
synthetic hover does not raise Slate's tooltip window, and the editor sits behind other windows on
this desktop, so a screen grab cannot catch one either. What is verified is the label; the tooltip is
a code read. See issue 11, M06.
