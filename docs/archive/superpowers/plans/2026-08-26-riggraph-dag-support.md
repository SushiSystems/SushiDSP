# RigGraph DAG Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Generalize `RigGraph` from a linear chain to a true DAG (fan-out and fan-in), add
`SplitterNode`/`MixerNode` to route and mix parallel signal paths, and add a DAG-capable JSON
preset serializer — all in `core`/`modules`, with zero change to `apps/host_gui`'s current
linear-chain UI.

**Architecture:** `RigGraph` already stores a plain `Edge` list and computes a topological order;
`process()` currently assumes exactly one upstream edge per node and copies the last
topologically-sorted node to the graph output. This plan fixes both assumptions (multi-input
gather, explicit single-sink detection), adds cycle detection to `compile()`, adds a new
`modules/routing` library with `SplitterNode`/`MixerNode`, and adds a new `modules/rig_preset`
library that serializes/deserializes an arbitrary `RigGraph` topology to/from JSON — independent
of `apps/host_gui`'s existing `RigController`/`Preset.cpp`, which stay untouched.

**Tech Stack:** C++17, GoogleTest (already in the tree via `tests/CMakeLists.txt`), nlohmann::json
(already fetched unconditionally by the root `CMakeLists.txt`).

**Spec:** `docs/superpowers/specs/2026-08-26-riggraph-dag-design.md`

## Global Constraints

- Everything reachable from `IAudioNode::process()` — `SplitterNode::process()`,
  `MixerNode::process()`, and `RigGraph::process()`'s multi-input gather — stays
  allocation-free, lock-free, syscall-free, throw-free. No `std::vector` resize inside any
  `process()` override; use fixed-size stack arrays sized by the constants defined in Task 2.
- `RigGraph` structural methods (`add_node`, `remove_node`, `connect`, `disconnect`, `compile`)
  are prepare-time-only; never called from the audio thread. This plan's new checks
  (fan-in/fan-out limits in `connect()`, cycle/multi-sink detection in `compile()`) live on this
  same non-RT side and may throw `std::runtime_error`.
- New CMake targets `SushiDSP::routing` and `SushiDSP::rig_preset` follow the existing
  `SushiDSP::<module>` alias convention (mirrors `modules/tubescreamer`, `modules/jcm800`,
  `modules/node_registry`).
- Every `.hpp`/`.cpp` file (tests included) carries the exact 76-column boxed license header —
  copy it verbatim from `include/SushiDSP/core/PortLayout.hpp`, changing only the filename line.
- C++17 only, Allman braces, `PascalCase` types / `snake_case` functions and variables /
  `impl_`-style trailing-underscore members / `UPPER_SNAKE` constants, no abbreviations,
  Doxygen `@brief`/`@param`/`@return` on every public function, no comments beyond non-obvious
  *why*.
- Test files go in `tests/functional/unit/test_<snake_case_name>.cpp`, registered in
  `tests/CMakeLists.txt` under a matching `# --- Section ---` group, GTest case names prefixed
  `Unit_Dsp` (matching every existing test in this suite).
- `apps/host_gui`'s existing rack strip, drag-and-drop, parameter panel, device selection, and
  JSON preset save/load are unchanged by every task in this plan — no file under `apps/host_gui/`
  is modified.
- Run `sd build` and `sd test -s unit` after every task; never invoke `cmake`/`ctest` directly.

---

### Task 1: `RigGraph::compile()` — cycle detection and single-sink detection

**Files:**
- Modify: `include/SushiDSP/core/RigGraph.hpp`
- Modify: `src/core/RigGraph.cpp`
- Test: `tests/functional/unit/test_rig_graph.cpp` (append)

**Interfaces:**
- Produces: `RigGraph::compile()` now throws `std::runtime_error` on a cycle or on more than one
  sink node (a "sink" is a live node with no outgoing edge); it caches the single sink's handle
  in a new private member `sink_handle_` (type `NodeHandle`, valid only when
  `!topological_order_.empty()`) for Task 2's `process()` to read.
- Consumes: nothing new from other tasks.

Today, `compile()`'s Kahn's-algorithm loop silently produces a `topological_order_` shorter than
`nodes_.size()` when a cycle exists (the queue empties before every node is visited), and
`process()` (unchanged until Task 2) always treats `topological_order_.back()` as "the" output
node — which is already wrong for a graph with more than one node that has no outgoing edge, a
case impossible to construct with today's GUI (linear chains only) but now reachable once
`SplitterNode`/`MixerNode` exist. This task closes both gaps at the one place a badly-shaped
graph can be caught early and loudly: `compile()`.

- [ ] **Step 1: Write the failing tests**

Append to `tests/functional/unit/test_rig_graph.cpp` (the file already has a `GainNode`,
`ConstantSourceNode` and `LatencyNode` in its anonymous namespace — reuse them):

```cpp
TEST(Unit_Dsp, RigGraphCompileThrowsOnCycle)
{
    SushiDSP::RigGraph graph(256, 4);
    auto a = graph.add_node(std::make_unique<GainNode>(1.0f));
    auto b = graph.add_node(std::make_unique<GainNode>(1.0f));
    graph.connect(a, b);
    graph.connect(b, a);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    EXPECT_THROW(graph.compile(info), std::runtime_error);
}

TEST(Unit_Dsp, RigGraphCompileThrowsOnMultipleSinkNodes)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    auto sink_a = graph.add_node(std::make_unique<GainNode>(2.0f));
    auto sink_b = graph.add_node(std::make_unique<GainNode>(3.0f));
    graph.connect(source, sink_a);
    graph.connect(source, sink_b);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    EXPECT_THROW(graph.compile(info), std::runtime_error);
}

TEST(Unit_Dsp, RigGraphCompileAcceptsSingleSinkGraph)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    auto sink = graph.add_node(std::make_unique<GainNode>(2.0f));
    graph.connect(source, sink);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    EXPECT_NO_THROW(graph.compile(info));
}
```

Add `#include <stdexcept>` near the top of `tests/functional/unit/test_rig_graph.cpp` if it is
not already included.

- [ ] **Step 2: Run tests to verify they fail**

Run: `sd test -s unit`. Expected: `RigGraphCompileThrowsOnCycle` and
`RigGraphCompileThrowsOnMultipleSinkNodes` FAIL (no exception thrown today);
`RigGraphCompileAcceptsSingleSinkGraph` already passes (no behavior change needed for that case)
— confirm it still passes after Step 4, it is here as a regression guard.

- [ ] **Step 3: Add `sink_handle_` and `#include <stdexcept>` to RigGraph.hpp**

In `include/SushiDSP/core/RigGraph.hpp`, add `#include <stdexcept>` to the includes block, and
add a new private member after `topological_order_`:

```cpp
        std::vector<NodeHandle>                   topological_order_;
        NodeHandle                                sink_handle_ = 0;
```

- [ ] **Step 4: Implement cycle and multi-sink detection in RigGraph::compile()**

In `src/core/RigGraph.cpp`, replace the body of `compile()` from the `topological_order_.clear();`
line through the end of the `while (!ready.empty())` loop with:

```cpp
        topological_order_.clear();
        while (!ready.empty())
        {
            const NodeHandle current = ready.front();
            ready.pop();
            topological_order_.push_back(current);

            for (const Edge& edge : edges_)
            {
                if (edge.from == current)
                {
                    if (--in_degree[edge.to] == 0 && nodes_[edge.to] != nullptr)
                    {
                        ready.push(edge.to);
                    }
                }
            }
        }

        const std::size_t live_node_count =
            static_cast<std::size_t>(std::count_if(nodes_.begin(), nodes_.end(),
                [](const std::unique_ptr<IAudioNode>& node) { return node != nullptr; }));
        if (topological_order_.size() != live_node_count)
        {
            throw std::runtime_error("RigGraph::compile: cycle detected");
        }

        std::vector<bool> has_outgoing_edge(nodes_.size(), false);
        for (const Edge& edge : edges_)
        {
            has_outgoing_edge[edge.from] = true;
        }
        std::size_t sink_count = 0;
        for (NodeHandle handle : topological_order_)
        {
            if (!has_outgoing_edge[handle])
            {
                sink_handle_ = handle;
                ++sink_count;
            }
        }
        if (sink_count > 1)
        {
            throw std::runtime_error("RigGraph::compile: multiple sink nodes (ambiguous graph output)");
        }
```

This sits between the existing `in_degree`/`ready` setup (unchanged, above) and the existing
`node_output_scratch_.assign(...)` / `prepare()` loop (unchanged, below) — do not duplicate
either of those blocks.

- [ ] **Step 5: Run tests to verify they pass**

Run: `sd test -s unit`. Expected: all three new tests pass, and every pre-existing
`Unit_Dsp.RigGraph*` test still passes (they all build single-chain or single-sink graphs).

- [ ] **Step 6: Commit**

```bash
git add include/SushiDSP/core/RigGraph.hpp src/core/RigGraph.cpp tests/functional/unit/test_rig_graph.cpp
git commit -m "core: RigGraph::compile() detects cycles and ambiguous multi-sink graphs"
```

---

### Task 2: `RigGraph::connect()` — fan-in/fan-out limit enforcement

**Files:**
- Modify: `include/SushiDSP/core/RigGraph.hpp`
- Modify: `src/core/RigGraph.cpp`
- Test: `tests/functional/unit/test_rig_graph.cpp` (append)

**Interfaces:**
- Produces: `RigGraph::kMaxFanInOut` (public `static constexpr std::size_t`, value `8`) — the
  maximum outgoing edges any one node may have and the maximum incoming edges any one node may
  have, enforced by `connect()`. Consumed by Task 3 (the fixed-size input-gather array uses this
  same bound) and by Task 5/6 (`SplitterNode`/`MixerNode` size their own port declarations to
  match, so a fully-fanned node never gets more edges than `RigGraph` will accept).
- Consumes: nothing new from other tasks.

`connect()` today unconditionally `push_back`s an edge with no limit. Task 3 will process a
node's inputs into a fixed-size stack array; that array's size must be a compile-time constant,
and `connect()` must refuse to exceed it — otherwise a rig with more than N-way fan-in would
silently drop inputs at process() time instead of failing loudly when the user builds the graph.

- [ ] **Step 1: Write the failing tests**

Append to `tests/functional/unit/test_rig_graph.cpp`:

```cpp
TEST(Unit_Dsp, RigGraphConnectThrowsWhenFanOutExceedsLimit)
{
    SushiDSP::RigGraph graph(256, SushiDSP::RigGraph::kMaxFanInOut + 2);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    std::vector<SushiDSP::NodeHandle> sinks;
    for (std::size_t i = 0; i < SushiDSP::RigGraph::kMaxFanInOut; ++i)
    {
        sinks.push_back(graph.add_node(std::make_unique<GainNode>(1.0f)));
        graph.connect(source, sinks.back());
    }

    auto one_too_many = graph.add_node(std::make_unique<GainNode>(1.0f));
    EXPECT_THROW(graph.connect(source, one_too_many), std::runtime_error);
}

TEST(Unit_Dsp, RigGraphConnectThrowsWhenFanInExceedsLimit)
{
    SushiDSP::RigGraph graph(256, SushiDSP::RigGraph::kMaxFanInOut + 2);
    auto sink = graph.add_node(std::make_unique<GainNode>(1.0f));
    std::vector<SushiDSP::NodeHandle> sources;
    for (std::size_t i = 0; i < SushiDSP::RigGraph::kMaxFanInOut; ++i)
    {
        sources.push_back(graph.add_node(std::make_unique<ConstantSourceNode>(1.0f)));
        graph.connect(sources.back(), sink);
    }

    auto one_too_many = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    EXPECT_THROW(graph.connect(one_too_many, sink), std::runtime_error);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `sd test -s unit`. Expected: both new tests FAIL to compile (`kMaxFanInOut` does not exist
yet) — this is expected; proceed to Step 3 before re-running.

- [ ] **Step 3: Declare `kMaxFanInOut` in RigGraph.hpp**

In `include/SushiDSP/core/RigGraph.hpp`, add alongside the existing
`kParameterRingCapacity` constant:

```cpp
        /// @brief Maximum outgoing edges from, or incoming edges into, any single node.
        /// Bounds the fixed-size input-gather array RigGraph::process() uses per node.
        static constexpr std::size_t kMaxFanInOut = 8;
```

- [ ] **Step 4: Enforce the limit in RigGraph::connect()**

In `src/core/RigGraph.cpp`, replace `connect()`'s body:

```cpp
    void RigGraph::connect(NodeHandle from, NodeHandle to)
    {
        std::size_t fan_out = 0;
        std::size_t fan_in = 0;
        for (const Edge& edge : edges_)
        {
            if (edge.from == from) { ++fan_out; }
            if (edge.to == to) { ++fan_in; }
        }
        if (fan_out >= kMaxFanInOut)
        {
            throw std::runtime_error("RigGraph::connect: fan-out limit exceeded");
        }
        if (fan_in >= kMaxFanInOut)
        {
            throw std::runtime_error("RigGraph::connect: fan-in limit exceeded");
        }
        edges_.push_back({ from, to });
    }
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `sd test -s unit`. Expected: both new tests pass; every pre-existing `Unit_Dsp.RigGraph*`
test (none exceeds 8-way fan-in/out) still passes.

- [ ] **Step 6: Commit**

```bash
git add include/SushiDSP/core/RigGraph.hpp src/core/RigGraph.cpp tests/functional/unit/test_rig_graph.cpp
git commit -m "core: RigGraph::connect() enforces an 8-way fan-in/fan-out limit"
```

---

### Task 3: `RigGraph::process()` — multi-input gather and sink-based output

**Files:**
- Modify: `src/core/RigGraph.cpp`
- Test: `tests/functional/unit/test_rig_graph.cpp` (append)

**Interfaces:**
- Consumes: `RigGraph::kMaxFanInOut` (Task 2), `sink_handle_` (Task 1).
- Produces: `RigGraph::process()` now gathers every upstream edge into an input array (not just
  the first) and writes the graph's output from `sink_handle_` rather than
  `topological_order_.back()`. No signature change — existing callers (`apps/host`,
  `apps/host_asio`, `apps/host_gui`) are unaffected since every graph they build today has
  exactly one sink, which is also always last in topological order (this task changes *how*
  the sink is found, not which node it resolves to for any graph buildable today).

This is the core routing fix: a node with two upstream edges must see both, and the graph's
final output must come from the one true sink node, not "whatever compile() scheduled last" —
which is only accidentally correct for the linear chains built today.

- [ ] **Step 1: Write the failing tests**

Append to `tests/functional/unit/test_rig_graph.cpp` (this needs a node that reads more than one
input; add it to the file's anonymous namespace alongside `GainNode`):

```cpp
namespace
{
    class SumTwoInputsNode : public SushiDSP::IAudioNode
    {
    public:
        void prepare(const SushiDSP::PrepareInfo&) override {}

        void process(SushiDSP::ProcessContext& context) override
        {
            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
            {
                const float a = context.inputs[0] != nullptr ? context.inputs[0][frame] : 0.0f;
                const float b = context.inputs[1] != nullptr ? context.inputs[1][frame] : 0.0f;
                context.outputs[0][frame] = a + b;
            }
        }

        SushiDSP::PortLayout ports() const override { return { 2, 1 }; }
        void reset() override {}
        float get_parameter(const char*) const override { return 0.0f; }
        void set_parameter(const char*, float) override {}
    };
}

TEST(Unit_Dsp, RigGraphProcessGathersAllUpstreamInputsInConnectOrder)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source_a = graph.add_node(std::make_unique<ConstantSourceNode>(3.0f));
    auto source_b = graph.add_node(std::make_unique<ConstantSourceNode>(4.0f));
    auto sum = graph.add_node(std::make_unique<SumTwoInputsNode>());
    graph.connect(source_a, sum);
    graph.connect(source_b, sum);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = { output_buffer.data() };
    SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 7.0f);
    }
}

TEST(Unit_Dsp, RigGraphProcessFansOutOneNodeToTwoDownstreamNodes)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(2.0f));
    auto branch_a = graph.add_node(std::make_unique<GainNode>(3.0f));
    auto branch_b = graph.add_node(std::make_unique<GainNode>(5.0f));
    auto sum = graph.add_node(std::make_unique<SumTwoInputsNode>());
    graph.connect(source, branch_a);
    graph.connect(source, branch_b);
    graph.connect(branch_a, sum);
    graph.connect(branch_b, sum);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = { output_buffer.data() };
    SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };
    graph.process(context);

    // source(2.0) -> branch_a(*3=6.0), branch_b(*5=10.0) -> sum(6.0+10.0=16.0)
    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 16.0f);
    }
}
```

`GainNode` reads `context.inputs[0]` only, so both new tests exercise the existing single-input
path unchanged as well as the new multi-input path; `ConstantSourceNode` has 0 input ports and
is unaffected. Note `SumTwoInputsNode`'s `ports()` reports `{2, 1}`, but nothing in `RigGraph`
currently reads `ports()` to size anything — this plan does not change that; the test node simply
declares its true shape for documentation.

- [ ] **Step 2: Run tests to verify they fail**

Run: `sd test -s unit`. Expected: `RigGraphProcessGathersAllUpstreamInputsInConnectOrder` FAILS
(today's `process()` only wires the first matching edge, so `inputs[1]` stays whatever
`SumTwoInputsNode` sees as `nullptr`, giving `3.0` instead of `7.0`).
`RigGraphProcessFansOutOneNodeToTwoDownstreamNodes` may already incidentally pass or fail
depending on today's `break`-on-first-edge behavior for the fan-in side — either way, re-verify
after Step 3.

- [ ] **Step 3: Rewrite the gather/output section of RigGraph::process()**

In `src/core/RigGraph.cpp`, replace the `for (NodeHandle handle : topological_order_)` loop body
(from `float* node_buffer = buffer_pool_.acquire(context.frames);` through
`node_output[handle] = node_buffer;`) with:

```cpp
            float* node_buffer = buffer_pool_.acquire(context.frames);

            const float* gathered_inputs[kMaxFanInOut];
            std::size_t input_count = 0;
            for (const Edge& edge : edges_)
            {
                if (edge.to == handle)
                {
                    gathered_inputs[input_count] = node_output[edge.from];
                    ++input_count;
                }
            }

            const float* const* node_inputs = nullptr;
            if (input_count > 0)
            {
                node_inputs = gathered_inputs;
            }
            else if (context.inputs != nullptr)
            {
                node_inputs = context.inputs;
            }

            float* output_ptr[1] = { node_buffer };
            ProcessContext node_context{ output_ptr, node_inputs, context.frames, context.time_seconds };
            nodes_[handle]->process(node_context);

            node_output[handle] = node_buffer;
```

This removes the old `has_upstream_edge`/`upstream_input`/`input_ptr[1]` single-edge lookup
entirely — `gathered_inputs` replaces it and generalizes to up to `kMaxFanInOut` edges, matching
`connect()`'s Task 2 limit exactly (a node can never receive more upstream edges than
`gathered_inputs` has slots for).

Then replace the final-output block (`if (!topological_order_.empty()) { ... }`) to read from
`sink_handle_` instead of `topological_order_.back()`:

```cpp
        if (!topological_order_.empty())
        {
            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
            {
                context.outputs[0][frame] = node_output[sink_handle_][frame];
            }
        }
```

(The `else` branch that zero-fills for an empty graph is unchanged — leave it as-is.)

- [ ] **Step 4: Run tests to verify they pass**

Run: `sd test -s unit`. Expected: both new tests pass; every pre-existing `Unit_Dsp.RigGraph*`
test still passes (a linear chain's only sink is also its last topologically-sorted node, so
`sink_handle_` and the old `topological_order_.back()` agree for every graph shape those tests
build).

- [ ] **Step 5: Commit**

```bash
git add src/core/RigGraph.cpp tests/functional/unit/test_rig_graph.cpp
git commit -m "core: RigGraph::process() gathers multi-input fan-in and outputs from the true sink node"
```

---

### Task 4: `RigGraph::latency_samples()` — max over branches

**Files:**
- Modify: `include/SushiDSP/core/RigGraph.hpp`
- Modify: `src/core/RigGraph.cpp`
- Test: `tests/functional/unit/test_rig_graph.cpp` (append)

**Interfaces:**
- Produces: `RigGraph::latency_samples()` unchanged signature, corrected semantics: for a
  branching graph, returns the longest upstream path's cumulative latency, not the sum over
  every node.
- Consumes: `topological_order_`, `edges_` (both already members; no new dependency).

Computed once at `compile()` time (matching this project's "engineer it once, run it cheaply"
performance principle from `CLAUDE.md` — `latency_samples()` itself must stay O(1) since it may
be polled from a UI or host every frame) and cached.

- [ ] **Step 1: Write the failing test**

Append to `tests/functional/unit/test_rig_graph.cpp` (reuses `LatencyNode`, already defined
lower in the file — if Task 1's or Task 3's inserted tests landed above `LatencyNode`'s
definition, move this test below that definition, or move the `namespace { class LatencyNode
... }` block up above all new tests; either is fine as long as it compiles):

```cpp
TEST(Unit_Dsp, RigGraphLatencyIsMaxOverParallelBranchesNotSum)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    auto short_branch = graph.add_node(std::make_unique<LatencyNode>(10));
    auto long_branch = graph.add_node(std::make_unique<LatencyNode>(200));
    auto sum = graph.add_node(std::make_unique<SumTwoInputsNode>());
    graph.connect(source, short_branch);
    graph.connect(source, long_branch);
    graph.connect(short_branch, sum);
    graph.connect(long_branch, sum);

    graph.compile(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });

    // sum itself has 0 latency; the graph's total is max(10, 200) + 0 = 200, not 10 + 200 = 210.
    EXPECT_EQ(graph.latency_samples(), 200u);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `sd test -s unit`. Expected: FAIL, current sum-based implementation reports `210`.

- [ ] **Step 3: Add a cached-latency member and compute it in compile()**

In `include/SushiDSP/core/RigGraph.hpp`, add a private member after `sink_handle_`:

```cpp
        std::uint32_t                              cached_latency_samples_ = 0;
```

In `src/core/RigGraph.cpp`, append to the end of `compile()` (after the existing `for (auto&
node : nodes_) { ... node->prepare(info); ... }` loop, still inside `compile()`):

```cpp
        std::vector<std::uint32_t> latency_at(nodes_.size(), 0);
        for (NodeHandle handle : topological_order_)
        {
            std::uint32_t upstream_max = 0;
            for (const Edge& edge : edges_)
            {
                if (edge.to == handle)
                {
                    upstream_max = std::max(upstream_max, latency_at[edge.from]);
                }
            }
            latency_at[handle] = upstream_max + nodes_[handle]->latency_samples();
        }
        cached_latency_samples_ = topological_order_.empty() ? 0u : latency_at[sink_handle_];
```

- [ ] **Step 4: Replace latency_samples() to return the cached value**

In `src/core/RigGraph.cpp`, replace the body of `latency_samples()`:

```cpp
    std::uint32_t RigGraph::latency_samples() const
    {
        return cached_latency_samples_;
    }
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `sd test -s unit`. Expected: the new test passes; the pre-existing
`RigGraphLatencyIsSumOfChainedNodeLatencies` test (a linear two-node chain, where max-over-a
single-branch equals the sum) still passes unchanged.

- [ ] **Step 6: Commit**

```bash
git add include/SushiDSP/core/RigGraph.hpp src/core/RigGraph.cpp tests/functional/unit/test_rig_graph.cpp
git commit -m "core: RigGraph::latency_samples() reports max-over-branches, cached at compile()"
```

---

### Task 5: `modules/routing` — `SplitterNode`

**Files:**
- Create: `modules/routing/CMakeLists.txt`
- Create: `modules/routing/include/SushiDSP/routing/SplitterNode.hpp`
- Create: `modules/routing/src/SplitterNode.cpp`
- Modify: `CMakeLists.txt` (add `add_subdirectory(modules/routing)`)
- Test: `tests/functional/unit/test_splitter_node.cpp`
- Modify: `tests/CMakeLists.txt` (register the new test file, link `SushiDSP::routing`)

**Interfaces:**
- Produces: `SushiDSP::SplitterNode` (class), `SushiDSP::splitter_descriptor()` (free function
  returning `const NodeDescriptor&`, id `"sushidsp.splitter"`).
- Consumes: `SushiDSP::IAudioNode`, `SushiDSP::NodeDescriptor`, `SushiDSP::PortLayout`,
  `SushiDSP::PrepareInfo`, `SushiDSP::ProcessContext` (all pre-existing `core` types).

`SplitterNode` copies its single input to every one of its `kMaxOutputs` declared outputs, but
since `IAudioNode::process()`'s `ProcessContext` only exposes `outputs[0]` for a `RigGraph` node
today (`RigGraph::process()` allocates exactly one `node_buffer` per node — see Task 3, which
did not change this), `SplitterNode`'s "N outputs" only matters for `ports()` reporting and for
how many downstream `connect()` calls a builder makes: `RigGraph`'s fan-out mechanism (Task 3)
already lets multiple downstream nodes read the same single buffer, so `SplitterNode` itself is
a plain pass-through — its `process()` copies `inputs[0]` to `outputs[0]`, and fan-out to
multiple consumers is handled entirely by `RigGraph::connect()`/`process()`, not by
`SplitterNode` writing multiple buffers. State this in the class doc comment so a future reader
does not "fix" it into something more complicated than it needs to be.

- [ ] **Step 1: Write the failing tests**

Create `tests/functional/unit/test_splitter_node.cpp`:

```cpp
/**************************************************************************/
/* test_splitter_node.cpp                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#include <memory>
#include <new>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/routing/SplitterNode.hpp"

TEST(Unit_Dsp, SplitterNodeCopiesInputToOutput)
{
    SushiDSP::SplitterNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });

    std::vector<float> input(64, 3.5f);
    std::vector<float> output(64, 0.0f);
    const float* inputs[1] = { input.data() };
    float* outputs[1] = { output.data() };
    SushiDSP::ProcessContext context{ outputs, inputs, 64, 0.0 };

    node.process(context);

    for (float sample : output)
    {
        EXPECT_FLOAT_EQ(sample, 3.5f);
    }
}

TEST(Unit_Dsp, SplitterNodeHasNoParameters)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::splitter_descriptor();
    EXPECT_STREQ(descriptor.id, "sushidsp.splitter");
    EXPECT_EQ(descriptor.parameter_count, 0u);
}

TEST(Unit_Dsp, SplitterNodeProcessPerformsNoHeapAllocation)
{
    SushiDSP::SplitterNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    std::vector<float> input(64, 1.0f);
    std::vector<float> output(64, 0.0f);
    const float* inputs[1] = { input.data() };
    float* outputs[1] = { output.data() };
    SushiDSP::ProcessContext context{ outputs, inputs, 64, 0.0 };
    node.process(context);   // warm up before the trap
    node.process(context);   // second call is what test_rt_safety.cpp's pattern traps on
    SUCCEED();
}
```

`SplitterNodeProcessPerformsNoHeapAllocation` here is a smoke test only (this file has no
access to `test_rt_safety.cpp`'s global `operator new` trap, which lives in that file's own
translation unit) — the real RT-safety trap assertion for `SplitterNode` is added to
`test_rt_safety.cpp` in Step 5 below.

- [ ] **Step 2: Register the test file and create the routing module skeleton**

In `tests/CMakeLists.txt`, add `functional/unit/test_splitter_node.cpp` to the `# --- Core ---`
group, immediately after `functional/unit/test_rig_graph.cpp`. Add `SushiDSP::routing` to
`sushidsp_tests`'s `target_link_libraries` call.

Create `modules/routing/CMakeLists.txt`:

```cmake
add_library(routing STATIC
    src/SplitterNode.cpp
)
add_library(SushiDSP::routing ALIAS routing)
target_link_libraries(routing PUBLIC SushiDSP::sushi_dsp)
sushidsp_apply_optimization_flags(routing)
target_include_directories(routing PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
```

In the root `CMakeLists.txt`, add `add_subdirectory(modules/routing)` immediately after the
existing `add_subdirectory(modules/node_registry)` line.

- [ ] **Step 3: Run tests to verify they fail**

Run: `sd test -s unit`. Expected: build failure — `SplitterNode.hpp` doesn't exist yet.

- [ ] **Step 4: Create SplitterNode.hpp and SplitterNode.cpp**

Create `modules/routing/include/SushiDSP/routing/SplitterNode.hpp`:

```cpp
/**************************************************************************/
/* SplitterNode.hpp                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#pragma once

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"

namespace SushiDSP
{
    /// @brief A routing pass-through: copies its single input to its single output. Fan-out to
    /// multiple downstream nodes is entirely RigGraph's job (a RigGraph node's output buffer can
    /// already be read by more than one downstream connect() edge) — this node exists only so a
    /// rig can name an explicit "split point" in the gear panel / preset graph, not because it
    /// duplicates the buffer itself.
    class SplitterNode : public IAudioNode
    {
    public:
        void prepare(const PrepareInfo& info) override;
        void process(ProcessContext& context) override;
        PortLayout ports() const override;
        void reset() override;
        float get_parameter(const char* id) const override;
        void set_parameter(const char* id, float value) override;
    };

    /// @brief The splitter's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.splitter".
    const NodeDescriptor& splitter_descriptor();
}
```

Create `modules/routing/src/SplitterNode.cpp`:

```cpp
/**************************************************************************/
/* SplitterNode.cpp                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#include "SushiDSP/routing/SplitterNode.hpp"

#include <memory>

namespace SushiDSP
{
    void SplitterNode::prepare(const PrepareInfo&) {}

    void SplitterNode::process(ProcessContext& context)
    {
        const float* input = context.inputs != nullptr ? context.inputs[0] : nullptr;
        float* output = context.outputs[0];
        for (std::uint32_t i = 0; i < context.frames; ++i)
        {
            output[i] = input != nullptr ? input[i] : 0.0f;
        }
    }

    PortLayout SplitterNode::ports() const { return { 1, 1 }; }

    void SplitterNode::reset() {}

    float SplitterNode::get_parameter(const char*) const { return 0.0f; }
    void SplitterNode::set_parameter(const char*, float) {}

    namespace
    {
        std::unique_ptr<IAudioNode> make_splitter(const PrepareInfo&)
        {
            return std::make_unique<SplitterNode>();
        }
    }

    const NodeDescriptor& splitter_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.splitter",
            "Splitter",
            nullptr,
            0,
            PortLayout{ 1, 1 },
            &make_splitter,
        };
        return descriptor;
    }
}
```

- [ ] **Step 5: Add the RT-safety trap test**

In `tests/functional/unit/test_rt_safety.cpp`, add `#include "SushiDSP/routing/SplitterNode.hpp"`
to the includes block (alphabetical position: after `"SushiDSP/math/StreamingConvolution.hpp"`
and before `"SushiDSP/jcm800/Jcm800.hpp"` — match this file's existing include ordering exactly
by reading it first), and add a new test following the file's established
`*ProcessPerformsNoHeapAllocation` pattern (model it on the nearby `TubeScreamerProcessPerformsNoHeapAllocation`
test — read that test's exact body first, since it wraps calls with
`g_trap_allocations.store(true, ...)` / `g_trapped_allocation_count` and this new test must
follow the identical structure):

```cpp
TEST(Unit_Dsp, SplitterNodeProcessPerformsNoHeapAllocation)
{
    SushiDSP::SplitterNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });

    std::vector<float> input(256, 1.0f);
    std::vector<float> output(256, 0.0f);
    const float* inputs[1] = { input.data() };
    float* outputs[1] = { output.data() };
    SushiDSP::ProcessContext context{ outputs, inputs, 256, 0.0 };

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);
    for (int block = 0; block < 100; ++block)
    {
        node.process(context);
    }
    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
}
```

If the exact global-state save/restore idiom in this file differs from the sketch above once you
read the neighboring test, follow the file's established idiom, not this plan text, and note the
deviation in your report.

- [ ] **Step 6: Run tests to verify they pass**

Run: `sd test -s unit`. Expected: `Unit_Dsp.SplitterNode*` all pass, including the new
`test_rt_safety.cpp` case.

- [ ] **Step 7: Commit**

```bash
git add modules/routing tests/functional/unit/test_splitter_node.cpp tests/functional/unit/test_rt_safety.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -m "routing: add SplitterNode pass-through and its RT-safety trap test"
```

---

### Task 6: `modules/routing` — `MixerNode`

**Files:**
- Modify: `modules/routing/CMakeLists.txt`
- Create: `modules/routing/include/SushiDSP/routing/MixerNode.hpp`
- Create: `modules/routing/src/MixerNode.cpp`
- Test: `tests/functional/unit/test_mixer_node.cpp`
- Modify: `tests/CMakeLists.txt` (register the new test file)
- Modify: `tests/functional/unit/test_rt_safety.cpp` (RT-safety trap test)

**Interfaces:**
- Produces: `SushiDSP::MixerNode` (class), `SushiDSP::mixer_descriptor()` (free function
  returning `const NodeDescriptor&`, id `"sushidsp.mixer"`), `MixerNode::kMaxFanIn` (public
  `static constexpr std::size_t`, value `SushiDSP::RigGraph::kMaxFanInOut` — i.e. `8`, matching
  Task 2's graph-level limit so the descriptor's declared 8 `gain_N` parameters always cover
  every input slot `RigGraph::connect()` could ever route to this node).
- Consumes: `SushiDSP::RigGraph::kMaxFanInOut` (Task 2), `SushiDSP::IAudioNode`,
  `SushiDSP::NodeDescriptor`, `SushiDSP::ParameterDescriptor`, `SushiDSP::ParamKind` (pre-existing
  `core` types).

- [ ] **Step 1: Write the failing tests**

Create `tests/functional/unit/test_mixer_node.cpp`:

```cpp
/**************************************************************************/
/* test_mixer_node.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/routing/MixerNode.hpp"

TEST(Unit_Dsp, MixerNodeSumsTwoInputsWithDefaultUnityGain)
{
    SushiDSP::MixerNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });

    std::vector<float> a(64, 2.0f);
    std::vector<float> b(64, 3.0f);
    std::vector<float> output(64, 0.0f);
    const float* inputs[2] = { a.data(), b.data() };
    float* outputs[1] = { output.data() };
    SushiDSP::ProcessContext context{ outputs, inputs, 64, 0.0 };

    node.process(context);

    for (float sample : output)
    {
        EXPECT_FLOAT_EQ(sample, 5.0f);
    }
}

TEST(Unit_Dsp, MixerNodeAppliesPerInputGain)
{
    SushiDSP::MixerNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    node.set_parameter("gain_0", 0.5f);
    node.set_parameter("gain_1", 2.0f);

    std::vector<float> a(64, 4.0f);
    std::vector<float> b(64, 1.0f);
    std::vector<float> output(64, 0.0f);
    const float* inputs[2] = { a.data(), b.data() };
    float* outputs[1] = { output.data() };
    SushiDSP::ProcessContext context{ outputs, inputs, 64, 0.0 };

    node.process(context);

    // 4.0*0.5 + 1.0*2.0 = 4.0
    for (float sample : output)
    {
        EXPECT_FLOAT_EQ(sample, 4.0f);
    }
}

TEST(Unit_Dsp, MixerNodeGetParameterRoundTripsEveryGain)
{
    SushiDSP::MixerNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    node.set_parameter("gain_3", 0.25f);
    EXPECT_FLOAT_EQ(node.get_parameter("gain_3"), 0.25f);
    EXPECT_FLOAT_EQ(node.get_parameter("gain_0"), 1.0f);   // untouched slots default to 1.0
}

TEST(Unit_Dsp, MixerNodeUnknownParameterIdIsSafeNoOp)
{
    SushiDSP::MixerNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    node.set_parameter("not_a_real_id", 0.5f);   // must not crash
    EXPECT_FLOAT_EQ(node.get_parameter("not_a_real_id"), 0.0f);
}

TEST(Unit_Dsp, MixerNodeDescriptorDeclaresEightGainParameters)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::mixer_descriptor();
    EXPECT_STREQ(descriptor.id, "sushidsp.mixer");
    ASSERT_EQ(descriptor.parameter_count, SushiDSP::MixerNode::kMaxFanIn);
    EXPECT_STREQ(descriptor.parameters[0].id, "gain_0");
    EXPECT_FLOAT_EQ(descriptor.parameters[0].default_value, 1.0f);
}
```

- [ ] **Step 2: Register the test file**

In `tests/CMakeLists.txt`, add `functional/unit/test_mixer_node.cpp` immediately after
`functional/unit/test_splitter_node.cpp` in the `# --- Core ---` group. In
`modules/routing/CMakeLists.txt`, add `src/MixerNode.cpp` to the `add_library(routing STATIC
...)` source list, and add `#include "SushiDSP/core/RigGraph.hpp"`-visibility by linking nothing
new (`routing` already links `SushiDSP::sushi_dsp`, which exports `RigGraph.hpp`'s
`kMaxFanInOut` publicly).

- [ ] **Step 3: Run tests to verify they fail**

Run: `sd test -s unit`. Expected: build failure — `MixerNode.hpp` doesn't exist yet.

- [ ] **Step 4: Create MixerNode.hpp and MixerNode.cpp**

Create `modules/routing/include/SushiDSP/routing/MixerNode.hpp`:

```cpp
/**************************************************************************/
/* MixerNode.hpp                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#pragma once

#include <array>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"
#include "SushiDSP/core/RigGraph.hpp"

namespace SushiDSP
{
    /// @brief Sums up to kMaxFanIn inputs, each scaled by its own gain_N parameter (default
    /// 1.0), into a single output. Plain scalar multiply-add — no crossfade curve, no panning;
    /// a more elaborate mixing law is future work if a real rig needs one.
    class MixerNode : public IAudioNode
    {
    public:
        /// @brief Matches RigGraph::kMaxFanInOut: a MixerNode can never be connect()-ed more
        /// inputs than the graph itself allows, so every possible input slot has a declared gain.
        static constexpr std::size_t kMaxFanIn = RigGraph::kMaxFanInOut;

        MixerNode();

        void prepare(const PrepareInfo& info) override;
        void process(ProcessContext& context) override;
        PortLayout ports() const override;
        void reset() override;
        float get_parameter(const char* id) const override;
        void set_parameter(const char* id, float value) override;
        void set_parameter(std::uint16_t index, float value) override;

    private:
        std::array<float, kMaxFanIn> gains_;
    };

    /// @brief The mixer's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.mixer".
    const NodeDescriptor& mixer_descriptor();
}
```

Create `modules/routing/src/MixerNode.cpp`:

```cpp
/**************************************************************************/
/* MixerNode.cpp                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#include "SushiDSP/routing/MixerNode.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>

namespace SushiDSP
{
    namespace
    {
        constexpr std::uint16_t kUnknownParameterIndex = 0xFFFF;

        std::uint16_t parameter_index_for(const char* id)
        {
            for (std::uint16_t i = 0; i < static_cast<std::uint16_t>(MixerNode::kMaxFanIn); ++i)
            {
                char expected[16];
                std::snprintf(expected, sizeof(expected), "gain_%u", static_cast<unsigned>(i));
                if (std::strcmp(id, expected) == 0)
                {
                    return i;
                }
            }
            return kUnknownParameterIndex;
        }
    }

    MixerNode::MixerNode()
    {
        gains_.fill(1.0f);
    }

    void MixerNode::prepare(const PrepareInfo&) {}

    void MixerNode::process(ProcessContext& context)
    {
        float* output = context.outputs[0];
        std::fill(output, output + context.frames, 0.0f);

        if (context.inputs == nullptr)
        {
            return;
        }

        for (std::size_t input_slot = 0; input_slot < kMaxFanIn; ++input_slot)
        {
            const float* input = context.inputs[input_slot];
            if (input == nullptr)
            {
                continue;
            }
            const float gain = gains_[input_slot];
            for (std::uint32_t i = 0; i < context.frames; ++i)
            {
                output[i] += input[i] * gain;
            }
        }
    }

    PortLayout MixerNode::ports() const { return { static_cast<std::uint32_t>(kMaxFanIn), 1 }; }

    void MixerNode::reset() {}

    float MixerNode::get_parameter(const char* id) const
    {
        const std::uint16_t index = parameter_index_for(id);
        return index == kUnknownParameterIndex ? 0.0f : gains_[index];
    }

    void MixerNode::set_parameter(const char* id, float value)
    {
        set_parameter(parameter_index_for(id), value);
    }

    void MixerNode::set_parameter(std::uint16_t index, float value)
    {
        if (index < kMaxFanIn)
        {
            gains_[index] = value;
        }
    }

    namespace
    {
        constexpr ParameterDescriptor kMixerParameters[] = {
            { "gain_0", "Gain 1", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
            { "gain_1", "Gain 2", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
            { "gain_2", "Gain 3", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
            { "gain_3", "Gain 4", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
            { "gain_4", "Gain 5", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
            { "gain_5", "Gain 6", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
            { "gain_6", "Gain 7", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
            { "gain_7", "Gain 8", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
        };
        static_assert(sizeof(kMixerParameters) / sizeof(kMixerParameters[0]) == MixerNode::kMaxFanIn,
            "kMixerParameters must declare exactly MixerNode::kMaxFanIn entries");

        std::unique_ptr<IAudioNode> make_mixer(const PrepareInfo&)
        {
            return std::make_unique<MixerNode>();
        }
    }

    const NodeDescriptor& mixer_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.mixer",
            "Mixer",
            kMixerParameters,
            MixerNode::kMaxFanIn,
            PortLayout{ static_cast<std::uint32_t>(MixerNode::kMaxFanIn), 1 },
            &make_mixer,
        };
        return descriptor;
    }
}
```

- [ ] **Step 5: Add the RT-safety trap test**

In `tests/functional/unit/test_rt_safety.cpp`, add `#include "SushiDSP/routing/MixerNode.hpp"`
next to the `SplitterNode.hpp` include added in Task 5, and add (following the same established
save/restore idiom as Task 5's `SplitterNodeProcessPerformsNoHeapAllocation`):

```cpp
TEST(Unit_Dsp, MixerNodeProcessPerformsNoHeapAllocation)
{
    SushiDSP::MixerNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });

    std::vector<float> a(256, 1.0f);
    std::vector<float> b(256, 1.0f);
    std::vector<float> output(256, 0.0f);
    const float* inputs[2] = { a.data(), b.data() };
    float* outputs[1] = { output.data() };
    SushiDSP::ProcessContext context{ outputs, inputs, 256, 0.0 };

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);
    for (int block = 0; block < 100; ++block)
    {
        node.process(context);
    }
    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
}
```

- [ ] **Step 6: Run tests to verify they pass**

Run: `sd test -s unit`. Expected: all `Unit_Dsp.MixerNode*` tests pass.

- [ ] **Step 7: Commit**

```bash
git add modules/routing tests/functional/unit/test_mixer_node.cpp tests/functional/unit/test_rt_safety.cpp tests/CMakeLists.txt
git commit -m "routing: add MixerNode with per-input gain and its RT-safety trap test"
```

---

### Task 7: `node_registry` registration and end-to-end branching integration test

**Files:**
- Modify: `modules/node_registry/CMakeLists.txt`
- Modify: `modules/node_registry/src/NodeRegistry.cpp`
- Test: `tests/functional/unit/test_node_registry.cpp` (append)
- Test: `tests/functional/unit/test_rig_graph.cpp` (append — full split/mix integration test)

**Interfaces:**
- Consumes: `SushiDSP::splitter_descriptor()`, `SushiDSP::mixer_descriptor()` (Tasks 5/6).
- Produces: `node_registry()` now includes both under category `"Routing"`;
  `make_node("sushidsp.splitter", info)` / `make_node("sushidsp.mixer", info)` construct them.

- [ ] **Step 1: Write the failing test**

Append to `tests/functional/unit/test_node_registry.cpp` (read its current top-of-file structure
first — it already tests `make_node` for `"sushidsp.tubescreamer"`/`"sushidsp.jcm800"`, follow
that exact pattern):

```cpp
TEST(Unit_Dsp, NodeRegistryIncludesRoutingNodes)
{
    const SushiDSP::PrepareInfo info{ 48000.0, 64, 1 };

    auto splitter = SushiDSP::make_node("sushidsp.splitter", info);
    ASSERT_NE(splitter, nullptr);
    EXPECT_EQ(splitter->ports().output_channels, 1u);

    auto mixer = SushiDSP::make_node("sushidsp.mixer", info);
    ASSERT_NE(mixer, nullptr);
    EXPECT_EQ(mixer->ports().input_channels, SushiDSP::MixerNode::kMaxFanIn);

    bool found_routing_category = false;
    for (const auto& entry : SushiDSP::node_registry())
    {
        if (std::string(entry.category.name) == "Routing")
        {
            found_routing_category = true;
        }
    }
    EXPECT_TRUE(found_routing_category);
}
```

Add `#include "SushiDSP/routing/MixerNode.hpp"` and `#include <string>` to this test file's
includes if not already present.

- [ ] **Step 2: Run test to verify it fails**

Run: `sd test -s unit`. Expected: FAIL — `make_node` returns `nullptr` for both ids (not
registered yet), and linking fails until `modules/node_registry` depends on `SushiDSP::routing`.

- [ ] **Step 3: Register the routing nodes**

In `modules/node_registry/CMakeLists.txt`, add `SushiDSP::routing` to `target_link_libraries`.

In `modules/node_registry/src/NodeRegistry.cpp`, add
`#include "SushiDSP/routing/MixerNode.hpp"` and `#include "SushiDSP/routing/SplitterNode.hpp"`
to the includes, and add both entries to the `entries` table:

```cpp
        static const std::vector<NodeEntry> entries{
            { &tube_screamer_descriptor(), NodeCategory{ "Distortion" } },
            { &jcm800_descriptor(), NodeCategory{ "Amp" } },
            { &splitter_descriptor(), NodeCategory{ "Routing" } },
            { &mixer_descriptor(), NodeCategory{ "Routing" } },
        };
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `sd test -s unit`. Expected: `NodeRegistryIncludesRoutingNodes` passes.

- [ ] **Step 5: Write and pass the end-to-end split/mix integration test**

Append to `tests/functional/unit/test_rig_graph.cpp`:

```cpp
TEST(Unit_Dsp, RigGraphSplitterAndMixerRoundTripThroughTwoEffectBranches)
{
    SushiDSP::RigGraph graph(256, 6);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    auto splitter = SushiDSP::make_node("sushidsp.splitter", SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
    auto splitter_handle = graph.add_node(std::move(splitter));
    auto branch_a = graph.add_node(std::make_unique<GainNode>(2.0f));
    auto branch_b = graph.add_node(std::make_unique<GainNode>(3.0f));
    auto mixer = SushiDSP::make_node("sushidsp.mixer", SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
    auto mixer_handle = graph.add_node(std::move(mixer));

    graph.connect(source, splitter_handle);
    graph.connect(splitter_handle, branch_a);
    graph.connect(splitter_handle, branch_b);
    graph.connect(branch_a, mixer_handle);
    graph.connect(branch_b, mixer_handle);

    graph.node(mixer_handle)->set_parameter("gain_0", 0.5f);
    graph.node(mixer_handle)->set_parameter("gain_1", 1.0f);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = { output_buffer.data() };
    SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };
    graph.process(context);

    // source(1.0) -split-> branch_a(*2=2.0), branch_b(*3=3.0) -> mixer(2.0*0.5 + 3.0*1.0 = 4.0)
    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 4.0f);
    }
}
```

Add `#include "SushiDSP/node_registry/NodeRegistry.hpp"` to `test_rig_graph.cpp`'s includes.
Link `SushiDSP::node_registry` into `sushidsp_tests` if it is not already linked (it already is,
per `tests/CMakeLists.txt`'s existing `target_link_libraries` block).

Run: `sd test -s unit`. Expected: passes, proving Tasks 1-7 compose correctly end to end.

- [ ] **Step 6: Commit**

```bash
git add modules/node_registry tests/functional/unit/test_node_registry.cpp tests/functional/unit/test_rig_graph.cpp
git commit -m "node_registry: register SplitterNode/MixerNode; add end-to-end branching test"
```

---

### Task 8: `IAudioNode::type_id()` — self-reporting type id

**Files:**
- Modify: `include/SushiDSP/core/IAudioNode.hpp`
- Modify: `modules/tubescreamer/include/SushiDSP/tubescreamer/TubeScreamer.hpp`,
  `modules/tubescreamer/src/TubeScreamer.cpp`
- Modify: `modules/jcm800/include/SushiDSP/jcm800/Jcm800.hpp`,
  `modules/jcm800/src/Jcm800.cpp`
- Modify: `modules/routing/include/SushiDSP/routing/SplitterNode.hpp`,
  `modules/routing/src/SplitterNode.cpp`
- Modify: `modules/routing/include/SushiDSP/routing/MixerNode.hpp`,
  `modules/routing/src/MixerNode.cpp`
- Test: `tests/functional/unit/test_rig_graph.cpp` (append)

**Interfaces:**
- Produces: `IAudioNode::type_id() const -> const char*`, a new virtual with default
  implementation returning `""`. `TubeScreamer::type_id()`, `Jcm800::type_id()`,
  `SplitterNode::type_id()`, `MixerNode::type_id()` each override it to return their own
  `NodeDescriptor::id` string (`"sushidsp.tubescreamer"`, `"sushidsp.jcm800"`,
  `"sushidsp.splitter"`, `"sushidsp.mixer"`).
- Consumes: nothing new — every one of these types already has a `_descriptor()` free function
  whose `id` field is the exact string to return.

This is a small, mechanical, four-file addition chosen (per user decision) over an
`add_node()`-parameter alternative: a node already knows its own registration id via its
descriptor, so it is more broadly correct for it to self-report that id than for `RigGraph` to
carry a side-table of type strings.

- [ ] **Step 1: Write the failing test**

Append to `tests/functional/unit/test_rig_graph.cpp`:

```cpp
TEST(Unit_Dsp, GainNodeTypeIdDefaultsToEmptyString)
{
    GainNode node(1.0f);
    EXPECT_STREQ(node.type_id(), "");
}
```

(`GainNode` is this file's own test fixture, not a registered product — it exercises the base
class's default, proving the new virtual doesn't force every existing IAudioNode subclass to
implement it.)

- [ ] **Step 2: Run test to verify it fails**

Run: `sd test -s unit`. Expected: build failure — `type_id()` does not exist yet.

- [ ] **Step 3: Add the virtual to IAudioNode**

In `include/SushiDSP/core/IAudioNode.hpp`, add after `latency_samples()`:

```cpp
        /// @brief This node's NodeDescriptor::id, for a node registered in a node_registry.
        /// @return The registered type id, or "" for a node with no registry entry (e.g. a
        /// test fixture).
        virtual const char* type_id() const { return ""; }
```

- [ ] **Step 4: Override it in TubeScreamer, Jcm800, SplitterNode, MixerNode**

In `modules/tubescreamer/include/SushiDSP/tubescreamer/TubeScreamer.hpp`, add to the public
section (after `latency_samples()`'s declaration):

```cpp
        /// @brief This pedal's registered type id.
        /// @return "sushidsp.tubescreamer".
        const char* type_id() const override;
```

In `modules/tubescreamer/src/TubeScreamer.cpp`, add near the other member-function definitions:

```cpp
    const char* TubeScreamer::type_id() const { return "sushidsp.tubescreamer"; }
```

Repeat the identical pattern for `Jcm800` (`modules/jcm800/include/SushiDSP/jcm800/Jcm800.hpp` /
`modules/jcm800/src/Jcm800.cpp`, returning `"sushidsp.jcm800"` — read the existing file first to
place the declaration/definition consistently with its existing member ordering), for
`SplitterNode` (returning `"sushidsp.splitter"`), and for `MixerNode` (returning
`"sushidsp.mixer"`).

- [ ] **Step 5: Run tests to verify they pass**

Run: `sd test -s unit`. Expected: the new test passes; every pre-existing test still passes
(adding a defaulted virtual is source- and binary-compatible with every existing override site).

- [ ] **Step 6: Commit**

```bash
git add include/SushiDSP/core/IAudioNode.hpp modules/tubescreamer modules/jcm800 modules/routing tests/functional/unit/test_rig_graph.cpp
git commit -m "core: add IAudioNode::type_id() so a node can self-report its registered id"
```

---

### Task 9: `RigGraph::upstream_of()` — expose edges for serialization

**Files:**
- Modify: `include/SushiDSP/core/RigGraph.hpp`
- Modify: `src/core/RigGraph.cpp`
- Test: `tests/functional/unit/test_rig_graph.cpp` (append)

**Interfaces:**
- Produces: `RigGraph::upstream_of(NodeHandle handle) const -> std::vector<NodeHandle>` — the
  handles of every node with an edge into `handle`, in `connect()` call order. Not real-time
  safe (allocates a `std::vector`); matches the existing `nodes()` accessor's contract (prepare
  side only).
- Consumes: `edges_` (existing private member; no new dependency).

- [ ] **Step 1: Write the failing test**

Append to `tests/functional/unit/test_rig_graph.cpp`:

```cpp
TEST(Unit_Dsp, RigGraphUpstreamOfListsEdgesInConnectOrder)
{
    SushiDSP::RigGraph graph(256, 4);
    auto a = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    auto b = graph.add_node(std::make_unique<ConstantSourceNode>(2.0f));
    auto sink = graph.add_node(std::make_unique<SumTwoInputsNode>());
    graph.connect(a, sink);
    graph.connect(b, sink);

    const std::vector<SushiDSP::NodeHandle> upstream = graph.upstream_of(sink);
    ASSERT_EQ(upstream.size(), 2u);
    EXPECT_EQ(upstream[0], a);
    EXPECT_EQ(upstream[1], b);
    EXPECT_TRUE(graph.upstream_of(a).empty());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `sd test -s unit`. Expected: build failure — `upstream_of()` does not exist yet.

- [ ] **Step 3: Declare and implement upstream_of()**

In `include/SushiDSP/core/RigGraph.hpp`, add to the public section, after `nodes()`:

```cpp
        /// @brief Lists the handles of every node with an edge into handle, in connect() call
        /// order. Not real-time safe; call only off the processing thread.
        /// @param handle The downstream node to query.
        /// @return The upstream handles, or an empty vector if handle has no upstream edge.
        std::vector<NodeHandle> upstream_of(NodeHandle handle) const;
```

In `src/core/RigGraph.cpp`, add:

```cpp
    std::vector<NodeHandle> RigGraph::upstream_of(NodeHandle handle) const
    {
        std::vector<NodeHandle> result;
        for (const Edge& edge : edges_)
        {
            if (edge.to == handle)
            {
                result.push_back(edge.from);
            }
        }
        return result;
    }
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `sd test -s unit`. Expected: the new test passes.

- [ ] **Step 5: Commit**

```bash
git add include/SushiDSP/core/RigGraph.hpp src/core/RigGraph.cpp tests/functional/unit/test_rig_graph.cpp
git commit -m "core: add RigGraph::upstream_of() to expose edges for serialization"
```

---

### Task 10: `modules/rig_preset` — DAG JSON serialization

**Files:**
- Create: `modules/rig_preset/CMakeLists.txt`
- Create: `modules/rig_preset/include/SushiDSP/rig_preset/RigPreset.hpp`
- Create: `modules/rig_preset/src/RigPreset.cpp`
- Modify: `CMakeLists.txt` (add `add_subdirectory(modules/rig_preset)`)
- Test: `tests/functional/unit/test_rig_preset.cpp`
- Modify: `tests/CMakeLists.txt` (register the new test file, link `SushiDSP::rig_preset`)

**Interfaces:**
- Produces: `SushiDSP::serialize_rig_to_json(const RigGraph&) -> std::string`,
  `SushiDSP::deserialize_rig_from_json(RigGraph&, const std::string&)`. Both are free functions,
  not tied to `apps/host_gui`'s `RigController` — deliberately independent, per this spec's
  non-goal that `apps/host_gui` is untouched. A future GUI spec that wants to expose DAG presets
  in the UI would call these directly instead of duplicating the schema logic.
- Consumes: `SushiDSP::RigGraph` (`nodes()`, `node()`, `add_node()`, `connect()`, `compile()`,
  `upstream_of()` — the last from Task 9), `SushiDSP::IAudioNode::type_id()` (Task 8),
  `SushiDSP::node_registry()` / `SushiDSP::make_node()` (for looking up a node's registered
  `type` id's descriptor when saving, and for constructing nodes by `type` when loading).

This module depends on `nlohmann::json`, already fetched unconditionally by the root
`CMakeLists.txt` (see the comment there: "nlohmann::json is a test dependency ... fetched
regardless of SUSHIDSP_BUILD_GUI").

- [ ] **Step 1: Write the failing tests**

Create `tests/functional/unit/test_rig_preset.cpp`:

```cpp
/**************************************************************************/
/* test_rig_preset.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#include <memory>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/node_registry/NodeRegistry.hpp"
#include "SushiDSP/rig_preset/RigPreset.hpp"

TEST(Unit_Dsp, SerializeRigToJsonProducesOneEntryPerNodeWithInputs)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    SushiDSP::RigGraph graph(256, 4);
    auto ts = graph.add_node(SushiDSP::make_node("sushidsp.tubescreamer", info));
    auto amp = graph.add_node(SushiDSP::make_node("sushidsp.jcm800", info));
    graph.connect(ts, amp);
    graph.compile(info);

    const std::string json_text = SushiDSP::serialize_rig_to_json(graph);
    const nlohmann::json root = nlohmann::json::parse(json_text);

    ASSERT_EQ(root.at("nodes").size(), 2u);
    bool found_amp_with_one_input = false;
    for (const auto& node_json : root.at("nodes"))
    {
        if (node_json.at("type").get<std::string>() == "sushidsp.jcm800")
        {
            ASSERT_EQ(node_json.at("inputs").size(), 1u);
            found_amp_with_one_input = true;
        }
    }
    EXPECT_TRUE(found_amp_with_one_input);
}

TEST(Unit_Dsp, DeserializeRigFromJsonRebuildsBranchingTopologyAndParameters)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };

    const std::string json_text = R"({
        "nodes": [
            { "id": 0, "type": "sushidsp.tubescreamer", "params": { "drive": 0.75 }, "inputs": [] },
            { "id": 1, "type": "sushidsp.splitter", "params": {}, "inputs": [0] },
            { "id": 2, "type": "sushidsp.jcm800", "params": {}, "inputs": [1] },
            { "id": 3, "type": "sushidsp.tubescreamer", "params": { "drive": 0.1 }, "inputs": [1] },
            { "id": 4, "type": "sushidsp.mixer", "params": { "gain_0": 0.5, "gain_1": 0.5 }, "inputs": [2, 3] }
        ]
    })";

    SushiDSP::RigGraph graph(256, 8);
    SushiDSP::deserialize_rig_from_json(graph, json_text);
    graph.compile(info);

    const auto handles = graph.nodes();
    ASSERT_EQ(handles.size(), 5u);

    bool found_drive_075 = false;
    bool found_drive_010 = false;
    for (SushiDSP::NodeHandle handle : handles)
    {
        SushiDSP::IAudioNode* node = graph.node(handle);
        const float drive = node->get_parameter("drive");
        if (std::abs(drive - 0.75f) < 1e-6f) { found_drive_075 = true; }
        if (std::abs(drive - 0.1f) < 1e-6f) { found_drive_010 = true; }
    }
    EXPECT_TRUE(found_drive_075);
    EXPECT_TRUE(found_drive_010);
}

TEST(Unit_Dsp, SerializeThenDeserializeRoundTripsABranchingGraph)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    SushiDSP::RigGraph original(256, 6);
    auto source_ts = original.add_node(SushiDSP::make_node("sushidsp.tubescreamer", info));
    original.node(source_ts)->set_parameter("drive", 0.9f);
    auto splitter = original.add_node(SushiDSP::make_node("sushidsp.splitter", info));
    auto amp = original.add_node(SushiDSP::make_node("sushidsp.jcm800", info));
    auto dry = original.add_node(SushiDSP::make_node("sushidsp.tubescreamer", info));
    auto mixer = original.add_node(SushiDSP::make_node("sushidsp.mixer", info));
    original.connect(source_ts, splitter);
    original.connect(splitter, amp);
    original.connect(splitter, dry);
    original.connect(amp, mixer);
    original.connect(dry, mixer);
    original.node(mixer)->set_parameter("gain_0", 0.3f);
    original.node(mixer)->set_parameter("gain_1", 0.7f);
    original.compile(info);

    const std::string json_text = SushiDSP::serialize_rig_to_json(original);

    SushiDSP::RigGraph rebuilt(256, 6);
    SushiDSP::deserialize_rig_from_json(rebuilt, json_text);
    rebuilt.compile(info);

    ASSERT_EQ(rebuilt.nodes().size(), original.nodes().size());
    bool found_mixer_with_gains = false;
    for (SushiDSP::NodeHandle handle : rebuilt.nodes())
    {
        SushiDSP::IAudioNode* node = rebuilt.node(handle);
        if (std::abs(node->get_parameter("gain_0") - 0.3f) < 1e-6f
            && std::abs(node->get_parameter("gain_1") - 0.7f) < 1e-6f)
        {
            found_mixer_with_gains = true;
        }
    }
    EXPECT_TRUE(found_mixer_with_gains);
}
```

- [ ] **Step 2: Register the test file and create the rig_preset module skeleton**

In `tests/CMakeLists.txt`, add `functional/unit/test_rig_preset.cpp` to the `# --- Core ---`
group, after `functional/unit/test_mixer_node.cpp`. Add `SushiDSP::rig_preset` to
`sushidsp_tests`'s `target_link_libraries`.

Create `modules/rig_preset/CMakeLists.txt`:

```cmake
add_library(rig_preset STATIC
    src/RigPreset.cpp
)
add_library(SushiDSP::rig_preset ALIAS rig_preset)
target_link_libraries(rig_preset PUBLIC SushiDSP::sushi_dsp SushiDSP::node_registry PRIVATE nlohmann_json::nlohmann_json)
sushidsp_apply_optimization_flags(rig_preset)
target_include_directories(rig_preset PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
```

In the root `CMakeLists.txt`, add `add_subdirectory(modules/rig_preset)` immediately after
`add_subdirectory(modules/routing)` (added in Task 5) — this must come after the
`FetchContent_MakeAvailable(json)` call earlier in the same file, so move it to just before the
`enable_testing()` / `add_subdirectory(tests)` lines at the file's end rather than up in the
"Products" block with the other `modules/` subdirectories (`node_registry`/`routing` have no
`nlohmann::json` dependency and can stay where they are; `rig_preset` needs the
`nlohmann_json::nlohmann_json` target to already exist).

- [ ] **Step 3: Run tests to verify they fail**

Run: `sd test -s unit`. Expected: build failure — `RigPreset.hpp` doesn't exist yet.

- [ ] **Step 4: Create RigPreset.hpp**

Create `modules/rig_preset/include/SushiDSP/rig_preset/RigPreset.hpp`:

```cpp
/**************************************************************************/
/* RigPreset.hpp                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#pragma once

#include <string>

#include "SushiDSP/core/RigGraph.hpp"

namespace SushiDSP
{
    /// @brief Serializes a compiled RigGraph's topology and every declared parameter to a JSON
    /// string: `{"nodes": [{"id", "type", "params", "inputs"}, ...]}`, where each node's
    /// `inputs` lists the preset-local `id`s of its upstream nodes in connect() order.
    /// @param graph The graph to serialize. Must have been compile()'d (uses node_registry() to
    /// look up each live node's declared parameters by matching NodeDescriptor::id).
    /// @return The JSON text, pretty-printed.
    /// @throws std::runtime_error if a live node's type is not found in node_registry() (should
    /// not happen for any node constructed via make_node(), but a hand-built RigGraph using a
    /// non-registered IAudioNode subclass cannot be serialized).
    std::string serialize_rig_to_json(const RigGraph& graph);

    /// @brief Parses a JSON string produced by serialize_rig_to_json() (or hand-written to the
    /// same schema) and rebuilds it into graph via add_node()/connect() — does not call
    /// compile(); the caller compiles afterward once, the same as any other graph edit.
    /// @param graph An empty or existing RigGraph to add the parsed nodes/edges into.
    /// @param json_text The JSON text to parse.
    /// @throws std::runtime_error if the JSON is malformed, or references a `type` not found in
    /// node_registry().
    void deserialize_rig_from_json(RigGraph& graph, const std::string& json_text);
}
```

- [ ] **Step 5: Create RigPreset.cpp**

Create `modules/rig_preset/src/RigPreset.cpp`:

```cpp
/**************************************************************************/
/* RigPreset.cpp                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#include "SushiDSP/rig_preset/RigPreset.hpp"

#include <cstring>
#include <map>
#include <stdexcept>
#include <vector>

#include <nlohmann/json.hpp>

#include "SushiDSP/node_registry/NodeRegistry.hpp"

namespace SushiDSP
{
    namespace
    {
        const NodeDescriptor* descriptor_for_type(const char* type)
        {
            for (const NodeEntry& entry : node_registry())
            {
                if (std::strcmp(entry.descriptor->id, type) == 0)
                {
                    return entry.descriptor;
                }
            }
            return nullptr;
        }
    }

    std::string serialize_rig_to_json(const RigGraph& graph)
    {
        nlohmann::json root;
        root["nodes"] = nlohmann::json::array();

        const std::vector<NodeHandle> handles = graph.nodes();

        for (NodeHandle handle : handles)
        {
            IAudioNode* node = graph.node(handle);
            const char* type = node->type_id();
            const NodeDescriptor* descriptor = descriptor_for_type(type);
            if (descriptor == nullptr)
            {
                throw std::runtime_error(
                    "serialize_rig_to_json: node type_id() '" + std::string(type)
                    + "' is not registered in node_registry()");
            }

            nlohmann::json node_json;
            node_json["id"] = handle;
            node_json["type"] = type;

            nlohmann::json params_json = nlohmann::json::object();
            for (std::size_t p = 0; p < descriptor->parameter_count; ++p)
            {
                const char* param_id = descriptor->parameters[p].id;
                params_json[param_id] = node->get_parameter(param_id);
            }
            node_json["params"] = params_json;

            nlohmann::json inputs_json = nlohmann::json::array();
            for (NodeHandle upstream : graph.upstream_of(handle))
            {
                inputs_json.push_back(upstream);
            }
            node_json["inputs"] = inputs_json;

            root["nodes"].push_back(node_json);
        }

        return root.dump(2);
    }

    void deserialize_rig_from_json(RigGraph& graph, const std::string& json_text)
    {
        const nlohmann::json root = nlohmann::json::parse(json_text);

        std::map<int, NodeHandle> preset_id_to_handle;
        std::map<NodeHandle, std::vector<int>> pending_inputs;
        std::map<NodeHandle, std::map<std::string, float>> pending_params;

        for (const auto& node_json : root.at("nodes"))
        {
            const int preset_id = node_json.at("id").get<int>();
            const std::string type = node_json.at("type").get<std::string>();

            const NodeDescriptor* descriptor = descriptor_for_type(type.c_str());
            if (descriptor == nullptr)
            {
                throw std::runtime_error("deserialize_rig_from_json: unknown node type '" + type + "'");
            }

            const PrepareInfo dummy_info{ 48000.0, 256, 1 };
            NodeHandle handle = graph.add_node(descriptor->make(dummy_info));
            preset_id_to_handle[preset_id] = handle;

            std::map<std::string, float> params;
            for (const auto& [key, value] : node_json.at("params").items())
            {
                params[key] = value.get<float>();
            }
            pending_params[handle] = std::move(params);

            std::vector<int> inputs;
            for (const auto& input_id : node_json.at("inputs"))
            {
                inputs.push_back(input_id.get<int>());
            }
            pending_inputs[handle] = std::move(inputs);
        }

        for (const auto& [handle, params] : pending_params)
        {
            IAudioNode* node = graph.node(handle);
            for (const auto& [key, value] : params)
            {
                node->set_parameter(key.c_str(), value);
            }
        }

        for (const auto& [handle, input_preset_ids] : pending_inputs)
        {
            for (int input_preset_id : input_preset_ids)
            {
                graph.connect(preset_id_to_handle.at(input_preset_id), handle);
            }
        }
    }
}
```

- [ ] **Step 6: Run tests to verify they pass**

Run: `sd test -s unit`. Expected: all `Unit_Dsp.SerializeRigToJson*` /
`Unit_Dsp.DeserializeRigFromJson*` / `Unit_Dsp.SerializeThenDeserialize*` tests pass.

- [ ] **Step 7: Commit**

```bash
git add modules/rig_preset tests/functional/unit/test_rig_preset.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -m "rig_preset: add DAG-capable JSON serialize/deserialize, independent of apps/host_gui"
```

---

## Final Review

After Task 10, run `sd test -s unit` once more for the whole suite (not just the new files) to
confirm no regression in `apps/host_gui`'s existing `test_rig_controller.cpp`/`test_preset.cpp`
(they must be completely unaffected — this plan never touches `apps/host_gui/`). Then re-read
`docs/superpowers/specs/2026-08-26-riggraph-dag-design.md` once more against what was actually
built and confirm every "Goals" bullet has a corresponding passing test, and report any deferred
or deviated item explicitly (per CLAUDE.md's honesty requirement) rather than silently.
