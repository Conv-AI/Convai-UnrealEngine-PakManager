# Convai Pak Manager

Turns the Unreal project a creator is working in into an uploadable package, and publishes it to
Convai so that Convai products can load it.

## Language

### Inherited

This repo runs its work on the Convai Job System and uses that glossary unchanged: **Workflow**,
**Job Queue**, **Job**, **Workflow Handle**, **Workflow Context**, **Workflow Input**, **Declared
Output**, **Precheck**, **Skip**, **Cancel**, **Force Cancel**, **All-group**, **Any-group**.
Defined in that plugin's `CONTEXT.md`; never redefined here.

### The interface

**Command**:
One named thing a caller asks the Pak Manager to do. The caller may be the editor UI, a script, or
a test, and the Command is identical whichever it is — that interchangeability is the whole reason
the concept exists.
Distinct from **Job**: a Command is what was *asked for*; a Job is one step of the work that answers
it. Answering one Command may take a Workflow of several Jobs, or no Job at all.
_Avoid_: action, operation, request — the first two are the Job System's reserved words, and the
third names the Job System's own start-a-workflow type.

### The thing being published

Four concepts wore the word "asset" between them. Only the last one keeps it.

**Source Package**:
One Unreal package a creator wants published — a level, a blueprint, a material. Many of them make
up one publishable thing; none of them is one on its own.
_Avoid_: asset, uasset, content

**Entry Point**:
The one **Source Package** in a Chunk that a Convai product opens — the level for a Scene, the
blueprint for an Avatar. A Chunk gathers everything in its label's reach and does not on its own say
which of those things is the thing to load; the Entry Point does. Its kind must match the **Asset
Type**.
The UI does NOT use this term. It labels the field "Selected asset", because a creator reads "entry
point" as a place — it sits two rows from "Spawn point" — and because one general label is right
where the code needs a general concept: the field holds a level path or a blueprint path and the
creator only ever meets one of them. "Entry Point" stays the word in code, in the Commands and here.
Still not "source package", which names any member of the Chunk, not the distinguished one.
_Avoid_: source package (for this), main asset; and "Entry point" as a UI label

**Chunk**:
The Source Packages gathered into one publishable unit by one Primary Asset Label, identified by
that label's **Chunk ID**. The unit of packaging AND the unit of publishing — one Chunk is one
**Asset**, never a part of one.
Defined by the label alone, and by nothing about *where* the label lives: a project may label a
generated **Modding Plugin**, or its own content directly, and both are ordinary Chunks.
_Avoid_: bundle, package, group

**Modding Plugin**:
The content-only plugin, uniquely named, that the Convai Modding Tool generates into an external
creator's project, already carrying a Primary Asset Label at its content root. A convention for
where a creator puts things, not a part of what a **Chunk** is — projects set up by hand have none.
_Avoid_: mod folder, content plugin, asset plugin

**Pak**:
The built artefact for one Chunk on one platform. A Chunk yields one Pak per platform, so naming a
Pak takes a platform as well as a Chunk.
_Avoid_: pak file, build, output

**Asset**:
The record on Convai's servers that a Chunk is published as, and the thing Convai products load.
Carries the name, description, thumbnail, type and version the creator supplies.
_Avoid_: entity, upload, scene — the last is a *kind* of Asset, not a synonym for one

**Environment**:
The Convai backend a **Publish** reaches, identified by the base URL its requests resolve to.
Derived from that URL at the moment a request is built, never chosen — a setting that disagreed with
the URL would name a backend the bytes never reached. Custom URLs are Environments like any other.
_Avoid_: backend, server, stage, target — the first two name machines, the last two imply a chosen
slot

**Draft**:
What the creator has authored about a Chunk's **Asset** — name, description, **Entry Point** — kept
at chunk level because it is the same for every **Environment**. Distinct from the per-Environment
metadata cache the server echoes back.

**Raw Project Archive**:
The creator's project source, archived and published alongside the Paks so the Chunk can be rebuilt
later from what made it. Not a build artefact — the inputs, not the output.
_Avoid_: raw zip, source zip, backup — though `raw` is what it is called on the wire

**Version**:
One named slot of an Asset that holds one uploaded artefact, named for what produced it — an engine
and platform for a Pak, `raw` for the **Raw Project Archive**. One Asset holds several, which is how
a single Asset serves more than one engine version and platform at once, and is what makes deleting
one artefact without deleting the Asset possible.
_Avoid_: revision, release, build number — a Version names a *variant*, not a point in time

**Publish**:
Taking a Chunk all the way from the creator's project to a usable Asset: everything from gathering
its content to the last artefact landing in its **Version**. One request by the creator, however
many steps it takes.
_Avoid_: upload, export, deploy — upload is one step of a Publish, not the whole of it

**Publish Policy**:
Which platforms a Publish builds for, at which build configuration, and whether it includes the Raw
Project Archive. Held by Convai and the same for every creator, so that what a Publish produces can
be changed without every creator updating their tools. States the DEFAULT, not the permitted maximum
— see **Platform Selection**.
_Avoid_: settings, options, preferences — a creator does not choose these

**Platform Selection**:
The platforms one **Publish** actually builds and uploads. Defaults to what the **Publish Policy**
names and is chosen per Publish, never stored as a project setting — an override that outlives the
run that needed it is how a project silently keeps publishing a platform nobody remembers agreeing
to. May both add a platform the Policy omits and drop one it names; it is the only thing that may
add, and it never extends to the **Raw Project Archive**.
_Avoid_: platform override, forced platforms, target platforms — the first two name the exceptional
use for the ordinary control, and a Selection that matches the Policy is the common case

**Asset Type**:
What kind of thing an Asset is — Scene or Avatar. Fixed when the Modding Tool generates the project,
not chosen at publish time, which is why the creator is shown it rather than asked for it.
_Avoid_: category, entity type

## Relationships

- A **Command** is invoked by exactly one caller and may start at most one **Workflow**
- A **Workflow** started by a **Command** is identified to that caller by its **Workflow Handle**
- A **Command** that needs no asynchronous work starts no **Workflow** and answers immediately
- A **Modding Plugin** holds many **Source Packages** and exactly one Primary Asset Label
- That label gathers every **Source Package** in the plugin into one **Chunk**
- A **Chunk** builds to one **Pak** per platform
- A **Chunk** publishes as at most one **Asset** per **Environment**
- A **Draft** belongs to a **Chunk**, not to an **Environment**
- An **Asset** has exactly one **Asset Type**, decided before the creator ever opens the Pak Manager

- The Pak Manager is the ONLY thing that edits an **Asset**'s name, description, type or thumbnail.
  A creator has no other way in — no dashboard, no web editor — so what the creator's project says
  is authoritative and the server never disagrees with it on its own

- A creator's project settings may make a **Publish** do LESS than the **Publish Policy** asks —
  build no **Pak** that is already on disk, send no **Raw Project Archive** the **Asset** already
  holds — and never more. A *setting* only skips work whose result is already there

- The **Platform Selection** is the exception, and the only one: it may both add and remove
  platforms. The **Publish Policy** states what a project publishes *by default*, not what it is
  permitted to publish, because the two diverge for a real customer — Convai agrees to host a Linux
  build for one enterprise project while the published policy names Windows alone, and the reverse
  once Linux is general but one creator should send Windows only. Neither is expressible by a policy
  that is the same for every creator, and neither is worth a policy branch per customer.
  The **Raw Project Archive** is NOT covered by this: it still only subtracts, because a Publish
  that adds an archive nothing expects sends the creator's whole project somewhere it was not asked
  for. Whether Convai *serves* a Version it did not ask for stays Convai's decision; this only
  governs what a Publish uploads

## Flagged ambiguities

- **~~Whether the Pak Manager can mint a Chunk~~ — resolved: yes, the tool mints and repairs the
  Chunk's label when the project has none.** A Chunk still exists only where a Primary Asset Label
  exists; the tool now authors that label rather than only discovering it, because a project without
  one is otherwise a dead end the creator can only leave by learning what a label is.

- **~~How many Chunks a project holds~~ — resolved: it depends on the project, not on the tool.**
  A Chunk is whatever a Primary Asset Label gathers, so a project has as many as it has labels.
  Externally that is one, because the Modding Tool authors one; internal projects carry over a
  hundred. Both are ordinary projects to the Pak Manager. The one-Chunk rule for creators is a
  policy the tool applies, not a fact about what a Chunk is.

- **Losing a creator's on-disk state is unrecoverable, and the documentation knows it.** The
  identity linking a Chunk to its published Asset exists only in the creator's project, and now
  once per **Environment**, so deleting it orphans that Environment's Asset — no update, no delete,
  ever. The Modding Tool documentation warns creators never to touch that folder, which is a warning
  standing in for a recovery path that does not exist. Whether one should exist is unresolved.
