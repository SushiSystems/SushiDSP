# AmpliTube 5 channels and cabinet section Implementation Plan

> **For agentic workers:** Execution method chosen by the owner: inline, with subagents for independent read-only checks. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reproduce AmpliTube 5's signal architecture in SushiDSP, phase by phase, as the spec's section 11 orders them.

**Architecture:** A stereo rig, output ports on `AudioNode`, channel strips with a bus, the Parallel and DI paths, and a cabinet section in which a room places cabinets and microphones. Acoustics are model-based.

**Tech Stack:** C++17 (foundation, nodes, host), CMake through `sd`, GoogleTest (`sushidsp_tests`), Dear ImGui in the GUI host.

**Spec:** [`SPEC.md`](SPEC.md) in this folder. This plan implements it, and the spec's section 11 is the phase list.

**Scope of this plan.** It gives full task detail for **P0** only. P1 to P6 are listed at phase level below. Each gets its own detailed section when it starts, because each depends on decisions the spec lists in section 12 and on code that P0 to P(n-1) change. Writing their steps now would guess at interfaces that do not exist yet.

**Record of order.** The P0 code change was made before this plan was written. The writing-plans skill asked for the plan first; that was a process miss. The task list below records what was done, what was checked and what is still open.

## Global Constraints

- The spec's section 9: a 256-frame block at 48 kHz is 5.33 ms; `AudioNode::process()` and everything it calls allocate nothing, lock nothing, make no system call and throw nothing.
- The spec's section 10: golden files for the existing mono products stay bit-identical through P0 to P3.
- `K_MAX_CHANNELS` stays 2 (spec section 6, item 5).
- `AudioNode` public change (ports) is P1 and needs its `api-stability` entry at that point.
- No `sd build` or `sd test` without the owner's permission (memory: ask before building; never kill the running GUI). Syntax checks through `clang++ -fsyntax-only` with the flags of `build/compile_commands.json` are allowed.
- Every change lands its module README, manual page, changelog line and backlog row in the same commit (`documentation`, `CONTRIBUTING.md`).
- Commit messages in English; `Co-Authored-By: Claude Haiku 5.5 <noreply@anthropic.com>` at the end.

## Review Focus

1. **A device with one output channel.** The rig stays one channel, as before. Pinned by `RigOutputAdaptsToDeviceCoversTheGraphsTable` (`tests/functional/unit/test_channel_selection.cpp`).
2. **A device with two output channels.** The rig renders two channels; a mono chain reaches both. Pinned by `RigGraphCopiesAMonoProducerIntoAStereoConsumer` (`tests/functional/unit/test_rig_graph.cpp`) and `WriteRigOutputPassesTwoRigChannelsStraightThrough`.
3. **A device with more than two, or zero, output channels.** Refused at setup with a message, as before. Pinned by `RigOutputAdaptsToDeviceRefusesWidthsWithNoRule`. The example's own refusal path has no unit test (see task 1).
4. **Left or Right output selection on a stereo rig.** The excluded device channel is silent. Pinned by `WriteRigOutputZeroesTheChannelTheSelectionExcludes`.
5. **The width choice is duplicated.** `applications/host_asio/main.cpp` and `applications/host_gui/main.cpp` each compute `min(max(1, outputs), K_MAX_CHANNELS)`. Nothing pins the two to each other. A shared rule would need a new public function, which is the owner's decision; see task 3.

## P0: Stereo output on the ASIO example

**Status:** code changed, checked by compiling; not built or run.

### Task 1: Render the ASIO example rig at the device's width

**Files:**
- Modify: `applications/host_asio/main.cpp`
- Modify: `applications/host_asio/README.md`

**Interfaces:**
- Consumes: `RigGraph::K_MAX_CHANNELS`, `rig_output_adapts_to_device`, `write_rig_output_to_device` (`modules/foundation/audio_format/include/SushiDSP/audio_format/channel_selection.hpp`).
- Produces: nothing for later tasks.

- [x] **Step 1: Choose the rig width from the device.** `rig_channels = min(max(1, actual.output_channels), RigGraph::K_MAX_CHANNELS)`, passed to `PrepareInfo`.
- [x] **Step 2: Allocate one block per rig channel off the audio thread.** `rig_output` plus two pointer arrays, one mutable and one const, filled once in `main()`.
- [x] **Step 3: Map the rig output onto the device.** The callback renders into `rig_output_pointers` and calls `write_rig_output_to_device` with `rig_channels`.
- [x] **Step 4: Update the README** to say the rig is as wide as the device and the buffer is `rig_output`.
- [x] **Step 5: Syntax-check the file.** Command and output below. No binary was built.
- [ ] **Step 6: Build and run the example on a stereo ASIO driver.** Needs the owner's permission and a device. Not done.

Syntax check, run from `build/` with the compile command recorded for `applications/host_asio/main.cpp` in `build/compile_commands.json`, with `-fsyntax-only -Wall -Wextra` added:

```
clang++ -I<the -I paths of that entry> -std=c++17 -Wall -Wextra -fsyntax-only D:/Projects/sushidsp/applications/host_asio/main.cpp
clang exit=0
```

### Task 2: Record the closure in the manual and the backlog

**Files:**
- Modify: `docs/reference/KNOWN_ISSUES.md` (the entry "The ASIO host still renders the rig in mono" is deleted; its own text said it closes by this change)
- Modify: `docs/design/REMAINING_WORK.md` (the stereo bullet is rewritten to name what is still open)
- Modify: `docs/reference/CHANGELOG.md` (one line under Unreleased)
- Modify: `docs/agent/2026_10_13_AMPLITUBE5_CHANNELS/SPEC.md` (P0 row and tests; stereo input moved to P3)

- [x] **Step 1: Delete the closed known issue.**
- [x] **Step 2: Rewrite the backlog bullet** so it names only the two entries still open.
- [x] **Step 3: Add the changelog line.**
- [x] **Step 4: Update the spec's P0 rows and decision table.**
- [ ] **Step 5: Run the three documentation checkers.** Run before the commit; see the result in the report.

### Task 3: Shared width rule (open, needs the owner's decision)

The width formula sits in two hosts. Moving it into one place would be a small public function (for example on `RigGraph`), a public interface addition. Not started. The owner decides the name and the home.

## P1 to P6: phase level

Each phase starts with its own detailed plan section, written against the code as it stands then.

- **P1 Output ports.** `AudioNode` declares named output ports; `RigGraph::connect` names the producer's port; `compile()` refuses an undeclared port. Public change: `api-stability` entry and changelog `!` marker. Blocked on nothing; starts after P0 is verified.

  **P1 status, 2026-10-13:** code in `audio_node.hpp`, `rig_graph.hpp` and `rig_graph.cpp`; tests in `tests/functional/unit/test_rig_graph_output_ports.cpp` (seven cases); core README section "Output ports"; changelog entry marked breaking. Syntax-checked with `clang++ -fsyntax-only` for the graph source, the new and existing graph tests, the ASIO example, the pan and cabinet and room node sources. **Not built and not run.** Build and test wait for the owner's go-ahead, as agreed.

  **P1 interface shape, decided 2026-10-13: shape A.** The owner asked for the most robust engineering choice. A keeps a single process entry point, so no node implements two paths and the graph calls one function. Its change to `ProcessContext` is additive and defaulted, so the 122 construction sites compile unchanged. The one cost is the invariant `port_outputs[0] == outputs`, which the graph alone sets. The layout change of `ProcessContext` and the new `connect` parameter are still breaking for the shared library, so the changelog entry names them. The two shapes as first written:
  - **A (recommended):** `AudioNode` gains `virtual std::uint32_t output_ports() const` (default 1) and `virtual std::uint32_t output_port_channels(std::uint32_t port) const` (default: `ports().output_channels`). `ProcessContext` gains a trailing member `float* const* const* port_outputs = nullptr`, where `port_outputs[0]` is `outputs`. Existing aggregate initialisers keep compiling. `RigGraph::connect(from, to, from_port = 0)` and an `Edge::from_port` field. `disconnect(from, to)` removes every edge between the pair.
  - **B:** keep `ProcessContext` unchanged and add a virtual `process_ports(PortContext&)` whose default forwards to `process()`. No change to the 122 construction sites, but a second process entry point that every multi-port node must implement.

  Both change `AudioNode` or `RigGraph` public surfaces. The choice of shape is the owner's; the plan above does not start until it is made.
- **P2 Channel strips and bus.** `ChannelStripNode` (level, pan, mute, solo, polarity, delay, width) and the bus. `MixerNode` narrowed to summing. Blocked on the spec's open item 5 (room flag location).

  **P2 detail, decided 2026-10-13.** Owner decision: the bus holds solo. The rest follows from that.
  - **Channel strip** (`sushidsp.channel_strip`, new): mono in, stereo out. Parameters `level` (Decibel, -60 to 12, default 0), `pan` (-1 to 1, constant power, as `PanNode`), `polarity` (Boolean), `delay` (ms, 0 to 50, default 0). Level and pan are smoothed over 20 ms. The delay line is sized in `prepare()` for 50 ms; a delay change takes effect at once, a click the DI delay accepts.
  - **Bus** (`sushidsp.bus`, new): eight stereo inputs, `PortLayout{16, 2}`, so `RigGraph` gives eight stereo slots. Parameters `mute_0`..`mute_7` and `solo_0`..`solo_7` (Boolean). Rule: a muted input is silent whatever its solo; when any input is soloed, only soloed inputs sound. Each input's gate is smoothed over 5 ms, so a toggle does not click.
  - **Not in P2.** Strip width moves to P4, where the room's stereo sum exists to need it. Group processing of the Bus stage (AmpliTube's middle Bus track inserts) is P6. `MixerNode` is unchanged, because the splitter topologies still use it.
  - **Spec deviation, recorded here:** the spec's section 5 lists mute, solo and width on the strip. Mute and solo move to the bus; width moves to P4.
  - **P2 status, 2026-10-13:** `SampleDelayLine`, `ChannelStripNode` and `BusNode` are written and registered in the Routing category; tests in `test_sample_delay_line.cpp`, `test_channel_strip_node.cpp` and `test_bus_node.cpp`; routing README and registry README updated; changelog line added. The bus test includes a `RigGraph` case that checks each strip reaches its own stereo slot. Syntax-checked with `clang++ -fsyntax-only`. **Not built and not run.** The P2 mixer panel, the last item of the spec's P2 acceptance, is not started: it belongs with the 3D scene work in P4, where the strips get their UI.
- **P3 Parallel and DI paths.** Stereo input pair through `ChannelSelection`; the Parallel path as two mono chains; the DI path beside the main chain. Blocked on the spec's open item 4 (Loop FX sends are out of P3 unless decided).
- **P4 Cabinet section.** `MicrophoneNode`, the room environment with sources and receivers, placement by node identity, the 3D scene. Blocked on the spec's open items 1 (benchmark threshold), 2 (tail sharing) and 3 (VIRTM view). Starts with the benchmark, not the code.
- **P5 Preset format.** Versioned JSON for placements, microphones and strips, with the migration in spec section 8. Blocked on P4.
- **P6 Mixer view and polish.** Blocked on P2 and P4.

## Open before P0 can close

- Build and run on a stereo ASIO driver (task 1, step 6). Requires the owner's permission and a device.
- The three documentation checkers (task 2, step 5).
- The stereo-branch-collapse and 2-into-1 entries remain open in KNOWN_ISSUES; this work does not close them.

## Status after the unattended run, 2026-10-13

Everything below is written and syntax-checked with `clang++ -fsyntax-only`. Nothing is built or run.

- **P0** stereo output on the ASIO example: done.
- **P1** output ports: done.
- **P2** channel strip, bus, delay line: done. Mixer panel: not done.
- **P3** Parallel chain and DI path: done in the host model, applier, input binding and tests. The DI path joins after the chain's last slot; it does not yet sit before the effects.
- **P4** room environment, microphone node and host wiring: done as code and tests. The benchmark gate is not run; it needs a build.
- **P5** preset format for the cabinet section: done (`room_layout_codec.cpp`, `rig_persistence.cpp`). The room's open switch and the chain topology are not saved yet.
- **P6** mixer view and plan view: done as a plan (top-down) with draggable markers, and a mixer panel. Not checked on screen.

## Build status, 2026-10-13

Built with `sd build` at 4 parallel jobs (`CMAKE_BUILD_PARALLEL_LEVEL=4`, RelWithDebInfo tree): exit 0, 120 steps, no error lines, no warnings in the files this work touched.

- `sd test -s unit`: 1175 tests, 1175 passed after one fix. The first run failed `RigGraphCompileRefusesAnEdgeWithNoAdaptationRule`: the new port check refused a 3-channel port before the adaptation check. The refusal is needed, since port buffers hold two channels. Fixed in `1a00bac`, which also renames the test to `RigGraphCompileRefusesAPortWiderThanTheRoute`.
- `sd test -s regression`: 23 of 23 passed.
- Not run: `sd test -s benchmark`. The P4 benchmark case (3 cabinets, 8 microphones) is not written, and the threshold is still open (section 12 of the spec).
- Not checked: the UI on screen; the mixer and plan view.
