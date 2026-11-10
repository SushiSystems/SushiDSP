# tests/applications/host_gui

## `test_audio_import_cache.cpp`

### `AudioImportCacheReusesTheWavItAlreadyHolds`

`// An untouched file proves that no decode ran.`

The source would fail to decode, so a returned path with its contents untouched is proof that no
decode was run.

## `test_channel_resampler.cpp`

### `ResampledFrameCountRoundsTheRatio`

`// 9093888 frames at 44.1 kHz are 9898109 at 48 kHz.`

1000 \* 48000 / 44100 = 1088.43..., and the 44.1 kHz song the cache was measured on is 9093888
frames long, which is 9898109 frames at 48 kHz.

### `ResampleChannelsKeepsTheWaveAwayFromTheEnds` and `ResampleChannelsKeepsTheWaveWithinTheEndErrorAtTheEnds`

The interior bound is two stopband levels of the kernel (-100.477 dB) plus two float roundings; the
tone is measured at 4e-6. The first and last samples read a filter that has silence on one side of
it, and stay within the characterised 0.035, which no derivation backs.

## The rig controller files

The tests of the rig controller sit in the folder `test_rig_controller/`, one file per subject:
`test_chain.cpp`, `test_bypass.cpp`, `test_preset.cpp`, `test_cabinet.cpp`, `test_tracks.cpp`,
`test_clips.cpp`, `test_room.cpp` and `test_logging.cpp`. The timeline cases are in
`test_session_timeline.cpp` and the selection cases in `test_slot_selection.cpp`. Every rig is built
on `tests/support/rig_products.hpp`, a fixture whose catalogue lists the products the cases name, so
no case reaches the process-wide node registry. The cases that load a take, place a clip or write a
preset file build their rigs and sessions on `tests/support/rig_products_in_memory_files.hpp`, which
adds the in-memory adapters of the audio file port and the text file port. The WAV writers of the
cases that still read a real file are in `tests/support/wav_fixture.hpp`.

## `test_audio_files.cpp` and `test_text_files.cpp`

The contract of the `AudioFiles` port (`applications/host_gui/audio_files.hpp`) and of the `TextFiles`
port (`applications/host_gui/text_files.hpp`). Each promise is one `expect_*` helper that takes the port,
run once on the in-memory adapter as `Small_AudioFilesContract` or `Small_TextFilesContract` and once on
the disk adapter as `Medium_AudioFilesContract` or `Medium_TextFilesContract`, in a file named after the
case. The cases the contract cannot state, such as refusing a path or an unreadable file, are in
`Small_InMemoryAudioFiles` and `Small_InMemoryTextFiles`. The expected samples are exact: 0.5, 0.25 and
0.125 are dyadic, and the 16-bit file holds a sample as lround(x * 32767) / 32768, which is 16384, 8192
and 32767 over 32768 for 0.5, 0.25 and a clamped 2.0.
## `test_rig_controller/test_chain.cpp`

### `BatchedEditsBuildASplitterTwoBranchMixerShapeThatDoublesABlock`

The split/mix shape needs publish batching to build: before the publish-batching parameter it was
impossible to construct through the public API at all, because the second add_node() would have
republished a two-node, zero-edge graph, which RigGraph::compile() rejects for having no sink. The
case builds the shape from routing nodes only. A splitter copies its input and the mixer sums its
inputs at gain 1 each (`gains_` starts at 1.0 and `process` accumulates `input * gain`), so a block
that enters the root leaves the sink as 0 + x + x, which is exactly 2x in binary floating point.
The case compares every sample bit for bit. The old case used a screamer and an amplifier in the
branches; their output has no closed form, so it asserted nothing about the block.

### `AddBranchBuildsTheParallelShapeTheGuiOffers`

The whole point of the feature, in the order a user performs it: drag a source, a splitter and one
effect into a straight line, then drop a second effect on the splitter's open row. add_branch() is
what turns that one drop into the four-edge shape RigGraph::compile() accepts, mixer and all.

### `AppendWithUnknownDescriptorIdKeepsTheSlotAndBuildsNoNodeForIt`

A slot whose descriptor id the catalogue does not list is kept when the graph publishes: it stays in
the slot order and keeps its descriptor id, the graph builds no node for it, and the node after it is
fed by the node before it.

## `test_rig_controller/test_bypass.cpp`

A bypassed slot keeps its place in the slot order, its parameters and its edges; only the published
graph leaves it out, wiring its upstream neighbours straight to its downstream ones.

## `test_rig_controller/test_preset.cpp`

### `SaveToJsonUsesOwnStoredValueNotLiveNodeState`

Deliberately no process() call and no further structural edit here: the live node still reads the
pre-edit default, but save_to_json() reports the controller's own stored value, matching what
get_parameter() and the GUI slider show.

### `AnUnknownSlotIsWrittenBackUnchanged`

A slot whose descriptor the catalogue does not list is saved at its place in the order with its stored
values and its edges, loaded into a second rig, and saved again: both documents hold the same node
types in the same order and the same stored value.

### `AnUnknownDescriptorIsWarnedOnceNamingItsId` (`test_logging.cpp`)

Three appends publish the graph three times with an unlisted slot in it; the `host_gui` category
gets one `WARN`, and it names the descriptor id.

### `ASavedRigAloneHoldsNeitherTheTracksNorTheTempo` and the `Small_RigControllerRigDocument` cases

A rig document carries the slots and none of the timeline. Loading one onto a controller that
shares a timeline builds the slots and leaves the lanes and the tempo of that timeline as they were;
the timeline has one lane fewer than a new one and a tempo of 97, so a load that reset either would
show.

## `test_rig_controller/test_cabinet.cpp`

The cabinet cases write a 64-sample impulse-response WAV into the test temporary folder, so the file
is `Medium_`. They stay `Medium_` until set_cabinet reads its file through the `AudioFiles` port:
`SlotGraph::set_cabinet` calls `IrLoader::load` itself, and `slot_graph.*` is not part of this job.

## `test_rig_controller/test_tracks.cpp` and `test_rig_controller/test_clips.cpp`

The takes are float32 files of the in-memory `AudioFiles` adapter, so the track cases and the clip
cases are `Small_RigControllerTracks` and `Small_RigControllerClips`. The two recording cases are
`Medium_RigControllerClipsRecording`: a recorder drains its rings on a writer thread, which a
`Small_` case may not start, so they check the boundary of the recorder, into the in-memory files.
The re-amp leg runs at unity gain and speed, so it copies each float32 sample of a take into the
zeroed input, and the cases compare bit for bit.

### `ReAmpsARecordedTakeFromTheBarItWasRecordedAt`

The re-amp leg is silent before the bar the take was recorded at, which is the whole point of the
stamp: the arrangement view design spec in the version history section 5.

## `test_rig_controller/test_room.cpp`

### The room is its in-memory adapter, in all but one case

The cases check what the rig keeps, saves and loads of a room: the scene, the arrangement, the
furnishings, the format of the edges, the publish interval and the HRTF set it hands to the rooms it
holds. None reads a rendered sample. Appending a real room to a rig prepared it and solved every
pair, which took 5 s for a bare room and 13 s for the arrangement round trip in the Debug run of
2026-11-10, against a limit of 5 s. Nineteen cases now run on `RigProductsInMemoryRoom` as
`Small_RigControllerRoom` (they read no file), and the five cases that read an HRTF file run on it as
`Medium_RigControllerRoomHrtf`. `InstallsAMeasuredHrtfIntoTheRoomsItHolds` stays on the real room as
`Medium_RigControllerRoom`, so one case still hands a measured set to a real room node, which
checks the decoder and the render set the adapter does not build. The adapter does not refuse a set at
another sample rate; that refusal is `RoomNode::set_hrtf`'s, tested in `tests/nodes/room/`.

### `LoadsAPresetWithNoRoomAsTheStarterScene`

A fresh node carries the starter scene and no drag has landed, so the slot took the node's scene
rather than publishing one.

### `CoalescesADragToThirtyPublishesPerSecond`

The first edit publishes at once and every later one only after 1 / 30 s has passed since the
previous publish. The injected clock moves one millisecond per edit, so the count is exact: edits
0, 34, 68 and on, 1 + floor(299 / 34).

## `test_session_timeline.cpp`

The lane cases build a SessionTimeline alone; the preset cases build a SlotGraph and a RigPersistence
on the fixture's catalogue.

## `test_slot_selection.cpp`

A SlotId stays valid across insert, move and remove, unlike the old int-index scheme, so the only
remaining job is dropping a selection that names no live slot.

## `test_rig_topology.cpp`

### `RigTopologyOpeningTheSecondPathReportsItAndLeavesItEmpty`

`// The path the model opens starts empty.`

The model holds identities, so the path it opens starts empty: filling it from the main path is
the caller's work.

## `test_topology_applier.cpp`

### The room is its in-memory adapter

The cases read how the applier wires the room strips and fills the arrangement, and none reads a
rendered sample. The fixture is `RigProductsInMemoryRoom`, whose catalogue lists
`TestSupport::InMemoryRoom` (`tests/support/in_memory_room.hpp`) under the room's id. It is an
adapter of the `RoomHost` port that declares the real room's nine output ports, keeps the
arrangement it is given after `Arrangement::is_valid`, and writes silence. The real room solves every
source and microphone pair on each publish, and a case that adds a microphone publishes once more.
In the run of 2026-11-10 the cases that added eight or nine microphones took 20 s, and the ones that
built a cabinet rig took 4 to 9 s against 2 s for a rig of two pedals. The acoustics of the real room
are checked in `tests/nodes/room/`, and the rig's own use of the real room in
`test_rig_controller/test_room.cpp`, so no promise leaves the suite.

### `TopologyApplierOpeningAPathCopiesTheMainPathWithItsParameters`

`// The opened path carries a copy of the main path.`

The main path is untouched and the path that opened carries a copy of it, the knob position
included.

### `MigratingAnOldRoomHeardByAMicrophoneRestoresItsLevelOnTheStrip`

The strip of a centred microphone is 10 log10(2) = 3.0103 dB down a side, so the migration adds that
amount back to the old level. It adds the decimal 3.0103 held in a float. The oracle is the old
level plus 10 log10(2) in double precision, and the bound is the error of that constant: its decimal
rounding, |3.0103 - 10 log10(2)| = 4.3e-8, plus one float rounding of 3.0103, which is
u * 3.0103 = 1.8e-7. The sum of the two floats -3 and 3.0103 is within a factor of two and so is
exact. The comparison keeps the repository's threshold of 16 on that bound.

## `test_session.cpp`

### `AStripGivesTheLeftAndRightGainOfItsRow`

The fader row is the decibel law 10^(-6.0206 / 20), about 0.5, in double precision. The float code
takes one division for the exponent, one powf and one product, so the row allows three roundings of
relative error. A centre balance, a hard turn, a clamped turn and a mute scale or zero a unity gain
by 1, 0.5 or 0, which are exact, so those rows allow none.

## `test_session_renderer.cpp`

### The `Small_SessionRendererMix` cases

Every lane holds a constant that is a power of two (0.5, 0.125, 0.25), so the sums are exact in
binary. The bound allows four roundings against the oracle for the path the sample takes: the lane
gain, the add of the live input, the strip gain and the add into the master. A rig that holds one
mixer with `gain_0` at 0.5 halves a lane, because a mono mixer reads host channel 0 and the render
copies its one channel to both sides; the bypassed rig leaves the lane at 0.5 and the engaged one
brings it to 0.25.

## `test_session_file.cpp`

A float crosses the project document as its double, which parses back to the same float, so the
strip level and balance compare exactly. The preset file cases and the case that places a clip run on
the in-memory adapters of the text file port and the audio file port, as `Small_SessionFileOnFiles`.

## `test_session_bounce.cpp`

The clip is a constant of 0.5 and the track's fader is -6.0206 dB, so the exported sample is 0.5
times 10^(-6.0206 / 20) to within three roundings, as in the strip cases. The clip and the export are
files of the in-memory audio file adapter, so the cases are `Small_SessionBounce`. The clip is 4800 frames,
which is 75 blocks of 64, so block 75 is the first silent one. The tail ends after
floor(0.5 * 48000 / 64) + 1 = 376 silent blocks, so the file holds 4800 + 376 * 64 frames, exactly.
The test target compiles `session_bounce.cpp` without `SUSHIDSP_HAVE_LAME`, so an MP3 request is
refused; a build of the target with the encoder would change that case.
