# RW-1 Tag v0.1.0 or fold its heading back into Unreleased

The row of this entry is in `docs/backlog/GENERAL.md`. This file holds what did not fit the row.

- State: decision
- Done when: the `## v0.1.0` heading of the changelog has a tag behind it, or is folded into `## Unreleased`
- Where: `docs/reference/CHANGELOG.md`

The changelog and commit `de235ac` declare the release and no tag exists.

The `## v0.1.0` heading has no tag behind it. `git tag -l` prints nothing. Cause: the release was
declared and not tagged.
