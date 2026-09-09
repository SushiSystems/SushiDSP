# `modules/room` — one cabinet in one room

One `IAudioNode`, mono in and two channels out, registered as `sushidsp.room` under the `Room`
category with four parameters: `mix`, `level`, `listener` and `direct_delay`. The node solves a
shoebox room on the control thread and renders its standing waves, its early reflections and its
late tail on the audio thread. The module depends on `SushiDSP::sushi_dsp` alone.

Intent, the physics behind each solver and the acceptance list live in
[`docs/superpowers/specs/2026-09-08-room-acoustics-design.md`](../../docs/superpowers/specs/2026-09-08-room-acoustics-design.md);
the task order is
[`docs/superpowers/plans/2026-09-08-room-acoustics.md`](../../docs/superpowers/plans/2026-09-08-room-acoustics.md).
This file records what the code in this directory does.

## Which class owns what

| Concern | Class |
| --- | --- |
| The shoebox, its six materials, patches, and the source, listener and microphone poses | `RoomScene` |
| Six-band absorption per material, with a citation per row | `Materials.hpp` |
| Six band gains and the arithmetic on them | `BandResponse` |
| A six-band target fitted to two biquads, minimum phase | `BandResponseFit` |
| ISO 9613-1 air attenuation over a path length | `AirAbsorption` |
| Four pistons in a baffle: gain per band for an outgoing direction | `CabinetDirectivity` |
| A first-order pattern and its proximity effect | `MicrophoneModel` |
| Mirror images to order 4, each reduced to one tap | `ImageSourceSolver` |
| The taps one scene publishes | `EarlyTapSet` |
| Eyring's decay per band and the Schroeder frequency | `EyringDecay` |
| Line lengths, per-line absorption filters and the input gain of the tail | `LateReverbDesign` |
| The room's eigenmodes below the Schroeder frequency | `ModalSolver` |
| Minimum-phase HRIR pairs on a grid, ITDs, the diffuse-field average | `HrtfSet` |
| The rigid-sphere HRTF for a machine with no measured set | `SphericalHeadHrtf` |
| Reading a converted HRTF asset from disk | `HrtfSetLoader` |
| Third-order Ambisonic encode gains for a direction | `AmbisonicEncoder` |
| Sixteen Ambisonic channels to two ears | `BinauralDecoder` |
| The Linkwitz-Riley pair at the modal seam | `CrossoverPair` |
| Up to 512 second-order resonators | `ModalBank` |
| Everything the audio thread reads for one scene | `RoomRenderSet`, `build_render_set` |
| What a host calls to install a scene or an HRTF | `IRoomHost` |
| One input ring, up to 128 fractional taps, two biquad state banks, sixteen channels out | `EarlyReflectionRenderer` |
| The 16-line Hadamard feedback delay network | `LateReverb` |
| The topology, the swap, the crossfade, the mix and the level | `RoomNode` |

`RoomNode` is the only class here that knows the topology. `ImageSourceSolver` and
`LateReverbDesign` consume the materials, the fits, the air and the directivity; the renderers
consume the records the solvers publish and nothing else, so a renderer is tested with a
hand-written record and a solver with arrival times alone.

## Which thread runs what

The audio thread calls `RoomNode::process`, and through it `CrossoverPair::process_sample`,
`EarlyReflectionRenderer::write`, `EarlyReflectionRenderer::render`,
`EarlyReflectionRenderer::clear_bank`, `LateReverb::process` and `ModalBank::process`; at the
block that adopts a set it also calls `CrossoverPair::set_crossover` and `ModalBank::reset`, both
arithmetic. None of them allocates, locks, opens a file or throws after `prepare()`;
`tests/functional/unit/test_rt_safety.cpp` traps `operator new` around each of them, and around
`RoomNode::process` while publishes land that move the microphone, move the listener and widen
the room, so each path the modal bank can take runs under the trap.

Everything else runs on the control thread. `RoomNode::set_scene` validates the scene, calls
`build_render_set`, which solves the images, designs the tail, finds the Schroeder frequency,
designs the modal bank and takes the direct HRIR pair from the HRTF set, and publishes the
finished `RoomRenderSet` through one `RtSwapSlot`. A scene the validator refuses is not built and
the previous scene stays in place. `reclaim_rooms()` frees every set the audio thread has moved
past; the host calls it once per GUI frame, as it does `reclaim_cabinets()`.

`process()` does one `acquire_for_audio_thread()` per block. The pointer it gets back is compared
against the one adopted last and read inside that block only: the taps are copied into the node's
own bank the first block a set is seen, so no set is ever read after the block that acquired it,
and a reclaim can free a retired set at any time.

## The two-bank crossfade

`EarlyReflectionRenderer` holds two banks of biquad state and `RoomNode` holds two copies of
`EarlyTapSet`, one per bank. A set that is not the one adopted last goes into the spare bank: its
taps are copied in, the bank's state is zeroed, every tap that matches its retiring counterpart
takes that tap's filter state through `carry_tap`, and a 512-frame raised-cosine fade starts.
During the fade both banks render every block, the retiring bank's sixteen channels are blended
into the arriving bank's, and when the fade ends the retiring bank is the next spare. A set that
arrives mid-fade restarts the fade from the bank that was arriving; the node never holds more than
two. The tail is not crossfaded by the node: `LateReverb` adopts a design's coefficients at the
block it sees them and runs its own 512-frame fade only when the line lengths move.

Taps pair by index, the arrival order `ImageSourceSolver` returns. An arriving tap past the
retiring count starts from zero; a retiring tap past the arriving count is dropped. A pair
matches when the delay moved by at most 2 frames and none of the ten coefficients moved by more
than 0.2 (`kMatchDelayFrames`, `kMatchCoefficientMove` in `RoomNode.cpp`). A tap that does not
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
| The microphone moved 1 cm        | 78 of 96     | 0.0010, -59.6 dBFS | 0.0160, -35.9 dBFS | -37.5 dBFS       |
| The microphone moved 0.36 m      | 5 of 94      | 0.0025, -52.1 dBFS | 0.0233, -32.7 dBFS | -30.8 dBFS       |

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
1 cm case reaches the -60 dBFS of spec §5.3. The three cases are
`RoomNodeCrossfadesAPublishWithoutAStep`, `RoomNodeCrossfadesASmallMoveFromTheCarriedState` and
`RoomNodeRepublishesTheSameSceneWithoutAResidual`, bounded at -50, -58 and -120 dBFS. The
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
blend of the two steady renders, where the input split measures the -52.1 dBFS of the table
above.

The modal bank has its own state, two banks of it, and a published set is adopted under a match
rule shaped like the early renderer's `carry_tap`. `RoomNode::same_resonators` compares the
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
count would pass `kMaxModes` and the solver lowered it (spec §9a.4), and Eyring's figure itself
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
the 20 to 100 Hz band is 14.1 dB louder with the listener in the corner at (0.15, 0.15, 0.15)
than at the centre, where every mode with an odd index is at a node; the spec asks for 12 dB.

Two things this layer does not do yet, both outside this directory:

- **The seam is not level-matched.** Spec §9a.4 asks the octave under f_s and the octave over
  it to agree within 2 dB. Measured as mean power per hertz on the garage: -2.04 dB under and
  -8.79 dB over at the pose above, a 6.75 dB step, and -1.64 and -6.66 dB averaged over eight
  poses, a 5.0 dB step. The modal octave is scaled by `ModalSolver` to Eyring's diffuse field
  and the tail's level is `LateReverbDesign`'s; `RoomNodeIsContinuousAcrossTheSchroederFrequency`
  records the step and skips, its bound still at 2 dB, until the match is settled. Part of the
  step is the pair itself: an LR4 pair sums flat only when both halves carry the same signal.
  The modal field and the geometric field are not the same signal, so at f_s, where each half
  is 6 dB down, their powers add to 3 dB down rather than to flat.
- **The modal ears stand at the listener.** `ModalSolver` evaluates the ear gains at
  `listener_position`, while every set is built for the microphone listener whose geometric
  layers stand at `microphone_position`. The two agree only when the scene puts them together,
  which the tests do; the listener choice of the next phase resolves it.

## What each set renders today

Every published set is built for the microphone listener, whatever the `listener` parameter
holds: the ears need the Ambisonic decoder and the direct HRIR path, which land with the binaural
phase. The microphone's encode is channel 0 alone, so `process()` sums channel 0 of the early
accumulation buffers with the tail, adds each ear's modal field and writes the left and the right
sum to the two outputs. `listener` and `direct_delay` are declared so the parameter table never
reorders; `latency_samples()` returns 0 until the decoder adds its partition.

The (0, 0, 0) image is the shortest path of every solve, so it is tap 0 of the set the solver
returns. For the microphone that tap is the direct sound and stays. For the ears the direct sound
is rendered through the HRIR pair, so `RoomNode::remove_direct_image` drops tap 0 from a set built
for the ears before it is published; the invariant is that a published ears set carries
reflections only.

## Sizes fixed at `prepare()`

The input ring holds the longest crossover time the scene bounds allow, `(4 + 1) * 12.0 / 343` s,
8397 frames at 48 kHz. The tail's lines hold 0.525 s, 25200 frames at 48 kHz, the longest line
an RT60 of 3 s needs. The accumulation buffers are two sets of sixteen channels of one block; the
two modal banks hold `kMaxModes` resonators each and one block of lane partials, and the node
keeps four copies of `ModalDesign`, two per bank plus the ramp's ends, beside its two tap sets.
The spherical-head HRTF is built at the prepare rate; a measured set installed through `set_hrtf`
at another rate is dropped for the fallback at the next `prepare()`.

## Approximations, named as such

- **The piston array.** `CabinetDirectivity` is four rigid pistons in an infinite baffle, and the
  cone's effective radius shrinks above the 2.5 kHz breakup frequency in `kCabinet4x12`. No cone
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
- **Six materials.** `Materials.hpp` ships unpainted concrete, painted concrete block, gypsum
  board on studs, heavy carpet on concrete, a glass window and the open doorway, each as six
  octave-band absorption coefficients with a zero-phase reflection factor. Spec §4.2 lists
  fourteen; the other eight are rows to add, not code.
- **Modal damping at a soft boundary.** `ModalSolver` damps each mode by first-order
  perturbation, right to a few percent for concrete, brick and glass and about 30 % wrong in the
  decay of a mode bounded by gypsum on studs at 125 Hz; the frequency is right in both cases.
