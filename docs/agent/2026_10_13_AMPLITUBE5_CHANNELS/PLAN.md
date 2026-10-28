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

## Tasks, as built

The owner asked on 2026-10-13 for one build at the end of the plan, in Release, in place of a
build per task. Every task below was therefore written against `clang++ -fsyntax-only` with the
flags of `build/compile_commands.json`, over all 330 sources of the tree, and is verified only by
the build status at the end of this file.

| Task | State | What differs from the plan |
| --- | --- | --- |
| A1 Nine ports, per-port fan-out | Written | Nothing. |
| B0 Pin today's output | Not done | A golden of the old node needs the old node built, and there was one build. The node's own tests, adapted, stand in for it. |
| B1 Solvers take a source and a receiver | Not done | The solvers keep their signatures. Each pair is solved from its own scene, and the shell's part of a design repeats per pair; an unchanged pair is copied. Backlog: "Solve the shell once per arrangement". |
| B2 The tail's network and its output | Written | `LateReverb::process_sums` gives the raw sums; the pair's gain, tone and coherence live in `Room::PairStage`, read out of two rings per cabinet. No separate `TailOutput` class. |
| B3 Modal bank with read-out rows | Written | Nothing. |
| B4 Early renderer reads a shared ring | Not done | Each pair keeps its renderer's ring. Backlog: "One early ring per cabinet". |
| B5 The stages | Written | `Room::SourceStage`, `Room::PairStage`, `Room::EarsOutput`, `Room::MicrophoneOutput`, `Room::HeadHolder`. `GeometricPath`, `ModalPath` and `BinauralStage` are deleted. |
| B6 Arrangement and the environment render set | Written | `room_render_set` stays: a pair's set is still a `RoomRenderSet`. |
| B7 `RoomNode` as the environment | Written | The port layout is `{6, 2}`: three slots as wide as the main output, of which the first channel is read. |
| B8 Delete `RoomEnvironmentNode` | Done | Nothing. |
| B9 The benchmark gate | Run, failed | 3.896 ms against 2.67 ms; see the build status. |
| C1 Microphone module | Written | The module does not link the room; its pattern is its own enumeration. |
| D1 Stereo strip | Written | Also: a bus now starts each gate at its target on its first block, because every publish builds a fresh bus and a muted listener would otherwise leak for 5 ms. |
| E1 Topology | Written | Nothing. |
| E2 Wiring | Written | Nothing. |
| E3 Applier and rig | Written | The section exists while the rig holds any gear, not only while it holds a cabinet; a path with no cabinet gives the room no source. The rig's shape is recorded after every apply. |
| E4 Limits | Written | Nothing. |
| F1 Preset format 2 | Written | The codec is in `rig_persistence.cpp`; there is no separate arrangement codec file. |
| G1 to G5 Screen | Written | The workspace switch sits in its own row under the top bar. |
| G6 On screen, with the owner | Not done | Needs the build. |
| H1 Close | Done | `check_changelog`, `check_docs_layout` and `check_source_comments` are clean for the files this work touched; `check_docs_truth` reports only findings that predate it. |

## Build status, 2026-10-13

One full build: `sd build -D CMAKE_BUILD_TYPE=Release` at four parallel jobs, 600 steps, exit 0,
no error and no warning in a file this work touched. After the first test run three files were
changed and rebuilt incrementally; nothing else was built.

- `sd test -s unit`: 1227 tests. The first run passed 1223. The four failures and what they were:
  a hand-written format 2 preset with no `"edges"` key loaded with no edges, a defect in the
  loader, fixed by wiring such a preset from its nodes' own inputs; two gear filter cases still
  counted the room as a listed item; and one room case compared the microphone's tail bit for bit
  against a network that applies its gain before the lines, where the node applies it after. The
  second run passed 1227 of 1227. Three cases are skipped by the harness, as before this work.
- `sd test -s regression`: 23 of 23.
- `sd test -s benchmark`: ran. One cabinet heard by the listener costs 0.240 ms per 256-frame
  block, against 0.238 ms before this work. The room at its limits, three cabinets, the listener
  and eight microphones, costs 1.802 ms. **The gate fails:** three JCM800 chains into that room
  cost 3.896 ms against 2.67 ms. The amplifiers are 1.96 ms of it. The plan said work stops here
  and the owner is told; `docs/backlog/README.md` holds the figures.
- Not done: G6, the check on screen with the owner. The host was built and linked; it has not
  been opened.

## Open

- P0, task 1, step 6: the ASIO example has not been run on a stereo driver.
- The width formula `min(max(1, outputs), K_MAX_CHANNELS)` sits in both hosts. A shared function is a public addition and waits for the owner.
- Loop FX sends stay out (spec section 3).
- Three unit tests are skipped by the harness (`ListDirectoryLogsOneWarning...`, two `DkTsToneBlock...`). Not looked into.
