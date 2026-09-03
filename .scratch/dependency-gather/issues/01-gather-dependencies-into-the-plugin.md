# Gather an Entry Point's outside dependencies into the Modding Plugin

Status: `needs-triage` — the behaviour below is landed and verified; the three gaps at the bottom
are what a maintainer still has to rule on.

A Chunk's Primary Asset Label gathers only what lives under its own mount. Verified by listing a
built `pakchunk10-Windows.pak`, which held the Modding Plugin's own `Map.umap`/`NewMap.umap` and
nothing else — `bApplyRecursively` does not reach across mounts, and the docs that said a dependency
is cooked in wherever it lives were wrong. So an Entry Point already sitting inside the Modding
Plugin still published without the `/Game/` meshes and materials it reaches, and loaded in a Convai
product with that content missing.

Landed on `fix/modding-plugin-convai-dependency` (`71aa9b4`, `e55b7b2`, `24f86a8`):

- **`GatherDependenciesIntoPlugin`**, a Command for an Entry Point already inside the Modding
  Plugin: copies its outside dependencies under the mount and repoints every in-plugin package in
  the closure at the copies — the level *and* the World Partition external actor packages that are
  what actually hold the references. Offered at pick time through a modal.
- **Engine content is an outside dependency** and is copied in
  (`EnginePolicy::CopyIntoDestination`). Only the Convai SDK content root and `/ConvaiHTTP/` stay
  excluded, because every Convai product ships those.
- **`FCPM_DependencyCopyOptions::AdditionalPackagesToFixup`** — the reference fixup now also
  rewrites packages it did not copy, which is what repoints an Entry Point that was already inside.
  `FCPM_DependencyCopyReport::bReferencesFixedUp` reports whether it worked.
- **The generated Modding Plugin's `.uplugin` declares the Convai SDK**, so
  `AssetValidator_AssetReferenceRestrictions` stops rejecting the BP chatbot component the tool adds
  to an Avatar.

Verified: the build succeeded, 55/55 automation tests pass, and an adversarial 38-agent review ran
with every confirmed finding fixed.

Left open on purpose:

- The publish path never re-checks for outside dependencies. Only the pick-time dialog offers the
  gather, so an Entry Point that grows a `/Game/` reference afterwards still publishes broken.
- Engine **plugin** content (Niagara, Megascans) counts as engine and is copied in with it.
- No automation test covers the gather itself.
- `RelocateEntryPointIntoPlugin` still ignores `bReferencesFixedUp`. Only the gather fails on it, so
  a relocate whose repointing failed still reports success.

Overlaps with `.scratch/overnight-fixes/`, which was opened against the same code:

- Its issue 01 asks what the `/ConvAI/` exclusion is really serving. The `.uplugin` change here
  answers the validator half — the reference is declared, so
  `AssetValidator_AssetReferenceRestrictions` accepts it without the SDK being copied. The runtime
  half of that question is untouched and still open.
- Its issue 02 ("Copy into plugin copies nothing") predates the engine-content flip: the relocate
  path used to skip `/Engine/` and now copies it, so re-check the symptom against this branch before
  hunting it.

## Comments
