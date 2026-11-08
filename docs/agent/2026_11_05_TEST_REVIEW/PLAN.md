# Test review: plan

**Status:** Open — not started; the owner chooses inline or parallel

Applies the verdicts of `REPORT.md`, in hierarchy order: the shared helpers, adapters and seams the
rewrites need come first, then the defects the review found, then each module's tests. A module job
applies every record of its module: rewrite, resize, split, merge and delete as the record says, and
writes each add. Nothing in a module job changes production code except a seam a job above built.

## Jobs

| Job | Files | Waits on | Status |
| --- | --- | --- | --- |
| T1 | `tests/support/tolerance.hpp` copied from SushiSkills |  | done |
| T2 | `compare_or_regenerate`: a regenerate run reports no pass |  | todo |
| T3 | In-memory nodes beside `AudioNode`, an in-memory `AudioDevice` and `CabinetHost`, and the DK glue ports |  | todo |
| T4 | The registry and counter seams of decisions 1 and 2, and `ClapRigPlugin` taking its rig in place of `SUSHIDSP_PLUGIN_RIG` |  | todo |
| T5 | The CLI tests on sushicore's in-memory runner and console | sushicore T1 | done |
| T6 | The defects of this report, each with a test that fails first |  | todo |
| T7 | `tools/testing/check_test_additions.py`, `tools/tests/test_check_test_additions.py` deleted |  | done |
| M1 | the tests of `applications` (34 files): 102 rewrite, 72 resize, 50 split, 13 merge, 10 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M2 | the tests of `catalog` (1 file): 3 rewrite, 2 split, 1 merge | T1, T2, T3, T4, T5, T6, T7 | todo |
| M3 | the tests of `cli` (11 files): 82 rewrite, 15 resize, 2 split, 2 merge, 7 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M4 | the tests of `foundation/allocation_trap` (1 file): 1 rewrite, 1 merge | T1, T2, T3, T4, T5, T6, T7 | todo |
| M5 | the tests of `foundation/audio_format` (2 files): 5 rewrite, 1 split, 2 merge, 1 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M6 | the tests of `foundation/benchmark_harness` (6 files): 11 rewrite, 1 resize, 3 split | T1, T2, T3, T4, T5, T6, T7 | todo |
| M7 | the tests of `foundation/circuit_blocks` (2 files): 6 rewrite, 2 merge, 2 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M8 | the tests of `foundation/circuits` (26 files): 81 rewrite, 2 resize, 5 split, 3 merge, 11 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M9 | the tests of `foundation/core` (23 files): 68 rewrite, 14 resize, 7 split, 5 merge, 25 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M10 | the tests of `foundation/device` (5 files): 10 rewrite, 3 resize, 2 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M11 | the tests of `foundation/golden_compare` (2 files): 2 rewrite, 1 resize, 1 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M12 | the tests of `foundation/io` (5 files): 13 rewrite, 5 resize, 1 merge, 4 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M13 | the tests of `foundation/log` (2 files): 1 rewrite, 1 resize, 1 split, 2 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M14 | the tests of `foundation/log_capture` (2 files): 3 resize | T1, T2, T3, T4, T5, T6, T7 | todo |
| M15 | the tests of `foundation/math` (23 files): 92 rewrite, 2 split, 14 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M16 | the tests of `foundation/profile` (7 files): 6 rewrite, 6 resize, 6 split, 2 merge | T1, T2, T3, T4, T5, T6, T7 | todo |
| M17 | the tests of `foundation/regression_test` (1 file): 1 rewrite | T1, T2, T3, T4, T5, T6, T7 | todo |
| M18 | the tests of `foundation/signal_measure` (7 files): 18 rewrite, 1 split, 4 merge | T1, T2, T3, T4, T5, T6, T7 | todo |
| M19 | the tests of `foundation/spice_reference` (2 files): 3 rewrite, 2 resize | T1, T2, T3, T4, T5, T6, T7 | todo |
| M20 | the tests of `nodes/cabinet` (2 files): 5 rewrite, 2 resize, 3 split, 3 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M21 | the tests of `nodes/capture` (13 files): 46 rewrite, 2 resize, 9 split, 9 merge, 4 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M22 | the tests of `nodes/dd3` (9 files): 51 rewrite, 2 split, 2 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M23 | the tests of `nodes/diezel_vh4` (5 files): 19 rewrite, 1 merge, 2 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M24 | the tests of `nodes/jcm800` (5 files): 27 rewrite, 1 merge, 1 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M25 | the tests of `nodes/mesa_mark2c` (5 files): 22 rewrite, 1 merge, 2 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M26 | the tests of `nodes/microphone` (1 file): 3 rewrite, 1 split | T1, T2, T3, T4, T5, T6, T7 | todo |
| M27 | the tests of `nodes/peavey5150` (5 files): 20 rewrite, 1 merge, 2 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M28 | the tests of `nodes/room` (52 files): 240 rewrite, 14 resize, 15 split, 8 merge, 19 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M29 | the tests of `nodes/room_scenes` (2 files): 3 rewrite, 1 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M30 | the tests of `nodes/routing` (7 files): 34 rewrite, 3 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M31 | the tests of `nodes/tubescreamer` (7 files): 34 rewrite, 2 resize, 4 split, 1 merge, 7 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M32 | the tests of `session` (1 file): 7 rewrite, 1 merge | T1, T2, T3, T4, T5, T6, T7 | todo |
| M33 | the tests of `tools` (3 files): 1 rewrite, 3 resize, 1 split, 5 delete | T1, T2, T3, T4, T5, T6, T7 | todo |

## Acceptance

- **T1.** The helper is in place and its self-check passes. Build: the repository's test command.
- **T2.** A golden test under `SUSHIDSP_REGENERATE_GOLDEN` fails or is skipped, never passes. Build: the golden tests.
- **T3.** Each keeps its port's promise; the rig, slot and DK tests use them. Build: the repository's test command.
- **T4.** No small test reads a process-wide counter, registry or the environment. Build: the repository's test command.
- **T5.** No CLI test patches the repository's own functions. Build: the CLI tests.
- **T6.** Every defect above is fixed or has a backlog entry. Build: the tests of each touched module.
- **T7.** Neither file exists and the checkers pass. Build: `python tools/check_all.py .`
- **M jobs.** Every record of the module is applied: no test the report deletes remains, every kept and
  new test passes at the size its label states, and the module's tests pass through the project's own
  test command. Build: that command over the module.

The owner chooses inline or parallel mode.
