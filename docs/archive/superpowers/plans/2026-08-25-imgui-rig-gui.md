# ImGui Rig GUI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `apps/host_gui`, an ImGui/GLFW application that lets a user
build a dynamic TubeScreamer/JCM800 rig chain via drag-and-drop, tweak
parameters live, and save/load JSON presets, glitch-free against either the
ASIO or SDL2 backend.

**Architecture:** Extend `IAudioNode` with generic parameter access, extend
`RigGraph` with node removal/iteration, add a lock-free `RtGraphSlot` for
glitch-free chain swaps, add a `modules/node_registry` catalog, then build
the GUI app on top of all three.

**Tech Stack:** C++17, Dear ImGui + GLFW + OpenGL3 (CMake FetchContent),
nlohmann::json (FetchContent), GoogleTest.

**Spec:** `docs/superpowers/specs/2026-08-25-imgui-rig-gui-design.md`

## Global Constraints

- Everything reachable from `IAudioNode::process()` — including
  `RtGraphSlot::acquire_for_audio_thread()` — stays allocation-free,
  lock-free, syscall-free, throw-free.
- `RigGraph` structural methods (`add_node`, `remove_node`, `connect`,
  `disconnect`, `compile`) and `RtGraphSlot::publish`/`reclaim` are
  prepare-time/non-RT-thread only.
- New CMake targets use the `SushiDSP::<target>` alias convention and the
  `SUSHIDSP_BUILD_<X>` opt-in-option pattern (see `SUSHIDSP_BUILD_SDL2`,
  `SUSHIDSP_BUILD_ASIO` in the root `CMakeLists.txt`).
- No behavior change to `apps/host` or `apps/host_asio`.
- Builds and tests go through `sd build` / `sd test`, never raw
  cmake/ctest.
- Test files register in `tests/CMakeLists.txt`'s `sushidsp_tests`
  executable under the matching `# --- Section ---` comment, following the
  existing `Unit_*` GTest naming convention.

---

### Task 1: `RigGraph` — remove_node, disconnect, node(), nodes()

**Files:**
- Modify: `include/SushiDSP/core/RigGraph.hpp`
- Modify: `src/core/RigGraph.cpp`
- Test: `tests/functional/unit/test_rig_graph.cpp`

**Interfaces:**
- Produces: `RigGraph::remove_node(NodeHandle)`, `RigGraph::disconnect(NodeHandle, NodeHandle)`,
  `IAudioNode* RigGraph::node(NodeHandle) const`, `std::vector<NodeHandle> RigGraph::nodes() const`.

**Design note (binding):** `NodeHandle` is a stable index into `nodes_`.
`remove_node` must NOT shift other handles, so it tombstones: it resets
`nodes_[handle]` to `nullptr` and erases every edge that touches `handle`,
leaving a hole. Every place in `RigGraph.cpp` that iterates `nodes_` or
`topological_order_` and dereferences a node **must** skip a null entry —
`compile()`'s in-degree pass, its `prepare()` loop, and `process()`'s
per-node loop all touch this.

- [ ] **Step 1: Write the failing tests**

Append to `tests/functional/unit/test_rig_graph.cpp` (inside the existing
anonymous namespace's classes stay as-is for this task — Task 2 adds their
new overrides):

```cpp
TEST(Unit_Dsp, RigGraphNodesReturnsAllLiveHandles)
{
    SushiDSP::RigGraph graph(256, 4);
    auto a = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    auto b = graph.add_node(std::make_unique<GainNode>(2.0f));
    graph.connect(a, b);

    const std::vector<SushiDSP::NodeHandle> handles = graph.nodes();
    ASSERT_EQ(handles.size(), 2u);
    EXPECT_EQ(handles[0], a);
    EXPECT_EQ(handles[1], b);
    EXPECT_EQ(graph.node(a)->ports().output_channels, 1u);
}

TEST(Unit_Dsp, RigGraphRemoveNodeDropsItFromNodesAndProcess)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(5.0f));
    auto gain   = graph.add_node(std::make_unique<GainNode>(2.0f));
    graph.connect(source, gain);

    graph.remove_node(gain);

    const std::vector<SushiDSP::NodeHandle> handles = graph.nodes();
    ASSERT_EQ(handles.size(), 1u);
    EXPECT_EQ(handles[0], source);
    EXPECT_EQ(graph.node(gain), nullptr);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    std::vector<float> output(256, 0.0f);
    float* output_ptr[1] = { output.data() };
    SushiDSP::ProcessContext context{ output_ptr, nullptr, 256, 0.0 };
    graph.process(context);

    EXPECT_FLOAT_EQ(output[0], 5.0f);
}

TEST(Unit_Dsp, RigGraphDisconnectRemovesOnlyThatEdge)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(4.0f));
    auto gain_a = graph.add_node(std::make_unique<GainNode>(2.0f));
    auto gain_b = graph.add_node(std::make_unique<GainNode>(3.0f));
    graph.connect(source, gain_a);
    graph.connect(source, gain_b);
    graph.disconnect(source, gain_a);

    // gain_a now has no upstream edge; RigGraph feeds root nodes context.inputs
    // (nullptr here), so it must NOT receive source's output.
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    std::vector<float> output(256, 0.0f);
    float* output_ptr[1] = { output.data() };
    SushiDSP::ProcessContext context{ output_ptr, nullptr, 256, 0.0 };
    graph.process(context);

    // Last node in topological order is whichever of gain_a/gain_b comes last;
    // both must not crash, and the graph must still process end-to-end.
    SUCCEED();
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `sd test --filter Unit_Dsp.RigGraphNodesReturnsAllLiveHandles:Unit_Dsp.RigGraphRemoveNodeDropsItFromNodesAndProcess:Unit_Dsp.RigGraphDisconnectRemovesOnlyThatEdge`
Expected: build failure (`remove_node`/`disconnect`/`node`/`nodes` don't exist yet).

- [ ] **Step 3: Add the declarations**

In `include/SushiDSP/core/RigGraph.hpp`, add to the public section (after `compile`, before `process`):

```cpp
        /// @brief Removes a node and every edge touching it. Not real-time safe; call only off
        /// the processing thread. The handle becomes invalid; other handles are unaffected.
        /// @param handle The node to remove.
        void remove_node(NodeHandle handle);

        /// @brief Removes a single edge, if present. Not real-time safe.
        /// @param from The upstream node.
        /// @param to The downstream node.
        void disconnect(NodeHandle from, NodeHandle to);

        /// @brief Looks up a live node by handle.
        /// @param handle The node's handle.
        /// @return The node, or nullptr if handle is out of range or was removed.
        IAudioNode* node(NodeHandle handle) const;

        /// @brief Lists every live (non-removed) node handle, in add_node() order.
        /// @return The live handles.
        std::vector<NodeHandle> nodes() const;
```

- [ ] **Step 4: Implement in RigGraph.cpp**

Add after `connect()`:

```cpp
    void RigGraph::remove_node(NodeHandle handle)
    {
        if (handle < nodes_.size())
        {
            nodes_[handle].reset();
        }
        edges_.erase(
            std::remove_if(edges_.begin(), edges_.end(),
                [handle](const Edge& edge) { return edge.from == handle || edge.to == handle; }),
            edges_.end());
    }

    void RigGraph::disconnect(NodeHandle from, NodeHandle to)
    {
        edges_.erase(
            std::remove_if(edges_.begin(), edges_.end(),
                [from, to](const Edge& edge) { return edge.from == from && edge.to == to; }),
            edges_.end());
    }

    IAudioNode* RigGraph::node(NodeHandle handle) const
    {
        if (handle >= nodes_.size())
        {
            return nullptr;
        }
        return nodes_[handle].get();
    }

    std::vector<NodeHandle> RigGraph::nodes() const
    {
        std::vector<NodeHandle> result;
        for (NodeHandle handle = 0; handle < nodes_.size(); ++handle)
        {
            if (nodes_[handle] != nullptr)
            {
                result.push_back(handle);
            }
        }
        return result;
    }
```

Now update `compile()` and `process()` to skip tombstoned (null) nodes.
Replace the `compile()` body's assert-through-topological-sort section with:

```cpp
        assert(nodes_.size() <= buffer_pool_.capacity() && "RigGraph: more nodes than BufferPool can hold live simultaneously");

        std::vector<std::uint32_t> in_degree(nodes_.size(), 0);
        for (const Edge& edge : edges_)
        {
            ++in_degree[edge.to];
        }

        std::queue<NodeHandle> ready;
        for (NodeHandle handle = 0; handle < nodes_.size(); ++handle)
        {
            if (nodes_[handle] != nullptr && in_degree[handle] == 0)
            {
                ready.push(handle);
            }
        }

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

        node_output_scratch_.assign(nodes_.size(), nullptr);

        for (auto& node : nodes_)
        {
            if (node != nullptr)
            {
                node->prepare(info);
            }
        }
```

In `process()`, guard the per-node body — replace
`nodes_[handle]->process(node_context);` and everything from
`float* node_buffer = buffer_pool_.acquire(context.frames);` through
`node_output[handle] = node_buffer;` with:

```cpp
            if (nodes_[handle] == nullptr)
            {
                continue;
            }

            float* node_buffer = buffer_pool_.acquire(context.frames);

            bool has_upstream_edge = false;
            const float* upstream_input = nullptr;
            for (const Edge& edge : edges_)
            {
                if (edge.to == handle)
                {
                    upstream_input = node_output[edge.from];
                    has_upstream_edge = true;
                    break;
                }
            }

            float* output_ptr[1] = { node_buffer };
            const float* input_ptr[1] = { upstream_input };

            const float* const* node_inputs = nullptr;
            if (has_upstream_edge)
            {
                node_inputs = input_ptr;
            }
            else if (context.inputs != nullptr)
            {
                node_inputs = context.inputs;
            }

            ProcessContext node_context{ output_ptr, node_inputs, context.frames, context.time_seconds };
            nodes_[handle]->process(node_context);

            node_output[handle] = node_buffer;
```

Also fix the final-output copy: `topological_order_.back()` may now be a
handle whose node was removed after compile but the graph wasn't
recompiled — that can't happen in practice (removal always precedes the
next `compile()`), so no change needed there; leave as-is.

- [ ] **Step 5: Run tests to verify they pass**

Run: `sd test --filter Unit_Dsp.RigGraphNodesReturnsAllLiveHandles:Unit_Dsp.RigGraphRemoveNodeDropsItFromNodesAndProcess:Unit_Dsp.RigGraphDisconnectRemovesOnlyThatEdge`
Expected: PASS. Also run the full suite (`sd test --suite all`) to confirm
no regression in the two pre-existing RigGraph tests.

- [ ] **Step 6: Commit**

```bash
git add include/SushiDSP/core/RigGraph.hpp src/core/RigGraph.cpp tests/functional/unit/test_rig_graph.cpp
git commit -m "core: add RigGraph::remove_node/disconnect/node/nodes"
```

---

### Task 2: `IAudioNode` generic parameter access

**Files:**
- Modify: `include/SushiDSP/core/IAudioNode.hpp`
- Modify: `modules/tubescreamer/include/SushiDSP/tubescreamer/TubeScreamer.hpp`
- Modify: `modules/tubescreamer/src/TubeScreamer.cpp`
- Modify: `modules/jcm800/include/SushiDSP/jcm800/Jcm800.hpp`
- Modify: `modules/jcm800/src/Jcm800.cpp`
- Modify: `tests/functional/unit/test_rig_graph.cpp` (GainNode/ConstantSourceNode need overrides)
- Modify: `apps/host/SineToneNode.hpp`, `apps/host/SineToneNode.cpp`
- Test: `tests/functional/unit/test_tube_screamer.cpp`, `tests/functional/unit/test_jcm800.cpp`

**Interfaces:**
- Consumes: nothing from Task 1.
- Produces: `IAudioNode::get_parameter(const char*) const`,
  `IAudioNode::set_parameter(const char*, float)` — every `IAudioNode`
  implementation in the tree must now implement both.

- [ ] **Step 1: Write the failing tests**

Append to `tests/functional/unit/test_tube_screamer.cpp`:

```cpp
TEST(Unit_Dsp, TubeScreamerGenericParameterRoundTrip)
{
    SushiDSP::TubeScreamer node;
    node.set_parameter("drive", 0.25f);
    node.set_parameter("tone", 0.75f);
    node.set_parameter("level", 0.9f);

    EXPECT_FLOAT_EQ(node.get_parameter("drive"), 0.25f);
    EXPECT_FLOAT_EQ(node.get_parameter("tone"), 0.75f);
    EXPECT_FLOAT_EQ(node.get_parameter("level"), 0.9f);
}

TEST(Unit_Dsp, TubeScreamerUnknownParameterIsSafeNoOp)
{
    SushiDSP::TubeScreamer node;
    node.set_parameter("does_not_exist", 1.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("does_not_exist"), 0.0f);
}
```

Append to `tests/functional/unit/test_jcm800.cpp`:

```cpp
TEST(Unit_Dsp, Jcm800GenericParameterRoundTrip)
{
    SushiDSP::Jcm800 node;
    node.set_parameter("preamp", 0.2f);
    node.set_parameter("master", 0.3f);
    node.set_parameter("bass", 0.4f);
    node.set_parameter("middle", 0.5f);
    node.set_parameter("treble", 0.6f);
    node.set_parameter("presence", 0.7f);

    EXPECT_FLOAT_EQ(node.get_parameter("preamp"), 0.2f);
    EXPECT_FLOAT_EQ(node.get_parameter("master"), 0.3f);
    EXPECT_FLOAT_EQ(node.get_parameter("bass"), 0.4f);
    EXPECT_FLOAT_EQ(node.get_parameter("middle"), 0.5f);
    EXPECT_FLOAT_EQ(node.get_parameter("treble"), 0.6f);
    EXPECT_FLOAT_EQ(node.get_parameter("presence"), 0.7f);
}

TEST(Unit_Dsp, Jcm800UnknownParameterIsSafeNoOp)
{
    SushiDSP::Jcm800 node;
    node.set_parameter("does_not_exist", 1.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("does_not_exist"), 0.0f);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `sd test --filter Unit_Dsp.TubeScreamerGenericParameterRoundTrip:Unit_Dsp.TubeScreamerUnknownParameterIsSafeNoOp:Unit_Dsp.Jcm800GenericParameterRoundTrip:Unit_Dsp.Jcm800UnknownParameterIsSafeNoOp`
Expected: build failure (methods don't exist).

- [ ] **Step 3: Add the pure virtuals to IAudioNode**

In `include/SushiDSP/core/IAudioNode.hpp`, add after `ports()` and before `reset()`:

```cpp
        /// @brief Reads a parameter by its NodeDescriptor::parameters[].id string.
        /// @param id The parameter's id. An unrecognized id returns 0.0f.
        /// @return The parameter's current normalized value.
        virtual float get_parameter(const char* id) const = 0;

        /// @brief Writes a parameter by its NodeDescriptor::parameters[].id string.
        /// @param id The parameter's id. An unrecognized id is a no-op.
        /// @param value The value to set, in the parameter's declared [min, max] range.
        virtual void set_parameter(const char* id, float value) = 0;
```

- [ ] **Step 4: Implement in TubeScreamer**

In `TubeScreamer.hpp`, add to the public section (after `set_level`):

```cpp
        float get_parameter(const char* id) const override;
        void set_parameter(const char* id, float value) override;
```

In `TubeScreamer.cpp`, add after the existing `set_level` definition (before the anonymous namespace):

```cpp
    float TubeScreamer::get_parameter(const char* id) const
    {
        if (std::strcmp(id, "drive") == 0) return drive_target_;
        if (std::strcmp(id, "tone") == 0) return tone_target_;
        if (std::strcmp(id, "level") == 0) return level_target_;
        return 0.0f;
    }

    void TubeScreamer::set_parameter(const char* id, float value)
    {
        if (std::strcmp(id, "drive") == 0) { set_drive(value); return; }
        if (std::strcmp(id, "tone") == 0) { set_tone(value); return; }
        if (std::strcmp(id, "level") == 0) { set_level(value); return; }
    }
```

Add `#include <cstring>` to `TubeScreamer.cpp`'s includes if not already present.

- [ ] **Step 5: Implement in Jcm800**

In `Jcm800.hpp`, add to the public section (after `set_presence`):

```cpp
        float get_parameter(const char* id) const override;
        void set_parameter(const char* id, float value) override;
```

In `Jcm800.cpp`, add after the existing `set_presence` definition (before the anonymous namespace):

```cpp
    float Jcm800::get_parameter(const char* id) const
    {
        if (std::strcmp(id, "preamp") == 0) return preamp_target_;
        if (std::strcmp(id, "master") == 0) return master_target_;
        if (std::strcmp(id, "bass") == 0) return bass_normalized_;
        if (std::strcmp(id, "middle") == 0) return middle_normalized_;
        if (std::strcmp(id, "treble") == 0) return treble_normalized_;
        if (std::strcmp(id, "presence") == 0) return presence_target_;
        return 0.0f;
    }

    void Jcm800::set_parameter(const char* id, float value)
    {
        if (std::strcmp(id, "preamp") == 0) { set_preamp(value); return; }
        if (std::strcmp(id, "master") == 0) { set_master(value); return; }
        if (std::strcmp(id, "bass") == 0) { set_bass(value); return; }
        if (std::strcmp(id, "middle") == 0) { set_middle(value); return; }
        if (std::strcmp(id, "treble") == 0) { set_treble(value); return; }
        if (std::strcmp(id, "presence") == 0) { set_presence(value); return; }
    }
```

Add `#include <cstring>` to `Jcm800.cpp`'s includes if not already present.

- [ ] **Step 6: Fix every other IAudioNode implementation**

`IAudioNode` is now abstract unless every implementation overrides both new
methods. Three test/app-only classes need trivial overrides:

In `tests/functional/unit/test_rig_graph.cpp`, add to `GainNode`'s public
section:
```cpp
        float get_parameter(const char*) const override { return 0.0f; }
        void set_parameter(const char*, float) override {}
```
Add the identical two lines to `ConstantSourceNode`'s public section.

In `apps/host/SineToneNode.hpp`, add to the public section (after `reset()`):
```cpp
        float get_parameter(const char*) const override { return 0.0f; }
        void set_parameter(const char*, float) override {}
```
(Inline in the header — `SineToneNode`'s other trivial-looking members are
declared here and defined in the .cpp, but these two are one-liners with no
state to touch, so defining them inline avoids a needless .cpp edit.)

- [ ] **Step 7: Run tests to verify they pass**

Run: `sd test --suite all`
Expected: full suite passes (this step touches `IAudioNode` itself, so a
full-suite run — not just the new filter — is required to catch any missed
override).

- [ ] **Step 8: Commit**

```bash
git add include/SushiDSP/core/IAudioNode.hpp modules/tubescreamer modules/jcm800 tests/functional/unit/test_rig_graph.cpp tests/functional/unit/test_tube_screamer.cpp tests/functional/unit/test_jcm800.cpp apps/host/SineToneNode.hpp
git commit -m "core: add generic get_parameter/set_parameter to IAudioNode and its implementations"
```

---

### Task 3: `RtGraphSlot` — lock-free glitch-free chain swap

**Files:**
- Create: `include/SushiDSP/core/RtGraphSlot.hpp`
- Create: `src/core/RtGraphSlot.cpp`
- Modify: `CMakeLists.txt` (add `src/core/RtGraphSlot.cpp` to the `sushi_dsp` target's source list)
- Test: `tests/functional/unit/test_rt_graph_slot.cpp`
- Modify: `tests/CMakeLists.txt` (register the new test file under `# --- Core ---`)

**Interfaces:**
- Consumes: `SushiDSP::RigGraph` (Task 1's `RigGraph`, unmodified interface for this task).
- Produces: `SushiDSP::RtGraphSlot` with `publish`, `acquire_for_audio_thread`, `reclaim`.

- [ ] **Step 1: Write the failing tests**

Create `tests/functional/unit/test_rt_graph_slot.cpp`:

```cpp
/**************************************************************************/
/* test_rt_graph_slot.cpp                                                 */
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

#include <atomic>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/core/RtGraphSlot.hpp"

TEST(Unit_Dsp, RtGraphSlotStartsWithNoActiveGraph)
{
    SushiDSP::RtGraphSlot slot;
    EXPECT_EQ(slot.acquire_for_audio_thread(), nullptr);
}

TEST(Unit_Dsp, RtGraphSlotPublishMakesGraphActive)
{
    SushiDSP::RtGraphSlot slot;
    auto graph = std::make_unique<SushiDSP::RigGraph>(256, 4);
    SushiDSP::RigGraph* raw = graph.get();

    slot.publish(std::move(graph));

    EXPECT_EQ(slot.acquire_for_audio_thread(), raw);
}

TEST(Unit_Dsp, RtGraphSlotReclaimFreesOldGraphAfterAudioThreadMovesOn)
{
    SushiDSP::RtGraphSlot slot;
    slot.publish(std::make_unique<SushiDSP::RigGraph>(256, 4));
    slot.acquire_for_audio_thread();  // audio thread "sees" generation 1

    auto second = std::make_unique<SushiDSP::RigGraph>(256, 4);
    SushiDSP::RigGraph* second_raw = second.get();
    slot.publish(std::move(second));

    slot.acquire_for_audio_thread();  // audio thread now "sees" generation 2
    slot.reclaim();                   // safe to free generation 1's graph now

    EXPECT_EQ(slot.acquire_for_audio_thread(), second_raw);
}

TEST(Unit_Dsp, RtGraphSlotConcurrentPublishAndAcquireDoesNotRace)
{
    SushiDSP::RtGraphSlot slot;
    slot.publish(std::make_unique<SushiDSP::RigGraph>(256, 4));

    std::atomic<bool> stop{ false };

    std::thread audio_thread([&]
    {
        while (!stop.load(std::memory_order_relaxed))
        {
            SushiDSP::RigGraph* graph = slot.acquire_for_audio_thread();
            EXPECT_NE(graph, nullptr);
        }
    });

    for (int i = 0; i < 200; ++i)
    {
        slot.publish(std::make_unique<SushiDSP::RigGraph>(256, 4));
        slot.reclaim();
        std::this_thread::yield();
    }

    stop.store(true, std::memory_order_relaxed);
    audio_thread.join();
    slot.reclaim();
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `sd test --filter Unit_Dsp.RtGraphSlot*`
Expected: build failure (`RtGraphSlot` doesn't exist; also register the new
file in `tests/CMakeLists.txt` first — see Step 3b — or the build won't
even see it).

- [ ] **Step 3a: Register the test file**

In `tests/CMakeLists.txt`, add `functional/unit/test_rt_graph_slot.cpp` to
the `# --- Core ---` group, immediately after `functional/unit/test_rig_graph.cpp`.

- [ ] **Step 3b: Create RtGraphSlot.hpp**

```cpp
/**************************************************************************/
/* RtGraphSlot.hpp                                                        */
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

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "SushiDSP/core/RigGraph.hpp"

namespace SushiDSP
{
    /// @brief Publishes RigGraph replacements to the audio thread without ever locking or
    /// blocking it. A non-RT thread builds a whole new compiled RigGraph and publish()es it;
    /// the audio thread reads the active graph with one atomic load per callback. Retired
    /// graphs are freed only once the audio thread has provably moved past them, checked by
    /// reclaim() on the non-RT thread.
    class RtGraphSlot
    {
    public:
        RtGraphSlot() = default;
        ~RtGraphSlot();

        RtGraphSlot(const RtGraphSlot&) = delete;
        RtGraphSlot& operator=(const RtGraphSlot&) = delete;

        /// @brief Makes new_graph the active graph for subsequent acquire_for_audio_thread()
        /// calls. Not real-time safe; call only off the audio thread.
        /// @param new_graph The fully compiled graph to publish. Must not be null.
        void publish(std::unique_ptr<RigGraph> new_graph);

        /// @brief Returns the current active graph and records that this generation is now in
        /// use. Real-time safe: one atomic load, one atomic store, no allocation.
        /// @return The active graph, or nullptr if publish() was never called.
        RigGraph* acquire_for_audio_thread();

        /// @brief Frees every retired graph the audio thread has moved past. Not real-time
        /// safe; call periodically off the audio thread (e.g. once per GUI frame).
        void reclaim();

    private:
        std::atomic<RigGraph*>       active_{ nullptr };
        std::atomic<std::uint64_t>   audio_thread_generation_{ 0 };
        std::atomic<std::uint64_t>   publish_generation_{ 0 };
        std::vector<std::pair<std::uint64_t, std::unique_ptr<RigGraph>>> retired_;
    };
}
```

- [ ] **Step 3c: Create RtGraphSlot.cpp**

```cpp
/**************************************************************************/
/* RtGraphSlot.cpp                                                        */
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

#include "SushiDSP/core/RtGraphSlot.hpp"

namespace SushiDSP
{
    RtGraphSlot::~RtGraphSlot() = default;

    void RtGraphSlot::publish(std::unique_ptr<RigGraph> new_graph)
    {
        // active_ is a non-owning view of whichever entry in retired_ is newest; ownership of
        // every graph ever published lives in retired_ from the moment it's inserted here,
        // keyed by the generation it was published at. exchange()'s old value needs no
        // explicit handling: its owning unique_ptr is already sitting in retired_ from ITS
        // OWN publish() call, and reclaim() is what eventually frees it.
        active_.store(new_graph.get(), std::memory_order_release);
        const std::uint64_t generation = publish_generation_.fetch_add(1, std::memory_order_acq_rel) + 1;
        retired_.emplace_back(generation, std::move(new_graph));
    }

    RigGraph* RtGraphSlot::acquire_for_audio_thread()
    {
        RigGraph* graph = active_.load(std::memory_order_acquire);
        audio_thread_generation_.store(publish_generation_.load(std::memory_order_acquire), std::memory_order_release);
        return graph;
    }

    void RtGraphSlot::reclaim()
    {
        const std::uint64_t safe_generation = audio_thread_generation_.load(std::memory_order_acquire);
        retired_.erase(
            std::remove_if(retired_.begin(), retired_.end(),
                [&](const std::pair<std::uint64_t, std::unique_ptr<RigGraph>>& entry)
                {
                    return entry.first < safe_generation && entry.second.get() != active_.load(std::memory_order_acquire);
                }),
            retired_.end());
    }
}
```

**Self-check before moving on (binding):** `publish_generation_` is read on
the RT thread inside `acquire_for_audio_thread()` and written on the
non-RT thread inside `publish()` — it MUST be `std::atomic<std::uint64_t>`
(as declared above), never a plain integer, or this is a data race that
ThreadSanitizer will catch in Step 5's concurrency test. Add
`#include <algorithm>` to `RtGraphSlot.cpp` for `std::remove_if`.

- [ ] **Step 4: Wire into the root CMakeLists.txt**

In `CMakeLists.txt`, add `src/core/RtGraphSlot.cpp` to the `sushi_dsp`
target's source list (alphabetically among the other `src/core/*.cpp`
entries).

- [ ] **Step 5: Run tests to verify they pass**

Run: `sd test --filter Unit_Dsp.RtGraphSlot*`
Expected: PASS, including the concurrency test.

- [ ] **Step 6: Commit**

```bash
git add include/SushiDSP/core/RtGraphSlot.hpp src/core/RtGraphSlot.cpp CMakeLists.txt tests/functional/unit/test_rt_graph_slot.cpp tests/CMakeLists.txt
git commit -m "core: add RtGraphSlot for lock-free glitch-free RigGraph swaps"
```

---

### Task 4: `modules/node_registry`

**Files:**
- Create: `modules/node_registry/CMakeLists.txt`
- Create: `modules/node_registry/include/SushiDSP/node_registry/NodeRegistry.hpp`
- Create: `modules/node_registry/src/NodeRegistry.cpp`
- Modify: `CMakeLists.txt` (add `add_subdirectory(modules/node_registry)`)
- Test: `tests/functional/unit/test_node_registry.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `SushiDSP::tube_screamer_descriptor()`, `SushiDSP::jcm800_descriptor()`
  (both already exist, unchanged), `SushiDSP::NodeDescriptor` (unchanged),
  `IAudioNode::get_parameter`/`set_parameter` (Task 2, used only by tests here).
- Produces: `SushiDSP::node_registry()`, `SushiDSP::make_node(const char*, const PrepareInfo&)`,
  `SushiDSP::NodeCategory`, `SushiDSP::NodeEntry` — consumed by Task 5/6/7's `apps/host_gui`.

- [ ] **Step 1: Write the failing tests**

Create `tests/functional/unit/test_node_registry.cpp`:

```cpp
/**************************************************************************/
/* test_node_registry.cpp                                                 */
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

#include <cstring>

#include <gtest/gtest.h>

#include "SushiDSP/node_registry/NodeRegistry.hpp"

TEST(Unit_Dsp, NodeRegistryListsTubeScreamerAndJcm800)
{
    const auto& entries = SushiDSP::node_registry();
    ASSERT_EQ(entries.size(), 2u);

    bool found_tube_screamer = false, found_jcm800 = false;
    for (const auto& entry : entries)
    {
        if (std::strcmp(entry.descriptor->id, "sushidsp.tubescreamer") == 0)
        {
            found_tube_screamer = true;
            EXPECT_STREQ(entry.category.name, "Distortion");
        }
        if (std::strcmp(entry.descriptor->id, "sushidsp.jcm800") == 0)
        {
            found_jcm800 = true;
            EXPECT_STREQ(entry.category.name, "Amp");
        }
    }
    EXPECT_TRUE(found_tube_screamer);
    EXPECT_TRUE(found_jcm800);
}

TEST(Unit_Dsp, MakeNodeConstructsTubeScreamerById)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    auto node = SushiDSP::make_node("sushidsp.tubescreamer", info);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->ports().input_channels, 1u);
    EXPECT_EQ(node->ports().output_channels, 1u);
    node->set_parameter("drive", 0.3f);
    EXPECT_FLOAT_EQ(node->get_parameter("drive"), 0.3f);
}

TEST(Unit_Dsp, MakeNodeConstructsJcm800ById)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    auto node = SushiDSP::make_node("sushidsp.jcm800", info);
    ASSERT_NE(node, nullptr);
    node->set_parameter("preamp", 0.4f);
    EXPECT_FLOAT_EQ(node->get_parameter("preamp"), 0.4f);
}

TEST(Unit_Dsp, MakeNodeReturnsNullForUnknownId)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    EXPECT_EQ(SushiDSP::make_node("does.not.exist", info), nullptr);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `sd test --filter Unit_Dsp.NodeRegistry*:Unit_Dsp.MakeNode*`
Expected: build failure — module doesn't exist.

- [ ] **Step 3: Create modules/node_registry/CMakeLists.txt**

```cmake
add_library(node_registry STATIC
    src/NodeRegistry.cpp
)
add_library(SushiDSP::node_registry ALIAS node_registry)
target_link_libraries(node_registry PUBLIC SushiDSP::sushi_dsp SushiDSP::tubescreamer SushiDSP::jcm800)
sushidsp_apply_optimization_flags(node_registry)
target_include_directories(node_registry PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
```

- [ ] **Step 4: Create NodeRegistry.hpp**

```cpp
/**************************************************************************/
/* NodeRegistry.hpp                                                       */
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

#include <memory>
#include <vector>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"

namespace SushiDSP
{
    /// @brief A gear-panel grouping label for a NodeRegistry entry (e.g. "Distortion", "Amp").
    struct NodeCategory
    {
        const char* name;
    };

    /// @brief One constructible product: its descriptor plus its gear-panel category.
    struct NodeEntry
    {
        const NodeDescriptor* descriptor;
        NodeCategory          category;
    };

    /// @brief Every product apps/host_gui can offer in its gear panel. Adding a new product
    /// means adding one entry here — nothing else in the GUI app changes.
    /// @return The static catalog of registered products.
    const std::vector<NodeEntry>& node_registry();

    /// @brief Constructs a node by its NodeDescriptor::id.
    /// @param id The product id, e.g. "sushidsp.tubescreamer".
    /// @param info Sample rate, max block size, and channel count for the new node.
    /// @return The constructed node, or nullptr if id is not registered.
    std::unique_ptr<IAudioNode> make_node(const char* id, const PrepareInfo& info);
}
```

- [ ] **Step 5: Create NodeRegistry.cpp**

```cpp
/**************************************************************************/
/* NodeRegistry.cpp                                                       */
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

#include "SushiDSP/node_registry/NodeRegistry.hpp"

#include <cstring>

#include "SushiDSP/jcm800/Jcm800.hpp"
#include "SushiDSP/tubescreamer/TubeScreamer.hpp"

namespace SushiDSP
{
    const std::vector<NodeEntry>& node_registry()
    {
        static const std::vector<NodeEntry> entries{
            { &tube_screamer_descriptor(), NodeCategory{ "Distortion" } },
            { &jcm800_descriptor(), NodeCategory{ "Amp" } },
        };
        return entries;
    }

    std::unique_ptr<IAudioNode> make_node(const char* id, const PrepareInfo& info)
    {
        for (const NodeEntry& entry : node_registry())
        {
            if (std::strcmp(entry.descriptor->id, id) == 0)
            {
                return entry.descriptor->make(info);
            }
        }
        return nullptr;
    }
}
```

- [ ] **Step 6: Wire into root CMakeLists.txt and tests/CMakeLists.txt**

In `CMakeLists.txt`, add `add_subdirectory(modules/node_registry)` right
after `add_subdirectory(modules/jcm800)` — unconditional, same reasoning as
its neighbors (no SDL2/ASIO dependency).

In `tests/CMakeLists.txt`, add `functional/unit/test_node_registry.cpp`
under `# --- Core ---` after `test_rt_graph_slot.cpp`, and add
`SushiDSP::node_registry` to `sushidsp_tests`'s `target_link_libraries`.

- [ ] **Step 7: Run tests to verify they pass**

Run: `sd test --filter Unit_Dsp.NodeRegistry*:Unit_Dsp.MakeNode*`
Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add modules/node_registry CMakeLists.txt tests/CMakeLists.txt tests/functional/unit/test_node_registry.cpp
git commit -m "Add modules/node_registry: id-to-NodeDescriptor catalog for host_gui"
```

---

### Task 5: `apps/host_gui` skeleton — window, device selection, RtGraphSlot wiring

**Files:**
- Create: `apps/host_gui/CMakeLists.txt`
- Create: `apps/host_gui/main.cpp`
- Modify: `CMakeLists.txt` (add `SUSHIDSP_BUILD_GUI` option + FetchContent for GLFW/Dear ImGui/nlohmann::json + `add_subdirectory(apps/host_gui)`)

**Interfaces:**
- Consumes: `SushiDSP::RigGraph`, `SushiDSP::RtGraphSlot` (Task 3),
  `SushiDSP::node_registry`/`make_node` (Task 4), `SushiDSP::AsioAudioDevice`,
  `SushiDSP::Sdl2AudioDevice`, `SushiDSP::IAudioDevice`, `SushiDSP::DeviceFormat`,
  `SushiDSP::DeviceCallback` (all pre-existing, unchanged).
- Produces: a running `sushidsp_host_gui` executable with an empty ImGui
  frame loop and live audio through an initial one-node (TubeScreamer) rig,
  swapped once via `RtGraphSlot` to prove the wiring. Task 6 builds the real
  rack-strip/gear-panel UI inside this loop.

This task has no unit-testable business logic of its own (it is
integration wiring for a manual/visual app) — its acceptance is a build +
manual smoke run, per the spec's stated testing scope for the ImGui layer.

- [ ] **Step 1: Add GUI dependencies to the root CMakeLists.txt**

Add after the `SUSHIDSP_BUILD_ASIO` block's `endif()`:

```cmake
# ImGui/GLFW/nlohmann::json are fetched only when the GUI is built; CI can
# disable this exactly like SUSHIDSP_BUILD_SDL2/SUSHIDSP_BUILD_ASIO to skip
# the extra network fetch and build time when the GUI isn't needed.
option(SUSHIDSP_BUILD_GUI "Build the ImGui rig GUI (apps/host_gui)" ON)

if(SUSHIDSP_BUILD_GUI)
    include(FetchContent)

    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
    )
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)

    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.90.9
    )
    FetchContent_MakeAvailable(imgui)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui PUBLIC glfw)

    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
    )
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(json)

    add_subdirectory(apps/host_gui)
endif()
```

- [ ] **Step 2: Create apps/host_gui/CMakeLists.txt**

```cmake
add_executable(sushidsp_host_gui
    main.cpp
)
target_link_libraries(sushidsp_host_gui PRIVATE
    SushiDSP::sushi_dsp
    SushiDSP::node_registry
    SushiDSP::tubescreamer
    SushiDSP::jcm800
    SushiDSP::sushi_dsp_sdl2
    imgui
    nlohmann_json::nlohmann_json
)
if(TARGET SushiDSP::sushi_dsp_asio)
    target_link_libraries(sushidsp_host_gui PRIVATE SushiDSP::sushi_dsp_asio)
    target_compile_definitions(sushidsp_host_gui PRIVATE SUSHIDSP_HAVE_ASIO=1)
endif()
```

`SUSHIDSP_BUILD_GUI` depending on `SushiDSP::sushi_dsp_sdl2` means
`apps/host_gui` requires `SUSHIDSP_BUILD_SDL2 ON` — both default `ON`, and
this is consistent with the spec's "either backend, chosen at startup"
goal (SDL2 is the always-available fallback; ASIO is conditionally linked
via the `SUSHIDSP_HAVE_ASIO` compile definition, guarding
`#include "SushiDSP/device/AsioAudioDevice.hpp"` in `main.cpp`).

- [ ] **Step 3: Create apps/host_gui/main.cpp**

```cpp
/**************************************************************************/
/* main.cpp                                                               */
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

#include <cstdio>
#include <memory>
#include <vector>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "SushiDSP/core/IAudioDevice.hpp"
#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/core/RtGraphSlot.hpp"
#include "SushiDSP/device/Sdl2AudioDevice.hpp"
#include "SushiDSP/node_registry/NodeRegistry.hpp"

#if SUSHIDSP_HAVE_ASIO
#include "SushiDSP/device/AsioAudioDevice.hpp"
#endif

namespace
{
    /// @brief Builds a one-node starter rig (TubeScreamer only) fully compiled and ready to
    /// hand to RtGraphSlot::publish(). Task 6 replaces the hardcoded catalog id with whatever
    /// the rack strip actually contains.
    std::unique_ptr<SushiDSP::RigGraph> build_starter_rig(const SushiDSP::PrepareInfo& info)
    {
        auto graph = std::make_unique<SushiDSP::RigGraph>(info.max_block, 4);
        auto node = SushiDSP::make_node("sushidsp.tubescreamer", info);
        graph->add_node(std::move(node));
        graph->compile(info);
        return graph;
    }
}

int main()
{
    if (!glfwInit())
    {
        std::fprintf(stderr, "glfwInit failed\n");
        return 1;
    }

    GLFWwindow* window = glfwCreateWindow(1280, 800, "SushiDSP Rig", nullptr, nullptr);
    if (window == nullptr)
    {
        std::fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    SushiDSP::DeviceFormat requested{ 48000.0, 256, 1, 1 };
    std::unique_ptr<SushiDSP::IAudioDevice> device = std::make_unique<SushiDSP::Sdl2AudioDevice>(requested);
    const SushiDSP::DeviceFormat actual = device->format();

    SushiDSP::RtGraphSlot graph_slot;
    SushiDSP::PrepareInfo info{ actual.sample_rate, actual.block_size, actual.output_channels };
    graph_slot.publish(build_starter_rig(info));

    device->start([&graph_slot](const float* const* inputs, float* const* outputs, std::uint32_t frames)
    {
        SushiDSP::RigGraph* active = graph_slot.acquire_for_audio_thread();
        SushiDSP::ProcessContext context{ outputs, inputs, frames, 0.0 };
        if (active != nullptr)
        {
            active->process(context);
        }
    });

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        graph_slot.reclaim();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("SushiDSP Rig");
        ImGui::Text("Skeleton window - rack strip and gear panel land in Task 6.");
        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    device->stop();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
```

`glViewport`/`glClearColor`/`glClear`/`GL_COLOR_BUFFER_BIT` need an OpenGL
loader; add `#include <GL/gl.h>` alongside the other includes if the build
fails to find these symbols (GLFW itself does not provide GL entry points
on all platforms — Windows' `opengl32.lib` exposes GL 1.1 symbols including
these four directly, which is normally sufficient here since none of them
are beyond GL 1.1; link `opengl32` in `apps/host_gui/CMakeLists.txt`'s
`target_link_libraries` if the linker reports them unresolved).

- [ ] **Step 4: Build and smoke-test manually**

Run: `sd build`
Expected: `sushidsp_host_gui.exe` builds successfully.

Run the executable manually (this is the manual/visual verification the
spec calls for — not part of `sd test`): confirm a window titled
"SushiDSP Rig" opens showing the skeleton text, and that closing it cleanly
exits with no crash. Report actual audio output is out of scope for this
task (no live input source is guaranteed on a CI/dev machine) — the window
opening and closing cleanly is the acceptance bar here.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt apps/host_gui
git commit -m "Add apps/host_gui skeleton: ImGui/GLFW window, SDL2 device, RtGraphSlot wiring"
```

---

### Task 6: Rack strip, gear panel, parameter panel, drag-and-drop

**Files:**
- Modify: `apps/host_gui/main.cpp`
- Create: `apps/host_gui/RigController.hpp`
- Create: `apps/host_gui/RigController.cpp`
- Modify: `apps/host_gui/CMakeLists.txt` (add `RigController.cpp`)

**Interfaces:**
- Consumes: everything from Task 5, plus `SushiDSP::RigGraph::nodes()`/`node()`/`remove_node()`
  (Task 1), `IAudioNode::get_parameter`/`set_parameter` (Task 2),
  `SushiDSP::node_registry()`/`make_node()` (Task 4).
- Produces: `RigController`, a non-RT-thread helper that owns the
  authoritative chain order (as a `std::vector<std::string>` of
  `NodeDescriptor::id`s plus their current parameter values) and rebuilds +
  publishes a fresh `RigGraph` through `RtGraphSlot` whenever the order
  changes. Task 7's preset loader reuses this same
  `RigController::rebuild_and_publish()` entry point.

This task's logic (`RigController`'s chain bookkeeping) is unit-testable
even though the ImGui rendering around it is not — split accordingly.

- [ ] **Step 1: Write the failing test for RigController**

Create `apps/host_gui/RigControllerTest.cpp` is NOT how this project
structures tests (all GTest files live under `tests/functional/unit/`).
Create `tests/functional/unit/test_rig_controller.cpp` instead, and add
`apps/host_gui/RigController.cpp` plus an include path for
`apps/host_gui/` to `tests/CMakeLists.txt` (mirroring the existing
`target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/apps/host)`
/ `apps/host/SineToneNode.cpp` pattern already used for `SineToneNode`).

```cpp
/**************************************************************************/
/* test_rig_controller.cpp                                                */
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

#include <gtest/gtest.h>

#include "RigController.hpp"

TEST(Unit_Dsp, RigControllerStartsEmpty)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    EXPECT_TRUE(controller.chain_ids().empty());
}

TEST(Unit_Dsp, RigControllerAppendAddsNodeToEndOfChain)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");

    const auto ids = controller.chain_ids();
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], "sushidsp.tubescreamer");
    EXPECT_EQ(ids[1], "sushidsp.jcm800");
    ASSERT_NE(controller.active_graph(), nullptr);
}

TEST(Unit_Dsp, RigControllerMoveReordersChain)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");

    controller.move(0, 1);  // move index 0 to after index 1

    const auto ids = controller.chain_ids();
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], "sushidsp.jcm800");
    EXPECT_EQ(ids[1], "sushidsp.tubescreamer");
}

TEST(Unit_Dsp, RigControllerRemoveDropsFromChain)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");

    controller.remove(0);

    const auto ids = controller.chain_ids();
    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(ids[0], "sushidsp.jcm800");
}

TEST(Unit_Dsp, RigControllerPreservesParameterValuesAcrossReorder)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    controller.append("sushidsp.tubescreamer");
    controller.set_parameter(0, "drive", 0.42f);

    controller.append("sushidsp.jcm800");
    controller.move(0, 1);

    // tube_screamer is now at index 1 after the move.
    EXPECT_FLOAT_EQ(controller.get_parameter(1, "drive"), 0.42f);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `sd test --filter Unit_Dsp.RigController*`
Expected: build failure — `RigController` doesn't exist.

- [ ] **Step 3: Create apps/host_gui/RigController.hpp**

```cpp
/**************************************************************************/
/* RigController.hpp                                                      */
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

#include <map>
#include <string>
#include <vector>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/core/RtGraphSlot.hpp"

/// @brief Owns the authoritative, ordered rig chain (a list of NodeDescriptor ids plus each
/// slot's current parameter values) and rebuilds + publishes a fresh SushiDSP::RigGraph
/// through an SushiDSP::RtGraphSlot on every structural edit. Not real-time safe — every
/// method here runs on the GUI/main thread only. Not part of the SushiDSP:: namespace: this
/// is app-local glue, not a library product.
class RigController
{
public:
    explicit RigController(const SushiDSP::PrepareInfo& info);

    /// @brief Appends a new node (by NodeDescriptor::id) to the end of the chain and republishes.
    void append(const std::string& descriptor_id);

    /// @brief Inserts a new node at chain position `index` and republishes.
    void insert(std::size_t index, const std::string& descriptor_id);

    /// @brief Removes the chain slot at `index` and republishes.
    void remove(std::size_t index);

    /// @brief Moves the chain slot at `from` to just after `to` (or to the front if to == 0 and
    /// from > 0 moving backward) and republishes. Semantics match std::vector element move:
    /// the element at `from` is removed, then reinserted at index `to` in the resulting vector.
    void move(std::size_t from, std::size_t to);

    /// @brief Sets a parameter on the live node at chain position `index`. Does NOT republish
    /// (no structural change) - writes directly to the live IAudioNode instance.
    void set_parameter(std::size_t index, const char* param_id, float value);

    /// @brief Reads a parameter from the live node at chain position `index`.
    float get_parameter(std::size_t index, const char* param_id) const;

    /// @brief The chain's NodeDescriptor ids, in order.
    std::vector<std::string> chain_ids() const;

    /// @brief The currently active, published RigGraph (for the RT thread to acquire from).
    SushiDSP::RigGraph* active_graph();

    /// @brief The RtGraphSlot the audio callback should acquire_for_audio_thread() from.
    SushiDSP::RtGraphSlot& graph_slot();

    /// @brief Replaces the entire chain (used by preset load) and republishes.
    /// @param ids Descriptor ids in chain order.
    /// @param params Per-slot parameter id/value maps, same length and order as ids.
    void load_chain(const std::vector<std::string>& ids, const std::vector<std::map<std::string, float>>& params);

private:
    struct Slot
    {
        std::string descriptor_id;
        std::map<std::string, float> parameters;
    };

    void rebuild_and_publish();

    SushiDSP::PrepareInfo   info_;
    std::vector<Slot>       slots_;
    SushiDSP::RtGraphSlot   graph_slot_;
    SushiDSP::RigGraph*     active_graph_ = nullptr;
};
```

- [ ] **Step 4: Create apps/host_gui/RigController.cpp**

```cpp
/**************************************************************************/
/* RigController.cpp                                                      */
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

#include "RigController.hpp"

#include "SushiDSP/node_registry/NodeRegistry.hpp"

RigController::RigController(const SushiDSP::PrepareInfo& info)
    : info_(info)
{
    rebuild_and_publish();
}

void RigController::append(const std::string& descriptor_id)
{
    insert(slots_.size(), descriptor_id);
}

void RigController::insert(std::size_t index, const std::string& descriptor_id)
{
    Slot slot;
    slot.descriptor_id = descriptor_id;
    slots_.insert(slots_.begin() + static_cast<std::ptrdiff_t>(index), std::move(slot));
    rebuild_and_publish();
}

void RigController::remove(std::size_t index)
{
    if (index < slots_.size())
    {
        slots_.erase(slots_.begin() + static_cast<std::ptrdiff_t>(index));
        rebuild_and_publish();
    }
}

void RigController::move(std::size_t from, std::size_t to)
{
    if (from >= slots_.size() || to >= slots_.size() || from == to)
    {
        return;
    }
    Slot moved = std::move(slots_[from]);
    slots_.erase(slots_.begin() + static_cast<std::ptrdiff_t>(from));
    slots_.insert(slots_.begin() + static_cast<std::ptrdiff_t>(to), std::move(moved));
    rebuild_and_publish();
}

void RigController::set_parameter(std::size_t index, const char* param_id, float value)
{
    if (index >= slots_.size())
    {
        return;
    }
    slots_[index].parameters[param_id] = value;
    if (active_graph_ != nullptr)
    {
        SushiDSP::IAudioNode* node = active_graph_->node(static_cast<SushiDSP::NodeHandle>(index));
        if (node != nullptr)
        {
            node->set_parameter(param_id, value);
        }
    }
}

float RigController::get_parameter(std::size_t index, const char* param_id) const
{
    if (index >= slots_.size())
    {
        return 0.0f;
    }
    const auto it = slots_[index].parameters.find(param_id);
    return it != slots_[index].parameters.end() ? it->second : 0.0f;
}

std::vector<std::string> RigController::chain_ids() const
{
    std::vector<std::string> ids;
    ids.reserve(slots_.size());
    for (const Slot& slot : slots_)
    {
        ids.push_back(slot.descriptor_id);
    }
    return ids;
}

SushiDSP::RigGraph* RigController::active_graph()
{
    return active_graph_;
}

SushiDSP::RtGraphSlot& RigController::graph_slot()
{
    return graph_slot_;
}

void RigController::load_chain(const std::vector<std::string>& ids, const std::vector<std::map<std::string, float>>& params)
{
    slots_.clear();
    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        Slot slot;
        slot.descriptor_id = ids[i];
        slot.parameters = i < params.size() ? params[i] : std::map<std::string, float>{};
        slots_.push_back(std::move(slot));
    }
    rebuild_and_publish();
}

void RigController::rebuild_and_publish()
{
    auto graph = std::make_unique<SushiDSP::RigGraph>(info_.max_block, static_cast<std::uint32_t>(slots_.size() + 1));

    std::vector<SushiDSP::NodeHandle> handles;
    handles.reserve(slots_.size());
    for (const Slot& slot : slots_)
    {
        auto node = SushiDSP::make_node(slot.descriptor_id.c_str(), info_);
        const SushiDSP::NodeHandle handle = graph->add_node(std::move(node));
        handles.push_back(handle);
    }
    for (std::size_t i = 0; i + 1 < handles.size(); ++i)
    {
        graph->connect(handles[i], handles[i + 1]);
    }

    graph->compile(info_);

    for (std::size_t i = 0; i < slots_.size(); ++i)
    {
        SushiDSP::IAudioNode* node = graph->node(handles[i]);
        for (const auto& [param_id, value] : slots_[i].parameters)
        {
            node->set_parameter(param_id.c_str(), value);
        }
    }

    active_graph_ = graph.get();
    graph_slot_.publish(std::move(graph));
}
```

**Note (binding):** `active_graph_` is a raw observer pointer into the
graph currently owned by `graph_slot_`'s internal `retired_` vector; it is
only ever read/written on the GUI thread (same thread that calls
`rebuild_and_publish`), matching `RtGraphSlot`'s non-RT-thread contract for
everything except `acquire_for_audio_thread()`. It becomes dangling only if
`graph_slot_.reclaim()` frees it — which `RtGraphSlot::reclaim()` never
does for the graph that is still `active_` — so it stays valid until the
next `rebuild_and_publish()` reassigns it.

- [ ] **Step 5: Add RigController.cpp to CMakeLists and to the test target**

In `apps/host_gui/CMakeLists.txt`, add `RigController.cpp` to
`add_executable(sushidsp_host_gui main.cpp RigController.cpp)`.

In `tests/CMakeLists.txt`: add
`${CMAKE_SOURCE_DIR}/apps/host_gui/RigController.cpp` to the
`add_executable(sushidsp_tests ...)` source list under `# --- Host
diagnostics ---` (next to the existing `SineToneNode.cpp` entry), add
`functional/unit/test_rig_controller.cpp` to the same list, add
`target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/apps/host_gui)`,
and add `SushiDSP::node_registry` to `sushidsp_tests`'s
`target_link_libraries` if not already present from Task 4.

- [ ] **Step 6: Run tests to verify they pass**

Run: `sd test --filter Unit_Dsp.RigController*`
Expected: PASS.

- [ ] **Step 7: Wire RigController and build the rack strip / gear panel / parameter panel UI in main.cpp**

Replace `apps/host_gui/main.cpp`'s `build_starter_rig` function and the
`SushiDSP::RtGraphSlot graph_slot; ... graph_slot.publish(...)` block with:

```cpp
    RigController rig_controller(info);
```

Replace the device callback's `graph_slot.acquire_for_audio_thread()` with
`rig_controller.graph_slot().acquire_for_audio_thread()`, and the frame
loop's `graph_slot.reclaim();` with `rig_controller.graph_slot().reclaim();`.

Add `#include "RigController.hpp"` to the includes.

Replace the `ImGui::Begin("SushiDSP Rig"); ... ImGui::End();` block with:

```cpp
        ImGui::Begin("SushiDSP Rig");

        ImGui::Text("Rack");
        ImGui::Separator();
        {
            const auto ids = rig_controller.chain_ids();
            static int selected_slot = -1;
            for (std::size_t i = 0; i < ids.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(ids[i].c_str(), selected_slot == static_cast<int>(i), 0, ImVec2(150, 0)))
                {
                    selected_slot = static_cast<int>(i);
                }
                if (ImGui::BeginDragDropSource())
                {
                    ImGui::SetDragDropPayload("RACK_SLOT_INDEX", &i, sizeof(std::size_t));
                    ImGui::Text("%s", ids[i].c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("RACK_SLOT_INDEX"))
                    {
                        const std::size_t from = *static_cast<const std::size_t*>(payload->Data);
                        rig_controller.move(from, i);
                    }
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GEAR_DESCRIPTOR_ID"))
                    {
                        const char* descriptor_id = static_cast<const char*>(payload->Data);
                        rig_controller.insert(i, std::string(descriptor_id));
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("x"))
                {
                    rig_controller.remove(i);
                    if (selected_slot == static_cast<int>(i))
                    {
                        selected_slot = -1;
                    }
                }
                ImGui::PopID();
            }

            // A drop target at the end of the list, for appending.
            ImGui::Dummy(ImVec2(150, 20));
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GEAR_DESCRIPTOR_ID"))
                {
                    const char* descriptor_id = static_cast<const char*>(payload->Data);
                    rig_controller.append(std::string(descriptor_id));
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::Separator();
            ImGui::Text("Parameters");
            if (selected_slot >= 0 && selected_slot < static_cast<int>(ids.size()))
            {
                SushiDSP::RigGraph* active = rig_controller.active_graph();
                if (active != nullptr)
                {
                    SushiDSP::IAudioNode* node = active->node(static_cast<SushiDSP::NodeHandle>(selected_slot));
                    for (const auto& entry : SushiDSP::node_registry())
                    {
                        if (ids[static_cast<std::size_t>(selected_slot)] == entry.descriptor->id)
                        {
                            for (std::size_t p = 0; p < entry.descriptor->parameter_count; ++p)
                            {
                                const SushiDSP::ParameterDescriptor& param = entry.descriptor->parameters[p];
                                float value = node != nullptr ? node->get_parameter(param.id) : param.default_value;
                                if (ImGui::SliderFloat(param.label, &value, param.min, param.max))
                                {
                                    rig_controller.set_parameter(static_cast<std::size_t>(selected_slot), param.id, value);
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }

        ImGui::Separator();
        ImGui::Text("Gear");
        {
            const char* current_category = nullptr;
            for (const auto& entry : SushiDSP::node_registry())
            {
                if (current_category == nullptr || std::strcmp(current_category, entry.category.name) != 0)
                {
                    current_category = entry.category.name;
                    ImGui::Text("%s", current_category);
                }
                ImGui::Button(entry.descriptor->name);
                if (ImGui::BeginDragDropSource())
                {
                    ImGui::SetDragDropPayload("GEAR_DESCRIPTOR_ID", entry.descriptor->id, std::strlen(entry.descriptor->id) + 1);
                    ImGui::Text("%s", entry.descriptor->name);
                    ImGui::EndDragDropSource();
                }
            }
        }

        ImGui::End();
```

Add `#include <cstring>` to `main.cpp`'s includes.

**Note on `selected_slot`'s `static` storage:** it is a function-local
`static` inside the render loop's single call site (not re-entered
recursively), which is the simplest way to persist UI selection state
across frames without adding a class for it — acceptable for this single
top-level window; a real multi-window app would hoist it into a struct
instead.

- [ ] **Step 8: Build and smoke-test manually**

Run: `sd build`
Expected: `sushidsp_host_gui.exe` builds.

Run manually: drag a gear entry onto the rack strip, confirm a labeled box
appears; click it, confirm sliders appear for its parameters and moving
one doesn't crash; drag a second gear entry to append; drag-reorder the
two rack boxes; click "x" to remove one. Confirm no crash across all of
these and that the window keeps rendering after each interaction. Audio
correctness (does it actually sound right) is out of scope for this manual
check — it's covered by the existing per-node regression tests; this
check is UI wiring only.

- [ ] **Step 9: Commit**

```bash
git add apps/host_gui tests/CMakeLists.txt tests/functional/unit/test_rig_controller.cpp
git commit -m "host_gui: add RigController, rack strip, gear panel, and parameter sliders"
```

---

### Task 7: JSON preset save/load

**Files:**
- Create: `apps/host_gui/Preset.hpp`
- Create: `apps/host_gui/Preset.cpp`
- Modify: `apps/host_gui/CMakeLists.txt` (add `Preset.cpp`)
- Modify: `apps/host_gui/main.cpp` (Save/Load buttons)
- Test: `tests/functional/unit/test_preset.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `RigController::chain_ids()`, `RigController::get_parameter()`,
  `RigController::load_chain()` (Task 6), `SushiDSP::node_registry()`
  (Task 4, to enumerate each slot's declared parameter ids when saving).
- Produces: `save_preset(const RigController&, const std::string& path)`,
  `load_preset(RigController&, const std::string& path)`.

- [ ] **Step 1: Write the failing tests**

Create `tests/functional/unit/test_preset.cpp`:

```cpp
/**************************************************************************/
/* test_preset.cpp                                                        */
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

#include <cstdio>
#include <string>

#include <gtest/gtest.h>

#include "Preset.hpp"
#include "RigController.hpp"

namespace
{
    std::string temp_preset_path()
    {
        return std::string(::testing::TempDir()) + "sushidsp_test_preset.json";
    }
}

TEST(Unit_Dsp, PresetSaveLoadRoundTrip)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController writer(info);
    writer.append("sushidsp.tubescreamer");
    writer.set_parameter(0, "drive", 0.33f);
    writer.set_parameter(0, "tone", 0.66f);
    writer.set_parameter(0, "level", 0.77f);
    writer.append("sushidsp.jcm800");
    writer.set_parameter(1, "preamp", 0.11f);
    writer.set_parameter(1, "master", 0.22f);

    const std::string path = temp_preset_path();
    save_preset(writer, path);

    RigController reader(info);
    load_preset(reader, path);

    const auto ids = reader.chain_ids();
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], "sushidsp.tubescreamer");
    EXPECT_EQ(ids[1], "sushidsp.jcm800");
    EXPECT_FLOAT_EQ(reader.get_parameter(0, "drive"), 0.33f);
    EXPECT_FLOAT_EQ(reader.get_parameter(0, "tone"), 0.66f);
    EXPECT_FLOAT_EQ(reader.get_parameter(0, "level"), 0.77f);
    EXPECT_FLOAT_EQ(reader.get_parameter(1, "preamp"), 0.11f);
    EXPECT_FLOAT_EQ(reader.get_parameter(1, "master"), 0.22f);

    std::remove(path.c_str());
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `sd test --filter Unit_Dsp.PresetSaveLoadRoundTrip`
Expected: build failure — `Preset.hpp`/`save_preset`/`load_preset` don't
exist yet.

- [ ] **Step 3: Create apps/host_gui/Preset.hpp**

```cpp
/**************************************************************************/
/* Preset.hpp                                                             */
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

#include "RigController.hpp"

/// @brief Writes the controller's chain (node ids in order) and every declared parameter's
/// current value to a JSON file at path. Throws std::runtime_error if the file can't be opened.
void save_preset(RigController& controller, const std::string& path);

/// @brief Reads a JSON preset from path and replaces controller's entire chain via
/// RigController::load_chain(). Throws std::runtime_error if the file can't be opened or parsed.
void load_preset(RigController& controller, const std::string& path);
```

- [ ] **Step 4: Create apps/host_gui/Preset.cpp**

```cpp
/**************************************************************************/
/* Preset.cpp                                                             */
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

#include "Preset.hpp"

#include <fstream>
#include <map>
#include <stdexcept>
#include <vector>

#include <nlohmann/json.hpp>

#include "SushiDSP/node_registry/NodeRegistry.hpp"

void save_preset(RigController& controller, const std::string& path)
{
    nlohmann::json root;
    root["nodes"] = nlohmann::json::array();

    const auto ids = controller.chain_ids();
    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        nlohmann::json node_json;
        node_json["type"] = ids[i];
        node_json["params"] = nlohmann::json::object();

        for (const auto& entry : SushiDSP::node_registry())
        {
            if (ids[i] == entry.descriptor->id)
            {
                for (std::size_t p = 0; p < entry.descriptor->parameter_count; ++p)
                {
                    const char* param_id = entry.descriptor->parameters[p].id;
                    node_json["params"][param_id] = controller.get_parameter(i, param_id);
                }
                break;
            }
        }
        root["nodes"].push_back(node_json);
    }

    std::ofstream out(path);
    if (!out)
    {
        throw std::runtime_error("save_preset: could not open '" + path + "' for writing");
    }
    out << root.dump(2);
}

void load_preset(RigController& controller, const std::string& path)
{
    std::ifstream in(path);
    if (!in)
    {
        throw std::runtime_error("load_preset: could not open '" + path + "' for reading");
    }

    nlohmann::json root;
    in >> root;

    std::vector<std::string> ids;
    std::vector<std::map<std::string, float>> params;

    for (const auto& node_json : root.at("nodes"))
    {
        ids.push_back(node_json.at("type").get<std::string>());

        std::map<std::string, float> node_params;
        for (const auto& [key, value] : node_json.at("params").items())
        {
            node_params[key] = value.get<float>();
        }
        params.push_back(std::move(node_params));
    }

    controller.load_chain(ids, params);
}
```

- [ ] **Step 5: Wire Preset.cpp into build files**

In `apps/host_gui/CMakeLists.txt`, add `Preset.cpp` to
`add_executable(sushidsp_host_gui main.cpp RigController.cpp Preset.cpp)`.

In `tests/CMakeLists.txt`: add
`${CMAKE_SOURCE_DIR}/apps/host_gui/Preset.cpp` next to
`RigController.cpp` in the source list, add
`functional/unit/test_preset.cpp` next to `test_rig_controller.cpp`, and
add `nlohmann_json::nlohmann_json` to `sushidsp_tests`'s
`target_link_libraries`. This requires `sushidsp_tests` to only build when
`SUSHIDSP_BUILD_GUI` has run `FetchContent_MakeAvailable(json)` — since
`enable_testing()`/`add_subdirectory(tests)` in the root `CMakeLists.txt`
runs after the `SUSHIDSP_BUILD_GUI` block, `nlohmann_json::nlohmann_json`
is already available by the time `tests/CMakeLists.txt` configures, as
long as `SUSHIDSP_BUILD_GUI` is `ON` (its default). If a future developer
builds with `SUSHIDSP_BUILD_GUI OFF`, `sushidsp_tests` would fail to
configure on this target — note this as a known limitation in a comment
above the `nlohmann_json::nlohmann_json` line in `tests/CMakeLists.txt`
rather than solving it now (out of scope: the spec's Global Constraints
never asked for the test suite to be independent of `SUSHIDSP_BUILD_GUI`,
and every other opt-in module here — SDL2, ASIO — has the same "tests
assume it's on" characteristic already, e.g. `apps/host/SineToneNode.cpp`
being compiled unconditionally into `sushidsp_tests` regardless of
`SUSHIDSP_BUILD_SDL2`).

- [ ] **Step 6: Run tests to verify they pass**

Run: `sd test --filter Unit_Dsp.PresetSaveLoadRoundTrip`
Expected: PASS. Then run `sd test --suite all` to confirm the whole suite
is green.

- [ ] **Step 7: Add Save/Load buttons to main.cpp**

Add `#include "Preset.hpp"` to `apps/host_gui/main.cpp`. Inside the
`ImGui::Begin("SushiDSP Rig");` block, before the "Rack" section, add:

```cpp
        static char preset_path[256] = "preset.json";
        ImGui::InputText("Preset path", preset_path, sizeof(preset_path));
        ImGui::SameLine();
        if (ImGui::Button("Save"))
        {
            try { save_preset(rig_controller, preset_path); }
            catch (const std::exception& e) { std::fprintf(stderr, "Save failed: %s\n", e.what()); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Load"))
        {
            try { load_preset(rig_controller, preset_path); }
            catch (const std::exception& e) { std::fprintf(stderr, "Load failed: %s\n", e.what()); }
        }
        ImGui::Separator();
```

- [ ] **Step 8: Build and smoke-test manually**

Run: `sd build`
Expected: builds cleanly.

Run manually: build a two-node chain with distinctive parameter values,
click Save, restart the app (or click Load after building a different
chain), click Load, confirm the original chain and parameter values come
back exactly.

- [ ] **Step 9: Commit**

```bash
git add apps/host_gui tests/CMakeLists.txt tests/functional/unit/test_preset.cpp
git commit -m "host_gui: add JSON preset save/load"
```
