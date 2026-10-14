# Test tree to the testing skill

**Status:** Open. Decisions taken on 2026-10-15 (section 2). T1 is committed with this folder.
T0 (include path) and T2 to T10 are planned in PLAN.md. No test is moved or written yet.

## 1. What the owner asked

Re-read the SushiSkills rules and refactor `tests/` to match them. The rules that govern it are
`testing`, `repository-layout` (`SKILL.md` and `TESTS.md`), `project-tools` and `documentation`.

## 2. Owner decisions

| Question | Answer (date) |
| --- | --- |
| How the plan runs | Inline: one session, in hierarchy order, no workers (2026-10-14) |
| Build shape | Mirror now, one binary `sushidsp_tests`; the 39 application sources stay in it until the host is a library (2026-10-14) |
| Units with no test of their own | Written, layer by layer: foundation, then nodes, then applications, one task per group (2026-10-15). The plan itself was requested on 2026-10-14. |
| Shared test support | Five responsibility-named internal modules, as proposed in PLAN.md T5 (2026-10-15) |
| `timing_gate` and the benchmark program | `timing_gate` is an internal module of the foundation tier. Benchmark programs stay with the owner of the unit they time (2026-10-15) |
| Commit of T1 | Commit now, staged by path (2026-10-15) |
| Include path form | Move to the tier form, `<Project>/<tier>/<module>/<name>.hpp`, and move the headers with it (2026-10-15). See PLAN.md T0. |

The first answer for the untested units assumed 28 headers. The count is 99 (section 3).

## 3. Where the tree stands, measured 2026-10-14

| Rule | What the tree does | Measure |
| --- | --- | --- |
| Mirror: `tests/<tier>/<module>/test_<name>.cpp` | Every file is in `tests/functional/unit/` or `tests/functional/regression/` | 155 test files; none sits at its mirror path |
| Names: a unit's test is named for the unit | 125 of the 155 files match a header by name; 1 matches a `.cpp` only; 29 match nothing | Table of the 29 in PLAN.md T7 |
| Units: each header has its own test file | 224 header units: 160 in `modules/`, 64 in `applications/` | 99 have no same-named test: 62 module, 37 application |
| Kind is a label, not a name prefix | Names are `Unit_<Group>` or `Regression_<Group>`; labels come from those prefixes | 1290 `Unit_` and 23 `Regression_` of 1313 tests |
| One test program per module | One binary, `sushidsp_tests`, from `tests/CMakeLists.txt` | 1 program for 160 module headers |
| Application code is not compiled into a test | 39 application sources are compiled into `sushidsp_tests` | `applications/host`, `host_gui`, `plugin` |
| Shared test code is an internal module | `tests/measure/`, `tests/circuits/`, `tests/golden/`, `tests/common/` hold it, outside any module | Used by 2 to 12 test files each |
| No kind folder under `tests/` | `functional/`, `unit/`, `regression/` | The checker's `rule_mirror_folders`: 8 findings |
| Checker `check_test_mirror.py` | Present now, copied from SushiSkills | `check_all.py` runs it; it reports 387 findings on this tree |
| `tests/` READMEs within the ceiling | `tests/functional/unit/README.md` | 4401 lines against 800 |
| Include path form | Project includes are quoted, flat: `"SushiDSP/<module>/<name>.hpp"`. The rule asks for angle brackets and the tier form. | 1,235 include lines in 425 files, counted over the whole repository including archived plans under `docs/archive/`. The exact count of source and test includes is taken in T0. |

What already conforms: file names are `test_<snake_case>.cpp`; the golden comparisons keep a
README beside them; the timing harness is one `benchmark` case.

Not audited in this pass: the cases inside each test file, and the contents of `golden/`,
`fixtures/`, `measure/`, `circuits/` and `common/` beyond their listing.

## 4. Target

Taken from `TESTS.md` and `SKILL.md`, as they read on 2026-10-15:

- `tests/<tier>/<module>/test_<name>.cpp` for each module; `tests/applications/<app>/...` for each
  application with tests. A unit whose tests need several files uses the folder
  `tests/<tier>/<module>/test_<name>/` instead of the file, never both.
- A test name reads as behaviour and starts with the unit it tests. No `Unit_` or `Regression_`.
- Labels `unit`, `regression`, `golden`, `benchmark`, `device` set per test in the build.
- Shared test code becomes internal modules, each named for its responsibility, in the lowest
  tier its dependencies allow. The names `utils`, `common`, `helpers`, `shared`, `misc`,
  `support`, `testing` and `test_support` are forbidden. The five modules in PLAN.md T5 are the
  owner's confirmed split.
- Include paths take the tier form, `<Project>/<tier>/<module>/<name>.hpp`, with angle brackets,
  as `repository-layout` (Names) and `cpp-code-style` (Includes) state. This is task T0 and runs
  before the test moves, so the moved files are written in their final form.
  - The headers move with the form, so each include line equals the header's path below `include/`
    (2026-10-15). PLAN.md T0 lists the steps.

## 5. Tools drift, recorded

Only the mirror checker and its test were copied from SushiSkills, on the peer's correction. Keeping
the repository's own `K_` settings. Still different from the reference:

- `check_docs_truth.py`: the reference skips git-ignored and rule folders. Its own findings on this
  repository are pre-existing, in other documents, and are not caused by this work.
- `check_layering.py`: the reference checks layering by module owner (`owner_of`) and reads
  `K_ANGLE_INCLUDE` differently. `K_TIER_ORDER` stays the repository's own. T0 changes the include
  form the checker reads, so this difference is handled in T0.
- `check_docs_layout.py`, `check_changelog.py`: header only.
- `sd check` runs three checkers; `check_all.py` runs six. The CLI is not changed here.

## 6. Not in scope

- Turning the host into a library. That is its own work; this plan only removes the test's
  dependence on it once the owner approves a route.
- Changing `sd test -s` option names. The CLI keeps `unit`, `regression`, `benchmark` and `all`.
- Any behaviour change in the code under test. Every existing test stays green, with the same
  assertions.
- Editing the archived plans under `docs/archive/`, which keep the old include form as written.

## 7. Build rule

One Release build with four threads, at the end of the plan, after the owner's permission. The
session does not run `sd build` or `sd test` before that.
