# tools

The checkers every Sushi Systems repository carries, and this repository's own tools beside
them. The shared files are copied from SushiSkills and changed there first; the section
"In SushiDSP" says what this repository keeps of its own.

| Path | Checks | Takes |
| --- | --- | --- |
| `check_all.py` | Nothing itself; runs the eight shared checkers, any other `tools/<area>/check_*.py` of the repository, and the checker unit tests; prints only what failed | Repository root |
| `checking/checker.py` | Nothing itself; runs a checker's rule table and prints findings | |
| `checking/ignored_paths.py` | Nothing itself; names the files and folders git ignores, which no checker reads | |
| `checking/copy_shared_tools.py` | Writes the shared files of a repository from a SushiSkills checkout; with `--report`, lists the ones that differ and writes nothing | Repository root, `--from` |
| `testing/check_test_mirror.py` | `tests/` mirrors the source: one `test_<name>` file per header or source file, no test without a unit, no folder without a counterpart | Repository root |
| `testing/check_test_sizes.py` | Every test names one size: a `Small_`, `Medium_` or `Large_` C++ suite prefix, a pytest size mark, a TypeScript `describe` prefix | Repository root |
| `documentation/check_source_comments.py` | C++, GLSL, TypeScript and Python: license block, file header, block ceiling, `//` and `#` runs, separators, history words, `@date` | Files or folders |
| `documentation/check_docs_layout.py` | `docs/` entries, required documents, document names, work folders, status lines, design ceiling, links under `docs/`, reachability from `docs/README.md`, READMEs under `modules/<tier>/`, archive candidates | Repository root |
| `documentation/check_changelog.py` | Headings and entry shape, releases kept live, length, one sentence, nesting, cited places | Repository root |
| `documentation/check_docs_truth.py` | The manual against the source: names and paths a page writes in code exist, examples cite their file, no history words, page ceilings, no paragraph repeated across pages, no em dash in prose, a reason on every allow-list entry | Repository root |
| `layering/check_layering.py` | Declared tiers, upward includes, cross-domain and same-tier includes, reaches into another module's `source/`, declared dependencies | Repository root |
| `logs/check_logging.py` | Library code: console writes outside `K_CONSOLE_PATHS`, empty `catch` and pass-only `except`, a log call without its module's `K_<MODULE>_LOG_CATEGORY`, a logging module without its category header, Python root and f-string logging | Repository root |
| `licensing/write_license_block.py` | Nothing; writes the license block of `source-comments` into every tracked source file, or lists the files that lack it with `--report` | Repository root, `--project` |
| `tests/` | Unit tests for the checker rules; run `python -m unittest discover -s tools/tests` | |

`check_docs_layout.py` reads inline Markdown links, with or without a title, wrapped over a
line or in angle brackets. It does not read reference-style links or a link around an image.

`write_license_block.py` is the one tool here that writes files. It takes its licence lines
from `check_source_comments.py`. It covers `.in` templates and `.inc` fragments by the file they
become. Its options are `--closed` for a closed repository, `--skip GLOB` for generated files and
`--upstream PATH=LINE;LINE` for a ported file; a later run without `--upstream` keeps those lines.
A `--report` run that prints nothing proves a repository's headers are in place.

Every checker takes `--report` and `--rule NAME`, exits 1 on findings, 0 when clean or
reporting, 2 on a bad path. Python 3.11, standard library only.

## Per-repository settings

`K_` constants at the top of each checker. `check_layering.py` does nothing until
`K_TIER_ORDER` lists the repository's tiers, lowest first. `K_COPYRIGHT_LINE` and
`K_LICENSE_LINES` in `check_source_comments.py` hold the repository's licence lines; a closed
repository sets `K_LICENSE_LINES` to `("All rights reserved. No licence is granted.",)`.

The constants that differ between repositories are read from `tools/settings.py`, which
`copy_shared_tools.py` writes only when a repository has none and never overwrites.

## Copying the shared tools

`copy_shared_tools.py` runs from a SushiSkills checkout, never from this repository's copy:

```
python D:/Projects/sushiskills/tools/checking/copy_shared_tools.py . --from D:/Projects/sushiskills
python D:/Projects/sushiskills/tools/checking/copy_shared_tools.py . --from D:/Projects/sushiskills --report
```

The first writes every shared file that differs, under this repository's own licence header; the
second lists the differences and exits 1 when there are any. `tools/settings.py` and
`tools/README.md` are kept, and so is every tool that is not shared.

## In SushiDSP

This repository sets `K_TIER_ORDER` to `("foundation", "nodes", "catalog", "session")`. In
`settings.py`, `K_LOG_PREFIX` is `SD` and `K_CONSOLE_PATHS` names
`modules/foundation/benchmark_harness`, whose table is the bench's output, and
`modules/foundation/log`, whose stderr sink writes the console.

`sd check` runs the eight shared checkers in the table of `cli/sushidsp/services/project.py`.
`check_all.py` runs the same eight and the checker unit tests, and any checker of this
repository's own; it has none today.

Two folders here are SushiDSP's own and are not checkers:

| Folder | Holds |
| --- | --- |
| [`dsp/`](dsp/README.md) | Design-time scripts that generate the half-band and resampler tables, the HRTF asset and the room reference fixtures |
| [`spice/`](spice/README.md) | `export.py`, which regenerates the CSVs under `docs/reference/spice/` from the committed schematics |
