# The declared Convai dependency does not take effect in the running editor

Status: `ready-for-agent`

Selecting a MetaHuman avatar still fails reference validation, **with `71aa9b4` in the branch**:

```
/JBILN5CDNI4TRYELD6CS/BP_Hana illegally references: /ConvAI/MetaHumans/Animations/Convai_MetaHuman_FaceAnim
/JBILN5CDNI4TRYELD6CS/BP_Hana illegally references: /ConvAI/MetaHumans/Animations/Convai_MetaHuman_BodyAnim
/JBILN5CDNI4TRYELD6CS/BP_Hana illegally references: /ConvAI/ConvaiConveniencePack/ConvaiBPComponent/BP_ConvaiChatbotComponent
  - You may only reference assets from EngineContent, ProjectContent, and Plugin:JBILN5CDNI4TRYELD6CS here.
    (AssetValidator_AssetReferenceRestrictions)
```

This is not the open policy question it looks like. [ADR-0011](../../../docs/adr/0011-a-pak-holds-only-its-own-mount.md)
already decided it: the SDK's content is referenced rather than copied, and a pick writes the Convai
dependency into the Modding Plugin's `.uplugin` so the restriction accepts it. `EnsureConvaiDependency`
([CPM_Chunk.cpp:1106](../../../Source/ConvaiPakManager/Private/Chunk/CPM_Chunk.cpp#L1106)) implements
exactly that. The mechanism exists and is still not working.

## The leading hypothesis

**Nothing rebuilds the domain database after the descriptor is written.** Unreal's asset-referencing
policy builds its domain table once and caches it; `IPlugin::UpdateDescriptor` updates the plugin's
descriptor and the file on disk, but the validator goes on consulting the table it built at startup,
in which the Modding Plugin does not depend on Convai. So the declaration is correct, written, and
invisible until the editor restarts.

**Test this first, and it costs nothing:** reproduce, restart the editor, retry the same pick without
changing any code. If it passes after a restart, this is the bug and the fix is to refresh the domain
database after a successful `UpdateDescriptor` — grep the engine for what the Plugins editor calls
when a creator enables a plugin dependency by hand, and call the same thing.

If it still fails after a restart, the declaration is not landing at all, and the suspects become:

- `EnsureConvaiDependency` **warns rather than refuses** by design ("a read-only descriptor should
  not stop a publish"), so every failure here is silent to anyone not reading the Output Log. Check
  the log for its three messages: no plugin of that name mounted, Convai not enabled, descriptor not
  writable.
- `FindEnabledPlugin("ConvAI")` returning null while the SDK is merely discovered.
- The pick path for a MetaHuman not reaching `SetEntryPoint` at all.

Read the `.uplugin` for `JBILN5CDNI4TRYELD6CS` on disk before doing anything else. Whether Convai is
in its `Plugins` array with `bEnabled` true splits the two branches above immediately.

## While you are in here

The same silent-warn design is worth revisiting. A creator whose descriptor could not be written gets
a Pak that fails validation at cook time with a message naming neither the descriptor nor the
setting. Warning rather than refusing is right for a *publish*; at *pick* time, where the creator is
standing right there and the fix is one file, it should be reported to them — `SetEntryPoint`'s
`OutSetupNotes` explicitly does not carry it today
([ConvaiPakEditorSubsystem.h:222](../../../Source/ConvaiPakManager/Public/ConvaiPakEditorSubsystem.h#L222)).

## Related reports from the same session

Check both against the fix here before treating either as its own bug:

- **"Dependencies button is not showing the correct dependencies."** `ListDependencies`
  ([ConvaiPakEditorSubsystem.cpp:753](../../../Source/ConvaiPakManager/Private/ConvaiPakEditorSubsystem.cpp#L753))
  stops its walk at `ContentEveryProductShips()` — the SDK root and `/ConvaiHTTP/` — so SDK
  references appear in neither bucket. That is deliberate per ADR-0011, but it means the window
  cannot show a creator the very references the validator is rejecting. Decide whether "correct"
  here means listing them in a third bucket ("referenced, not copied") rather than hiding them.
- **"Assets in the ControlRig module are not getting copied."** First check whether the missed ones
  sit under the SDK root — if they do, same exception, same story. If they do not, this is a real
  gap in the walk and needs its own issue with evidence.

## Done when

A MetaHuman avatar picks and cooks with no `AssetReferenceRestrictions` errors, in an editor that was
already running when the pick happened. Whatever the cause turns out to be, a failure to declare the
dependency reaches the creator rather than only the Output Log. Add coverage: `EnsureConvaiDependency`
has none, and ADR-0011 already notes the gather has no automation test at all.
