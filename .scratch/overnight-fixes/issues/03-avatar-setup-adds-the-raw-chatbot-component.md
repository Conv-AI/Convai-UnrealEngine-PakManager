# Avatar setup adds the raw ChatbotComponent, not the BP one

Status: `needs-info`

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
