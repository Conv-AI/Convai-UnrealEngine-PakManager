# Drop "(fixed by project)" from the Asset type chip

Status: `ready-for-agent`

The Asset type chip reads "Scene (fixed by project)" / "Avatar (fixed by project)". Show just
"Scene" / "Avatar".

[SCPM_AssetDetailPanel.cpp:516-518](../../../Source/ConvaiPakManager/Private/UI/SCPM_AssetDetailPanel.cpp#L516).

The parenthetical was explaining why the chip is not editable. If that still needs saying, it
belongs in the tooltip, not in the label.

## Done when

The chip reads "Scene" or "Avatar".
