# RigController DAG Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace `apps/host_gui`'s index-as-handle `RigController` with a `SlotId`-based
node-graph model capable of holding a branching rig, and retire `apps/host_gui`'s bespoke linear
JSON preset format in favor of `modules/rig_preset`, cabinet IR included. No visual change to the
GUI in this plan — every edit path the GUI drives today still only ever builds a linear chain;
this plan only makes the controller layer capable of holding more.

**Architecture:** `modules/rig_preset` gains two small opt-in callback parameters first
(non-breaking). `RigController`'s internal model changes from `std::vector<Slot>` (position IS
`NodeHandle`) to `std::unordered_map<SlotId, Slot>` + `std::vector<Edge>` + an ordered
`slot_order_` for stable enumeration, with a `SlotId -> NodeHandle` translation table rebuilt on
every `rebuild_and_publish()`. `apps/host_gui/main.cpp`'s rack-list loop and
`apps/host_gui/SlotSelection.hpp` are updated to track a `SlotId` (or none) instead of an `int`
index — this actually *simplifies* `SlotSelection.hpp`, since a `SlotId` stays valid across
inserts/moves/removes and no longer needs index-remapping arithmetic.
`apps/host_gui/Preset.hpp`/`.cpp` and their test are deleted; Save/Load call
`modules/rig_preset` directly through two new `RigController` methods.

**Tech Stack:** C++17, GoogleTest, nlohmann::json (all pre-existing in the tree).

**Spec:** `docs/superpowers/specs/2026-08-26-rigcontroller-dag-support-design.md`

## Global Constraints

- `RigController`'s structural methods (all of them) remain GUI/main-thread-only, non-RT — no
  change to this existing contract.
- `modules/rig_preset` stays free of any `Jcm800`/`IrLoader`/cabinet-specific dependency; cabinet
  handling is threaded through generic callback parameters, never hardcoded per-type knowledge.
- Every `.hpp`/`.cpp` file (tests included) carries the exact 76-column boxed license header,
  copied verbatim from an existing file, only the filename line changed.
- C++17, Allman braces, `PascalCase` types / `snake_case` functions and variables / trailing
  underscore members / `UPPER_SNAKE` constants, Doxygen `@brief`/`@param`/`@return` on every
  public function, no comments beyond non-obvious *why*.
- Test files: `tests/functional/unit/test_<snake_case_name>.cpp`, GTest cases prefixed `Unit_Dsp`.
- Run `sd build` and `sd test -s unit` after every task; never invoke `cmake`/`ctest` directly.
- No change to `apps/host` or `apps/host_asio` — neither touches `RigController`, `Preset.cpp`,
  or `modules/rig_preset` today, and this plan doesn't change that.
- `apps/host_gui`'s on-screen behavior for a linear chain (the only shape any edit path in this
  plan constructs) must be pixel-identical to before — this plan proves that via the existing
  manual-verification convention for this app's render layer (no automated ImGui test harness
  exists in this project) plus the retained unit-test coverage for `RigController`/
  `SlotSelection`'s logic.

---

### Task 1: `modules/rig_preset` — opt-in per-node callback extension points

**Files:**
- Modify: `modules/rig_preset/include/SushiDSP/rig_preset/RigPreset.hpp`
- Modify: `modules/rig_preset/src/RigPreset.cpp`
- Test: `tests/functional/unit/test_rig_preset.cpp` (append)

**Interfaces:**
- Produces: `serialize_rig_to_json(const RigGraph&, AnnotateNodeFn annotate_node = nullptr)` and
  `deserialize_rig_from_json(RigGraph&, const std::string&, const PrepareInfo&, OnNodeLoadedFn on_node_loaded = nullptr)`,
  where `using AnnotateNodeFn = std::function<void(NodeHandle, nlohmann::json&)>;` and
  `using OnNodeLoadedFn = std::function<void(NodeHandle, const nlohmann::json&)>;`. Both default
  to `nullptr` (no-op), so every existing call site (including Task 8-10's own tests from the
  prior plan) keeps compiling and behaving identically.
- Consumed by: Task 3 (`RigController`'s cabinet-aware save/load).

This is purely additive — no existing behavior changes when the new parameter is omitted.

- [ ] **Step 1: Write the failing tests**

Append to `tests/functional/unit/test_rig_preset.cpp`:

```cpp
TEST(Unit_Dsp, SerializeRigToJsonCallsAnnotateNodeOncePerNode)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(SushiDSP::make_node("sushidsp.tubescreamer", info));
    graph.add_node(SushiDSP::make_node("sushidsp.jcm800", info));
    graph.compile(info);

    int call_count = 0;
    const std::string json_text = SushiDSP::serialize_rig_to_json(graph,
        [&call_count](SushiDSP::NodeHandle, nlohmann::json& node_json)
        {
            ++call_count;
            node_json["marker"] = "annotated";
        });

    EXPECT_EQ(call_count, 2);
    const nlohmann::json root = nlohmann::json::parse(json_text);
    for (const auto& node_json : root.at("nodes"))
    {
        EXPECT_EQ(node_json.at("marker").get<std::string>(), "annotated");
    }
}

TEST(Unit_Dsp, DeserializeRigFromJsonCallsOnNodeLoadedWithFullEntry)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    const std::string json_text = R"({
        "nodes": [
            { "id": 0, "type": "sushidsp.tubescreamer", "params": {}, "inputs": [], "marker": "hello" }
        ]
    })";

    SushiDSP::RigGraph graph(256, 4);
    int call_count = 0;
    std::string seen_marker;
    SushiDSP::deserialize_rig_from_json(graph, json_text, info,
        [&call_count, &seen_marker](SushiDSP::NodeHandle, const nlohmann::json& node_json)
        {
            ++call_count;
            seen_marker = node_json.value("marker", std::string());
        });

    EXPECT_EQ(call_count, 1);
    EXPECT_EQ(seen_marker, "hello");
}

TEST(Unit_Dsp, SerializeAndDeserializeStillWorkWithNoCallback)
{
    // Regression guard: every existing call site (no callback argument) must keep compiling
    // and behaving exactly as before this task.
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(SushiDSP::make_node("sushidsp.tubescreamer", info));
    graph.compile(info);

    const std::string json_text = SushiDSP::serialize_rig_to_json(graph);
    SushiDSP::RigGraph rebuilt(256, 4);
    SushiDSP::deserialize_rig_from_json(rebuilt, json_text, info);

    EXPECT_EQ(rebuilt.nodes().size(), 1u);
}
```

Add `#include <functional>` to the test file's includes if not already present.

- [ ] **Step 2: Run tests to verify they fail**

Run: `sd test -s unit`. Expected: build failure — the new callback parameters don't exist yet.

- [ ] **Step 3: Add the callback typedefs and parameters to RigPreset.hpp**

In `modules/rig_preset/include/SushiDSP/rig_preset/RigPreset.hpp`, add `#include <functional>`
to the includes, and change the two declarations to:

```cpp
    /// @brief Called once per serialized node, after its "id"/"type"/"params"/"inputs" fields
    /// are written, with the JSON object being built for it — lets a caller add extra fields
    /// modules/rig_preset itself has no opinion about (e.g. a Jcm800's cabinet IR path).
    using AnnotateNodeFn = std::function<void(NodeHandle, nlohmann::json&)>;

    /// @brief Called once per node constructed during deserialize, with its full parsed JSON
    /// entry, after the node is add_node()-ed but before params/edges are applied — lets a
    /// caller read an extra field it wrote via AnnotateNodeFn and act on it (e.g. install a
    /// cabinet). Never receives a malformed entry: the entry has already passed every shape
    /// check in this function's first validation pass.
    using OnNodeLoadedFn = std::function<void(NodeHandle, const nlohmann::json&)>;

    /// @brief Serializes a compiled RigGraph's topology and every declared parameter to a JSON
    /// string: `{"nodes": [{"id", "type", "params", "inputs"}, ...]}`, where each node's
    /// `inputs` lists the preset-local `id`s of its upstream nodes in connect() order.
    /// @param graph The graph to serialize. Must have been compile()'d (uses node_registry() to
    /// look up each live node's declared parameters by matching NodeDescriptor::id).
    /// @param annotate_node Optional; if non-null, called once per node with the JSON object
    /// being built for it, letting the caller add extra fields. Null means no extra fields.
    /// @return The JSON text, pretty-printed.
    /// @throws std::runtime_error if a live node's type is not found in node_registry() (should
    /// not happen for any node constructed via make_node(), but a hand-built RigGraph using a
    /// non-registered IAudioNode subclass cannot be serialized).
    std::string serialize_rig_to_json(const RigGraph& graph, AnnotateNodeFn annotate_node = nullptr);

    /// @brief Parses a JSON string produced by serialize_rig_to_json() (or hand-written to the
    /// same schema) and rebuilds it into graph via add_node()/connect() — does not call
    /// compile(); the caller compiles afterward once, the same as any other graph edit.
    /// @param graph An empty or existing RigGraph to add the parsed nodes/edges into.
    /// @param json_text The JSON text to parse.
    /// @param info The PrepareInfo each node is constructed with.
    /// @param on_node_loaded Optional; if non-null, called once per constructed node with its
    /// full parsed JSON entry. Null means no extra handling.
    /// @throws std::runtime_error if the JSON is malformed: a missing or wrong-shaped `id`,
    /// `type`, `params` or `inputs` key, a `type` not found in node_registry(), a duplicate `id`,
    /// or an `inputs` entry referencing an id no entry declares. Every entry is fully validated
    /// in a first pass before any node is added, so graph is left unmodified for any malformed
    /// entry. (nlohmann::json::parse_error still escapes for text that is not valid JSON.)
    void deserialize_rig_from_json(RigGraph& graph, const std::string& json_text,
        const PrepareInfo& info, OnNodeLoadedFn on_node_loaded = nullptr);
```

- [ ] **Step 4: Wire both callbacks into RigPreset.cpp**

In `modules/rig_preset/src/RigPreset.cpp`, in `serialize_rig_to_json`, add the parameter to the
signature and, immediately before `root["nodes"].push_back(node_json);`, add:

```cpp
            if (annotate_node)
            {
                annotate_node(handle, node_json);
            }
```

In `deserialize_rig_from_json`, add the parameter to the signature and, in the node-construction
loop (right after `NodeHandle handle = graph.add_node(descriptor->make(info));` and its
`preset_id_to_handle[preset_id] = handle;` line), add:

```cpp
            if (on_node_loaded)
            {
                on_node_loaded(handle, node_json);
            }
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `sd test -s unit`. Expected: all three new tests pass; every pre-existing
`Unit_Dsp.SerializeRigToJson*`/`Unit_Dsp.DeserializeRigFromJson*`/
`Unit_Dsp.SerializeThenDeserialize*` test still passes unchanged (no-callback call sites).

- [ ] **Step 6: Commit**

```bash
git add modules/rig_preset/include/SushiDSP/rig_preset/RigPreset.hpp modules/rig_preset/src/RigPreset.cpp tests/functional/unit/test_rig_preset.cpp
git commit -m "rig_preset: add opt-in per-node annotate/on-loaded callbacks"
```

---

### Task 2: `RigController` — SlotId-based node-graph model, GUI rewiring, old Preset deletion

**Files:**
- Modify: `apps/host_gui/RigController.hpp`
- Modify: `apps/host_gui/RigController.cpp`
- Modify: `apps/host_gui/SlotSelection.hpp`
- Modify: `apps/host_gui/main.cpp`
- Delete: `apps/host_gui/Preset.hpp`
- Delete: `apps/host_gui/Preset.cpp`
- Delete: `tests/functional/unit/test_preset.cpp`
- Modify: `tests/functional/unit/test_rig_controller.cpp` (full rewrite to `SlotId`)
- Modify: `tests/CMakeLists.txt` (drop `test_preset.cpp` and `apps/host_gui/Preset.cpp` from the
  build; add `SushiDSP::rig_preset` to `sushidsp_tests`'s link libraries if not already present —
  it is not, since only `modules/rig_preset`'s own tests linked it before)
- Modify: `apps/host_gui/CMakeLists.txt` (drop `Preset.cpp` from `sushidsp_host_gui`'s sources,
  add `SushiDSP::rig_preset` to its link libraries)

**Interfaces:**
- Produces: `SlotId` (public typedef, `std::uint32_t`), `RigController::add_node`,
  `connect`, `disconnect`, `remove(SlotId)`, `append`, `insert_before`, `move_before`,
  `slot_order() -> std::vector<SlotId>`, `save_to_json() -> std::string`,
  `load_from_json(const std::string&)` (this task adds `save_to_json`/`load_from_json` WITHOUT
  cabinet-IR round-trip yet — Task 3 layers that on afterward via Task 1's callbacks; this task's
  save/load is a correct, working, cabinet-*losing* round-trip, an accepted interim state within
  this same plan).
- Consumes: `SushiDSP::serialize_rig_to_json`/`deserialize_rig_from_json` (no-callback overload,
  Task 1's defaults), `SushiDSP::RigGraph::nodes()`/`upstream_of()`/`node()`.

This is the plan's largest task because `RigController`'s identity model, `main.cpp`'s rack-list
loop, and `SlotSelection.hpp`'s remap logic are mutually coupled through a single public API
surface — changing one without the others leaves the tree uncompilable. It is one cohesive,
independently-testable unit: at the end, `sd build` succeeds (GUI included) and `sd test -s unit`
passes, with the GUI's on-screen linear-chain behavior unchanged.

- [ ] **Step 1: Read every file this task touches, in full, before editing anything**

Read (in this order, so later files' current call sites make sense against the earlier ones):
`apps/host_gui/RigController.hpp`, `apps/host_gui/RigController.cpp`,
`apps/host_gui/SlotSelection.hpp`, `apps/host_gui/main.cpp`, `apps/host_gui/Preset.hpp`,
`apps/host_gui/Preset.cpp`, `tests/functional/unit/test_rig_controller.cpp`,
`tests/functional/unit/test_preset.cpp`, `tests/CMakeLists.txt`, `apps/host_gui/CMakeLists.txt`,
`modules/rig_preset/include/SushiDSP/rig_preset/RigPreset.hpp` (post-Task-1). This task's
description below quotes exact current code where precision matters, but the files may have
minor differences by the time you reach this task — always trust what you read over this text
where they disagree, and note any deviation in your report.

- [ ] **Step 2: Write the failing tests (full rewrite of test_rig_controller.cpp)**

Replace `tests/functional/unit/test_rig_controller.cpp`'s contents (keep the license header,
replace everything below the includes) with:

```cpp
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "RigController.hpp"
#include "SushiDSP/node_registry/NodeRegistry.hpp"

TEST(Unit_Dsp, RigControllerStartsEmpty)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    EXPECT_TRUE(controller.slot_order().empty());
}

TEST(Unit_Dsp, RigControllerAppendAddsNodeToEndOfChain)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId b = controller.append("sushidsp.jcm800");

    const auto order = controller.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], a);
    EXPECT_EQ(order[1], b);
    EXPECT_EQ(controller.descriptor_id(a), "sushidsp.tubescreamer");
    EXPECT_EQ(controller.descriptor_id(b), "sushidsp.jcm800");
    ASSERT_NE(controller.active_graph(), nullptr);
}

TEST(Unit_Dsp, RigControllerInsertBeforePlacesNewSlotImmediatelyBeforeAnchor)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId c = controller.append("sushidsp.jcm800");

    const RigController::SlotId b = controller.insert_before(c, "sushidsp.tubescreamer");

    const auto order = controller.slot_order();
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], a);
    EXPECT_EQ(order[1], b);
    EXPECT_EQ(order[2], c);
}

TEST(Unit_Dsp, RigControllerInsertBeforeNoAnchorPlacesNewSlotAtFront)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");

    const RigController::SlotId new_head = controller.insert_before(std::nullopt, "sushidsp.jcm800");

    const auto order = controller.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], new_head);
    EXPECT_EQ(order[1], a);
}

TEST(Unit_Dsp, RigControllerMoveBeforeReordersChain)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId b = controller.append("sushidsp.jcm800");

    controller.move_before(a, std::nullopt);  // a is already at the front; no-op shape, exercised separately below
    controller.move_before(b, a);             // move b to immediately before a

    const auto order = controller.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], b);
    EXPECT_EQ(order[1], a);
}

TEST(Unit_Dsp, RigControllerRemoveDropsFromChainAndReconnectsSurvivors)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId b = controller.append("sushidsp.jcm800");
    const RigController::SlotId c = controller.append("sushidsp.tubescreamer");

    controller.remove(b);

    const auto order = controller.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], a);
    EXPECT_EQ(order[1], c);
    // b's removal must not leave a and c disconnected — the chain must re-link around the gap,
    // matching today's behavior where removing a middle slot still produces one working chain.
    ASSERT_NE(controller.active_graph(), nullptr);
    const auto handles = controller.active_graph()->nodes();
    ASSERT_EQ(handles.size(), 2u);
}

TEST(Unit_Dsp, RigControllerAppendWithUnknownDescriptorIdIsSafeNoOp)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.does_not_exist");

    const auto order = controller.slot_order();
    ASSERT_EQ(order.size(), 1u);
    EXPECT_EQ(order[0], a);
    ASSERT_NE(controller.active_graph(), nullptr);
}

TEST(Unit_Dsp, RigControllerPreservesParameterValuesAcrossReorder)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");
    controller.set_parameter(ts, "drive", 0.42f);
    const RigController::SlotId amp = controller.append("sushidsp.jcm800");

    controller.move_before(ts, std::nullopt);
    (void)amp;

    EXPECT_FLOAT_EQ(controller.get_parameter(ts, "drive"), 0.42f);
}

TEST(Unit_Dsp, RigControllerGetParameterReturnsDescriptorDefaultBeforeAnySet)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");

    float expected_default = 0.0f;
    bool found = false;
    for (const auto& entry : SushiDSP::node_registry())
    {
        if (std::string("sushidsp.tubescreamer") == entry.descriptor->id)
        {
            for (std::size_t p = 0; p < entry.descriptor->parameter_count; ++p)
            {
                if (std::string("drive") == entry.descriptor->parameters[p].id)
                {
                    expected_default = entry.descriptor->parameters[p].default_value;
                    found = true;
                }
            }
        }
    }
    ASSERT_TRUE(found);
    ASSERT_NE(expected_default, 0.0f);

    EXPECT_FLOAT_EQ(controller.get_parameter(ts, "drive"), expected_default);
}

TEST(Unit_Dsp, RigControllerSetParameterReachesNodeOnlyAfterProcess)
{
    SushiDSP::PrepareInfo info{ 48000.0, 64, 1 };
    RigController controller(info);
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");
    controller.set_parameter(ts, "tone", 0.9f);

    SushiDSP::IAudioNode* node = controller.active_graph()->node(controller.active_graph()->nodes().front());
    ASSERT_NE(node, nullptr);
    EXPECT_FLOAT_EQ(node->get_parameter("tone"), 0.5f);

    std::vector<float> in(64, 0.0f);
    std::vector<float> out(64, 0.0f);
    const float* ins[1] = { in.data() };
    float* outs[1] = { out.data() };
    SushiDSP::ProcessContext context{ outs, ins, 64, 0.0 };
    controller.active_graph()->process(context);

    EXPECT_FLOAT_EQ(node->get_parameter("tone"), 0.9f);
    EXPECT_EQ(controller.dropped_parameter_changes(), 0u);
}

TEST(Unit_Dsp, RigControllerSetParameterStillMirroredByGetParameterImmediately)
{
    SushiDSP::PrepareInfo info{ 48000.0, 64, 1 };
    RigController controller(info);
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");
    controller.set_parameter(ts, "tone", 0.9f);

    EXPECT_FLOAT_EQ(controller.get_parameter(ts, "tone"), 0.9f);
}

TEST(Unit_Dsp, RigControllerRebuildSeedsFreshNodesWithStoredParameters)
{
    SushiDSP::PrepareInfo info{ 48000.0, 64, 1 };
    RigController controller(info);
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");
    controller.set_parameter(ts, "tone", 0.9f);
    controller.append("sushidsp.jcm800");

    SushiDSP::NodeHandle handle_for_ts = 0;   // ts was added first, so it is the graph's first handle
    SushiDSP::IAudioNode* node = controller.active_graph()->node(handle_for_ts);
    ASSERT_NE(node, nullptr);
    EXPECT_FLOAT_EQ(node->get_parameter("tone"), 0.9f);
}

TEST(Unit_Dsp, RigControllerSaveAndLoadJsonRoundTripsChainAndParameters)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController original(info);
    const RigController::SlotId ts = original.append("sushidsp.tubescreamer");
    original.set_parameter(ts, "drive", 0.77f);
    original.append("sushidsp.jcm800");

    const std::string json_text = original.save_to_json();

    RigController reloaded(info);
    reloaded.load_from_json(json_text);

    const auto order = reloaded.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(reloaded.descriptor_id(order[0]), "sushidsp.tubescreamer");
    EXPECT_EQ(reloaded.descriptor_id(order[1]), "sushidsp.jcm800");
    EXPECT_FLOAT_EQ(reloaded.get_parameter(order[0], "drive"), 0.77f);
}
```

`RigControllerAppendWithUnknownDescriptorIdIsSafeNoOp`'s equivalent for `load_chain` and
`RigControllerLoadChainSeedsDefaultsForParametersMissingFromPreset` (present in the old test
file) are superseded by `RigControllerSaveAndLoadJsonRoundTripsChainAndParameters` plus Task 1's
own `deserialize_rig_from_json` malformed-entry tests, which already cover the "missing
parameter falls back to descriptor default" behavior (a preset that omits a parameter simply
never calls `set_parameter` for it on the freshly-`make()`-d node, which already carries its
descriptor default). Do not re-add a chain-loading-specific defaults test here — it would
duplicate coverage `modules/rig_preset` already owns.

Append `SlotSelection` tests below (same file), replacing every test in the old file's
`--- Rack-list selection remapping ---` section:

```cpp
// --- Rack-list selection tracking (apps/host_gui/SlotSelection.hpp) ---
//
// A SlotId stays valid across insert/move/remove, unlike the old int-index scheme, so the only
// remaining job is dropping a selection that no longer names a live slot.

TEST(Unit_Dsp, SlotSelectionStillValidReturnsTrueWhenSlotIsInTheList)
{
    std::vector<RigController::SlotId> order{ 3, 7, 9 };
    EXPECT_TRUE(slot_selection::still_valid(std::optional<RigController::SlotId>(7), order));
}

TEST(Unit_Dsp, SlotSelectionStillValidReturnsFalseWhenSlotWasRemoved)
{
    std::vector<RigController::SlotId> order{ 3, 9 };
    EXPECT_FALSE(slot_selection::still_valid(std::optional<RigController::SlotId>(7), order));
}

TEST(Unit_Dsp, SlotSelectionStillValidReturnsFalseForNoSelection)
{
    std::vector<RigController::SlotId> order{ 3, 7, 9 };
    EXPECT_FALSE(slot_selection::still_valid(std::nullopt, order));
}
```

- [ ] **Step 3: Run tests to verify they fail**

Run: `sd test -s unit`. Expected: build failure — `RigController::SlotId`, `slot_order()`,
`descriptor_id()`, `save_to_json()`, `load_from_json()`, `slot_selection::still_valid()` don't
exist yet.

- [ ] **Step 4: Rewrite RigController.hpp**

Replace `apps/host_gui/RigController.hpp`'s contents below the license header and includes with:

```cpp
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/core/RtGraphSlot.hpp"

/// @brief Owns the authoritative rig topology (a set of slots, each with a descriptor id and
/// current parameter values, plus edges between them) and rebuilds + publishes a fresh
/// SushiDSP::RigGraph through an SushiDSP::RtGraphSlot on every structural edit. Not real-time
/// safe — every method here runs on the GUI/main thread only. Not part of the SushiDSP::
/// namespace: this is app-local glue, not a library product.
///
/// Slot identity is a SlotId, stable across every edit (insert, move, remove) — unlike a chain
/// index, which shifts meaning whenever the chain changes shape. A SlotId is unrelated to
/// SushiDSP::NodeHandle, which RigGraph::add_node() assigns fresh on every rebuild.
class RigController
{
public:
    using SlotId = std::uint32_t;

    explicit RigController(const SushiDSP::PrepareInfo& info);

    /// @brief Adds a new, unconnected slot and republishes.
    /// @param descriptor_id The NodeDescriptor id to construct.
    /// @return The new slot's id.
    SlotId add_node(const std::string& descriptor_id);

    /// @brief Connects one slot's output to another's input and republishes.
    void connect(SlotId from, SlotId to);

    /// @brief Removes a single edge, if present, and republishes.
    void disconnect(SlotId from, SlotId to);

    /// @brief Removes a slot and every edge touching it, then republishes. If the removed slot
    /// sat in the middle of a linear run (one upstream edge, one downstream edge), its neighbors
    /// are reconnected directly to each other, preserving a working chain around the gap.
    void remove(SlotId slot);

    /// @brief Adds a new slot connected after the chain's current tail (or as the sole slot, if
    /// the rig is empty) and republishes. Convenience for the common linear-append case.
    /// @return The new slot's id.
    SlotId append(const std::string& descriptor_id);

    /// @brief Adds a new slot immediately before `following` in chain order (rewiring the one
    /// edge that must move), or at the very front if `following` is std::nullopt, and
    /// republishes. Convenience for a linear "insert at this rack position" edit.
    /// @return The new slot's id.
    SlotId insert_before(std::optional<SlotId> following, const std::string& descriptor_id);

    /// @brief Moves `moved` so it sits immediately before `following` in chain order (or at the
    /// very front if `following` is std::nullopt), and republishes.
    void move_before(SlotId moved, std::optional<SlotId> following);

    /// @brief Sets a parameter on `slot`. Does NOT republish (no structural change) and never
    /// touches the live node directly: the value is stored in this controller's own slot state
    /// and queued on the active RigGraph's lock-free parameter ring, where the audio thread
    /// applies it at the top of its next process() call. If the ring is full the change is
    /// dropped and dropped_parameter_changes() is incremented; the call never blocks.
    void set_parameter(SlotId slot, const char* param_id, float value);

    /// @brief Reads this controller's stored value for a parameter on `slot`.
    float get_parameter(SlotId slot, const char* param_id) const;

    /// @brief Installs a cabinet impulse response on the JCM800 at `slot`. Loads the file at the
    /// controller's prepared sample rate, hands it to the live node, and remembers the path so a
    /// later rebuild (or a preset save) can reproduce it.
    /// @return True if `slot` is a JCM800 and the impulse response was installed.
    /// @throws std::runtime_error if the file cannot be opened or parsed.
    bool set_cabinet(SlotId slot, const std::string& path);

    /// @brief Reclaims retired graphs and every live JCM800's retired cabinet. Control thread
    /// only; call once per GUI frame.
    void reclaim();

    /// @brief Counts parameter changes dropped because the audio thread's ring was full.
    std::size_t dropped_parameter_changes() const;

    /// @brief Every live slot's id, in a stable (insertion-derived) enumeration order — for a
    /// purely linear chain (the only shape any current GUI edit path constructs), this is also
    /// the chain's visual left-to-right order.
    std::vector<SlotId> slot_order() const;

    /// @brief A slot's NodeDescriptor id.
    /// @return The id, or "" if slot is not live.
    std::string descriptor_id(SlotId slot) const;

    /// @brief A slot's cabinet impulse response path, or "" if it has none or isn't a JCM800.
    std::string cabinet_path(SlotId slot) const;

    /// @brief Lists the ids of every slot with an edge into `slot`, in connect() call order.
    std::vector<SlotId> upstream_of(SlotId slot) const;

    /// @brief The currently active, published RigGraph (for the RT thread to acquire from).
    SushiDSP::RigGraph* active_graph();

    /// @brief The RtGraphSlot the audio callback should acquire_for_audio_thread() from.
    SushiDSP::RtGraphSlot& graph_slot();

    /// @brief Serializes the current rig to JSON via SushiDSP::serialize_rig_to_json(). Cabinet
    /// IR is included (see Task 3 of this plan) once wired.
    std::string save_to_json() const;

    /// @brief Replaces the entire rig from JSON via SushiDSP::deserialize_rig_from_json() and
    /// republishes. Fresh SlotIds are assigned in the parsed graph's node order — preset-local
    /// continuity across a save/load round trip is not preserved (the JSON schema was never
    /// designed to carry a GUI-local SlotId, only a preset-local integer id).
    /// @throws std::runtime_error if json_text is malformed (same contract as
    /// SushiDSP::deserialize_rig_from_json()).
    void load_from_json(const std::string& json_text);

private:
    struct Slot
    {
        std::string                     descriptor_id;
        std::map<std::string, float>    parameters;
        std::string                     cabinet_path;
    };
    struct Edge { SlotId from; SlotId to; };

    void rebuild_and_publish();
    void apply_cabinet_to_live_node(SlotId slot);
    void seed_defaults(Slot& slot) const;

    SushiDSP::PrepareInfo                          info_;
    std::unordered_map<SlotId, Slot>               slots_;
    std::vector<Edge>                              edges_;
    std::vector<SlotId>                             slot_order_;
    SlotId                                          next_slot_id_ = 0;
    std::unordered_map<SlotId, SushiDSP::NodeHandle> slot_to_handle_;
    SushiDSP::RtGraphSlot                          graph_slot_;
    SushiDSP::RigGraph*                            active_graph_ = nullptr;
    std::size_t                                     dropped_parameter_changes_ = 0;
};
```

- [ ] **Step 5: Rewrite RigController.cpp**

Replace `apps/host_gui/RigController.cpp`'s contents below the license header and includes with:

```cpp
namespace
{
    bool find_parameter_index(const std::string& descriptor_id, const char* param_id, std::uint16_t& out_index)
    {
        for (const auto& entry : SushiDSP::node_registry())
        {
            if (descriptor_id != entry.descriptor->id)
            {
                continue;
            }
            for (std::size_t p = 0; p < entry.descriptor->parameter_count; ++p)
            {
                if (std::strcmp(param_id, entry.descriptor->parameters[p].id) == 0)
                {
                    out_index = static_cast<std::uint16_t>(p);
                    return true;
                }
            }
            return false;
        }
        return false;
    }
}

RigController::RigController(const SushiDSP::PrepareInfo& info)
    : info_(info)
{
    rebuild_and_publish();
}

void RigController::seed_defaults(Slot& slot) const
{
    for (const auto& entry : SushiDSP::node_registry())
    {
        if (slot.descriptor_id == entry.descriptor->id)
        {
            for (std::size_t p = 0; p < entry.descriptor->parameter_count; ++p)
            {
                const auto& param = entry.descriptor->parameters[p];
                slot.parameters[param.id] = param.default_value;
            }
            break;
        }
    }
}

RigController::SlotId RigController::add_node(const std::string& descriptor_id)
{
    const SlotId id = next_slot_id_++;
    Slot slot;
    slot.descriptor_id = descriptor_id;
    seed_defaults(slot);
    slots_[id] = std::move(slot);
    slot_order_.push_back(id);
    rebuild_and_publish();
    return id;
}

void RigController::connect(SlotId from, SlotId to)
{
    edges_.push_back({ from, to });
    rebuild_and_publish();
}

void RigController::disconnect(SlotId from, SlotId to)
{
    edges_.erase(
        std::remove_if(edges_.begin(), edges_.end(),
            [from, to](const Edge& edge) { return edge.from == from && edge.to == to; }),
        edges_.end());
    rebuild_and_publish();
}

void RigController::remove(SlotId slot)
{
    // Find slot's one upstream and one downstream neighbor (if any) before erasing its edges,
    // so a middle-of-chain removal can reconnect around the gap — matches today's behavior of
    // never leaving two previously-adjacent slots disconnected by a removal.
    std::optional<SlotId> upstream;
    std::optional<SlotId> downstream;
    for (const Edge& edge : edges_)
    {
        if (edge.to == slot) { upstream = edge.from; }
        if (edge.from == slot) { downstream = edge.to; }
    }

    edges_.erase(
        std::remove_if(edges_.begin(), edges_.end(),
            [slot](const Edge& edge) { return edge.from == slot || edge.to == slot; }),
        edges_.end());

    if (upstream.has_value() && downstream.has_value())
    {
        edges_.push_back({ *upstream, *downstream });
    }

    slots_.erase(slot);
    slot_order_.erase(std::remove(slot_order_.begin(), slot_order_.end(), slot), slot_order_.end());
    rebuild_and_publish();
}

RigController::SlotId RigController::append(const std::string& descriptor_id)
{
    const SlotId id = next_slot_id_++;
    Slot slot;
    slot.descriptor_id = descriptor_id;
    seed_defaults(slot);
    slots_[id] = std::move(slot);

    if (!slot_order_.empty())
    {
        edges_.push_back({ slot_order_.back(), id });
    }
    slot_order_.push_back(id);
    rebuild_and_publish();
    return id;
}

RigController::SlotId RigController::insert_before(std::optional<SlotId> following, const std::string& descriptor_id)
{
    const SlotId id = next_slot_id_++;
    Slot slot;
    slot.descriptor_id = descriptor_id;
    seed_defaults(slot);
    slots_[id] = std::move(slot);

    if (following.has_value())
    {
        // Retarget whichever edge currently points at `following` (if any) to point at the new
        // slot instead, then connect the new slot to `following`.
        for (Edge& edge : edges_)
        {
            if (edge.to == *following)
            {
                edge.to = id;
            }
        }
        edges_.push_back({ id, *following });

        const auto position = std::find(slot_order_.begin(), slot_order_.end(), *following);
        slot_order_.insert(position, id);
    }
    else if (!slot_order_.empty())
    {
        edges_.push_back({ id, slot_order_.front() });
        slot_order_.insert(slot_order_.begin(), id);
    }
    else
    {
        slot_order_.push_back(id);
    }

    rebuild_and_publish();
    return id;
}

void RigController::move_before(SlotId moved, std::optional<SlotId> following)
{
    if (following.has_value() && *following == moved)
    {
        return;
    }

    // Detach `moved` the same way remove() does (reconnect its old neighbors around it), but
    // keep `moved`'s own slot alive.
    std::optional<SlotId> upstream;
    std::optional<SlotId> downstream;
    for (const Edge& edge : edges_)
    {
        if (edge.to == moved) { upstream = edge.from; }
        if (edge.from == moved) { downstream = edge.to; }
    }
    edges_.erase(
        std::remove_if(edges_.begin(), edges_.end(),
            [moved](const Edge& edge) { return edge.from == moved || edge.to == moved; }),
        edges_.end());
    if (upstream.has_value() && downstream.has_value())
    {
        edges_.push_back({ *upstream, *downstream });
    }
    slot_order_.erase(std::remove(slot_order_.begin(), slot_order_.end(), moved), slot_order_.end());

    if (following.has_value())
    {
        for (Edge& edge : edges_)
        {
            if (edge.to == *following)
            {
                edge.to = moved;
            }
        }
        edges_.push_back({ moved, *following });
        const auto position = std::find(slot_order_.begin(), slot_order_.end(), *following);
        slot_order_.insert(position, moved);
    }
    else if (!slot_order_.empty())
    {
        edges_.push_back({ moved, slot_order_.front() });
        slot_order_.insert(slot_order_.begin(), moved);
    }
    else
    {
        slot_order_.push_back(moved);
    }

    rebuild_and_publish();
}

void RigController::set_parameter(SlotId slot, const char* param_id, float value)
{
    const auto it = slots_.find(slot);
    if (it == slots_.end())
    {
        return;
    }
    it->second.parameters[param_id] = value;

    std::uint16_t parameter_index = 0;
    if (!find_parameter_index(it->second.descriptor_id, param_id, parameter_index))
    {
        return;
    }

    const auto handle_it = slot_to_handle_.find(slot);
    if (active_graph_ != nullptr && handle_it != slot_to_handle_.end())
    {
        const SushiDSP::ParameterChange change{ handle_it->second, parameter_index, value };
        if (!active_graph_->push_parameter_change(change))
        {
            ++dropped_parameter_changes_;
        }
    }
}

float RigController::get_parameter(SlotId slot, const char* param_id) const
{
    const auto it = slots_.find(slot);
    if (it == slots_.end())
    {
        return 0.0f;
    }
    const auto param_it = it->second.parameters.find(param_id);
    return param_it != it->second.parameters.end() ? param_it->second : 0.0f;
}

bool RigController::set_cabinet(SlotId slot, const std::string& path)
{
    const auto handle_it = slot_to_handle_.find(slot);
    if (handle_it == slot_to_handle_.end() || active_graph_ == nullptr)
    {
        return false;
    }
    auto* amp = dynamic_cast<SushiDSP::Jcm800*>(active_graph_->node(handle_it->second));
    if (amp == nullptr)
    {
        return false;
    }
    if (!amp->set_cabinet(SushiDSP::IrLoader::load(path, info_.sample_rate)))
    {
        return false;
    }
    slots_[slot].cabinet_path = path;
    return true;
}

void RigController::apply_cabinet_to_live_node(SlotId slot)
{
    const auto it = slots_.find(slot);
    const auto handle_it = slot_to_handle_.find(slot);
    if (it == slots_.end() || it->second.cabinet_path.empty()
        || handle_it == slot_to_handle_.end() || active_graph_ == nullptr)
    {
        return;
    }
    auto* amp = dynamic_cast<SushiDSP::Jcm800*>(active_graph_->node(handle_it->second));
    if (amp == nullptr)
    {
        it->second.cabinet_path.clear();
        return;
    }
    try
    {
        if (!amp->set_cabinet(SushiDSP::IrLoader::load(it->second.cabinet_path, info_.sample_rate)))
        {
            it->second.cabinet_path.clear();
        }
    }
    catch (const std::exception&)
    {
        it->second.cabinet_path.clear();
    }
}

void RigController::reclaim()
{
    graph_slot_.reclaim();
    if (active_graph_ == nullptr)
    {
        return;
    }
    for (const auto& [slot, handle] : slot_to_handle_)
    {
        if (auto* amp = dynamic_cast<SushiDSP::Jcm800*>(active_graph_->node(handle)))
        {
            amp->reclaim_cabinets();
        }
    }
}

std::size_t RigController::dropped_parameter_changes() const
{
    return dropped_parameter_changes_;
}

std::vector<RigController::SlotId> RigController::slot_order() const
{
    return slot_order_;
}

std::string RigController::descriptor_id(SlotId slot) const
{
    const auto it = slots_.find(slot);
    return it != slots_.end() ? it->second.descriptor_id : std::string();
}

std::string RigController::cabinet_path(SlotId slot) const
{
    const auto it = slots_.find(slot);
    return it != slots_.end() ? it->second.cabinet_path : std::string();
}

std::vector<RigController::SlotId> RigController::upstream_of(SlotId slot) const
{
    std::vector<SlotId> result;
    for (const Edge& edge : edges_)
    {
        if (edge.to == slot)
        {
            result.push_back(edge.from);
        }
    }
    return result;
}

SushiDSP::RigGraph* RigController::active_graph()
{
    return active_graph_;
}

SushiDSP::RtGraphSlot& RigController::graph_slot()
{
    return graph_slot_;
}

std::string RigController::save_to_json() const
{
    return active_graph_ != nullptr ? SushiDSP::serialize_rig_to_json(*active_graph_) : std::string("{\"nodes\":[]}");
}

void RigController::load_from_json(const std::string& json_text)
{
    SushiDSP::RigGraph scratch(info_.max_block, 64);
    SushiDSP::deserialize_rig_from_json(scratch, json_text, info_);
    scratch.compile(info_);

    slots_.clear();
    edges_.clear();
    slot_order_.clear();
    next_slot_id_ = 0;

    std::unordered_map<SushiDSP::NodeHandle, SlotId> handle_to_slot;
    for (SushiDSP::NodeHandle handle : scratch.nodes())
    {
        SushiDSP::IAudioNode* node = scratch.node(handle);
        const SlotId id = next_slot_id_++;
        Slot slot;
        slot.descriptor_id = node->type_id();
        seed_defaults(slot);
        for (const auto& entry : SushiDSP::node_registry())
        {
            if (slot.descriptor_id == entry.descriptor->id)
            {
                for (std::size_t p = 0; p < entry.descriptor->parameter_count; ++p)
                {
                    const char* param_id = entry.descriptor->parameters[p].id;
                    slot.parameters[param_id] = node->get_parameter(param_id);
                }
                break;
            }
        }
        slots_[id] = std::move(slot);
        slot_order_.push_back(id);
        handle_to_slot[handle] = id;
    }
    for (SushiDSP::NodeHandle handle : scratch.nodes())
    {
        for (SushiDSP::NodeHandle upstream_handle : scratch.upstream_of(handle))
        {
            edges_.push_back({ handle_to_slot.at(upstream_handle), handle_to_slot.at(handle) });
        }
    }

    rebuild_and_publish();
}

void RigController::rebuild_and_publish()
{
    auto graph = std::make_unique<SushiDSP::RigGraph>(info_.max_block, static_cast<std::uint32_t>(slot_order_.size() + 1));

    std::vector<SlotId> surviving_order;
    surviving_order.reserve(slot_order_.size());
    std::unordered_map<SlotId, SushiDSP::NodeHandle> fresh_handles;
    for (SlotId id : slot_order_)
    {
        auto node = SushiDSP::make_node(slots_.at(id).descriptor_id.c_str(), info_);
        if (node == nullptr)
        {
            slots_.erase(id);
            continue;
        }
        fresh_handles[id] = graph->add_node(std::move(node));
        surviving_order.push_back(id);
    }
    slot_order_ = std::move(surviving_order);
    edges_.erase(
        std::remove_if(edges_.begin(), edges_.end(),
            [&fresh_handles](const Edge& edge)
            { return fresh_handles.find(edge.from) == fresh_handles.end()
                  || fresh_handles.find(edge.to) == fresh_handles.end(); }),
        edges_.end());

    for (const Edge& edge : edges_)
    {
        graph->connect(fresh_handles.at(edge.from), fresh_handles.at(edge.to));
    }

    graph->compile(info_);

    for (SlotId id : slot_order_)
    {
        SushiDSP::IAudioNode* node = graph->node(fresh_handles.at(id));
        if (node == nullptr)
        {
            continue;
        }
        for (const auto& [param_id, value] : slots_.at(id).parameters)
        {
            node->set_parameter(param_id.c_str(), value);
        }
    }

    slot_to_handle_ = std::move(fresh_handles);
    active_graph_ = graph.get();

    for (SlotId id : slot_order_)
    {
        apply_cabinet_to_live_node(id);
    }

    graph_slot_.publish(std::move(graph));
}
```

Add `#include <algorithm>` and `#include "SushiDSP/rig_preset/RigPreset.hpp"` to the top of
`RigController.cpp`'s includes (alongside the existing `SushiDSP/io/IrLoader.hpp`,
`SushiDSP/jcm800/Jcm800.hpp`, `SushiDSP/node_registry/NodeRegistry.hpp`).

**Note on the removed "handle == chain position" comment/optimization**: the old
`set_parameter`'s comment explaining "a slot's chain position IS its NodeHandle" no longer
applies — `slot_to_handle_` now does this lookup explicitly (an `unordered_map::find`, O(1)
amortized, not the O(1)-by-construction the old scheme had, but not a hot path: this runs once
per GUI-driven parameter edit, not per audio block).

- [ ] **Step 6: Simplify SlotSelection.hpp**

Replace `apps/host_gui/SlotSelection.hpp`'s contents below the license header with:

```cpp
#pragma once

#include <algorithm>
#include <optional>
#include <vector>

#include "RigController.hpp"

/// @brief Whether the rack list's "currently selected slot" still names a live slot, kept out of
/// main.cpp's ImGui frame so it can be unit tested. A SlotId (unlike the old chain-index scheme)
/// stays valid across insert/move/remove, so the only remaining job after an edit is dropping a
/// selection that no longer exists — no index-remapping arithmetic is needed anymore.
namespace slot_selection
{
    /// @brief Whether `selected` still names a slot present in `order`.
    /// @param selected The current selection, or std::nullopt for none.
    /// @param order The rig's current slot_order().
    inline bool still_valid(std::optional<RigController::SlotId> selected, const std::vector<RigController::SlotId>& order)
    {
        if (!selected.has_value())
        {
            return false;
        }
        return std::find(order.begin(), order.end(), *selected) != order.end();
    }
}
```

This deletes `remap_after_insert`/`remap_after_move`/`remap_after_remove`/`clamp` entirely —
`SlotId` stability makes them unnecessary; `main.cpp`'s selection variable simply keeps whatever
`SlotId` it already held across an edit, and only needs `still_valid` to drop it if that slot was
removed.

- [ ] **Step 7: Rewire apps/host_gui/main.cpp**

Read the current file in full (Step 1 already did this) before editing — the exact surrounding
code may have shifted slightly since this plan was written. Make these changes:

1. Remove `#include "Preset.hpp"` from the includes; keep `#include "RigController.hpp"` and
   `#include "SlotSelection.hpp"`. Add `#include "SushiDSP/rig_preset/RigPreset.hpp"` and
   `#include <fstream>` if not already present (needed for reading/writing the preset file
   directly now that `save_preset`/`load_preset` no longer exist).

2. Change `apply_default_cabinet`'s signature from `std::size_t slot` to `RigController::SlotId slot`,
   and its body from `controller.chain_ids()`/index-checking to
   `if (controller.descriptor_id(slot) != "sushidsp.jcm800") { return; }` (drop the `ids`/`slot >= ids.size()`
   check entirely — a `SlotId` either names a live slot or `descriptor_id` returns `""`, which
   already fails the comparison safely).

3. Replace the Save/Load button block:

```cpp
        static char preset_path[256] = "preset.json";
        ImGui::InputText("Preset path", preset_path, sizeof(preset_path));
        ImGui::SameLine();
        if (ImGui::Button("Save"))
        {
            try
            {
                std::ofstream out(preset_path);
                if (!out) { throw std::runtime_error("could not open '" + std::string(preset_path) + "' for writing"); }
                out << rig_controller.save_to_json();
            }
            catch (const std::exception& e) { std::fprintf(stderr, "Save failed: %s\n", e.what()); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Load"))
        {
            try
            {
                std::ifstream in(preset_path);
                if (!in) { throw std::runtime_error("could not open '" + std::string(preset_path) + "' for reading"); }
                std::string json_text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                rig_controller.load_from_json(json_text);
            }
            catch (const std::exception& e) { std::fprintf(stderr, "Load failed: %s\n", e.what()); }
        }
```

Add `#include <iterator>` if `std::istreambuf_iterator` needs it (check whether it's already
transitively available; add explicitly if the build complains).

4. Replace the rack-list block's selection type and every index-based call. The `static int
   selected_slot = -1;` becomes `static std::optional<RigController::SlotId> selected_slot;`.
   The loop changes from `for (std::size_t i = 0; i < rack_ids.size(); ++i)` over
   `rig_controller.chain_ids()` to iterate `rig_controller.slot_order()`:

```cpp
            const auto rack_slots = rig_controller.slot_order();
            for (std::size_t i = 0; i < rack_slots.size(); ++i)
            {
                const RigController::SlotId slot = rack_slots[i];
                const std::string label = rig_controller.descriptor_id(slot);
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(label.c_str(), selected_slot == std::optional<RigController::SlotId>(slot), 0, ImVec2(150, 0)))
                {
                    selected_slot = slot;
                }
                if (ImGui::BeginDragDropSource())
                {
                    ImGui::SetDragDropPayload("RACK_SLOT_ID", &slot, sizeof(RigController::SlotId));
                    ImGui::Text("%s", label.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("RACK_SLOT_ID"))
                    {
                        const RigController::SlotId moved = *static_cast<const RigController::SlotId*>(payload->Data);
                        rig_controller.move_before(moved, slot);
                    }
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GEAR_DESCRIPTOR_ID"))
                    {
                        const char* descriptor_id = static_cast<const char*>(payload->Data);
                        const RigController::SlotId new_slot = rig_controller.insert_before(slot, std::string(descriptor_id));
                        apply_default_cabinet(rig_controller, new_slot, default_ir_path);
                        selected_slot = new_slot;
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("x"))
                {
                    rig_controller.remove(slot);
                }
                ImGui::PopID();
            }

            ImGui::Dummy(ImVec2(150, 20));
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GEAR_DESCRIPTOR_ID"))
                {
                    const char* descriptor_id = static_cast<const char*>(payload->Data);
                    const RigController::SlotId new_slot = rig_controller.append(std::string(descriptor_id));
                    apply_default_cabinet(rig_controller, new_slot, default_ir_path);
                    selected_slot = new_slot;
                }
                ImGui::EndDragDropTarget();
            }

            const auto current_order = rig_controller.slot_order();
            if (!slot_selection::still_valid(selected_slot, current_order))
            {
                selected_slot = std::nullopt;
            }

            ImGui::Separator();
            ImGui::Text("Parameters");
            if (selected_slot.has_value())
            {
                const RigController::SlotId slot = *selected_slot;
                const std::string slot_descriptor = rig_controller.descriptor_id(slot);
                for (const auto& entry : SushiDSP::node_registry())
                {
                    if (slot_descriptor != entry.descriptor->id)
                    {
                        continue;
                    }
                    for (std::size_t p = 0; p < entry.descriptor->parameter_count; ++p)
                    {
                        const SushiDSP::ParameterDescriptor& param = entry.descriptor->parameters[p];
                        float value = rig_controller.get_parameter(slot, param.id);
                        char label[128];
                        if (param.kind == SushiDSP::ParamKind::Decibel)
                        {
                            std::snprintf(label, sizeof(label), "%s (%s)", param.label, param.unit);
                        }
                        else
                        {
                            std::snprintf(label, sizeof(label), "%s", param.label);
                        }
                        if (ImGui::SliderFloat(label, &value, param.min, param.max))
                        {
                            rig_controller.set_parameter(slot, param.id, value);
                        }
                    }
                    break;
                }

                if (slot_descriptor == "sushidsp.jcm800")
                {
                    ImGui::Separator();
                    const std::string cabinet = rig_controller.cabinet_path(slot);
                    ImGui::Text("Cabinet: %s", cabinet.empty() ? "(none)" : cabinet.c_str());

                    static char cabinet_input[512] = "";
                    static std::optional<RigController::SlotId> cabinet_input_slot;
                    if (cabinet_input_slot != selected_slot)
                    {
                        cabinet_input_slot = selected_slot;
                        std::snprintf(cabinet_input, sizeof(cabinet_input), "%s", cabinet.c_str());
                    }
                    ImGui::InputText("Cabinet IR path", cabinet_input, sizeof(cabinet_input));
                    ImGui::SameLine();
                    if (ImGui::Button("Load##cabinet"))
                    {
                        cabinet_error.clear();
                        try
                        {
                            if (!rig_controller.set_cabinet(slot, cabinet_input))
                            {
                                cabinet_error = "Cabinet not installed: sample rate mismatch or unprepared amp";
                            }
                        }
                        catch (const std::exception& error)
                        {
                            cabinet_error = error.what();
                        }
                        cabinet_error_time = ImGui::GetTime();
                    }
                    if (!cabinet_error.empty())
                    {
                        if (ImGui::GetTime() - cabinet_error_time > 3.0)
                        {
                            cabinet_error.clear();
                        }
                        else
                        {
                            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", cabinet_error.c_str());
                        }
                    }
                }

                if (rig_controller.dropped_parameter_changes() > 0)
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "Dropped parameter changes: %zu",
                        rig_controller.dropped_parameter_changes());
                }
            }
```

Note the `RACK_SLOT_ID` drag-drop payload now carries a `RigController::SlotId` (4 bytes) instead
of a `std::size_t` index — rename the payload type string too (`"RACK_SLOT_INDEX"` →
`"RACK_SLOT_ID"`) so a stale drag from before this change can never be misread as the new type.

- [ ] **Step 8: Delete the old Preset files and update CMake**

Delete `apps/host_gui/Preset.hpp`, `apps/host_gui/Preset.cpp`,
`tests/functional/unit/test_preset.cpp`.

In `apps/host_gui/CMakeLists.txt`, remove `Preset.cpp` from `add_executable(sushidsp_host_gui
...)`'s source list, and add `SushiDSP::rig_preset` to its `target_link_libraries`.

In `tests/CMakeLists.txt`: remove `functional/unit/test_preset.cpp` and
`${CMAKE_SOURCE_DIR}/apps/host_gui/Preset.cpp` from `sushidsp_tests`'s source list; add
`SushiDSP::rig_preset` to `sushidsp_tests`'s `target_link_libraries`.

- [ ] **Step 9: Run tests to verify they pass**

Run: `sd build` (confirms `sushidsp_host_gui` still links and `sushidsp_tests` still builds
without `Preset.cpp`), then `sd test -s unit`. Expected: every new `Unit_Dsp.RigController*` and
`Unit_Dsp.SlotSelection*` test passes; no `test_preset.cpp`/`test_rig_controller.cpp`-old-shape
test remains (they were replaced, not left alongside).

- [ ] **Step 10: Manual smoke test**

Run `sushidsp_host_gui.exe`, connect via SDL2, and confirm: dragging pedals from the gear panel
into the rack strip still builds a left-to-right chain exactly as before; reordering via drag
still works; removing a slot still leaves the remaining chain connected; parameter sliders still
move the right node; Save writes a `.json` file and Load rebuilds the same chain with the same
parameter values (cabinet excluded — expected until Task 3). Describe exactly what you did and
saw in your report.

- [ ] **Step 11: Commit**

```bash
git add apps/host_gui tests/functional/unit/test_rig_controller.cpp tests/CMakeLists.txt
git rm apps/host_gui/Preset.hpp apps/host_gui/Preset.cpp tests/functional/unit/test_preset.cpp
git commit -m "host_gui: RigController becomes a SlotId-based node graph; presets go through modules/rig_preset"
```

---

### Task 3: Cabinet IR round-trip through the preset callbacks

**Files:**
- Modify: `apps/host_gui/RigController.cpp` (`save_to_json`/`load_from_json`)
- Test: `tests/functional/unit/test_rig_controller.cpp` (append)

**Interfaces:**
- Consumes: `SushiDSP::AnnotateNodeFn`/`OnNodeLoadedFn` (Task 1), `Jcm800::set_cabinet`/
  `IrLoader::load` (pre-existing).
- Produces: `RigController::save_to_json()`/`load_from_json()` now round-trip a JCM800's
  cabinet IR path via the `"cabinet_ir"` JSON field.

This closes the "cabinet IR silently dropped" limitation `modules/rig_preset`'s Doxygen
documents, for the one concrete consumer (`RigController`) that now exists.

- [ ] **Step 1: Write the failing test**

Append to `tests/functional/unit/test_rig_controller.cpp`. This reuses the exact
minimal-WAV-writing helper the now-deleted `test_preset.cpp` used for its own
`PresetRoundTripsCabinetPath` test (a hand-built 64-sample mono 16-bit PCM impulse, just enough
for `IrLoader` to produce a valid `Ir`) — add it to this file's anonymous namespace:

```cpp
namespace
{
    void append_u32_le(std::vector<std::uint8_t>& buffer, std::uint32_t value)
    {
        for (int byte = 0; byte < 4; ++byte)
        {
            buffer.push_back(static_cast<std::uint8_t>((value >> (8 * byte)) & 0xFF));
        }
    }

    void append_u16_le(std::vector<std::uint8_t>& buffer, std::uint16_t value)
    {
        buffer.push_back(static_cast<std::uint8_t>(value & 0xFF));
        buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    }

    void append_tag(std::vector<std::uint8_t>& buffer, const char* tag)
    {
        buffer.insert(buffer.end(), tag, tag + 4);
    }

    std::string write_temp_impulse_wav()
    {
        const std::uint32_t sample_rate = 48000;
        std::vector<std::int16_t> samples(64, 0);
        samples[0] = 16384;

        const std::uint32_t data_bytes = static_cast<std::uint32_t>(samples.size() * sizeof(std::int16_t));
        std::vector<std::uint8_t> bytes;
        append_tag(bytes, "RIFF");
        append_u32_le(bytes, 36 + data_bytes);
        append_tag(bytes, "WAVE");
        append_tag(bytes, "fmt ");
        append_u32_le(bytes, 16);
        append_u16_le(bytes, 1);
        append_u16_le(bytes, 1);
        append_u32_le(bytes, sample_rate);
        append_u32_le(bytes, sample_rate * 2);
        append_u16_le(bytes, 2);
        append_u16_le(bytes, 16);
        append_tag(bytes, "data");
        append_u32_le(bytes, data_bytes);
        for (std::int16_t sample : samples)
        {
            append_u16_le(bytes, static_cast<std::uint16_t>(sample));
        }

        const std::string path = std::string(::testing::TempDir()) + "sushidsp_test_cabinet.wav";
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        return path;
    }
}

TEST(Unit_Dsp, RigControllerSaveAndLoadJsonRoundTripsCabinetIr)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    const std::string ir_path = write_temp_impulse_wav();

    RigController original(info);
    const RigController::SlotId amp = original.append("sushidsp.jcm800");
    ASSERT_TRUE(original.set_cabinet(amp, ir_path));

    const std::string json_text = original.save_to_json();

    RigController reloaded(info);
    reloaded.load_from_json(json_text);

    const auto order = reloaded.slot_order();
    ASSERT_EQ(order.size(), 1u);
    EXPECT_EQ(reloaded.cabinet_path(order[0]), ir_path);

    std::remove(ir_path.c_str());
}
```

Add `#include <cstdint>` and `#include <fstream>` to the test file's includes if not already
present (both are needed by the WAV-writing helper).

- [ ] **Step 2: Run test to verify it fails**

Run: `sd test -s unit`. Expected: `RigControllerSaveAndLoadJsonRoundTripsCabinetIr` FAILS
(`cabinet_path(order[0])` is empty — the cabinet was never written to JSON).

- [ ] **Step 3: Wire the callbacks in save_to_json/load_from_json**

In `apps/host_gui/RigController.cpp`, change `save_to_json()`:

```cpp
std::string RigController::save_to_json() const
{
    if (active_graph_ == nullptr)
    {
        return "{\"nodes\":[]}";
    }
    return SushiDSP::serialize_rig_to_json(*active_graph_,
        [this](SushiDSP::NodeHandle handle, nlohmann::json& node_json)
        {
            for (const auto& [slot, mapped_handle] : slot_to_handle_)
            {
                if (mapped_handle == handle && !slots_.at(slot).cabinet_path.empty())
                {
                    node_json["cabinet_ir"] = slots_.at(slot).cabinet_path;
                    return;
                }
            }
        });
}
```

Add `#include <nlohmann/json.hpp>` to `RigController.cpp`'s includes.

Change `load_from_json()`'s call to `deserialize_rig_from_json` to pass a callback that records
each node's `"cabinet_ir"` field (if present) against its handle, then apply it after the slot
is created in the main loop below — restructure the function's body:

```cpp
void RigController::load_from_json(const std::string& json_text)
{
    SushiDSP::RigGraph scratch(info_.max_block, 64);
    std::unordered_map<SushiDSP::NodeHandle, std::string> cabinet_paths_by_handle;
    SushiDSP::deserialize_rig_from_json(scratch, json_text, info_,
        [&cabinet_paths_by_handle](SushiDSP::NodeHandle handle, const nlohmann::json& node_json)
        {
            if (node_json.contains("cabinet_ir"))
            {
                cabinet_paths_by_handle[handle] = node_json.at("cabinet_ir").get<std::string>();
            }
        });
    scratch.compile(info_);

    slots_.clear();
    edges_.clear();
    slot_order_.clear();
    next_slot_id_ = 0;

    std::unordered_map<SushiDSP::NodeHandle, SlotId> handle_to_slot;
    for (SushiDSP::NodeHandle handle : scratch.nodes())
    {
        SushiDSP::IAudioNode* node = scratch.node(handle);
        const SlotId id = next_slot_id_++;
        Slot slot;
        slot.descriptor_id = node->type_id();
        seed_defaults(slot);
        for (const auto& entry : SushiDSP::node_registry())
        {
            if (slot.descriptor_id == entry.descriptor->id)
            {
                for (std::size_t p = 0; p < entry.descriptor->parameter_count; ++p)
                {
                    const char* param_id = entry.descriptor->parameters[p].id;
                    slot.parameters[param_id] = node->get_parameter(param_id);
                }
                break;
            }
        }
        const auto cabinet_it = cabinet_paths_by_handle.find(handle);
        if (cabinet_it != cabinet_paths_by_handle.end())
        {
            slot.cabinet_path = cabinet_it->second;
        }
        slots_[id] = std::move(slot);
        slot_order_.push_back(id);
        handle_to_slot[handle] = id;
    }
    for (SushiDSP::NodeHandle handle : scratch.nodes())
    {
        for (SushiDSP::NodeHandle upstream_handle : scratch.upstream_of(handle))
        {
            edges_.push_back({ handle_to_slot.at(upstream_handle), handle_to_slot.at(handle) });
        }
    }

    rebuild_and_publish();
}
```

`rebuild_and_publish()` already calls `apply_cabinet_to_live_node()` for every slot at the end of
its own body (unchanged from Task 2), so populating `Slot::cabinet_path` here is sufficient —
no separate cabinet-installation call is needed in `load_from_json` itself.

- [ ] **Step 4: Run tests to verify they pass**

Run: `sd test -s unit`. Expected: `RigControllerSaveAndLoadJsonRoundTripsCabinetIr` passes.

- [ ] **Step 5: Manual smoke test**

Run `sushidsp_host_gui.exe`, load a cabinet IR onto a JCM800 slot, Save, restart the app (or just
Load into a fresh session state if the app supports it without restarting), Load the same file,
and confirm the Cabinet field shows the same path and the amp sounds the same (cabinet applied,
not dry). Describe what you did and heard/observed in your report.

- [ ] **Step 6: Commit**

```bash
git add apps/host_gui/RigController.cpp tests/functional/unit/test_rig_controller.cpp
git commit -m "host_gui: round-trip a JCM800's cabinet IR through save_to_json/load_from_json"
```

---

## Final Review

After Task 3, run `sd test -s unit` once more for the whole suite. Re-read
`docs/superpowers/specs/2026-08-26-rigcontroller-dag-support-design.md` against what was
actually built and confirm every "Goals" bullet has a corresponding passing test or verified
manual behavior. Confirm explicitly in the final report: `apps/host_gui/Preset.hpp`/`.cpp` and
`tests/functional/unit/test_preset.cpp` are gone; `apps/host_gui/main.cpp` no longer contains any
`std::size_t`-as-slot-index arithmetic; a linear chain built through the GUI behaves identically
to before this plan (per Task 2 Step 10's and Task 3 Step 5's manual verification).
