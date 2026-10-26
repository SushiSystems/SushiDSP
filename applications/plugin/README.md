# `applications/plugin`, the rig as a plugin

`sushidsp_clap` is a CLAP plugin that runs one rig inside a DAW: stereo in, stereo out, no
editor. The rig is a preset the standalone host saved. Nothing in the tree links the plugin and
it holds no DSP of its own. This file records what the code in this directory does.

## What it owns

| Concern | Brick |
| --- | --- |
| One rig behind an interface that knows no plugin format | `RigAdapter` |
| One parameter of one slot, and the id that names it | `RigParameter` |
| The table the audio thread reads to reach a parameter's node | `ParameterRouting` |
| The preset a plugin starts on when it has no state | `read_startup_rig()` |
| One CLAP plugin instance: life cycle, block, news to the host | `ClapRigPlugin` |
| The audio-ports, state, latency and params tables | `find_extension()` |
| The entry symbol and the plugin factory | `clap_entry.cpp` |

`RigAdapter` and the three bricks above the CLAP rows include no CLAP header. A second plugin
format is a second shell over the same adapter: it adds files beside `clap_plugin.cpp` and edits
none of the adapter's.

`RigAdapter` wraps the host's `RigController`. It does not read or write a preset itself: it
calls `load_rig_from_json()` and `save_rig_to_json()`, so a preset key added to `RigPersistence`
reaches the plugin without a change here.

## How the rig is chosen

A new instance holds an empty rig, which copies each input channel to its output channel.

`ClapRigPlugin::init()` reads the environment variable `SUSHIDSP_PLUGIN_RIG`. When it names a
file that opens, the file's text is loaded as the rig. A file that is missing, or that the
adapter refuses, leaves the empty rig in place. The variable is read once for each instance, in
`init()`, so that the host's first parameter scan already lists the rig's parameters.

A host that has a saved session then calls the state extension's load, and the state replaces
whatever `init()` loaded. The state is the preset text itself, byte for byte what
`RigAdapter::save_state()` returns, so a DAW session recalls the rig without the file the
variable named. A preset the standalone host wrote with `save_to_json()` loads too: the adapter
reads the rig out of it and ignores the lanes, the loop and the tempo.

## The block

`ClapRigPlugin::process()` queues every parameter value event of the block on the adapter, then
calls `RigAdapter::process()`. The adapter applies the queued changes, copies the two input
channels into its own buffer, and runs the graph the controller last published. The copy is what
lets a host render in place, with the input and the output buffers the same memory. A block that
is not one stereo 32-bit input and one stereo 32-bit output returns `CLAP_PROCESS_ERROR`. A
block longer than the activated maximum is cleared to silence.

Every event is applied at the start of its block, whatever sample offset it carries.

Nothing on this path allocates, locks, makes a system call or throws. The adapter's buffers are
sized in `RigAdapter::prepare()`, and the queue of pending changes is a fixed array of
`RigAdapter::K_PENDING_CAPACITY` entries. A change that arrives when the array is full is
dropped and counted in `dropped_parameter_changes()`.

## Parameters on two threads

A parameter id is the slot's position in the rig in the high 16 bits and the parameter's index
in its product in the low 16; `rig_parameter_id()` builds it. The same preset gives the same ids
on every load, which is what a DAW's recorded automation needs.

The control thread sets a parameter through `RigController::set_parameter()`, which stores the
value in the slot and queues it on the graph's own ring. The audio thread cannot use that path:
it writes a `std::map` and its ring has one producer. So each published rig has a
`ParameterRouting`, published through an `RtSwapSlot` beside the graph, that holds for every
parameter its node handle, its index in the node and an atomic copy of its value. The audio
thread looks an id up in the routing, stores the value and calls
`set_parameter_for_audio_thread()` on the node.

The routing records which graph its handles belong to. For the one block in which the audio
thread sees a new graph with the routing of the graph before it, the queued changes are dropped:
a state load has just replaced every value anyway.

`RigAdapter::save_state()` first writes every value the audio thread set back into the
controller, so the preset carries the automation's last value.

`RigController` does not say which node a slot became. The adapter pairs the slots that are not
bypassed, in slot order, with the graph's nodes in the order they were added, and checks each
pair's product id against the node's `type_id()`. If the two do not line up, no route gets a
node: an audio-thread change is then stored and saved but not heard until the next load.

## Sample rate and block size

A `RigController` is built at one sample rate, block size and width and keeps them. So
`RigAdapter::prepare()` saves the rig as text, builds a new controller at the new format and
loads the text into it. The adapter starts at `K_DEFAULT_SAMPLE_RATE` and
`K_DEFAULT_MAX_BLOCK_FRAMES` so that a host can load state and list parameters before it
activates the plugin. A `prepare()` at the format already in place does nothing.

`RigAdapter::load_state()` loads a preset into a second, throwaway controller first. A preset
that throws there is refused and the playing rig is not touched. `RigPersistence` clears the rig
before it rebuilds it, so a preset that failed halfway on the live controller would leave the
plugin silent.

## What the host is told

CLAP lets the parameter list and the latency change only while the plugin is not active. A state
load while inactive asks the host for a full parameter rescan at once. A state load while active
replaces the rig at once, asks the host for a restart, and gives the rescan in `deactivate()`.
`activate()` tells the host the latency changed when it differs from the value the host last
read.

Graphs and routings the audio thread has moved past are freed on the main thread: at every state
load and save, at `deactivate()`, and in `on_main_thread()`, which a state load requests.

## Where a DAW finds the plugin

The target's output file is `SushiDSP.clap`, in the build tree's `applications/plugin` folder.
No install step copies it. On Windows a CLAP host scans `%COMMONPROGRAMFILES%\CLAP` and
`%LOCALAPPDATA%\Programs\Common\CLAP`, and every folder the `CLAP_PATH` environment variable
lists; copy the file into one of them, or add the build folder to `CLAP_PATH`.

## Host sources

The plugin compiles `rig_controller.cpp`, `slot_graph.cpp`, `session_timeline.cpp`,
`rig_persistence.cpp` and `rig_topology.cpp` from `applications/host_gui` by source list, the
way `tests/CMakeLists.txt` does. `docs/design/REMAINING_WORK.md` is where turning that list into
a library belongs.

## Dependencies

Links `SushiDSP::sushi_dsp`, `SushiDSP::node_registry`, `SushiDSP::tubescreamer`,
`SushiDSP::jcm800`, `SushiDSP::rig_preset`, `SushiDSP::capture`, `SushiDSP::room`,
`SushiDSP::microphone` and `nlohmann_json`. CLAP 1.2.2 is MIT and header-only; the root
`CMakeLists.txt` fetches it by tag and the plugin links its `clap` target privately. No CLAP
type appears outside `clap_plugin.hpp`, `clap_plugin.cpp`, `clap_extensions.cpp` and
`clap_entry.cpp`.

## What the first plugin does not do

- It has no editor. A rig is built and edited in the standalone host and reaches the plugin as
  a preset.
- It cannot add, remove, reorder or bypass a slot. A bypassed slot's parameters are listed and
  saved, and are not heard.
- It does not load a cabinet impulse response or an HRTF set by itself. A cabinet comes from the
  path the preset holds, so a preset moved to a machine without that file loses its cabinet. A
  room runs on its spherical-head response.
- It reads and writes 32-bit float buffers only, on one stereo port each way.
- It does not apply a parameter event at its sample offset inside the block.
- `clap_plugin.reset` clears nothing: `AudioNode::reset()` belongs to the control thread, and
  CLAP calls reset on the audio thread.
- The timeline every `RigController` carries is built and never played: no lanes, no transport,
  no recording, no metronome.
- No VST3 or AU shell exists.

## Logging, counters and profiling

The plugin logs under `K_PLUGIN_LOG_CATEGORY`, declared in `plugin_log_category.hpp`, and its
records reach the host through `sushidsp_join_log` or stderr. `guarded` takes the callback's
name and writes one `ERROR` naming it and what was thrown before it returns the callback's failure
value. A startup rig the variable names but the plugin cannot open, or the adapter refuses, writes
one `WARN` with the path or the reason. A failed `activate` and a state that will not load or save
each write one `ERROR` with the adapter's reason.

The real-time path never logs. `RigAdapter` counts a block with the wrong channel count or frame
count (`format_mismatch`), the parameter changes it skipped because the routing belonged to another
graph (`stale_routing`), those that named an unknown id (`unknown_parameter`) and those a full
queue dropped. The instance counts the blocks it answered with `CLAP_PROCESS_ERROR` (`bad_block`).
The audio thread asks the host for a main-thread callback once in 512 blocks, and
`on_main_thread` writes the counters that moved as one `WARN` every five seconds.

When `SD_PROFILE_CONFIG` names a profiler configuration, `activate` starts a session and
`deactivate` ends it. The block is marked on the `dsp.block` timeline with the host's frame count
and the rate of `activate`, and the process zone is `dsp.process`. One session runs per
process: a second instance asks for a session, is refused, and writes one `WARN`. The plugin links the shared `SushiProfiler::SushiProfiler`, as every other target does.
