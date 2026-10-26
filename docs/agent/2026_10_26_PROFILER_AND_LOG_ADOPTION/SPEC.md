# SushiProfiler adoption and the logging gaps

**Status:** Open — analysis written 2026-10-26; section 9 holds the decisions; the code is written and waits for the first build.
**Implements:** SUSHIDSP#DP

## 1. What this is

An analysis of where SushiDSP stands against SushiProfiler v0.1.0 (design phase P7: the benchmark
harness on a budget timeline, solver counters, checks in place of hand-timed tests) and against
SushiLog (code that should log and does not). It writes no code. `PLAN.md` holds the tasks.

Every claim carries `file:line`. A path with no prefix is in SushiDSP. `SushiProfiler:` and
`SushiLog:` name the other two checkouts: SushiProfiler at `bc86dc9` with tag `v0.1.0`, SushiLog at
`e39e190`, which is past `v0.2.3`; SushiDSP pins `v0.2.2` (`cli/sushistack.deps.toml:28`).

No number here was measured. Costs are taken from the repository's own documents or computed from
counts the source shows, and each says which.

The real-time rule that bounds both topics is in `CLAUDE.md:40-45`: `AudioNode::process()` and
everything it calls allocates nothing, locks nothing, makes no system call and throws nothing.

## 2. Profiling

### 2.1 The processing block as a period

A block is one call of a device callback. Four programs own a callback, and two devices call it.

| Program | Callback | Reaches | Budget source |
| --- | --- | --- | --- |
| `host` (SDL2) | `applications/host/main.cpp:85-90` | `RigGraph::process` | `actual.block_size` and rate, `main.cpp:54` |
| `host_asio` | `applications/host_asio/main.cpp:228-241` | `RigGraph::process`, then the channel adapter | `actual`, `main.cpp:135` |
| `host_gui` | `applications/host_gui/main.cpp:389-424` | `SessionRenderer::render`, `applications/host_gui/session_renderer.cpp:50-150` | `actual`, `main.cpp:349` |
| CLAP plug-in | `applications/plugin/clap_plugin.cpp:243-264` | `RigAdapter::process`, `applications/plugin/rig_adapter.cpp:291-324` | the host's `frames_count` |
| SDL2 device | `modules/foundation/device/source/sdl2_audio_device.cpp:128-159` | the program's callback, plus interleave at `:157-158` | `frames` from `length_bytes`, `:133-134` |
| ASIO device | `modules/foundation/device/source/asio_audio_device.cpp:402-454` | the program's callback, plus sample conversion at `:409-419` and `:436-448` | `format_.block_size`, `:405` |

The budget of a block is `frames / sample_rate`: 5.333 ms at 256 frames and 48 kHz, 1.333 ms at
64, 0.667 ms at 32. The benchmark fixes 256 and 48 kHz (`modules/foundation/benchmark_harness/include/SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp:38,41`).
`PeriodTimeline` takes a budget per timeline or per mark
(`SushiProfiler:modules/capture/periods/include/SushiProfiler/periods/period_timeline.hpp:55,66-67`),
so a host whose block size changes between calls can state each block's own budget.

What a block period contains depends on where it is marked: the device layer includes the
deinterleave or sample conversion, `SessionRenderer::render` includes the track mix, the
metronome and the recorder write (`session_renderer.cpp:94-144`), and `RigGraph::process` alone
includes none of them. This is question Q4.

### 2.2 Zones in the signal chain

Three levels of granularity exist in the code today.

| Level | Where | Zones per block | Cost at 13.3 ns a zone |
| --- | --- | --- | --- |
| Track | `SessionRenderer::render` loop, `session_renderer.cpp:70-125`: lane read `:94`, graph `:104`, mix `:117-125`, recorder `:140`, metronome `:144` | about 5 per track, 8 tracks at most | under 1 µs |
| Node | `RigGraph::process` calls each node at `modules/foundation/core/source/rig_graph.cpp:578`; the fan-in and fan-out sums between nodes are `:464-570` | one per node plus one for the gather | under 1 µs for a chain of ten |
| Inside a node | `Jcm800::process`: bright cap `:317-320`, upsample `:322`, oversampled loop `:323-344`, downsample `:345-346`, post `:349-352`, clamp `:361` (`modules/nodes/jcm800/source/jcm800.cpp`) | 6 regions | negligible |
| Inside the oversampled loop | 14 `process_sample` calls per oversampled sample, `jcm800.cpp:325-343`, 2048 samples a block (`jcm800.cpp:280`, factor 8 at `jcm800.hpp:166`) | 28,672 | 381 µs, computed |

The 13.3 ns per recorded zone and 1.3 ns with the gate closed are SushiProfiler's own
measurement (`SushiProfiler:docs/reference/CHANGELOG.md:35`). A JCM800 costs about 0.68 ms a block
by the repository's figure (three JCM800s cost 2.03 ms, `docs/design/REMAINING_WORK.md:79`), so
zones around every stage in the loop add 56 % and write 28,672 events against a default ring of
65,536 events (`SushiProfiler:docs/design/PROFILER/CAPTURE_AND_SESSION.md`, configuration
example), which fills in two and a third blocks. A per-stage zone inside the loop measures the
zone, not the stage. The other amplifiers have the same shape (`peavey5150.cpp:547`,
`mesa_mark2c.cpp:634`, `diezel_vh4.cpp:417` end in the same clamp; I read the loop of the JCM800
only).

What the code supports without restructuring: zones per node from `RigGraph::process`, named once
at `compile()` from `AudioNode::type_id()` (`modules/foundation/core/include/SushiDSP/foundation/core/audio_node.hpp`),
which needs no edit in any node; region zones inside a node; and a lane per track
(`SushiProfiler:modules/capture/zones/README.md`, `SP_LANE`), because the audio thread serves up
to eight tracks (`session_renderer.cpp:70`). Per-stage cost inside the loop is Q5.

### 2.3 Solver counters

SushiDSP has four iterative solvers. Only the first is counted.

| Solver | Where it runs | What is kept | Read by |
| --- | --- | --- | --- |
| `DkCircuit` with its `DampedNewtonSolver` | tube screamer clip stage, `modules/nodes/tubescreamer/include/SushiDSP/nodes/tubescreamer/tube_screamer_circuits.hpp:114`, called per oversampled sample at `modules/nodes/tubescreamer/source/tube_screamer.cpp:130` | `non_converged_samples_` `dk_circuit.hpp:285`, `nan_fallback_samples_` `:359`, `total_samples_` `:362`, worst iterations `damped_newton_solver.hpp:163-165,321-323`; plain integers (`dk_circuit.hpp:811-813`, `damped_newton_solver.hpp:347`) | only `solver_counters.hpp:40-49`, in the benchmark. `dk_block_node.hpp:66` resets them. |
| `NewtonRaphsonSolver::solve_joint` | the triode and pentode stages of every amplifier (`modules/foundation/circuits/source/stages/triode_stage.cpp:66-95`, `pentode_stage.cpp:100`) | nothing: it returns a `float` (`modules/foundation/math/include/SushiDSP/foundation/math/newton_raphson_solver.hpp:60-64,114`). Three exits are invisible: iteration cap `:81`, non-finite step `:94-97`, fixed point `:108-111` | nobody |
| `BandResponseFit::fit` | publish path of the room, control thread | `Result{coefficients, worst_error_db}` only; cap of 8 steps `modules/nodes/room/source/band_response_fit.cpp:50,442`, exit on no accepted step `:475-478` | nobody. `docs/design/REMAINING_WORK.md:152-158` says it stops at the cap on about 60 % of fits |
| Mode and tap enumeration | publish path | counts are cut at `K_MAX_MODES` (`modal_solver.cpp:363-373`) and `K_MAX_EARLY_TAPS` (`image_source_solver.cpp:393-396`) | nobody (section 3, class D) |

The `DkCircuit` instances of `FmvToneStack` and `PowerAmpFeedback` have no nonlinear branch
(`DkCircuit<K_STATE_COUNT, 0, 1, 1>`, `modules/foundation/circuits/include/SushiDSP/foundation/circuits/fmv_tone_stack.hpp:116`,
`power_amp_feedback.hpp:124`; the second parameter is the nonlinear count, as the benchmark's
`DkCircuit<7, 2, 2, 1>` comment at `tests/foundation/math/dk/benchmark_dk_circuit.cpp` states), so
no Newton solve runs in them. The tube screamer is the only production user of the counted
solver, and the amplifiers' own solver is the uncounted one.

The counters are plain integers that the audio thread increments per sample. A thread that reads
them while the block runs races with the writer; the benchmark reads them after its loop on the
same thread, which is why it works. A flush at the end of a block, on the audio thread, is the
race-free way to move them: read, emit one `SP_COUNTER` each, reset. The counter kinds exist
(`SushiProfiler:modules/capture/zones/include/SushiProfiler/zones/macros.hpp:83`, ops `add`, `set`,
`max`, `min`). Worst iterations is `max`, solves and failures are `add`. Which home the counters
have is Q6, and the interface for the two solvers that return nothing is Q7.

### 2.4 Load, dropouts and jitter

None of the three is measured (`SushiProfiler:docs/design/PROFILER/STACK_SURVEY.md:45`). With the
profiler:

- Load of a block is the time the callback spent divided by the block's budget. It comes from a
  zone around the work, not from the period (SPG-2).
- Jitter is the median absolute difference between successive periods, which the `distribution`
  analysis reports for a timeline or a zone (`SushiProfiler:docs/design/PROFILER/RECORD_AND_ANALYSES.md`,
  analyses table). Over the arrival timeline it is the device's scheduling jitter. Over the
  processing zone it is the cost's own jitter.
- A dropout has no single definition in the code. These are the signals that exist.

| Signal | Where | Today |
| --- | --- | --- |
| ASIO overload message | `asio_audio_device.cpp:139,149`: acknowledged, not acted on | lost |
| ASIO resync, latency change, clock change | `:147-150`, `:128-131` | lost |
| Callback ran past its budget | no code | not measured |
| Arrival gap longer than a block | no code | not measured |
| Recorder refused a block | `modules/nodes/capture/source/track_recorder.cpp:191`, `recording_bus.cpp:88`: atomic counted | shown in the transport bar only, `applications/host_gui/ui/transport_bar.cpp:429` |
| Parameter change refused by a full ring | `applications/plugin/rig_adapter.cpp:274-283`, `applications/host_gui/slot_graph.cpp:430-432` | shown in the stage panel only, `ui/stage_panel.cpp:277`; the plug-in's count has no reader |
| Block answered with silence | `sdl2_audio_device.cpp:136-140,150-154`, `applications/host_gui/main.cpp:396-398`, `rig_adapter.cpp:297-301`, `clap_plugin.cpp:247,255` (returns `CLAP_PROCESS_ERROR`) | silent |
| Non-finite or clipped output | `modules/foundation/circuits/include/SushiDSP/foundation/circuits/output_safety_clamp.hpp:45` (`std::clamp` returns a NaN unchanged, by its definition); `dk_circuit.hpp:326-332,347-360` | silent |

What counts as a dropout is Q10.

### 2.5 The benchmarks and the hand-timed tests

The harness runs 20 warm-up and 200 timed repetitions and keeps the fastest
(`benchmark_harness.hpp:71-72,77-87`), prints one row per case and prints the budget beside it,
which no code compares (`benchmark_harness.hpp:100-110`, printing only). `benchmark_main.cpp:44`
returns 0 whenever a group ran. The budgets are therefore documentation. The 19 `bench::measure` calls and
7 `measure_node` calls sit in 17 `benchmark_*.cpp` files listed in `tests/CMakeLists.txt:454-470`.

| Case | Budget printed | File:line |
| --- | --- | --- |
| Jcm800, Mesa Mark IIC+, Peavey 5150, Diezel VH4 | 1.5 ms | `tests/nodes/{jcm800,mesa_mark2c,peavey5150,diezel_vh4}/benchmark_*.cpp:49-50` |
| TubeScreamer | 0.25 ms | `tests/nodes/tubescreamer/benchmark_tube_screamer.cpp:47` |
| Dd3Delay | 0.15 ms | `tests/nodes/dd3/benchmark_dd3_delay.cpp:48` |
| DkCircuit at 8x | 0.12 ms | `tests/foundation/math/dk/benchmark_dk_circuit.cpp:196` |
| Resampler round trip | 0.10 ms | `tests/foundation/math/benchmark_resampler.cpp:69` |
| Eight-track mix | 5.33 ms | `tests/nodes/capture/benchmark_track_player.cpp:97` |
| RoomNode ears, swap, beside JCM800, cabinet section | 1.8, 2.8, 2.0, 2.67 ms | `tests/nodes/room/benchmark_room_node.cpp:310-314`, `:315` |
| Oversampler, partitioned convolution, triode stage, binaural decoder, modal bank, early reflections, tube screamer blocks, DK pot update | none | the rest of the 17 files |

Four tests time code by hand.

| Test | Bound | File:line | What a record replaces |
| --- | --- | --- | --- |
| `Small_BandResponseFit.BandResponseFitCostsUnderTenMillisecondsForOneHundredAndFiftyFits` | 10 ms | `tests/nodes/room/test_band_response_fit.cpp:241-255` | zone `room.band_fit` over 150 calls in one period, check `zone:` at most 10 ms |
| `Small_ImageSourceSolver.ImageSourceSolverSolvesTheGarageInsideThePublishBudget` | 25 ms | `tests/nodes/room/test_image_source_solver.cpp:277-286` | zone `room.image_source`, check at most 25 ms |
| `Small_RoomRenderSet.BuildRenderSetCostsUnderOnePublishBudget` | 33 ms, `K_PUBLISH_BUDGET_MS` at `test_room_render_set.cpp:65` | `tests/nodes/room/test_room_render_set.cpp:216-229` | zone `room.build_render_set`, check at most 33 ms |
| `Medium_RigController.RigControllerCoalescesADragToThirtyPublishesPerSecond` | none: it reads the wall clock to compute how many publishes the rate limit allows | `tests/applications/host_gui/test_rig_controller.cpp:1548-1590` | nothing. It tests a rate limit through `Clock::now()` at `applications/host_gui/slot_graph.cpp:668` and `slot_graph.hpp:377`; the fix is an injected clock (Q17) |

`SUSHIDSP_REQUIRE_OPTIMISED_BUILD` (`modules/foundation/timing_gate/include/SushiDSP/foundation/timing_gate/timing_gate.hpp:39`)
skips the first three in an unoptimised or sanitised build. A record carries the build facts
(`SushiProfiler:docs/reference/RECORD_FORMAT.md:33`), so the information the gate reads is in the
record; no check type reads it (SPG-7), so the gate stays.

Two rules collide here. `testing` rule 4 says no test depends on wall-clock time. `performance`
says a benchmark never fails a run and a micro benchmark gates a change only on a dedicated machine
against a baseline measured there. Turning a printed budget into a verdict makes a timing test.
Q8 asks which way it goes.

Turning the budgets into verdicts fails today on rows the backlog already holds: the cabinet
section reads 3.562 ms against 2.67 ms (`docs/design/REMAINING_WORK.md:78-80`, RW-9); the Mark IIC+
reads 1.852 ms against 1.5 ms (`docs/guides/FAQ.md:130`); the 8x DK block read 0.963 ms against
0.12 ms (`docs/design/SUSHIDSP/DSP_MATH_LIBRARY.md:102`); the 16x amplifiers read 99 to 102 % of
the block (`docs/design/REMAINING_WORK.md:40-62`, RW-4, RW-5); the DD-3's 0.15 ms was never judged
(`docs/design/SUSHIDSP/README.md:263`). Q9.

How a benchmark becomes a record. Today one repetition is one call of `body()`. On the profiler
one repetition is one period on a timeline `bench.block` whose budget is `256 / 48000` s, the
window is `{warmup: 20, active: N}`, and the numbers `measure` prints come from the record's
median, p99, maximum and jitter. 200 active periods make p99 the second-largest sample, which says
little; N is an engineering call and the plan sets 2000. A `Session` starts a consumer thread
(`SushiProfiler:modules/session/session/README.md`, "Start"), so a test that holds one is
`medium`, not `small` (`testing`, sizes table). One session runs per process, so such tests run
one after another.

### 2.6 What is safe on the audio thread

| Call | Safe | Why |
| --- | --- | --- |
| `SP_ZONE`, `SP_COUNTER`, `SP_VALUE`, `SP_INSTANT`, `SP_FLOW_*`, `SP_LANE`, `PeriodTimeline::mark` | yes, after the thread is registered and the site has been used once | one ring write, no allocation, lock or system call (`SushiProfiler:modules/capture/rings/README.md`, `push`) |
| The first record of a site | no | the registry takes a mutex and copies strings (`SushiProfiler:modules/capture/sites/include/SushiProfiler/sites/site_registry.hpp:91-96,125-126,138`); `admission.hpp:44-70` catches the allocation failure; the changelog proves "never allocates after first use" (`CHANGELOG.md:35`) (SPG-1) |
| The first record of a thread | no, unless `Collector::register_this_thread(capacity)` ran first (`collector.hpp:131`) | a ring is made on first use |
| `PeriodTimeline::tag` | no | takes a lock (`period_timeline.hpp:72-77`) |
| Interning a run-time name | no | locks the name table; intern at `prepare()` or `compile()` |
| The window's `before_mark` and `after_mark` | yes, by reading | arithmetic and one atomic store (`SushiProfiler:modules/session/window/source/window.cpp:63-75`); the plan proves it under the allocation trap |
| `Session::start`, `stop`, `timeline`, `report`, `check` | no | control thread only |
| Any SushiLog call | no | `format_text` builds `std::string` values (`SushiLog:modules/core/text/include/SushiLog/text/text_format.hpp:129-131`), `emit_log_record` takes the writer's mutex and notifies a condition variable (`SushiLog:modules/foundation/log/source/log_writer.cpp:59-63`), the queue is a `std::deque` (`log_queue.hpp:44,86`) |

The existing `AllocationTrap` counts the calling thread's allocations only
(`modules/foundation/allocation_trap/README.md`), so the consumer thread's work does not show in a
trap test that runs with a session open. That makes such a test possible.

### 2.7 Overhead

| Case | Cost | Source |
| --- | --- | --- |
| Zone, gate closed | 1.3 ns | `SushiProfiler:docs/reference/CHANGELOG.md:35` |
| Zone, recording | 13.3 ns | same |
| Compiled out | none | `SushiProfiler:modules/capture/zones/README.md`, "Macros" |
| Node zones and marks, ten nodes plus five per track, eight tracks | under 2 µs a block, 0.04 % of 5.33 ms | computed from the two figures above |
| One zone per stage inside the oversampled loop | 381 µs a block on a JCM800 | computed, section 2.2 |
| `SD_ENABLE_PROFILER` off | the library builds without SushiProfiler (`SushiProfiler:docs/design/PROFILER/CLIENTS.md`) | |

The profiler's own cost lands in every record that has a baseline (`overhead` analysis), so a
profiling run and an unprofiled run are compared, not assumed equal.

## 3. Logging: places that fail without a record

Four categories exist: `host_gui`, `capture`, `room`, `golden_compare`. There are 21 call sites in
six files (`git grep SD_LOG_`). The modules `core`, `device`, `io`, `circuits`, `math`,
`tubescreamer`, the catalog, `rig_preset` and the plug-in log nothing. `tools/logs/check_logging.py`
looks at an empty `catch {}` only (`:31,128-133`) and at the folders `modules`, `source` and `cli`
(`:27`), so a `catch (...)` that returns a fallback and everything under `applications/` pass.

The units `device`, `clap_boundary`, `clap_plugin`, `startup_rig` and `session_renderer` have no test
file (`tools/testing/untested_units.txt:11,35-42`), so a change there carries its first test.

Levels follow the `logging` skill: a fallback or a refusal is `WARN`, a failed operation whose
caller sees a status is `ERROR`, a lifecycle event is `INFO`. The audio-thread column says whether
the place runs on the audio thread: such a place gets a counter and a control-thread report, never
a call.

### Class A: the audio thread, silent today

| Id | Place | What happens | Record | Audio |
| --- | --- | --- | --- | --- |
| A1 | `sdl2_audio_device.cpp:136-140` | no callback installed: silence | counter `device.no_callback`, report `WARN device` | yes |
| A2 | `sdl2_audio_device.cpp:150-154` | block longer than the scratch: silence | counter `device.oversize_block` with the largest frames seen | yes |
| A3 | `asio_audio_device.cpp:122-126` | `active_instance_` null: the buffer is left unwritten | counter | yes |
| A4 | `asio_audio_device.cpp:421-434` | no callback: silence | counter | yes |
| A5 | `asio_audio_device.cpp:139,147-150` | `kAsioOverload`, resync, latency change acknowledged and dropped | counter `device.overload`; thread is the driver's, not the audio thread's; `WARN` through the reporter | driver thread |
| A6 | `applications/host_gui/main.cpp:396-398` | `frames > rig_input.size()`: callback returns, outputs untouched | counter | yes |
| A7 | `session_renderer.cpp:53` | `frames = min(frames, max_block_)`: the tail is not rendered or written | counter with frames asked and rendered | yes |
| A8 | `modules/foundation/core/source/rig_graph.cpp:412-419` | a parameter change for a missing node is dropped | counter | yes |
| A9 | `rig_graph.cpp:443-446,503-506,520-523` | pool exhaustion is an `assert`; `BufferPool::acquire` returns null for `frames > block_size_` (`buffer_pool.cpp:41-43`), and `process` never checks `context.frames` against the block size, so a release build passes null buffers to a node | behaviour is Q14; counter once decided | yes |
| A10 | `rig_adapter.cpp:297-301` | channel or frame mismatch: outputs cleared | counter | yes |
| A11 | `rig_adapter.cpp:274-283` | a full pending queue drops a parameter change; counted, no reader | report the count | yes |
| A12 | `rig_adapter.cpp:415-418`, `:421-424` | a routing published for another graph, or an unknown id: the block's parameter changes are dropped | counters | yes |
| A13 | `clap_plugin.cpp:247,255` | `CLAP_PROCESS_ERROR` returned with no record | counter | yes |
| A14 | `modules/nodes/capture/source/track_player.cpp:343-346` | empty arguments: returns without writing | counter | yes |
| A15 | `track_player.cpp:365-368` | a block larger than the prepared size, or speed 0: the speed is forced to 1 | counter | yes |
| A16 | `modules/nodes/room/source/room_node.cpp:141-145` | no render set published yet: silence | counter; the cause is C1 | yes |
| A17 | `track_recorder.cpp:191`, `recording_bus.cpp:88` | block refused, counted | report the count at `WARN` | yes |
| A18 | `dk_circuit.hpp:285,326-332,347-360` | non-converged solve keeps stale currents; state clamped to `state_limit_`; non-finite sample replaced from a backup | counters per block (section 2.3) | yes |
| A19 | `output_safety_clamp.hpp:45` | clip to ±1 with no count; NaN passes | counter of clipped samples and non-finite samples per block | yes |
| A20 | `tube_screamer.cpp:97-101` | after a failed DK prepare the node copies input to output | follows C6 | yes |

### Class B: control thread, error swallowed or fallback taken

| Id | Place | What happens | Level, category |
| --- | --- | --- | --- |
| B1 | `applications/plugin/clap_boundary.hpp:38-48` | `catch (...)` returns the fallback; every CLAP control callback uses it (`clap_plugin.cpp:83,97,107,151`, `clap_extensions.cpp:93-267`) | `ERROR plugin`, naming the callback |
| B2 | `clap_plugin.cpp:202-207` | a refused startup rig is discarded (`static_cast<void>`) | `WARN plugin`, with the reason |
| B3 | `applications/plugin/startup_rig.cpp:43-47` | the variable is set and the file does not open: no rig, no word | `WARN plugin`, with the path |
| B4 | `clap_plugin.cpp:214-217` | `prepare` fails: `activate` returns false, the reason in `error` is dropped | `ERROR plugin` |
| B5 | `clap_plugin.cpp:274-277` | `load_state` fails: the reason in `error` is dropped | `ERROR plugin` |
| B6 | `rig_adapter.cpp:166,189,200,218` | four catches copy the message into `error_out` | covered by B4 and B5 if logged once at the plug-in boundary |
| B7 | `room_node.cpp:129-130`, `:338-339` | `publish(reason)` result and reason dropped in `prepare()` and on a delay change; a failed first publish leaves the node silent forever (A16) | `ERROR room` |
| B8 | `room_node.cpp:457-458` | `publish(reason)` dropped when the HRTF set is emptied | `WARN room` |
| B9 | `room_node.cpp:462-464` | `set_hrtf` returns false for a sample-rate mismatch with no record | `WARN room`, with both rates |
| B10 | `room_node.cpp:489-494` | publish failure after a new HRTF set: previous set restored, reason dropped | `WARN room` |
| B11 | `room_node.cpp:546-549` | the one `catch` of `publish` keeps the message in `reason_out` and nothing else | logged by B7 and B10 |
| B12 | `applications/host_gui/slot_graph.cpp:681-703` | `host->set_hrtf(hrtf_)` result dropped (`:691`); a stored arrangement the fresh node refuses is replaced by the node's (`:695,703`), with the reason in a local string never read | `WARN host_gui` |
| B13 | `slot_graph.cpp:707-739` | a failed HRTF load resets every live room to the spherical-head fallback (`:713-722`) and returns the message | `WARN host_gui`: what changed and to what |
| B14 | `slot_graph.cpp:520-527` | `publish_room(slot, false, reason)` per frame in `reclaim()`, result and reason dropped | `WARN host_gui`, rate limited (SLG-3) |
| B15 | `slot_graph.cpp:453-471` | `set_cabinet` returns false for an unknown slot, a node that is not a cabinet host, or a refusal | `WARN host_gui` |
| B16 | `modules/nodes/cabinet/source/cabinet_node.cpp:93-100` | `set_cabinet` returns false for three causes (empty IR, node not prepared, rate mismatch); `stage_panel.cpp:97-99` writes a guess for the caller; `host_asio/main.cpp:204` names two of the three | `WARN cabinet`, one message per cause (Q12) |
| B17 | `modules/foundation/circuits/source/fmv_tone_stack.cpp:107-109`, `power_amp_feedback.cpp:118-120` | `assert(prepared)` and `static_cast<void>(prepared)`: a release build runs an unprepared circuit | `ERROR circuits` (Q13) |
| B18 | `tube_screamer.cpp:65-66` | `circuits_ready_` takes the prepare result; a failure makes the node a pass-through (A20) | `ERROR tubescreamer` |
| B19 | `dk_block_node.hpp:60` | `prepared_` stored, never read; `process()` assumes it | `ERROR` at the same site as B18 if kept |
| B20 | `asio_audio_device.cpp:243-247` | an unsupported block size silently becomes the driver's preferred size | `WARN device`, both sizes |
| B21 | `asio_audio_device.cpp:250-256` | a sample rate the driver cannot take falls back to the driver's own | `WARN device`, both rates |
| B22 | `asio_audio_device.cpp:327,355-362,397`; `sdl2_audio_device.cpp:104,109` | the return values of `ASIOOutputReady`, `ASIOStop`, `SDL_PauseAudioDevice` are discarded | `WARN device` |
| B23 | `asio_audio_device.cpp:128-131`, `:153-154` | a driver clock change is not followed; a reset request answers 0 | `WARN device` (Q11) |
| B24 | `applications/host_gui/main.cpp:141-150`, `:178-181` | a missing HRTF set and a bundled impulse response that fails to load are fallbacks written with `fprintf`; the README says they are command output (`applications/host_gui/README.md:126-127`) but they are not an answer to a command | `WARN host_gui` |
| B25 | `applications/host_gui/main.cpp:442-444` | `connect_error = error.what()` for a device that failed to open | `ERROR host_gui` |
| B26 | `modules/foundation/io/source/ir_loader.cpp:141-143` | an impulse response is resampled when its rate differs | `INFO io`, both rates |
| B27 | `ir_loader.cpp:145-150` | an impulse response longer than `max_length_ms` is cut | `WARN io`, both lengths |
| B28 | `applications/host_gui/session_timeline.cpp:114-119`, `modules/nodes/capture/source/take.cpp:48-51` | a take's own sample rate is read and kept, then ignored: KI-25, a 44.1 kHz WAV plays 8.8 % slow (`docs/reference/KNOWN_ISSUES.md:91-98`) | `WARN capture` at placement, naming the rates |

### Class C: statuses created at a boundary and shown to a person, never recorded

`logging` asks for a record where a failure status is made. These make one and hand it to the
interface as a string. Which is the place that logs depends on the error model the owner has not
chosen (`docs/design/REMAINING_WORK.md:26-38`, RW-3); the plan logs at the creation site and
changes no signature (Q20).

| Id | Place | Note |
| --- | --- | --- |
| C1 | `session_timeline.cpp:67-79` (`load_take`), `:174-190`, `:579-595` | file and timeline failures into `error_out` |
| C2 | `applications/host_gui/session_bounce.cpp:245-248` | a failed bounce returns the message |
| C3 | `applications/host_gui/session_file.cpp:160-175,190-200` | preset load and save |
| C4 | `applications/host_gui/audio_import_queue.cpp:98-101` | a worker-thread decode failure stored in the result; the frame loop shows it (`main.cpp:499-501`) |
| C5 | `applications/host_gui/ui/rig_chain_view.cpp:80-95`, `ui/cabinet_section_pane.cpp:73-84`, `ui/arrangement_view.cpp:280-312`, `ui/stage_panel.cpp:95-110` | UI edits that catch and show |
| C6 | `modules/nodes/room/source/hrtf_set_loader.cpp:238-241`, the `catch` that keeps the message | file read status |
| C7 | `slot_graph.cpp:411-424` | `set_parameter` returns silently for an unknown slot or parameter id; the value is stored first, so the model and the node differ |
| C8 | `modules/foundation/core/include/SushiDSP/foundation/core/parameter_table.hpp:72-81,85-91` | an unknown id or an out-of-range index is a no-op by contract (`audio_node.hpp`, `set_parameter_for_control_thread`). The by-id write is control thread and can log; the by-index write is the audio thread and can only count |

### Class D: a limit reached, no record

| Id | Place | What is cut |
| --- | --- | --- |
| D1 | `modules/nodes/room/source/image_source_solver.cpp:393-396` | early taps past `K_MAX_EARLY_TAPS`: "the latest arrivals are the ones left out" |
| D2 | `modules/nodes/room/source/modal_solver.cpp:363-373` | modes past `K_MAX_MODES`: the crossover moves to the first mode left out |
| D3 | `modules/nodes/room/source/early_reflection_renderer.cpp:64-66,94-96` | `min(set.count, K_MAX_EARLY_TAPS)`; a tap past the cap is ignored |
| D4 | `modules/nodes/room/source/band_response_fit.cpp:442,475-478` | the fit stops at 8 steps (section 2.3) |
| D5 | `rig_graph.cpp:472` | edges past a consumer's last slot are folded into it |
| D6 | `rig_persistence.cpp:1027-1029` | `track_count` clamped to `K_MAX_TRACKS` |

D1, D2 and D4 run when a room is published: `WARN room` once per publish, with the count asked
and the count kept. They are control thread.

### Class E: defaults taken from a file

| Id | Place | What is defaulted |
| --- | --- | --- |
| E1 | `applications/host_gui/rig_persistence.cpp:227-231` | a track row with a missing or large `index` is skipped |
| E2 | `rig_persistence.cpp:236-262` | a missing `name`, `gain`, `pan`, `muted`, `soloed`, clip fields take defaults; a clip with an empty path is dropped (`:256-259`) |
| E3 | `rig_persistence.cpp:311,477,521-527,749,821-858,1164-1180` | room, shape and tempo fields take defaults |
| E4 | `rig_persistence.cpp:1033` | a file with no `format` is read as format 1 |

One `INFO` or `WARN host_gui` per load naming how many rows or keys took a default is enough; a
line per key would flood a large session.

### Classes that already hold

`track_recorder.cpp:90-146,289-303` logs every failure with what the program does next;
`topology_applier.cpp:894-925,989-995,1045-1058` logs rollback failures and the room migration;
`slot_graph.cpp:493-510` logs a refused cabinet; `room_node.cpp:475-482` logs a refused decoder.
Their shape is the model for the rest. The same two-catch block (`std::exception`, then `...`)
appears four times (`track_recorder.cpp:90-103,132-146,289-303`, `topology_applier.cpp:911-925`).

## 4. SushiLog gaps

| Id | Lacks | Needed by | Evidence |
| --- | --- | --- | --- |
| SLG-1 | A real-time-safe path: a record built without heap, lock or system call. A producer ring of fixed-size records (a static format id and a few numeric arguments) formatted on the logger thread | A1 to A20, section 3 class A | `log_front.hpp:93-97` formats into `std::string`; `log_writer.cpp:59-63` locks and notifies; `log/README.md:42-44` tells the audio thread to count instead |
| SLG-2 | A counter object and an interval report: a relaxed atomic increment a real-time thread may call, a registry per category, and one line per interval naming each counter that moved (`WARN device: 3 oversize blocks in 5 s, largest 1024 frames`) | A1 to A19; `dropped_blocks` and `dropped_parameter_changes` already exist and have no reporter (`stage_panel.cpp:277`, `transport_bar.cpp:429`, `rig_adapter.cpp:269-272`) | the `logging` skill: "Anything per frame: a counter, reported once per interval"; SushiLog offers no counter |
| SLG-3 | A rate limit or log-once per call site | B14 (per frame in `reclaim()`), C7 (per parameter drag event), B12; and today `ui/directory_listing.cpp:131` logs a `WARN` from `list_directory`, which `ui/file_browser.cpp:144` calls on every frame the widget draws, so an unreadable folder writes about 60 records a second until the 4096-record queue drops them (`log_queue.hpp:39`) | the call sites named |
| SLG-4 | A shared time base with SushiProfiler, so a record can be placed on a trace | any `WARN` in the block path next to a dropout in a record | `LogRecord::time_ns` is system-clock time (`log_record.hpp:49`); the profiler stamps with its own clock and calibrates to the steady clock (`SushiProfiler:docs/design/PROFILER/CAPTURE_AND_SESSION.md`, "The hot path") |
| SLG-5 | A helper that logs the exception in flight, type and message, from a `catch (...)`: `log_current_exception(category, level, context)` | the four duplicated two-catch blocks (section 3, last paragraph); B1, B2, B4, B5 | `track_recorder.cpp:90-103,132-146,289-303`, `topology_applier.cpp:911-925` |
| SLG-6 | A way to restore the previous sink: `set_log_sink` returns `void` (`log_calls.hpp:69`), so `LogCapture` restores stderr and "a test holds one capture at a time" (`modules/foundation/log_capture/README.md:16-17`) | tests that capture while a host sink is set; `applications/plugin/log_join.cpp` sets one | `log_capture.hpp:48` |
| SLG-7 | Structured fields beside the message: a numeric key and value list, or an event id. `LogRecord` holds `message` and `text` only (`log_record.hpp:50-51`) | I found no SushiDSP place that needs it for item 2. The nearest is B16, where a cause code would let the GUI show the reason without parsing; that is a return-type matter, not the logger's. Listed because the owner named it | |
| SLG-8 | The count of dropped records as a value a program reads. SushiLog writes a `WARN` line (`log_queue.cpp:83-89`) and has no query | a test that asserts nothing was lost; low evidence | |

## 5. SushiProfiler gaps

| Id | Lacks | Place | Evidence |
| --- | --- | --- | --- |
| SPG-1 | Registering a site before the audio thread uses it. The first record of each site locks and allocates, and a site registers only while the gate is open | every zone in the block path; a live host that opens a session mid-run takes the lock on the audio thread. The benchmark and tests cover it with their warm-up periods | `site_registry.hpp:91-96,125-126`; `admission.hpp:44-70`; `CHANGELOG.md:35` |
| SPG-2 | A period measures mark to next mark. For a device callback that is the arrival interval, about the budget whatever the load, so the `budget` analysis reads about 1.0 and counts overruns of the device clock. Load needs the time the callback was busy: root-zone time of the period, or a period end mark | the block timeline in the four callbacks; the offline benchmark is unaffected, its periods are back to back | `budget_analysis.cpp:88-89`; `period_timeline.hpp:63-67` |
| SPG-3 | No aggregated zone: a site that sums ticks and call count over a period and writes one event at the period's end. Per-stage zones in the oversampled loop write 28,672 events a block | `jcm800.cpp:323-344` and the other three amplifiers; `tube_screamer.cpp:125-135` | section 2.2. An aggregated zone fixes the volume, not the two clock reads |
| SPG-4 | A counter or value named at run time, so one site can carry an instance name. `SP_ZONE_NAMED_AT` exists (`macros.hpp:68`); `SP_COUNTER_AT` takes a literal (`:83`) | three JCM800 instances and eight track rigs share one code site | `macros.hpp:68,83` |
| SPG-5 | A live read of the last closed period's statistics. Analyses read a record and a session returns the record at `stop` | a CPU-load meter in `host_gui` beside `ui::LevelProbe`; `transport_bar.cpp:429` shows a count today | `SushiProfiler:modules/session/session/README.md`, "Stop" |
| SPG-6 | More than one session in a process. A second `start` is refused, and each image that links the static library holds its own collector | two instances of the CLAP plug-in in one host process share the plug-in image's collector | `session/README.md`, "Start"; `collector/README.md` |
| SPG-7 | Check statistics and conditions. `Statistic` has median, p50, p90, p95, p99, min, max (`check.hpp:40-49`): no count of periods over budget, no mean or sum. A check cannot compare with a stored baseline, and cannot be skipped on a build fact | the dropout check; the benchmark gate on a dedicated machine against its baseline (`performance`); the optimised-build gate | `check.hpp:40-49`; `checks/README.md` |
| SPG-8 | A device-time external period: `record_zone_at` takes the caller's steady-clock times (`records.hpp`), but the ASIO driver's own time info is declined (`asio_audio_device.cpp:157-158`). No profiler gap, a device option, listed so Q11 sees both sides | `buffer_switch_time_info` | `zones/README.md`, `records.hpp` |

## 6. How the profiler lands, in one view

- A `foundation/profile` front (Q3), beside `foundation/log`, holds `SD_ZONE`, `SD_COUNTER`,
  `SD_INSTANT` over the `_AT` macros and a compiled level, plus the names of the timelines, groups
  and counters as constants, the contract between SushiDSP and its checks files.
- The block is marked where Q4 says. Zones go at track, node and region level; Q5 decides inside
  the oversampled loop.
- A counter flush at the end of each block moves the solver and drop counters (Q6, Q7).
- The harness runs a `Session` over a `bench.block` timeline; its budgets become checks files
  (Q8, Q9).
- Control-thread logging needs no new SushiLog feature and goes first. Real-time reporting waits
  on SLG-2 or Q6.

## 7. Decisions for the owner

Each is numbered; the options are not ranked. `PLAN.md` names the tasks each one blocks.

**Q1. Dependency scope.** SushiDSP depends on SushiLog alone (`cli/sushistack.deps.toml:25`).
(a) Library modules link SushiProfiler, with zones in `core`, `device` and the nodes.
(b) Only the hosts, the plug-in, the benchmark and the test programs link it; zones sit at those
boundaries and the libraries carry none, so node and region zones are not possible.
(c) `core` declares a small zone interface and a program binds it to SushiProfiler, so the
libraries do not depend on it.

**Q2. The switch and its default.** The design makes SushiProfiler an optional dependency, on by
default (`SushiProfiler:docs/design/PROFILER/README.md`, calls the orchestrator made). Options:
`SD_ENABLE_PROFILER` on in every product; on in every product except the CLAP plug-in, which ships
into other people's hosts; off in release packages.

**Q3. Where the front lives.** A new module `modules/foundation/profile`, shaped like
`foundation/log`; inside `foundation/core`; inside `benchmark_harness`, which is internal and no
product links (`modules/foundation/benchmark_harness/README.md`), so only the bench could use it.

**Q4. Where the block is marked.** (a) In the two device callbacks, one place for the three hosts,
with conversion in the block and the plug-in left out. (b) In each of the four program callbacks.
(c) In `RigGraph::process` and `SessionRenderer::render`, so the block excludes device conversion.
(d) A mix: device layer for arrival and jitter, a zone in the renderer for load. Also the timeline
names; the plan assumes `dsp.block` for arrival and `dsp.process` for the zone.

**Q5. Per-stage cost inside the oversampled loop.** (a) Node and region zones only; per-stage cost
comes from the isolated stage benchmarks that exist (`benchmark_triode_stage.cpp`,
`benchmark_tube_screamer_circuits.cpp`). (b) Restructure each amplifier's loop stage-major, one
pass of the whole oversampled block per stage, so a stage zone is one per block; the cache and
scratch-buffer cost is measured first. (c) A level-3 compiled zone per stage, used in a profiling
build only, with its distortion stated in the record; shares are read as relative.

**Q6. Home of the real-time failure counters.** (a) A counter facility in SushiLog (SLG-2), logged
at an interval. (b) Atomics owned by SushiDSP in `core`, read and logged by the hosts on the
control thread. (c) SushiProfiler counters only: visible in a profiling session, absent from an
ordinary run. Also whether `DkCircuit`'s plain integer counters become atomics or are flushed per
block on the audio thread (section 2.3), and whether `apply_output_safety_clamp` returns the
number of samples it clipped or found non-finite, a change to a public inline function in
`circuits` that four amplifiers call.

**Q7. Interfaces for solvers that return nothing.** `NewtonRaphsonSolver::solve_joint` returns a
`float`; `BandResponseFit::Result` holds no step count. (a) Add an optional report parameter or
overload to each. (b) Give the solver a per-instance statistics member as `DampedNewtonSolver`
has, which needs the static function to become an object. (c) Leave both uncounted and count
`DkCircuit` only.

**Q8. Where timing verdicts run.** (a) The benchmark program prints verdicts from checks files on
a dedicated machine; no test fails on time. (b) Time checks are `Large_` tests with the
optimised-build gate, as the three hand-timed tests are today. (c) Tests assert deterministic
counters only (steps, taps, non-converged solves) and time stays print-only. Option (b) conflicts
with `testing` rule 4 and `performance`; (a) and (c) do not.

**Q9. The known over-budget rows.** When a budget becomes a verdict, these fail on day one:
cabinet section 3.562 ms against 2.67 ms, Mark IIC+ 1.852 ms against 1.5 ms, the 8x DK block 0.963
ms against 0.12 ms, the 16x 5150 and IIC+ at 99 to 102 % of the block, and the unjudged DD-3. Which
are expected failures with an entry in `KNOWN_ISSUES.md`, which budgets move, which stay red.

**Q10. What a dropout is.** The signals in section 2.4. Which ones are counted as dropouts, which
as warnings, and what an arrival gap threshold is (one block, one and a half, two).

**Q11. ASIO driver messages.** Today they are acknowledged and ignored. (a) Count and log only.
(b) Extend `AudioDevice` with an event callback so a host can react to a reset request or a rate
change, a public interface. (c) Leave them, as `modules/foundation/device/README.md` says the
device never rebuilds. Related: use the driver's time info for the arrival timeline (SPG-8).

**Q12. The reason for a refused cabinet or HRTF set.** `CabinetHost::set_cabinet` and
`RoomHost::set_hrtf` return a bare `bool` for several causes. (a) Add a reason out-parameter or an
overload to the two interfaces. (b) Log inside the node at each refusal and leave the signatures.

**Q13. A circuit that fails to prepare.** `assert(prepared)` in two classes, a silent pass-through
in the tube screamer. (a) Throw from `prepare()`, as `RigGraph::compile` does. (b) Return a status
and refuse the node. (c) Log `ERROR` and keep the pass-through as a stated state.

**Q14. `RigGraph::process` with a block larger than it was built for.** Release builds have no
check (section 3, A9). (a) Refuse the block: clear the outputs and count. (b) Process in pieces of
`block_size`. (c) Leave it to each host's guard, which is silent today.

**Q15. Where the plug-in's records go when the host never calls `sushidsp_join_log`.** The plug-in
links the static logger (`modules/foundation/log/README.md`, "Plug-ins"), so its records reach a
stderr a DAW may not show. (a) A file under the user's temp folder. (b) stderr only. (c) A level
set by an environment variable and a file only then.

**Q16. A CPU-load meter in the GUI.** In or out of this work. If in: a product atomic the callback
writes and the GUI reads, or a live read from SushiProfiler (SPG-5).

**Q17. The wall-clock rate-limit test.** (a) Inject a clock into `SlotGraph`, a constructor change
that reaches `RigController`. (b) Leave the test. (c) Quarantine it under `KNOWN_ISSUES.md`.

**Q18. The design record.** `docs/design/SUSHIDSP/README.md` has no phase for this. (a) Add a phase
row to it, so this folder can say `**Implements:**`. (b) Leave the work under `**Designs:**` (ruled (a); the phase is DP).

**Q19. When a gap in another repository blocks a task.** SLG-1 to SLG-8 are the owner's to build in
SushiLog; SPG-1 to SPG-8 are SushiProfiler's. For each, SushiDSP can wait, use the workaround
`PLAN.md` names, or drop the feature. The plan marks which tasks wait and which carry a
workaround.

**Q20. Where a status is logged while RW-3 is open.** (a) At the creation site, with no signature
change, so a later error model removes one log call. (b) Wait for RW-3 and log nothing in class C
until then.

**Q21. The harness interface.** 17 benchmark files call `bench::measure`, `measure_node`, `print` and
`print_solver_counters`. (a) Keep those signatures and `Result`, and run the `Session` inside
`measure`, so the files change only where they pass a budget. (b) The harness hands out the
`Session` and the record, and each benchmark file reads them and calls `check`. (c) Both: (a) on
top of (b). Under Q1(b) the harness is how the benchmark program reaches SushiProfiler.

## 8. Not covered

- I read in full: the device sources, `RigGraph::process`, `SessionRenderer`, the plug-in's adapter
  and extension boundary, the benchmark harness and its room, JCM800 and DK cases, and the cited
  parts of the room, capture, io, circuits and `host_gui` sources. I scanned `host_gui/ui/` by its
  `catch` blocks only; its 20-odd widget files may hold more.
- Not read for silent paths: `modules/foundation/math` outside the DK and Newton solvers,
  `dd3`, `microphone`, `routing`, `catalog`, `spice_reference`, `signal_measure`, `rig_preset`
  beyond its throws, and the second half of `rig_persistence.cpp`.
- The loop shape of `peavey5150`, `mesa_mark2c` and `diezel_vh4` is inferred from the JCM800's and
  their identical clamp line; the 28,672 figure is the JCM800's.
- SushiLog was read at its checkout, not at the `v0.2.2` SushiDSP pins; SushiProfiler at its HEAD,
  tag `v0.1.0`. I did not diff `v0.2.2` against `v0.2.3`.
- No build, test or benchmark ran, so no cost in this file is measured here.
- `docs/design/SUSHIDSP/README.md` and the manual pages the work will make false are listed in
  `PLAN.md`, not read for their other claims.

## 9. Decisions

The owner delegated every question of section 7 to the orchestrator on 2026-10-26, asking for the
engineering choice that holds as the stack grows, all code written first and every build at the end.

| # | Decision |
| --- | --- |
| Q1 | (a) The libraries link SushiProfiler, now a shared library (v0.2.0), with zones in `core`, `device` and the nodes |
| Q2 | `SD_ENABLE_PROFILER` on in every product except the CLAP plug-in, which ships into other people's hosts; the plug-in, when built with it, links `SushiProfiler::SushiProfilerStatic` |
| Q3 | A new module `modules/foundation/profile`, shaped like `foundation/log`, the only includer of SushiProfiler below the products |
| Q4 | (d) The device layer marks the `dsp.block` timeline on arrival (arrival and jitter); a `dsp.process` zone in `RigGraph::process` and `SessionRenderer::render` measures load |
| Q5 | (a) Node and region zones only; per-stage cost stays with the isolated stage benchmarks |
| Q6 | (b) Atomics owned by SushiDSP in `core`, read and logged by the hosts on the control thread; `DkCircuit`'s counters are flushed once per block; `apply_output_safety_clamp` returns the number of samples it clipped or found non-finite |
| Q7 | (a) An optional report parameter on `NewtonRaphsonSolver::solve_joint` and a step count in `BandResponseFit::Result` |
| Q8 | (c) Tests assert deterministic counters only; time is a benchmark verdict from a checks file, never a test failure |
| Q9 | Budgets stay as written; the rows over budget are listed in `KNOWN_ISSUES.md` with the measured value, and the benchmark reports them red |
| Q10 | A dropout is an arrival gap over 1.5 blocks, a callback whose processing passes its block period, or a driver-reported under- or overrun; a clipped or non-finite sample is a warning |
| Q11 | (a) Count and log only; the driver's time info is SushiProfiler RW-11's |
| Q12 | (a) `CabinetHost::set_cabinet` and `RoomHost::set_hrtf` return a status enum naming the reason in place of `bool` |
| Q13 | (a) `prepare()` throws, as `RigGraph::compile` does |
| Q14 | (b) `RigGraph::process` processes a larger block in pieces of `block_size` |
| Q15 | (c) Records go to stderr; an environment variable sets a level and a file, and only then is a file written |
| Q16 | Out of this work |
| Q17 | (a) `SlotGraph` takes an injected clock, so the rate-limit test reads no wall clock |
| Q18 | (a) A phase row in `docs/design/SUSHIDSP/README.md` |
| Q19 | Tasks that need a SushiLog feature use the local counter of Q6 now; nothing waits |
| Q20 | (a) A status is logged where it is created, with no signature change beyond Q12 |
| Q21 | (a) `bench::measure` keeps its signatures and runs the `Session` inside |
