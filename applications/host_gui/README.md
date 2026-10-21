# `applications/host_gui`, the standalone host

`sushidsp_host_gui` is the application the amplifier models are heard through: it opens an audio
device, builds a rig out of the products in `node_registry()`, draws their front panels, and
records and plays back takes against a timeline. It is a host, not a library, nothing in the
tree links it, and it holds no DSP of its own. `sd host` launches it. This file records what the
code in this directory does.

## What it owns

| Concern | Brick |
| --- | --- |
| The rig's slots, the edges between them, and the node work hanging off a slot | `SlotGraph` |
| Tracks, clips, the recorders and the players the audio callback drives | `SessionTimeline` |
| Reading and writing a preset that carries both of the above | `RigPersistence` |
| The single handle the GUI reaches all three through, for one rig | `RigController` |
| The tracks: up to eight, each a lane of the timeline with a rig and a strip of its own | `Session` |
| One block of the session, for the device and for the export alike | `SessionRenderer` |
| The project file, a new project, and one track's rig preset | `session_file.hpp` |
| The mix rendered offline into a WAV or an MP3 | `SessionBounce`, `Mp3Encoder` |
| The chain shape, one linear run, or a split into two or three branches | `RigTopology` |
| Compiling a chain type into slots and edges | `TopologyApplier` |
| Opening the device, running the frame loop, owning every UI state struct | `main.cpp` |
| Every widget, panel, browser and view | `ui/` |

`RigController` is a facade and nothing else: it forwards to the three bricks behind it and owns
the state none of them can claim alone, such as the remembered impulse-response folder. A caller
that wants only the graph includes `slot_graph.hpp` and links nothing extra.

A preset records the rig as a flat list, so a parallel chain saved today loads back as a series
chain. `docs/design/REMAINING_WORK.md` carries that and the rest of the host's open work.

## Tracks and their rigs

A session is up to eight tracks and starts with one. A track is a lane of the one
`SessionTimeline`, a `RigController` of its own built on that timeline, and a `TrackStrip`:
level, balance, mute, solo and a rig bypass. Eight tracks are eight live rigs. The rig view, the
cabinet section and the gear browser show the selected track's rig, and `Session::rig_revision()`
moves whenever that rig is another one, so `main.cpp` lays the rig view out again and drops every
slot id it held.

`SessionRenderer::render` is the one place a block is rendered. For each track it takes the
lane's clips through `TrackPlayer::process_lane`, adds the live input when the track is the
selected one, runs the result through the track's rig unless the rig is bypassed or empty,
scales it by the strip's two gains and adds it to the master. Then the recorder takes the block,
the click is added and the transport advances. A track that is neither rolling nor being played
into is skipped, so a stopped session costs one rig.

The audio thread reads the tracks through `Session::audio()`, eight entries of atomics. A
removed track's rig is taken out of that table at once and freed two blocks later.

A take is recorded dry onto the armed track, and arming a track selects it: what is played is
heard through the rig that will render the take.

Two things are saved, and they are not the same thing. A rig is saved as a preset, from the
top bar: `save_track_rig` and `load_track_rig` write and read the selected track's rig alone,
through `RigController::save_rig_to_json` and `load_rig_from_json`, and leave every lane, the
loop and the tempo as they are. The work in the mini-DAW is saved as a project, from the
mini-DAW's own row: `session_to_json` writes the first track's rig and the timeline as a preset
always did, and every track's strip, preset path and rig under `"track_rigs"`. `new_project`
empties the session to one track with an empty rig.

A file without `"track_rigs"` opened as a project is one rig: its rig becomes the first track's
and its other lanes become tracks that pass their audio by an empty rig, which is how they
played before. A clip on the first lane of such a file now runs through the rig.

`SessionBounce` renders the mix on a thread of its own. It builds a session of its own from
`session_to_json`'s text, so the device keeps playing the live one, renders from frame zero
to the end of the last clip, and stops when the output has stayed under -90 dBFS for half a
second or ten seconds after the last clip. It writes a 16-bit or a 32-bit float WAV, or an MP3
through `Mp3Encoder`, the one class that touches LAME. LAME is LGPL and is linked as a DLL that
sits beside the executable; `cmake/SushiDSPLame.cmake` builds it, on Windows only.

## The audio thread's boundary

The GUI thread never touches a graph the callback is rendering. Each track's `RigController` publishes a
compiled `RigGraph` into an `RtGraphSlot`, and the callback acquires from that slot once per
block. Every edit, adding a slot, connecting an edge, loading a preset, rebuilds a graph on the
GUI thread and publishes it; the callback picks it up at the next block boundary or keeps
rendering the previous one. Parameter edits do not go that way: a knob writes through
`set_parameter_for_audio_thread()` by index, which is a subscript into the node's own
`ParameterTable`.

## Panels

A node's front panel comes from `ui::PanelLibrary`. `panel_for()` returns a hand-authored layout
for a node type when one exists, a list of placements, each a control id, a widget kind and a
centre in `[0, 1]` on both axes, and `auto_panel()` lays the node's declared parameters
out on a grid when it does not. A product that registers in `node_registry()` therefore draws
something the day it registers, and gains a designed panel later by adding one entry here.
`DrawStagePanel` reads whichever of the two it is handed; it knows no node types.

## Dropped files

A song dropped on the window is decoded off the GUI thread. `FileDropQueue` carries GLFW's paths
into the frame, `AudioImportQueue` runs the decode on a worker, `decode_mp3()` and
`resample_channels()` turn the file into the rig's own rate and channel count, and
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

`main()` logs through SushiLog, which writes each record to stderr from its own thread and
delivers what is queued when the process exits. The host logs under `K_HOST_GUI_LOG_CATEGORY`,
declared in `host_gui_log_category.hpp`. The messages `main()` prints with `fprintf` are its
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
resized afterwards, so the callback allocates nothing. `applications/host_asio/main.cpp` sizes its
scratch buffer the same way. The two level probes sit in the same scope for the same reason:
the callback reports a peak into each of them on every block.

`session_storage` is declared before `device`. C++ destroys locals in reverse
declaration order, so `device` is destroyed before the session, its controller and the `RigGraph`s they own,
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
wide as the device can take, up to `RigGraph::K_MAX_CHANNELS`, and `RigGraph` adapts a mono
terminal node to that width itself. The channel selection still decides which physical channel
feeds the rig and which receives its output.

A rack holding nothing and a rack whose every slot is bypassed are both a cable, so the
callback copies the input to every output channel untouched.

The metronome's click is added to the output after both recorder legs are written, so it
reaches no recorded file. the arrangement view design spec in the version history
§4 specifies this.

GLFW delivers an Explorer drop outside the ImGui frame. The frame loop resolves each one
against the lane rectangles the previous frame recorded, then routes it through
`ui::route_file_drop`, the function an ImGui payload drop also calls.

The bottom divider lives in a thin window of its own between the workspace and the bottom bar;
the workspace's two dividers live inside it, between its child panes. A seam between panes that
do not resize carries no grab handle, so it needs only enough room for its hairline and is 5 px
thick. At the grab strip's 14 px it read as a black band.

### `audio_import_cache.hpp`

`import()` writes a partial file first and renames it into place, so a run that dies mid-decode
leaves nothing that could be mistaken for a finished entry.

### `mp3_decoder.cpp`

A path reaches the decoder narrow from the file browser and UTF-8 from the window's drop
callback. `open_mp3` tries the path as given first; the wide retry is what opens the second
kind.

### `SessionTimeline` and `RigController::place_clip`

`place_clip` shares the decoded take with every clip already reading the same path. It refuses,
with a message in `error_out` and the lane left as it was, when the file will not parse, when
the lane already holds `SushiDSP::TrackTimeline::K_MAX_CLIPS` clips, or when the clip would
overlap one the lane already holds.

A placement whose length is zero means the take's whole remainder. That is what a preset row
naming only a file means, and what a freshly dropped file means.

A lane converts a clip only through a sized reader, so the constructor prepares both players
and no caller can set a speed on an unprepared one.

The re-amp leg mirrors the lane's frames, so a take recorded at a bar drives the rig at that
bar (the arrangement view design spec in the version history §5).

When a recording is armed, the transport position is read before the recording bus opens. No
block the bus accepts then belongs to an earlier frame than the one the stopped recording's
clip begins at.

### `rig_persistence.cpp`

These follow the arrangement view design spec in the version history §8.

A preset written before the timeline existed names one file per track row, and the loader reads
that as one clip at frame 0. Such a preset carries no tempo either, which meant 120 BPM in 4/4
with the click off; the loader applies those values when the `tempo` object is absent.

The playback speed is a practice setting, not a session's. It is written nowhere, and every
loaded session opens at 1.0.

`load_from_json` sizes the scratch graph's buffer pool from the preset's own node count, plus
the one buffer of headroom `publish()` uses. `RigGraph::compile()` only asserts the pool's
capacity, so a fixed guess would hand the audio thread null buffers on any preset larger than
the guess.

### `rig_topology.cpp`

A chain with one path has no master bus (the `Post` stage): the path is the run itself. A stage
that is drawn nowhere and still carried gear would be audible with nothing on screen to explain
it.

### `slot_graph.cpp`

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

`publish()` drops a slot whose node type the registry does not know. It does so itself and not inside
`build_graph()`, which stays `const` so that a preset save can call it too.

### `TopologyApplier`

`roll_back` publishes the rig again when the rollback restored removed slots. The model it
publishes compiled once before the edit, so a failure there leaves the old graph playing;
`roll_back` logs it as one `WARN` and returns.

`switch_chain_type` fills a path that opens with a copy of the main path's gear, parameter
values included, so a split starts from what was already being heard and not from silence.
`docs/design/GUI_UX_REDESIGN.md` records that behaviour. A path that closes leaves its slots
behind as strays, and the apply removes them.

## The Parallel chain

`ChainType::Parallel` is the stereo chain: two mono paths, the left one fed by host input channel 0
and the right one by channel 1. Each path ends in a channel strip panned to its side
(`sushidsp.channel_strip`, pan -1 and +1), and both strips join a bus (`sushidsp.bus`), the hidden
slot the applier owns in place of the mixer. `RigTopology::parallel_edges` builds the edges.

The GUI host passes the rig two input channels every block: the one `--input` selects first, and the
device's other input second. A mono device repeats the first, so both paths hear the same signal. The
applier binds each path's head slot to its channel through `RigController::set_input_channel`, which
`SlotGraph::build_graph` passes to `RigGraph::bind_input`. A path left behind by a switch to another
chain type is rebound to channel 0.

## The DI path

`RigTopology::di()` turns the DI path on: a channel strip that reads the rig's input directly and
joins the rig without passing an amplifier or a cabinet. `RigTopology::di_position()` says where it
joins. `DiPosition::BeforeEffects` puts it on the amp bus, so it passes the post effects with the
amplifiers' sum; `DiPosition::AfterEffects` puts it on a master bus of its own after them. A rig
with no gear has no cabinet section, and there the DI strip and the chain's sink meet in that
master bus whatever the position says.

## The cabinet section

The cabinet section is the part of the rig after the cabinets: one room, its microphones, the
strips and the buses. `TopologyApplier` builds it whenever the topology routes at least one slot
and takes it away with the last one, so an empty rig still passes its input through. Its slots are
hidden: no stage of the chain shows them and the gear browser does not list them
(`ui/gear_filter.cpp`).

```
path k:  ... -> cabinet k -> ... --+--> room, input k
                                   +--> the chain's own join --> Cabinet

room:    port 0      listener (stereo) --> Listener strip
         port 1 + m  microphone m      --> microphone slot --> Mic strip

Mic strips, room flag clear --> Bus
Mic strips, room flag set   --> Room sum --> Room strip --> Bus

Cabinet + Bus + Listener (+ DI before the effects) --> amp bus --> post stage
post stage --> master bus (with the DI after the effects) --> Master strip
```

- **Cabinet** is the chain's join as it always was: the single path's last slot, the mixer of a
  split chain, or the Parallel chain's bus. A cabinet's impulse response is already a close
  microphone's capture, so this input is a signal in its own right. A new rig starts with Cabinet
  on and the listener muted, and sounds as its cabinets do until the room is brought in.
- **A source** is a path that holds a `sushidsp.cabinet` slot. The room hears the path at its
  last slot and keeps the source under the cabinet slot's id. A path with no cabinet reaches the
  join only.
- **A microphone** is two slots the applier owns: a `sushidsp.microphone`, which carries the
  pattern, the proximity switch, the polarity and the room flag, and a `sushidsp.channel_strip`.
  `TopologyApplier::add_microphone` and `remove_microphone` are the only way they come and go; a
  ninth is refused with a reason and the rig is left as it was.
- **The strips.** A microphone and the DI use the mono `sushidsp.channel_strip`. The listener, the
  Room sum and the master use `sushidsp.stereo_strip`.

`cabinet_section_wiring.cpp` holds the wiring as a pure function: `CabinetSectionNeeds::of` says
which optional slots the section needs for its microphones and its DI choice, and
`wire_cabinet_section` returns every edge with its port, in connect order, plus what feeds each
input of each bus. The applier creates the slots, connects the edges and moves each bus's mute and
solo flags to the input its feeder now holds (`TopologyApplier::remap_flags`), because a bus keeps
those flags by input index and an added or removed microphone shifts the indexes.

The room's placements live in the room slot's `SushiDSP::Room::Arrangement`
(`RigController::room_arrangement`, `set_room_arrangement`). On every apply the applier brings the
arrangement in line with the rig: one source per path that carries a cabinet, one microphone per
microphone slot, each new one at a default pose, each known one where it stood. It copies a
microphone's pattern and proximity switch from its slot into the arrangement, since the room's
solve reads them; `TopologyApplier::refresh_room` does the same without rewiring. A failed apply
puts the arrangement back with everything else.

After each apply the applier records the rig's `RigShape` (`rig_shape.hpp`) through
`RigController::set_shape`: the chain type, each stage's slots, the DI choice and every hidden
slot. A preset carries it, and a loaded rig hands it back, so `TopologyApplier::adopt` keeps the
hidden slots a preset saved with their levels, pans and flags.

On screen the section is a pane of the main layout, chosen by the switch under the top bar
(`ui/cabinet_section_pane.cpp`). It holds the room scene (`ui/room_scene_view.cpp`), an inspector
for the selected marker and for the room's shell (`ui/room_panel.cpp`), the DI switches and the
mixer (`ui/mixer_panel.cpp`). `ui/README.md` describes each.

## Preset format

A preset is the `"nodes"` document `SushiDSP::rig_preset` writes, with the host's own keys beside
it. `"format"` names the host's format; this build writes 2 and reads 1 and 2, and refuses a
higher number before it touches the rig.

| Key | Format | Holds |
| --- | --- | --- |
| `"format"` | 2 | The format number. A preset without it is format 1. |
| `"edges"` | 2 | Every edge as `{"from", "to", "port"}`, because a node's own `"inputs"` carries no port. |
| `"shape"` | 2 | The `RigShape`: `"chain_type"`, `"branches"`, `"post"`, `"di"`, `"di_position"` and `"hidden"`, every slot as its node id. |
| A room node's `"room"` | 2 | The arrangement: the shell and the listener as a scene writes them, then `"sources"` and `"microphones"`, each row a pose with the `"node"` it stands for. A source row adds `"cabinet"`, one of `"1x12"`, `"2x12"` and `"4x12"`, and without it is the shell's cabinet. A microphone row adds `"pattern"`, `"proximity"` and `"model"`, the key of a shipped microphone, which is the flat reference when left out. A furnishing row that has a place adds `"position"`, its x and y in metres, and `"yaw"`. |
| A room node's `"room"` | 1 | One scene: the shell and the poses of one cabinet, the listener and one microphone. |

A format 2 preset's nodes are read alone and its edges come from `"edges"`, so the rig's own
publish is what checks the graph. A format 1 preset is read whole and compiled first.

A format 1 room was a slot of the chain with `listener`, `level` and `mix` parameters. Loading
one takes the room out of the chain, bridges what fed it to what it fed, and keeps it as a
`LegacyRoom` for `RigController::take_legacy_room`. `TopologyApplier::migrate_legacy_room` turns
that into the section's terms: the room's shell and poses, the listener or one microphone as its
receiver, the old level on that receiver's strip, and Cabinet muted, because the old chain ended
in the room. A microphone's strip gets 3 dB more than the old level, since a centred strip is
3 dB down a side and the old room put its microphone on both channels at full level. The old
`mix` is not carried: the room has no dry path. Each step is logged.

## Designs

<!-- designs:begin -->
- [GUI_UX_REDESIGN](../../docs/design/GUI_UX_REDESIGN.md): The standalone host window: the device panel, the transport, the rig chain editor and the workspace.
<!-- designs:end -->
