# AmpliTube-Style GUI — Design Spec

## Summary

`apps/host_gui` today draws every parameter as an `ImGui::SliderFloat` and every rig slot as a
row in a vertical `Selectable` list, all from one 567-line `main.cpp`. This spec replaces that
with the layout the reference screenshot shows: a large stage rendering the selected node's own
hardware panel with turnable knobs, a two-row signal-chain strip that draws parallel branches
and carries a bypass switch per slot, a searchable gear browser down the right edge, a preset
bar across the top, and input/output level meters along the bottom.

Everything is drawn procedurally with `ImDrawList`. No image files, no new third-party
dependency. The one core change is a two-line `RigGraph::is_empty()` accessor, needed because
bypassing every slot leaves a graph with no nodes.

## Goals

- Every amp and pedal parameter is edited by turning a knob, not by dragging a slider.
- Each node type gets its own panel layout: control positions, captions, colors, and panel
  proportions chosen for that device rather than a generic list.
- The signal chain is drawn as a graph, so a splitter feeding two branches into a mixer reads as
  two rows on screen instead of a flat list.
- Any slot can be bypassed and un-bypassed, and the bypass state survives a preset round trip.
- A player can see signal reaching the input and leaving the output.
- Audio device and channel selection is reachable at any time, not only before the first
  connection.
- `main.cpp` stops being the place where all GUI code lives.

## Non-goals (deferred)

- A tuner. Pitch detection is its own DSP feature with its own stability filtering and tests.
- A transport bar (record, play, loop, tempo). SushiDSP has no timeline.
- A preset browser. The preset path text box and its Save/Load buttons move to the top bar
  unchanged; browsing a preset library is later work.
- Photo-realistic textures. `PanelSkin` carries a `texture_id` field that is `nullptr` for every
  panel this spec defines, so a later spec can add bitmap skins without reshaping the type.
- Custom typography. The GUI keeps ImGui's built-in ProggyClean bitmap font, so panel lettering
  will not resemble the reference. Layout, knobs, and color will.
- More than two parallel branches. `RigController`'s v1 shape limit (one splitter, two branches,
  one mixer) is unchanged, and `ChainLayout` is specified against that limit.
- MIDI, automation, and audio recording.

## Architecture Overview

`apps/host_gui` gains a `ui/` directory. The split rule: code that computes lives in a different
file from code that draws, so the computing half is unit-tested without an ImGui context.

```
apps/host_gui/
  main.cpp                    window, ImGui setup, device wiring, main loop, region calls
  RigController.hpp/.cpp      existing; gains bypass
  SlotSelection.hpp           existing, unchanged
  ui/Geometry.hpp             Vec2 and Color, free of ImGui types
  ui/KnobMath.hpp/.cpp        value/normalized/angle mapping, drag arithmetic      [tested]
  ui/Widgets.hpp/.cpp         Knob(), ToggleSwitch()
  ui/Theme.hpp/.cpp           palette, ImGuiStyle setup
  ui/PanelLibrary.hpp/.cpp    PanelDescriptor table, automatic grid fallback       [tested]
  ui/StagePanel.hpp/.cpp      draws the selected node's panel
  ui/ChainLayout.hpp/.cpp     graph to rows and columns                            [tested]
  ui/ChainStrip.hpp/.cpp      draws the strip, drag and drop, bypass switches
  ui/GearBrowser.hpp/.cpp     search, categories, card grid
  ui/LevelProbe.hpp           carries a block peak from the audio thread           [tested]
  ui/MeterBar.hpp/.cpp        two level bars
  ui/TopBar.hpp/.cpp          preset path, Save/Load, settings button
  ui/DeviceSettings.hpp/.cpp  device and channel selection window
```

`main.cpp` keeps the window, the ImGui context, the device callback, the frame loop, and the
five region calls. Everything else moves out.

### Why the data layer avoids ImGui types

`PanelLibrary` and `ChainLayout` are tested by `sushidsp_tests`, which does not link `imgui` —
and cannot, because the `imgui` target only exists when `SUSHIDSP_BUILD_GUI` is `ON`. So those
two units, and `Geometry.hpp` under them, use their own types:

```cpp
namespace ui
{
    /// @brief A point or a size in panel-local coordinates.
    struct Vec2 { float x; float y; };

    /// @brief A color packed as 0xRRGGBBAA.
    using Color = std::uint32_t;

    /// @brief Packs four 8-bit channels into a Color.
    constexpr Color rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);
}
```

`Widgets.cpp` and `StagePanel.cpp` convert with a single `to_imu32()` helper at the drawing
boundary.

## Core Change

One accessor is added to `SushiDSP::RigGraph`:

```cpp
/// @brief Reports whether compile() produced an empty schedule, i.e. the graph has no live
/// node. Real-time safe: reads one vector's size, allocates nothing.
/// @return True if process() would write nothing.
bool is_empty() const noexcept;   // returns topological_order_.empty()
```

The audio callback needs this. When every slot is bypassed the published graph holds no node at
all, `process()` writes nothing into `context.outputs[0]`, and the host's `mono_output` vector
replays the previous block forever. `nodes()` returns a `std::vector` by value and cannot be
called from the audio thread, so the query has to be its own O(1) accessor. With it, the callback
copies input to output when the graph is empty, which is the correct meaning of "every pedal is
bypassed".

The same commit fixes a stale Doxygen comment above `latency_samples()`. It reads "Sums the
chain; a parallel branch-and-rejoin graph would need the max over branches instead", which
described the pre-DAG implementation. `RigGraph.cpp:225` computes `latency_at[sink_handle_]`,
the longest path, so the comment contradicts the code it documents.

## Knob Math and Widgets

### `ui/KnobMath.hpp`

```cpp
namespace ui
{
    /// @brief The knob's sweep, from the 7 o'clock position clockwise to 5 o'clock.
    inline constexpr float kKnobSweepRadians = 4.71238898f;   // 270 degrees
    inline constexpr float kKnobStartRadians = 2.35619449f;   // 135 degrees, measured clockwise from +x

    /// @brief Pointer travel, in pixels, that sweeps the knob end to end.
    inline constexpr float kKnobDragPixels     = 200.0f;
    inline constexpr float kKnobFineDragPixels = 800.0f;

    float value_to_normalized(const SushiDSP::ParameterDescriptor& param, float value);
    float normalized_to_value(const SushiDSP::ParameterDescriptor& param, float normalized);
    float normalized_to_angle(float normalized);
    float apply_drag(float normalized, float pixels_dy, bool fine);
}
```

Mapping per `ParamKind`:

| Kind | Mapping |
|---|---|
| `Linear` | linear between `min` and `max` |
| `Decibel` | linear in decibels, i.e. linear between `min` and `max`, which are already dB |
| `Logarithmic` | `normalized = log(value/min) / log(max/min)`; requires `min > 0`, and falls back to linear when it is not |
| `Boolean` | not a knob; `ToggleSwitch` handles it |
| `Choice` | not a knob; drawn as a `Readout` control, see below |

`apply_drag` clamps the result to `[0, 1]`. Upward pointer travel (negative `pixels_dy` in ImGui's
coordinates) increases the value.

### `ui/Widgets.hpp`

```cpp
namespace ui
{
    /// @brief Draws a turnable knob for one parameter and applies pointer edits to *value.
    /// @return True on the frames where *value changed.
    bool Knob(const char* id, const SushiDSP::ParameterDescriptor& param, float* value, float diameter);

    /// @brief Draws a two-state rocker switch.
    /// @return True on the frames where *on changed.
    bool ToggleSwitch(const char* id, bool* on, float width, float height);
}
```

`Knob` behavior: left-button vertical drag turns it; holding Shift switches to
`kKnobFineDragPixels`; a double click restores `param.default_value`; hovering shows a tooltip
with the value and `param.unit`. It draws a shaded outer ring, tick marks along the sweep, a
filled arc from the start of the sweep to the current angle in the accent color, and a pointer
line. The caption is drawn by the caller, not by `Knob`, so `StagePanel` controls text placement.

## Panels and the Stage

### `ui/PanelLibrary.hpp`

```cpp
namespace ui
{
    enum class ControlKind { Knob, Switch, Readout };

    /// @brief One control's place on a panel. position and size are panel-relative: position is
    /// the control's center in [0,1] on both axes, size is its diameter or width as a fraction
    /// of panel height, so a panel scales with the window without re-laying out.
    struct ControlPlacement
    {
        const char* param_id;
        ControlKind kind;
        Vec2        position;
        float       size;
        const char* caption;      ///< nullptr means use ParameterDescriptor::label
    };

    /// @brief A panel's colors. texture_id is the hook a later bitmap-skin spec fills in; every
    /// panel defined today leaves it nullptr and the drawing code ignores it.
    struct PanelSkin
    {
        Color       body;         ///< the enclosure or cabinet
        Color       face;         ///< the control plate
        Color       accent;       ///< filled arcs, pointer lines, selected borders
        Color       text;
        const char* texture_id;
    };

    struct PanelDescriptor
    {
        const char*             descriptor_id;
        Vec2                    aspect;       ///< width and height in arbitrary units; only the ratio matters
        PanelSkin               skin;
        const ControlPlacement* controls;
        std::size_t             control_count;

        /// @brief Optional per-control visibility rule. nullptr means every control is drawn.
        /// Only the mixer sets it, to hide gain knobs for inputs nothing is connected to.
        bool (*is_control_visible)(std::size_t control_index, std::size_t upstream_count);
    };

    /// @brief Looks up the hand-authored panel for a node type.
    /// @return The panel, or nullptr when no entry exists for descriptor_id.
    const PanelDescriptor* panel_for(const char* descriptor_id);

    /// @brief Builds a generic grid panel from a NodeDescriptor's parameter list, for a node type
    /// with no hand-authored entry. Lays parameters out in row-major order across
    /// ceil(sqrt(parameter_count)) columns.
    PanelDescriptor auto_panel(const SushiDSP::NodeDescriptor& node,
                               std::vector<ControlPlacement>& storage);
}
```

`auto_panel` writes its placements into a caller-owned `storage` vector and points the returned
descriptor at it, because a `PanelDescriptor` holds a non-owning `const ControlPlacement*` and
the generated placements have to outlive the call. It picks each control's kind from the
parameter: `ParamKind::Boolean` becomes `ControlKind::Switch`, `ParamKind::Choice` becomes
`ControlKind::Readout`, and everything else becomes `ControlKind::Knob`.

### Panel entries

**JCM800** (`sushidsp.jcm800`), aspect 900 by 420. Black tolex body `rgba(26,26,28)`, gold
control plate `rgba(158,124,58)`, accent `rgba(214,168,74)`.

Six main knobs, diameter 0.14, at y 0.78, laid out left to right in the reference's order:

| param_id | x | caption |
|---|---|---|
| `presence` | 0.34 | Presence |
| `bass` | 0.42 | Bass |
| `middle` | 0.50 | Middle |
| `treble` | 0.58 | Treble |
| `master` | 0.66 | Master |
| `preamp` | 0.74 | Pre Amp |

Two trim knobs, diameter 0.10, at y 0.78, on the left where the reference puts its power and
sensitivity switches: `input_trim` at x 0.13, `output` at x 0.21.

The reference's Power and Lo Sensitivity switches have no counterpart in our JCM800 model, so
they are not drawn. A control that does nothing is worse than an absent one.

**TubeScreamer** (`sushidsp.tube_screamer`), aspect 320 by 560. Green enclosure
`rgba(46,112,58)`, darker face `rgba(32,78,42)`, cream accent `rgba(226,214,178)`.

| param_id | position | diameter | caption |
|---|---|---|---|
| `level` | (0.50, 0.13) | 0.13 | Level |
| `drive` | (0.24, 0.22) | 0.13 | Drive |
| `tone` | (0.76, 0.22) | 0.13 | Tone |
| `input_trim` | (0.50, 0.36) | 0.09 | Input Trim |

**Splitter** (`sushidsp.splitter`) has no parameters. Its entry sets an empty control list and a
neutral dark skin, so the stage shows a labeled plate rather than an empty grid.

**Mixer** (`sushidsp.mixer`), aspect 640 by 300. Eight `gain_N` knobs in one row, diameter 0.16,
at y 0.55, x from 0.10 to 0.90 in even steps. Its `is_control_visible` returns
`control_index < upstream_count`, so a mixer with two connected inputs shows two gain knobs.

### `ui/StagePanel.hpp`

```cpp
namespace ui
{
    /// @brief Draws the selected slot's panel into the current window and applies knob edits
    /// through the controller. Draws nothing when selected is std::nullopt.
    void DrawStagePanel(RigController& rig, std::optional<RigController::SlotId> selected);
}
```

`StagePanel` resolves the panel with `panel_for()`, falls back to `auto_panel()`, fits the
panel's aspect inside the region while preserving the ratio, draws the body and face, then walks
the placements. For each one it reads the value with `rig.get_parameter()`, draws the control,
and writes changes back with `rig.set_parameter()` — the controller's stored value, never the
live node's, because a write only reaches the node on the audio thread's next block.

The JCM800 cabinet IR field stays as it is today, drawn below the panel when the selected slot is
a JCM800.

## Chain Layout and Strip

### `ui/ChainLayout.hpp`

```cpp
namespace ui
{
    struct ChainCell { RigController::SlotId slot; int row; int column; };
    struct ChainLink { std::size_t from_cell; std::size_t to_cell; };

    struct ChainView
    {
        std::vector<ChainCell> cells;
        std::vector<ChainLink> links;
        int                    row_count;
        int                    column_count;
    };

    /// @brief Places every slot on a row and column grid for drawing. Column comes from
    /// topological depth: a node sits one column right of its deepest upstream. Row is 0 for a
    /// linear run; when a node has two downstream edges, its first downstream keeps row 0 and its
    /// second takes row 1, and every node reachable from that second branch keeps row 1 until a
    /// node with two upstream edges rejoins at row 0.
    /// @param slots Every live slot, in RigController::slot_order() order.
    /// @param edges Every edge, as (from, to) pairs.
    /// @return The grid. cells is empty and both counts are 0 for an empty rig.
    ChainView layout_chain(const std::vector<RigController::SlotId>& slots,
                           const std::vector<std::pair<RigController::SlotId, RigController::SlotId>>& edges);
}
```

The function takes the graph, not the controller, so a test builds a shape from two vectors
without constructing a `RigController`, a `PrepareInfo`, or any DSP node.

`layout_chain` is specified against `RigController`'s v1 shape limit: one splitter, two branches,
one mixer, no nesting. Two shapes fall outside it and get a defined but unhelpful answer rather
than an assertion. A splitter with three or more downstream edges places the third and later
branches on row 1 as well, overlapping the second. A splitter inside a branch places its own
second branch on row 1 too, overlapping its parent. Neither shape can be built through the GUI,
so both are documented consequences of the limit rather than defects to fix here.

### `ui/ChainStrip.hpp`

```cpp
namespace ui
{
    /// @brief Draws the two-row chain strip and handles selection, drag-reorder, gear drops,
    /// bypass toggles, and removal.
    /// @param selected In/out: the selected slot, cleared when that slot stops existing.
    /// @param created_slot Out: the id of the slot a gear drop just created, or std::nullopt when
    /// this frame created none. main.cpp uses it to run its own post-insert step, installing the
    /// bundled cabinet impulse response on a fresh JCM800.
    void DrawChainStrip(RigController& rig, std::optional<RigController::SlotId>& selected,
                        std::optional<RigController::SlotId>& created_slot);
}
```

Each cell draws a card: the node's name, a small procedural icon derived from its category, a
bypass switch, and a remove button. The selected card takes an accent border. A bypassed card
draws at 45% opacity with its bypass switch lit.

Links are drawn as lines with an arrow head between card edges: a straight line within a row, and
an S-curve through the vertical gap when the rows differ.

Drag and drop keeps today's payload names, `RACK_SLOT_ID` and `GEAR_DESCRIPTOR_ID`, and today's
semantics: dropping a slot on a card calls `move_before()`, dropping a gear card calls
`insert_before()`, and dropping a gear card past the last cell calls `append()`.

## Bypass

### `RigController` interface

```cpp
/// @brief Marks a slot bypassed or active and republishes. A bypassed slot keeps its parameters,
/// its cabinet path, and its edges; publish() simply leaves it out of the graph and connects its
/// upstream neighbors straight to its downstream neighbors.
void set_bypassed(SlotId slot, bool bypassed);

/// @brief Reports whether a slot is bypassed. False for a slot that is not live.
bool is_bypassed(SlotId slot) const;
```

`Slot` gains one `bool bypassed = false;` member.

### One graph builder, two callers

`publish()` and `save_to_json()` both need to turn slots and edges into a `RigGraph`, and they
need it with and without bypass applied. They share one private helper:

```cpp
/// @brief Builds a fresh RigGraph from the current slots and edges and compiles it. Const: a
/// slot whose descriptor_id the node registry does not know is skipped rather than erased, and
/// handles_out therefore names exactly the slots that made it into the graph.
/// @param apply_bypass When true, a bypassed slot is left out and its upstream neighbors are
/// connected directly to its downstream neighbors. When false, every live slot is added and
/// every edge is honored, which is the shape a preset must record.
/// @param handles_out Receives the slot-to-handle mapping for the graph that was built.
/// @return The compiled graph.
std::unique_ptr<SushiDSP::RigGraph> build_graph(bool apply_bypass,
    std::unordered_map<SlotId, SushiDSP::NodeHandle>& handles_out) const;
```

`publish()` calls it with `true`, then erases from `slots_`, `slot_order_`, and `edges_` any slot
that `handles_out` does not name and that is not merely bypassed — which preserves today's
pruning of an unknown descriptor id, now expressed as a step of `publish()` rather than a side
effect of building. It then publishes the graph through the `RtGraphSlot`.

`save_to_json()` calls it with `false` and serializes that graph, so a bypassed node still
appears in the preset. It stays `const` because `build_graph` is. With no live slot at all it
returns `{"nodes":[]}` without building anything, the same answer it gives today when no graph
has been published.

Bridging rule, applied when `apply_bypass` is true: for each bypassed slot, every
(upstream, downstream) pair of its edges becomes a direct edge. A bypassed slot at the head of a
chain has no upstream, so its downstream simply loses that incoming edge; a bypassed slot at the
tail leaves its upstream as the new sink. Two bypassed slots in a row bridge transitively,
because the rule is applied over the fully-collapsed edge set rather than one slot at a time.

Bridging never adds an edge that already exists, and it respects `RigGraph::kMaxFanInOut`: if
collapsing would push a node past 8 edges on either side, `build_graph` throws
`std::runtime_error` and `set_bypassed` leaves the bypass flag unchanged, so the rig keeps
working. This is unreachable with v1's shape limit and exists so a later N-way branching spec
finds a defined behavior rather than an assertion.

### Preset round trip

`save_to_json()`'s existing `AnnotateNodeFn` gains one line: it writes `"bypassed": true` for a
bypassed slot and omits the field otherwise, matching how `"cabinet_ir"` is written only when
non-empty. `load_from_json()`'s existing `OnNodeLoadedFn` reads it back the same way it reads
`cabinet_ir`. A preset written before this change has no `"bypassed"` field and loads with every
slot active.

## Gear Browser, Top Bar, Device Settings, Meters

**Gear browser.** A search box filters on a case-insensitive substring of `NodeDescriptor::name`.
Categories come from `NodeEntry::category` and draw as collapsible headers, open by default. Each
entry is a card with a procedural icon and its name, and is a drag source with the existing
`GEAR_DESCRIPTOR_ID` payload.

**Top bar.** The preset path text box and its Save and Load buttons move here unchanged, plus a
settings button that opens the device window and a label naming the connected device and its
channel selection.

**Device settings.** Today's `AppState::SelectingDevice` screen becomes a window that opens on
its own at startup and reopens from the top bar. Connecting runs exactly today's sequence:
`device.reset()`, construct the new device, derive `PrepareInfo` from `device->format()`,
`rig_controller_storage.emplace(info)`, size `mono_output` and `callback_silence`, then
`device->start(...)`. Reconnecting while running therefore replaces the rig with an empty one,
which the window states in its confirmation text before it happens.

**Meters.**

```cpp
namespace ui
{
    /// @brief Carries a block's peak magnitude from the audio thread to the GUI thread.
    class LevelProbe
    {
    public:
        /// @brief Records the largest magnitude in a block. Audio thread; allocation-free,
        /// lock-free, and never blocks.
        void report(const float* samples, std::uint32_t count) noexcept;

        /// @brief Reads the largest peak reported since the previous call and resets the
        /// accumulator to zero. GUI thread only.
        float read_peak() noexcept;

    private:
        std::atomic<float> peak_{ 0.0f };
    };
}
```

`report` raises the stored peak with a `std::atomic::compare_exchange_weak` loop, so it never
takes a lock and never blocks the audio thread. `main.cpp` owns one probe for input and one for
output, declared in the same outer scope as `mono_output` so the callback's captured references
stay valid for the device's whole lifetime.

`MeterBar` owns the ballistics, not the probe: it keeps the displayed level per bar, jumps
straight to a new peak that is higher, and multiplies the displayed level by 0.85 each frame when
the new peak is lower. It draws two horizontal bars, green below -6 dBFS and red at or above
0 dBFS, with the current level in dBFS as text.

## Region Layout and Theme

Five windows, all `NoTitleBar | NoMove | NoResize | NoCollapse | NoSavedSettings`, positioned
from the framebuffer size every frame so the layout survives a window resize:

| Region | Position and size |
|---|---|
| Top bar | full width, y 0, height 48 |
| Stage | x 0, y 48, width `w - 300`, height `(h - 88) * 0.58` |
| Chain strip | below the stage, same width, the remaining height above the meter bar |
| Gear browser | x `w - 300`, y 48, width 300, down to the meter bar |
| Meter bar | full width, height 40, flush with the bottom |

`ui/Theme.hpp` declares the two entry points the shell needs:

```cpp
namespace ui
{
    /// @brief Writes the palette, corner radii, and spacing into ImGui's active style. Call once,
    /// after ImGui::CreateContext().
    void apply_theme();

    /// @brief The palette, for drawing code that reaches ImDrawList directly.
    const struct Palette& palette();
}
```

Palette, set once in `apply_theme()`:

| Role | Color |
|---|---|
| Window background | `rgba(18,18,20)` |
| Panel background | `rgba(28,28,32)` |
| Accent | `rgba(214,168,74)` |
| Text | `rgba(228,228,232)` |
| Muted text | `rgba(140,140,148)` |
| Meter normal | `rgba(96,200,112)` |
| Meter clipping | `rgba(220,72,60)` |

## Testing

| Unit | What is verified |
|---|---|
| `KnobMath` | endpoints and midpoint for `Linear` and `Decibel`; `value_to_normalized` and `normalized_to_value` round-trip; `Logarithmic` mapping and its fallback when `min <= 0`; `apply_drag` clamps at both ends; Shift travel is exactly four times coarse travel; `normalized_to_angle` spans `kKnobSweepRadians` |
| `PanelLibrary` | a known id returns its panel; an unknown id returns `nullptr`; `auto_panel` produces `parameter_count` placements in `ceil(sqrt(n))` columns; the mixer's `is_control_visible` shows exactly `upstream_count` knobs |
| `PanelLibrary` consistency | every `param_id` in every table entry exists in that node's `NodeDescriptor`, and every entry's `descriptor_id` is in `node_registry()`. This catches the table drifting from the descriptors after a parameter is renamed |
| `ChainLayout` | a linear chain occupies one row with ascending columns; splitter into two branches into a mixer gives `row_count == 2` with the branches on rows 0 and 1 and the mixer back on row 0; an empty input gives an empty view; every edge produces exactly one link |
| `RigController` bypass | bypassing a middle slot bridges its neighbors and the rig still processes; un-bypassing restores the original graph; two consecutive bypassed slots bridge transitively; a bypassed head slot leaves a valid graph; `save_to_json` includes a bypassed node and its `"bypassed"` field; `load_from_json` restores the flag; a preset without the field loads with every slot active; bypassing every slot yields an empty graph |
| `RigGraph::is_empty` | true before any `add_node`; true after compiling an empty graph; false for a single-node graph |
| `LevelProbe` | `report` finds the peak magnitude including negative samples; `read_peak` returns it and resets the accumulator to zero; a second `read_peak` with no intervening `report` returns 0; an RT-safety trap test proves `report` performs no allocation |

The drawing code is not unit-tested. Its correctness is checked by running
`sushidsp_host_gui` and looking at it.

## Global Constraints

- `IAudioNode::process()` and everything reachable from it stays allocation-free, lock-free,
  syscall-free, and throw-free. `LevelProbe::report()` is called from the audio callback and is
  bound by the same rule; `RigGraph::is_empty()` is called from the audio callback and must stay
  O(1) with no allocation.
- `RigController`'s methods remain GUI-thread only. Bypass is a structural edit and republishes,
  exactly like the existing structural edits.
- No new third-party dependency, no image file, no font file. ImGui stays at v1.90.9 and GLFW at
  3.4.
- `ui/Geometry.hpp`, `ui/KnobMath.*`, `ui/PanelLibrary.*`, `ui/ChainLayout.*`, and
  `ui/LevelProbe.hpp` must not include any ImGui header, because `sushidsp_tests` compiles them
  and does not link `imgui`.
- Every new file carries the project's license header, a `@file` block with one `@brief` sentence
  and `@author`, and Doxygen `@brief` on every symbol, per `docs/CONTRIBUTING.md`.
- Builds and tests run through `sd` only, never `cmake` or `ctest` directly.
- The existing preset schema stays backward compatible: a file written before this change loads
  with every slot active.
