# tools

The checkers every Sushi Systems repository carries, and this repository's own tools beside
them. The shared files are copied from SushiSkills and changed there first; the section
"In SushiDSP" says what this repository keeps of its own.

| Path | Checks | Takes |
| --- | --- | --- |
| `check_all.py` | Nothing itself; runs the eleven checkers, then the repository's own `<area>/check_<what>.py` files and the tests, and prints only what failed | Repository root |
| `checking/checker.py` | Nothing itself; runs a checker's rule table and prints findings | |
| `checking/ignored_paths.py` | Nothing itself; names the files and folders git ignores, which no checker reads | |
| `checking/copy_shared_tools.py` | Nothing; copies these files, `.github/CODE_OF_CONDUCT.md` and `.github/SECURITY.md` into a repository, keeping its license blocks, `settings.py`, this README and its own tools, or lists the files that differ with `--report` | Repository root, `--from` |
| `settings.py` | Nothing; holds the settings that differ between repositories | |
| `testing/check_test_mirror.py` | `tests/` mirrors the source: one `test_<name>` file per header or source file (`<name>.test.ts` in TypeScript), no test without a unit, no folder without a counterpart, no test that includes a header under `source/`; in a Unity repository, `Assets/<Product>/Tests/` with `<Name>Tests.cs` | Repository root |
| `testing/check_test_sizes.py` | Every test names its size in its source: a `Small_`, `Medium_` or `Large_` C++ suite, a size mark on a pytest file, class or function, a `small:` TypeScript `describe` | Repository root |
| `testing/check_probes.py` | Folders named as probes or benchmarks outside `applications/probes/` and `applications/benchmarks/`, such a program using a test framework or added to the test runner, one named after another of its kind, one its README does not list | Repository root |
| `documentation/check_source_comments.py` | C++, GLSL, TypeScript and Python: license block, file header, block ceiling, `//` and `#` runs, separators, history words, `@date` | Files or folders |
| `documentation/check_docs_layout.py` | `docs/` entries, required documents, document names, work folders, status lines in the four forms, design ceiling, links under `docs/`, reachability from `docs/README.md`, READMEs under `modules/<tier>/`, archive candidates by the date in the status line, roadmap, journal, handover, milestone, priority and to-do files | Repository root |
| `documentation/check_changelog.py` | Headings and entry shape, releases kept live, length, one sentence, nesting, cited places | Repository root |
| `documentation/check_backlog.py` | `REMAINING_WORK.md` and `KNOWN_ISSUES.md`: shape, IDs and the counter, paths, references here and in siblings checked out beside the repository, entry size, a backlog entry for every open design document; `--report` lists the siblings read and the entries naming one | Repository root |
| `documentation/design_status.py` | Nothing itself; finds the design documents and work folders and reads their status lines | |
| `documentation/check_design_format.py` | Design documents: the five sections in order, a status line of at most 160 characters, the phase table and its states, no commit hash, no line that dates built or merged work | Repository root |
| `documentation/check_docs_truth.py` | The manual against the source: named symbols, cited paths, example sources, history words, page length, duplicated passages, em dashes, the allow-list | Repository root, `--source` |
| `layering/check_layering.py` | Declared tiers and domains, upward includes from a module or its tests, cross-domain and unlisted same-tier includes, reaches into another module's `source/`, dependencies the build file lists, internal headers in installed ones | Repository root |
| `logs/check_logging.py` | Console writes outside the paths `settings.py` allows, empty `catch` and `except: pass`, C++ log calls without their module's category, a logging module without `<module>_log_category.hpp`, Python root-logger, named-logger and f-string records | Repository root |
| `licensing/write_license_block.py` | Nothing; writes the license block of `source-comments` into every tracked source file, or lists the files that lack it with `--report` | Repository root, `--project` |
| `routing/summarize_dispatches.py` | Nothing; sums the dispatch lines of every `REPORT.md` per model and effort, for `model-routing` | Repository root |
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
`K_TIER_ORDER` lists the repository's tiers, lowest first. `K_LICENSE_HOLDER` and the four
`K_LICENSE_*_LINES` tuples in `settings.py` hold the repository's licence lines; a closed repository
sets `K_CLOSED`.

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
