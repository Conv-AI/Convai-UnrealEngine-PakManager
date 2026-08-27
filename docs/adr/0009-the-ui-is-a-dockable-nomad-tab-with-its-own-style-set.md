# The UI is a dockable nomad tab with its own style set

The Pak Manager UI is a nomad tab this plugin registers with the editor's tab manager, styled by a
small style set of its own over stock Slate and editor widgets. It no longer registers a page into
the Convai SDK shell and no longer uses the SDK's widget kit, superseding ADR-0006 and ADR-0007.

The core workflow forced this: a creator selects a level or blueprint in the Content Browser and
presses **Use selected asset** without leaving that context, so the panel must dock beside the
Content Browser and persist in the editor layout. The SDK shell is a fixed standalone `SWindow` and
cannot dock. The shell's widget kit went with it: the kit is styled as a web dashboard (cards,
rounded surfaces), while this panel must read as a native editor tool — compact property rows,
standard control heights — with the Convai black/dark-green palette only as accent.

## Consequences

- The `ConvaiEditor` module dependency is dropped entirely; the SDK's Pak Manager route is left
  unanswered, which the shell already tolerates by design.
- The shell's sign-in no longer fronts the panel. Publishing keeps authenticating exactly as it
  already does — `UConvaiUtils::GetAuthHeaderAndKey()` from the runtime `Convai` module — so nothing
  breaks, but the plaintext-API-key concern ADR-0007 hoped sign-in would retire stays open.
- The compile-time floor on the SDK *editor* module version disappears; only the runtime `Convai`
  module floor remains.
