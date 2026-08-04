# Issue tracker: Local Markdown

Issues and PRDs for this repo live as markdown files in `.scratch/`. They are tracked in git — `.gitignore` whitelists `/.scratch/` — so an issue survives a fresh clone and is visible to the rest of the team.

GitHub Issues on `Conv-AI/Convai-UnrealEngine-PakManager` are **not** the tracker the skills write to. Mirror something upstream by hand if an outside reporter needs to see it.

## Conventions

- One feature per directory: `.scratch/<feature-slug>/`
- The PRD is `.scratch/<feature-slug>/PRD.md`
- Implementation issues are `.scratch/<feature-slug>/issues/<NN>-<slug>.md`, numbered from `01`
- Triage state is recorded as a `Status:` line near the top of each issue file (see `triage-labels.md` for the role strings)
- Comments and conversation history append to the bottom of the file under a `## Comments` heading

## When a skill says "publish to the issue tracker"

Create a new file under `.scratch/<feature-slug>/` (creating the directory if needed).

## When a skill says "fetch the relevant ticket"

Read the file at the referenced path. The user will normally pass the path or the issue number directly.
