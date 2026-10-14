# Test tree to the testing skill: plan

**Goal:** Build what [`SPEC.md`](SPEC.md) describes.

**State on 2026-10-15.** T1 is committed with this folder. The owner decided the open points in
SPEC section 2. T0 waits for one answer (section T0 below). Nothing else is built. Build and test
wait for the owner's permission.

## Constraints

- SOLID. Each checker, module and test file answers one question.
- No `sd build` and no `sd test` until the owner allows it; then one Release build with four
  threads, and a rebuild only for what that build turns up.
- Every existing test stays, with the same assertions, and passes after the move.
- Code, its README, the manual pages it makes false and its changelog line land in one commit.
- Commits stage by path. No `git stash`, no `git add -A`, no worktrees.
- A module name is never `utils`, `common`, `helpers`, `shared`, `misc`, `support`, `testing` or
  `test_support`, with or without a prefix (`repository-layout`, module rule 1).

## Order

Hierarchy first: the include form (T0) comes before anything that writes new include lines, so
the new test files and modules are written once in their final form. The checker (T1) is done,
and it is how the moves are proved. The shared modules (T5) come before the move (T4) that depends
on them. The units without tests (T6) come after the move, so each new file lands at its mirror
path.

| Task | Files | Accepts when |
| --- | --- | --- |
| T0 Include path to tier form | every project `#include "SushiDSP/..."` under `modules/`, `applications/`, `tests/`, `tools/`; the module headers, if the open point in T0 says they move; `tools/layering/check_layering.py` for the new form; the module `CMakeLists.txt` include roots | Every project include is `<SushiDSP/<tier>/<module>/<name>.hpp>` with angle brackets; `git grep` finds no `#include "SushiDSP/` outside `docs/archive/`; the layering check reports nothing; one Release build at the end, after the owner allows it. |
| T1 Mirror checker | `tools/checking/`, `tools/check_all.py`, `tools/testing/check_test_mirror.py`, `tools/tests/test_check_test_mirror.py`, `tools/tests/test_ignored_paths.py`, `tools/documentation/check_source_comments.py`, `tools/README.md` | Done and committed. Tools tests pass (72 of 72). The mirror checker runs and reports 387 findings on the tree before T2 to T7. |
| T2 Labels per test | `tests/CMakeLists.txt` | Every test has one label from `unit`, `regression`, `golden`, `benchmark`, `device`; no label comes from a name prefix; the count per label is the same as before. |
| T3 Names read as behaviour | every `test_*.cpp` under `tests/functional/` | No `Unit_` or `Regression_` prefix remains; each test name starts with the unit it tests; 1313 tests in all. |
| T5 Shared test code as modules | new internal modules (table below), their `CMakeLists.txt` and READMEs, `cmake/SushiDSPLayers.cmake` for the same-tier edge | Each module names what it owns and depends on; its headers are not installed; the layering check reports nothing for it; `tests/measure`, `tests/circuits`, `tests/common` and the test-side `tests/golden` helper are gone. |
| T4 Move to the mirror | `tests/functional/**` to `tests/<tier>/<module>/` and `tests/applications/<app>/`; the golden, fixture and benchmark folders to their owners | The checker reports no misplaced file and no folder without a counterpart; `tests/functional/` and `tests/golden/` are gone. |
| T6 Units without a test | 99 units (list below), written foundation, then nodes, then applications, one task per group | Each unit has one test file at its mirror path, or a folder when its tests need several files. The checker reports no untested unit. |
| T7 Unmatched test files | 29 files (list below) | Each is renamed to the unit it tests, moved, or merged; the checker reports no orphan. |
| T8 Split the README | `tests/functional/unit/README.md` (4401 lines) into the READMEs of the owners it describes | Each README is at most 800 lines; every section of the old file lands in one of them. |
| T9 Manual and backlog | `docs/guides/`, `tests/` READMEs, `docs/reference/CHANGELOG.md`, `docs/design/REMAINING_WORK.md` (the item at line 168 closes), `docs/reference/KNOWN_ISSUES.md` if a test is quarantined | `check_docs_layout.py`, `check_changelog.py` and `check_docs_truth.py` report nothing for the files this change touches. |
| T10 Verification | none | After the owner allows it: one Release build; `sd test -s unit` and `sd test -s regression` green with their counts; `python tools/check_all.py .` reports the mirror checker clean. `check_docs_truth.py` findings that predate this work are reported, not fixed here. |

## T0: include paths to the tier form

The rule asks for `<Project>/<tier>/<module>/<name>.hpp` with angle brackets (`repository-layout`,
Names; `cpp-code-style`, Includes). SushiDSP today uses quotes and the flat form,
`"SushiDSP/<module>/<name>.hpp"`. The owner chose the tier form on 2026-10-15.

Measured: 1,235 include lines in 425 files over the whole repository, counting the archived plans
under `docs/archive/`. The archive is frozen and keeps its text. The source and test count is
taken in T0 before any edit.

**Open point, for the owner before T0 starts.** `repository-layout` shows a module's header at
`modules/<tier>/<module>/include/<Project>/<module>/<name>.hpp`, which has no tier, while its
include form has one. Two readings:

- (a) The headers move: `modules/<tier>/<module>/include/<Project>/<tier>/<module>/<name>.hpp`.
  The include line then equals the path below `include/`. The plan assumes this reading.
- (b) The headers stay where they are and only the include line gains the tier. The include line
  then no longer equals the path, which the rule's own tree does not describe.

Steps, once (a) is confirmed:

1. Count the include lines per module and per application, and the header moves.
2. Move each header by path with `git mv`, then rewrite the include lines with a one-off script
   kept in the scratchpad, never in the repository.
3. Update each module's `CMakeLists.txt` include roots so a module sees only the roots it declares
   (`repository-layout`, module rule 6).
4. Teach `tools/layering/check_layering.py` the angle-bracket tier form. Its test follows it.
5. Update every manual page that names an include path (`git grep -n "SushiDSP/" -- '*.md'`,
   excluding `docs/archive/`), in the same commit.

Acceptance: as in the table. The layering check is the proof that no include reaches up a tier.

## T5: the shared modules, confirmed

The owner confirmed the five modules on 2026-10-15, each named for its responsibility and placed in
the lowest tier its dependencies allow:

| Module | Tier | Holds now | Depends on |
| --- | --- | --- | --- |
| `signal_measure` | foundation | `tests/measure/` render, window, spectrum, thd, frequency_response, alias_floor, null_test | `SushiDSP/core/audio_node`, `SushiDSP/math/fft` |
| `spice_reference` | foundation | `tests/measure/spice_csv.hpp`, `tests/circuits/spice_block_compare.hpp` | `SushiDSP/math/biquad` |
| `circuit_blocks` | foundation | `tests/circuits/dk_block_node.hpp`, `tests/circuits/jcm_v1a_netlist.hpp` | `SushiDSP/core/audio_node`, `SushiDSP/math/dk`, `SushiDSP/circuits/models` |
| `golden_compare` | foundation | `tests/golden/golden_file.hpp` | nothing |
| `timing_gate` | foundation | `tests/common/timing_gate.hpp` | GoogleTest only, linked `PRIVATE` (`dependencies`, Wiring) |
| `room_scenes` | nodes | `tests/common/room_fixtures.hpp`, `tests/common/room_ports.hpp` | `room`, a same-tier edge, to be listed in `cmake/SushiDSPLayers.cmake` |

The benchmark programs leave `tests/benchmark/`. Each `benchmark_<name>.cpp` moves to the tests of
the owner of the unit it times, as `TESTS.md` gives (`tests/<owner>/benchmark_<name>.cpp`). The one
current case is placed when T4 runs, from the unit it times.

`tests/measure/window.hpp` is used by no test file directly; it is part of `signal_measure`.

## T6: the 99 units without a test of their own

Written in three groups, one task each, in this order: foundation (32), nodes (30), applications
(37). Each group is one task with its own acceptance and its own commit set.

Foundation units, 32, by module:

- `foundation/circuits`: soft_transition, triode12_ax7, antiderivative_antialiaser, grid_conduction_law, plate_swing, power_tube_presets
- `foundation/core`: audio_node, cabinet_host, calibration, node_descriptor, parameter_change, parameter_table, port_layout
- `foundation/device`: asio_audio_device, sdl2_audio_device
- `foundation/io`: ir
- `foundation/log`: capturing_log_sink, default_logger, log, log_argument, log_category, log_level, log_log_category, log_record, log_sink, scoped_log_sink, stderr_log_sink
- `foundation/math`: dense_lu, dk_netlist, linear_port, halfband_coefficients, resampler_design

Nodes units, 30, by module:

- `nodes/capture`: capture_log_category, recording_bus, take
- `nodes/dd3`: wet_fifo
- `nodes/diezel_vh4`: diezel_vh4_tone_stack
- `nodes/mesa_mark2c`: mesa_mark2c_tone_stack
- `nodes/peavey5150`: peavey5150_tone_stack
- `nodes/room`: built_room_loss, cabinet_geometry, cascade_filter, delay_ring, diffuse_field, direct_path, early_tap_set, environment_render_set, fade_window, fdn_design, fir_filter, frame_delay, furniture, materials, modal_design, pair_stage, receiver_output, room_host, room_log_category, source_stage, stage_limits, starter_scene
- `nodes/tubescreamer`: tube_screamer_circuits

Application units, 37, by application:

- `host_gui`: host_gui_log_category, mp3_decoder, mp3_encoder, rig_persistence, rig_shape, session_bounce, session_file, session_renderer, session_timeline, slot_graph, slot_selection, arrangement_view, cabinet_section_pane, device_settings, divider, file_browser, gear_browser, geometry, meter_bar, mixer_model, mixer_panel, rig_chain_view, room_panel, room_scene_view, scene_interaction, stage_panel, text_field, theme, top_bar, transport_bar, widgets
- `plugin`: clap_boundary, clap_extensions, clap_plugin, parameter_routing, rig_parameter, startup_rig

Notes for each group before it starts:

- Some of these may be generated or table-only (`halfband_coefficients`, `resampler_design`); a
  test of a generated table checks the table, not a hand-written value.
- Several are tiny log headers already covered by `test_logger`. The rule still asks for a file of
  their own, so each gets a short one.
- Many GUI headers need ImGui or a window. Their tests either build without it, or the unit is
  recorded in REMAINING_WORK.md with the reason, under an allow-list entry the owner approves.

## T7: test files that match no unit by name

The checker and this list agree: 125 test files match a header by name and only move (T4). These 29
do not:

- Regression, by product: `test_dd3_regression`, `test_diezel_vh4_regression`,
  `test_jcm800_regression`, `test_mesa_mark2c_regression`, `test_peavey5150_regression`,
  `test_tube_screamer_regression`. Each is the golden comparison of one product. They go to the
  product's module as its regression file, or into a folder for that product.
- Name differs from the header: `test_triode_12ax7` (header `triode12_ax7`), `test_ts_clip_stage`
  and `test_ts_tone_stage` (the header names differ), `test_dk_op_amp`, `test_dk_triode_stage`,
  `test_spice_csv`, `test_resampler_coefficients`, `test_measure`.
- Room group: `test_room_bras`, `test_room_environment`, `test_room_furniture`,
  `test_room_materials`, `test_room_motus`, `test_room_records`, `test_room_scene_furnishings`,
  `test_room_starter_scene`. Several cover one header each under a different name, or a scene.
- Logging and runtime: `test_dd3_acceptance`, `test_directory_listing_logging`,
  `test_log_compile_time_cut`, `test_room_node_logging`, `test_track_recorder_logging`,
  `test_rig_graph_output_ports`, `test_rt_safety`. `test_rt_safety` is the allocation trap; it
  stays one file and its unit is named in T7 when the owner decides.

## Who does what

Inline: the session does T0, then T2 to T9, one task at a time, and T10 after the owner allows a
build. No worker is dispatched; the owner chose inline.

## Decisions

Taken on 2026-10-15:

- Five shared modules, as in T5, with `timing_gate` added as a foundation module.
- Benchmark programs stay with the owner of the unit they time.
- The 99 units are written layer by layer: foundation, nodes, applications.
- T1 is committed now, staged by path.
- The include form moves to the tier form (T0).

Still open:

1. T0: the header location, reading (a) or (b). The plan assumes (a) until the owner says otherwise.
2. Build permission: T0 and T2 to T9 are verified by the checkers and reading, not by a build. The
   one Release build at the end (T10) needs the owner's go-ahead.
3. T7: the unit named for `test_rt_safety`.

## Result

T1 committed. T0 and T2 to T10 planned. No test moved or written, no CMake edited, no build run.
