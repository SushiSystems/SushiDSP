# Known issues

Defects the team has measured, understood, and deliberately left open, with the test that
documents each one and what closes it. This is not a bug tracker for the unmeasured; an entry
here has a figure, a mechanism, and a named exit.

## Alias floor at 8× after volt-true grid staging

The JCM800 measures −23.4 dB through `measure::alias_floor()` at preamp 1.0 and a −12 dBFS input,
against the −33.8 dB bound (−43.4 dB before DA4, −6.7 dB before DA5 part 1, −15.1 dB before
part 2, −13.4 dB before part 4), and −24.9 dB at its default (−16.3 dB before part 4); the 5150
at its default and the same input measures −26.8 dB against the spec's −30 dB (−11.4 dB before
part 4), the IIC+ −14.8 dB (−0.1 dB) and the VH4 −23.0 dB (−13.3 dB). DA5 part 4 antialiased
every `TriodeStage` and `PentodeStage` with a first-order antiderivative of the composite
grid-and-plate transfer (`AntiderivativeAntialiaser`, spec section 15): one stage alone now folds
at −102.4 dB at 3× past its window and −79.8 dB at 12× (−81.2 and −53.7 dB before), and the
EL34 at −101.2 and −84.4 dB (−88.1 and −55.1 dB). What remains in the amplifiers is not the
transformer's tanh, the sag envelope or the solver's iteration count, each measured and ruled out
(`.superpowers/sdd/amp-gain-staging/da5-part4-report.md` section 5): at every default the worst
line is the 20 kHz tone's 19th harmonic folding about 384 kHz to 3995 Hz, made by the stages
after the first, which receive an already-clipped waveform whose content reaches the oversampled
Nyquist and so breaks the linear-between-samples assumption first-order ADAA rests on. Documented
by `Regression_Dsp.Jcm800AliasFloorImprovedAtLeast30dBOverD5b` and
`Regression_Dsp.Peavey5150AliasFloorAtItsDefaultIsUnderMinusThirtyDb`
(`tests/functional/unit/test_measure.cpp`), both skipped with the measured figure printed. 16× was
measured without ADAA in part 3 (JCM800 default −35.0 dB, 5150 −19.9, IIC+ −14.3, VH4 −27.3 at
1.9 to 2.0× the CPU); 16× on top of the antialiaser, measured in part 4, reads JCM800 default
−45.3 dB, preamp 1.0 −32.7 dB, 5150 −36.2 dB, IIC+ −36.9 dB and VH4 −35.3 dB at 2.0× the CPU,
which puts the 5150 and the IIC+ at 167 % of the block at `-O0` and 99 to 102 % at `-O2`. What
closes the rest is the maintainer's: that 16×, a band limit between the stages that lets 8× stay,
or a higher-order antiderivative.

## The IIC+ and the 5150 leave little headroom at the smallest device buffer

`sushidsp_bench` measures the IIC+ at 7236 ns per output sample and the 5150 at 6286, against
the JCM800's 2587 and the VH4's 3875, so a 256-frame block costs the IIC+ 1.852 ms against the
1.5 ms budget that block is allowed at 48 kHz. The cost is the valve count solved at 8x: the
IIC+ runs six single triodes, a long-tailed pair and four power tubes, and a single `TriodeStage`
at the base rate measures 69.9 ns. Nothing else in a guitarist's chain is close - the Tube
Screamer costs 594.9 ns and a 4096-tap cabinet convolution 15.6 ns. Per-sample cost does not move
with the block size (7356 ns at 16 frames, 7243 at 256), so a small buffer adds no penalty of its
own; what a small buffer removes is the slack that absorbs a slow block, and the same node
measures 34.8 % of one core at its best and 57.6 % at its worst. A Tube Screamer into the IIC+
into a cabinet therefore runs near 38 % of one core steady and past 60 % on a slow block, which
underruns at the smallest USB buffer while leaving a larger one alone. What closes it is a
decision the maintainer owns: dropping this model to 4x, which halves the cost against the alias
floor the entry above measures, or a cheaper triode solve. Measured by `sushidsp_bench`
(`tests/benchmark/main.cpp`), which times each amplifier node.

## Removing a track can double one block of audio

`SessionTimeline::remove_track` republishes lanes *index* through *n-1* one at a time, each
through its own `RtSwapSlot`, so the removal is not atomic across lanes. A callback that lands
between two of those publishes renders one block in which lane 3 already holds lane 4's clips
while lane 4 still holds its own, and that block carries the same clip twice. Nothing on the
audio thread allocates, locks or throws, and no memory is raced; what the seam costs is one
block, 5.33 ms at 48 kHz and a 256-frame buffer. What closes it is a generation counter the
callback reads once per block, or refusing the removal while the transport runs; both are the
maintainer's call, and neither is worth doing before the removal is something people use.

## Mesa Mark IIC+ has no clean setting

The dossier's §2 places an EFFECTS SEND / RETURN network between V2A's plate and V2B's grid,
and its §3.7 gives no value for that link, so the model has no loop network at all — neither the
series path nor the shunt one, of which the missing return capacitor's 7.23 Hz corner is only a
part: V2A's plate drives
V2B's grid window about twenty-one times past it, and the node limits at every input level with
no clean end on the knob. Three tests are skipped, not weakened: `MesaMark2cHasACleanEndAtMinimumGain`,
`MesaMark2cToneStackSitsBeforeTheLeadStages` and `MesaMark2cEqBandBoostsItsCentre`
(`tests/functional/unit/test_mesa_mark2c.cpp`), each printing what it measures today against
what it will re-assert. Closes when the send/return network is read off the schematic — dossier
open questions 19 and 20.

## Diezel VH4's phase inverter and power section are a JCM800 stand-in

The dossier establishes nothing about the VH4's phase inverter or power section (§5: "nothing
established"), so the module keeps the JCM800's long-tailed-pair degeneration and power-tube
grid figures in their place rather than a VH4-specific derivation. Every number downstream of
that link is borrowed, not measured from the VH4 itself. Closes when a VH4 power-amp schematic
becomes available.

## OGG export is deferred, so a take leaves the host as WAV only

DS5 of `docs/archive/superpowers/specs/2026-09-07-stereo-tracks-design.md` puts `libvorbis` and `libogg`
inside `apps/host_gui` and nowhere else, and DS0 through DS4 shipped without it. A five-minute
stereo take at 48 kHz is 115 MB of 32-bit float on disk, which is what the host writes and what a
user copies out. No test is skipped for this; nothing is broken, the phase is unwritten. Closes
with DS5.

## DS0, DS2 and DS3 are committed under each other's names

Three sessions worked this checkout at once and each committed by explicit path, so 394a605 ("DS0:
RigGraph routes as many channels as its nodes declare") also carries Task 5's `Transport`,
`TrackPlayer` and `test_track_player.cpp`, and 18eb040 ("DS2: Take, TrackRecorder and
RecordingBus") carries the rest of that task. The content of every file is the one its own task
wrote and reviewed; only the commit each landed in is wrong. The ledger
(`.superpowers/sdd/2026-09-07-stereo-tracks/progress.md`, "Task 5") ruled against rewriting
history in a shared checkout, so `git log` stays as it is and this entry is the record. Closes by
reading the ledger, not by a repair.

## `write_rig_output_to_device`'s 2 into 1 downmix has no live caller

`apps/host_gui/main.cpp` prepares the rig at `min(device output channels, RigGraph::kMaxChannels)`,
so the rig is never wider than the device and the sum-and-halve branch of
`write_rig_output_to_device` cannot run from the host. The branch exists because
`ChannelSelection` carries the whole `RigGraph::channels_adapt` table rather than the part one
caller happens to reach, and
`WriteRigOutputSumsAndHalvesTwoRigChannelsIntoAMonoDevice`
(`tests/functional/unit/test_channel_selection.cpp`) is the only thing that exercises it. It stops
being dead the first time a rig's width is set by something other than the device — a fixed stereo
rig, or a preset that pins the width.

## The ASIO host still renders the rig in mono

`apps/host_asio/main.cpp` opens two ASIO output channels but prepares its rig with
`PrepareInfo{..., 1}`, so a `PanNode` at the end of a rig run under that host is summed back to one
channel by the graph's own output stage. It shares the GUI host's `rig_output_adapts_to_device` and
`write_rig_output_to_device` pair, so the mapping is the same rule; only the rig's own width differs.
The chain it builds is a fixed mono TubeScreamer into a JCM800 and nothing in it pans, so the limit
costs nothing today. Closes by preparing the rig at the device's width, as the GUI host does.

## `Transport::seek` racing `advance` can be lost

`seek()` and `advance()` both store to `position_`, so a seek issued while the audio thread is
inside a block can be overwritten by that block's store, leaving the transport a block-length away
from where the user put it. The host serialises the two, and `Transport::seek`'s `@pre` states the
rule rather than paying for a generation counter no caller needs today. Raised as finding 9 of
`.superpowers/sdd/2026-09-07-stereo-tracks/review-tasks-1-4-5.md`. Closes when a caller appears
that cannot serialise them.

## A stereo branch collapses back to mono at a `MixerNode`

`routing::MixerNode` declares `PortLayout{8, 1}`, so `RigGraph::channels_of` reads its input width
as 1 and the graph's gather averages any two-channel producer down to one channel before the mixer
sees it. A `PanNode` inside a parallel branch therefore loses its width the moment the branch
rejoins. This is what spec §2's adaptation table says should happen — 2 into 1 sums and halves —
and no test or user asks for anything else, so the behaviour stays. Closes when a mixer that keeps
its inputs' width exists, which needs a `PortLayout` that can say "eight slots, two channels each".

## A take's own sample rate is never read

`Take` carries the rate it was captured at, and neither `TrackPlayer` nor `TrackTimeline` asks
for it: a clip is read frame for frame against the rig's rate. A 44.1 kHz WAV dropped on a
48 kHz rig therefore plays 8.8 % slow and a semitone's fifth flat. Imported mp3 files are not
affected — `AudioImportCache` converts a decoded song to the rig's rate before the clip ever
points at it, which is why the defect survived the mp3 work. Closes when a clip carries the
ratio between its take's rate and the rig's into `VariRateReader`, which already reads at an
arbitrary speed.

## The imported-audio cache is never pruned

Every mp3 dropped on a lane leaves a float32 WAV under the executable's `cache/imported_audio/`
and nothing ever deletes one; a 3.4-minute song costs about 79 MB. The cache is what makes a
second drop of the same file cost 0 ms against the first drop's 3128 ms, and what lets a saved
session find its clip again, so it is kept rather than made optional. Closes when the cache
carries a size cap and evicts by last use.

## The room over-predicts decay against a measured room by 1.1 to 1.8x

`tests/functional/unit/test_room_motus.cpp` builds the Motus room, 4.9 x 4.4 x 2.9 m of concrete
and 61.13 m3 (Zenodo 4923187), from the module's own material rows and compares its T30 against
the measured figures in `tests/fixtures/motus_reference.csv`. Empty, the model reads 3.123, 3.856,
3.180, 3.429, 3.410 and 2.080 s from 125 Hz to 4 kHz where the room measures 2.572, 2.147, 2.009,
2.119, 1.958 and 1.508 s, a ratio of 1.214, 1.796, 1.583, 1.618, 1.741 and 1.380. With a carpet
down the ratios read 1.105, 1.689, 1.644, 1.461, 1.224 and 0.975, and the carpet's own reduction
comes out larger in the model than in the room: 39.2 % against 32.6 % at 1 kHz, 58.2 % against
40.4 % at 2 kHz, 53.2 % against 33.8 % at 4 kHz. The direction is what a tabulated coefficient
predicts. A published table is an idealisation of one mounting in one chamber, and a real room
also carries fixtures, cable trays, door and window seams, leakage and edge diffraction that six
octave numbers per surface do not hold; the empty concrete bedroom of Pettersson et al. reads its
absorption 1.5 to 2 times the tabulated figure the same way. The test bounds the model at twice
the measurement and prints the rest, so the disagreement is recorded rather than tuned away.
Closes on the maintainer's decision about a floor of absorption or a correction term
(`docs/design/ROOM_FURNISHINGS.md` section 11), which changes every decay the module reports and
so waits on more reference rooms than this one.

## Four furnishing and covering rows carry no measurement of their own

`docs/agent/2026_09_22_ROOM_FURNISHINGS/REPORT.md`'s "What was not found" records that no ISO 354
or JIS A 1409 test of a household desk, a double bed with its bedding, a filled bookcase or a
flat-woven kilim on a hard floor could be sourced. `Furniture.cpp`'s desk row is therefore the
Korean apartment study's 0.0131 m2 per m3 taken at 30 m3, 0.4 m2 flat across the bands, and its
bookcase row is EN 12354-6 formula 4 on an assumed 0.8 x 0.3 x 1.8 m volume, 0.57 m2 flat; the
double bed is the one measured single bed without bedding scaled by the mattress perimeter ratio
1.22; and a rug enters a scene as `ThinCarpetOnConcrete`, a fitted woven carpet row standing in
for a kilim and probably an upper bound. Each row's citation names its kind, so nothing is
presented as measured that is not, and the room-level cross-check in the same report puts the sum
inside the 0.16 to 0.35 m2 per m3 the furnished-room studies measure. Closes when a per-object
measurement of each is published or made.

## No measured HRTF set ships, so front and back sound alike

`assets/hrtf/sadie2_ku100_48k.hrtf` is not in the repository, and `tools/dsp/convert_hrtf.py`
needs a SOFA or WAV release plus numpy and scipy to make one, so every rig runs the fallback
`SphericalHeadHrtf`. A rigid sphere has an interaural time difference and a head shadow, both
right to within the sample the spec allows, and no pinna: the front and the rear of the cone of
confusion give the same pair, so a reflection off the wall behind the listener is not heard as
behind. Its third-order decoder is the sampling sum of §9.4, not a magnitude-least-squares fit,
and above about 1.5 kHz it sums a lobe of directions whose delays differ by up to one ITD, so
reflections and the tail come out combed against the direct sound. Two cases skip for want of the
file and print the path they looked at: `RoomNodeDecoderMagnitudeMatchesTheMeasuredSet` and
`RoomNodeCarriesTheRearNotchWithAMeasuredSet` (`tests/functional/unit/test_room_node.cpp`).
Closes when a set with an Apache-2.0-compatible licence is converted into the tree;
`assets/hrtf/README.md` states the layout and the licence bar.

## Four of the spec's fourteen material rows are not transcribed

`Materials.hpp` ships fifteen rows, nine of them added on 2026-09-22, and §4.2's brick, acoustic
foam, steel garage door and carpet on underlay are not among them. A user whose garage has a
steel door or a foam panel picks the nearest row instead, and the room's decay is wrong by the
difference between the two, which across §4.2's table reaches 0.4 in absorption at 500 Hz.
Nothing is skipped; the code takes any row the table holds. Closes by transcribing the other four
from the same source with a citation each, which is data and no code.

## The SM57 preset has no measured curve

`MicrophoneModel` evaluates any on-axis curve it is handed, and the module ships one preset,
`kMicrophoneFlatCardioid`, whose six band values are zero and whose citation reads "no maker's
curve". Spec §8 asks for a Shure SM57: a cardioid with a presence rise near 6 kHz and a low-end
roll-off, which the flat preset does not have, so a microphone render is missing the colour the
capsule itself contributes. The six numbers were not invented, which is why the preset says so in
its own citation. Closes when the SM57's published frequency response is read off Shure's sheet
into a second `MicrophonePreset`.

## Modal damping at a soft boundary is about 30 % wrong

`ModalSolver::damping_constant` damps each mode by the first-order perturbation of spec §9a.2,
which is linear in the absorption coefficient and so right to a few percent while α stays under
about 0.1. Gypsum board on studs absorbs 0.29 at 125 Hz (`Materials.cpp`), and a mode bounded by
it decays about 30 % away from the true rate. The mode's frequency is right either way, so the
room's pitch is right and its low-end decay time is not. Solving the damped eigenproblem against
a complex wall impedance costs what §9a.2 measures FDTD at, for an error inside a layer that did
not exist before 2026-09-08. Closes when someone judges that cost worth paying.

## The FDN's own modes below the Schroeder frequency are crossed out, not corrected

A 16-line feedback delay network has its own modal structure, and below f_s that structure is not
the room's. `CrossoverPair` low-passes the modal bank and high-passes the tail at f_s in both
quality modes, so the FDN carries no low end and its line lengths are designed for the band above
f_s alone. The room's low end therefore comes entirely from `ModalSolver`'s eigenmodes, and the
tail contributes nothing under f_s that a longer reverberation would fill in. This is the design's
choice rather than a defect to repair, recorded because a reader measuring the tail alone below
f_s will find it empty and should not go looking for the bug.

## A compensated ears set's tail lands after its direct sound

`RoomNode::compensate_direct_delay` moves an ears set earlier by the propagation delay, and it
moves the tail's read delay with everything else. The network's shortest line is the handover
time, so that delay is already zero for all three reference rooms and the subtraction floors
there: the tail keeps its physical timing while the rest of the render moves up, 363 frames or
7.6 ms for the default scene. Below the 20 ms a listener resolves as a separate arrival, and
inaudible against the 70 ms hole the old timing left, so it is recorded rather than repaired.
Closes when the designer is told the compensation and shortens its lines to absorb it.

## Nothing between the node and the device bounds the output

A 12 m unpainted-concrete cube used to be a scene the panel could reach: 1728 m3, an Eyring RT60
of 12.8 s at 500 Hz, 1068 modes under f_s, a slowest modal decay of 48 s, and +36 dB of steady
gain through the node at its 32 Hz mode after 20 s with the cabinet and the capsule in one
corner, still rising. Since 2026-09-10 `RoomScene::is_valid` refuses it: the mean absorption of
every band has to clear the floor a 12 s decay asks of the room's volume over its area (spec
§4.1). The three reference rooms reach +9.7 dB, which is what a room booming at a mode does, and
the loudest a scene that now passes can get has not been measured.

What the gate does not do is police the audio. No stage between `RoomNode` and the device bounds
the output; `AsioSampleConverter::clamp_unit` clips at full scale for the integer formats and a
float format passes the value through. Whether a guard belongs there is the maintainer's call.

## The raw octave step at the seam is not bounded

`RoomNodeIsContinuousAcrossTheSchroederFrequency` measures each layer against the shared
`DiffuseField` reference and holds both within 2 dB, which is what the spec asks. The raw step
between the octave under f_s and the octave over it is recorded and not bounded: it reads
+0.16 dB for the garage, +1.29 dB for the bedroom and +3.75 dB for the block room. The step
cannot be zero, because the reference itself changes across the seam: the 4x12's directivity
index rises 4.6 dB between 250 and 500 Hz, the block room's absorption halves from 125 to 250 Hz,
and the poses a room is used at sit 2 to 4 dB under the volume average in the octave below f_s.
An earlier run recorded −0.3, +0.6 and +3.1 dB; those are the figures the Linkwitz-Riley pair
gave before `CrossoverPair` became power complementary, and each half then lost 2.4 dB at the
seam. Closes when a bound follows from the reference's own step rather than from a measurement.

## `BandResponseFit` stops at eight steps on about 60 % of fits

`kMaxSteps` in `BandResponseFit.cpp` bounds the refinement at eight accepted steps, and about
three fits in five reach it rather than converging, so those tap filters sit at whatever the
eighth step reached. The fit still holds the direct path within 0.1 dB and a one-band 20 dB cut
within 0.5 dB, so the cap is not what the module's accuracy is limited by today; it is what a
tighter tolerance would run into first. The refinement differentiates its residual numerically,
so each step costs six extra evaluations. Closes when the Jacobian is written analytically, which
buys a higher cap at the cost the fit pays now.

## A structural rig edit empties the room's tail

`RigController::publish()` rebuilds the graph from the slot list, so every node in the rig is
constructed afresh whenever a node is added, removed, connected or disconnected. The new
`RoomNode` gets its parameters and its scene back through `apply_room_to_live_node`, and its
delay lines, its FDN and its modal resonators start at zero, so the room's tail stops at the edit
and rebuilds over the next RT60. A drag inside the room panel goes through
`publish_room_scene` instead, which crossfades; only an edit to the graph's shape rebuilds the
node. Closes when a rebuild
carries surviving nodes across instead of reconstructing them, which is a `RigController` change
and touches no module.

## The HRTF converter counts every measured direction equally

`tools/dsp/convert_hrtf.py`'s `diffuse_field_average` sums the magnitude responses of the
directions a measurement file carries without weighting each by the solid angle it stands for,
while `SphericalHeadHrtf` weights its own grid by `4*pi*cos(e)*sin(delta/2)/36` per ring point.
On a latitude-longitude grid the two disagree: at ten degree spacing the rings near the poles
stand for about a fifth of the solid angle of a ring at the equator, so they carry up to 5.8
times the weight they should, and the tail through a converted set is coloured towards whatever
the poles measured. Nothing in the tree hits this today because `assets/hrtf/sadie2_ku100_48k.hrtf`
is not in the repository and the fallback set is what every rig loads. Closes when the converter
derives each direction's solid angle from the grid it finds in the file.

## A publish steps the tail's output filter on its running state

`LateReverbDesign` fits the tail's output cascade to what the early taps leave of the diffuse
field in each band, so a moved microphone or listener refits it, by 3 % in the second section's
coefficients for a 0.36 m move in the garage, and `RoomNode` loads the new coefficients into the
one `LateReverb` at the block the fade begins, on the network's stored energy. With the tail at
its diffuse-field level, 12 dB above where it sat before 2026-09-09, the residual against the
blend of the two steady renders reads -45.8 dBFS for that move where it read -52.1 dBFS, and the
ears' 0.3 m listener move reads -72.3 dB where it read -88.3 dB; the step ratios are unchanged,
so nothing clicks (`RoomNodeCrossfadesAPublishWithoutAStep`,
`RoomNodeRampsTheEarGainsAsTheOutputBlendWould`). Closes when the tail's output cascade is run
twice under the fade, the retiring coefficients and the arriving ones on the same line sum,
blended under the raised cosine, which is two more biquads per ear in `LateReverb`.
