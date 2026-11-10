# Test review: plan

**Status:** Open — every job is applied once and the whole tree builds and passes (2026-11-10, 84 s wall); M1, M21 and M28 are `failed`, because records listed under "Not done as written" are still not applied as written

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
| M1 | the tests of `applications` (34 files): 102 rewrite, 72 resize, 50 split, 13 merge, 10 delete | T1, T2, T3, T4, T5, T6, T7 | failed |
| M2 | the tests of `catalog` (1 file): 3 rewrite, 2 split, 1 merge | T1, T2, T3, T4, T5, T6, T7 | done |
| M3 | the tests of `cli` (11 files): 82 rewrite, 15 resize, 2 split, 2 merge, 7 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M4 | the tests of `foundation/allocation_trap` (1 file): 1 rewrite, 1 merge | T1, T2, T3, T4, T5, T6, T7 | done |
| M5 | the tests of `foundation/audio_format` (2 files): 5 rewrite, 1 split, 2 merge, 1 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M6 | the tests of `foundation/benchmark_harness` (6 files): 11 rewrite, 1 resize, 3 split | T1, T2, T3, T4, T5, T6, T7 | done |
| M7 | the tests of `foundation/circuit_blocks` (2 files): 6 rewrite, 2 merge, 2 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M8 | the tests of `foundation/circuits` (26 files): 81 rewrite, 2 resize, 5 split, 3 merge, 11 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M9 | the tests of `foundation/core` (23 files): 68 rewrite, 14 resize, 7 split, 5 merge, 25 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M10 | the tests of `foundation/device` (5 files): 10 rewrite, 3 resize, 2 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M11 | the tests of `foundation/golden_compare` (2 files): 2 rewrite, 1 resize, 1 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M12 | the tests of `foundation/io` (5 files): 13 rewrite, 5 resize, 1 merge, 4 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M13 | the tests of `foundation/log` (2 files): 1 rewrite, 1 resize, 1 split, 2 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M14 | the tests of `foundation/log_capture` (2 files): 3 resize | T1, T2, T3, T4, T5, T6, T7 | done |
| M15 | the tests of `foundation/math` (23 files): 92 rewrite, 2 split, 14 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M16 | the tests of `foundation/profile` (7 files): 6 rewrite, 6 resize, 6 split, 2 merge | T1, T2, T3, T4, T5, T6, T7 | done |
| M17 | the tests of `foundation/regression_test` (1 file): 1 rewrite | T1, T2, T3, T4, T5, T6, T7 | done |
| M18 | the tests of `foundation/signal_measure` (7 files): 18 rewrite, 1 split, 4 merge | T1, T2, T3, T4, T5, T6, T7 | done |
| M19 | the tests of `foundation/spice_reference` (2 files): 3 rewrite, 2 resize | T1, T2, T3, T4, T5, T6, T7 | done |
| M20 | the tests of `nodes/cabinet` (2 files): 5 rewrite, 2 resize, 3 split, 3 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M21 | the tests of `nodes/capture` (13 files): 46 rewrite, 2 resize, 9 split, 9 merge, 4 delete | T1, T2, T3, T4, T5, T6, T7 | failed |
| M22 | the tests of `nodes/dd3` (9 files): 51 rewrite, 2 split, 2 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M23 | the tests of `nodes/diezel_vh4` (5 files): 19 rewrite, 1 merge, 2 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M24 | the tests of `nodes/jcm800` (5 files): 27 rewrite, 1 merge, 1 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M25 | the tests of `nodes/mesa_mark2c` (5 files): 22 rewrite, 1 merge, 2 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M26 | the tests of `nodes/microphone` (1 file): 3 rewrite, 1 split | T1, T2, T3, T4, T5, T6, T7 | done |
| M27 | the tests of `nodes/peavey5150` (5 files): 20 rewrite, 1 merge, 2 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M28 | the tests of `nodes/room` (52 files): 240 rewrite, 14 resize, 15 split, 8 merge, 19 delete | T1, T2, T3, T4, T5, T6, T7 | failed |
| M29 | the tests of `nodes/room_scenes` (2 files): 3 rewrite, 1 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M30 | the tests of `nodes/routing` (7 files): 34 rewrite, 3 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M31 | the tests of `nodes/tubescreamer` (7 files): 34 rewrite, 2 resize, 4 split, 1 merge, 7 delete | T1, T2, T3, T4, T5, T6, T7 | done |
| M32 | the tests of `session` (1 file): 7 rewrite, 1 merge | T1, T2, T3, T4, T5, T6, T7 | done |
| M33 | the tests of `tools` (3 files): 1 rewrite, 3 resize, 1 split, 5 delete | T1, T2, T3, T4, T5, T6, T7 | done |

## Not done as written

Each item is a record or a ruling that was not applied as it is written. The job stays `failed`
until it is. Owner marks an item whose shape the owner reserves. On 2026-11-10 the owner approved
the owner-reserved items as ports with in-memory adapters beside the production code.

- **M1.** Decision 708 for `set_cabinet`: `SlotGraph::set_cabinet` still calls `IrLoader::load`
  itself, so `test_rig_controller/test_cabinet.cpp` stays `Medium_` (RW-95). The room HRTF file
  seam for the `Medium_RigControllerRoomHrtf` cases (RW-96). Decision 1208 for every group but
  `SincResampler`: `Resampler` and `ResamplerRoundTrip` need their bounds re-derived,
  `FrequencyResponse` and `AliasFloor` need a sweep-length parameter on `frequency_response()`,
  a public interface (owner), and the nodes and circuits groups were not shrunk (RW-97).
- **M21.** Decision 301: the writer port exists (`AudioFileWriterOpener`), but `TrackRecorder` takes
  no log sink port; its logging case still installs `LogCapture` on the process-wide logger
  (RW-98).
- **M28.** Decision 501: the binaural decoder reports through no allocation counter. Its convolvers
  are `PartitionedConvolution` in `foundation/math`, and a counter fed by an injected allocator
  cannot see a plain `new` in `process()`; the counter's shape is the owner's (RW-99).

Applied on 2026-11-10: M13 (decision 110, `EnvironmentReader` on the SushiDSP side;
`apply_log_level_setting` has no production caller yet), M20 (decision 300, `CabinetLog`), M31
(decision 801, an independent BDF2 solve; the floors gain a derived 0.035), M1 decisions 708 and
712 for tracks, clips, export, preset and project files, and M28 decisions 605, 610, 611, 614 and
616.

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
