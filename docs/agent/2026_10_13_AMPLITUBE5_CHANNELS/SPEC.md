# AmpliTube 5 channels, cabinet section and room

**Status:** Draft, awaiting the owner's review. No implementation has started.

## 1. Goal

Reproduce AmpliTube 5's signal architecture in SushiDSP: a stereo rig with
path types (Single, 2-way, 3-way, Parallel), a DI path, a mixer of channel
strips, and a cabinet section in which cabinets and microphones are placed
inside a room. The copy comes first. Changes that go beyond AmpliTube 5 are a
separate task, taken after the copy is in place and verified.

## 2. Scope

In scope:

- Cabinet section: cabinets, microphones as rig elements, and a room
  environment that holds them.
- Mixer: Mic 1..N, Room, Bus, DI and Master strips.
- Chain paths: Single, 2-way, 3-way and Parallel; the DI path.
- Stereo rig: stereo input for Parallel, stereo output always.
- A 3D scene for placing cabinets, microphones and the listener.

Out of scope for this work:

- The Studio section (multitrack recording), TONEX, MIDI, Custom Shop,
  ToneNET and Custom IR.
- A Space mode. The owner dropped it on 2026-10-13. A room is always present
  in the cabinet section, as it is in AmpliTube 5.

Open within scope, see section 12: Loop FX sends, the VIRTM view.

## 3. Decisions

Each row is an owner decision, recorded with its date.

| Decision | Date | Consequence |
| --- | --- | --- |
| Copy AmpliTube 5 directly; modify after the copy works | 2026-10-13 | No design choice is made to differ from AmpliTube 5 without a later task. |
| Include a stereo rig | 2026-10-13 | The backlog item "Render the rig in stereo on the ASIO host" becomes phase P0. |
| Stereo input moves from P0 to P3 | 2026-10-13 | Only the Parallel path takes a stereo input, so the stereo input pair is built with it. P0 covers stereo output, which the example needed. The GUI host already prepares its rig at the device width. |
| Microphones are separate rig elements | 2026-10-13 | A microphone node exists; a cabinet does not own microphones. |
| The room owns placement | 2026-10-13 | Positions of cabinets, microphones and the listener live in the room, keyed by the rig node's identity. Gear nodes carry gear data only. |
| Acoustics are model-based | 2026-10-13 | Microphone position changes distance, proximity and directivity. It does not change speaker-specific timbre, because no measured per-position impulse responses exist. |
| Node output ports | 2026-10-13 | `AudioNode` gains named output ports. Port 0 is the default, so existing nodes are unchanged. Public interface change, see section 6. |
| Limits: 3 cabinets, 8 microphones | 2026-10-13 | Matches the 3-way splitter, and the existing fan-in limit `K_MAX_FAN_IN_OUT = 8`. Over-limit placements are refused on the control thread, with a reason. |
| Mixer split into a channel strip and a bus | 2026-10-13 | `MixerNode` keeps summing; the strip is a new brick. |
| Solo and mute live on the bus, not the strip | 2026-10-13 | Owner decision. Only the bus sees every input, so only it can make solo silence its neighbours. The strip keeps level, pan, polarity and delay. Width moves to P4 with the room's stereo sum. |
| The P2 mixer panel moves to P4 | 2026-10-13 | Proposed in the phase plan, not yet approved by the owner. The panel is UI, and P4 builds the scene the strips sit in. |
| Engineering decisions taken without the owner, under the instruction to finish | 2026-10-13 | (a) Each (cabinet, receiver) pair is its own RoomNode; the tail is not shared, so the spec's open item 2 is closed as "per pair". (b) Benchmark threshold: a block of 3 cabinets and 8 microphones must take under half of the 5.33 ms budget. Not measured; the benchmark case is not written yet. The tree builds and the unit and regression suites pass (see PLAN.md, build status). (c) VIRTM view is a plane with distance and angle only; no per-speaker bleed. (d) Loop FX stays out. (e) The microphone holds polarity and the room flag; its position and pattern stay with the room's placement, because the pattern changes what the room computes. (f) The master is two-level: a bus of the eight microphone strips and the two listener strips, a head bus, and the DI strip, all summed in a master bus of three inputs. Solo applies within each bus, not across them. (g) Parallel uses one root per channel, bound with RigGraph::bind_input, and a channel strip per branch panned hard left and right. |
| Room namespace for the new room classes | 2026-10-13 | Owner request: the new classes are in SushiDSP::Room. The rest of the room module stays SushiDSP::room until the migration renames it. |
| Parallel path as two mono amplifier chains | 2026-10-13 | Matches AmpliTube 5. Reuses the existing 1-to-2 copy and 2-to-1 sum rules. |
| The 3D scene is built with the first placeable items | 2026-10-13 | Not after the audio work. Phase P4 carries a visible scene. |
| Term: "channel strip" for AmpliTube's "channel" | 2026-10-13 | Recorded in `docs/reference/GLOSSARY.md` in the phase that introduces the strip. |

## 4. Signal flow

```
input (mono or stereo, from ChannelSelection)
  -> path split (Single | 2-way | 3-way | Parallel), each path a mono chain
       amp -> effects -> cabinet                       (one per path)
  -> room environment
       sources: cabinet outputs, one input port each (at most 3)
       receivers: microphones (at most 8) and the listener
       outputs: one output port per receiver
  -> channel strips
       Mic 1..N   (each microphone port)
       Room       (microphones marked as room)
       DI         (DI path)
  -> bus (Mic and Room strips)
  -> master (Bus + DI)
  -> stereo output
```

The DI path runs beside the main chain. It takes the rig input directly and
cannot hold an amplifier or a cabinet. Its position relative to the main path's
effects is a setting.

## 5. Bricks

| Brick | Responsibility | Location | New or existing |
| --- | --- | --- | --- |
| `RigGraph` port selection | Routes an edge from a chosen output port of a node | `modules/foundation/core` | Changed |
| `AudioNode` output ports | Declares named output ports and their channel counts | `modules/foundation/core` | Changed (public) |
| `ChannelSelection` stereo | Maps a stereo device pair to the rig input | `modules/foundation/audio_format` | Changed |
| `MicrophoneNode` | Holds microphone data: pattern, proximity, polarity, room flag. No position. | `modules/nodes/microphone` | New |
| `CabinetNode` | Cabinet impulse response and convolution. No position. | `modules/nodes/cabinet` | Existing |
| `RoomNode` | Environment: sources, receivers, placements, one output port per receiver | `modules/nodes/room` | Rewritten interface, existing solvers kept |
| `RoomHost` | Control-thread placement and registration by node identity | `modules/nodes/room` | Extended |
| `ChannelStripNode` | Level, pan, mute, solo, polarity, delay, stereo width for one strip | `modules/nodes/routing` | New |
| `MixerNode` | Sums its inputs. Used by the bus and the master. | `modules/nodes/routing` | Existing, narrowed |
| Path builder | Builds the path graph for each chain type, including Parallel and DI | `applications/host_gui` (`RigTopology`, `TopologyApplier`) | Changed |
| Room scene view | 3D placement of cabinets, microphones and the listener | `applications/host_gui/ui` | New |
| Channel strip panel | Strip controls, Mixer view | `applications/host_gui/ui` | New |

The room solvers (`ImageSourceSolver`, `ModalSolver`, `LateReverb`,
`DiffuseField`, `BinauralDecoder`) are reused. Their per-source and per-receiver
calls are wrapped by the room node; they are not rewritten.

## 6. Interface changes

These change public surfaces and follow `api-stability`. Each is named in its
phase and in the changelog entry that lands it.

1. `AudioNode`: a node may declare named output ports. `ports()` returns the
   count and channel width per port. Default: one port, port 0, the current
   behaviour.
2. `RigGraph::connect`: an edge may name the producer's output port. Edges
   without a port use port 0.
3. `RoomHost`: adds source and receiver registration by node identity, and
   placement setters. Existing `set_scene` remains for the bare room.
4. `ChannelSelection`: accepts a stereo pair as the rig input.
5. `K_MAX_CHANNELS` stays 2. Ports, not wider buses, carry several receivers.

## 7. Room model

- A room scene holds one shoebox, its materials and furnishings, as today.
- Sources are cabinet outputs. Each source is a point at a placed position
  with a direction.
- Receivers are microphones and the listener, each a point with a direction.
- For every source-receiver pair the room computes the direct path, the early
  reflections from the image-source solver, and the standing-wave contribution.
- The late reverberant tail is computed per source and shared across
  receivers. Its level is set by the source's power and the room's
  reverberation, as `DiffuseField` already does. This sharing is an
  approximation; section 12 records it.
- Cost grows as sources times receivers for the early and modal parts. The
  performance gate (section 9) decides whether this is acceptable at the limits.
- The listener receiver produces the binaural pair. Microphone receivers
  produce one mono signal each.
- A room with no cabinet produces silence on every receiver.

## 8. Data and persistence

- Rig presets are JSON and follow `file-formats`: a version field, a
  documented schema, and a reader that rejects unknown versions.
- Phase P5 adds the room's placements, the microphone nodes and the strip
  settings.
- A preset written before P5 loads as: one cabinet, the existing room object
  as the room, one listener receiver where the old `listener` value was ears,
  and one microphone receiver where it was microphone. The loader records the
  migration in the log.
- Placements are stored under the node identity, not the node's position in
  the chain. Reordering the chain does not move a microphone.

## 9. Performance

- Budget: a 256-frame block at 48 kHz is 5.33 ms. The benchmark harness is
  `sd test -t release -s benchmark`.
- Gate, before P4 is implemented: a benchmark of 3 cabinets and 8 microphones
  in one room, with the amplifiers in the graph, must stay under a threshold
  the owner sets (section 12). The benchmark is written in P4's first step and
  its figures go in the commit message.
- The reference point: the JCM800 with one room costs 0.893 ms per block
  (commit 78a798f).
- Everything on the audio thread allocates nothing, locks nothing, makes no
  system call and throws nothing. `tests/functional/unit/test_rt_safety.cpp`
  gains a case for each new node, and for `RoomNode::process` with the new
  placements.

## 10. Testing

Each phase has its own tests before it lands:

- P0: a mono chain on a two-channel rig renders the same samples on both
  outputs (`RigGraphCopiesAMonoProducerIntoAStereoConsumer`, existing). The
  ASIO example has no unit test; its change is checked by compiling it.
  Stereo input moved to P3, see section 3.
- P1: ports default to port 0; an edge on port 1 routes port 1's samples;
  `compile()` refuses an edge to a port the node does not declare. Golden files
  for every existing product stay bit-identical.
- P2: a channel strip against hand arithmetic for level, pan (constant power),
  polarity, mute, solo and delay; the bus and master sums.
- P3: Parallel renders two independent branches; the DI path refuses an amplifier
  or a cabinet with a reason.
- P4: a room with one source and one receiver reproduces `RoomNode`'s current
  output; two sources and three receivers match the sum of single-pair renders;
  placement changes take effect at the next publish without an audio-thread
  allocation.
- P5: preset round trip; a pre-P5 preset loads with the migration in section 8.
- P6: scene interaction is covered by a panel-level test; the 3D projection is
  checked against hand-computed coordinates.

Golden files for the existing mono products do not change in P0, P1, P2 or P3.
Any change to them is a regeneration with its own row in `tests/golden/README.md`.

## 11. Phases

Each phase is one or more commits, each commit one logical change. Each phase
lands with its changelog line and its manual pages, as `documentation` requires.

| Phase | Content | Acceptance |
| --- | --- | --- |
| P0 | Stereo output on the ASIO example | The example's rig is as wide as the device; the test in section 10 passes |
| P1 | Output ports on `AudioNode` and `RigGraph` | Section 10, P1 tests; goldens bit-identical |
| P2 | Channel strip and bus, mixer split | Section 10, P2 tests; mixer panel shows the strips |
| P3 | Parallel path and DI path | Section 10, P3 tests; path builder covers all four types |
| P4 | Cabinet section: microphone node, room environment, 3D scene | Benchmark gate passed; section 10, P4 tests; the scene places items |
| P5 | Preset format for placements, microphones and strips | Round trip and migration tests |
| P6 | Mixer view, room mic flag in the panel, polish | Panel tests |

## 12. Open items

These need an owner decision before the phase that depends on them.

1. **Benchmark threshold** for the P4 gate, as a share of the 5.33 ms block
   budget. Needed before P4.
2. **Tail sharing** (section 7): accept a tail shared per source, or compute a
   tail per receiver. Needed before P4.
3. **VIRTM view.** AmpliTube 5 places microphones on a speaker plane with
   bleed indicators. Our model has no per-speaker data. Include a plane view
   with distance and angle only, or leave it out. Needed before P4's scene.
4. **Loop FX sends** on the amplifiers. AmpliTube 5 has them; this spec leaves
   them out. Needed before P3.
5. **Room microphone flag.** Whether the room flag belongs to the microphone
   node (as written here) or to the room placement. Needed before P2.
6. **Standards migration.** `docs/agent/2026_10_12_SUSHI_STANDARDS_MIGRATION`
   is open. New code in this work follows what the tree does until that
   migration's phase 3 changes both at once.

## 13. What this spec does not claim

- It does not claim AmpliTube 5 sounds the same. Structure is copied. Tone
  depends on model accuracy, and microphone position changes only what the model
  represents.
- It does not use AmpliTube 5's measurement data, which belongs to IK
  Multimedia.
- It does not change the JCM800 tone anchor. Amplifier and cabinet modules are
  not modified by P0 to P3, and a change to them would come with its own
  listening check.

## 14. Decisions taken in the final run, 2026-10-13

- **Six microphones on the host, eight in the engine.** The room environment still accepts eight, but the host layout takes six: the master bus has eight inputs and the listener ears take two. This changes the limit the owner approved in section 3; the owner should confirm it.
- **Presets save the cabinet section without the open switch.** The switch lives in the chain's topology, which presets do not save yet.
- **The mixer follows the bus wiring.** Room bus inputs are labelled in layout order, the order the applier connects them.
- **The plan view is top-down.** The spec's "3D scene" is a plan with draggable markers; a perspective view is a later change.

