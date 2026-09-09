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

DS5 of `docs/superpowers/specs/2026-09-07-stereo-tracks-design.md` puts `libvorbis` and `libogg`
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
