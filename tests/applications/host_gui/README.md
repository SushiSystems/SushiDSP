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
no case reaches the process-wide node registry. The WAV writers are in
`tests/support/wav_fixture.hpp`.

## `test_rig_controller/test_chain.cpp`

### `BatchedEditsBuildASplitterTwoBranchMixerShapeThatProcessesABlock`

The split/mix shape needs publish batching to build: before the publish-batching parameter it was
impossible to construct through the public API at all, because the second add_node() would have
republished a two-node, zero-edge graph, which RigGraph::compile() rejects for having no sink. The
case checks that every node exists and the graph published; it checks no output, because the
splitter and mixer gain laws are not stated anywhere a test can derive a block from.

### `AddBranchBuildsTheParallelShapeTheGuiOffers`

The whole point of the feature, in the order a user performs it: drag a source, a splitter and one
effect into a straight line, then drop a second effect on the splitter's open row. add_branch() is
what turns that one drop into the four-edge shape RigGraph::compile() accepts, mixer and all.

### `AppendWithUnknownDescriptorIdKeepsTheSlotAndBuildsNoNodeForIt`

A slot whose descriptor id the catalogue does not list stays in the rig, so a preset that names it
survives, and the graph builds no node for it.

## `test_rig_controller/test_bypass.cpp`

A bypassed slot keeps its place in the slot order, its parameters and its edges; only the published
graph leaves it out, wiring its upstream neighbours straight to its downstream ones.

## `test_rig_controller/test_preset.cpp`

### `SaveToJsonUsesOwnStoredValueNotLiveNodeState`

Deliberately no process() call and no further structural edit here: the live node still reads the
pre-edit default, but save_to_json() reports the controller's own stored value, matching what
get_parameter() and the GUI slider show.

## `test_rig_controller/test_cabinet.cpp`

The cabinet cases write a 64-sample impulse-response WAV into the test temporary folder, so the file
is `Medium_`. They stay `Medium_` until set_cabinet reads its file through a port.

## `test_rig_controller/test_tracks.cpp` and `test_rig_controller/test_clips.cpp`

The cases write float32 WAV takes into the test temporary folder and stay `Medium_` until load_track,
place_clip and the recorder read and write through a port. The re-amp leg runs at unity gain and
speed, so it copies each float32 sample of a take into the zeroed input, and the cases compare
bit for bit.

### `ReAmpsARecordedTakeFromTheBarItWasRecordedAt`

The re-amp leg is silent before the bar the take was recorded at, which is the whole point of the
stamp: the arrangement view design spec in the version history section 5.

## `test_rig_controller/test_room.cpp`

The room node is built by every case, and the HRTF cases read a fixture file, so the suite is
`Medium_`.

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

### `TopologyApplierOpeningAPathCopiesTheMainPathWithItsParameters`

`// The opened path carries a copy of the main path.`

The main path is untouched and the path that opened carries a copy of it, the knob position
included.
