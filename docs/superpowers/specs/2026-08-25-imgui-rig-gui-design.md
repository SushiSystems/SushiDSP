# ImGui Rig GUI — Design Spec

## Summary

A new application, `apps/host_gui`, gives SushiDSP an AmpliTube-style graphical
front end: a dockable ImGui window backed by GLFW + OpenGL3, showing a
horizontal rack strip of the live `RigGraph` chain, a gear panel to add
pedals/amps by dragging from a categorized list, per-node parameter sliders,
and JSON preset save/load. It runs against either audio backend
(`AsioAudioDevice` or `Sdl2AudioDevice`) chosen at startup.

Reference layout: AmpliTube 5's rack-strip + gear-panel + parameter-panel
arrangement (user-supplied screenshot), scoped down for v1 — no transport bar,
no recording/looper, no cloud features, no photo-realistic gear art.

## Goals (v1)

- Dynamic rig chain: add, remove, and reorder nodes from the existing node
  catalog (`TubeScreamer`, `Jcm800`) via drag-and-drop, with **zero audio
  dropout** on chain changes.
- Live parameter control via plain ImGui sliders (not skinned knobs).
- Save/load the chain + parameter values as a JSON preset.
- Works with either `AsioAudioDevice` or `Sdl2AudioDevice`, selected at
  startup.

## Non-goals (deferred)

- Photo-realistic or custom-drawn gear art (flat ImGui widgets only).
- Transport bar, recording, looper, metronome, cloud sync, gear search box.
- Branching/parallel signal paths in the preset format (linear chain only).
- Automated UI testing of the ImGui render layer (manual/visual verification
  only — no headless ImGui test harness exists in this project).
- A `category` field on `NodeDescriptor` — categories are assigned in the
  registry, not the node itself.

## Architecture Overview

Three layers change:

1. **Core extensions** (`include/SushiDSP/core/`): generic parameter
   get/set on `IAudioNode`; `RigGraph` gains removal/iteration; a new
   `RtGraphSlot` provides lock-free, glitch-free chain swapping for the
   audio thread.
2. **`modules/node_registry`** (new): a small module depending on `core`,
   `tubescreamer`, and `jcm800` that maps a string id to a `NodeDescriptor`
   and a category label, and constructs nodes by id.
3. **`apps/host_gui`** (new): the GLFW/ImGui application — device selection,
   rack strip, gear panel, parameter panel, JSON preset I/O.

Chain edits (add/remove/reorder node, load preset) never stop the audio
device. The GUI/main thread builds a complete new `RigGraph` off the audio
thread (`add_node`/`connect`/`compile`), then publishes it to `RtGraphSlot`,
which the audio callback reads with a single atomic load per callback.

## Core Changes

### `IAudioNode` — generic parameter access

```cpp
virtual float get_parameter(const char* id) const = 0;
virtual void set_parameter(const char* id, float value) = 0;
```

`TubeScreamer` and `Jcm800` implement these by dispatching on `id` (string
compare) to their existing named setters/getters. `id` strings are exactly
the `ParameterDescriptor::id` values already declared in each node's
`NodeDescriptor`. An unknown `id` is a no-op for `set_parameter` and returns
`0.0f` from `get_parameter`.

### `RigGraph` — removal and iteration

```cpp
void remove_node(NodeHandle);
void disconnect(NodeHandle from, NodeHandle to);
IAudioNode* node(NodeHandle) const;
std::vector<NodeHandle> nodes() const;
```

All four are prepare-time-only calls (same thread-safety contract as
`add_node`/`connect`/`compile`) — never called from the audio thread.

### `RtGraphSlot` — lock-free chain swap (new: `core/RtGraphSlot.hpp`)

```cpp
class RtGraphSlot {
public:
    // Non-RT thread. Takes ownership of a fully compiled graph and makes
    // it the active one for the next audio callback.
    void publish(std::unique_ptr<RigGraph> new_graph);

    // RT thread only. Allocation-free, lock-free, syscall-free: one atomic
    // load of the active pointer, one atomic store publishing the
    // generation the caller is about to use.
    RigGraph* acquire_for_audio_thread();

    // Non-RT thread. Frees any previously retired graph the audio thread
    // has provably moved past (its published generation advanced beyond
    // the graph's retirement generation). Call periodically (e.g. once per
    // GUI frame).
    void reclaim();

private:
    std::atomic<RigGraph*> active_;
    std::atomic<std::uint64_t> audio_thread_generation_;
    std::uint64_t publish_generation_ = 0;
    std::vector<std::pair<std::uint64_t, std::unique_ptr<RigGraph>>> retired_;
};
```

Both `AsioAudioDevice` and `Sdl2AudioDevice`'s callback signatures change to
take an `RtGraphSlot&` (or equivalently, the owning app calls
`acquire_for_audio_thread()` inside its own `DeviceCallback` lambda and
passes the returned `RigGraph*` into `process()`) — no change to
`IAudioDevice`'s public interface itself, since `DeviceCallback` is already
just `std::function<void(...)>` supplied by the app. `apps/host` and
`apps/host_asio` are unaffected: they keep constructing a single `RigGraph`
directly and never touch `RtGraphSlot`.

## `modules/node_registry`

```cpp
namespace SushiDSP {
    struct NodeCategory { const char* name; };

    struct NodeEntry {
        const NodeDescriptor* descriptor;
        NodeCategory category;
    };

    const std::vector<NodeEntry>& node_registry();

    std::unique_ptr<IAudioNode> make_node(const char* id, const PrepareInfo&);
}
```

Static table, hand-maintained: `{ &tube_screamer_descriptor(), {"Distortion"} }`,
`{ &jcm800_descriptor(), {"Amp"} }`. Adding a future node module means adding
one line here — nothing else in `apps/host_gui` changes. `make_node` looks up
`descriptor->id == id` linearly (catalog is small; no need for a map) and
calls `descriptor->make(info)`.

## `apps/host_gui`

### Dependencies

- `SushiDSP::core`, `SushiDSP::node_registry`
- GLFW + Dear ImGui, pulled via CMake `FetchContent` (not vendored under
  `third_party/`), gated behind a new `SUSHIDSP_BUILD_GUI` option — mirrors
  the existing `SUSHIDSP_BUILD_ASIO`/`SUSHIDSP_BUILD_SDL2` opt-in pattern.
  Renderer: OpenGL3 backend (`imgui_impl_glfw` + `imgui_impl_opengl3`).
- `nlohmann::json`, header-only, also via `FetchContent`, used only by
  `apps/host_gui` for preset serialization.

### Device selection

At startup, a combo box lists available devices: ASIO drivers (if
`SUSHIDSP_BUILD_ASIO` compiled in and drivers are present) plus a fixed
"SDL2 Default" entry. Selecting one constructs the matching `IAudioDevice`
behind a common pointer; the rest of the UI is device-agnostic.

### Rack strip

Horizontal row of boxes, one per node in chain order, each labeled with the
node's `NodeDescriptor::name`. Two drag-and-drop interactions via ImGui's
`BeginDragDropSource`/`BeginDragDropTarget`:

- **Add**: drag a gear-panel entry onto the rack strip → inserts a new node
  at the drop position.
- **Reorder**: drag a rack-strip box to a new position → moves that node.

Both interactions run the same path: build a new `RigGraph` reflecting the
new order (via `node_registry::make_node` for the freshly added entry, and
the existing nodes' current parameter values re-applied to a new instance
via `set_parameter`), `compile()` it, and `RtGraphSlot::publish()` it. A
node can also be removed with a delete button on its rack-strip box.

### Gear panel

Right-hand column: `node_registry()` entries grouped under a header per
distinct `NodeCategory::name`. No search box (out of scope — see
Non-goals).

### Parameter panel

Below the rack strip, for whichever rack box is currently selected: one
`ImGui::SliderFloat` per `descriptor->parameters[i]`, range
`[min, max]`, initial value from `node->get_parameter(id)`. `on edit` calls
`node->set_parameter(id, new_value)` directly on the *live* node instance —
parameter tweaks do **not** go through `RtGraphSlot` (no structural change,
no swap needed; the node object itself is unchanged, only its internal
state).

## Preset Format (JSON)

```json
{
  "nodes": [
    { "type": "tube_screamer", "params": { "drive": 0.6, "tone": 0.4, "level": 0.7 } },
    { "type": "jcm800", "params": { "preamp": 0.5, "master": 0.5, "bass": 0.5, "middle": 0.5, "treble": 0.5, "presence": 0.5 } }
  ]
}
```

- `nodes` is ordered; `nodes[i]` connects to `nodes[i+1]` (linear chain
  only, matching the current rack strip — no branching in v1).
- `type` matches `NodeDescriptor::id`, resolved through
  `node_registry::make_node`.
- `params` keys match `ParameterDescriptor::id` values; applied via
  `set_parameter` after construction.
- **Save**: iterate `RigGraph::nodes()` in chain order, read each via
  `get_parameter` for every entry in its descriptor's `parameters` array.
- **Load**: parse JSON, build a new `RigGraph` the same way a drag-and-drop
  edit would, publish via `RtGraphSlot` — preset load reuses the rack
  strip's swap path, not a separate code path.

## Testing

- `RtGraphSlot` concurrency test: one thread loops
  `acquire_for_audio_thread()`, another loops `publish()`/`reclaim()`;
  run under ThreadSanitizer to confirm no data race or use-after-free. This
  is an RT-safety trap test per the project's existing convention.
- `get_parameter`/`set_parameter` round-trip test per node (`TubeScreamer`,
  `Jcm800`): set every declared parameter, confirm `get_parameter` returns
  it; confirm an unknown id is a safe no-op / returns `0.0f`.
- `RigGraph::remove_node`/`disconnect`/`nodes()` unit tests.
- `node_registry::make_node` test: each registered id constructs the
  expected concrete type (checked via `ports()`/`get_parameter` behavior,
  since there's no RTTI-based type query).
- Preset save/load round-trip test: build a chain, set distinctive
  parameter values, save to JSON, load into a fresh `RigGraph`, assert
  every parameter matches.
- The ImGui render/interaction layer itself (window, drag-and-drop,
  sliders) is verified manually — no automated coverage, stated here
  explicitly rather than silently skipped.

## Global Constraints

- Everything reachable from `IAudioNode::process()` remains allocation-free,
  lock-free, syscall-free, throw-free — `RtGraphSlot::acquire_for_audio_thread()`
  included.
- `RigGraph` structural methods (`add_node`, `remove_node`, `connect`,
  `disconnect`, `compile`) are prepare-time-only; never called from the
  audio thread, matching the existing `add_node`/`connect`/`compile`
  contract.
- New CMake targets (`SushiDSP::node_registry`, `SushiDSP::host_gui`) follow
  the existing `SushiDSP::<target>` alias convention and the
  `SUSHIDSP_BUILD_<X>` opt-in-option pattern already used for SDL2/ASIO.
- No change to `apps/host` or `apps/host_asio`'s behavior — `RtGraphSlot`
  is additive and optional; both keep using a plain `RigGraph` directly.
