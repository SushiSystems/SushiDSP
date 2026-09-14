# Host GUI Phase 2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development
> (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use
> checkbox (`- [ ]`) syntax for tracking.

**Status:** draft, awaiting owner approval (2026-09-14).

**Goal:** Build `docs/design/gui_ux_redesign.md` §10-§18 in `apps/host_gui`: one resizable
workspace, a chain-type picker that sets the whole rig's topology and routes audio through it,
pre/post stages, a list/grid gear browser, a bottom transport bar with inline meters, and the
mini-DAW as a full workspace swap.

**Architecture:** A new ImGui-free `RigTopology` owns the chain type and the slot lists (series,
pre, branches, post). A `TopologyApplier` compiles that model into `RigController` edges with one
publish, placing the hidden splitter and mixer slots itself. Every piece of logic that needs a
test (topology, split geometry, divider clamping, gear filtering, dB text) lives in an ImGui-free
unit compiled into `sushidsp_tests`; the ImGui draw units only read those results and forward
input.

**Tech Stack:** C++17, Dear ImGui, GoogleTest, `sd build` / `sd test`.

**Spec:** `docs/design/gui_ux_redesign.md` §10-§19, with the mockup
<https://claude.ai/code/artifact/86c82259-6c54-4fcf-be65-424903fd5c90> as the reference for
spacing and color.

## Owner decisions this plan rests on (2026-09-14)

- The chain type routes real audio: a split topology compiles to splitter, branch and mixer
  slots in `RigController`, not a drawing only.
- Topology state lives in a new `RigTopology` brick, not inside `RigController`.
- The §12 Pitch field drives a real, speed-independent pitch shift of **track playback**. The
  DSP kernel is a **separate plan**. This plan ships the field disabled, reading `0 st`, with a
  tooltip naming that plan; wiring it is the last task of the pitch plan, not of this one.

## Open items that need an answer before Wave 3

1. **Deletion.** Wave 3 deletes `ui/ChainStrip.{hpp,cpp}`, `ui/SplitterPicker.{hpp,cpp}`,
   `ui/ChainLayout.{hpp,cpp}` and `tests/functional/unit/test_chain_layout.cpp`, and
   `RigController::add_branch` loses its last caller. Approve deleting them, or keep them.
2. **Empty branch.** An empty branch compiles to a direct splitter→mixer edge. `RigController`
   dedupes edges, so two empty branches become one edge and the dry signal enters the mixer once,
   not twice. Proposed: accept that (an all-empty split sounds like a cable). The alternative is a
   unity pass-through slot per empty branch.
3. **Loading a preset.** `load_from_json` can produce any graph. Proposed: a preset whose graph is
   a single linear run loads as Series; any other shape loads as Series built from `slot_order()`
   with a status message saying the parallel shape was flattened, because §19 leaves persisting a
   parallel topology out of scope.
4. **Plan location.** The global rules put agent plans in `docs/agent/plans/`; this repository
   keeps every plan in `docs/superpowers/plans/`, and the spec's status line cites that path. This
   plan follows the repository.

## Global Constraints

- C++17 only; Allman braces; `PascalCase` types, `snake_case` functions, trailing `_` members,
  `kPascalCase` constants; the 76-column license box copied from
  `include/SushiDSP/core/PortLayout.hpp`; `@file`/`@brief` Doxygen on every file and symbol.
- `sushidsp_tests` does not link ImGui. Any unit with a test includes no `imgui.h`.
- Tests live in `tests/functional/unit/test_<name>.cpp`, suite `Unit_Dsp`, case names that
  state the behaviour.
- Subagents never run `sd`, `cmake`, `ninja`, `ctest` and never edit a `CMakeLists.txt`. The
  coordinator makes every CMake edit listed per wave, then asks the owner to run the build.
- No change to `SplitterNode`/`MixerNode` DSP, node settings panels, or the node catalog (§19).
- No persistence of divider sizes, view mode or chain type (§18, §19).
- Every edit that changes the rig goes through `run_edit`-style try/catch so a throw never
  unwinds through ImGui; a failed apply leaves the previous graph playing and the model unchanged.

---

## Wave 1 — ImGui-free bricks (parallel, disjoint files)

Build after the wave: coordinator adds the new `.cpp` and test files to `tests/CMakeLists.txt`
(host_gui section, beside `ChainLayout.cpp`) and to `apps/host_gui/CMakeLists.txt`; owner runs
`sd build` then `sd test -s unit`.

### Task 1: `RigTopology` model

**Files:**
- Create: `apps/host_gui/RigTopology.hpp`, `apps/host_gui/RigTopology.cpp`
- Test: `tests/functional/unit/test_rig_topology.cpp`

**Interfaces — Produces:**

```cpp
enum class ChainType : std::uint8_t { Series, TwoWay, ThreeWay, TwoWayAlt };
enum class StageKind : std::uint8_t { Series, Pre, Branch, Post };

struct StageAddress
{
    StageKind   kind = StageKind::Series;
    std::size_t branch = 0;   ///< Holds the branch index; read only when kind is Branch.
};

class RigTopology
{
public:
    using SlotId = ui::SlotId;

    static std::size_t branch_count(ChainType type) noexcept;  // 0, 2, 3, 2

    ChainType chain_type() const noexcept;

    /// Switches type. Returns the slots the switch discards (the old branches' slots when leaving
    /// or changing a split type). Series, pre and post lists survive every switch.
    std::vector<SlotId> set_chain_type(ChainType type);

    const std::vector<SlotId>& stage(const StageAddress& address) const;
    bool is_visible(const StageAddress& address) const;  // Series only in Series; others only split

    /// Inserts at index (clamped to size). Returns false for an invisible stage.
    bool insert(const StageAddress& address, std::size_t index, SlotId slot);

    /// Moves a slot to (address, index) in the post-removal list; same-stage moves adjust index.
    bool move(SlotId slot, const StageAddress& address, std::size_t index);

    /// Removes the slot from whichever visible stage holds it.
    bool remove(SlotId slot);

    std::optional<StageAddress> find(SlotId slot) const;

    /// Every slot the active type routes, in signal order: series, or pre, branches, post.
    std::vector<SlotId> active_slots() const;

    /// Every slot held but not routed (the series list while a split is active).
    std::vector<SlotId> parked_slots() const;

    /// Edges for the active type. splitter/mixer are required when a split type is active.
    std::vector<std::pair<SlotId, SlotId>> edges(std::optional<SlotId> splitter,
                                                 std::optional<SlotId> mixer) const;

    /// Builds a Series topology from a linear slot order (preset load).
    static RigTopology from_series(std::vector<SlotId> order);
};
```

Edge rules for `edges()`:
- Series: `s[i] → s[i+1]`. A single slot or none yields no edges.
- Split: pre chain linear; `pre.back() → splitter` if pre is non-empty; for each branch,
  `splitter → b.front()`, linear inside, `b.back() → mixer`, or `splitter → mixer` when empty;
  `mixer → post.front()` and post linear. Duplicates are removed, first occurrence kept.

- [ ] **Step 1: Write the failing tests.** One `TEST(Unit_Dsp, ...)` per rule:

```cpp
TEST(Unit_Dsp, RigTopologySeriesEdgesJoinNeighboursInOrder)
{
    RigTopology topology = RigTopology::from_series({ 4, 7, 9 });
    const auto edges = topology.edges(std::nullopt, std::nullopt);
    ASSERT_EQ(edges.size(), 2u);
    EXPECT_EQ(edges[0], std::make_pair(RigTopology::SlotId{ 4 }, RigTopology::SlotId{ 7 }));
    EXPECT_EQ(edges[1], std::make_pair(RigTopology::SlotId{ 7 }, RigTopology::SlotId{ 9 }));
}

TEST(Unit_Dsp, RigTopologySwitchingToSplitAndBackKeepsTheSeriesChain)
{
    RigTopology topology = RigTopology::from_series({ 1, 2 });
    EXPECT_TRUE(topology.set_chain_type(ChainType::TwoWay).empty());
    EXPECT_EQ(topology.parked_slots(), (std::vector<RigTopology::SlotId>{ 1, 2 }));
    topology.set_chain_type(ChainType::Series);
    EXPECT_EQ(topology.active_slots(), (std::vector<RigTopology::SlotId>{ 1, 2 }));
}

TEST(Unit_Dsp, RigTopologyChangingSplitTypeDiscardsBranchesAndKeepsPrePost)
{
    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWay);
    topology.insert({ StageKind::Pre }, 0, 10);
    topology.insert({ StageKind::Branch, 1 }, 0, 11);
    topology.insert({ StageKind::Post }, 0, 12);
    EXPECT_EQ(topology.set_chain_type(ChainType::ThreeWay),
              (std::vector<RigTopology::SlotId>{ 11 }));
    EXPECT_EQ(topology.stage({ StageKind::Pre }), (std::vector<RigTopology::SlotId>{ 10 }));
    EXPECT_EQ(topology.stage({ StageKind::Post }), (std::vector<RigTopology::SlotId>{ 12 }));
    EXPECT_TRUE(topology.stage({ StageKind::Branch, 2 }).empty());
}

TEST(Unit_Dsp, RigTopologySplitEdgesRouteThroughSplitterAndMixer)
{
    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWay);
    topology.insert({ StageKind::Pre }, 0, 1);
    topology.insert({ StageKind::Branch, 0 }, 0, 2);
    topology.insert({ StageKind::Post }, 0, 3);
    const std::vector<std::pair<RigTopology::SlotId, RigTopology::SlotId>> expected{
        { 1, 100 }, { 100, 2 }, { 2, 101 }, { 100, 101 }, { 101, 3 } };
    EXPECT_EQ(topology.edges(100, 101), expected);
}
```

Plus cases: `TwoWayAltIsADistinctTypeWithTwoBranches`, `InsertIntoAnInvisibleStageIsRefused`,
`MoveWithinAStageAdjustsTheIndexAfterRemoval`, `MoveAcrossBranchesKeepsTheSlot`,
`RemoveFindsTheSlotInAnyVisibleStage`, `TwoEmptyBranchesYieldOneSplitterToMixerEdge`.

- [ ] **Step 2:** Coordinator registers the files; owner runs `sd build` and
  `sd test -s unit`. Expected: link failure or FAIL on the new cases.
- [ ] **Step 3:** Implement. Storage: `ChainType type_`, `std::vector<SlotId> series_, pre_,
  post_`, `std::vector<std::vector<SlotId>> branches_` sized by `branch_count()`.
- [ ] **Step 4:** Owner runs `sd test -s unit`. Expected: PASS.
- [ ] **Step 5:** Commit `Add RigTopology, the host's chain-type model`.

### Task 2: `RigController` snapshot and edge accessors

**Files:**
- Modify: `apps/host_gui/RigController.hpp`, `apps/host_gui/RigController.cpp`
- Test: `tests/functional/unit/test_rig_controller.cpp` (append cases)

**Interfaces — Produces:**

```cpp
/// @brief Holds what a slot needs to be rebuilt: descriptor, parameters, cabinet, bypass, room.
struct SlotSnapshot
{
    std::string                               descriptor_id;
    std::map<std::string, float>              parameters;
    std::string                               cabinet_path;
    bool                                      bypassed = false;
    std::optional<SushiDSP::room::RoomScene>  room_scene;
};

std::vector<std::pair<SlotId, SlotId>> edges() const;          // every stored edge, stored order
std::optional<SlotSnapshot> snapshot(SlotId slot) const;       // nullopt when not live
SlotId restore(const SlotSnapshot& snapshot, bool publish_now = true);  // new id, unconnected
void clear_edges(bool publish_now = true);                     // drops every edge
```

`restore` re-adds the slot, copies parameters over the seeded defaults, stores cabinet path,
bypass and scene, and applies them on the publish that builds the node, the way `load_from_json`
does today.

- [ ] **Step 1:** Failing tests: `RigControllerSnapshotRestoreKeepsParametersAndCabinetPath`
  (set `gain` on a `sushidsp.jcm800` slot, snapshot, remove, restore, read `gain` back),
  `RigControllerEdgesListsStoredEdgesInOrder`, `RigControllerClearEdgesLeavesSlotsLive`.
- [ ] **Step 2:** Owner build + `sd test -s unit`: FAIL.
- [ ] **Step 3:** Implement against `slots_`/`edges_`.
- [ ] **Step 4:** Owner `sd test -s unit`: PASS, including every existing RigController case.
- [ ] **Step 5:** Commit `Let RigController snapshot and restore a slot`.

### Task 3: `SplitLayout` geometry

**Files:**
- Create: `apps/host_gui/ui/SplitLayout.hpp`, `apps/host_gui/ui/SplitLayout.cpp`
- Test: `tests/functional/unit/test_split_layout.cpp`

**Interfaces — Consumes:** `ui::Vec2`, `ui::SlotId` (`ui/Geometry.hpp`). It takes plain counts,
not `RigTopology`, so it depends on nothing above Geometry.

```cpp
namespace ui
{
struct SplitMetrics
{
    float node_width = 78.0f;   float node_height = 86.0f;  float wire_length = 30.0f;
    float io_size = 34.0f;      float glyph_size = 34.0f;   float row_gap = 14.0f;
    float branch_pad = 46.0f;   float empty_slot_width = 78.0f; float corner_radius = 6.0f;
};

struct Rect { Vec2 min; Vec2 max; };

struct StageGeometry
{
    std::vector<Rect> nodes;  ///< One per slot, in stage order.
    std::vector<Rect> gaps;   ///< size()+1 drop gaps; gap i inserts at index i.
    Rect              bounds;
};

struct SplitGeometry
{
    Rect input; Rect output;
    std::optional<Rect> splitter; std::optional<Rect> mixer;
    StageGeometry series; StageGeometry pre; StageGeometry post;
    std::vector<StageGeometry> branches;
    std::vector<std::vector<Vec2>> cables;  ///< Orthogonal polylines, splitter→row, row→mixer.
    Vec2 size;
};

/// @param branch_sizes empty for Series; one entry per branch otherwise.
SplitGeometry layout_split(std::size_t series_size, std::size_t pre_size,
                           const std::vector<std::size_t>& branch_sizes, std::size_t post_size,
                           const SplitMetrics& metrics);

/// Returns the gap index whose centre is nearest x, or nullopt when y is outside the stage.
std::optional<std::size_t> nearest_gap(const StageGeometry& stage, Vec2 point);
}
```

Placement follows the mockup's `redrawSplitWires`: IN at x=0 centred on the row block; branch
rows stacked with `row_gap`, the block's vertical centre is the main line; every branch row is as
wide as the widest branch; each cable is `splitter.right → midX → row.y → row.left`, and
`row.right → midX → mixer.y → mixer.left`. An empty stage still gets one gap and, for a branch,
one `empty_slot_width` placeholder so a drop target exists.

- [ ] **Step 1:** Failing tests: `SplitLayoutSeriesPlacesNodesLeftToRightWithOneGapMore`,
  `SplitLayoutThreeWayStacksRowsAroundTheMainLine`, `SplitLayoutCablesStartAtSplitterAndEndAtMixer`,
  `SplitLayoutEmptyBranchStillOffersOneGap`, `SplitLayoutNearestGapPicksTheClosestCentre`,
  `SplitLayoutNearestGapRejectsAPointOutsideTheStage`. Expected values computed by hand from the
  default metrics in each test's arrange block.
- [ ] **Step 2-4:** Build, FAIL, implement, PASS.
- [ ] **Step 5:** Commit `Add SplitLayout, the chain view's geometry`.

### Task 4: `DividerMath`

**Files:**
- Create: `apps/host_gui/ui/DividerMath.hpp`, `apps/host_gui/ui/DividerMath.cpp`
- Test: `tests/functional/unit/test_divider_math.cpp`

```cpp
namespace ui
{
struct DividerLimits { float minimum; float other_minimum; };

/// Returns the pane size after a drag, clamped to [minimum, available - other_minimum]; when
/// available cannot hold both minimums, the pane gets minimum.
float drag_pane_size(float current, float delta, float available, DividerLimits limits) noexcept;

/// Re-clamps a stored size after the window shrinks.
float fit_pane_size(float current, float available, DividerLimits limits) noexcept;
}
```

- [ ] Tests: `DividerMathDragGrowsWithinLimits`, `DividerMathDragStopsAtTheOtherPanesMinimum`,
  `DividerMathDragStopsAtItsOwnMinimum`, `DividerMathFitShrinksWithTheWindow`,
  `DividerMathTooSmallWindowKeepsTheMinimum`. Build, FAIL, implement, PASS, commit
  `Add DividerMath for resizable panes`.

### Task 5: `GearFilter`

**Files:**
- Create: `apps/host_gui/ui/GearFilter.hpp`, `apps/host_gui/ui/GearFilter.cpp`
- Test: `tests/functional/unit/test_gear_filter.cpp`

```cpp
namespace ui
{
enum class GearTab : std::uint8_t { All, Amps, Pedals, Cabs };
enum class GearViewMode : std::uint8_t { List, Grid };

struct GearItem { std::string id; std::string name; std::string category; };

/// Maps a registry category to its tab: "Amp" → Amps, "Cabinet" → Cabs, "Routing" → none,
/// everything else → Pedals.
std::optional<GearTab> tab_of(const std::string& category);

struct GearGroup { std::string category; std::vector<const GearItem*> items; };

/// Groups matching items by category, first-seen order; search is case-insensitive on name.
std::vector<GearGroup> filter_gear(const std::vector<GearItem>& items, GearTab tab,
                                   const std::string& search);
}
```

Before writing `tab_of`, the implementer lists every `entry.category.name` in
`modules/node_registry` and pastes the list into the report; the mapping above is corrected to the
real names, and routing nodes (`sushidsp.splitter`, `sushidsp.mixer`) are excluded from every tab,
since §16 makes them non-draggable.

- [ ] Tests: `GearFilterAllTabKeepsEveryNonRoutingItem`, `GearFilterAmpsTabKeepsOnlyAmps`,
  `GearFilterSearchIgnoresCase`, `GearFilterGroupsKeepFirstSeenCategoryOrder`,
  `GearFilterNeverListsSplitterOrMixer`. Build, FAIL, implement, PASS, commit
  `Add GearFilter for the gear browser's tabs and search`.

### Task 6: `LevelText`

**Files:**
- Create: `apps/host_gui/ui/LevelText.hpp`, `apps/host_gui/ui/LevelText.cpp`
- Test: `tests/functional/unit/test_level_text.cpp`

```cpp
namespace ui
{
float peak_to_decibels(float peak) noexcept;               // -inf for peak <= 0
float decibels_to_fill(float decibels, float floor_db = -60.0f) noexcept;  // [0,1]
std::string decibel_label(float decibels);                  // "-12.3 dB", "-inf dB"
}
```

- [ ] Tests: unity peak reads `0.0 dB`, half peak reads `-6.0 dB`, zero reads `-inf dB`, fill
  clamps at both ends. Build, FAIL, implement, PASS, commit `Add LevelText for the bar meters`.

---

## Wave 2 — ImGui draw units (parallel, disjoint files; waits on Wave 1)

Build after the wave: coordinator adds new `.cpp` files to `apps/host_gui/CMakeLists.txt`; owner
runs `sd build`. These units carry no test (ImGui); acceptance is a clean build plus the manual
check named in each task, done in Wave 3 when `main.cpp` mounts them. Each subagent syntax-checks
with `clang -fsyntax-only` through `compile_commands.json`.

### Task 7: `TopologyApplier`

**Files:** Create `apps/host_gui/TopologyApplier.hpp`, `apps/host_gui/TopologyApplier.cpp`;
Test `tests/functional/unit/test_topology_applier.cpp` (ImGui-free, so it is tested).

```cpp
class TopologyApplier
{
public:
    /// Rewires rig to match topology in one publish: creates or drops the hidden splitter and
    /// mixer, snapshots and removes parked slots, restores unparked ones, clears every edge and
    /// connects topology.edges(). On a throw the rig and topology are rolled back and the error
    /// rethrown.
    void apply(RigTopology& topology, RigController& rig);

    /// Parked series slots, keyed by their pre-park id, so Series comes back with parameters.
    std::size_t parked_count() const noexcept;

private:
    std::optional<ui::SlotId> splitter_;
    std::optional<ui::SlotId> mixer_;
    std::vector<std::pair<ui::SlotId, RigController::SlotSnapshot>> parked_;
};
```

Parked slots leave the graph (a disconnected node would become a second sink) and come back
through `restore`, whose new id replaces the old one in `RigTopology` via a new
`RigTopology::replace(SlotId from, SlotId to)`. Task 7 adds `replace` and its test
(`RigTopologyReplaceRenamesASlotInPlace`) to Task 1's files; nothing else in Wave 2 touches them.

- [ ] Tests: `TopologyApplierSeriesWiresALinearChain`,
  `TopologyApplierTwoWayAddsOneSplitterAndOneMixer` (count descriptors in `slot_order()`),
  `TopologyApplierBackToSeriesRestoresParkedParameters`,
  `TopologyApplierDiscardedBranchSlotsAreRemovedFromTheRig`,
  `TopologyApplierLeavesThePreviousGraphWhenApplyThrows` (a topology exceeding
  `RigGraph::kMaxFanInOut` is not reachable with 3 branches, so force the throw with an
  unregistered descriptor through a test seam named in the implementation report).
  Build, FAIL, implement, PASS, commit `Compile RigTopology into RigController edges`.

### Task 8: `Divider` widget

**Files:** Create `apps/host_gui/ui/Divider.hpp`, `apps/host_gui/ui/Divider.cpp`.

```cpp
namespace ui
{
enum class DividerAxis : std::uint8_t { Vertical, Horizontal };
/// Draws a 14 px grab strip with a 3x36 px handle, accent-colored while hovered or held, and
/// updates size through drag_pane_size. invert grows the pane when dragging toward it.
bool DrawDivider(const char* id, DividerAxis axis, float& size, float available,
                 DividerLimits limits, bool invert);
}
```

Commit `Add the Divider widget`.

### Task 9: `RigChainView` (replaces `ChainStrip` + `SplitterPicker`)

**Files:** Create `apps/host_gui/ui/RigChainView.hpp`, `apps/host_gui/ui/RigChainView.cpp`.

```cpp
namespace ui
{
struct RigChainState
{
    RigTopology                topology;
    TopologyApplier            applier;
    std::optional<SlotId>      selected;
    std::string                edit_error;
};
/// Draws the four-button chain-type rail and the chain; created receives a slot a gear drop made.
void DrawRigChainView(RigController& rig, RigChainState& state, std::optional<SlotId>& created);
}
```

Behaviour: rail buttons draw the mockup's four SVG paths as ImDrawList polylines, active one
teal; click calls `set_chain_type`, removes returned slots from the rig, and `applier.apply`.
Nodes, IN/OUT circles, splitter/mixer glyphs (no remove button, no drag source, no selection),
branch number badges, and cables (`AddPolyline` with rounded joins, teal, 3 px) come from
`layout_split`. Payloads stay `"GEAR_DESCRIPTOR_ID"` (insert at `nearest_gap`) and
`"RACK_SLOT_ID"` (move, across stages allowed); the hovered gap shows the drop caret. The remove
button and the bypass toggle keep today's behaviour. Every edit runs inside a try/catch that
writes `edit_error`. Commit `Add RigChainView with chain types and routed cables`.

### Task 10: Gear browser list/grid and tabs

**Files:** Modify `apps/host_gui/ui/GearBrowser.hpp`, `apps/host_gui/ui/GearBrowser.cpp`.

```cpp
namespace ui
{
struct GearBrowserState { GearTab tab = GearTab::All; GearViewMode mode = GearViewMode::List;
                          char search[128] = {}; };
void DrawGearBrowser(GearBrowserState& state);
}
```

Header row: title, item count, two-button list/grid toggle. Then search, then the four tabs,
then groups from `filter_gear`. List mode keeps today's full-width card; grid mode wraps 76 px
tiles with icon and name only. Both keep the `"GEAR_DESCRIPTOR_ID"` drag source. Commit
`Give the gear browser tabs and a grid view`.

### Task 11: Bottom transport bar

**Files:** Modify `apps/host_gui/ui/TransportBar.hpp`, `apps/host_gui/ui/TransportBar.cpp`,
`apps/host_gui/ui/MeterBar.hpp`, `apps/host_gui/ui/MeterBar.cpp`.

```cpp
void DrawTransportBar(RigController& rig, LevelProbe& input, LevelProbe& output,
                      bool& mini_daw_active, std::string& transport_error);
void DrawInlineMeters(LevelProbe& input, LevelProbe& output);  // replaces DrawMeterBar
```

Order per the mockup: record/play/stop/loop, position readout, click, BPM, signature, Tap, Speed,
Pitch (disabled `0 st`, tooltip "Pitch shift arrives with the track pitch-shift plan"), the
Mini-DAW / Back to Rig button, then In/Out meters with `decibel_label` pinned right. Items that
exist today and are not in the mockup (take folder/name, input source, loop range, click level,
dropped-block count) stay, placed after Speed in a wrapping row; the implementer lists where each
went. Behaviour of every existing control is unchanged. Commit `Move meters and the Mini-DAW
toggle into the bottom bar`.

### Task 12: Top bar

**Files:** Modify `apps/host_gui/ui/TopBar.hpp`, `apps/host_gui/ui/TopBar.cpp`.

Three columns: left preset field, Save, Load; centre `SushiDSP` brand mark placed at
`(window_width - brand_width) / 2` independent of both groups; right connection pill and Settings.
Also draws the disconnected state (label plus Settings) that `main.cpp` draws inline today.
Signature gains `bool connected`; `RigController*` becomes nullable. Commit
`Centre the brand in a three-column top bar`.

### Task 13: Arrangement header width

**Files:** Modify `apps/host_gui/ui/ArrangementView.hpp`, `apps/host_gui/ui/ArrangementView.cpp`.

`kHeaderWidth` becomes a `float& header_width` parameter; a vertical `DrawDivider` sits between
header column and grid, limits `{ 120, 160 }`. The lane geometry the file-drop router reads uses
the live width. Commit `Make the arrangement's lane-header column resizable`.

---

## Wave 3 — Composition (serial; waits on Wave 2 and on open items 1-3)

### Task 14: Workspace in `main.cpp`

**Files:** Modify `apps/host_gui/main.cpp`. Delete (if approved) `ui/ChainStrip.*`,
`ui/SplitterPicker.*`, `ui/ChainLayout.*`, `tests/functional/unit/test_chain_layout.cpp`;
coordinator removes them from both CMake files.

Frame: `##top` (40 px) → workspace → horizontal divider → `##bottom` (height from divider, limits
`{ 72, 200 }`). Workspace shows exactly one of: rig view (settings pane over chain pane on the
left, split by a horizontal divider, limits `{ 120, 140 }`; vertical divider; gear pane on the
right, limits `{ 220, 360 }`, default width 300), or `DrawArrangementView`. Every size is
`fit_pane_size`-clamped each frame. After a preset load, `topology = RigTopology::from_series(
rig.slot_order())` and the applier resets, following open item 3. Selection clears on a chain-type
switch, as the mockup does.

Manual acceptance, run by the owner in the built GUI:
1. Drag each divider: panes resize, stop at their minimums, survive a window resize.
2. Series: drag JCM800 and a cabinet in; audio plays as before (JCM800 tone anchor unchanged).
3. 2-Way: pre/post stages appear; a pedal in branch 1 and nothing in branch 2 sounds as a
   parallel blend; splitter/mixer glyphs cannot be selected, dragged or removed.
4. 3-Way then 2-Way Alt: branches empty, pre/post keep their gear.
5. Back to Series: the original chain returns with its knob values.
6. Mini-DAW swaps the whole workspace; Back to Rig restores it; transport bar never moves.
7. Gear grid/list toggle and tabs filter the same catalog.

Build: owner runs `sd build` and `sd test -s unit`. Commit `Mount the Phase 2 workspace`.

## Wave 4 — Documentation (parallel with nothing; waits on Wave 3)

### Task 15: Docs in the same change set

**Files:** `docs/design/gui_ux_redesign.md` (status line: Phase 2 implemented, citing this plan;
mark §12 Pitch as waiting on the pitch plan), `docs/design/REMAINING_WORK.md` (close the Phase 2
entry and the gear-browser grid item; add "Track playback pitch shift independent of speed"),
`docs/reference/CHANGELOG.md` (one-line entries per the global format), `docs/README.md` if it
lists host GUI files. Creating `apps/host_gui/README.md` is part of the separate documentation
skeleton task the global rules call for and is not done here; this task records that gap in
`REMAINING_WORK.md`. Commit `Record Phase 2 of the host GUI redesign as implemented`.

## Out of this plan

- The pitch-shift DSP and its wiring to the Pitch field (separate plan, owner decision
  2026-09-14).
- Saving and loading a parallel topology in a preset (§19).
- Persisting divider sizes, view mode or chain type (§18, §19).
