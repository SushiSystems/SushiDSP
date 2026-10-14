# tools

Reference implementations of the six checkers every Sushi Systems repository carries. A
repository copies this folder whole; a rule is fixed here first and copied out.

| Path | Checks | Takes |
| --- | --- | --- |
| `check_all.py` | Nothing itself; runs the six checkers and the checker tests, prints only what failed | Repository root |
| `checking/checker.py` | Nothing itself; runs a checker's rule table and prints findings | |
| `checking/ignored_paths.py` | Nothing itself; names the files and folders git ignores, which no checker reads | |
| `testing/check_test_mirror.py` | `tests/` mirrors the source: one `test_<name>` file per header or source file, no test without a unit, no folder without a counterpart | Repository root |
| `documentation/check_source_comments.py` | C++, GLSL, TypeScript and Python: license block, file header, block ceiling, `//` and `#` runs, separators, history words, `@date` | Files or folders |
| `documentation/check_docs_layout.py` | `docs/` entries, required documents, document names, work folders, status lines, design ceiling, links under `docs/`, reachability from `docs/README.md`, READMEs under `modules/<tier>/`, archive candidates | Repository root |
| `documentation/check_changelog.py` | Headings and entry shape, releases kept live, length, one sentence, nesting, cited places | Repository root |
| `layering/check_layering.py` | Declared tiers, upward includes, reaches into another module's `source/` | Repository root |
| `licensing/write_license_block.py` | Nothing; writes the license block of `source-comments` into every tracked source file, or lists the files that lack it with `--report` | Repository root, `--project` |
| `tests/` | Unit tests for the checker rules; run `python -m unittest discover -s tools/tests` | |

`check_docs_layout.py` reads inline Markdown links, with or without a title, wrapped over a
line or in angle brackets. It does not read reference-style links or a link around an image.

`write_license_block.py` is the one tool here that writes files. It takes its licence lines
from `check_source_comments.py`, covers `.in` templates and `.inc` fragments by the file they become, `--closed` for a closed repository, `--skip GLOB` for generated
files and `--upstream PATH=LINE;LINE` for a ported file; a later run without it keeps those lines. A `--report` run that prints nothing
proves a repository's headers are in place.

Every checker takes `--report` and `--rule NAME`, exits 1 on findings, 0 when clean or
reporting, 2 on a bad path. Python 3.11, standard library only.

## Per-repository settings

`K_` constants at the top of each checker. `check_layering.py` does nothing until
`K_TIER_ORDER` lists the repository's tiers, lowest first. `K_COPYRIGHT_LINE` and
`K_LICENSE_LINES` in `check_source_comments.py` hold the repository's licence lines; a closed
repository sets `K_LICENSE_LINES` to `("All rights reserved. No licence is granted.",)`.

## In SushiDSP

Everything above is copied from SushiSkills, where a rule changes first. This repository sets
`K_TIER_ORDER` to `("foundation", "nodes", "catalog", "session")` and adds `.superpowers` to
`K_SKIPPED_FOLDERS` in `check_source_comments.py`. `sd check` runs `check_docs_layout.py`,
`check_changelog.py` and `check_layering.py`; `check_all.py` runs all six checkers and the tests.

`check_docs_truth.py` and `check_layering.py` still differ from the SushiSkills reference in
behaviour: the reference skips git-ignored and rule folders, and checks layering by module owner.
Those differences are not yet copied across.

Two folders here are SushiDSP's own and are not checkers:

| Folder | Holds |
| --- | --- |
| [`dsp/`](dsp/README.md) | Design-time scripts that generate the half-band and resampler tables, the HRTF asset and the room reference fixtures |
| [`spice/`](spice/README.md) | `export.py`, which regenerates the CSVs under `docs/reference/spice/` from the committed schematics |
