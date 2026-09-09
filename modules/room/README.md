# `modules/room` — one cabinet in one room

One `IAudioNode`, mono in and two channels out, registered as `sushidsp.room` under the `Room`
category with four parameters: `mix`, `level`, `listener` and `direct_delay`. The node solves a
shoebox room on the control thread and renders its standing waves, its direct sound, its early
reflections and its late tail on the audio thread, for two ears under a head or for one
microphone. The module depends on `SushiDSP::sushi_dsp` alone.

Intent, the physics behind each solver and the acceptance list live in
[`docs/superpowers/specs/2026-09-08-room-acoustics-design.md`](../../docs/superpowers/specs/2026-09-08-room-acoustics-design.md);
the task order is
[`docs/superpowers/plans/2026-09-08-room-acoustics.md`](../../docs/superpowers/plans/2026-09-08-room-acoustics.md).
This file records what the code in this directory does. `docs/design/sushidsp.md` §3c places the
module in the tree and §9 carries its roadmap rows;
[`docs/reference/KNOWN_ISSUES.md`](../../docs/reference/KNOWN_ISSUES.md) holds what the phase
knowingly left open.

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
| Everything the audio thread reads for one scene | `RoomRenderSet`, `build_render_set` |
| What a host calls to install a scene or an HRTF | `IRoomHost` |
| One input ring, up to 128 fractional taps, two biquad state banks, sixteen channels out | `EarlyReflectionRenderer` |
| The 16-line Hadamard feedback delay network | `LateReverb` |
| The topology, the swap, the crossfade, the direct path, the ears, the mix and the level | `RoomNode` |

The direct path's renderer, the 128-tap FIR, the fixed frame delay and the fractional-delay ring
the ears need are nested classes of `RoomNode` (`DirectPath`, `FirFilter`, `FrameDelay`,
`DelayRing`); they are bricks in their own right and the day the module's build list may change
they move to files of their own beside `EarlyReflectionRenderer`.

`RoomNode` is the only class here that knows the topology. `ImageSourceSolver` and
`LateReverbDesign` consume the materials, the fits, the air and the directivity; the renderers
consume the records the solvers publish and nothing else, so a renderer is tested with a
hand-written record and a solver with arrival times alone.

## Which thread runs what

The audio thread calls `RoomNode::process`, and through it `CrossoverPair::process_sample`,
`EarlyReflectionRenderer::write`, `EarlyReflectionRenderer::render`,
`EarlyReflectionRenderer::clear_bank`, `LateReverb::process`, `ModalBank::process`,
`BinauralDecoder::process` and the nested `DirectPath`, `FirFilter`, `FrameDelay` and
`DelayRing`; at the block that adopts a set it also calls `CrossoverPair::set_crossover`,
`ModalBank::reset` and `BinauralDecoder::reset`, all arithmetic. None of them allocates, locks,
opens a file or throws after `prepare()`; `tests/functional/unit/test_rt_safety.cpp` traps
`operator new` around each of them, and around `RoomNode::process` while publishes land that
move the microphone, move the listener, widen the room, switch the listener, switch
`direct_delay` and re-install the fallback HRTF, so each path the node can take runs under the
trap.

Everything else runs on the control thread. `RoomNode::set_scene` validates the scene, calls
`build_render_set` for the listener the parameter names, which solves the images, designs the
tail, finds the Schroeder frequency, designs the modal bank and takes the direct HRIR pair from
the HRTF set, re-times an ears set for `direct_delay`, places the modal receiver, and publishes
the finished `RoomRenderSet` through one `RtSwapSlot`. A scene the validator refuses is not built
and the previous scene stays in place. `prepare()` and `set_hrtf` build a `BinauralDecoder` from
the HRTF set in place and publish it through a second slot; a set the decoder refuses, one with
no third-order decoder, is refused whole and the set in place stays. `reclaim_rooms()` frees
every set and every decoder the audio thread has moved past; the host calls it once per GUI
frame, as it does `reclaim_cabinets()`.

The `listener` parameter has two paths. `set_parameter("listener", …)`, the id path, is control
thread only: it stores the choice and, when the set published last was built for the other
listener, publishes a new one through the path `set_scene` uses. `set_parameter(index, …)`, the
index path `RigGraph` drains on the audio thread, stores the choice and nothing else; the next
publish, whatever triggers it, builds for the stored choice.
`RoomNodeAppliesAListenerWrittenByIndexAtTheNextPublish` checks both.

`process()` does one `acquire_for_audio_thread()` per slot per block. The set pointer it gets
back is compared against the one adopted last and read inside that block only: the taps, the
direct tap and its pair, the tail's design and the diffuse pair are copied into the node's own
banks the first block a set is seen, so no set is ever read after the block that acquired it, and
a reclaim can free a retired set at any time. The decoder pointer is used inside the block alone.

## The two-bank crossfade

`EarlyReflectionRenderer` holds two banks of biquad state and `RoomNode` holds two copies of
`EarlyTapSet`, one per bank. A set that is not the one adopted last goes into the spare bank: its
taps are copied in, the bank's state is zeroed, every tap that matches its retiring counterpart
takes that tap's filter state through `carry_tap`, and a 512-frame raised-cosine fade starts.
During the fade both banks render every block, the retiring bank's sixteen channels are blended
into the arriving bank's, and when the fade ends the retiring bank is the next spare. A set that
arrives mid-fade restarts the fade from the bank that was arriving; the node never holds more than
two. The direct path has two banks of its own, loaded and carried under the same match rule as the
taps and blended after rendering. The tail's coefficients are not crossfaded: `LateReverb` adopts
a design at the block the node hands it over and runs its own 512-frame fade only when the line
lengths move; its delay to t_c, though, is read out of the node's ring and blended between the
retiring and the arriving delay under the fade, so a re-timed set never jumps the tail's input.

When the ears take part in a fade, the fade starts one decoder partition, 128 frames at 48 kHz,
after the set is adopted, when the decoder's first partition of the arriving set comes out. The
paths the decoder or the alignment delay hold back read the fade's window unlagged, the paths
that blend at the output, the direct pair, the microphone's channel 0, the tail's read delay and
the presentations of the tail and the modal field, read it at the lag, and the tail's
coefficients switch at the block the lag runs out; so every path comes out in step and nothing at
the output moves before the fade begins. A fade between two microphone sets has no lag. The
ears' reference blend in the tests is delayed the same 128 frames.

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
the 20 to 100 Hz band is 14.0 dB louder with the ears in the corner at (0.15, 0.15, 0.15) than
at the centre, where every mode with an odd index is at a node; the spec asks for 12 dB.

### The level the two layers meet at

Both layers are scaled to one reference, `DiffuseField`: the room's steady-state reverberant
power per hertz of a white input, relative to the cabinet's on-axis direct sound at 1 m, which is
16 pi RT60(f) / (0.161 V) over the cabinet's directivity index, per band and linear in log
frequency between the centres (spec §6.3). `DiffuseField.hpp` is header-only; it caches the
directivity index of the last cabinet geometry behind a mutex, since the 2562-direction integral
costs 18 ms against a 2 ms build, both measured on an unoptimised binary, and a drag never
changes the geometry. Its cases live in
`tests/functional/unit/test_modal_solver.cpp` (`DiffuseFieldIsEyringOverTheCabinetsDirectivity`)
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
`round(t_c fs)`, and the sixteen still sum to `kOverlapRatio` times the mid-band RT60, which is
what fixes the tail's modal density (spec §6.2, three modes per resonance bandwidth). The
spread that satisfies the second given the first is bisected; the garage's is 5.836, its lines
run 1753 to 10211 frames and total 78098 against the 78003 the rule asks for. A room whose handover time exceeds a sixteenth of its
delay budget cannot start there, and then the budget sets the shortest line at the old spread
of 2 and the input delay carries the rest. `FdnDesign::kMaxLineSeconds` caps the longest line,
so a room past the design's 3 s RT60 loses density rather than overrunning the renderer's
storage.

The garage's impulse response now runs -12.4 dB at 5 ms for the direct sound, the reflections
down to -27 dB by 35 ms, the tail from -22.4 dB at 35 ms and a smooth decay past that; the
bedroom and the block room hand over at 37.98 and 39.35 ms against their own t_c of 37.90 and
39.36 ms.
`LateReverbDesignDelaysTheTailToTheHandoverTime` walks all three and holds the onset within 8
frames of t_c. A compensated ears set moves the tail's read delay earlier with everything else,
but the shortest line is latency the design cannot give back, so the delay floors at zero and
the ears' tail sits up to the propagation delay late, 363 frames for the default scene.

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

## The listener, and where the standing waves are read

`listener` 0 is the ears and 1 the microphone, and one of them renders at a time (spec §8). The
set is built for that listener: the image solver ends its paths at `listener_position` or at
`microphone_position`, encodes each tap to sixteen Ambisonic channels for the ears or to channel
0 alone for the microphone, folds the microphone's pattern and proximity into the microphone's
taps, and the tail's design carries the coherence pair for the ears and unity for the microphone.

The modal field is read where that receiver stands. `ModalSolver::design` takes the listener
kind: for the ears it evaluates psi at two points 8.5 cm to either side of `listener_position`,
for the microphone at `microphone_position` in both ear gains, and the resonators, the count,
the crossover and the level are the room's whatever the receiver
(`ModalSolverReadsTheMicrophoneAtOnePoint`, `BuildRenderSetReadsTheStandingWavesAtTheSetsReceiver`).
Until 2026-09-09 the solver knew only the listener and `RoomNode::place_modal_receiver` rewrote a
microphone set's gains after the build; that rule and its count check are gone, since nothing
is rewritten any more.

The microphone fills both outputs with one signal: channel 0 of the early set, the tail read
once, and the modal field at one point, so the two channels are equal bit for bit whatever the
listener does (`RoomNodeMicrophoneListenerFillsBothChannelsIdentically`), at physical timing and
with no latency. The ears hear the binaural path of the next section.

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
`IRoomHost::set_hrtf`; `hrtf_source()` says which. No measured asset is in the tree yet, and the
host does not load one yet either: `apps/host_gui` rebuilds its nodes on every structural edit
and re-applies a slot's cabinet at each rebuild, so an HRTF installed once from `main.cpp` would
be lost at the next edit; the load belongs beside the cabinet path in `RigController`, with the
`assets/hrtf` copy and the room link in the host's build list, and none of that is in this
directory.

**The direct path.** Tap 0 of an ears set, the (0, 0, 0) image, is taken out of the bank at the
block the set is adopted (`RoomNode::take_direct_image`) and rendered by `DirectPath`: read out
of the node's ring at the alignment delay plus the tap's, through the tap's fitted filter and 1/r
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
491 for the default scene's 2.596 m, the 363 frames the plan's 280 assumed a 2 m scene for
(`RoomNodeDefaultsDirectDelayToCompensated`). `RoomNodeDirectOnsetMovesWithTheDirectDelayChoice`
sees the direct onset and the floor image's onset both move by 280. The choice is an ears
choice: the microphone renders at physical timing with no partition in its path and reports 0,
whatever the parameter says. A changed choice publishes through the crossfade like a changed
listener; the index path stores it for the next publish.

Measured cost at 48 kHz and 256 frames on a Release build, best of 200, the garage with the
cabinet at (0.4, 0.5, 0.5) and the listener at (5.4, 3.4, 2.0), every layer running
(`tests/benchmark/main.cpp`):

| Row | ns/sample | One core | Per 256 frames |
| --- | --- | --- | --- |
| `RoomNode`, ears, steady state | 931.6 | 4.47 % | 0.238 ms |
| `RoomNode`, ears, a scene republished every block | 8495.7 | 40.78 % | 2.175 ms |
| `RoomNode`, microphone, steady state | 440.2 | 2.11 % | 0.113 ms |
| `EarlyReflectionRenderer::render` | 207.0 | 0.99 % | 0.053 ms |
| `ModalBank::process` | 153.5 | 0.74 % | 0.039 ms |
| `BinauralDecoder::process` | 350.0 | 1.68 % | 0.090 ms |
| JCM800 into `RoomNode` | 3490.2 | 16.75 % | 0.893 ms |
| the room's share of that chain | 936.7 | 4.50 % | 0.240 ms |

The swap row is a bound and not a working figure: it republishes a scene on every block, which
nothing does. A slider drag republishes at 30 Hz, one block in six at this rate and block size,
and the blocks between it run the steady-state row. Spec §10 budgeted 1.63 ms and 2.57 ms scalar
for the ears, and the 0.893 ms the whole chain costs is what leaves DE7's AVX2 lanes unopened.

## Sizes fixed at `prepare()`

The early renderer's ring holds the longest crossover time the scene bounds allow,
`(4 + 1) * 12.0 / 343` s, 8397 frames at 48 kHz; the node's own ring, which the direct path and
the tail read, holds that plus the alignment delay plus 128 frames for the far ear's ITD, rounded
up to 16384. The tail's lines hold `FdnDesign::kMaxLineSeconds`, 0.525 s or 25200 frames at
48 kHz, which is also the ceiling the designer spreads the lines under. The accumulation buffers are two sets of sixteen channels of one block; the two modal
banks hold `kMaxModes` resonators each and one block of lane partials, and the node keeps four
copies of `ModalDesign`, two per bank plus the ramp's ends, beside its two tap sets, its two
direct banks with their four 128-tap histories, the two diffuse filters, the four alignment
delays and one copy of `FdnDesign`. The decoder's 32 convolvers, 256 taps in two partitions of
128, live in the decoder the slot holds. The spherical-head HRTF and its decoder are built at
the prepare rate; a measured set installed through `set_hrtf` at another rate is dropped for the
fallback at the next `prepare()`.

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

Eleven entries in [`docs/reference/KNOWN_ISSUES.md`](../../docs/reference/KNOWN_ISSUES.md) belong to
this module, each with its figure and its exit: no measured HRTF set ships, so front and back
sound alike; eight of the spec's fourteen material rows are untranscribed; the SM57 preset has no
measured curve; modal damping at a gypsum boundary is about 30 % wrong; the FDN's own modes below
f_s are crossed out rather than corrected; the raw octave step at the seam is recorded and not
bounded; `BandResponseFit` stops at eight steps on about three fits in five; a structural rig edit
rebuilds the node and empties its tail; the room panel cannot edit a patch; a compensated ears
set's tail floors at zero delay and lands 7.6 ms after its direct sound; and a 12 m concrete
cube, which the scene validator allows, rings to +36 dB at its 32 Hz mode with nothing between
the node and the device to bound it. Two more, the
converter's unweighted diffuse-field average and the tail's output filter stepping on its running
state, were written when the change that caused them landed.
