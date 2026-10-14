# `modules/nodes/room`, cabinets, a listener and microphones in one room

One `AudioNode`, registered as `sushidsp.room` under the `Room` category with one parameter,
`direct_delay`. It holds up to three cabinets, one listener and up to eight microphones in one
shoebox room. Each cabinet is an input slot; the listener's two ears leave on output port 0 and
microphone `m` leaves, mono, on port `1 + m`. The node solves the room on the control thread
and renders its standing waves, its direct sound, its early reflections and its late tail on the
audio thread, for every cabinet at every receiver. The module depends on `SushiDSP::sushi_dsp`
alone.

"The room as an environment" below describes how the node is put together; the sections after it
describe each layer of one cabinet heard at one receiver, which is what every pair of the
environment renders.

Intent, the physics behind each solver and the acceptance list live in
[`docs/archive/superpowers/specs/2026-09-08-room-acoustics-design.md`](../../../docs/archive/superpowers/specs/2026-09-08-room-acoustics-design.md);
the task order is
[`docs/archive/superpowers/plans/2026-09-08-room-acoustics.md`](../../../docs/archive/superpowers/plans/2026-09-08-room-acoustics.md).
This file records what the code in this directory does. `docs/design/SUSHIDSP.md` §3c places the
module in the tree and §9 carries its roadmap rows;
[`docs/reference/KNOWN_ISSUES.md`](../../../docs/reference/KNOWN_ISSUES.md) holds what the phase
knowingly left open.

## Which class owns what

| Concern | Class |
| --- | --- |
| The shoebox, its six materials, patches, and the source, listener and microphone poses | `RoomScene` |
| Six-band absorption per material, with a citation per row | `materials.hpp` |
| Six-band equivalent absorption area and a nominal box per item of furniture, with a citation per row | `furniture.hpp` |
| Where a placed furnishing's absorption lies, for a reflected path and for a mode | `FurnishingField` |
| A cabinet's box, its cones on the baffle and the three shipped cabinets | `CabinetGeometry`, `cabinet_preset` |
| The cones as radiating elements and what reaches a point from each | `DriverField` |
| The room's volumetric loss per metre: the air's ISO 9613-1 figure plus the furnishings', the one seam `EyringDecay`, `ImageSourceSolver` and `ModalSolver` all read | `MediumAbsorption` |
| The absorption a built room adds to every surface, per band | `built_room_loss.hpp` |
| The room a fresh room slot loads | `StarterScene` |
| Six band gains and the arithmetic on them | `BandResponse` |
| A six-band target fitted to two biquads, minimum phase | `BandResponseFit` |
| ISO 9613-1 air attenuation over a path length | `AirAbsorption` |
| Four pistons in a baffle: gain per band for an outgoing direction | `CabinetDirectivity` |
| A first-order pattern and its proximity effect | `MicrophoneModel` |
| The shipped microphones, one row each: name, transducer, starting pattern, on-axis curve and its source | `MicrophonePreset`, `microphone_preset`, `find_microphone` |
| Mirror images to order 4, each reduced to one tap | `ImageSourceSolver` |
| The taps one scene publishes | `EarlyTapSet` |
| Eyring's decay per band and the Schroeder frequency | `EyringDecay` |
| The steady-state reverberant field one cabinet raises in one room, per hertz | `DiffuseField` |
| Line lengths, per-line absorption filters, the input gain and the output filter of the tail | `LateReverbDesign` |
| The room's eigenmodes below the Schroeder frequency, read at the listener's receiver | `ModalSolver` |
| Minimum-phase HRIR pairs on a grid, ITDs, the diffuse-field average | `HrtfSet` |
| The rigid-sphere HRTF for a machine with no measured set | `SphericalHeadHrtf` |
| Reading a converted HRTF asset from disk | `HrtfSetLoader` |
| Third-order Ambisonic encode gains for a direction | `AmbisonicEncoder` |
| Sixteen Ambisonic channels to two ears | `BinauralDecoder` |
| The power-complementary Butterworth pair at the modal seam | `CrossoverPair` |
| Up to 512 second-order resonators | `ModalBank` |
| Everything the audio thread reads for one cabinet at one receiver | `RoomRenderSet`, `build_render_set` |
| The shell, the listener, the cabinets and the microphones, with the limits checked on every edit | `Room::Arrangement` |
| Every pair's render set, built as one and published as one | `Room::EnvironmentRenderSet`, `Room::EnvironmentBuilder` |
| What a host calls to install an arrangement, a scene or an HRTF | `RoomHost` |
| Up to 128 fractional taps read from a ring it is handed, two biquad state banks, sixteen channels out | `EarlyReflectionRenderer` |
| The 16-line Hadamard feedback delay network | `LateReverb` |
| What one cabinet puts into the room: the split at the seam, the tail's network, the modal bank | `Room::SourceStage` |
| What one receiver hears of one cabinet above the seam: the direct sound, the taps, the tail's output | `Room::PairStage` |
| The listener's sum: one decode, the diffuse pair, the alignment delays | `Room::EarsOutput` |
| One microphone's sum, held back to line up with the listener | `Room::MicrophoneOutput` |
| The HRTF set and the decoder published for it | `Room::HeadHolder` |
| The topology, the one render set, the swap and the crossfade | `RoomNode` |

The direct path's renderer, the 128-tap FIR, the fixed frame delay and the fractional-delay ring
the ears need are files of their own beside `EarlyReflectionRenderer` (`DirectPath`, `FirFilter`,
`FrameDelay`, `DelayRing`), each owned by the stage that drives it. `FadeWindow` carries the one
crossfade window every stage reads, and `CascadeFilter` is the fourth-order filter the tail's
output cascades run through.

The classes named `Room::` are in the namespace `SushiDSP::Room`; the rest of the module is in
`SushiDSP::room`.

`RoomNode` is the only class here that knows the topology. `ImageSourceSolver` and
`LateReverbDesign` consume the materials, the fits, the air and the directivity; the renderers
consume the records the solvers publish and nothing else, so a renderer is tested with a
hand-written record and a solver with arrival times alone.

## The room as an environment

`RoomNode` declares `PortLayout{6, 2}`: three input slots as wide as its main output, of which it
reads the first channel, so a mono cabinet is copied into its slot by the graph's own adaptation
rule. It declares nine output ports: port 0, the listener, is two channels wide, and ports 1 to 8
are one channel each. A port with no microphone behind it carries silence, and a node run outside
a `RigGraph`, with no port table, renders the listener into its two outputs.

What is placed in the room is a `Room::Arrangement`: the shell (a `RoomScene`'s dimensions,
materials, patches, furnishings and cabinet geometry), the listener's pose, up to three sources
and up to eight microphones. A source and a microphone each carry the identity the host gave
them, a pose, and for a microphone the polar pattern and the proximity switch. A source's index
is the node's input slot and a microphone's index plus one is its port, so the order of each list
is part of the arrangement. Every edit checks the limits and answers a refusal with a reason; a
fourth cabinet and a ninth microphone are refused and leave the arrangement as it was
(`ArrangementRefusesAFourthCabinetAndStaysAsItWas`).

`RoomHost::set_arrangement` validates every pair's scene, builds one `Room::EnvironmentRenderSet`
that holds a `RoomRenderSet` per (cabinet, receiver) pair, and publishes it through one
`RtSwapSlot`. The audio thread therefore adopts a placement whole: it never renders some pairs of
the new arrangement beside others of the old. `Room::EnvironmentBuilder` keeps the scene each pair
was built from, so a pair whose cabinet, receiver and shell did not change is copied from the set
in place and not solved again: dragging one microphone solves one pair per cabinet.
`RoomHost::set_scene` stays for the bare room. It replaces the shell, the listener's pose and the
first cabinet's pose, and leaves the microphones as they are.

On the audio thread the work is split by what each piece of state is a function of.

| Stage | Count | What it renders |
| --- | --- | --- |
| `Room::SourceStage` | one per cabinet | The split at the seam, the high half held in a ring, the tail's network, and the modal resonators read at every receiver |
| `Room::PairStage` | one per cabinet and receiver | The direct sound, the image-source taps, and the tail read at this pair's delay and level |
| `Room::EarsOutput` | one | The listener's pairs summed, decoded once, with the tail through the diffuse pair |
| `Room::MicrophoneOutput` | one per microphone | A microphone's pairs summed into one mono block |

**The tail is rendered once per cabinet.** `LateReverb::process_sums` runs the network at unit
gain with no input delay and hands back its two output sums, A and B, before the tone correction
and the coherence pair. The source stage writes both into rings. Each pair reads them at its own
delay to the crossover time and its own level, which a set calls `input_gain`: the network is
linear, so a gain and a delay at its input and the same gain and delay at its output are the same
signal. The pair then runs the tone correction, and for the ears the coherence pair, over what it
read. The line lengths and the per-line absorption depend on the shell alone and are taken from
the cabinet's listener pair.

**The modal resonators run once per cabinet.** A resonator's state depends on the cabinet's
signal and its mode shape at the cabinet. `ModalBank::process` takes a list of read-outs, each a
row of one gain per mode and a block to add into, and advances the resonators once whatever the
count. The source stage holds rows 0 and 1 for the listener's two ears and row `2 + m` for
microphone `m`. A row is the same sum the bank made when it read two ears alone, in the same
order (`ModalBankReadsEveryRowAsItReadsTheTwoEars`).

**Every port carries the decoder's partition.** The listener's reflections come out of the
decoder one partition late, 128 frames at 48 kHz, and its direct sound, tail and modal field wait
that long to meet them. A microphone has no decoder, so `Room::MicrophoneOutput` holds its block
back by the same partition and the ports line up. `latency_samples()` reports the partition, plus
the first cabinet's propagation delay to the listener when `direct_delay` is physical.

Solving stays one pair at a time, and the room's own part is solved once. `design_shell` designs
what no pose enters, the tail's lines with their filters and the modal resonators, and
`build_render_set` takes it for each pair's own scene: the shell with that cabinet's pose and
geometry and that receiver's pose. `Room::EnvironmentBuilder` keeps one `ShellDesign` per distinct
room and cabinet geometry, so a drag that moves a microphone or a cabinet solves images and
designs nothing else (`RoomShellDesignDoesNotReadThePoses`). Every pair of a cabinet reads the
one ring its `Room::SourceStage` writes.

## The cones in the near field

A cabinet is a box with one, two or four cones on its baffle, and a source's pose is the baffle's
centre. `Room::SourcePlacement` carries the geometry, so two cabinets in one room can differ.

Far from the cabinet the cones act as one source with a pattern, which is what
`CabinetDirectivity` gives and what every reflection and the listener's line of sight use. Against
the grille they do not. `DriverField` breaks each cone into seven elements, its centre and six
points on a ring at 0.72 of its radius, and `ImageSourceSolver` writes a microphone's line of
sight as one tap per element: the element's own delay and 1/r, its share of the cabinet's
pressure per band, the air, and the capsule's pattern and proximity boost at that element's
distance and direction. `EarlyTapSet::direct_count` says how many leading taps those are: 7, 14
or 28 for a microphone, one for the ears.

The shares follow the model the far field already has. A cone's effective radius shrinks above
the breakup frequency, and the centre element carries the centre disc's part of that shrinking
piston's area: a seventh at 125 Hz, half at 4 kHz. The shares of a cabinet sum to one in every
band, so on axis and far away the elements add up to the far field's unit gain
(`DriverFieldAddsUpToOneOverTheDistanceFarOnAxis`). Close in, a capsule on the dust cap is nearest
the element that holds the highs, one at the cone's edge is nearest the ring, and one between two
cones takes two arrivals that comb (`DriverFieldIsBrighterOnTheDustCapThanAtTheConesEdge`). An
element is never heard from nearer than 2 cm, and one behind the baffle plane takes the rear
shelf.

## What a scene has to clear

`RoomScene::is_valid` bounds each dimension to [1.5, 12] m, holds the source, the listener and
the microphone inside the box, holds each patch inside its own face, and refuses a room whose
reverberation runs past `K_MAX_DECAY_SECONDS`, 12 s. That last bound is one chord change per bar at
60 bpm, four seconds, with the previous chord 20 dB down when the next one lands; spec §4.1
carries the derivation.

`RoomScene` cannot ask `EyringDecay` for a decay time, since `EyringDecay` reads `RoomScene`, so
the bound reaches the scene as a floor on the mean absorption that the scene computes itself:
`min_mean_absorption()` returns 1 - exp(-0.161 V / (S * K_MAX_DECAY_SECONDS)), Eyring inverted for
alpha with the air term dropped. All six bands are checked against it, because a bare concrete
box is dead at 4 kHz and enormous at 125 Hz. Dropping the air term makes the gate refuse a little
more than Eyring's own figure would: `RoomScene{}` reads 1.32 s at 500 Hz against Eyring's
1.29 s.

The floor rises with V / S, so the same material passes in one room and fails in another. Bare
unpainted concrete, alpha = 0.01 at 125 Hz, passes at 6.0 x 4.0 x 2.5 m, at 9.81 s, and fails as
a cube past an edge of 4.495 m. The block room is the tightest of the rooms in the tree, at 15.9 %
of the bound; `RoomScene{}` sits at 11.0 %. `RoomSceneAcceptsEveryReferenceRoom` and
`RoomSceneRefusesARoomThatRingsPastTheModulesDecay` walk them.

`RoomScene{}` is that 6.0 x 4.0 x 2.5 m box with gypsum board on studs on the four walls and
the ceiling and heavy carpet on the floor, no patch and no furnishing. It is the bare reference
room the tests and the figures above pin; spec §4.1 carries why it is furnished rather than bare
and why it carries no doorway. What a fresh room slot and a preset with no `"room"` object load
is `starter_scene()`, the next section.

The panel prints the reason `is_valid` writes, so a refused drag says which band, which value and
which floor without any change to `RoomPanel` or `RigController`.

## Furniture, coverings and the starter scene

A furniture row is an equivalent absorption area per item, in square metres per octave band, not
a geometry: the increase in the room's equivalent absorption area one bed, one wardrobe module or
one chair brings, the floor and the walls under it included (ISO 354 object absorption, ISO
20189). `furniture.hpp` carries seven rows, each with its citation, and
`RoomScene::furnishing_absorption_area()` sums quantity times area over the scene's live
furnishings. `docs/design/ROOM_FURNISHINGS.md` section 6 says which rows are measured, which are
tabulated and which are derived.

A covering is not a furniture row. A curtain over a window or a rug on a floor replaces the
absorption of the surface under it, so both are `Materials` rows that enter a scene as a `Patch`,
which takes its area out of its face. Counting a curtain as an area added on top of the untouched
window would count the glass twice: 49 % too much absorption at 500 Hz for 3 m2 of heavy velour
over a glass window (design section 3).

A furnishing may have a place: a point on the floor and a yaw. One without a place absorbs
evenly over the room, which is everything below. One with a place keeps the room's total and
moves who pays it. `FurnishingField` spreads its absorption as a bell around its box, normalised
to one over the room, and answers twice. A reflected path loses by the density it runs through,
sampled every 25 cm along the unfolded path and folded back into the room, so a reflection past
the sofa loses more than one across the empty half (`FurnishingFieldWeighsAPathPastTheFurnishingAboveOneThatMissesIt`).
A mode loses by its pressure squared over the density against its mean, so a bed at an antinode
damps that mode more than the same bed at its node
(`FurnishingFieldDampsAModeMoreAtItsAntinodeThanAtItsNode`). Eyring's time and the tail do not
change, and the line of sight is still not thinned: a furnishing casts no shadow and reflects
nothing.

`MediumAbsorption` is the one seam the three layers read. It returns the air's ISO 9613-1
coefficient plus `A_f(f) / 4V` for the furnishings, per metre and per band, and `EyringDecay`,
`ImageSourceSolver` and `ModalSolver` each take their loss from it, so a furnished scene's tail,
its early reflections and its standing waves stay in level agreement instead of stepping at the
seam. `ImageSourceSolver` folds it on reflected taps only: the `(0, 0, 0)` image is a clear line
of sight, and furniture absorbs what reflects off it rather than thinning a path nothing crosses.

`RoomScene::is_valid` refuses a furnishing that names no row, a non-positive or non-finite
quantity and more than `K_MAX_FURNISHINGS`, 16, entries, and it counts the furniture against the
12 s decay bound: each band's surface absorption `a_s` is raised to
`1 - (1 - a_s) * exp(-A_f / S)` before the floor of `min_mean_absorption()` is applied. That form
is the exact algebraic inversion of adding `A_f` to Eyring's denominator, not a linearisation, so
the gate and the decay the node then reports cannot disagree. Patches are bounded the same way:
`K_MAX_PATCHES`, 8, per scene, each area positive, finite and inside its own face, and the patches
sharing one face may not together exceed that face's area.

`starter_scene()` is what a fresh room slot and a preset with no `"room"` object load: 6.0 x 4.0
x 2.6 m, painted plaster on five faces and parquet on the floor, a 4 m2 heavy velour curtain, a
12 m2 rug and a 1.9 m2 wooden door as patches, and a double bed, a desk, three wardrobe modules
and a chair as furnishings, with no sofa. It sets `built_room_loss`.

`RoomScene::built_room_loss` adds `K_BUILT_ROOM_LOSS`, 0.020, 0.025, 0.020, 0.020, 0.020 and 0.015
from 125 Hz to 4 kHz, to every face's absorption, capped at 1, inside `face_absorption()`, so
Eyring, the image sources, the modal damping and the decay gate all see it. It stands for what a
built room absorbs beyond its tabulated surfaces: doors, windows, radiators, ducts, seams and
leakage. A bare `RoomScene{}` leaves it off, as does a preset room object without the
`"built_room_loss"` field, so an ideal box or a sealed chamber can still be described.

### Measured against the Motus room and the BRAS CR2 room

Two real rooms are checked in. The first: 4.9 x 4.4 x 2.9 m of concrete, 61.13 m3, from the Motus dataset
(Zenodo 4923187). `tools/dsp/motus_reference.py` computes T20, T30, EDT, DRR and C50 per octave
from three of its ambisonic impulse responses into `tests/fixtures/motus_reference.csv`, whose
method and provenance are in `tests/fixtures/motus_reference.README.md`;
`tests/functional/unit/test_room_motus.cpp` builds the same scenes here and prints both figures.
The empty room (measurement 0001) and the room with a carpet (0002):

| Band | Measured T30, empty | Model, empty | Ratio | Measured T30, carpet | Model, carpet | Ratio |
| --- | --- | --- | --- | --- | --- | --- |
| 125 Hz | 2.572 s | 3.123 s | 1.214 | 2.704 s | 2.989 s | 1.105 |
| 250 Hz | 2.147 s | 3.856 s | 1.796 | 2.057 s | 3.473 s | 1.689 |
| 500 Hz | 2.009 s | 3.180 s | 1.583 | 1.635 s | 2.688 s | 1.644 |
| 1 kHz | 2.119 s | 3.429 s | 1.618 | 1.428 s | 2.087 s | 1.461 |
| 2 kHz | 1.958 s | 3.410 s | 1.741 | 1.166 s | 1.427 s | 1.224 |
| 4 kHz | 1.508 s | 2.080 s | 1.380 | 0.998 s | 0.973 s | 0.975 |

The module decays 1.10 to 1.80 times longer than the room does, worst at 250 Hz. The carpet
shortens both, and the model's reduction is the larger one: 39.2 % against 32.6 % at 1 kHz,
58.2 % against 40.4 % at 2 kHz, 53.2 % against 33.8 % at 4 kHz.

The second is the empty BRAS CR2 seminar room at RWTH Aachen, 146.1 m3 and 202.5 m2 of linoleum,
concrete, plaster and windows (TU Berlin DepositOnce, doi 10.14279/depositonce-6726.3).
`tools/dsp/bras_reference.py` averages the T30 of its ten dodecahedron responses into
`tests/fixtures/bras_cr2_reference.csv`, and `tests/functional/unit/test_room_bras.cpp` models it
as a box with the same volume and area per material. With `built_room_loss` set, the model's
decay against the measured one:

| Band | Motus, measured | Motus, model | Ratio | CR2, measured | CR2, model | Ratio |
| --- | --- | --- | --- | --- | --- | --- |
| 125 Hz | 2.572 s | 1.917 s | 0.745 | 1.402 s | 2.991 s | 2.133 |
| 250 Hz | 2.147 s | 1.961 s | 0.913 | 1.740 s | 2.569 s | 1.476 |
| 500 Hz | 2.009 s | 1.940 s | 0.966 | 2.022 s | 2.442 s | 1.208 |
| 1 kHz | 2.119 s | 2.033 s | 0.959 | 1.938 s | 2.201 s | 1.136 |
| 2 kHz | 1.958 s | 2.029 s | 1.036 | 1.741 s | 2.084 s | 1.197 |
| 4 kHz | 1.508 s | 1.584 s | 1.051 | 1.563 s | 1.588 s | 1.016 |

The tests bound 500 Hz to 2 kHz: within a fifth for Motus, within a quarter for CR2. Below
500 Hz the two rooms disagree, Motus wanting less loss and CR2 more, so no single figure fits
both there; [`docs/reference/KNOWN_ISSUES.md`](../../../docs/reference/KNOWN_ISSUES.md) carries that
entry and `docs/design/ROOM_FURNISHINGS.md` section 11 the evidence.

## Which thread runs what

The audio thread calls `RoomNode::process`, and through it `CrossoverPair::process_sample`,
`EarlyReflectionRenderer::render`,
`EarlyReflectionRenderer::clear_bank`, `LateReverb::process`, `ModalBank::process`,
`BinauralDecoder::process` and, through `Room::SourceStage`, `Room::PairStage` and the two
output stages, `DirectPath`, `FirFilter`, `FrameDelay` and `DelayRing`; at the block that adopts a set it also
calls `CrossoverPair::set_crossover`,
`ModalBank::reset` and `BinauralDecoder::reset`, all arithmetic. None of them allocates, locks,
opens a file or throws after `prepare()`; `tests/functional/unit/test_rt_safety.cpp` traps
`operator new` around each of them, and around `RoomNode::process` while publishes land that
move a microphone, move the listener, widen the room, take cabinets and microphones away and
bring them back, and switch `direct_delay`, so each path the node can take runs under the trap
(`RoomNodeProcessPerformsNoHeapAllocationAtTheLimits`).

Everything else runs on the control thread. `RoomNode::set_arrangement` validates the
arrangement and, for each pair that changed, calls `build_render_set` for that pair's receiver,
which solves the images, designs the tail, finds the Schroeder frequency, designs the modal bank
and takes the direct HRIR pair from the HRTF set. It re-times each listener pair for
`direct_delay` and publishes the finished `Room::EnvironmentRenderSet` through one `RtSwapSlot`.
An arrangement the validator refuses is not built and the previous one stays in place. `prepare()` and `set_hrtf` build a `BinauralDecoder` from
the HRTF set in place and publish it through a second slot; a set the decoder refuses, one with
no third-order decoder, is refused whole and the set in place stays. `reclaim_rooms()` frees
every set and every decoder the audio thread has moved past; the host calls it once per GUI
frame, as it does `reclaim_cabinets()`.

`process()` does one `acquire_for_audio_thread()` per slot per block. The set pointer it gets
back is compared against the one adopted last and read inside that block only: each pair's taps,
direct tap and HRIR pair, the tail's design, every modal row and the diffuse pair are copied into
the stages' own banks the first block a set is seen, so no set is ever read after the block that acquired it, and
a reclaim can free a retired set at any time. The decoder pointer is used inside the block alone.

## The two-bank crossfade

Each `Room::PairStage` owns an `EarlyReflectionRenderer`, which holds two banks of biquad state,
and two copies of `EarlyTapSet`, one per bank. What follows is one pair; every pair of the room
does the same under the one window `RoomNode` keeps. A set that is not the one adopted last goes into the spare bank: its
taps are copied in, the bank's state is zeroed, every tap that matches its retiring counterpart
takes that tap's filter state through `carry_tap`, and a 512-frame raised-cosine fade starts.
During the fade both banks render every block, the retiring bank's sixteen channels are blended
into the arriving bank's, and when the fade ends the retiring bank is the next spare. A set that
arrives mid-fade restarts the fade from the bank that was arriving; the node never holds more than
two. The direct path has two banks of its own, loaded and carried under the same match rule as the
taps and blended after rendering. The tail's coefficients are not crossfaded: `LateReverb` adopts
a design at the block the stage hands it over and runs its own 512-frame fade only when the line
lengths move. A pair's delay to t_c and its level, though, are read out of the cabinet's two tail
rings and blended between the retiring and the arriving values under the fade, so a re-timed or
re-levelled set never steps the tail.

The fade starts one decoder partition, 128 frames at 48 kHz, after the set is adopted, when the
decoder's first partition of the arriving set comes out. The paths something downstream holds
back read the fade's window unlagged: the ambisonic channels, which the decoder holds; the modal
rows, which an alignment delay holds; and everything of a microphone pair, whose whole block
`Room::MicrophoneOutput` holds. The listener's paths that blend at the output, its direct pair
and its tail's read, take the window at the lag, and its tail's cascades switch at the block the
lag runs out. So every path comes out in step and nothing at a port moves before the fade
begins. The ears' reference blend in the tests is delayed the same 128 frames.

Taps pair by index, the arrival order `ImageSourceSolver` returns. An arriving tap past the
retiring count starts from zero; a retiring tap past the arriving count is dropped. A pair
matches when the delay moved by at most 2 frames and none of the ten coefficients moved by more
than 0.2 (`K_MATCH_DELAY_FRAMES`, `K_MATCH_COEFFICIENT_MOVE` in `pair_stage.cpp`). A tap that does not
match starts from zero, because a retiring state run through a different filter is further from
that filter's steady state than zero is: carrying every tap regardless measured -15.4 dBFS for a
one-centimetre move against -32.0 dBFS from zero.

The fade hides the switch of taps, not a tap's start from zero. The slowest pole among the
garage's tap filters has radius 0.99918, a time constant of 1220 frames, so a zero-state
transient outlasts the 512-frame fade by a factor of 2.4 and is still at 66 per cent when the
fade ends. Measured in the garage with a 1 kHz full-scale tone, the output during the fade
differs from a raised-cosine blend of the two steady renders by:

| Publish                          | Taps carried | Residual           | Two-shelf fit      | Before the carry |
| -------------------------------- | ------------ | ------------------ | ------------------ | ---------------- |
| The same scene republished       | 96 of 96     | 0, exactly         | 0, exactly         | -36.2 dBFS       |
| The microphone moved 1 cm        | 78 of 96     | 0.0010, -59.8 dBFS | 0.0160, -35.9 dBFS | -37.5 dBFS       |
| The microphone moved 0.36 m      | 5 of 94      | 0.0051, -45.8 dBFS | 0.0233, -32.7 dBFS | -30.8 dBFS       |

The "two-shelf fit" column is the figure with `BandResponseFit` as it stood before 2026-09-09,
two shelves that cannot dip; the "before the carry" column is the zero-state figure with the band
fit before `fa95bb5`. The tap counts apply the node's match rule, a delay within 2 frames and
every coefficient within 0.2, to the solver's two tap sets. A 0.36 m move reorders the taps, so
the carry buys nothing there and the residual is what a zero-started tap leaves: with the two
shelves the slowest pole over the garage's 96 microphone taps had radius 0.99779, a time constant
of 453 frames, and 44 of the 96 taps started from zero after a 1 cm move because a target the
shelves could not fit, the direct path's 4x12 directivity at 2 kHz for one, landed on a different
local minimum for a 0.03 dB change and moved a coefficient by up to 3.2. With the sections that
can dip and the directivity averaged over each octave, the slowest pole has radius 0.98801, a
time constant of 83 frames, so a zero-started tap settles inside the 512-frame fade, and the
1 cm case reaches the -60 dBFS of spec §5.3. The 0.36 m case read -52.1 dBFS while the tail sat
12 dB under its diffuse-field level; at that level, with the tail's output cascade following what
the taps leave of the field, a move steps the cascade's coefficients on the network's running
state, 3 % in the second section here, and the residual is -45.8 dBFS
(`docs/reference/KNOWN_ISSUES.md`). The three cases are
`RoomNodeCrossfadesAPublishWithoutAStep`, `RoomNodeCrossfadesASmallMoveFromTheCarriedState` and
`RoomNodeRepublishesTheSameSceneWithoutAResidual`, bounded at -44, -58 and -120 dBFS. The
residual column is measured through the seam of the next section, whose high half feeds the
taps: before the wave layer landed the same two moves read -53.9 and -60.2 dBFS. The
step bound of the same cases, no sample-to-sample step beyond the steady signal's own, measures
a ratio of 1.0033 for the 0.36 m move, where it measured 1.0051 with the two shelves and 0.9994
before `fa95bb5`; the tests bound it at 1.01.

## The wave layer and the seam

Below the Schroeder frequency the room is a set of standing waves, and the node renders them
with `ModalBank` in both quality modes, on the maintainer's ruling of 2026-09-08 (spec §9a.1).
One `CrossoverPair` splits the input at `RoomRenderSet::crossover_hz`: the low half feeds the
modal bank, the high half feeds the early renderer and the tail, and each output is that ear's
modal field plus the geometric sum. A crossover of zero, which the solver publishes when f_s is
under 40 Hz, bypasses the pair: the high half carries the whole input and the bank, whose count is
then zero, adds nothing; `RoomNodeSwitchesTheWaveLayerOffInADeadRoom` renders a 12 m carpeted
room with four open faces, f_s = 22 Hz, through the node and through the two bricks alone and
finds the outputs equal sample for sample.

The split sits at the input, not on the ear sums, so that every filter stays upstream of the
crossfade. A high-pass after the fade does not commute with the raised cosine, and measured the
0.36 m microphone move of `RoomNodeCrossfadesAPublishWithoutAStep` at -41.9 dBFS against the
blend of the two steady renders, where the input split measured -52.1 dBFS with the tail as it
then stood.

The modal bank has its own state, two banks of it, and a published set is adopted under a match
rule shaped like the early renderer's `carry_tap`. `Room::SourceStage::same_resonators` compares the
arriving design's sections with the active bank's, bit for bit: one room designs one set of
resonators whatever the poses, so a moved cabinet or listener leaves every section equal and
changes `source_gain` and `ear_gain` alone (spec §9a.2; `RoomNodePublishesNewModalGainsWithoutRebuildingAResonator`
counts 161 of the garage's 187 modes feeling a move along x and 150 a move along y). When the
resonators match, the active bank keeps its state and its gains ramp from the retiring design to
the arriving one, frame by frame, under the same raised cosine the taps fade under; the bank's
output is linear in the ear gains, so the ramp is the blend of the two steady renders sample for
sample, measured at -144 dB in `RoomNodeRampsTheEarGainsAsTheOutputBlendWould`. The source gain
ramps the drive, and the resonators then take their own decay time to settle to it, as the room's
standing waves would after the cabinet is carried across the floor:
`RoomNodeCarriesTheStandingWavesAcrossAMovedCabinet` measures a step ratio of 1.0000 and the
arriving pose's steady energy within 0.02 dB four blocks after the fade. When the resonators
differ, a changed dimension or material, the spare modal bank is reset and rendered with the
arriving design while the retiring bank renders the retiring design, and the two are blended
under the fade; the new field then builds from zero at its decay time, -16.8 dB four blocks
after the fade in `RoomNodeRebuildsTheStandingWavesInAChangedRoom`. A set arriving mid-fade
restarts from the bank now active, as the taps do; the crossover's coefficients move at the same
block, on a running state, which is a transient a room change carries anyway.

`RoomNode::schroeder_frequency_hz()` reports the frequency the published set splits at: the
solver's `crossover_hz` while the modal layer runs, which is Eyring's figure unless the mode
count would pass `K_MAX_MODES` and the solver lowered it (spec §9a.4), and Eyring's figure itself
when the layer is off, so a panel still shows the room's own number under 40 Hz. Before
`prepare()` there is no set and the report is Eyring's figure for the scene in place.
`RoomNodeReportsTheCrossoverTheSetSplitsAt` walks the garage, a 3.0 x 2.5 x 2.4 m concrete room
whose 877 Hz Eyring figure has about 1250 modes under it, and the dead room through it.

Measured on the garage with the cabinet at (0.4, 0.5, 0.5) and the listener at (5.4, 3.4, 2.0),
a 262144-frame impulse response at 48 kHz: the first six peaks sit at 28.59, 42.93, 51.45,
57.34, 68.52 and 71.70 Hz against the analytic 28.58, 42.88, 51.53, 57.17, 68.60 and 71.46 Hz,
the worst 0.33 % off; the -3 dB widths of the first five read 1.48, 0.89, 1.54, 1.59 and 0.88 Hz
against 2.199 / T60 = 1.49, 0.82, 1.52, 1.49 and 0.84 Hz from the solver's decays, the sixth
being unreadable because (2, 1, 0) sits 2.86 Hz above (0, 0, 1) and their contours overlap; and
the 20 to 100 Hz band is 14.0 dB louder with the ears in the corner at (0.15, 0.15, 0.15) than
at the centre, where every mode with an odd index is at a node; the spec asks for 12 dB.

### The level the two layers meet at

Both layers are scaled to one reference, `DiffuseField`: the room's steady-state reverberant
power per hertz of a white input, relative to the cabinet's on-axis direct sound at 1 m, which is
16 pi RT60(f) / (0.161 V) over the cabinet's directivity index, per band and linear in log
frequency between the centres (spec §6.3). `diffuse_field.hpp` is header-only; it caches the
directivity index of the last cabinet geometry behind a mutex, since the 2562-direction integral
costs 18 ms against a 2 ms build, both measured on an unoptimised binary, and a drag never
changes the geometry. Its cases live in
`tests/nodes/room/test_modal_solver.cpp` (`DiffuseFieldIsEyringOverTheCabinetsDirectivity`)
until the test list can take a file of its own. The index is the
directivity of `CabinetDirectivity`, 1.1, 3.0, 7.6, 12.0, 15.7 and 18.8 dB for the 4x12, and it
is what keeps a beaming cabinet's room quieter than a monopole's: the garage's octave under f_s
holds 2.4 dB less than Eyring alone says.

`ModalSolver` sets `level_scale` so that the octave under the crossover, summed over its modes as
each section's white-noise gain 2 / (1 - r^2) and averaged over the cabinet's and the receiver's
positions, equals the field's integral over that octave; that is an identity of the modal sum,
since the volume mean of psi^2 is the mode's norm, the mode density is Weyl's and each mode's
damping is Eyring's. `LateReverbDesign` hands the tail what the taps leave of the field, per band:
the field times the receiver's share of a diffuse field, 1 for the ears and a^2 + (1 - a)^2 / 3
for a microphone of pattern a + (1 - a) cos(theta), less every tap past the direct sound at gain
squared times its fitted filter, floored at zero; sixteen lines keeping a mean G of
10^(-6 tau_i / RT60(f)) per pass deliver 16 G / (1 - G) per unit input gain, exact for
incoherent energy flow whatever the lengths, so the input gain squared is the mean over bands of
remainder over that, each band's share floored at -30 dB of the largest, and the output cascade
is fitted to the square root of each band's share of the mean. Jot's tone correction, which
flattened the tail's energy spectrum, is one factor of that cascade; a room's reverberant energy
is not flat. Rendered alone through `LateReverb`, 131072 frames, each band's energy lands within
1 dB of its remainder from 250 Hz up and within 2 dB at 125 Hz in all three reference rooms
(`LateReverbDesignRendersEachBandsRemainder`); the garage's tail carries an input gain of 0.38,
where the trend of the last 5 ms of taps before t_c, 5 taps of the 56 a diffuse shell holds
there, had set 0.11.

### When the tail takes over

The network emits nothing until its shortest line has run, so that line is the tail's own
latency and the delay ahead of it is what is left of the handover time:
`delay_to_crossover_frames` is `round(t_c fs) - line_frames[0]`, floored at zero. Until
2026-09-10 it was `round(t_c fs)` outright, and the garage's tail arrived at 1749 + 3449 = 5198
frames, 108 ms, instead of the 36 ms the image set stops at. What the maintainer heard of that
is in the impulse response: the 5 ms window at 105 ms read -23.3 dB against -38 dB for the
windows just before it, so the whole tail landed at once, after a 70 ms hole, 15 dB above what
preceded it. The energy was right and the time was not.

The shortest line is now the handover time itself. `line_frames` takes a shortest line and a
spread rather than a mean, and `design` picks them from two constraints: the shortest line is
`round(t_c fs)`, and the sixteen still sum to `K_OVERLAP_RATIO` times the mid-band RT60, which is
what fixes the tail's modal density (spec §6.2, three modes per resonance bandwidth). The
spread that satisfies the second given the first is bisected; the garage's is 5.836, its lines
run 1753 to 10211 frames and total 78098 against the 78003 the rule asks for. A room whose handover time exceeds a sixteenth of its
delay budget cannot start there, and then the budget sets the shortest line at the old spread
of 2 and the input delay carries the rest. `FdnDesign::K_MAX_LINE_SECONDS` caps the longest line,
so a room past the design's 3 s RT60 loses density rather than overrunning the renderer's
storage.

The garage's impulse response now runs -12.4 dB at 5 ms for the direct sound, the reflections
down to -27 dB by 35 ms, the tail from -22.4 dB at 35 ms and a smooth decay past that; the
bedroom and the block room hand over at 37.98 and 39.35 ms against their own t_c of 37.90 and
39.36 ms.
`LateReverbDesignDelaysTheTailToTheHandoverTime` walks all three and holds the onset within 8
frames of t_c. A compensated ears set moves the tail's read delay earlier with everything else,
but the shortest line is latency the design cannot give back, so the delay floors at zero and
the ears' tail sits up to the propagation delay late, 363 frames for `RoomScene{}`.

Measured at the microphone, energy-averaged over six poses per room, 65536 frames each: the
octave under f_s reads +0.5, +0.9 and +0.8 dB against the modal design's own power at those poses
through the low half, and the octave over it -0.4, +0.0 and -1.2 dB against the field through the
high half, garage, bedroom and block room; both within the spec's 2 dB
(`RoomNodeIsContinuousAcrossTheSchroederFrequency`). The raw step between the two octaves reads
+0.16, +1.29 and +3.75 dB and is recorded, not bounded: it is the reference's own change across the
seam, the 4x12's directivity index rising 4.6 dB between 250 and 500 Hz and the block room's
absorption halving from 125 to 250 Hz, less the modal field's interior deficit. That deficit is
the pose's mode-shape factor, the product of cos^2(n pi x / L) over the six coordinates against
the mean of a half each, which the diffuse average assumes: with four modes along the garage's
height, a cabinet 0.5 m off the floor and a listener at 2 m read 0.375 for that factor's z terms
where the average is 0.5, and the poses a room is used at sit 2 to 4 dB under the volume average
in the octave under f_s, the standing waves piling up at the walls and corners instead. The rays
have no counterpart for it. Before the shared reference the garage's seam stepped by 7.7 dB at one
pose, -1.64 dB under and -9.37 dB over: the tail was 10 dB under the field and the modal octave
2 dB over it.

`DiffuseField` is a ratio, so a test that scales both layers to it cannot see it move. One
case holds the module's absolute level down from outside: `RoomNodeCarriesOneOverROnTheCabinetsAxis`
reads an omni capsule on the cabinet's axis in the dead room at 1, 2 and 4 m as -0.30, -6.64 and
-13.32 dB against the 0, -6.02 and -12.04 dB of 1 / r, the shortfall being the air's over 4 m.
The reference itself agrees with the classical critical distance where the cabinet is close to
a monopole: the garage's D/R at 2.60 m reads -15.61 and -13.71 dB at 125 and 250 Hz from
`DiffuseField` against the -16.66 and -16.63 dB of (r_c / r)^2 with r_c = 0.057 sqrt(V / RT60),
and the two part above 500 Hz by the cabinet's directivity index, which the classical figure
takes as unity.

The pair is power-complementary, a fourth-order Butterworth low-pass and high-pass at one
corner with section Q 0.5412 and 1.3066, because the two halves carry uncorrelated fields and
their powers add; until 2026-09-09 it was Linkwitz-Riley and the seam dipped 3 dB at f_s.

## The receivers, and where the standing waves are read

A receiver is the listener or a microphone, and its port fixes which: receiver 0 of every cabinet
is the listener and receiver `1 + m` is microphone `m` (spec §8). Each pair's set is built for its
receiver: the image solver ends its paths at `listener_position` or at
`microphone_position`, encodes each tap to sixteen Ambisonic channels for the ears or to channel
0 alone for the microphone, folds the microphone's pattern and proximity into the microphone's
taps, and the tail's design carries the coherence pair for the ears and unity for the microphone.

The modal field is read where that receiver stands. `ModalSolver::design` takes the listener
kind: for the ears it evaluates psi at two points 8.5 cm to either side of `listener_position`,
for the microphone at `microphone_position` in both ear gains, and the resonators, the count,
the crossover and the level are the room's whatever the receiver
(`ModalSolverReadsTheMicrophoneAtOnePoint`, `BuildRenderSetReadsTheStandingWavesAtTheSetsReceiver`).

A microphone's port carries one signal: channel 0 of the early set, the tail read once, and
the modal field at one point, at physical timing, held back by the decoder's partition so it
lines up with the listener's port. The ears hear the binaural path of the next section.

Measured through the node, impulse responses at 48 kHz over the direct-path window of a 12 m
carpeted room with four open faces, the cabinet at its centre:

- A cardioid 5 cm on axis against the same capsule at 3 m reads 13.34 dB more in the 125 Hz
  third octave, taken against the 4 kHz third octave so that 1 / r cancels, where the model
  says 13.68 dB. The plan's 15.5 dB at 100 Hz is the model's own value and does not reach the
  output: `BandResponseFit` is anchored at the 125 Hz band and its low shelf holds level below
  it, so the rendered 100 Hz third octave reads 13.79 dB.
- The ears 2 m from the 4x12, 60 degrees off axis against on axis: the 2 kHz third octave drops
  15.2 dB where the piston model's band says 15.3 dB, and the 125 Hz third octave moves 0.43 dB
  where the model says 0.49 dB.
- In the garage, the cardioid 5 cm off the cone has a direct-to-reverberant ratio of 20.2 dB
  and the ears at 2.12 m one of -4.5 dB, the direct window running from two frames before the
  onset to 60 after it and everything else, the standing waves included, counting as room.
- Switching the listener mid-tone is a publish: every tap is new and starts from a cleared
  bank. With `direct_delay` physical, in the dead room the residual against the raised-cosine
  blend of the two steady renders is -38.1 dB of the peak to the ears and -38.9 dB to the
  microphone and the step ratio 1.0105, bounded at -36 dB and 1.02; they read -42.1, -43.4 dB
  and 1.00 while the dead room's tail was nil, and its tail now sits at the floor of its
  design, an input gain of 0.0075 for the ears against the garage's 0.42. In the garage the
  residual is -22.6 and -23.7 dB of the peak and the step ratio 1.00 to the ears and 1.044 to
  the microphone, and that is the tail: its input gain is 0.42 for the ears' set and 0.24 for
  the cardioid's, a third of the field, and the lines carry the old listener's stored energy
  into the new set at the room's decay, where the reference blend has no memory, so both
  figures read the tail's own level against that blend rather than a discontinuity and both
  are recorded, not bounded. With `direct_delay` compensated the ears' tail is read 280 frames
  earlier than the microphone's in the dead room and 363 in the garage, so the switch also
  moves the tail's input under the fade and the tail rings with the old timing at the room's
  decay: -31.5 and -32.2 dB in the dead room, -13.7 and -17.1 dB in the garage, recorded and
  bounded in neither room.

## The binaural path

The ears hear three paths that share one HRTF set, which is what keeps the seam between them
inaudible: the direct sound through the set's HRIR pair for its direction, the early reflections
through the set's third-order decoder, and the tail through the set's diffuse-field average
(spec §9.4). The set is `SphericalHeadHrtf` until a host installs a measured one through
`RoomHost::set_hrtf`; `hrtf_source()` says which. The tree ships
`assets/hrtf/sadie2_ku100_48k.hrtf`, the SADIE II KU100 set thinned to 1531 directions, and
`applications/host_gui` installs it through `RigController::set_hrtf_path` at start-up.

**The direct path.** Tap 0 of an ears set, the (0, 0, 0) image, is taken out of the bank at the
block the set is adopted (`Room::PairStage::take_direct_image`) and rendered by `DirectPath`: read out
of the cabinet's ring at the alignment delay plus the tap's, through the tap's fitted filter and 1/r
in the early renderer's own arithmetic, then convolved with the set's minimum-phase HRIR for its
ear, the far ear read the interaural delay later. `RoomNodeEarsHearTheDirectSoundThroughTheHrirPair`
renders the dead room and matches that chain, built in the test from the render set, to
1e-5 of the peak in both ears. Woodworth's delay comes through the whole node:
`RoomNodeReproducesWoodworthsInterauralDelayOnTheFallback` reads the lag of the interaural
cross-correlation peak at 30, 60 and 90 degrees as 0.2650, 0.4963 and 0.6593 ms against
(a / c)(theta + sin theta) = 0.2611, 0.4881 and 0.6558 ms, within the one sample the spec allows,
with the cabinet facing the listener; 60 degrees off the cabinet's axis its 2 kHz notch weights
the correlation toward the low frequencies, where a rigid sphere's delay runs half again
Woodworth's (Kuhn, JASA 62(1), 1977), and the lag read 0.58 ms.

**The reflections.** The sixteen accumulation channels go through `BinauralDecoder`, whose 32
partitioned convolutions add one 128-frame partition of latency at 48 kHz. `SphericalHeadHrtf`
now builds its own decoder, since the spec's magnitude-least-squares design lives in the
conversion tool and a machine with no asset had none: each grid pair, delayed by its ITD, is
summed at its solid angle times the channel's encode gain times (2n + 1) times the max-rE weight
of its order (Zotter and Frank, JAES 60(10), 2012), the least-squares decoder of a grid that
samples the sphere by solid angle. Above about 1.5 kHz a third-order decoder sums a lobe of
directions whose delays differ by up to the ITD, so a reflection's high end comes out combed and
dull against the direct sound's, the known defect of low-order binaural Ambisonics that the
magnitude-least-squares design of the conversion tool is there to remove for a measured set. It
is not measured here; the decoder acceptance of spec §9.5 runs only with the measured set present.

**The tail.** The tail's two outputs pass one 128-tap FIR per ear, the diffuse-field average, then
a 128-frame delay to the decoder's partition; the modal field passes the same delay, so every
ears path waits one partition. The tail's coherence pair survives the head:
`RoomNodeTailIsCoherentLowAndIncoherentHigh` reads the interaural cross-correlation of the dead
room's tail alone as 0.944 in the 125 Hz octave and 0.176 in the 4 kHz octave, against the spec's
0.8 and 0.3.

**`direct_delay`.** Compensated, the default, moves an ears set's whole render earlier by
round(d / c * fs) at publish (`RoomNode::compensate_direct_delay`): tap 0 by that many frames,
every reflection by the same but floored at one frame, the tail's delay to t_c by the same but
floored at zero. The direct sound then lands at the alignment delay, 128 frames, and the
reflections and the tail keep their spacing to it; `latency_samples()` reports 128. Physical
leaves the set as solved and reports 128 plus the propagation delay: 408 for the dead room's 2 m,
491 for `RoomScene{}`'s 2.596 m, the 363 frames the plan's 280 assumed a 2 m scene for
(`RoomNodeDefaultsDirectDelayToCompensated`). `RoomNodeDirectOnsetMovesWithTheDirectDelayChoice`
sees the direct onset and the floor image's onset both move by 280. The choice is the
listener's: a microphone pair renders at physical timing whatever the parameter says. A changed
choice publishes through the crossfade like a moved receiver; the index path stores it for the
next publish.

Measured cost at 48 kHz and 256 frames on a Release build, best of 200, the garage with the
cabinet at (0.4, 0.5, 0.5) and the listener at (5.4, 3.4, 2.0), every layer running
(`tests/benchmark/main.cpp`):

| Row | ns/sample | One core | Per 256 frames |
| --- | --- | --- | --- |
| `RoomNode`, one cabinet, ears, steady state | 880.1 | 4.22 % | 0.225 ms |
| `RoomNode`, one cabinet, ears, a scene republished every block | 9415.2 | 45.19 % | 2.410 ms |
| `RoomNode`, one cabinet, ears and one microphone, steady state | 1006.6 | 4.83 % | 0.258 ms |
| `EarlyReflectionRenderer::render`, the ears' 84 taps | 107.4 | 0.52 % | 0.028 ms |
| `EarlyReflectionRenderer::render`, a microphone's 111 taps | 97.3 | 0.47 % | 0.025 ms |
| `ModalBank::process` | 171.9 | 0.83 % | 0.044 ms |
| `BinauralDecoder::process` | 368.4 | 1.77 % | 0.094 ms |
| JCM800 into `RoomNode` | 3683.2 | 17.68 % | 0.943 ms |
| the room's share of that chain | 858.6 | 4.12 % | 0.220 ms |
| `RoomNode`, three cabinets, ears and eight microphones | 5002.0 | 24.01 % | 1.280 ms |
| three JCM800 into that room | 13915.6 | 66.80 % | 3.562 ms |

The swap row is a bound and not a working figure: it republishes a scene on every block, which
nothing does. A slider drag republishes at 30 Hz, one block in six at this rate and block size,
and the blocks between it run the steady-state row. Spec §10 budgeted 1.63 ms and 2.57 ms scalar
for the ears.

The taps are filtered four to a vector. A tap's two biquads are a chain no core can overlap with
itself, about 2 ns a frame, so one tap after another the 84 taps cost 0.052 ms; four chains side
by side in one AVX register cost 0.028 ms. The loop is written with intrinsics because the same
loop left to the compiler's vectoriser ran no faster than the scalar one. A build without AVX
runs the taps one lane after another to the same samples.

The room at its limits renders 27 pairs in 1.280 ms, with a 4x12's 28 element taps at the head
of every microphone pair. A microphone added to one cabinet costs 0.033 ms. The last row is the
cabinet section's gate, set at 2.67 ms, half the block, and it does not pass: three JCM800
amplifiers alone cost 2.03 ms, so the gate leaves the room 0.64 ms.
[`docs/reference/KNOWN_ISSUES.md`](../../../docs/reference/KNOWN_ISSUES.md) carries it.

## Sizes fixed at `prepare()`

A tap's delay is bounded by the longest crossover time the scene bounds allow,
`(4 + 1) * 12.0 / 343` s, 8397 frames at 48 kHz; a cabinet's ring, which its taps and its direct
paths read, holds that plus the alignment delay plus 128 frames for the far ear's ITD, rounded
up to 16384. The tail's lines hold `FdnDesign::K_MAX_LINE_SECONDS`, 0.525 s or 25200 frames at
48 kHz, which is also the ceiling the designer spreads the lines under. The accumulation buffers are two sets of sixteen channels of one block; the two modal
banks hold `K_MAX_MODES` resonators each and one block of lane partials, and the node keeps four
copies of `ModalDesign`, two per bank plus the ramp's ends, beside its two tap sets, its two
direct banks with their four 128-tap histories, the two diffuse filters, the four alignment
delays and one copy of `FdnDesign`. The decoder's 32 convolvers, 256 taps in two partitions of
128, live in the decoder the slot holds. The spherical-head HRTF and its decoder are built at
the prepare rate; a measured set installed through `set_hrtf` at another rate is dropped for the
fallback at the next `prepare()`.

## Approximations, named as such

- **Seven elements a cone.** `DriverField` samples a cone at its centre and on one ring, with
  the ring's radius fixed while the effective piston shrinks, so off axis and far away the
  elements only approximate the pattern they sum to on axis. The listener's line of sight and
  every reflection keep the far-field pattern, whatever the distance. Every cone of a cabinet is
  the same cone, and the grille cloth is not modelled.
- **A furnishing is a bell, not a box.** `FurnishingField` spreads a placed furnishing over half
  its box's size, round in plan, so its yaw turns the drawing and not the sound, and a path that
  misses the bell loses nothing to it. The mode weight is tabulated to index 48; past it a
  furnishing damps evenly.
- **The piston array.** `CabinetDirectivity` is the cabinet's rigid pistons in an infinite baffle, and the
  cone's effective radius shrinks above the 2.5 kHz breakup frequency in `K_CABINET4X12`. No cone
  is a rigid piston; the model gives the off-axis roll-off its shape, not a measured polar plot.
  Each band is the energy mean of the closed form over its octave, sixteen log-spaced points,
  so an interference null inside a band lowers the band by what the null removes and no more:
  the garage's direct path, 15.6 degrees up, reads -11.5 dB at 2 kHz where the closed form at
  the centre reads -22.1 dB, since the two rows of drivers cancel at 2120 Hz there.
- **Two sections per tap.** `BandResponseFit` gives each of a tap's two biquads a step and a
  peak, so a section can shelve, dip or both, and the fit reaches the direct path above within
  0.1 dB and a one-band cut of 20 dB within 0.5 dB. A target with two dips is beyond it: over
  629 directions around the 4x12 the worst octave-mean target misses by 4.6 dB and 34 miss by
  more than 1 dB, none nearer than 45 degrees to the axis. Of 3774 nudges of 0.05 dB across
  those targets, 52 move a coefficient by more than 0.1 and 2 by more than 1, where the fit
  switches between two local minima of a target it misses by over 2 dB.
  The ceiling image of the reference garage, 53 degrees above the baffle's axis, comes out of
  that fit with a group delay that puts its rendered peak one frame after its arrival, which is
  why `RoomNodePlacesTheFirstOrderPeaksWhereTheSolverSaid` measures placement with a 1 cm piston.
- **Fifteen materials.** `materials.hpp` ships unpainted concrete, painted concrete block, gypsum
  board on studs, heavy carpet on concrete, thin carpet on concrete, painted plaster on masonry,
  ceramic tile, parquet on concrete, a wood panel, a glass window, a double-glazed window, a
  wooden door, a heavy velour curtain, a light curtain and the open doorway, each as six
  octave-band absorption coefficients with a zero-phase reflection factor. Of spec §4.2's
  fourteen, brick, acoustic foam, a steel garage door and carpet on underlay are still rows to
  add, not code.
- **The microphone reads the standing waves as pressure.** A microphone set's modal ear gains
  are psi at the capsule, whatever its pattern. A gradient capsule in a standing wave also
  reads the pressure gradient, a term in quadrature whose size against the pressure term is
  c / (2 pi f) times the mode shape's slope, of order one at 50 Hz in the garage; modelling it
  needs a second, phase-shifted gain per mode, which `ModalDesign` does not carry.
- **Modal damping at a soft boundary.** `ModalSolver` damps each mode by first-order
  perturbation, right to a few percent for concrete, brick and glass and about 30 % wrong in the
  decay of a mode bounded by gypsum on studs at 125 Hz; the frequency is right in both cases.
- **The fallback's decoder.** The spherical head's third-order decoder is the sampling sum
  above, not a magnitude-least-squares fit, so reflections lose high end against the direct
  sound; a measured set brings its own decoder from the conversion tool.
- **A reflection within a frame of the direct sound.** Compensated, a reflection whose path is
  under 7 mm longer than the direct path is floored at one frame of delay, since the early
  renderer's kernel reads one frame ahead; its spacing to the direct sound is off by under a
  frame.
- **The tail across a re-timed set.** A compensated set moves the tail's delay to t_c with the
  listener's distance, so a listener drag moves that delay a frame every 7 mm, under the fade,
  and the network re-settles to each move at the room's decay; a listener or `direct_delay`
  switch moves it by the whole propagation delay and the tail rings with the old timing for the
  RT60, the residuals recorded above.
- **A fade across listeners.** The modal ramp reads the fade at the lag when the microphone
  takes part, so the ears' delayed presentation shows the ramp one partition late during a
  listener switch; the microphone's tail is its mono tail whichever set arrives.

## What the phase left open

Twelve entries in [`docs/reference/KNOWN_ISSUES.md`](../../../docs/reference/KNOWN_ISSUES.md) belong to
this module, each with its figure and its exit: below 500 Hz the built room's loss fits neither
measured room, reading 0.75 times Motus's decay and 2.1 times CR2's at 125 Hz; the bed, desk, bookcase and kilim
rows rest on a derivation or an analogue, since no measurement of them was found; the measured
HRTF's third-order decoder misses the near ear by up to 5.1 dB above 5 kHz; four of the spec's fourteen material rows are
untranscribed; the SM57 preset has no
measured curve; modal damping at a gypsum boundary is about 30 % wrong; the FDN's own modes below
f_s are crossed out rather than corrected; the raw octave step at the seam is recorded and not
bounded; `BandResponseFit` stops at eight steps on about three fits in five; a structural rig edit
rebuilds the node and empties its tail; a compensated ears
set's tail floors at zero delay and lands 7.6 ms after its direct sound; and nothing between the
node and the device bounds the output, which the decay gate narrows rather than closes. Two more,
the
converter's unweighted diffuse-field average and the tail's output filter stepping on its running
state, were written when the change that caused them landed.

## Implementation notes

### The decoder's transforms and cursor

Each convolver transforms its own input, so the sixteen channels are each transformed twice.
That is the unshared figure spec section 10 budgets. Sharing the spectra between the two ears
would change no interface.

In `binaural_decoder.cpp`, `output_partition_[fill_ ..)` holds the result of the previous partition
that has not been emitted yet. One cursor therefore reads an output frame and writes an input
frame.

### A tap set arriving mid-fade

A tap set that arrives while a crossfade is running restarts the fade from the bank that is
active at that moment. The host coalesces edits, so the path never holds more than two sets
(spec section 5.2).

### The tail's delay lines

The feedback delay network emits nothing until its shortest line has run, so that line is what
the handover time bounds. The sum of the sixteen lines stays `K_OVERLAP_RATIO` times the mid-band
decay, which is what fixes the tail's modal density (spec section 6.2).

A room whose handover exceeds its whole delay budget cannot start there: the budget sets the
shortest line and the input delay carries what is left. The shortest line is the network's own
latency, so the delay ahead of it is what is left of the handover time.

A Butterworth pair is power complementary, so the right ear carries the left ear's energy whatever
the split between the coherent and incoherent sums.

### The proximity boost

The proximity boost multiplies the gradient share `(1 - a) cos(theta)` and never the pressure
share `a`. The on-axis far field therefore reads `a + (1 - a) = 1`, and a 5 cm cardioid reads the
15.5 dB of spec section 8.1 at 100 Hz.

### The audio thread's share of a publish

One atomic acquire per slot is the audio thread's whole share of a publish. The set pointer is
compared against the last one the thread saw and is read only inside the block that acquired it.

### The baffle-step shelf

`cabinet_directivity.cpp` puts the shelf's pole at the baffle step and its zero an octave above it,
so the shelf reads 0 dB far below the step and -6 dB far above it. The octave mean samples the
midpoints of `K_OCTAVE_POINTS` equal log-frequency intervals, from the centre over root two to the
centre times root two.

### The interpolation window

In `delay_ring.cpp` the window starts two frames behind the integer delay. The frames whose four
samples end inside the ring are read without the mask, in one run the compiler vectorises; the
few that wrap are read through it. `EarlyReflectionRenderer` then filters four taps side by side,
each lane with the arithmetic one tap alone would run
(`EarlyReflectionRendererRendersATapTheSameAloneAndInAGroup`).

### Loading an HRTF set

`hrtf_set_loader.cpp` lands every field in a local set first and moves it into the caller's set only
on success, so a failure anywhere leaves the caller's set exactly as it was.

### A set the decoder refuses

`RoomNode::set_hrtf()` runs on the control thread. When `BinauralDecoder::prepare` throws on a
set, it logs one `ERROR` under `K_ROOM_LOG_CATEGORY`, declared in `room_log_category.hpp`, with
the set's direction count, the sample rate and the decoder's message, and returns false with the
HRTF in place untouched.

### The reported crossover

`modal_solver.cpp` reports the first mode left out as the crossover, so `enumerate()` at that
frequency returns exactly the modes the design carries.

### A patch's area

In `room_scene.cpp` a patch replaces area on its face and does not add to it, so its own material's
area is subtracted from the face before either is weighted.
