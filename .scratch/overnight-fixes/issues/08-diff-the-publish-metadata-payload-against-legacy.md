# Diff the publish metadata payload against legacy, field for field

Status: `ready-for-agent`

Carried from [#263](https://github.com/ar-convai/ConvaiTask/issues/263), register item 2. **#263's
own recommendation is that this is the next thing to do, ahead of the Scene audit** — the Scene gap
merely leaves something unbuilt, while a mismatch here is silent and corrupts every published
record.

Legacy `GetCreateMetaData` (97 nodes) and `GetUpdateMetaData` (67 nodes) have never been diffed
against the tool's `FillRequiredMetadataFields` / `ComposePakMetadataAt`. The PRD's own words: "A
silent mismatch here corrupts every published record."

Go field by field. For each one in the legacy graphs, find its counterpart, and record: present and
matching, present but differently named or typed, or missing. A missing field that the server
tolerates today is still worth logging — tolerated is not the same as unused.

Do not stop at the create path. Update is the one that overwrites a record that already exists, so a
wrong field there destroys data rather than merely publishing it wrong.

## Done when

Every field in both legacy graphs is accounted for in a written diff, every mismatch is either fixed
or logged as its own issue with the reason it was left, and the fixed ones have a test that reads
the composed payload rather than trusting the composer.
