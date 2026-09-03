# "Copy into plugin" copies nothing

Status: `ready-for-agent`

The button reports success, or reports nothing, and the asset is still outside the Modding Plugin.

`HandleRelocateEntryPoint` at
[SCPM_AssetDetailPanel.cpp:1009](../../../Source/ConvaiPakManager/Private/UI/SCPM_AssetDetailPanel.cpp#L1009)
→ `RelocateEntryPointIntoPlugin` at
[ConvaiPakEditorSubsystem.cpp:599](../../../Source/ConvaiPakManager/Private/ConvaiPakEditorSubsystem.cpp#L599).

This is the **relocate** half, not the gather. `GatherDependenciesIntoPlugin` landed and was verified
in `e55b7b2`/`24f86a8`; relocate is its older sibling and did not get the same attention — and
[#263](https://github.com/ar-convai/ConvaiTask/issues/263) explicitly lists the Copy-into-plugin
button under "no visual pass", so there is no prior evidence it works in a real editor at all.

Suspects, in the order they are cheap to check:

- The "this project records no modding plugin to copy into" path at
  [:606](../../../Source/ConvaiPakManager/Private/ConvaiPakEditorSubsystem.cpp#L606) —
  `GetModdingMetadataForChunk` returning a chunk with no plugin name.
- A copy report whose `FailedPackages` is being swallowed by `WhyCopyFailed` and shown as nothing.
- The relocate succeeding on disk but never writing the new package back as the chunk's entry point,
  so the panel still shows the old path and the creator reads that as "nothing happened".
- `bReferencesFixedUp` false — copies landed, nothing points at them. Note the header change in this
  branch: that flag is "ran to its end without reporting an error", not a per-package guarantee.

Check whether the fixup gained by `AdditionalPackagesToFixup` in `24f86a8` needs the same treatment
here. Relocate moves the Entry Point itself, so the packages needing repointing are different from
the gather's, and the newer option may simply not be wired into this path.

## Done when

The button copies a project-content entry point and its closure into the Modding Plugin, the panel
shows the new package path, and the chunk records it. Whatever the failure was, it reports itself to
the creator rather than succeeding silently.
