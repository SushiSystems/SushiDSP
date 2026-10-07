# `apps/host_gui` — the standalone host

`sushidsp_host_gui` is the application the amplifier models are heard through: it opens an audio
device, builds a rig out of the products in `node_registry()`, draws their front panels, and
records and plays back takes against a timeline. It is a host, not a library — nothing in the
tree links it, and it holds no DSP of its own. `sd host` launches it. This file records what the
code in this directory does.

## What it owns

| Concern | Brick |
| --- | --- |
| The rig's slots, the edges between them, and the node work hanging off a slot | `SlotGraph` |
| Tracks, clips, the recorders and the players the audio callback drives | `SessionTimeline` |
| Reading and writing a preset that carries both of the above | `RigPersistence` |
| The single handle the GUI reaches all three through | `RigController` |
| The chain shape — one linear run, or a split into two or three branches | `RigTopology` |
| Compiling a chain type into slots and edges | `TopologyApplier` |
| Opening the device, running the frame loop, owning every UI state struct | `main.cpp` |
| Every widget, panel, browser and view | `ui/` |

`RigController` is a facade and nothing else: it forwards to the three bricks behind it and owns
the state none of them can claim alone, such as the remembered impulse-response folder. A caller
that wants only the graph includes `SlotGraph.hpp` and links nothing extra.

A preset records the rig as a flat list, so a parallel chain saved today loads back as a series
chain. `docs/design/REMAINING_WORK.md` carries that and the rest of the host's open work.

## The audio thread's boundary

The GUI thread never touches the graph the callback is rendering. `RigController` publishes a
compiled `RigGraph` into an `RtGraphSlot`, and the callback acquires from that slot once per
block. Every edit — adding a slot, connecting an edge, loading a preset — rebuilds a graph on the
GUI thread and publishes it; the callback picks it up at the next block boundary or keeps
rendering the previous one. Parameter edits do not go that way: a knob writes through
`set_parameter_for_audio_thread()` by index, which is a subscript into the node's own
`ParameterTable`.

## Panels

A node's front panel comes from `ui::PanelLibrary`. `panel_for()` returns a hand-authored layout
for a node type when one exists — a list of placements, each a control id, a widget kind and a
centre in `[0, 1]` on both axes — and `generic_panel_for()` lays the node's declared parameters
out on a grid when it does not. A product that registers in `node_registry()` therefore draws
something the day it registers, and gains a designed panel later by adding one entry here.
`StagePanel` reads whichever of the two it is handed; it knows no node types.

## Dropped files

A song dropped on the window is decoded off the GUI thread. `FileDropQueue` carries GLFW's paths
into the frame, `AudioImportQueue` runs the decode on a worker, `Mp3Decoder` and
`ChannelResampler` turn the file into the rig's own rate and channel count, and
`AudioImportCache` keeps the result as a WAV beside the executable so the same drop decodes once.
The frame loop collects finished imports and places them as clips.

## Assets

The host resolves `assets/ir/default_4x12.wav` and `assets/hrtf/` relative to its own executable,
which is why `CMakeLists.txt` copies both directories next to the binary after a build. Neither
ships in the checkout: without an IR a fresh amp slot gets none, and without an HRTF set the room
keeps the spherical-head response it builds for itself. `assets/ir/README.md` and
`assets/hrtf/README.md` say what to put there.

## Dependencies

Links `SushiDSP::sushi_dsp`, `SushiDSP::node_registry`, the product libraries it draws panels for
(`tubescreamer`, `jcm800`, `capture`, `room`), `SushiDSP::rig_preset`, `SushiDSP::sushi_dsp_sdl2`,
Dear ImGui, `nlohmann_json` and `opengl32`. `SushiDSP::sushi_dsp_asio` is linked and
`SUSHIDSP_HAVE_ASIO` defined only when that target exists, so a checkout without the ASIO SDK
builds the same host on SDL2 alone.

## Notes by file

The source comments in this directory cite the sections below. [`ui/README.md`](ui/README.md)
holds the same for `ui/`.

### `main.cpp`

`main()` installs an `AsyncLogSink` over a `StderrLogSink` through a `ScopedLogSink`, which
flushes the sink on every return path. The host logs under `K_HOST_GUI_LOG_CATEGORY`, declared
in `host_gui_log_category.hpp`. The messages `main()` prints with `fprintf` are its
command-line output and do not go through the logger.

The zoom, the scroll and the snap mode belong to the GUI loop and not to the rig: they are how
the arrangement is being looked at, not what it plays.

A dropped mp3 is decoded to a WAV in `cache/imported_audio`, a folder of the host's own beside
the impulse responses it already keeps next to the executable, and the clip points at that WAV.
A session stores a clip as a path, so the decode has to leave a file behind, and that file must
not be written into the user's music folder. The decode runs on the queue's worker thread, so a
five-minute song does not stop the window. A decode that finished on the worker becomes a clip
on the GUI thread, which owns the rig.

`rig_input`, `rig_output` and the two pointer arrays are captured by reference in the device
callback, and that callback keeps firing on the audio thread for as long as the device runs,
which is every frame after the one where Connect was clicked. They therefore live in `main()`'s
own scope and not inside the `connect_device` lambda. A declaration in the lambda's body would
be destroyed when the connect sequence returns, and the callback's references would dangle on
the next audio callback. The buffers start empty and are sized once, inside `connect_device`,
when the device's block size is known. That happens off the audio thread and they are never
resized afterwards, so the callback allocates nothing. `apps/host_asio/main.cpp` sizes its
scratch buffer the same way. The two level probes sit in the same scope for the same reason:
the callback reports a peak into each of them on every block.

`rig_controller_storage` is declared before `device`. C++ destroys locals in reverse
declaration order, so `device` is destroyed before the controller and the `RigGraph`s it owns,
by construction and without relying on the explicit `device->stop()` call. `RigController`'s
constructor needs a `PrepareInfo` derived from `device->format()`, so it cannot be constructed
until the device exists; `std::optional` fixes the declaration order, and with it the
destruction order, independently of the construction order. The buffers and the probes are
declared earlier still, so they are destroyed last, after the device whose callback references
them is gone.

`connect_device` calls `device.reset()` before it constructs the next device. `AsioAudioDevice`
and `Sdl2AudioDevice` both wrap process-wide SDK state. Assigning straight into the
`std::unique_ptr` would run the new construction (`ASIOInit`, `ASIOCreateBuffers` and the rest)
while the old device was still alive and tear the old state down only afterwards, destroying
the SDK state the new device had just built.

The rig's input is always one channel, because an amplifier is a mono circuit. Its output is as
wide as the device can take, up to `RigGraph::kMaxChannels`, and `RigGraph` adapts a mono
terminal node to that width itself. The channel selection still decides which physical channel
feeds the rig and which receives its output.

A rack holding nothing and a rack whose every slot is bypassed are both a cable, so the
callback copies the input to every output channel untouched.

The metronome's click is added to the output after both recorder legs are written, so it
reaches no recorded file. `docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md`
§4 specifies this.

GLFW delivers an Explorer drop outside the ImGui frame. The frame loop resolves each one
against the lane rectangles the previous frame recorded, then routes it through
`ui::route_file_drop`, the function an ImGui payload drop also calls.

The bottom divider lives in a thin window of its own between the workspace and the bottom bar;
the workspace's two dividers live inside it, between its child panes. A seam between panes that
do not resize carries no grab handle, so it needs only enough room for its hairline and is 5 px
thick. At the grab strip's 14 px it read as a black band.

### `AudioImportCache.hpp`

`import()` writes a partial file first and renames it into place, so a run that dies mid-decode
leaves nothing that could be mistaken for a finished entry.

### `Mp3Decoder.cpp`

A path reaches the decoder narrow from the file browser and UTF-8 from the window's drop
callback. `open_mp3` tries the path as given first; the wide retry is what opens the second
kind.

### `SessionTimeline` and `RigController::place_clip`

`place_clip` shares the decoded take with every clip already reading the same path. It refuses,
with a message in `error_out` and the lane left as it was, when the file will not parse, when
the lane already holds `SushiDSP::TrackTimeline::kMaxClips` clips, or when the clip would
overlap one the lane already holds.

A placement whose length is zero means the take's whole remainder. That is what a preset row
naming only a file means, and what a freshly dropped file means.

A lane converts a clip only through a sized reader, so the constructor prepares both players
and no caller can set a speed on an unprepared one.

The re-amp leg mirrors the lane's frames, so a take recorded at a bar drives the rig at that
bar (`docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md` §5).

When a recording is armed, the transport position is read before the recording bus opens. No
block the bus accepts then belongs to an earlier frame than the one the stopped recording's
clip begins at.

### `RigPersistence.cpp`

These follow `docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md` §8.

A preset written before the timeline existed names one file per track row, and the loader reads
that as one clip at frame 0. Such a preset carries no tempo either, which meant 120 BPM in 4/4
with the click off; the loader applies those values when the `tempo` object is absent.

The playback speed is a practice setting, not a session's. It is written nowhere, and every
loaded session opens at 1.0.

`load_from_json` sizes the scratch graph's buffer pool from the preset's own node count, plus
the one buffer of headroom `publish()` uses. `RigGraph::compile()` only asserts the pool's
capacity, so a fixed guess would hand the audio thread null buffers on any preset larger than
the guess.

### `RigTopology.cpp`

A chain with one path has no master bus (the `Post` stage): the path is the run itself. A stage
that is drawn nowhere and still carried gear would be audible with nothing on screen to explain
it.

### `SlotGraph.cpp`

`remove()` finds the slot's one upstream and one downstream neighbor before it erases the
slot's edges, and reconnects them around the gap. A removal from the middle of a chain never
leaves the two slots that were adjacent to it disconnected. `move_before()` detaches a slot the
same way and keeps the slot itself alive.

`insert_before()` retargets whichever edge pointed at the following slot to the new one, then
connects the new slot to the following one.

A bypassed slot has no live node to ask whether a cabinet file is acceptable. `set_cabinet()`
keeps the path, and the node decides on the publish that brings the slot back;
`apply_cabinet_to_live_node()` drops the path if the node refuses it. A stored room scene the
fresh node refuses is dropped for the node's own scene in the same way.

`apply_cabinet_to_live_node()` logs one `WARN` when the impulse response cannot be loaded and
one when the node refuses it, each with the slot, the path and the sample rate, before it drops
the path.

`set_room_scene()` validates the scene itself and does not leave that to the node. A drag that
takes a pose out of the room is then refused on the frame it happens, not on the later publish
the pacing interval allows.

`restore()` writes a snapshot's values over the defaults `add_node()` already seeded, the way a
preset's values layer over a fresh slot's defaults.

`publish()` drops a slot the registry no longer knows. It does so itself and not inside
`build_graph()`, which stays `const` so that a preset save can call it too.

### `TopologyApplier`

`roll_back` publishes the rig again when the rollback restored removed slots. The model it
publishes compiled once before the edit, so a failure there leaves the old graph playing;
`roll_back` logs it as one `WARN` and returns.

`switch_chain_type` fills a path that opens with a copy of the main path's gear, parameter
values included, so a split starts from what was already being heard and not from silence.
`docs/design/GUI_UX_REDESIGN.md` records that behaviour. A path that closes leaves its slots
behind as strays, and the apply removes them.
