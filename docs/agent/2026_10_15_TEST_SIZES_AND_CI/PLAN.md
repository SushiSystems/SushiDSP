# Test sizes and staged CI

**Status:** Open: S1 to S6 done inline, as the owner chose; the workflows have not run on GitHub

Adopts SushiSkills `ac2242a` in SushiDSP: `testing` "Sizes, scopes and names",
`continuous-integration` "Stages" and "Shape", and `repository-layout/TESTS.md` "Sizes are labels".
The sushicore pilot (`7b6f7a4`, `eea8cd0`, `e9d0e6c`) is the reference for the workflow shape.

## Where SushiDSP stands

- `tests/CMakeLists.txt` labels each case `unit` or `regression`, chosen by its macro. The
  benchmark is its own program but is registered as a CTest test labelled `benchmark`.
- `sd test --suite unit|regression|benchmark|all` selects by those labels. No `--label`.
- `.github/workflows/ci.yml` calls `cmake` and `ctest` directly, uses `@v4` tags, has no `gate`,
  no timeouts, no concurrency, no cache. No `nightly.yml`.
- `cmake/Sanitizers.cmake` has `SUSHIDSP_ENABLE_SANITIZERS` (ASan+UBSan, Clang only); `sd` cannot
  turn it on. There is no TSan option.
- Of 236 test files, 32 use threads, files, fixtures, goldens or the repository's assets and so are
  `medium`; the rest are `small`. No test opens a real device, so nothing is `large` today.

## Tasks

Hierarchy first: the build declares sizes, then the CLI selects them, then CI calls the CLI.

| Task | Files | Accepts when |
| --- | --- | --- |
| S1 Sizes in the build | `tests/CMakeLists.txt`: `sushidsp_register_tests(target source SIZE)` sets `LABELS` to the size and `TIMEOUT` to 60, 300 or 900 s; the 32 medium files listed by name, every other file small; `unit` and `regression` labels removed; the benchmark is no longer an `add_test` | Every registered test carries exactly one of `small`, `medium`, `large` and its timeout (read back with `ctest --print-labels` through the CLI's verbose output); `SUSHIDSP_REGRESSION_TEST` stays as a name-only macro |
| S2 The CLI | `cli/sushidsp/services/project.py`, `cli/sushidsp/cli.py`, `cli/tests/`: `sd test --label small|medium|large` (repeatable) replaces `--suite`; `sd bench [filter]` runs `sushidsp_bench`; `sd build --sanitize address|thread` turns on the matching CMake option in its own build tree | CLI tests pass; `sd test --label small` and `--label medium` run disjoint sets whose sum is the whole suite |
| S3 Sanitizer builds | `cmake/Sanitizers.cmake`: TSan option beside ASan+UBSan; the allocation trap checked under ASan (ASan replaces `operator new` too; the trap either chains to it or the trap tests are measured in the ASan lane by another route, decided once the lane runs) | A Linux Clang build with each option configures and the small set runs green under ASan+UBSan |
| S4 Workflows | `.github/workflows/ci.yml` (pre-merge and post-merge), new `nightly.yml`, `release.yml` calling nightly | Every job calls `sd` and prints the command first; `gate` needs every pre-merge job; `timeout-minutes` on every job; `concurrency` cancels superseded PR runs; ccache keyed on compiler, flags and CMake files; pip cache keyed on `cli/pyproject.toml`; every action pinned to a commit SHA; TSan on push to `main` only; benchmarks and `quarantined` in nightly, not gating |
| S5 Manual and backlog | `docs/CONTRIBUTING.md`, `docs/guides/COMMAND_LINE_INTERFACE.md`, test READMEs that name labels, `docs/design/REMAINING_WORK.md` (the 39 planned tests: device backends `large`, fake CLAP host and windowless ImGui `medium`), `CHANGELOG.md` | `check_all` reports nothing new; no page names `--suite`, `unit` or `regression` as a label |
| S6 Verification | none | One Release build (4 threads) and `sd test --label small` then `--label medium`, both green with their counts; the workflow files are not run here, since no runner is available, and the report says so |

## Not done here

- GitHub settings, such as making `gate` the required check, are the owner's.
- `large` has no test until the device tests of the planned 39 exist.
- The workflows cannot be run from this machine; they are checked by reading and by `actionlint`
  only if it is already installed.

## Decisions for the owner

1. Inline, or waves. Waves would be: S1 alone; then S2 and S3 together (disjoint files); then S4;
   then S5 and S6.
2. `sd test --suite` is removed, not kept as an alias, as the rule names `--label` only.

## Result

- S1: 1234 tests `small` (60 s), 259 `medium` (300 s), none `large`; no `unit`, `regression` or
  `benchmark` label remains in `build/tests/CTestTestfile.cmake`.
- S2: `sd test --label`, `sd bench`, `--sanitize address|thread`; `--suite` removed. CLI tests
  93 of 93 pass.
- S3: `SUSHIDSP_SANITIZE` applies to every module library, the allocation trap and the test
  program. Not built here: the machine is Windows and the lanes are Linux Clang.
- S4: `ci.yml`, `nightly.yml`, `release.yml` call `sd` and print it first, behind `gate`, with
  timeouts, PR cancellation, ccache and pip caches and SHA-pinned actions. They parse as YAML;
  `actionlint` is not installed, and no runner has run them.
- S5: CONTRIBUTING section 5, the CLI guide, the golden and regression READMEs, the backlog.
- S6: Release build with four jobs; `sd test --label small` 1234 of 1234, `--label medium` 259 of
  259.

Left open: the CLI's own tests run through `python -m pytest cli/tests` in the checks job, a
direct call the "CLI is the only door" default does not name. No quarantined lane exists
because no test is quarantined.
