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
| A golden file | A row in `tests/golden/README.md` saying what changed |

Before a commit that touches `docs/`, both of these print nothing:

```bash
python tools/documentation/check_docs_layout.py .
python tools/documentation/check_changelog.py .
```

`sd check` runs those two and the layering checker. `check_source_comments.py` is run by hand;
[`tools/README.md`](../tools/README.md) describes all four.

## 3. Before you start coding

- One logical change per commit. A refactor and a behaviour change together are hard to review
  and hard to revert.
- Keep the layer boundaries. The core under `include/SushiDSP/` and `src/` depends on the C++17
  standard library and SIMD intrinsics only. A module under `modules/nodes/` depends on the
  core and on no other node, device or host. `tools/layering/check_layering.py` checks the
  order of the three tiers under `modules/`; nothing checks the areas inside the core yet.
- A decision that narrows scope is written down in the design document or the work folder's
  plan. The WDF adaptor tree, deferred until a circuit needs it, is the pattern.

## 4. Code conventions

The `cpp-code-style`, `python-code-style` and `source-comments` skills are the rules. Where the
tree differs from them today, the difference is listed here and in
[`reference/KNOWN_ISSUES.md`](reference/KNOWN_ISSUES.md); new code follows what the tree does
until the migration in
[`design/SUSHI_STANDARDS_MIGRATION.md`](design/SUSHI_STANDARDS_MIGRATION.md) phase 3 changes
both at once.

| Rule | In this tree |
| --- | --- |
| Language | C++17; no C++20 or C++23 feature |
| Braces | Allman |
| Names | Types `PascalCase`, functions and variables `snake_case`, members with a trailing underscore |
| Constants | `kPascalCase`, such as `RigGraph::kMaxChannels` |
| Files | Named after their class, `RigGraph.hpp` |
| License block | Written by `tools/licensing/write_license_block.py`, never typed by hand |
| Comments | Doxygen, as `source-comments` states: what a thing does, never why |
| Real-time path | `IAudioNode::process()` and everything it calls allocates nothing, locks nothing, makes no system call and throws nothing |

A class that will sit on the audio path gets a `*ProcessPerformsNoHeapAllocation` case in
`tests/functional/unit/test_rt_safety.cpp`, which replaces `operator new` and fails when the
audio path reaches it. Every scratch buffer is sized in the constructor or in `prepare()`.

## 5. Tests

All tests build into one binary, `sushidsp_tests`, from `tests/CMakeLists.txt`.

| Folder | Holds |
| --- | --- |
| `tests/functional/unit/` | One `test_<snake_case_name>.cpp` per class or behaviour |
| `tests/functional/regression/` | One golden-file comparison per product |
| `tests/golden/` | The pinned `.f32` renders and the record of each regeneration |
| `tests/fixtures/` | Reference data from measured rooms, each with its own README |
| `tests/measure/` | Headers that measure THD, alias floor, frequency response and nulls |
| `tests/circuits/` | Netlists and block wrappers the SPICE comparisons share |
| `tests/common/` | `test_main.cpp` and fixtures shared across suites |
| `tests/benchmark/` | The timing harness, one CTest case under the `benchmark` label |

- A GoogleTest suite name carries its label as a prefix: `TEST(Unit_Dsp, SomeCase)` runs under
  `sd test -s unit`, `TEST(Regression_Dsp, SomeCase)` under `sd test -s regression`.
- A golden file is one second of mono float32 per node at 48 kHz and at 96 kHz, compared
  against a fresh render at −80 dB RMS residual. A deliberate behaviour change regenerates
  them with `SUSHIDSP_REGENERATE_GOLDEN=1 sd test -s regression`; a golden file is never
  regenerated to silence a failure whose cause is unknown.
- A numerical test compares against a value derived independently of the implementation.
- The alias-floor cases in `tests/functional/unit/test_measure.cpp` carry the `Regression_Dsp`
  prefix, because each renders several seconds of audio and `sd test -s unit` is the edit loop.
- Run `sd test` before every commit, and `sd test --suite all` before a change to the audio
  path lands.
- A change to the audio path is timed before and after with `sd test -t release -s benchmark`,
  and the figures go in the commit message. A change meant to cost nothing keeps the golden
  files bit-identical.

## 6. Commit messages

The `commits` skill is the rule. Name the roadmap phase (`D0`, `DS2`, `DE`) in the body when a
change belongs to one; the phases are listed in [`design/SUSHIDSP.md`](design/SUSHIDSP.md) §9.
