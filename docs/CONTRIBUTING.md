# Contributing to SushiDSP

Contributions from outside Sushi Systems are not accepted yet; a bug report or a design
discussion as an issue is the way in. SushiDSP is source-available, free for non-commercial use
under the PolyForm Noncommercial License 1.0.0 (see [`LICENSE`](../LICENSE)); commercial use
needs a licence from Sushi Systems, see [`COMMERCIAL.md`](../COMMERCIAL.md).

The rules this repository follows are the SushiSkills skills named in
[`CLAUDE.md`](../CLAUDE.md). This page holds what is particular to SushiDSP and does not
restate them.

## 1. Getting set up

[`getting_started/INTRODUCTION.md`](getting_started/INTRODUCTION.md) has the first-run
sequence. SushiDSP pins no toolchain: `sd` uses the compiler its configuration names, else an
intel/llvm clang++ when `sd setup` has provisioned one, else a `clang++` on `PATH`, else the
platform compiler. `sd config` reports
which one it chose and `sd env` the environment it builds in. The regression snapshots hold
across compilers because `cmake/SushiDSPCompilerFlags.cmake` pins FMA contraction off for every
toolchain.

Every build, test and run goes through `sd`. `cmake` and `ctest` are not called by hand.

## 2. What a change must carry

A change is finished when these land in the same commit as the code:

| Changed | Also changes |
| --- | --- |
| A module's behaviour, classes, constants or stand-ins | That module's `README.md` |
| Anything a manual page states | That page |
| The phase a design document is in | Its status line and its row in [`design/README.md`](design/README.md) |
| Work opened or closed | [`design/REMAINING_WORK.md`](design/REMAINING_WORK.md) |
| A defect measured and left open, or one closed | [`reference/KNOWN_ISSUES.md`](reference/KNOWN_ISSUES.md) |
| Anything at all | One line under `## Unreleased` in [`reference/CHANGELOG.md`](reference/CHANGELOG.md) |
| A golden file | A row in `modules/foundation/golden_compare/README.md` saying what changed |

Before a commit that touches `docs/`, both of these print nothing:

```bash
python tools/documentation/check_docs_layout.py .
python tools/documentation/check_changelog.py .
```

`sd check` runs all five checkers under `tools/`; [`tools/README.md`](../tools/README.md) describes
them.

## 3. Before you start coding

- One logical change per commit. A refactor and a behaviour change together are hard to review
  and hard to revert.
- Keep the layer boundaries. The foundation tier under `modules/foundation/` depends on the
  C++17 standard library and SIMD intrinsics only, plus the same-tier edges listed in
  `cmake/SushiDSPLayers.cmake`; an edge that table does not list fails to configure. A module
  under `modules/nodes/` depends on the foundation and on no other node, device or host.
  `tools/layering/check_layering.py` checks the order of the four tiers under `modules/`.
- A decision that narrows scope is written down in the design document or the work folder's
  plan. The WDF adaptor tree, deferred until a circuit needs it, is the pattern.

## 4. Code conventions

The `cpp-code-style`, `python-code-style` and `source-comments` skills are the rules. Where the
tree differs from them today, the difference is listed here and in
[`reference/KNOWN_ISSUES.md`](reference/KNOWN_ISSUES.md); new code follows what the tree does
until the migration in
[`design/SUSHI_STANDARDS_MIGRATION.md`](design/SUSHI_STANDARDS_MIGRATION.md) phase 3 changes
both at once. The rules below are the tree today.

| Rule | In this tree |
| --- | --- |
| Language | C++17; no C++20 or C++23 feature |
| Braces | Allman |
| Names | Types `PascalCase`, functions and variables `snake_case`, members with a trailing underscore |
| Interfaces | No `I` prefix: `AudioNode`, `AudioDevice`, `CabinetHost`, `LogSink`, `RoomHost` |
| Constants | `K_UPPER_SNAKE`, such as `RigGraph::K_MAX_CHANNELS` |
| Files | `snake_case`, named after the one primary symbol, `rig_graph.hpp` |
| License block | Written by `tools/licensing/write_license_block.py`, never typed by hand |
| Comments | Doxygen, as `source-comments` states: what a thing does, never why |
| Real-time path | `AudioNode::process()` and everything it calls allocates nothing, locks nothing, makes no system call and throws nothing |

A class that will sit on the audio path gets a `ProcessPerformsNoHeapAllocation` case in its own
test file, armed with `SushiDSP::test::AllocationTrap` from `modules/foundation/allocation_trap`,
which replaces `operator new` and fails the case when the audio path reaches it. Every scratch buffer is sized in the constructor or in `prepare()`.

## 5. Tests

All tests build into one binary, `sushidsp_tests`, from `tests/CMakeLists.txt`.

| Folder | Holds |
| --- | --- |
| `tests/<tier>/<module>/`, `tests/applications/<app>/` | One `test_<snake_case_name>.cpp` per unit, at the path of the unit's header |
| `tests/<tier>/<module>/goldens/` | The pinned `.f32` renders, one folder per unit; the record of each regeneration is in `modules/foundation/golden_compare/README.md` |
| `tests/nodes/room/fixtures/built_room_loss/` | Reference data from measured rooms, each with its own README |
| `modules/foundation/{signal_measure,spice_reference,circuit_blocks,golden_compare,timing_gate,regression_test,allocation_trap,benchmark_harness}/`, `modules/nodes/room_scenes/` | Shared test code, each an internal module: measurement, SPICE comparison, circuit blocks, golden comparison, the timing gate, the regression macro, the allocation trap, the benchmark harness, the room scenes |
| `tests/` | `main.cpp`, the entry point of `sushidsp_tests`; `README.md` |
| `tests/benchmark_main.cpp`, `tests/<owner>/benchmark_<name>.cpp` | The benchmark program `sushidsp_bench`, run by `sd bench` and never by `sd test` |

- Every test carries one size, which `tests/CMakeLists.txt` sets as its CTest label and its
  timeout: `small` (one thread, no file outside the build tree, 60 s), `medium` (threads, a
  temporary folder, a fixture, a golden or a shipped asset, 300 s) or `large` (a real audio
  device, 900 s). A file is small unless `SUSHIDSP_MEDIUM_TEST_SOURCES` lists it.
  `SUSHIDSP_REGRESSION_TEST`, from
  `modules/foundation/regression_test/include/SushiDSP/foundation/regression_test/regression_test.hpp`,
  names a case that keeps a fixed defect fixed; it carries a size like any other.
- A golden file is one second of mono float32 per node at 48 kHz and at 96 kHz, compared
  against a fresh render at −80 dB RMS residual. A deliberate behaviour change regenerates
  them with `SUSHIDSP_REGENERATE_GOLDEN=1 sd test --label medium`; a golden file is never
  regenerated to silence a failure whose cause is unknown.
- A numerical test compares against a value derived independently of the implementation.
- `sd test --label small` is the edit loop. Run `sd test --label small --label medium`, what a
  pull request runs, before every commit, and `sd test` with every size before a change to
  the audio path lands.
- A change to the audio path is timed before and after with `sd bench --type release`, and
  the figures go in the commit message.
- CI runs the sizes by stage. A pull request runs `python tools/check_all.py .`, the build on
  Linux and Windows, `small` and `medium` tests on both, and `small` again under
  `--sanitize address`; `gate` is the check `main` requires. A push to `main` adds the
  ThreadSanitizer lane. `nightly.yml` adds `sd bench`, and `release.yml` runs nightly before it
  packages. `large` tests run on no runner: before a release they are run with
  `sd test --label large` on a machine with the device, and the proposal carries the output. A change meant to cost nothing keeps the golden
  files bit-identical.

## 6. Commit messages

The `commits` skill is the rule. Name the roadmap phase (`D0`, `DS2`, `DE`) in the body when a
change belongs to one; the phases are listed in [`design/SUSHIDSP.md`](design/SUSHIDSP.md) §9.
