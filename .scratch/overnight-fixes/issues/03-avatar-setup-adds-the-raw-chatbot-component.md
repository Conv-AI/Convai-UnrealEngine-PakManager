# Avatar setup adds the raw ChatbotComponent, not the BP one

Status: `wontfix` — reading 3 is what happened, and
[issue 01](01-the-declared-convai-dependency-does-not-take-effect.md) closed it.

Reported as: "Add BP_ChatbotComponent instead of ChatbotComponent".

The code already intends to do this.
[CPM_AvatarBlueprint.cpp:267](../../../Source/ConvaiPakManager/Private/Avatar/CPM_AvatarBlueprint.cpp#L267)
loads `BP_ConvaiChatbotComponent` and adds it, and
[:279](../../../Source/ConvaiPakManager/Private/Avatar/CPM_AvatarBlueprint.cpp#L279) *refuses* a
blueprint that already carries the raw C++ `UConvaiChatbotComponent`, telling the creator to replace
it by hand. `CPM_AvatarBlueprintTest.cpp` covers both.

**A third reading, and the likeliest one.** The validator errors in issue 01 include
`/ConvAI/ConvaiConveniencePack/ConvaiBPComponent/BP_ConvaiChatbotComponent` as an illegal reference —
which means the tool *did* add the BP component, and what was actually seen was the reference to it
being rejected. If so this is not a separate bug at all, and issue 01 closes it. Check that before
anything else.

Otherwise the report is one of two things, and they want opposite fixes:

1. **Some other path still adds the raw component.** Find it and change it. Grep for
   `UConvaiChatbotComponent::StaticClass()` outside the refusal check and the tests.
2. **The refusal is what was hit,** and the ask is to stop refusing — swap the raw component for the
   BP one automatically instead of sending the creator away.

Reading (2) is the more likely one given the code, and it is a real usability complaint: the tool
knows exactly what the fix is and makes a human do it. But it is a behaviour change with a reason
behind the current choice, so establish which happened before changing anything.

Reproduce first. If it is (1), fix it. If it is (2), implement the automatic swap, keep the log line
saying it happened, and update `FCPMAvatarRefusesRawChatbotComponent` to match the new behaviour
rather than deleting it.

## Done when

The repro is written down, the right one of the two is fixed, and the test suite reflects the
behaviour that was chosen.

## What the logs say

Reading 3. The tool added the BP component and the validator rejected the reference to it, in the
same second, in the session that produced the report
(`Saved/Logs/Dev_CPM_58-backup-2026.09.03-17.00.17.log`):

```
:3041  ConvaiPakManagerLog: Prepared Avatar blueprint 'NewBlueprint': added BP_ConvaiChatbotComponent, added ConvaiFaceSyncComponent.
:3068  AssetCheck: Error: /JBILN5CDNI4TRYELD6CS/NewBlueprint illegally references:
       /ConvAI/ConvaiConveniencePack/ConvaiBPComponent/BP_ConvaiChatbotComponent - You may only reference
       assets from EngineContent, ProjectContent, and Plugin:JBILN5CDNI4TRYELD6CS here .
       (AssetValidator_AssetReferenceRestrictions)

:3428  ConvaiPakManagerLog: Prepared Avatar blueprint 'BP_Hana': added BP_ConvaiChatbotComponent, added
       ConvaiFaceSyncComponent, assigned Convai_MetaHuman_BodyAnim_C to 'Body', assigned
       Convai_MetaHuman_FaceAnim_C to 'Face'.
:3455  AssetCheck: Error: /JBILN5CDNI4TRYELD6CS/BP_Hana illegally references:
       /ConvAI/ConvaiConveniencePack/ConvaiBPComponent/BP_ConvaiChatbotComponent - ...
:3456-7 ... the same for Convai_MetaHuman_BodyAnim and Convai_MetaHuman_FaceAnim.
```

Twice, on two different blueprints: the BP component went on, and the only thing anyone saw about it
was the validator naming `BP_ConvaiChatbotComponent` as illegal. That error is issue 01's — the
session was running a binary built before `71aa9b4`, so the `.uplugin` never declared Convai. Nothing
in the session log adds a raw `UConvaiChatbotComponent`.

Reading 1 is dead too, by grep: `UConvaiChatbotComponent::StaticClass()` appears in exactly two
places under `Source/` — the refusal at
[CPM_AvatarBlueprint.cpp:279](../../../Source/ConvaiPakManager/Private/Avatar/CPM_AvatarBlueprint.cpp#L279)
and `CPM_AvatarBlueprintTest.cpp`. No path adds it.

## Why this is `wontfix` rather than done

Reading 2 — the refusal was hit and the ask is to stop refusing — cannot be ruled out from the logs:
a blueprint that already carried the raw component would have been refused before any of the lines
above were written, and a refusal the creator dismissed leaves nothing behind. It is not what the
report's own session shows, so nothing is built for it here. `FCPMAvatarRefusesRawChatbotComponent`
is unchanged.

This flips back to `ready-for-agent` — automatic swap, keep the log line saying it happened, update
`FCPMAvatarRefusesRawChatbotComponent` to match — the moment Anmol confirms the refusal is what he
actually hit.
