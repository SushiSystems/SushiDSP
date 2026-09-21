# `apps/host_gui` — the standalone host

`sushidsp_host_gui` is the application the amplifier models are heard through: it opens an audio
device, builds a rig out of the products in `node_registry()`, draws their front panels, and
records and plays back takes against a timeline. It is a host, not a library — nothing in the
tree links it, and it holds no DSP of its own. `sd run` launches it. This file records what the
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
