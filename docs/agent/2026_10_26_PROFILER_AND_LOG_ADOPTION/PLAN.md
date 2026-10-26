# SushiProfiler adoption and the logging gaps: plan

**Status:** Open — planned 2026-10-26; every task is written and none is built; the first build, the ASIO sites and the owner's own runs close it.
**Implements:** SUSHIDSP#DP

**Goal:** Carry out what [`SPEC.md`](SPEC.md) finds. Section and question numbers below are that
file's: `§3` is its section 3, `Q6` its question 6, `A9`, `B16`, `SLG-2`, `SPG-1` its rows.

## How to read this plan

- Tasks are in hierarchy order: build edges first, then the foundation modules, then the nodes,
  then the applications. A module boundary is fixed and its edge committed before code on either
  side of it is written.
- Each task has one acceptance criterion and one build. The build is one `sd` invocation after the
  edit; `-l` is `--label`, `-f` is `--filter`.
- **Who** is `worker` or `orchestrator`. A step that edits a build file (`CMakeLists.txt`,
  `cmake/*.cmake`, `cli/sushistack.deps.toml`, `tests/CMakeLists.txt`) or a checker setting
  (`tools/settings.py`, `tools/testing/untested_units.txt`) is `orchestrator`: the implementer does
  not touch it. A task that adds a test file names the line the orchestrator adds to
  `tests/CMakeLists.txt`.
- A worker loads `sushiskills`, `cpp-code-style`, `source-comments`, `logging`, `testing`,
  `humanizer`, plus `performance` for a task that changes a hot path. A worker writes the files its
  task lists and a module README only when the task lists it. Manual pages, `docs/design/`, the
  changelog and the backlog are the orchestrator's (task D1).
- Every test name opens with its size (`Small_`, `Medium_`, `Large_`). A test that holds a
  `Session` starts a consumer thread, so it is `Medium_`. A logging test uses `LogCapture`, which
  holds one capture at a time (SLG-6).
- Nothing here picks an option of a question. A task that depends on one says `Waits on Qn` and
  names the option it needs; the dispatch is written after the owner answers.
- Verification is what `sd` prints. Nothing in this plan has been built or run.

## The gaps in the other two repositories

Each runs in SushiLog or SushiProfiler, which the owner builds. Q19 says whether a task waits or
takes the workaround.

| Gap | Workaround inside SushiDSP |
| --- | --- |
| SLG-1, SLG-2 | Counters owned by SushiDSP (Q6b) and a control-thread reporter, tasks C1 to C7 |
| SLG-3 | Log the first failure of a streak and re-arm on success, with one flag at the call site (B14, C7, directory listing) |
| SLG-5 | Keep the two-catch blocks; the plug-in task adds three more of them |
| SLG-6 | `LogCapture` stays one capture at a time |
| SPG-1 | Warm-up periods with the gate open before the active window, in the harness and in tests. A live host starts its session while the device is stopped |
| SPG-2 | Read load from the zone `dsp.process` through `zone:` checks; ignore the `budget` analysis on the arrival timeline |
| SPG-3 | Q5a: no zone inside the oversampled loop |
| SPG-4 | One counter per node type; instance names on zones only, as interned ids made at `compile()` |
| SPG-5 | No live meter (Q16) |
| SPG-6 | Only one plug-in instance profiles; stated in the plug-in README |
| SPG-7 | A dropout counter checked with `counter:` `max`; the baseline comparison stays `sp record compare` outside tests; the optimised-build gate stays in test code and reads the record's build facts |

## Stage 0: build edges (orchestrator)

**E1. Pin SushiProfiler and resolve it.** Waits on Q1, Q2.
- Files: `cli/sushistack.deps.toml` (`depends_on`, `[sushisystems.sushiprofiler]` with `version = "v0.1.0"`
  and its commit), root `CMakeLists.txt` (resolve `SUSHIPROFILER_DIR`, add it excluded from all, add
  the `SD_ENABLE_PROFILER` option as Q2 answers). The pin follows the shape of `[sushisystems.sushilog]`
  (`cli/sushistack.deps.toml:27-29`); the orchestrator confirms that sushicore hands
  `-DSUSHIPROFILER_DIR` as it hands `-DSUSHILOG_DIR` (`cli/tests/test_project.py:49-56`).
- Accepts when: `sd build` configures with the option on and with it off (`-D SD_ENABLE_PROFILER=OFF`),
  and with it on the SushiProfiler targets exist.
- Build: `sd build -t release`, twice (one configure per setting counts as the one build of the pin).
- Tests: none; the layering and docs checkers run through `sd check`.

**E2. The `profile` module's edges.** Waits on Q3.
- Files: `modules/foundation/profile/CMakeLists.txt`, `modules/CMakeLists` entry in the root
  `CMakeLists.txt`, `cmake/SushiDSPLayers.cmake`, `tools/settings.py` (`K_SAME_TIER_EDGES`).
- Accepts when: an empty target `sushi_dsp_profile` links SushiProfiler when the option is on and
  links nothing when it is off, and `tools/layering/check_layering.py` passes.
- Build: `sd build`.

**E3. Log edges for the modules that gain their first log call.** No question blocks it.
- Files: the `CMakeLists.txt` of `io`, `device`, `circuits`, `tubescreamer`, `core`, and the plug-in
  target; the same-tier edges in `cmake/SushiDSPLayers.cmake` and `tools/settings.py`
  (`io`, `device`, `circuits`, `core` to `log`; today only `golden_compare` and `log_capture` have
  one).
- Accepts when: each of those targets links `sushi_dsp_log` and the layering checker passes.
- Build: `sd build`.

## Stage 1: logging that needs nothing new from SushiLog

Control thread only. These tasks can start before any question is answered, except where marked.
Each adds its module's category in `<module>_log_category.hpp` and a mirror test
`test_<module>_log_category.cpp`, as `test_capture_log_category.cpp` does.

**L1. `io`: impulse response transformations (B26, B27).**
- Files: `modules/foundation/io/include/SushiDSP/foundation/io/io_log_category.hpp` (new),
  `modules/foundation/io/source/ir_loader.cpp`, `modules/foundation/io/README.md`.
- Accepts when: loading an impulse response at a different rate writes one `INFO io` naming both
  rates, one longer than `max_length_ms` writes one `WARN io` naming both lengths, and a matching
  rate and length writes nothing.
- Build: `sd test -l small -f "IrLoader|IoLogCategory"`.
- Tests: three cases in `tests/foundation/io/test_ir_loader.cpp`; new
  `tests/foundation/io/test_io_log_category.cpp`. Orchestrator adds the second to `tests/CMakeLists.txt`.
- Waits on: E3.

**L2. `core`: the by-id parameter write (C8).**
- Files: `modules/foundation/core/include/SushiDSP/foundation/core/core_log_category.hpp` (new),
  `parameter_table.hpp`, `core/README.md`.
- Accepts when: `write(node, id, value)` with an id no row carries writes one `WARN core` naming the
  id, the by-index `write` stays silent and allocation free, and a known id writes nothing.
- Build: `sd test -l small -f "ParameterTable|CoreLogCategory"`.
- Tests: two cases in `tests/foundation/core/test_parameter_table.cpp`; new
  `test_core_log_category.cpp` (orchestrator adds the line).
- Waits on: E3.

**L3. `device`: construction fallbacks (B20 to B23).**
- Files: `modules/foundation/device/include/SushiDSP/foundation/device/device_log_category.hpp`
  (new), `asio_audio_device.cpp`, `sdl2_audio_device.cpp`, `device/README.md`.
- Accepts when: the five sites write the records `§3` gives (`WARN device`, both values), and the
  category test passes.
- Build: `sd test -l small -f DeviceLogCategory`.
- Tests: the category test (new; orchestrator adds it). The ASIO branches need a driver and the
  Steinberg SDK, which is not in the tree (`modules/foundation/device/CMakeLists.txt:31`), so they
  carry a `Large_` case that skips without a driver and are unverified here. The report says so.
- Waits on: E3, and Q11 for B23.

**L4. `circuits` and `tubescreamer`: a circuit that fails to prepare (B17 to B19).**
- Files: `circuits_log_category.hpp`, `tubescreamer_log_category.hpp` (new), `fmv_tone_stack.cpp`,
  `power_amp_feedback.cpp`, `tube_screamer.cpp`, the two READMEs.
- Accepts when: a failed `prepare` of each of the three writes one `ERROR` naming the circuit and
  what the node does next, in release as in debug.
- Build: `sd test -l small -f "FmvToneStack|PowerAmpFeedback|TubeScreamer|CircuitsLogCategory|TubescreamerLogCategory"`.
- Tests: one case per class in the existing test files, driven by a component set that makes the
  matrix singular; if no such set exists the worker reports it rather than adding a seam. Two new
  category tests (orchestrator adds the lines).
- Waits on: E3, Q13 (the behaviour after the failure).

**L5. `room`: publish failures and limits (B7 to B10, D1, D2).**
- Files: `room_node.cpp`, `image_source_solver.cpp`, `modal_solver.cpp`, `room/source/README.md`.
- Accepts when: B9 writes `WARN room` with both rates; D1 and D2 write one `WARN room` per publish
  with the count asked and kept, and nothing when the limit is not met.
- Build: `sd test -l small -f "RoomNode|ImageSourceSolver|ModalSolver"`.
- Tests: cases in `tests/nodes/room/test_room_node/test_behaviour.cpp`, `test_image_source_solver.cpp`,
  `test_modal_solver.cpp`. B7, B8 and B10 need a `publish` that throws, and `RoomNode` holds its
  builder as a member (`room_node.cpp:524-550`), so there is no seam: the worker logs them and
  reports them untested, and the seam is a boundary question for the owner. D4 (the fit cap) waits
  on P2.
- Waits on: nothing for the testable rows.

**L6. `host_gui`: a take at another sample rate (B28).**
- Files: `applications/host_gui/session_timeline.cpp`.
- Accepts when: placing a take whose rate differs from the session's writes one `WARN host_gui`
  naming both rates and the 8.8 % example of KI-25, and an equal rate writes nothing.
- Build: `sd test -l small -f "Session"`.
- Tests: two cases in `tests/applications/host_gui/test_session.cpp`.
- Waits on: nothing. It records KI-25; the fix is RW-35 and stays out.

**L7. The plug-in (B1 to B6).**
- Files: `applications/plugin/plugin_log_category.hpp` (new), `clap_boundary.hpp` (`guarded` takes
  the callback's name and writes `ERROR plugin`), `clap_plugin.cpp`, `startup_rig.cpp`,
  `rig_adapter.cpp`, `applications/plugin/README.md`.
- Accepts when: a control callback whose body throws returns its fallback and writes one `ERROR`
  naming the callback; a refused startup rig, an unreadable startup file, a failed `activate` and a
  failed state load each write one record with the reason.
- Build: `sd test -l small -f "Plugin|ClapBoundary|StartupRig|RigAdapter"`.
- Tests: cases in `tests/applications/plugin/test_rig_adapter.cpp`; new `test_clap_boundary.cpp`,
  `test_startup_rig.cpp`, `test_plugin_log_category.cpp` (orchestrator adds three lines). None of
  `clap_boundary`, `clap_plugin`, `startup_rig` has a test today, and the orchestrator removes their
  lines from `tools/testing/untested_units.txt` (`:35-40`).
- Waits on: E3. The destination of the records when the host never joins is Q15 and is its own
  task after the answer.

**L8. `host_gui`: slot graph (B12 to B15, C7).**
- Files: `applications/host_gui/slot_graph.cpp`, `slot_graph.hpp` for one flag per streak.
- Accepts when: each of the six sites writes one `WARN host_gui` naming the slot and the cause, B14
  writes at most one record per failing streak, and a succeeding publish re-arms it.
- Build: `sd test -l small -f RigController`.
- Tests: six cases in `tests/applications/host_gui/test_rig_controller.cpp`.
- Waits on: Q12 for B15 and B16; the other rows wait on nothing.

**L9. `host_gui`: defaults taken from a file (E1 to E4, D6).**
- Files: `applications/host_gui/rig_persistence.cpp`.
- Accepts when: loading a session that skips a row, drops an empty-path clip, takes a default or
  clamps `track_count` writes one `WARN host_gui` for the whole load with the counts, and a complete
  file writes none; a file with no `format` writes one `INFO`.
- Build: `sd test -l small -f Session`.
- Tests: cases in `tests/applications/host_gui/test_session.cpp`.
- Waits on: nothing.

**L10. `host_gui`: statuses created at a boundary and startup fallbacks (B24, B25, C1 to C6).**
- Files: `main.cpp`, `session_timeline.cpp`, `session_bounce.cpp`, `session_file.cpp`,
  `audio_import_queue.cpp`, `modules/nodes/room/source/hrtf_set_loader.cpp`.
- Accepts when: each creation site writes one record at its level (`ERROR` for a failed operation,
  `WARN` for a fallback) with the values `§3` names, without a signature change.
- Build: `sd test -l small -f "Session|AudioImportQueue|HrtfSetLoader"`.
- Tests: cases in `test_session.cpp`, `test_audio_import_queue.cpp`, `test_hrtf_set_loader.cpp`.
  `main()` and the widgets in `ui/` hold no testable unit: B24 and B25 are one call each and are
  reported untested.
- Waits on: Q20(a).

**L11. The reason of a refused cabinet or HRTF set (B15, B16).**
- Files: `modules/foundation/core/include/SushiDSP/foundation/core/cabinet_host.hpp`,
  `modules/nodes/cabinet/source/cabinet_node.cpp`, `applications/host_gui/slot_graph.cpp`,
  `ui/stage_panel.cpp`, `applications/host_asio/main.cpp`.
- Accepts when: each of the three causes of `set_cabinet` gives its own message, and the stage panel
  shows the message the node gave, not a guess.
- Build: `sd test -l small -f "CabinetNode|CabinetHost|RigController"`.
- Tests: three cases in `tests/nodes/cabinet/test_cabinet_node.cpp`, one in `test_cabinet_host.cpp`.
- Waits on: Q12.

## Stage 2: real-time failure counters

Audio thread. The home of the counters is Q6; the tasks are written for option (b), the one that
needs nothing from SushiLog, and are re-planned if the owner picks (a) or (c).

**C1. The counter type and its reporter.**
- Files: `modules/foundation/core/include/SushiDSP/foundation/core/rt_counter.hpp`,
  `rt_counter_report.hpp`, `core/README.md`.
- Accepts when: `increment()` from one thread while another reads is race free under
  `--sanitize thread`, performs no heap allocation under `AllocationTrap`, and the reporter writes
  one `WARN` per interval naming each counter that moved and by how much, and nothing when none did.
- Build: `sd test -l medium -f RtCounter`.
- Tests: new `tests/foundation/core/test_rt_counter.cpp` (orchestrator adds the line): increment,
  read, interval report, no report without movement, trap.
- Waits on: Q6, E3.

**C2. Device counters (A1 to A5).**
- Files: `sdl2_audio_device.cpp`, `asio_audio_device.cpp`, the two device headers for the accessor.
- Accepts when: starting an SDL2 device with no callback raises the `no_callback` counter once per
  block, and the other four sites raise their counters at the places `§3` names.
- Build: `sd test -l medium -f Sdl2AudioDevice` with `SDL_AUDIODRIVER=dummy`.
- Tests: new `tests/foundation/device/test_sdl2_audio_device.cpp` (orchestrator adds the line and
  removes `device/*` from `tools/testing/untested_units.txt:41-42`). A2 to A5 have no seam; the
  report lists them untested.
- Waits on: C1, Q10, Q11.

**C3. `RigGraph` (A8, A9).**
- Files: `modules/foundation/core/source/rig_graph.cpp`, `rig_graph.hpp`.
- Accepts when: a parameter change for a missing node raises its counter, a block longer than the
  built size is handled as Q14 says and counted, and `ProcessPerformsNoHeapAllocationAcrossManyBlocks`
  still passes.
- Build: `sd test -l small -f RigGraph`.
- Tests: cases in `tests/foundation/core/test_rig_graph/test_behaviour.cpp` and `test_rt_safety.cpp`.
- Waits on: C1, Q14.

**C4. The session path (A6, A7, A14 to A17).**
- Files: `applications/host_gui/session_renderer.cpp`, `main.cpp` (the callback),
  `modules/nodes/capture/source/track_player.cpp`, `modules/nodes/room/source/room_node.cpp`.
- Accepts when: each silent return of `§3` class A in these files raises its counter and the
  renderer's output is unchanged for every input that reached it before.
- Build: `sd test -l small -f "SessionRenderer|TrackPlayer|RoomNode"`.
- Tests: new `tests/applications/host_gui/test_session_renderer.cpp` (orchestrator adds the line and
  removes `session_renderer.hpp` from `untested_units.txt:11`), cases in `test_track_player.cpp` and
  `test_room_node/test_rt_safety.cpp`.
- Waits on: C1.

**C5. The plug-in's silent blocks (A10 to A13).**
- Files: `rig_adapter.cpp`, `rig_adapter.hpp`, `clap_plugin.cpp`.
- Accepts when: a channel or frame mismatch, an unknown routing and a refused event each raise a
  counter, `dropped_parameter_changes()` is reported, and the process path performs no allocation.
- Build: `sd test -l small -f RigAdapter`.
- Tests: cases in `tests/applications/plugin/test_rig_adapter.cpp`.
- Waits on: C1.

**C6. Output clamp and solver flush (A18, A19).**
- Files: `modules/foundation/circuits/include/SushiDSP/foundation/circuits/output_safety_clamp.hpp`,
  the four amplifier sources at their `apply_output_safety_clamp` lines (`jcm800.cpp:361`,
  `peavey5150.cpp:547`, `mesa_mark2c.cpp:634`, `diezel_vh4.cpp:417`), `tube_screamer.cpp`.
- Accepts when: a block that clips, and a block that holds a NaN, raise their counters by the
  number of samples, a clean block raises none, and the output samples are unchanged.
- Build: `sd test -l small -f "OutputSafetyClamp|Jcm800|Peavey5150|MesaMark2c|DiezelVh4|TubeScreamer"`.
- Tests: cases in `tests/foundation/circuits/test_output_safety_clamp.cpp`; the amplifier
  regression tests stay unchanged and green (`sd test -l medium` on the four).
- Waits on: C1, Q6 (the clamp's signature).

**C7. Hosts report on the control thread.**
- Files: `applications/host/main.cpp`, `applications/host_asio/main.cpp`, `applications/host_gui/main.cpp`
  (the frame loop), `applications/plugin/clap_plugin.cpp` (`on_main_thread`).
- Accepts when: a host whose counters moved writes one `WARN` per interval and a host whose
  counters did not writes none; the two console hosts print no more than they do today.
- Build: `sd test -l small -f "ClapPlugin|SessionRenderer"`.
- Tests: a case for `on_main_thread` in a new `tests/applications/plugin/test_clap_plugin.cpp`
  (orchestrator adds the line). The three `main()` functions are reported untested.
- Waits on: C1 to C5.

## Stage 3: the profiler in the code

**P1. The front.** Waits on Q3, E1, E2.
- Files: `modules/foundation/profile/include/SushiDSP/foundation/profile/profile.hpp`
  (`SD_ZONE`, `SD_COUNTER`, `SD_INSTANT` over the `_AT` macros with a per-translation-unit level),
  `sushidsp_profile_names.hpp` (timelines `dsp.block`, `bench.block`; groups; counter names),
  `profile/README.md`.
- Accepts when: with `SD_ENABLE_PROFILER` 0 a zone is an empty object and the translation unit
  includes no SushiProfiler header, and with it 1 a zone inside a `Session` window appears in the
  record under its group.
- Build: `sd test -l medium -f Profile`.
- Tests: new `tests/foundation/profile/test_profile.cpp` (orchestrator adds the line): compiled
  out, gate closed writes no event, recording writes one, a counter folds by its operation.

**P2. Reports from the solvers that return nothing (D4).** Waits on Q7.
- Files: `newton_raphson_solver.hpp`, `band_response_fit.hpp`, `band_response_fit.cpp`, the two READMEs.
- Accepts when: each reports its step count and how it ended (cap, non-finite step, fixed point,
  converged), and every existing result is bit for bit what it was.
- Build: `sd test -l small -f "NewtonRaphsonSolver|BandResponseFit"`.
- Tests: new cases for each ending in `test_newton_raphson_solver.cpp` and `test_band_response_fit.cpp`;
  the existing cases unchanged.

**P3. Solver counters into the record.** Waits on P1, P2, C1 (or Q6c).
- Files: `tube_screamer.cpp` (flush `non_converged`, `nan_fallback`, `solves`, worst iterations per
  block, then `reset_statistics()`), `triode_stage.cpp`, `pentode_stage.cpp` and the four
  amplifiers' ends of block for the Newton tally.
- Accepts when: after one block the record's `counter:` values equal the circuits' own tallies and
  the process path performs no allocation with a session open.
- Build: `sd test -l medium -f "TubeScreamer|Jcm800|Peavey5150|MesaMark2c|DiezelVh4"`.
- Tests: one `Medium_` case per node with a session, plus the existing `PerformsNoHeapAllocation`
  cases rerun with the gate open.

**P4. Node zones in `RigGraph`.** Waits on P1, Q1, Q5.
- Files: `rig_graph.cpp`, `rig_graph.hpp` (one `NameID` per handle, interned in `compile()`).
- Accepts when: a graph of three nodes processed in a session writes one zone per node named by its
  `type_id()` and one for the gather, writes nothing with the gate closed, and
  `ProcessPerformsNoHeapAllocationAcrossManyBlocks` passes with the gate open after warm-up.
- Build: `sd test -l medium -f RigGraph`.
- Tests: new `Medium_` cases in `tests/foundation/core/test_rig_graph/test_rt_safety.cpp`.

**P5. Track lanes and zones in `SessionRenderer`.** Waits on P1, C4, Q4.
- Files: `session_renderer.cpp`.
- Accepts when: eight tracks rendered in a session give eight lanes, each with the zones of
  `§2.2`, and the rendered samples are unchanged.
- Build: `sd test -l medium -f SessionRenderer`.
- Tests: cases in `tests/applications/host_gui/test_session_renderer.cpp`.

**P6. The block period and the dropout counters.** Waits on P1, Q4, Q10.
- Files: the callbacks Q4 names (`sdl2_audio_device.cpp`, `asio_audio_device.cpp`, or the four
  program callbacks), `applications/plugin/clap_plugin.cpp`.
- Accepts when: a device run for 100 blocks yields 100 periods on `dsp.block`, each with a budget
  of `frames / sample_rate`, a zone `dsp.process`, and the counters Q10 names.
- Build: `sd test -l medium -f "Sdl2AudioDevice|ClapPlugin"`.
- Tests: `Medium_` cases with the SDL2 dummy driver; the ASIO branch is unverified without a driver.

**P7. Region zones inside each node (Q5a).** One task per node, same shape: `P7a` Jcm800, `P7b`
Peavey5150, `P7c` MesaMark2c, `P7d` DiezelVh4, `P7e` TubeScreamer, `P7f` RoomNode. Waits on P1, Q5.
- Files: the node's `source/<node>.cpp` and README. Regions for the JCM800: bright cap, upsample,
  oversampled loop, downsample, post, clamp (`jcm800.cpp:317-361`); for the room: tail, modal, pair
  render, ears, microphones (`room_node.cpp:156-203`).
- Accepts when: the regions appear in a record under one group per node, and the node's regression
  golden is bit for bit unchanged.
- Build: `sd test -l medium -f <Node>` (golden and trap tests included).
- Tests: one `Medium_` case per node checking the region names and counts.

**P8. Stage-major loops, only if Q5b.** Not planned until the owner answers. It opens with a
`performance` measurement of the JCM800 loop restructured stage-major, five runs each way, before
any other amplifier is touched.

## Stage 4: the harness, the budgets and the hand-timed tests

**B1. The harness on a timeline.** Waits on P1, Q8, Q21.
- Files: `benchmark_harness.hpp`, `node_benchmark.hpp`, `solver_counters.hpp`,
  `benchmark_registry.hpp` as Q21 needs, `benchmark_harness/README.md`.
- Accepts when: `measure` runs 20 warm-up and 2000 active periods of `bench.block` with the budget
  `frames / sample_rate` and returns the median, p99, maximum and jitter from the record; the
  harness's own tests assert the period counts, the warm-up count and the budget, never a time.
- Build: `sd test -l medium -f "BenchmarkHarness|NodeBenchmark|SolverCounters|BenchmarkRegistry"`.
- Tests: the five files in `tests/foundation/benchmark_harness/` are rewritten; the two that hold a
  `Session` become `Medium_`.

**B2. The printed budgets become checks.** Waits on B1, Q8, Q9.
- Files: one checks file per budgeted case in the unit's test folder, written in SushiProfiler's
  published format, for the 12 rows of `§2.5`; the 12 `benchmark_*.cpp` lines that pass a budget.
- Accepts when: `sd bench Jcm800` prints each check's verdict and its reason, a bound of 1 ns fails
  with `max is ... above at most 1 ns`, and the rows Q9 names as expected failures are labelled so.
- Build: `sd bench -t release Jcm800 Room`.
- Tests: a `Medium_` case that runs `check` on a fabricated record with a tight bound.

**B3. The three hand-timed tests.** Waits on B1, Q8.
- Files: `tests/nodes/room/test_band_response_fit.cpp`, `test_image_source_solver.cpp`,
  `test_room_render_set.cpp`.
- Accepts when: each of the three keeps its bound (10, 25 and 33 ms) as a check over a record, no
  `steady_clock` remains in the three files, and each skips in an unoptimised build as before.
  Under Q8(c) the bound becomes a counter (steps, taps) and the time check is dropped.
- Build: `sd test -t release -l medium -f "BandResponseFit|ImageSourceSolver|RoomRenderSet"`.
- Tests: the three cases move to `Medium_` suites in the same files.

**B4. Deterministic solver checks.** Waits on P3, Q8.
- Files: `tests/nodes/tubescreamer/test_tube_screamer/test_behaviour.cpp`,
  `tests/nodes/room/test_band_response_fit.cpp`.
- Accepts when: over the reference tone, the tube screamer's clip stage records no non-converged
  solve and no NaN fallback and stays under its iteration cap, and the band fit's cap hits on a
  fixed set of targets equal a stated count.
- Build: `sd test -l medium -f "TubeScreamer|BandResponseFit"`.
- Tests: those cases, each stating its tolerance.

**B5. The rate-limit test (Q17).** Waits on Q17.
- Files: `applications/host_gui/slot_graph.hpp`, `.cpp`, `rig_controller.cpp` for (a);
  `tests/applications/host_gui/test_rig_controller.cpp`.
- Accepts when: the test drives an injected clock and asserts the publish count exactly, with no
  `sleep_for` and no `steady_clock`.
- Build: `sd test -l small -f RigController`.

## Stage 5: close (orchestrator)

**D1. Manual, design, backlog, changelog.** One commit per merged task, as `documentation` asks.
- Pages the work makes false: `modules/foundation/benchmark_harness/README.md`,
  `modules/foundation/timing_gate/README.md`, `modules/foundation/log/README.md` ("The audio
  thread"), `docs/architecture/MODULES.md`, `docs/guides/COMMAND_LINE_INTERFACE.md` (`sd bench`
  prints verdicts), `tests/README.md`, `tests/nodes/room/README.md` (the three timing entries),
  `docs/design/SUSHIDSP/README.md` (the phase row, Q18).
- Backlog: an entry for each of Q9's expected failures and for each SLG and SPG a workaround stands
  in for; `docs/reference/KNOWN_ISSUES.md` for B28 and the unverified ASIO sites.
- Accepts when: `tools/documentation/check_docs_truth.py`, `check_changelog.py` and
  `check_docs_layout.py` pass, and `git grep -n <old name> -- '*.md'` finds no old name.
- Build: `sd check`.

## Order and waves

Inline, the order is E1 to E3, then L1 to L11, C1 to C7, P1 to P7, B1 to B5, D1, which is the
hierarchy order of the stages above. The owner chooses
inline or waves.

If waves, the file sets below are disjoint inside a wave.

| Wave | Tasks | Waits on | Closes with |
| --- | --- | --- | --- |
| 0 | E1, E2, E3 | Q1 to Q3 for E1 and E2 | `sd build` |
| 1 | L1, L2, L3, L4, L5, L7, L9 | E3, Q11 (for B23), Q13 | one `sd test -l small` |
| 2 | L6, L8 | wave 1 | one `sd test -l small` |
| 3 | L10, L11 | Q12, Q20 | one `sd test -l small` |
| 4 | C1 | Q6, E3 | `sd test -l medium -f RtCounter` |
| 5 | C2, C3, C5, C6 | C1, Q6, Q10, Q11, Q14 | one `sd test` |
| 6 | C4, P1, P2 | C1, Q3, Q7, E1, E2 | one `sd test` |
| 7 | C7, P3, P4, P5 | wave 6, Q1, Q4, Q5 | one `sd test -l medium` |
| 8 | P6, P7a to P7f, B1 | wave 7, Q4, Q5, Q10, Q21 | one `sd test -l medium` |
| 9 | B2, B3, B5 | B1, Q8, Q9, Q17 | `sd bench` and one `sd test -t release` |
| 10 | B4 | P3, Q8 | `sd test -l medium` |
| 11 | D1 | wave 10 | `sd check` |

A pair that shares a file is kept in different waves: `L5` and `C4` (`room_node.cpp`), `L6`, `L9`
and `L10` (`test_session.cpp`, `session_timeline.cpp`), `L8` and `L11` (`slot_graph.cpp`), `C4` and
`P5` (`session_renderer.cpp`, `test_session_renderer.cpp`), `C6`, `P3` and `P7` (the four amplifier
sources and `tube_screamer.cpp`), `B3` and `B4` (`test_band_response_fit.cpp`).

## What the owner has to do

- Answer Q1 to Q21 in `SPEC.md`, section 7. Wave 1 needs only Q11 (one row of L3) and Q13; the rest
  of stage 1 can begin without an answer.
- Run the ASIO sites on a driver: L3, C2 and P6 leave them unverified.
- Decide the SushiLog and SushiProfiler gaps (Q19) before wave 3, since C1 is the workaround for
  SLG-1 and SLG-2.

## Decisions taken during the build

Each is an engineering call the rulings of `SPEC.md` section 9 do not cover.

- Products start a profiling session from the file `SD_PROFILE_CONFIG` names, through `ProfileSession` in `foundation/profile`: nothing else in the stack names a start, and an environment variable needs no new interface.
- `ProfileSession`, `ProfileRecord` and `run_profiled` hide SushiProfiler behind a pimpl: no product or test header includes a SushiProfiler header, so the module stays the only includer.
- `RigGraph`, `RoomNode` and the amplifier counters are process-wide: a rig replaces its graph and its nodes, and a reporter must hold one object whose count survives the swap.
- `RigGraph::process` writes the zone `graph.process`, and `dsp.process` sits in `SessionRenderer::render` and the three program callbacks: two sites with one name make the subject `zone:dsp.process` ambiguous in a check.
- `apply_output_safety_clamp` returns one count of clipped and non-finite samples, as Q6 says; a NaN is counted and passes unchanged, so no output sample moves.
- `AudioDevice` gains `add_counters_to` with an empty default body: hosts reach a device through the interface, and no device or test double has to change.
- `AudioDevice` and `RigGraph` forward-declare or avoid the headers they only name, and each module that owns counters writes its own report under its own category (`CoreCounterReport`, `DeviceCounterReport`), because the logging checker forbids one module logging under another's category.
- `profile::NameID` is a `std::uint32_t` in the public headers of `RigGraph` and `SessionRenderer`, so they include no profile header; `.cpp` files include it.
- The five units that gained a test leave `tools/testing/untested_units.txt`, and the same-tier edges the new includes need are added to `tools/settings.py`, so `check_all` reads true; the CMake table is the owner's.
- Known issue entries: the cabinet row cites RW-9, which already holds it, so the entry that the backlog rule would refuse is not written.
- `FmvToneStack` and `PowerAmpFeedback` derive in their constructor, so the constructor throws (Q13a) where the plan said `prepare()`.
- `SessionRenderer::render` keeps cutting a long block and counts it: Q14 settled `RigGraph` only, and its acceptance asks for unchanged output.
- `SDL_PauseAudioDevice` returns nothing, so B22 has no SDL2 status to log.
- Region zone names live in `sushidsp_profile_names.hpp` under one namespace per node: a name two nodes share would make two subjects with one name.
- `RigController::set_cabinet` gains an overload with a reason string, so the stage panel shows the node's own message; the two-argument form stays.
- Counters are recorded every block, as 0 when nothing happened: a `counter:` check fails on a subject with no samples.
- `parameter_table.hpp` includes no log header: the by-id miss calls `report_unknown_parameter`, one non-template function in `parameter_report.cpp`, so a change to the log front no longer rebuilds every node.
- The profiler's static or shared target is chosen per build, not per target: the cache option `SD_PROFILER_STATIC` (default OFF) selects `SushiProfiler::SushiProfilerStatic` and defines `SUSHIPROFILER_STATIC` for every library of the build, because a library compiled static and linked to the shared profiler loses the collector's dllimport data on Windows. The plug-in package is built in a tree of its own with it ON, and keeps its own collector.
- `timing_gate` is deleted: SushiProfiler checks in `.checks.json` files judge a time, and no test uses the gate any more.
- `SD_ENABLE_PROFILER` and `SD_PROFILE_LEVEL` come from a generated `profile_config.hpp` (`configure_file` of `modules/foundation/profile/profile_config.hpp.in`), not from PUBLIC compile definitions, so a change of level rebuilds only the files that include it; `profile.hpp` includes it, defines neither macro and keeps no default, so a missing header fails to compile. SushiProfiler is found with `find_package(SushiProfiler CONFIG REQUIRED)` under `SD_ENABLE_PROFILER`.
