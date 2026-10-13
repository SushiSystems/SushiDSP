# AmpliTube 5 channels and cabinet section Implementation Plan

> **For agentic workers:** The owner chose inline execution: one session implements the tasks in the order below. Subagents are used for read-only checks only. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reproduce AmpliTube 5's signal architecture in SushiDSP as [`SPEC.md`](SPEC.md) describes it.

**Tech stack:** C++17 (foundation, nodes, host), CMake through `sd`, GoogleTest (`sushidsp_tests`), Dear ImGui in the GUI host.

**State on 2026-10-13.** P0 to P3 are in the tree, built and tested. The cabinet section (P4 to P6) was written in an unattended run that departed from the spec, and it does not work: the room cannot be opened. This plan replaces that work. The owner delegated the engineering decisions on 2026-10-13; they are recorded in the spec's section 3.

## What stands

| Phase | Content | State |
| --- | --- | --- |
| P0 | Stereo output on the ASIO example | Built. Not run on a stereo ASIO driver. |
| P1 | Output ports on `AudioNode` and `RigGraph` | Built, unit tests pass. |
| P2 | `ChannelStripNode`, `BusNode`, `SampleDelayLine` | Built, unit tests pass. |
| P3 | Parallel chain, DI path, input binding | Built, unit tests pass. The DI path joins after the chain only. |

Last full run: `sd test -s unit` 1175 of 1175, `sd test -s regression` 23 of 23, on the RelWithDebInfo tree.

## What was wrong with the cabinet section

Each row is a departure from the spec that this plan removes.

| Spec | Tree |
| --- | --- |
| `RoomNode` is the environment (section 5) | `RoomNode` is unchanged. `RoomEnvironmentNode` wraps 27 whole `RoomNode` objects, about 83 MB, each with its own tail, modal bank, HRTF and decoder. |
| One tail per source, shared by its receivers (section 7) | One tail per pair. |
| One output port per receiver | Ten ports. `RigGraph::K_MAX_OUTPUT_PORTS` is 8, so `compile()` refuses the graph the moment the room opens. No test renders the environment through a graph. |
| Placement lives in the room, keyed by node identity (sections 3 and 6) | Placement lives in the host's `RoomLayout`, cabinets keyed by path index. `RoomHost` is untouched. |
| `MicrophoneNode` holds pattern, proximity, polarity and the room flag, in `modules/nodes/microphone` | Polarity and room flag only, inside the room module. |
| Mic 1..N strips, a Room strip, Bus, DI, Master (section 4) | Microphones have no strip. The room flag routes nothing. The master exists only while the DI path is on. |
| A room is always present; no Space mode (section 2) | A "Room open" switch, and the old room slot still sits in the chain: two rooms. |
| 8 microphones | 6 on the host. |
| A 3D scene | A top-down plan in a floating window drawn over the main layout. |
| The benchmark comes before the code (section 9) | No benchmark case. |
| Placement changes are atomic | `configure()` publishes pair by pair, so the audio thread can render a half-applied placement. |

## Global constraints

- A 256-frame block at 48 kHz is 5.33 ms. `AudioNode::process()` and everything it calls allocate nothing, lock nothing, make no system call and throw nothing.
- `K_MAX_CHANNELS` stays 2.
- New room classes are in `SushiDSP::Room`. Existing classes keep `SushiDSP::room` until the standards migration renames the module.
- One acceptance criterion and one build per task. No `sd build` or `sd test` without the owner's permission; the running GUI is never killed without asking. Between builds, C++ is syntax-checked with `clang++ -fsyntax-only` and the flags of `build/compile_commands.json`.
- Every task lands its module README, manual page, changelog line and backlog row in its own commit.
- The JCM800 tone anchor does not move. A rig with no microphone and a muted listener must render what it renders today.

## Design

### Signal flow in the host

```
path k:  amp -> effects -> cabinet k ----+--> room input k            (k = 0..2)
                                         |
                                         +--> chain join (as today) --> Cabinet

room:    port 0      listener, stereo --> Listener strip
         port 1 + m  microphone m     --> MicrophoneNode m --> Mic strip m

Mic strips, room flag clear --> Bus
Mic strips, room flag set   --> Room sum --> Room strip --> Bus

Cabinet + Bus + Listener (+ DI when it sits before the effects) --> Amp bus
Amp bus --> Post stage --> Master bus <-- DI (when it sits after the effects)
Master bus --> Master strip --> stereo output
```

- **Cabinet** is the chain's existing join, kept as it is: a splitter and mixer for the split chains, two panned strips and a bus for Parallel. The cabinet's impulse response is already a close-microphone capture, so this input is a real signal, and it keeps every existing rig sounding as it does. A new rig starts with Cabinet on, the listener muted and no microphone.
- **Bus** takes at most eight inputs. A flagged microphone goes to the Room sum, so the Room strip appears only when at least one microphone has left the Bus; eight inputs are never exceeded.
- **A path with no cabinet** has no source in the room. It reaches the Cabinet join only.
- **Source identity** is the cabinet slot's id. Microphone identity is the `MicrophoneNode` slot's id.

### Room engine

One `RoomNode`, `PortLayout{3, 2}`: three mono source inputs, port 0 the listener's stereo pair, ports 1 to 8 one mono microphone each. `RigGraph::K_MAX_OUTPUT_PORTS` becomes 9, and the fan-out limit applies per output port.

State is split by what it is a function of.

| Stage | Count | Owns | Built from |
| --- | --- | --- | --- |
| `Room::SourceStage` | 3 | Input crossover, one high-half ring, the tail's network (lines, absorption, Hadamard), the modal resonator bank | `CrossoverPair`, `DelayRing`, `LateReverb` core, `ModalBank` |
| `Room::PairStage` | up to 27 | Early taps and their filter state, the direct path, the tail's output filter (gain, tone, coherence, remainder delay), the modal read-out gains | `EarlyReflectionRenderer`, `DirectPath`, a new `Room::TailOutput` |
| `Room::EarsOutput` | 1 | The ambisonic sum of its pairs, the decoder, the diffuse FIR pair, the alignment delays | `BinauralDecoder`, `FirFilter`, `FrameDelay` |
| `Room::MicrophoneOutput` | 8 | The mono sum of its pairs, delayed by the ears' alignment so every port lines up | `FrameDelay` |
| `Room::HeadHolder` | 1 | The HRTF set and the decoder's swap slot | `HrtfSet`, `RtSwapSlot` |

- **The tail.** The network runs at unit input gain and emits its two raw sums. The pair's level moves to `TailOutput`; a scalar on a linear network commutes to the output. The network is fed at the smallest pre-delay among the source's receivers, and each pair delays its output by the remainder, so the one-pair case keeps today's timing.
- **The modal layer.** The resonator state depends on the source alone. One bank per source runs the resonators; each receiver reads it through its own gain row. The sum per pair is the same sum as today.
- **One render set.** `Room::EnvironmentRenderSet` carries every pair and is published through one `RtSwapSlot`, so a placement change reaches the audio thread whole. One fade window covers the set. A receiver's kind is fixed by its port, so the fade across listener kinds is removed.
- **Latency.** The node reports the ears' alignment delay. Microphone ports carry the same delay.
- **Parameters.** `direct_delay` stays. `listener` goes, because the port names the receiver. `mix` and `level` go, because the strips hold level and the room has no dry path. Breaking change, recorded under `api-stability`.

### Placement

`Room::Arrangement` is a value brick: the shell (dimensions, materials, furnishings, patches), the listener's pose, up to three sources `{id, pose}` and up to eight microphones `{id, pose, pattern, proximity}`. Its edits check the limits and return a reason on refusal. A source's index is its input slot; a microphone's index plus one is its port.

`RoomHost` gains `set_arrangement(arrangement, reason_out)` and `arrangement()`. `set_scene` stays for the bare room: it replaces the shell and the poses of source 0 and the listener.

`MicrophoneNode` is the home of pattern, proximity, polarity and the room flag. The host copies pattern and proximity into the arrangement when it applies an edit, because the room's solve needs them. The host's `RoomLayout` is deleted; the room slot's snapshot carries the arrangement.

### Screen

The cabinet section is a pane of the main layout, chosen by a workspace switch beside the mini-DAW toggle. It holds the 3D scene, an inspector for the selected item, and the mixer. The floating window, the plan view, the room switch and the room's entry in the gear browser are removed.

## Tasks

Hierarchy order. Each task names its files, its acceptance criterion and its build. "Room tests" means every `tests/functional/unit/test_room_*.cpp` and the room cases of `test_rt_safety.cpp`.

### A. Graph limits

- [ ] **A1. Nine output ports and a per-port fan-out limit.**
  Files: `modules/foundation/core/include/SushiDSP/core/rig_graph.hpp`, `modules/foundation/core/source/rig_graph.cpp`, `tests/functional/unit/test_rig_graph_output_ports.cpp`, `modules/foundation/core/README.md`.
  Acceptance: a node with nine ports, each feeding one consumer, compiles and renders each port's samples at its consumer; a tenth port and a ninth edge from one port are refused with a reason.
  Build: `sd build`, `sd test -s unit -f RigGraph`.

### B. Room engine

`RoomNode` keeps working after every task in this group, as one source and one receiver, so its existing tests guard each step.

- [ ] **B0. Pin today's output.**
  Files: `tests/functional/unit/test_room_reference.cpp`, `tests/golden/` (two new files), `tests/golden/README.md`, `tests/CMakeLists.txt`.
  Renders the starter scene through `RoomNode` for the ears and for the microphone, 2 s of a fixed input, and stores both as goldens.
  Acceptance: the test passes against the goldens it wrote, on two consecutive runs.
  Build: `sd build`, `sd test -s unit -f RoomReference`.

- [ ] **B1. Solvers take a source and a receiver.**
  Files: `image_source_solver.hpp/.cpp`, `modal_solver.hpp/.cpp`, `late_reverb_design.hpp/.cpp`, `fdn_design.hpp`, `modal_design.hpp`, `room_render_set.cpp`, new `room_placement.hpp` (`Room::Pose`, `Room::Source`, `Room::Receiver`), the three solver tests.
  `ImageSourceSolver::solve(shell, source, receiver)`. `ModalSolver` splits into the resonators with their source gains and a receiver gain row. `LateReverbDesign` splits into the shell's network and the pair's output. `build_render_set` calls the new entry points.
  Acceptance: B0 goldens bit-identical; room tests pass unchanged; new tests show each split equals the whole.
  Build: `sd build`, `sd test -s unit -f Room`.

- [ ] **B2. The tail's network and its output filter.**
  Files: `late_reverb.hpp/.cpp`, new `tail_output.hpp/.cpp`, `geometric_path.hpp/.cpp`, `test_late_reverb.cpp`, new `test_tail_output.cpp`, room `CMakeLists.txt`, room README.
  Acceptance: B0 goldens within 1e-6 relative; the goldens are regenerated once with a row in `tests/golden/README.md` naming the moved gain.
  Build: as B1.

- [ ] **B3. Modal bank with receiver read-out rows.**
  Files: `modal_bank.hpp/.cpp`, `modal_path.hpp/.cpp`, `test_modal_bank.cpp`.
  `ModalBank::process` takes the feed, N gain rows and N output pairs.
  Acceptance: B0 goldens unchanged; a two-row render equals two one-row renders sample for sample.
  Build: as B1.

- [ ] **B4. Early renderer reads a shared ring; direct path takes its filters.**
  Files: `early_reflection_renderer.hpp/.cpp`, `direct_path.hpp/.cpp`, `geometric_path.cpp`, their tests.
  The renderer's `render` reads a ring passed in, as `DirectPath` already does. `DirectPath::load_bank` takes the HRIR pair and the ITD, not a whole render set. The duplicate ring in `GeometricPath` goes.
  Acceptance: B0 goldens unchanged.
  Build: as B1.

- [ ] **B5a. `Room::SourceStage`.** Files: new `source_stage.hpp/.cpp`, `test_source_stage.cpp`. Acceptance: its high half, tail sums and modal read-out match what `GeometricPath` and `ModalPath` produce for the same input. Build: as B1.
- [ ] **B5b. `Room::PairStage`.** Files: new `pair_stage.hpp/.cpp`, `test_pair_stage.cpp`. Acceptance: one pair's ambisonic block, direct pair and tail pair match `GeometricPath`'s. Build: as B1.
- [ ] **B5c. `Room::EarsOutput`, `Room::MicrophoneOutput`, `Room::HeadHolder`.** Files: three new header and source pairs, their tests. Acceptance: fed one pair, each output matches `BinauralStage::join` for its kind. Build: as B1.
- [ ] **B5d. `RoomNode` on the stages.** Files: `room_node.hpp/.cpp`, `test_room_node.cpp`; deletes `geometric_path`, `modal_path`, `binaural_stage` and their tests' cases, which move to the stage tests. Acceptance: B0 goldens unchanged; `test_room_node.cpp` passes. Build: as B1.

- [ ] **B6. `Room::Arrangement` and the environment render set.**
  Files: new `room_arrangement.hpp/.cpp`, `environment_render_set.hpp/.cpp`, their tests; `room_render_set.hpp/.cpp` is folded in and deleted.
  Acceptance: a fourth source and a ninth microphone are refused with a reason and leave the arrangement unchanged; the set built for one source and one receiver holds the same coefficients as B1's set.
  Build: as B1.

- [ ] **B7. `RoomNode` becomes the environment.**
  Files: `room_node.hpp/.cpp`, `room_host.hpp`, `test_room_node.cpp`, new `test_room_environment.cpp`, `test_rt_safety.cpp`, `test_rig_controller.cpp` where it sets the removed parameters, room README, `docs/reference/CHANGELOG.md` (breaking), the manual pages `git grep` finds for `listener`, `mix` and `level`.
  Acceptance, the spec's P4 tests: one source and one receiver reproduce B0's goldens on port 0 and on port 1; two sources and three receivers equal the sum of the six one-pair renders; a moved microphone is heard at the next publish with no audio-thread allocation; a room with no source is silent on every port; the whole node renders through a `RigGraph`.
  Build: `sd build`, `sd test -s unit`, `sd test -s regression`.

- [ ] **B8. Delete `RoomEnvironmentNode`.**
  Files: `room_environment_node.hpp/.cpp`, `test_room_environment_node.cpp`, `node_registry.cpp`, room README, `tests/CMakeLists.txt`, room `CMakeLists.txt`.
  Acceptance: `git grep RoomEnvironmentNode` and `git grep room_environment` return nothing; the tree builds.
  Build: `sd build`. The host does not compile between B8 and E3, so B8 lands in the same build as E3.

- [ ] **B9. The benchmark gate.**
  Files: `tests/benchmark/main.cpp`, room README.
  Two rows: three sources and eight microphones with the listener; the same behind three JCM800 amplifiers and cabinets.
  Acceptance: the second row is under 2.67 ms per 256-frame block on Release. The figures go in the commit message. If the row fails, work stops and the owner is told.
  Build: `sd test -t release -s benchmark`. Needs a Release tree, see the note on `sd build` types.

### C. Microphone module

- [ ] **C1. `modules/nodes/microphone`.**
  Files: new `modules/nodes/microphone/` (`CMakeLists.txt`, `README.md`, `include/SushiDSP/microphone/microphone_node.hpp`, `source/microphone_node.cpp`), `modules/nodes/CMakeLists.txt` or its equivalent, `node_registry.cpp`, `test_microphone_node.cpp`, `test_rt_safety.cpp`; deletes the copies under the room module.
  Parameters: `pattern`, `proximity`, `polarity`, `room`. The module links the room module for `PolarPattern`; if `tools/` forbids a node module depending on another, the enum moves to the microphone module and the room links it.
  Acceptance: each parameter reads back what was written; polarity inverts the block; the node allocates nothing in `process()`.
  Build: `sd build`, `sd test -s unit -f Microphone`.

### D. Routing

- [ ] **D1. `StereoStripNode`.**
  Files: new `stereo_strip_node.hpp/.cpp` in `modules/nodes/routing`, `test_stereo_strip_node.cpp`, `node_registry.cpp`, routing README, `docs/reference/GLOSSARY.md`.
  Stereo in, stereo out: `level` (dB), `balance`, `width`. Used by the Listener and Master strips.
  Acceptance: level, balance and width against hand arithmetic; width 0 is mono, width 1 is the input.
  Build: `sd build`, `sd test -s unit -f StereoStrip`.

### E. Host model

- [ ] **E1. `RigTopology` without the room switch, with the DI position.**
  Files: `applications/host_gui/rig_topology.hpp/.cpp`, `tests/functional/unit/test_rig_topology.cpp`.
  `room()` and `set_room()` go. `di_position()` is `BeforeEffects` or `AfterEffects`.
  Acceptance: the DI path still refuses an amplifier and a cabinet with a reason; both positions round-trip.
  Build: with E3.

- [ ] **E2. `CabinetSectionWiring`.**
  Files: new `applications/host_gui/cabinet_section_wiring.hpp/.cpp`, `test_cabinet_section_wiring.cpp`.
  A pure function from the topology, the arrangement and the microphones' room flags to the hidden slots the section needs and the edges between them, as data. No rig is touched.
  Acceptance: the edge lists for Single, 2-way, 3-way and Parallel, with zero, one flagged and eight unflagged microphones, match the diagram in the Design section; no node receives more than eight inputs.
  Build: with E3.

- [ ] **E3. The applier and the rig on the wiring.**
  Files: `topology_applier.hpp/.cpp`, `slot_graph.hpp/.cpp`, `rig_controller.hpp/.cpp`, `test_topology_applier.cpp`, `test_rig_controller.cpp`; deletes `room_layout.hpp/.cpp` and `test_room_layout.cpp`; host README.
  The applier owns the one room slot and applies the wiring's plan with the journal it has. The rig exposes `room_arrangement()` and `set_room_arrangement()`, and copies each microphone's pattern and proximity into the arrangement. A slot snapshot of the room carries the arrangement; a restore renames the ids it holds.
  Acceptance: a new rig with one cabinet renders the same samples as the rig before this plan; adding a microphone and unmuting it changes the output; a failed apply rolls back to the previous graph and arrangement.
  Build: `sd build`, `sd test -s unit`.

- [ ] **E4. Limits on the control thread.**
  Files: `rig_controller.cpp`, `test_rig_controller.cpp`.
  Acceptance: a fourth cabinet path and a ninth microphone are refused with the reason shown to the user, and the rig is unchanged.
  Build: `sd build`, `sd test -s unit -f RigController`.

### F. Presets

- [ ] **F1. Preset format with the cabinet section.**
  Files: `rig_persistence.hpp/.cpp`, new `room_arrangement_codec.hpp/.cpp` (replaces `room_layout_codec`), `test_rig_persistence.cpp`, `test_room_arrangement_codec.cpp`, a pre-P5 preset fixture under `tests/`, the preset page of the manual.
  The version field rises by one. The preset holds the chain type and each stage's slots, the DI switch and position, the arrangement, the microphone nodes and every strip's settings. The reader rejects an unknown version.
  Migration of an older preset, logged once: its room slot becomes the room, with one listener when `listener` was ears and one microphone when it was microphone; its `level` becomes that strip's level; a `mix` below 1 is named in the log as not carried; Cabinet is muted. A preset with no room slot loads with Cabinet on and the listener muted.
  Acceptance: a three-cabinet, eight-microphone rig round-trips to an equal rig; the fixture loads with the migration above.
  Build: `sd build`, `sd test -s unit -f Persistence`.

### G. Screen

- [ ] **G1. `SceneCamera`.** Files: new `ui/scene_camera.hpp/.cpp`, `test_scene_camera.cpp`. An orbit camera: world to screen, and a screen point back onto a horizontal plane. Acceptance: both directions against hand-computed coordinates. Build: `sd build`, `sd test -s unit -f SceneCamera`.
- [ ] **G2. `SceneInteraction`.** Files: new `ui/scene_interaction.hpp/.cpp`, `test_scene_interaction.cpp`. Picking the nearest marker under a point and turning a drag into a pose, kept inside the shoebox. No ImGui. Acceptance: pick and drag cases against hand-computed poses. Build: as G1.
- [ ] **G3. `RoomSceneView`.** Files: new `ui/room_scene_view.hpp/.cpp`; deletes `ui/cabinet_section_view.hpp/.cpp`. Draws the shoebox, the floor grid, the cabinets, microphones and listener with their facing, and applies `SceneInteraction`. Acceptance: compiles and links; seen on screen in G6. Build: with G5.
- [ ] **G4. Mixer panel as strips.** Files: `ui/mixer_panel.hpp/.cpp`, `test_mixer_panel_model.cpp`. Vertical strips in order: Mic 1..N, Room, Listener, Cabinet, Bus, DI, Master; the room flag sits on the microphone's strip. The strip list is built by a pure function. Acceptance: the list for the E2 cases matches the wiring. Build: with G5.
- [ ] **G5. The cabinet section pane.** Files: new `ui/cabinet_section_pane.hpp/.cpp`, `ui/room_panel.hpp/.cpp` (its shell editors serve the inspector), `ui/gear_browser.cpp`, `ui/transport_bar.cpp`, `main.cpp`, `CMakeLists.txt` (also drops the two files listed twice), `ui/README.md`; deletes `ui/cabinet_section_window.hpp/.cpp`. Acceptance: `sd build` passes and `sd test -s unit` is green. Build: those two.
- [ ] **G6. On screen, with the owner.** `sd host`. Place three cabinets and eight microphones, drag each, flag one as room, switch the DI position, save and reload. Every refusal shows its reason. Not closed until the owner has seen it.

### H. Close

- [ ] **H1.** `sd check`; a `humanizer` and `source-comments` pass over every comment this plan wrote; the spec's status line, `docs/design/REMAINING_WORK.md` and `docs/reference/KNOWN_ISSUES.md` brought to what is true.

## Builds the owner is asked for

In order: A1, B0, B1, B2, B3, B4, B5a to B5d, B6, B7, C1, D1, B8 with E1 to E3, E4, F1, G1 with G2, G3 to G5, then B9 on a Release tree and G6 on screen. B9 runs as soon as B7 is in, before the host work, so a failed gate stops the plan early.

`sd build` configures only an unconfigured tree, so `--type release` is ignored on the existing RelWithDebInfo tree. B9 needs the tree's type checked in `build/CMakeCache.txt` first.

## Open

- P0, task 1, step 6: the ASIO example has not been run on a stereo driver.
- The width formula `min(max(1, outputs), K_MAX_CHANNELS)` sits in both hosts. A shared function is a public addition and waits for the owner.
- Loop FX sends stay out (spec section 3).
- Three unit tests are skipped by the harness (`ListDirectoryLogsOneWarning...`, two `DkTsToneBlock...`). Not looked into.
