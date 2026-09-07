# Known issues

Defects the team has measured, understood, and deliberately left open, with the test that
documents each one and what closes it. This is not a bug tracker for the unmeasured; an entry
here has a figure, a mechanism, and a named exit.

## Alias floor at 8× after volt-true grid staging

The JCM800 measures −6.7 dB through `measure::alias_floor()` at preamp 1.0 and a −12 dBFS input,
against the −33.8 dB bound (−43.4 dB before DA4). DA4 removed the level-staging devices that used to keep every grid off
its clipping bound, so each stage now clips at the level its own circuit sets, and the 8×
oversampled region folds the extra harmonics that clipping produces. The same mechanism applies
to all four amplifiers, since all four share `TriodeStage` and lost their staging in the same
phase. The ledger's readings point at which stage folds: adding the two documented circuit
losses *raised* the floor, from −12.5 dB to −3.0 dB, so the harmonics come from stages leaving
hard limiting and producing more distinct products, not from clipping more deeply. Documented by
`Regression_Dsp.Jcm800AliasFloorImprovedAtLeast30dBOverD5b`
(`tests/functional/unit/test_measure.cpp`), skipped with the measured figure printed. Closes with
DA5's antialiasing work — antiderivative antialiasing in `TriodeStage`, or 16× oversampling.

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

## Spec §5's timeline alignment is not implemented

`RecordingBus::arm()` used to store the block index both files began at, and nothing read it: the
re-amp path in `RigController` starts a loaded take at transport position 0 whatever block it was
recorded at. Wiring it up needs a per-track start offset in `TrackPlayer`, the armed position
carried through the take file or the preset, and a preset field to hold it — more than the accessor
implied — so the fix round of 2026-09-07 deleted `armed_block_index()` rather than leave a dead
accessor that reads as a feature. Closes when a track carries its own start frame.
