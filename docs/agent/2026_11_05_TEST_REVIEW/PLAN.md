# Test review: plan

**Status:** Open — in progress, run in parallel; T1 to T7, M2, M4, M5, M6 and M7 are done, the other M jobs follow in table order

Applies the verdicts of `REPORT.md`, in hierarchy order: the shared helpers, adapters and seams the
rewrites need come first, then the defects the review found, then each module's tests. A module job
applies every record of its module: rewrite, resize, split, merge and delete as the record says, and
writes each add. Nothing in a module job changes production code except a seam a job above built.

## Jobs

| Job | Files | Waits on | Status |
| --- | --- | --- | --- |
| T1 | `tests/support/tolerance.hpp` copied from SushiSkills |  | done |
| T2 | `compare_or_regenerate`: a regenerate run reports no pass |  | done |
| T3 | In-memory nodes beside `AudioNode`, an in-memory `AudioDevice` and `CabinetHost`, and the DK glue ports |  | done |
| T4 | The registry and counter seams of decisions 1 and 2, and `ClapRigPlugin` taking its rig in place of `SUSHIDSP_PLUGIN_RIG` |  | done |
| T5 | The CLI tests on sushicore's in-memory runner and console | sushicore T1 | done |
| T6 | The defects of this report, each with a test that fails first |  | done |
| T7 | `tools/testing/check_test_additions.py`, `tools/tests/test_check_test_additions.py` deleted |  | done |
| M1 | the tests of `applications` (34 files): 102 rewrite, 72 resize, 50 split, 13 merge, 10 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M2 | the tests of `catalog` (1 file): 3 rewrite, 2 split, 1 merge | T1, T2, T3, T4, T5, T6, T7 | done |
| M3 | the tests of `cli` (11 files): 82 rewrite, 15 resize, 2 split, 2 merge, 7 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M4 | the tests of `foundation/allocation_trap` (1 file): 1 rewrite, 1 merge | T1, T2, T3, T4, T5, T6, T7 | todo |
| M5 | the tests of `foundation/audio_format` (2 files): 5 rewrite, 1 split, 2 merge, 1 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M6 | the tests of `foundation/benchmark_harness` (6 files): 11 rewrite, 1 resize, 3 split | T1, T2, T3, T4, T5, T6, T7 | done |
| M7 | the tests of `foundation/circuit_blocks` (2 files): 6 rewrite, 2 merge, 2 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M8 | the tests of `foundation/circuits` (26 files): 81 rewrite, 2 resize, 5 split, 3 merge, 11 delete | T1, T2, T3, T4, T5, T6, T7 | partial (lane A, 2026-11-10): every file is done and syntax-checked except `stages/test_triode_stage/test_dk_netlist.cpp` (4 records; the bias point, convergence and the two LTspice nulls need derived bands that are not yet written; resume there). Done since the last note: both pentode files, `test_power_tube_presets.cpp` (keep), both `test_triode_stage` files, the six loose files, and `tests/support/oversampled_stage_node.hpp` (REPORT decisions 102 to 104) |
| M9 | the tests of `foundation/core` (23 files): 68 rewrite, 14 resize, 7 split, 5 merge, 25 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M10 | the tests of `foundation/device` (5 files): 10 rewrite, 3 resize, 2 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M11 | the tests of `foundation/golden_compare` (2 files): 2 rewrite, 1 resize, 1 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M12 | the tests of `foundation/io` (5 files): 13 rewrite, 5 resize, 1 merge, 4 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M13 | the tests of `foundation/log` (2 files): 1 rewrite, 1 resize, 1 split, 2 delete | T1, T2, T3, T4, T5, T6, T7 | todo |
| M14 | the tests of `foundation/log_capture` (2 files): 3 resize | T1, T2, T3, T4, T5, T6, T7 | todo |
| M15 | the tests of `foundation/math` (23 files): 92 rewrite, 2 split, 14 delete | T1, T2, T3, T4, T5, T6, T7 | partial (lane B, syntax-checked only): all of `tests/foundation/math/dk/` done (`test_dense_lu`, `test_damped_newton_solver`, `test_dk_circuit/*`, `test_dk_netlist` and `test_linear_port` replaced by `compile_dk_netlist` and `compile_linear_port`); resume at `tests/foundation/math/test_biquad.cpp`, then `test_fft`, `test_halfband_coefficients`, `test_newton_raphson_solver`, `test_oversampler`, `test_partitioned_convolution`, `test_resampler`, `test_resampler_design`, `test_sinc_resampler`, `test_smoothed_parameter`, `test_streaming_convolution`, `test_wdf`; the benchmarks are keeps; M16 to M19, M26, M29, M32 and M33 not started |
| M16 | the tests of `foundation/profile` (7 files): 6 rewrite, 6 resize, 6 split, 2 merge | T1, T2, T3, T4, T5, T6, T7 | todo |
| M17 | the tests of `foundation/regression_test` (1 file): 1 rewrite | T1, T2, T3, T4, T5, T6, T7 | todo |
| M18 | the tests of `foundation/signal_measure` (7 files): 18 rewrite, 1 split, 4 merge | T1, T2, T3, T4, T5, T6, T7 | todo |
| M19 | the tests of `foundation/spice_reference` (2 files): 3 rewrite, 2 resize | T1, T2, T3, T4, T5, T6, T7 | todo |
| M20 | the tests of `nodes/cabinet` (2 files): 5 rewrite, 2 resize, 3 split, 3 delete | T1, T2, T3, T4, T5, T6, T7 | done; the two log-port resizes wait on the owner's constructor shape (decision 300) |
| M21 | the tests of `nodes/capture` (13 files): 46 rewrite, 2 resize, 9 split, 9 merge, 4 delete | T1, T2, T3, T4, T5, T6, T7 | done; the three port resizes wait on the owner (decision 301) |
| M22 || partial (lane C, 2026-11-10, syntax-checked only): `test_dd3_analog_filters.cpp` and `test_dd3_clock.cpp` done; resume at `test_dd3_converter.cpp`; M23, M24, M25 and M27 follow in that order |
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

## Resume note

M2, M4, M5, M6 and M7 are applied and M8 is partly applied, all syntax-checked (`clang -fsyntax-only`
through `build/compile_commands.json`, with `-I tests`), not built or run. M6 used the owner's seams
of 2026-11-10 (REPORT decision 10). M8 so far covers `models/test_soft_transition.cpp` and
`models/test_silicon_diode.cpp`; resume at `models/test_pentode_el34.cpp`, then `test_triode12_ax7.cpp`,
`stages/` and the loose files, in the order of the REPORT section for `tests/foundation/circuits`.
A rewritten test that is renamed or split also renames its heading in `tests/foundation/circuits/models/README.md`
and keeps the quoted source comments that README and `modules/foundation/circuits/README.md` cite
(`v_max` for one). The new files have no entry in `build/compile_commands.json` until the next
configure, so a syntax check borrows a neighbour's entry. M9 must account for
`tests/foundation/core/test_rig_graph/test_channels_adapt.cpp`.
