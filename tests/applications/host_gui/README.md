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

### `ResampleChannelsDoublesTheRateAndKeepsTheWave`

`// The ends read silence on one side and stay within 0.035.`

Away from the ends the converted tone sits on the analytic sine to within 4e-6; the first and
last samples read a filter that has silence on one side of it, and stay within 0.035.

## `test_rig_controller.cpp`

The file holds five groups of cases under one test binary. The groups are listed here and the file is reported as a candidate for splitting, not split:

- Per-slot bypass, from `RigControllerBypassBridgesTheSlotsNeighbours`.
- The impulse-response folder, from `RigControllerRemembersTheIrFolder`. The folder is
  preset-level and has no audio behind it.
- Rack-list selection tracking (`applications/host_gui/slot_selection.hpp`), from
  `SlotSelectionStillValidReturnsTrueWhenSlotIsInTheList`.
- The room: its scene in the preset, its publish pacing and its HRTF (DE6).
- Snapshot and edge accessors (`applications/host_gui`, Phase 2 chain-type rewiring), from
  `RigControllerSnapshotRestoreKeepsParametersAndCabinetPath`.

### `RigControllerRemoveDropsFromChainAndReconnectsSurvivors`

`// The chain re-links around a removed middle slot.`

b's removal must not leave a and c disconnected, the chain must re-link around the gap,
matching today's behavior where removing a middle slot still produces one working chain.

### `RigControllerBuildsSplitterTwoBranchMixerShapeAndProcessesCorrectly`

`// The split/mix shape needs publish batching to build.`

Every node exists and the graph compiled/published without throwing, the split/mix shape this
test builds was, before the publish-batching parameter, impossible to construct through the
public API at all (the second add_node() would have republished a two-node, zero-edge graph,
which RigGraph::compile() rejects for having no sink). Confirm the rig actually processes a
block without crashing or throwing.

### `RigControllerAddBranchBuildsTheParallelShapeTheGuiOffers`

`// add_branch() turns one drop into the four-edge shape.`

The whole point of the feature, in the order a user performs it: drag a source, a splitter and
one effect into a straight line, then drop a second effect on the splitter's open row.
add_branch() is what turns that one drop into the four-edge shape RigGraph::compile() accepts,
mixer and all.

### `RigControllerSaveToJsonUsesOwnStoredValueNotLiveNodeState`

`// No process() runs here; the live node still reads 0.5.`

Deliberately no process() call and no further structural edit here, the live node still reads
the pre-edit default (0.5), but save_to_json() must report the controller's own authoritative
0.9f, matching what get_parameter() and the GUI slider both already show.

### `RigControllerBypassBridgesTheSlotsNeighbours`

`// A bypassed slot keeps its place, parameters and edges.`

Per-slot bypass.

A bypassed slot keeps its place in slot_order_, its parameters and its edges; only the published
graph leaves it out, wiring its upstream neighbours straight to its downstream ones.

### `SlotSelectionStillValidReturnsTrueWhenSlotIsInTheList`

`// A SlotId stays valid across insert, move and remove.`

Rack-list selection tracking (applications/host_gui/slot_selection.hpp).

A SlotId stays valid across insert/move/remove, unlike the old int-index scheme, so the only
remaining job is dropping a selection that names no live slot.

### `RigControllerReAmpsARecordedTakeFromTheBarItWasRecordedAt`

`// The re-amp leg is silent before the take's bar.`

The re-amp leg is silent before the bar the take was recorded at, which is the whole point of
the stamp: the arrangement view design spec in the version history Â§5.

### `RigControllerLoadsAPresetWithNoRoomAsTheStarterScene`

`// A fresh node carries the starter scene.`

A fresh node carries the starter scene and no drag has landed, so the slot took the node's scene
rather than publishing one.

### `RigControllerCoalescesADragToThirtyPublishesPerSecond`

`// At most 1 + floor(30 T) publishes land in T seconds.`

The first edit publishes at once and every later one only after 1 / 30 s has passed since the
previous publish, so the count is bounded by the loop's own wall time: at most 1 + floor(30 T),
which is 3 for a loop inside 100 ms.

## `test_rig_topology.cpp`

### `RigTopologyOpeningAPathReportsItForTheCallerToFill`

`// The path the model opens starts empty.`

The model holds identities, so the path it opens starts empty: filling it from the main path is
the caller's work.

## `test_topology_applier.cpp`

### `TopologyApplierOpeningAPathCopiesTheMainPathWithItsParameters`

`// The opened path carries a copy of the main path.`

The main path is untouched and the path that opened carries a copy of it, the knob position
included.
