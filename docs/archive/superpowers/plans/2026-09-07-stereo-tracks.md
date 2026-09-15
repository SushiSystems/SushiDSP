# Stereo rig, track recording and playback (DS) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Carry channels end to end through the rig graph, pan the output, record what you hear beside the dry guitar, and play takes back — layered or re-amped.

**Architecture:** One serial foundation task makes `RigGraph` route as many buffers per edge as the two nodes declare, with the spec's adaptation table. Then four independent bricks land in parallel: a pan node, a streaming WAV writer, the recorder pair, and the player with its transport. The host wires them last.

**Tech Stack:** C++17, gtest, `SushiDSP/core` (`PortLayout`, `BufferPool`, `SpscRing`, `RtSwapSlot`, `SmoothedParameter`), `SushiDSP/io/WavFile`, Dear ImGui in `apps/host_gui`, `sd build` / `sd test`.

**Spec:** `docs/superpowers/specs/2026-09-07-stereo-tracks-design.md`

## Global Constraints

- C++17; Allman braces; `snake_case` functions, `PascalCase` types, `kPascalCase` constants, trailing-underscore members; no abbreviations.
- Every `.hpp`/`.cpp`, tests included, opens with the 76-column license box copied verbatim from `include/SushiDSP/core/PortLayout.hpp`, only the filename line changed. `@brief` on every symbol. A comment states an invariant, never a reason; the reason lives in the spec and is cited by path.
- The core (`include/SushiDSP`, `src/`) depends on the C++17 standard library and SIMD intrinsics only. A product module depends only on `sushi_dsp`. Only `apps/host_gui` may gain a third-party dependency, and only with the maintainer's word.
- Anything reachable from `process()` or from a real-time `write()` is allocation-free, lock-free, syscall-free and throw-free, and carries an `operator new` trap case in `tests/functional/unit/test_rt_safety.cpp`.
- Parameter order in a `NodeDescriptor` is a contract: appended, never reordered.
- Subagents run no `sd`, `cmake`, `ninja` or `ctest` and edit no CMake file; the controller builds, tests and edits CMake, and never terminates a running `sushidsp_host_gui.exe`.
- Commit by path with a plain `-m` subject, never `--amend`; another session shares this checkout. Trailers on every commit.

---

## Wave 1 (one task, serial)

### Task 1: `RigGraph` routes the channels its nodes declare

**Files:** `include/SushiDSP/core/RigGraph.hpp`, `src/core/RigGraph.cpp`, `tests/functional/unit/test_rig_graph.cpp`.

**Interfaces produced:**

```cpp
/// @brief Copies or sums between a producer's channels and a consumer's, per the spec's table.
/// @return False when the pair has no rule, which compile() turns into a refusal.
static bool channels_adapt(std::uint32_t from_channels, std::uint32_t to_channels);

/// @brief Total single-channel buffers the compiled graph needs live at once.
std::uint32_t live_buffer_count() const;
```

`compile()` gains: a per-node cache of `ports()`, a check of every edge against the adaptation table that throws `std::runtime_error` naming both node handles and both counts when the pair has no rule, and the `live_buffer_count()` figure.

- [ ] **Step 1: Write the failing tests** in `test_rig_graph.cpp`, using the file's existing fake-node idiom (a node that declares its `PortLayout` and writes a known constant per channel):

```cpp
TEST(Unit_Dsp, RigGraphCopiesAMonoProducerIntoAStereoConsumer)
TEST(Unit_Dsp, RigGraphSumsAStereoProducerIntoAMonoConsumerAtHalfLevel)
TEST(Unit_Dsp, RigGraphCarriesTwoChannelsThroughAStereoChain)
TEST(Unit_Dsp, RigGraphWritesEveryOutputChannelTheHostAsksFor)   // mono sink, stereo host buffer
TEST(Unit_Dsp, RigGraphCompileRefusesAnEdgeWithNoAdaptationRule) // e.g. 3 into 2
TEST(Unit_Dsp, RigGraphSumsFanInPerChannel)
TEST(Unit_Dsp, RigGraphEmptyGraphZeroesEveryOutputChannel)
```

Each asserts values, not just survival: a mono producer at 0.5 must read 0.5 on both consumer channels; a stereo producer at (1.0, 0.0) must read 0.5 on a mono consumer.

- [ ] **Step 2: Implement.** `node_output_scratch_` becomes one `float*` per node per output channel, indexed `handle * max_output_channels_ + channel`, sized at `compile()`. The gather becomes a fixed-size `const float* gathered[kMaxFanInOut][kMaxChannels]` member (not a stack array — `kMaxChannels` is 2 today but the array is sized by a named constant), filled per edge with the adaptation rule applied into a scratch buffer only when the rule needs arithmetic. The final copy to `context.outputs` applies the same table between the sink's channel count and the host's. The empty-graph path zeroes every host channel.
- [ ] **Step 3: RT safety.** No allocation in `process()`; the adaptation's scratch buffers come from `buffer_pool_`, whose size `compile()` now derives from `live_buffer_count()`. Add `RigGraphStereoProcessPerformsNoHeapAllocation` to `test_rt_safety.cpp`.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Controller builds and runs the suite**; every existing test and every golden must be unchanged, because every node in the tree is still mono. **Step 6: Commit** `DS0: RigGraph routes as many channels as its nodes declare`.

---

## Wave 2 (four tasks, parallel, disjoint files)

### Task 2: `routing::PanNode`

**Files:** `modules/routing/include/SushiDSP/routing/PanNode.hpp`, `modules/routing/src/PanNode.cpp`, `tests/functional/unit/test_pan_node.cpp`. Controller adds the source to `modules/routing/CMakeLists.txt` and the test to `tests/CMakeLists.txt`.

`PortLayout{1, 2}`, descriptor id `sushidsp.pan`, name `Pan`, one parameter `pan` (Linear, −1 to 1, default 0). Constant power: `gain_left = cos((pan + 1) * pi / 4)`, `gain_right = sin((pan + 1) * pi / 4)`, so centre is −3.01 dB on both and the sum of squares is 1 at every position. `SmoothedParameter` for the knob, the JCM800's idiom for recomputing only when it moved. Tests: centre reads −3.01 dB on both within 0.01 dB; hard left reads unity on the left and at least 80 dB down on the right; the law's power stays within 0.01 dB across 21 positions; an `operator new` trap case.

### Task 3: `WavWriter`

**Files:** `include/SushiDSP/io/WavWriter.hpp`, `src/io/WavWriter.cpp`, `tests/functional/unit/test_wav_writer.cpp`. Controller adds the source to the root `CMakeLists.txt`.

The write half of `WavFile`. `WavWriter(path, sample_rate, channels, WavWriter::Format)` where the format is 16-bit PCM or 32-bit float; `append(const float* const* channels, frames)`; `close()` rewrites the RIFF and data sizes. Interleaves on write. Control-thread only: it allocates and touches the filesystem, and its `@brief` says so. Tests: a file written and read back through `WavFile::load_mono` matches the samples within the format's quantisation; a two-channel file downmixes to the mean; `close()` twice is safe; an unopened path throws.

### Task 4: `Take` and `TrackRecorder`

**Files:** `modules/capture/CMakeLists.txt` (controller writes it), `modules/capture/include/SushiDSP/capture/{Take,TrackRecorder,RecordingBus}.hpp`, `modules/capture/src/{Take,TrackRecorder,RecordingBus}.cpp`, `modules/capture/README.md`, `tests/functional/unit/test_track_recorder.cpp`.

- `Take`: channels, sample rate, frames, and the samples; `static Take load(path)` on the control thread through `WavFile`; `const float* channel(std::size_t) const` for the RT reader. No RT-side allocation: a `Take` is built, then published.
- `TrackRecorder(channels, sample_rate, ring_capacity_frames)`: `arm(path)` starts the writer thread; `write(const float* const* channels, frames)` is the RT path, wait-free, one `SpscRing<float>` per channel, returning false and incrementing `dropped_blocks()` when a ring is full; `stop()` joins and finalises. `frames_written()` for the UI.
- `RecordingBus(processed_channels, sample_rate)`: `arm(folder, take_name)` opens both recorders and records the block index they began at; `write_processed`/`write_dry` from the callback; `stop()`; `dropped_blocks()` summed.

Tests: a recorded ramp reads back sample for sample; a deliberately undersized ring drops blocks and counts them without blocking; `write` after `stop` is a no-op; the two files of one bus have equal frame counts; an `operator new` trap case around `write`.

### Task 5: `Transport` and `TrackPlayer`

**Files:** `modules/capture/include/SushiDSP/capture/{Transport,TrackPlayer}.hpp`, `modules/capture/src/{Transport,TrackPlayer}.cpp`, `tests/functional/unit/test_track_player.cpp`.

- `Transport`: `std::atomic<std::uint64_t> position_frames`, an atomic state (`Stopped`, `Playing`, `Recording`), an optional loop range, `advance(frames)` on the RT thread wrapping at the loop end, and `seek`, `play`, `stop`, `set_loop` from the control thread.
- `TrackPlayer`: `kMaxTracks = 8`; per track a `RtSwapSlot<Take>`, gain, pan, mute, solo; `set_take(index, std::unique_ptr<Take>)` on the control thread; `process(float* const* outputs, channels, frames, Transport&)` mixes the audible tracks at the transport's position with the same constant-power pan law as Task 2, allocation-free; `reclaim()` on the control thread.

Tests: one track at unity reproduces its take; a muted track contributes nothing; solo silences the others; two tracks sum; pan places a track by the same law as `PanNode` within 0.01 dB; playing past the end yields silence; a loop range wraps at the right sample; an `operator new` trap case around `process`.

---

## Wave 3 (one task, serial)

### Task 6: the host

**Files:** `apps/host_gui/RigController.{hpp,cpp}` (the rig's input source and the player's lifetime), `apps/host_gui/main.cpp` (two output channels, the callback's record and play calls), `apps/host_gui/ui/{TransportBar,TrackPanel}.{hpp,cpp}` (new), `apps/host_gui/ui/ChainStrip.cpp` if the transport shares its row, `tests/functional/unit/test_rig_controller.cpp`, `modules/node_registry/src/NodeRegistry.cpp` (the pan node), `apps/host_gui/ui/PanelLibrary.cpp` (the pan node's panel). Controller edits CMake.

The device request becomes two output channels. The callback records the processed output and the dry input when the bus is armed, and mixes the player into the output. The rig's input source is a three-way choice: live input, a named track, or both. The transport bar and the track panel follow the file's existing ImGui idiom. The preset gains the track list and the loop range.

---

## Wave 4 (one task, serial)

### Task 7: documentation

`docs/design/sushidsp.md` (a §12 for the studio path and the roadmap rows), `modules/capture/README.md`, `docs/README.md`, `docs/reference/CHANGELOG.md` (one line per task), the spec's status line, `tests/golden/README.md` if any golden moved.

## Wave summary

| Wave | Tasks | Waits on | Build the controller runs |
| --- | --- | --- | --- |
| 1 | 1 | none | `sd build && sd test`; every golden unchanged |
| 2 | 2, 3, 4, 5 | 1 | one build for all four |
| 3 | 6 | 2–5 | build, then the GUI opened by the maintainer |
| 4 | 7 | 6 | build |

## Self-review

- Spec §3 → Task 1; §4 → Tasks 2 and 6; §5 → Tasks 3 and 4; §6 → Task 5 and 6; §7 → Task 6; §10 → each task's tests plus the re-amp alignment case, which lands in Task 6 because it needs the host's input switch.
- Names across tasks: `channels_adapt`, `live_buffer_count`, `PanNode`, `WavWriter`, `Take`, `TrackRecorder`, `RecordingBus`, `Transport`, `TrackPlayer`; each defined where first produced.
- The re-amp alignment acceptance is the one criterion no single brick can prove; Task 6 owns it.
