> **Status: superseded by ADR-0009.**

# The UI is built from the SDK's widget kit

The Slate panel is built from the widgets the Convai SDK's editor module already exports — form
fields, cards, progress bars, dropdowns and the shared style set — rather than from a widget library
of the Pak Manager's own.

An abandoned refactor had hand-rolled that library: buttons, labels, text boxes, combo boxes, a
key-value row and list, and a style set, around two thousand lines reimplementing what every creator
already has installed, because the Pak Manager cannot function without the SDK. Reusing it also
means the Pak Manager stops looking like a different product from the SDK panel beside it.

The generic key-value form goes with it. This form has a small, fixed set of fields backed by a
typed structure; a form engine over a fixed schema gives up compile-time checking on every field
name in exchange for flexibility nothing has asked for.

## Consequences

This sets a hard floor on the SDK version a creator must have — an older one is a compile error, not
a degraded panel. The floor must therefore be a version the Modding Tool actually installs into
generated projects, which makes what that tool pins a release constraint on this plugin. If a
creator's per-project custom fields ever become a requirement, the generic form returns then, on
evidence.
