# Standalone Cabinet Node Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Promote the cabinet impulse-response convolver from a mixin baked into each amp
node (`ICabinetHost`) to `sushidsp.cabinet`, a standalone `IAudioNode` product that a rig
places after an amp explicitly, and retire the amp-embedded path.

**Architecture:** A new `modules/cabinet/` module owns one class, `CabinetNode`, which
implements both `IAudioNode` and the existing `ICabinetHost` contract by wrapping the
existing `StreamingConvolution` the same way `Jcm800` does today — a control-thread build,
one atomic `RtSwapSlot` publish, an audio-thread acquire. `Jcm800`, `Peavey5150`,
`DiezelVh4` and `MesaMark2c` drop `ICabinetHost` and the convolution members it drove;
`apps/host_gui/RigController` needs **no code changes** — its cabinet API already
`dynamic_cast`s onto whatever node lives at a slot, and its GUI caller
(`apps/host_gui/ui/StagePanel.cpp`) already gates the cabinet controls on
`rig.hosts_cabinet(slot)`, so once `CabinetNode` is registered those controls simply appear
against the cabinet node's own slot instead of an amp's.

**Tech Stack:** C++17, GoogleTest via ctest (`sd test`), existing
`StreamingConvolution`/`IrLoader`/`RtSwapSlot` infrastructure — no new dependency.

**Spec:** `docs/design/gui_ux_redesign.md` §8.

## Global Constraints

- C++17, Allman braces, `PascalCase` types, `snake_case` functions/variables, trailing
  underscore on members (`impl_`), `UPPER_SNAKE` constants, no abbreviations.
- Apache 2.0 license header box (`docs/design/sushidsp.md` §0a) on every new `.hpp`/`.cpp`.
- Doxygen `@brief`/`@param`/`@return` on every public function; no comments beyond that
  unless the *why* is non-obvious.
- `IAudioNode::process()` and everything it calls: no allocation, no lock, no syscall, no
  exception, on the real-time thread. `CabinetNode::process()` follows the exact
  control/audio-thread split `Jcm800::process()` already uses for its cabinet stage.
- Build and test only through `sd build` / `sd test`; never `cmake`/`ctest` directly. Do not
  run a build or test without the user's go-ahead per session convention — the acceptance
  criterion for each task below names the exact `sd` command to hand to the user.
- Every subagent prompt for this plan opens with the four lines the user's global
  CLAUDE.md requires: SOLID-brick discipline, the `humanizer` skill on any prose written,
  never invoke `sd`/`cmake`/`ninja`/`ctest` and never edit build configuration files, and
  report what was done and what wasn't with every verification's output pasted.

---

## Wave 1 — the new node (no dependents yet)

### Task 1: `CabinetNode` — a standalone IR-convolving node

**Files:**
- Create: `modules/cabinet/include/SushiDSP/cabinet/CabinetNode.hpp`
- Create: `modules/cabinet/src/CabinetNode.cpp`
- Create: `modules/cabinet/CMakeLists.txt`
- Modify: `CMakeLists.txt:57` — insert `add_subdirectory(modules/cabinet)` after the
  `modules/room` line.
- Test: Create `tests/functional/unit/test_cabinet_node.cpp`
- Modify: `tests/CMakeLists.txt` — add the new test file to the unit-test source list
  (alongside `functional/unit/test_jcm800.cpp`, line 84) and add `SushiDSP::cabinet` to the
  test executable's `target_link_libraries` list (alongside `SushiDSP::jcm800`, line 202).

**Interfaces:**
- Produces: `SushiDSP::CabinetNode` (`IAudioNode`, `ICabinetHost`), `const NodeDescriptor&
  SushiDSP::cabinet_descriptor()`, `type_id() == "sushidsp.cabinet"`, mono
  `PortLayout{1, 1}`, no parameters (`parameter_count == 0`, matching `SplitterNode`).
- Consumes: `SushiDSP::ICabinetHost` (`include/SushiDSP/core/ICabinetHost.hpp`),
  `SushiDSP::Ir` (`include/SushiDSP/io/Ir.hpp`), `SushiDSP::StreamingConvolution`
  (`include/SushiDSP/math/StreamingConvolution.hpp`), `SushiDSP::RtSwapSlot<T>`
  (`include/SushiDSP/core/RtSwapSlot.hpp`), `SushiDSP::NodeDescriptor`/`ParamKind`
  (`include/SushiDSP/core/NodeDescriptor.hpp`).

- [ ] **Step 1: Write the failing tests**

```cpp
/**************************************************************************/
/* test_cabinet_node.cpp                                                 */
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
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/cabinet/CabinetNode.hpp"

namespace
{
    std::vector<float> make_identity_impulse_response()
    {
        return std::vector<float>{ 1.0f };
    }
}

TEST(Unit_Dsp, CabinetNodeIsAPassthroughWithNoCabinetInstalled)
{
    SushiDSP::CabinetNode node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);

    std::vector<float> input(256);
    for (std::uint32_t i = 0; i < input.size(); ++i)
    {
        input[i] = std::sin(static_cast<float>(i) * 0.1f);
    }
    std::vector<float> output(256, 0.0f);
    const float* inputs[1] = { input.data() };
    float* outputs[1] = { output.data() };
    SushiDSP::ProcessContext context{ outputs, inputs, static_cast<std::uint32_t>(input.size()), 0.0 };

    node.process(context);

    for (std::size_t i = 0; i < input.size(); ++i)
    {
        EXPECT_FLOAT_EQ(output[i], input[i]);
    }
    EXPECT_FALSE(node.has_cabinet());
}

TEST(Unit_Dsp, CabinetNodeInstallsAndAppliesAnImpulseResponse)
{
    SushiDSP::CabinetNode node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);

    EXPECT_FALSE(node.has_cabinet());
    EXPECT_TRUE(node.set_cabinet(SushiDSP::Ir{ make_identity_impulse_response(), 48000.0 }));
    EXPECT_TRUE(node.has_cabinet());
    node.reclaim_cabinets();

    std::vector<float> input(256, 1.0f);
    std::vector<float> output(256, 0.0f);
    const float* inputs[1] = { input.data() };
    float* outputs[1] = { output.data() };
    SushiDSP::ProcessContext context{ outputs, inputs, static_cast<std::uint32_t>(input.size()), 0.0 };

    node.process(context);

    // An identity IR makes the cabinet a pure delay of one partition; every sample that has
    // seen the delayed input by now must reproduce it exactly.
    const std::uint32_t latency = node.latency_samples();
    for (std::uint32_t i = latency; i < input.size(); ++i)
    {
        EXPECT_FLOAT_EQ(output[i], 1.0f);
    }
}

TEST(Unit_Dsp, CabinetNodeRejectsAnImpulseResponseAtTheWrongSampleRate)
{
    SushiDSP::CabinetNode node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);

    EXPECT_FALSE(node.set_cabinet(SushiDSP::Ir{ make_identity_impulse_response(), 44100.0 }));
    EXPECT_FALSE(node.has_cabinet());
}

TEST(Unit_Dsp, CabinetNodeRejectsAnImpulseResponseBeforePrepare)
{
    SushiDSP::CabinetNode node;

    EXPECT_FALSE(node.set_cabinet(SushiDSP::Ir{ make_identity_impulse_response(), 48000.0 }));
    EXPECT_FALSE(node.has_cabinet());
}

TEST(Unit_Dsp, CabinetNodeDescriptorMatchesTypeId)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::cabinet_descriptor();
    EXPECT_STREQ(descriptor.id, "sushidsp.cabinet");
    EXPECT_EQ(descriptor.ports.input_channels, 1u);
    EXPECT_EQ(descriptor.ports.output_channels, 1u);
    EXPECT_EQ(descriptor.parameter_count, 0u);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    auto node = descriptor.make(info);
    ASSERT_NE(node, nullptr);
    EXPECT_STREQ(node->type_id(), "sushidsp.cabinet");
}
```

- [ ] **Step 2: Run tests to verify they fail to compile (the class does not exist yet)**

Do not run this yourself — hand the exact command to the user, since building/testing needs
their go-ahead: `sd test --filter Unit_Dsp.CabinetNode*`
Expected: build failure, `CabinetNode.hpp` not found.

- [ ] **Step 3: Write `CabinetNode.hpp`**

```cpp
/**************************************************************************/
/* CabinetNode.hpp                                                       */
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

#include <cstdint>
#include <vector>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/ICabinetHost.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"
#include "SushiDSP/core/RtSwapSlot.hpp"
#include "SushiDSP/math/StreamingConvolution.hpp"

namespace SushiDSP
{
    /// @brief A standalone cabinet impulse-response convolver. Mono in, mono out; a
    /// passthrough until set_cabinet() installs an impulse response.
    class CabinetNode : public IAudioNode, public ICabinetHost
    {
    public:
        void prepare(const PrepareInfo& info) override;
        void process(ProcessContext& context) override;
        PortLayout ports() const override;
        void reset() override;
        float get_parameter(const char* id) const override;
        void set_parameter(const char* id, float value) override;
        std::uint32_t latency_samples() const override;

        /// @brief This node's registered type id.
        /// @return "sushidsp.cabinet".
        const char* type_id() const override;

        bool set_cabinet(Ir impulse_response) override;
        void reclaim_cabinets() override;
        bool has_cabinet() const override;

    private:
        double sample_rate_ = 48000.0;
        std::uint32_t cabinet_partition_ = 0;
        std::vector<float> cabinet_impulse_response_;
        RtSwapSlot<StreamingConvolution> cabinet_slot_;
    };

    /// @brief The cabinet's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.cabinet".
    const NodeDescriptor& cabinet_descriptor();
}
```

- [ ] **Step 4: Write `CabinetNode.cpp`**

```cpp
/**************************************************************************/
/* CabinetNode.cpp                                                       */
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
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#include "SushiDSP/cabinet/CabinetNode.hpp"

#include <memory>

namespace SushiDSP
{
    void CabinetNode::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;
        cabinet_partition_ = StreamingConvolution::partition_size_for(sample_rate_);
        if (!cabinet_impulse_response_.empty())
        {
            cabinet_slot_.publish(std::make_unique<StreamingConvolution>(
                cabinet_partition_, cabinet_impulse_response_.data(),
                static_cast<std::uint32_t>(cabinet_impulse_response_.size()), info.max_block));
        }
    }

    void CabinetNode::process(ProcessContext& context)
    {
        const float* input = context.inputs != nullptr ? context.inputs[0] : nullptr;
        float* output = context.outputs[0];

        StreamingConvolution* const cabinet = cabinet_slot_.acquire_for_audio_thread();
        if (cabinet != nullptr && input != nullptr)
        {
            cabinet->process(input, output, context.frames);
            return;
        }
        for (std::uint32_t i = 0; i < context.frames; ++i)
        {
            output[i] = input != nullptr ? input[i] : 0.0f;
        }
    }

    PortLayout CabinetNode::ports() const { return { 1, 1 }; }

    void CabinetNode::reset()
    {
        if (StreamingConvolution* const cabinet = cabinet_slot_.active_for_publisher())
        {
            cabinet->reset();
        }
    }

    float CabinetNode::get_parameter(const char*) const { return 0.0f; }
    void CabinetNode::set_parameter(const char*, float) {}

    std::uint32_t CabinetNode::latency_samples() const
    {
        return has_cabinet() ? cabinet_partition_ : 0;
    }

    const char* CabinetNode::type_id() const { return "sushidsp.cabinet"; }

    bool CabinetNode::set_cabinet(Ir impulse_response)
    {
        if (impulse_response.samples.empty() || cabinet_partition_ == 0
            || impulse_response.sample_rate != sample_rate_)
        {
            return false;
        }
        cabinet_impulse_response_ = std::move(impulse_response.samples);
        cabinet_slot_.publish(std::make_unique<StreamingConvolution>(
            cabinet_partition_, cabinet_impulse_response_.data(),
            static_cast<std::uint32_t>(cabinet_impulse_response_.size()), cabinet_partition_));
        return true;
    }

    void CabinetNode::reclaim_cabinets()
    {
        cabinet_slot_.reclaim();
    }

    bool CabinetNode::has_cabinet() const
    {
        return cabinet_slot_.has_active();
    }

    namespace
    {
        std::unique_ptr<IAudioNode> make_cabinet(const PrepareInfo&)
        {
            return std::make_unique<CabinetNode>();
        }
    }

    const NodeDescriptor& cabinet_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.cabinet",
            "Cabinet",
            nullptr,
            0,
            PortLayout{ 1, 1 },
            &make_cabinet,
        };
        return descriptor;
    }
}
```

Note on `set_cabinet`'s `max_block` argument: unlike `Jcm800` (which tracks the host's
`max_block` in a scratch buffer sized at `prepare()`), `CabinetNode` has no scratch buffer
to size — it convolves directly from `context.inputs[0]` into `context.outputs[0]`, so
`StreamingConvolution`'s internal re-blocking only needs a `max_block` upper bound. Passing
`cabinet_partition_` here is wrong if a later `prepare()` raises `PrepareInfo::max_block`
past the partition size; store `max_block_` alongside `cabinet_partition_` in `prepare()`
and pass that instead. Fix this before Step 5: add `std::uint32_t max_block_ = 0;` as a
private member, set it in `prepare()`, and use it in both the `prepare()` and `set_cabinet()`
`StreamingConvolution` constructions in place of `cabinet_partition_`/literal values.

- [ ] **Step 5: Write `modules/cabinet/CMakeLists.txt`**

```cmake
add_library(cabinet STATIC
    src/CabinetNode.cpp
)
add_library(SushiDSP::cabinet ALIAS cabinet)
target_link_libraries(cabinet PUBLIC SushiDSP::sushi_dsp)
sushidsp_apply_optimization_flags(cabinet)
target_include_directories(cabinet PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
```

- [ ] **Step 6: Wire the new module into the top-level build**

In `CMakeLists.txt`, after line 57 (`add_subdirectory(modules/room)`), add:
```cmake
add_subdirectory(modules/cabinet)
```

In `tests/CMakeLists.txt`: add `functional/unit/test_cabinet_node.cpp` to the unit-test
source list next to `functional/unit/test_jcm800.cpp` (line 84), and add
`SushiDSP::cabinet` to the test executable's `target_link_libraries` next to
`SushiDSP::jcm800` (line 202).

- [ ] **Step 7: Run the tests and verify they pass**

Hand the user this exact command: `sd test --filter Unit_Dsp.CabinetNode*`
Expected: 5/5 tests pass — `CabinetNodeIsAPassthroughWithNoCabinetInstalled`,
`CabinetNodeInstallsAndAppliesAnImpulseResponse`,
`CabinetNodeRejectsAnImpulseResponseAtTheWrongSampleRate`,
`CabinetNodeRejectsAnImpulseResponseBeforePrepare`, `CabinetNodeDescriptorMatchesTypeId`.
Paste the full output before moving on.

- [ ] **Step 8: Commit**

```bash
git add modules/cabinet CMakeLists.txt tests/CMakeLists.txt tests/functional/unit/test_cabinet_node.cpp
git commit -m "Add sushidsp.cabinet, a standalone cabinet IR node

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_0194iyViCa7vt5xWm1YKeveX"
```

---

## Wave 2 — register the node, retire the amp-embedded path (parallel, disjoint files)

Every task in this wave depends only on Wave 1 (the `CabinetNode` class existing) and each
touches a disjoint file set, so all five can run in parallel.

### Task 2: Register `sushidsp.cabinet` in the node registry

**Files:**
- Modify: `modules/node_registry/src/NodeRegistry.cpp`
- Modify: `modules/node_registry/CMakeLists.txt`

**Interfaces:**
- Consumes: `SushiDSP::cabinet_descriptor()` from Task 1.

- [ ] **Step 1: Add the include and registry entry**

In `modules/node_registry/src/NodeRegistry.cpp`, add `#include
"SushiDSP/cabinet/CabinetNode.hpp"` alongside the other module includes (after line 28,
keeping the block's existing name order), and add one line to the `entries` vector body
(after the `room_descriptor()` line, `NodeRegistry.cpp:53`):
```cpp
{ &cabinet_descriptor(), NodeCategory{ "Cabinet" } },
```

- [ ] **Step 2: Link the new module**

In `modules/node_registry/CMakeLists.txt`, add `SushiDSP::cabinet` to the
`target_link_libraries` line, alongside the existing `SushiDSP::room` entry.

- [ ] **Step 3: Write the failing registry test**

Add to `tests/functional/unit/test_node_registry.cpp` (follow that file's existing test
shape — one `TEST(Unit_Dsp, ...)` per registered product, asserting `make_node("sushidsp.X",
info)` returns non-null and the constructed node's `type_id()` matches):

```cpp
TEST(Unit_Dsp, NodeRegistryBuildsTheCabinet)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    auto node = SushiDSP::make_node("sushidsp.cabinet", info);
    ASSERT_NE(node, nullptr);
    EXPECT_STREQ(node->type_id(), "sushidsp.cabinet");
}
```

- [ ] **Step 4: Run and verify**

Hand the user: `sd test --filter Unit_Dsp.NodeRegistryBuildsTheCabinet`
Expected: PASS. Paste the output.

- [ ] **Step 5: Commit**

```bash
git add modules/node_registry tests/functional/unit/test_node_registry.cpp
git commit -m "Register sushidsp.cabinet in the node registry

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_0194iyViCa7vt5xWm1YKeveX"
```

### Task 3: Retire the embedded cabinet in `Jcm800`

**Files:**
- Modify: `modules/jcm800/include/SushiDSP/jcm800/Jcm800.hpp`
- Modify: `modules/jcm800/src/Jcm800.cpp`
- Modify: `tests/functional/unit/test_jcm800.cpp`

**Interfaces:**
- Produces: `Jcm800`'s constructor drops its `cabinet_impulse_response` parameter —
  `Jcm800();` (no arguments). No other call site in the tree passes one (confirmed:
  `apps/host/main.cpp:66` only mentions it in a comment).

- [ ] **Step 1: Remove `ICabinetHost` from the class**

In `Jcm800.hpp`:
- Line 52: `class Jcm800 : public IAudioNode, public ICabinetHost` → `class Jcm800 : public
  IAudioNode`. Remove the now-unneeded `#include "SushiDSP/core/ICabinetHost.hpp"` if it is
  the only reason that header is included (check for other `Ir`/`ICabinetHost` uses first).
- Line 56: `explicit Jcm800(std::vector<float> cabinet_impulse_response = {});` → `Jcm800();`
- Lines 86-104: delete the three `ICabinetHost` override declarations (`set_cabinet`,
  `reclaim_cabinets`, `has_cabinet`) and their doc comments.
- Lines 187-188, 222: delete `cabinet_partition_`, `cabinet_impulse_response_`, and
  `RtSwapSlot<StreamingConvolution> cabinet_slot_`. Remove the now-unneeded
  `#include "SushiDSP/math/StreamingConvolution.hpp"` and `RtSwapSlot.hpp` includes if
  nothing else in the file uses them.

- [ ] **Step 2: Remove the cabinet stage from the implementation**

In `Jcm800.cpp`:
- Constructor (`:257-275`): drop the `cabinet_impulse_response_(std::move(...))` member
  initializer and the parameter.
- `prepare()` (`:342-359`): delete the `cabinet_partition_ = ...` line and the `if
  (!cabinet_impulse_response_.empty()) { cabinet_slot_.publish(...); }` block.
- `process()` (`:434-454`): the loop that applies `presence_shelf_`/`master_gain` into
  `scratch_base_` stays; replace the `cabinet_slot_.acquire_for_audio_thread()` /
  `cabinet->process(...)` / else-copy block with a direct copy of `scratch_base_` into
  `output` for `context.frames` samples — `Jcm800` now always hands its dry (post-presence,
  post-master) signal straight to `output`.
- Delete `set_cabinet()`, `reclaim_cabinets()`, `has_cabinet()` (`:476-507`) in full.
- `latency_samples()` (`:468-472`): drop the `+ (has_cabinet() ? cabinet_partition_ : 0)`
  term — it returns `oversampling_latency` alone now.
- `reset()`'s tail (`:514-540`): delete the `if (StreamingConvolution* const cabinet =
  cabinet_slot_.active_for_publisher()) { cabinet->reset(); }` block.

- [ ] **Step 3: Remove the now-obsolete cabinet tests, run the rest**

Delete these `TEST(Unit_Dsp, ...)` blocks from `test_jcm800.cpp` in full (they test the
removed embedded path — their behavior is now covered by
`tests/functional/unit/test_cabinet_node.cpp` from Task 1):
`Jcm800CabinetStageEngagesAtANonDefaultBlockSize`, `Jcm800CabinetSurvivesVariableBlockSizes`,
`Jcm800RejectsACabinetImpulseResponseAtTheWrongSampleRate`,
`Jcm800RejectsACabinetBeforePrepare`. Search the file for any other test that constructs
`Jcm800(...)` with a non-empty vector argument or calls `.set_cabinet`/`.has_cabinet`/
`.reclaim_cabinets` on a `Jcm800` instance and update it to construct `Jcm800()` with no
argument and drop the cabinet assertions — the amp itself no longer has an opinion about
cabinets.

Hand the user: `sd test --filter Unit_Dsp.Jcm800*`
Expected: every remaining `Jcm800*` test passes; none of the four deleted names appear in
the output. Paste it.

- [ ] **Step 4: Commit**

```bash
git add modules/jcm800 tests/functional/unit/test_jcm800.cpp
git commit -m "Retire Jcm800's embedded cabinet in favor of sushidsp.cabinet

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_0194iyViCa7vt5xWm1YKeveX"
```

### Task 4: Retire the embedded cabinet in `Peavey5150`

**Files:**
- Modify: `modules/peavey5150/include/SushiDSP/peavey5150/Peavey5150.hpp`
- Modify: `modules/peavey5150/src/Peavey5150.cpp`
- Modify: `tests/functional/unit/test_peavey5150.cpp` (if cabinet tests exist there —
  search for `cabinet`/`Cabinet` first, same as Task 3 Step 3's search)

**Interfaces:**
- Produces: `Peavey5150();` (no-argument constructor).

- [ ] **Step 1: Apply the same structural removal as Task 3, Step 1-2, to this file**

`Peavey5150.hpp:60` has the identical `explicit Peavey5150(std::vector<float>
cabinet_impulse_response = {});` shape as `Jcm800.hpp:56`; its `ICabinetHost` overrides and
its `cabinet_partition_`/`cabinet_impulse_response_`/`RtSwapSlot<StreamingConvolution>
cabinet_slot_` members sit at the analogous position (declared at lines 195, 196, 251 per
the code survey). Remove `public ICabinetHost` from the class declaration, the constructor
parameter, the three override declarations and their bodies in the `.cpp`
(`set_cabinet`/`reclaim_cabinets`/`has_cabinet`), the cabinet-publish block in `prepare()`,
the cabinet-acquire-and-convolve block in `process()` (replaced by a direct copy to
`output`, exactly as in Task 3), the `cabinet_slot_.active_for_publisher()` reset in
`reset()`, and any `has_cabinet()` term inside `latency_samples()`.

- [ ] **Step 2: Search for and remove/update cabinet-specific tests**

`grep -n "cabinet\|Cabinet" tests/functional/unit/test_peavey5150.cpp` first — delete or
rewrite any test that constructs `Peavey5150(...)` with a non-empty IR vector or calls
`.set_cabinet`/`.has_cabinet`/`.reclaim_cabinets`, following the same pattern as Task 3
Step 3.

- [ ] **Step 3: Run and verify**

Hand the user: `sd test --filter Unit_Dsp.Peavey5150*`
Expected: every remaining test passes, no cabinet-specific test remains. Paste the output.

- [ ] **Step 4: Commit**

```bash
git add modules/peavey5150 tests/functional/unit/test_peavey5150.cpp
git commit -m "Retire Peavey5150's embedded cabinet in favor of sushidsp.cabinet

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_0194iyViCa7vt5xWm1YKeveX"
```

### Task 5: Retire the embedded cabinet in `DiezelVh4`

**Files:**
- Modify: `modules/diezel_vh4/include/SushiDSP/diezel_vh4/DiezelVh4.hpp`
- Modify: `modules/diezel_vh4/src/DiezelVh4.cpp`
- Modify: `tests/functional/unit/test_diezel_vh4.cpp` (if cabinet tests exist there)

**Interfaces:**
- Produces: `DiezelVh4();` (no-argument constructor).

- [ ] **Step 1: Apply the same structural removal as Task 3, Step 1-2, to this file**

`DiezelVh4.hpp:69` has the identical constructor shape; its `ICabinetHost` overrides and
`cabinet_partition_`/`cabinet_impulse_response_`/`RtSwapSlot<StreamingConvolution>
cabinet_slot_` members sit at lines 199, 200, 231. Apply the same removals described in
Task 4, Step 1, to this file's declaration and implementation.

- [ ] **Step 2: Search for and remove/update cabinet-specific tests**

`grep -n "cabinet\|Cabinet" tests/functional/unit/test_diezel_vh4.cpp` first, then follow
Task 4 Step 2's procedure.

- [ ] **Step 3: Run and verify**

Hand the user: `sd test --filter Unit_Dsp.DiezelVh4*`
Expected: every remaining test passes, no cabinet-specific test remains. Paste the output.

- [ ] **Step 4: Commit**

```bash
git add modules/diezel_vh4 tests/functional/unit/test_diezel_vh4.cpp
git commit -m "Retire DiezelVh4's embedded cabinet in favor of sushidsp.cabinet

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_0194iyViCa7vt5xWm1YKeveX"
```

### Task 6: Retire the embedded cabinet in `MesaMark2c`

**Files:**
- Modify: `modules/mesa_mark2c/include/SushiDSP/mesa_mark2c/MesaMark2c.hpp`
- Modify: `modules/mesa_mark2c/src/MesaMark2c.cpp`
- Modify: `tests/functional/unit/test_mesa_mark2c.cpp` (if cabinet tests exist there)

**Interfaces:**
- Produces: `MesaMark2c();` (no-argument constructor).

- [ ] **Step 1: Apply the same structural removal as Task 3, Step 1-2, to this file**

`MesaMark2c.hpp:68` has the identical constructor shape; its `ICabinetHost` overrides and
`cabinet_partition_`/`cabinet_impulse_response_`/`RtSwapSlot<StreamingConvolution>
cabinet_slot_` members sit at lines 200, 201, 253. Apply the same removals described in
Task 4, Step 1, to this file's declaration and implementation.

- [ ] **Step 2: Search for and remove/update cabinet-specific tests**

`grep -n "cabinet\|Cabinet" tests/functional/unit/test_mesa_mark2c.cpp` first, then follow
Task 4 Step 2's procedure.

- [ ] **Step 3: Run and verify**

Hand the user: `sd test --filter Unit_Dsp.MesaMark2c*`
Expected: every remaining test passes, no cabinet-specific test remains. Paste the output.

- [ ] **Step 4: Commit**

```bash
git add modules/mesa_mark2c tests/functional/unit/test_mesa_mark2c.cpp
git commit -m "Retire MesaMark2c's embedded cabinet in favor of sushidsp.cabinet

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_0194iyViCa7vt5xWm1YKeveX"
```

---

## Wave 3 — RigController-level tests (depends on Waves 1-2)

### Task 7: Update `RigController`'s cabinet tests for the standalone node

`apps/host_gui/RigController.cpp`/`.hpp` need **no source change** — `set_cabinet`,
`cabinet_path`, `hosts_cabinet` and `apply_cabinet_to_live_node` already operate by
`dynamic_cast<ICabinetHost*>` on whatever node occupies a slot (verified at
`RigController.cpp:675-727`), and `apps/host_gui/ui/StagePanel.cpp`'s cabinet controls
already gate on `rig.hosts_cabinet(slot)` (`StagePanel.cpp:95,119,152,269`) rather than on
a specific node type. Only the tests' fixture shape changes, since an amp slot alone no
longer hosts a cabinet.

**Files:**
- Modify: `tests/functional/unit/test_rig_controller.cpp`

**Interfaces:**
- Consumes: `RigController::append("sushidsp.cabinet")`,
  `RigController::set_cabinet(SlotId, const std::string&)`,
  `RigController::hosts_cabinet(SlotId) const`, `RigController::connect(SlotId, SlotId,
  bool)` — all pre-existing, unchanged signatures.

- [ ] **Step 1: Rewrite `RigControllerSaveAndLoadJsonRoundTripsCabinetIr`**

Replace the body (currently `test_rig_controller.cpp:623-639`) so the cabinet lives on a
`sushidsp.cabinet` node placed after the amp, not on the amp slot itself:

```cpp
TEST(Unit_Dsp, RigControllerSaveAndLoadJsonRoundTripsCabinetIr)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    const std::string ir_path = write_temp_impulse_wav();

    RigController original(info);
    const RigController::SlotId amp = original.append("sushidsp.jcm800");
    const RigController::SlotId cabinet = original.append("sushidsp.cabinet");
    ASSERT_TRUE(original.set_cabinet(cabinet, ir_path));

    const std::string json_text = original.save_to_json();

    RigController reloaded(info);
    reloaded.load_from_json(json_text);

    const auto order = reloaded.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(reloaded.cabinet_path(order[1]), ir_path);

    std::remove(ir_path.c_str());
}
```

- [ ] **Step 2: Replace `RigControllerLoadsACabinetOnAnAmplifierThatIsNotTheJcm800`**

This test's premise (an amp other than `Jcm800` hosts a cabinet) no longer holds — no amp
hosts one. Replace it with a test naming the new invariant directly:

```cpp
TEST(Unit_Dsp, RigControllerRefusesACabinetOnAnAmplifierSlot)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    const std::string ir_path = write_temp_impulse_wav();

    RigController controller(info);
    const RigController::SlotId amp = controller.append("sushidsp.peavey5150");
    EXPECT_FALSE(controller.set_cabinet(amp, ir_path));
    EXPECT_EQ(controller.cabinet_path(amp), "");
    EXPECT_FALSE(controller.hosts_cabinet(amp));

    std::remove(ir_path.c_str());
}
```

- [ ] **Step 3: Leave `RigControllerRefusesACabinetOnANodeThatHostsNone` as-is**

It already asserts `sushidsp.dd3` refuses a cabinet, which still holds unchanged — no edit.

- [ ] **Step 4: Run and verify**

Hand the user: `sd test --filter Unit_Dsp.RigController*Cabinet*`
Expected: `RigControllerSaveAndLoadJsonRoundTripsCabinetIr`,
`RigControllerRefusesACabinetOnAnAmplifierSlot`, and
`RigControllerRefusesACabinetOnANodeThatHostsNone` all pass. Paste the output.

- [ ] **Step 5: Commit**

```bash
git add tests/functional/unit/test_rig_controller.cpp
git commit -m "Point RigController's cabinet tests at sushidsp.cabinet

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_0194iyViCa7vt5xWm1YKeveX"
```

---

## Documentation (part of the same change, not a follow-up)

### Task 8: Update the manual and changelog

**Files:**
- Modify: `docs/README.md` — add a `modules/cabinet/README.md` line next to the
  `modules/routing/` entry (§ list of module READMEs).
- Create: `modules/cabinet/README.md` — one short module README, the module's own facts:
  what `CabinetNode` does, that it replaces the amp-embedded `ICabinetHost` path, and a
  pointer to `docs/design/gui_ux_redesign.md` §8 for the rationale.
- Modify: `docs/reference/CHANGELOG.md` — one line, past tense, per this project's
  changelog format: `- 2026-09-10 — Added sushidsp.cabinet and retired the amp-embedded
  cabinet path (\`modules/cabinet\`, \`modules/jcm800\`, \`modules/peavey5150\`,
  \`modules/diezel_vh4\`, \`modules/mesa_mark2c\`).`
- Modify: `docs/design/REMAINING_WORK.md` — remove the cabinet-node bullet from the GUI/UX
  redesign entry once this plan's waves are done (the remaining GUI-layout entry stays,
  tracked by the second plan).

- [ ] **Step 1: Write `modules/cabinet/README.md`, update `docs/README.md`, append the
  changelog line, trim `REMAINING_WORK.md`'s entry**

- [ ] **Step 2: Commit**

```bash
git add modules/cabinet/README.md docs/README.md docs/reference/CHANGELOG.md docs/design/REMAINING_WORK.md
git commit -m "Document sushidsp.cabinet

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_0194iyViCa7vt5xWm1YKeveX"
```

---

## Self-Review

1. **Spec coverage:** `docs/design/gui_ux_redesign.md` §8's decision (2026-09-10) — the
   only cabinet path is `sushidsp.cabinet`, every amp drops `ICabinetHost` — is covered by
   Tasks 1-6. The GearBrowser auto-listing (§3's gear grid) needs no task: `GearBrowser.cpp`
   already reads `node_registry()` live (confirmed in the code survey), so registering
   `sushidsp.cabinet` in Task 2 is sufficient.
2. **Placeholder scan:** no TBD/TODO; every step carries real code or a named, greppable
   search-and-replace instruction (Tasks 4-6 point at exact line numbers and exact member
   names rather than saying "similarly").
3. **Type consistency:** `CabinetNode::set_cabinet`, `::has_cabinet`, `::reclaim_cabinets`
   match `ICabinetHost`'s signatures exactly (Task 1); `RigController`'s calls to them go
   through the unchanged `ICabinetHost*` interface pointer, so no signature drift is
   possible between tasks.
