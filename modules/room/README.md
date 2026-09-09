# `modules/room` — one cabinet in one room

One `IAudioNode`, mono in and two channels out, registered as `sushidsp.room` under the `Room`
category with four parameters: `mix`, `level`, `listener` and `direct_delay`. The node solves a
shoebox room on the control thread and renders its early reflections and its late tail on the
audio thread. The module depends on `SushiDSP::sushi_dsp` alone.

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

The audio thread calls `RoomNode::process`, and through it `EarlyReflectionRenderer::write`,
`EarlyReflectionRenderer::render`, `EarlyReflectionRenderer::clear_bank` and
`LateReverb::process`. None of them allocates, locks, opens a file or throws after `prepare()`;
`tests/functional/unit/test_rt_safety.cpp` traps `operator new` around each of them, and around
`RoomNode::process` while a publish lands.

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
taps are copied in, the bank's state is zeroed, and a 512-frame raised-cosine fade starts. During
the fade both banks render every block, the retiring bank's sixteen channels are blended into the
arriving bank's, and when the fade ends the retiring bank is the next spare. A set that arrives
mid-fade restarts the fade from the bank that was arriving; the node never holds more than two.
The tail is not crossfaded by the node: `LateReverb` adopts a design's coefficients at the block
it sees them and runs its own 512-frame fade only when the line lengths move.

The fade hides the switch of taps, not the arriving bank's start from zero state. Measured in the
garage with a 1 kHz full-scale tone and the microphone moved 0.36 m, the output during the fade
differs from a raised-cosine blend of the two steady renders by 0.029 peak, -30.8 dBFS, and by
-36.2 dBFS when the same scene is republished, where the two steady renders are identical and the
whole residual is the zero-state transient. The slowest pole among the garage's tap filters has
radius 0.99918, a time constant of 1220 frames, so the transient outlasts the fade by a factor of
sixteen. `RoomNodeCrossfadesAPublishWithoutAStep` asserts the -60 dBFS figure spec §5.3 asks for
and fails at these numbers; the same case's step bound, no sample-to-sample step beyond the steady
signal's own, passes at a ratio of 0.9994.

## What each set renders today

Every published set is built for the microphone listener, whatever the `listener` parameter
holds: the ears need the Ambisonic decoder and the direct HRIR path, which land with the binaural
phase. The microphone's encode is channel 0 alone, so `process()` sums channel 0 of the early
accumulation buffers with the tail and writes the same signal to both outputs. `listener` and
`direct_delay` are declared so the parameter table never reorders; `latency_samples()` returns 0
until the decoder adds its partition.

The (0, 0, 0) image is the shortest path of every solve, so it is tap 0 of the set the solver
returns. For the microphone that tap is the direct sound and stays. For the ears the direct sound
is rendered through the HRIR pair, so `RoomNode::remove_direct_image` drops tap 0 from a set built
for the ears before it is published; the invariant is that a published ears set carries
reflections only. The modal seam, `RoomRenderSet::crossover_hz`, is zero when the modal layer is
off and is not read by the node yet; the modal layer and the crossover pairs are the next phase.

## Sizes fixed at `prepare()`

The input ring holds the longest crossover time the scene bounds allow, `(4 + 1) * 12.0 / 343` s,
8397 frames at 48 kHz. The tail's lines hold 0.525 s, 25200 frames at 48 kHz, the longest line
an RT60 of 3 s needs. The accumulation buffers are two sets of sixteen channels of one block. The
spherical-head HRTF is built at the prepare rate; a measured set installed through `set_hrtf` at
another rate is dropped for the fallback at the next `prepare()`.

## Approximations, named as such

- **The piston array.** `CabinetDirectivity` is four rigid pistons in an infinite baffle, and the
  cone's effective radius shrinks above the 2.5 kHz breakup frequency in `kCabinet4x12`. No cone
  is a rigid piston; the model gives the off-axis roll-off its shape, not a measured polar plot.
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
