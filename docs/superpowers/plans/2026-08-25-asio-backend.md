# ASIO Audio Backend Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give SushiDSP a duplex, low-latency ASIO device backend so a real guitar signal
(via a Roland Rubix22 or any ASIO-capable interface) can be captured, routed live through a
rig, and played back — not just a synthetic tone or offline file.

**Architecture:** `IAudioDevice` becomes duplex (input + output). `RigGraph` gains the
ability to feed its own live input into any node that has no upstream connection (a "root"
node) — a real gap found while writing this plan, not present in the original design spec's
"no RigGraph change needed" claim; see Task 1. `AsioAudioDevice` (SDK-gated, Windows-only,
optional CMake target) wraps the classic Steinberg ASIO C callback API behind that seam. A
new `apps/host_asio` executable is the actual playable program.

**Tech Stack:** C++17, CMake, the Steinberg ASIO SDK 2.3 (user-vendored, not committed),
GoogleTest.

**Spec:** `docs/superpowers/specs/2026-08-25-asio-backend-design.md`

## Global Constraints

- `IAudioNode::process()` and everything it calls: allocation-free, lock-free, syscall-free,
  throw-free (project-wide RT-safety bar, enforced by `operator new`-trap tests in
  `tests/functional/unit/test_rt_safety.cpp`).
- Never invoke cmake/ctest directly — all builds/tests go through `sd build`/`sd test`.
- Apache 2.0 boxed license header, exactly 76 columns per line, on every new `.hpp`/`.cpp`,
  copied verbatim from an existing file (e.g. `src/device/Sdl2AudioDevice.cpp`) — never
  retyped.
- C++17, Allman braces, PascalCase types, snake_case functions/members, trailing-underscore
  private members, flat `namespace SushiDSP`, Doxygen `@brief`/`@param`/`@return` on public
  API.
- No dead code, no unused variables/functions, no speculative/YAGNI scope beyond what a task
  asks for.
- The ASIO SDK (`third_party/asio/`) must never be committed — already `.gitignore`d.
- `AsioAudioDevice` and `apps/host_asio` are built but never *run* by an agent — they open
  real audio hardware.
- CI (`.github/workflows/ci.yml`) needs no change: it never sets `ASIO_SDK_DIR`, so the ASIO
  target is always silently skipped there (same pattern `SUSHIDSP_BUILD_SDL2` already uses
  for missing dependencies).

---

### Task 1: `RigGraph` root-node live-input passthrough

**Why this task exists:** the design spec assumed "`RigGraph` needs no change — the device
callback simply forwards its own `inputs`/`outputs`/`frames` into a `ProcessContext`." That's
true for the *outer* `RigGraph::process()` call, but `RigGraph::process()` currently only
ever gives a node's own `context.inputs` to nodes that have an *upstream edge* in the graph —
a node with no incoming edge (a "root" node, e.g. a `TubeScreamer` placed first in the chain
with no `SineToneNode` ahead of it) always receives `nullptr`, regardless of what the caller
passed as the graph's own `context.inputs`. `apps/host_asio` (Task 5) needs exactly this: a
root `TubeScreamer` node that receives the *live device input* instead of a synthetic source
node feeding it. This task closes that gap in `RigGraph` itself, with its own test, before
anything ASIO-specific is built.

**Files:**
- Modify: `src/core/RigGraph.cpp` (the `process()` method)
- Test: `tests/functional/unit/test_rig_graph.cpp` (add one test, reuse the existing `GainNode`
  helper already defined there)

**Interfaces:**
- No signature changes. `RigGraph::process(ProcessContext& context)` behavior changes only
  for nodes with zero incoming edges: they now receive `context.inputs` (the graph's own
  live input) instead of always `nullptr`. When `context.inputs == nullptr` (the existing
  SDL2/no-input path), root nodes still receive `nullptr` — unchanged, byte-for-byte
  backward compatible with every existing test and with `apps/host`'s SDL2 device.

- [ ] **Step 1: Write the failing test**

Add to `tests/functional/unit/test_rig_graph.cpp` (after the existing two `TEST`s, using the
file's existing `GainNode` helper — no new helper needed):

```cpp
TEST(Unit_Dsp, RigGraphFeedsItsOwnInputToARootNodeWithNoUpstreamEdge)
{
    SushiDSP::RigGraph graph(256, 4);
    auto gain_handle = graph.add_node(std::make_unique<GainNode>(4.0f));
    // No connect() call: gain_handle has no upstream edge, so it is a root node.

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    std::vector<float> input_buffer(256, 2.0f);
    std::vector<float> output_buffer(256, 0.0f);
    const float* input_channels[1] = { input_buffer.data() };
    float* output_channels[1] = { output_buffer.data() };

    SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 8.0f);
    }
}

TEST(Unit_Dsp, RigGraphRootNodeStillReceivesNullptrWhenGraphHasNoInput)
{
    // Unchanged behavior: a root node must not crash or misbehave when the graph itself has
    // no live input (e.g. the existing SDL2 output-only path) — GainNode would dereference a
    // null context.inputs, so this uses ConstantSourceNode (0 input ports) to prove the
    // *graph* passes nullptr through, without the test node needing to read it.
    SushiDSP::RigGraph graph(256, 4);
    auto source_handle = graph.add_node(std::make_unique<ConstantSourceNode>(5.0f));

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = { output_buffer.data() };

    SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 5.0f);
    }
}
```

- [ ] **Step 2: Run the tests to verify the first one fails**

Run: `sd test`
Expected: `RigGraphFeedsItsOwnInputToARootNodeWithNoUpstreamEdge` FAILS (every sample is 0,
since `GainNode::process` reads `context.inputs[0][frame]` and currently gets a null
`inputs` array, which the test's `GainNode` would actually crash on — if it crashes instead
of failing an assertion, that itself confirms the current behavior is "root nodes get
nullptr"). `RigGraphRootNodeStillReceivesNullptrWhenGraphHasNoInput` PASSES already (it
documents already-correct behavior).

- [ ] **Step 3: Fix `RigGraph::process()`**

In `src/core/RigGraph.cpp`, replace the body of the `for (NodeHandle handle : topological_order_)` loop's input-resolution logic. Current code:

```cpp
            const float* upstream_input = nullptr;
            for (const Edge& edge : edges_)
            {
                if (edge.to == handle)
                {
                    upstream_input = node_output[edge.from];
                    break;
                }
            }

            float* output_ptr[1] = { node_buffer };
            const float* input_ptr[1] = { upstream_input };

            ProcessContext node_context{ output_ptr, upstream_input != nullptr ? input_ptr : nullptr, context.frames, context.time_seconds };
```

Replace with:

```cpp
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

            // A node with no upstream edge is a root node. Feed it the graph's own live
            // input (e.g. an ASIO device's captured audio) rather than always nullptr, so a
            // rig can start directly from live input without a synthetic source node.
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
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `sd test`
Expected: PASS — both new tests, plus every pre-existing test (in particular
`RigGraphProcessesTwoNodeChainInOrder` and `RigGraphRecompileAfterAddingNodeProducesUpdatedOutput`,
which pass `context.inputs = nullptr` at the outer `graph.process()` call and use
`ConstantSourceNode` as their root — must still produce identical output).

- [ ] **Step 5: Commit**

```bash
git add src/core/RigGraph.cpp tests/functional/unit/test_rig_graph.cpp
git commit -m "RigGraph: feed a root node the graph's own live input, not always nullptr"
```

---

### Task 2: `IAudioDevice` becomes duplex

**Files:**
- Modify: `include/SushiDSP/core/IAudioDevice.hpp`
- Modify: `include/SushiDSP/device/Sdl2AudioDevice.hpp`, `src/device/Sdl2AudioDevice.cpp`
- Modify: `tests/functional/unit/test_audio_device.cpp`
- Modify: `apps/host/main.cpp`

**Interfaces:**
- Produces: `DeviceFormat` with `input_channels`/`output_channels` (replacing the single
  `channels` field); `DeviceCallback` as
  `std::function<void(const float* const* inputs, float* const* outputs, std::uint32_t frames)>`.
  Every later task in this plan (3, 4, 5) uses this exact shape.
- Consumes: nothing new from earlier tasks.

- [ ] **Step 1: Update `IAudioDevice.hpp`**

Replace the `DeviceFormat` struct and `DeviceCallback` alias in
`include/SushiDSP/core/IAudioDevice.hpp`:

```cpp
    /// @brief The sample rate, block size, and channel counts a device is configured for.
    struct DeviceFormat
    {
        double        sample_rate;
        std::uint32_t block_size;
        std::uint32_t input_channels;
        std::uint32_t output_channels;
    };

    /// @brief Invoked by an IAudioDevice on its real-time thread once per block. Must fill
    /// exactly frames samples of each of outputs[0..output_channels). inputs may be nullptr,
    /// or point to input_channels channels of captured audio, or be non-null with fewer
    /// meaningful channels than input_channels reports (a device with zero input capability
    /// reports input_channels == 0 and always passes nullptr). The callback itself must be
    /// allocation-free, lock-free, syscall-free, and throw-free.
    using DeviceCallback = std::function<void(const float* const* inputs, float* const* outputs, std::uint32_t frames)>;
```

- [ ] **Step 2: Update `Sdl2AudioDevice`**

In `include/SushiDSP/device/Sdl2AudioDevice.hpp`, no signature changes are needed (the class
already just implements the base interface's virtuals) — only `src/device/Sdl2AudioDevice.cpp`
changes.

In `src/device/Sdl2AudioDevice.cpp`, constructor: replace
`desired.channels = static_cast<Uint8>(requested_format.channels);` with
`desired.channels = static_cast<Uint8>(requested_format.output_channels);`, and after
`SDL_OpenAudioDevice` succeeds, replace `format_.channels = obtained.channels;` with:

```cpp
        format_.input_channels = 0;
        format_.output_channels = obtained.channels;
```

In `sdl_callback`, replace `self->format_.channels` (used twice: in the frame-count division
and the `== 1` check) with `self->format_.output_channels`, and update the callback
invocation to pass `nullptr` as the new first argument:

```cpp
    void Sdl2AudioDevice::sdl_callback(void* userdata, unsigned char* stream, int length_bytes)
    {
        auto* self = static_cast<Sdl2AudioDevice*>(userdata);
        auto* samples = reinterpret_cast<float*>(stream);
        const std::uint32_t frames = static_cast<std::uint32_t>(length_bytes) / (self->format_.output_channels * sizeof(float));

        if (!self->callback_)
        {
            std::memset(stream, 0, static_cast<std::size_t>(length_bytes));
            return;
        }

        if (self->format_.output_channels == 1)
        {
            float* channel_pointers[1] = { samples };
            self->callback_(nullptr, channel_pointers, frames);
            return;
        }

        std::memset(stream, 0, static_cast<std::size_t>(length_bytes));
    }
```

- [ ] **Step 3: Update `test_audio_device.cpp`**

Replace `NullAudioDevice::start` and the three `TEST`s in
`tests/functional/unit/test_audio_device.cpp`:

```cpp
        void start(SushiDSP::DeviceCallback callback) override
        {
            std::vector<std::vector<float>> channel_storage(
                format_.output_channels, std::vector<float>(format_.block_size, 0.0f));
            std::vector<float*> channel_pointers(format_.output_channels);
            for (std::uint32_t channel = 0; channel < format_.output_channels; ++channel)
            {
                channel_pointers[channel] = channel_storage[channel].data();
            }

            for (std::uint32_t block = 0; block < blocks_to_run_; ++block)
            {
                callback(nullptr, channel_pointers.data(), format_.block_size);
            }
        }
```

```cpp
TEST(Unit_Dsp, NullAudioDeviceReportsConfiguredFormat)
{
    SushiDSP::DeviceFormat expected{ 48000.0, 256, 0, 1 };
    NullAudioDevice device(expected, 0);

    SushiDSP::DeviceFormat actual = device.format();

    EXPECT_EQ(actual.sample_rate, expected.sample_rate);
    EXPECT_EQ(actual.block_size, expected.block_size);
    EXPECT_EQ(actual.input_channels, expected.input_channels);
    EXPECT_EQ(actual.output_channels, expected.output_channels);
}

TEST(Unit_Dsp, NullAudioDeviceInvokesCallbackOncePerBlock)
{
    NullAudioDevice device({ 48000.0, 256, 0, 1 }, 5);

    int call_count = 0;
    device.start([&call_count](const float* const*, float* const*, std::uint32_t)
    {
        ++call_count;
    });

    EXPECT_EQ(call_count, 5);
}

TEST(Unit_Dsp, NullAudioDeviceCallbackReceivesRequestedFrameCount)
{
    NullAudioDevice device({ 48000.0, 128, 0, 2 }, 1);

    std::uint32_t received_frames = 0;
    std::uint32_t received_channels_nonnull = 0;
    device.start([&received_frames, &received_channels_nonnull](const float* const*, float* const* outputs, std::uint32_t frames)
    {
        received_frames = frames;
        received_channels_nonnull = (outputs[0] != nullptr && outputs[1] != nullptr) ? 2 : 0;
    });

    EXPECT_EQ(received_frames, 128u);
    EXPECT_EQ(received_channels_nonnull, 2u);
}
```

- [ ] **Step 4: Update `apps/host/main.cpp`**

Replace `SushiDSP::DeviceFormat requested{ 48000.0, 256, 1 };` with
`SushiDSP::DeviceFormat requested{ 48000.0, 256, 0, 1 };`.

Replace the `std::printf` format-report call:

```cpp
    std::printf(
        "Opened audio device: %.0f Hz, %u frames/block, %u output channel(s)\n",
        actual.sample_rate, actual.block_size, actual.output_channels);
```

Replace `SushiDSP::PrepareInfo info{ actual.sample_rate, actual.block_size, actual.channels };`
with `SushiDSP::PrepareInfo info{ actual.sample_rate, actual.block_size, actual.output_channels };`.

Replace the `device->start(...)` call's lambda signature:

```cpp
    device->start([&rig](const float* const* inputs, float* const* outputs, std::uint32_t frames)
    {
        SushiDSP::ProcessContext context{ outputs, inputs, frames, 0.0 };
        rig.process(context);
    });
```

(`inputs` will be `nullptr` here since `Sdl2AudioDevice` reports `input_channels == 0` — this
is unchanged behavior, just passed through explicitly now instead of hardcoded.)

- [ ] **Step 5: Run the tests to verify they pass**

Run: `sd test`
Expected: PASS — every test in `test_audio_device.cpp`, plus the full existing suite
(confirms nothing else references the old `DeviceFormat::channels` field or the old
two-argument `DeviceCallback` shape).

Run: `sd build`
Expected: SUCCESS — confirms `apps/host`/`sushidsp_host` still compiles against the new
signature.

- [ ] **Step 6: Commit**

```bash
git add include/SushiDSP/core/IAudioDevice.hpp include/SushiDSP/device/Sdl2AudioDevice.hpp \
        src/device/Sdl2AudioDevice.cpp tests/functional/unit/test_audio_device.cpp \
        apps/host/main.cpp
git commit -m "Make IAudioDevice duplex: split DeviceFormat::channels, add inputs to DeviceCallback"
```

---

### Task 3: `AsioSampleConverter` — SDK-independent sample format conversion

**Why this is its own task, and SDK-independent:** ASIO hardware reports its native sample
type (commonly 32-bit signed integer or 32-bit float, hardware-dependent) via
`ASIOGetChannelInfo`; `AsioAudioDevice` (Task 4) must convert every block to/from this
codebase's `float` in `[-1, 1]`. That conversion logic has no genuine dependency on any ASIO
SDK type — it only needs to know "this hardware channel is N-bit signed int" or "this
channel is 32-bit float," which this task expresses as SushiDSP's own enum, not the SDK's
`ASIOSampleType`. Keeping it SDK-independent means: (a) it compiles and is unit-tested on
every platform and in CI, unlike everything in Task 4, which only builds on Windows with the
SDK present; (b) `AsioAudioDevice` stays "the only class that touches ASIO SDK types," per
this codebase's established one-class-per-third-party-dependency pattern (`Sdl2AudioDevice`
is "the only class that touches SDL2").

**Files:**
- Create: `include/SushiDSP/device/AsioSampleConverter.hpp`, `src/device/AsioSampleConverter.cpp`
- Test: `tests/functional/unit/test_asio_sample_converter.cpp`
- Modify: `CMakeLists.txt` (add `src/device/AsioSampleConverter.cpp` to the `sushi_dsp`
  target's source list — this is core infrastructure, always built, not gated behind
  `SUSHIDSP_BUILD_ASIO`)
- Modify: `tests/CMakeLists.txt` (add the test file)

**Interfaces:**
- Produces: `SushiDSP::AsioSampleFormat` (enum), `SushiDSP::asio_sample_to_float(AsioSampleFormat format, const void* sample_bytes) -> float`,
  `SushiDSP::float_to_asio_sample(AsioSampleFormat format, float value, void* out_sample_bytes) -> void`.
  Task 4 maps the real `ASIOSampleType` (from `ASIOGetChannelInfo`) to `AsioSampleFormat`
  once at device-open time, then calls these two functions per sample on the RT path.
- Consumes: nothing from earlier tasks.

- [ ] **Step 1: Write the failing test**

Create `tests/functional/unit/test_asio_sample_converter.cpp`:

```cpp
/**************************************************************************/
/* test_asio_sample_converter.cpp                                        */
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

#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

#include "SushiDSP/device/AsioSampleConverter.hpp"

TEST(Unit_Dsp, AsioSampleConverterInt32RoundTripsKnownValues)
{
    // Int32LSB: full-scale is +/-2147483648.
    const std::int32_t half_scale_positive = 1073741824;  // 0.5 * INT32_MAX+1
    std::uint8_t bytes[4];
    std::memcpy(bytes, &half_scale_positive, 4);

    const float decoded = SushiDSP::asio_sample_to_float(SushiDSP::AsioSampleFormat::Int32LSB, bytes);
    EXPECT_NEAR(decoded, 0.5f, 1e-6f);

    std::uint8_t re_encoded[4];
    SushiDSP::float_to_asio_sample(SushiDSP::AsioSampleFormat::Int32LSB, 0.5f, re_encoded);
    std::int32_t re_encoded_value = 0;
    std::memcpy(&re_encoded_value, re_encoded, 4);
    EXPECT_NEAR(re_encoded_value, half_scale_positive, 2);  // rounding tolerance
}

TEST(Unit_Dsp, AsioSampleConverterInt32ClampsFullScaleNegative)
{
    const std::int32_t full_scale_negative = -2147483647 - 1;  // INT32_MIN, avoids UB literal
    std::uint8_t bytes[4];
    std::memcpy(bytes, &full_scale_negative, 4);

    const float decoded = SushiDSP::asio_sample_to_float(SushiDSP::AsioSampleFormat::Int32LSB, bytes);
    EXPECT_NEAR(decoded, -1.0f, 1e-6f);
}

TEST(Unit_Dsp, AsioSampleConverterFloat32PassesThroughUnchanged)
{
    const float original = -0.375f;
    std::uint8_t bytes[4];
    std::memcpy(bytes, &original, 4);

    const float decoded = SushiDSP::asio_sample_to_float(SushiDSP::AsioSampleFormat::Float32LSB, bytes);
    EXPECT_FLOAT_EQ(decoded, original);

    std::uint8_t re_encoded[4];
    SushiDSP::float_to_asio_sample(SushiDSP::AsioSampleFormat::Float32LSB, 0.25f, re_encoded);
    float re_encoded_value = 0.0f;
    std::memcpy(&re_encoded_value, re_encoded, 4);
    EXPECT_FLOAT_EQ(re_encoded_value, 0.25f);
}

TEST(Unit_Dsp, AsioSampleConverterInt16RoundTripsKnownValues)
{
    const std::int16_t quarter_scale = 8192;  // 0.25 * 32768
    std::uint8_t bytes[2];
    std::memcpy(bytes, &quarter_scale, 2);

    const float decoded = SushiDSP::asio_sample_to_float(SushiDSP::AsioSampleFormat::Int16LSB, bytes);
    EXPECT_NEAR(decoded, 0.25f, 1e-4f);
}

TEST(Unit_Dsp, AsioSampleConverterFloatToSampleClampsOutOfRangeInput)
{
    // A value outside [-1, 1] (e.g. from a hot upstream signal) must not overflow the
    // integer container - it clamps to the format's full-scale value instead.
    std::uint8_t bytes[4];
    SushiDSP::float_to_asio_sample(SushiDSP::AsioSampleFormat::Int32LSB, 2.0f, bytes);
    std::int32_t decoded_value = 0;
    std::memcpy(&decoded_value, bytes, 4);
    EXPECT_EQ(decoded_value, 2147483647);
}
```

- [ ] **Step 2: Add the test file to `tests/CMakeLists.txt` and run to verify failure**

Add `functional/unit/test_asio_sample_converter.cpp` under the `# --- IO ---` group (next to
`test_wav_file.cpp` — this is core infrastructure, same category).

Run: `sd test`
Expected: FAIL — `SushiDSP/device/AsioSampleConverter.hpp` does not exist.

- [ ] **Step 3: Write `include/SushiDSP/device/AsioSampleConverter.hpp`**

```cpp
#pragma once

#include <cstdint>

namespace SushiDSP
{
    /// @brief The sample container formats this converter supports, independent of any ASIO
    /// SDK type - AsioAudioDevice maps the real ASIOSampleType to this enum once at device-
    /// open time, keeping the ASIO SDK's headers out of every other translation unit that
    /// needs sample conversion (this file compiles and is tested on every platform).
    enum class AsioSampleFormat
    {
        Int16LSB,
        Int32LSB,
        Float32LSB,
    };

    /// @brief Decodes one sample from a hardware buffer into a float in [-1, 1].
    /// @param format The sample's container format.
    /// @param sample_bytes Pointer to the sample's raw bytes (2 or 4, per format).
    /// @return The decoded value.
    float asio_sample_to_float(AsioSampleFormat format, const void* sample_bytes);

    /// @brief Encodes a float in [-1, 1] into a hardware buffer's native format, clamping
    /// values outside that range to the format's full-scale value rather than overflowing.
    /// @param format The destination container format.
    /// @param value The value to encode.
    /// @param out_sample_bytes Destination for the sample's raw bytes (2 or 4, per format).
    void float_to_asio_sample(AsioSampleFormat format, float value, void* out_sample_bytes);
}
```

- [ ] **Step 4: Write `src/device/AsioSampleConverter.cpp`**

```cpp
#include "SushiDSP/device/AsioSampleConverter.hpp"

#include <algorithm>
#include <cstring>

namespace SushiDSP
{
    namespace
    {
        constexpr float kInt16FullScale = 32768.0f;
        constexpr float kInt32FullScale = 2147483648.0f;

        float clamp_unit(float value)
        {
            return std::max(-1.0f, std::min(1.0f, value));
        }
    }

    float asio_sample_to_float(AsioSampleFormat format, const void* sample_bytes)
    {
        switch (format)
        {
            case AsioSampleFormat::Int16LSB:
            {
                std::int16_t raw = 0;
                std::memcpy(&raw, sample_bytes, sizeof(raw));
                return static_cast<float>(raw) / kInt16FullScale;
            }
            case AsioSampleFormat::Int32LSB:
            {
                std::int32_t raw = 0;
                std::memcpy(&raw, sample_bytes, sizeof(raw));
                return static_cast<float>(raw) / kInt32FullScale;
            }
            case AsioSampleFormat::Float32LSB:
            {
                float raw = 0.0f;
                std::memcpy(&raw, sample_bytes, sizeof(raw));
                return raw;
            }
        }
        return 0.0f;
    }

    void float_to_asio_sample(AsioSampleFormat format, float value, void* out_sample_bytes)
    {
        const float clamped = clamp_unit(value);
        switch (format)
        {
            case AsioSampleFormat::Int16LSB:
            {
                const std::int16_t raw = static_cast<std::int16_t>(clamped * (kInt16FullScale - 1.0f));
                std::memcpy(out_sample_bytes, &raw, sizeof(raw));
                return;
            }
            case AsioSampleFormat::Int32LSB:
            {
                const std::int32_t raw = static_cast<std::int32_t>(clamped * (kInt32FullScale - 1.0f));
                std::memcpy(out_sample_bytes, &raw, sizeof(raw));
                return;
            }
            case AsioSampleFormat::Float32LSB:
            {
                std::memcpy(out_sample_bytes, &clamped, sizeof(clamped));
                return;
            }
        }
    }
}
```

- [ ] **Step 5: Add the source file to `CMakeLists.txt`'s `sushi_dsp` target**

In the root `CMakeLists.txt`, add `src/device/AsioSampleConverter.cpp` to the `add_library(sushi_dsp STATIC ...)` source list (alphabetically among the other `src/...` entries, e.g. right before `src/io/WavFile.cpp`).

- [ ] **Step 6: Run the tests to verify they pass**

Run: `sd test`
Expected: PASS — all `AsioSampleConverter*` tests plus every pre-existing test.

Manually verify the clamp assertion's exact value: `EXPECT_EQ(decoded_value, 2147483647)` —
`clamp_unit(2.0f)` returns `1.0f`; `1.0f * (2147483648.0f - 1.0f) = 2147483647.0f`, which
`static_cast<std::int32_t>` truncates to `2147483647`, the exact value asserted. This must
hold exactly (not just "near") since it's an integer round-trip of an exact float value — if
your build disagrees, recheck the arithmetic rather than loosening the assertion.

- [ ] **Step 7: Add an RT-safety trap test**

`asio_sample_to_float`/`float_to_asio_sample` are called per-sample on `AsioAudioDevice`'s
audio thread in Task 4 — confirm now, before that class exists, that the conversion
functions themselves are allocation-free. Add to `tests/functional/unit/test_rt_safety.cpp`
(include `"SushiDSP/device/AsioSampleConverter.hpp"` at the top with the other includes):

```cpp
TEST(Unit_Dsp, AsioSampleConverterPerformsNoHeapAllocation)
{
    std::uint8_t bytes[4] = { 0, 0, 0, 0 };

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int i = 0; i < 10000; ++i)
    {
        const float decoded = SushiDSP::asio_sample_to_float(SushiDSP::AsioSampleFormat::Int32LSB, bytes);
        SushiDSP::float_to_asio_sample(SushiDSP::AsioSampleFormat::Int32LSB, decoded, bytes);
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
}
```

Run: `sd test`
Expected: PASS.

- [ ] **Step 8: Add license headers (already included above verbatim), commit**

```bash
git add include/SushiDSP/device/AsioSampleConverter.hpp src/device/AsioSampleConverter.cpp \
        tests/functional/unit/test_asio_sample_converter.cpp tests/functional/unit/test_rt_safety.cpp \
        CMakeLists.txt tests/CMakeLists.txt
git commit -m "Add AsioSampleConverter: SDK-independent, testable-everywhere sample format conversion"
```

---

### Task 4: `AsioAudioDevice` and its CMake target

**Files:**
- Create: `include/SushiDSP/device/AsioAudioDevice.hpp`, `src/device/AsioAudioDevice.cpp`
- Modify: `CMakeLists.txt` (add the `ASIO_SDK_DIR`/`SUSHIDSP_BUILD_ASIO` block)
- Modify: `.gitignore` (already done — `third_party/asio/` is already excluded; verify only,
  no edit needed unless this task finds it missing)

**Interfaces:**
- Consumes: `IAudioDevice`, `DeviceFormat`, `DeviceCallback` (Task 2);
  `AsioSampleFormat`, `asio_sample_to_float`, `float_to_asio_sample` (Task 3).
- Produces: `SushiDSP::AsioAudioDevice` (only buildable/testable manually, on Windows, with
  the SDK present — no automated test in this plan exercises it directly, per the design
  spec's Testing section).

**This task cannot be TDD'd in the usual sense** (no automated test can open a real ASIO
driver), so its acceptance criterion is: it builds cleanly via `sd build` on this machine
(where the SDK is present and a real driver is installed), and its code is reviewed for
correctness against the ASIO SDK's documented calling sequence rather than proven by a
red/green test cycle. Read `third_party/asio/common/iasiodrv.h` and
`third_party/asio/host/asiodrivers.h` before writing this file — they are the authoritative
source for the exact function signatures below; treat any mismatch between this task's
sketch and those headers as a plan defect to fix while implementing, matching every prior
phase's established pattern of correcting a plan against the real, current source rather
than transcribing it blindly.

- [ ] **Step 1: Write `include/SushiDSP/device/AsioAudioDevice.hpp`**

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "SushiDSP/core/IAudioDevice.hpp"
#include "SushiDSP/device/AsioSampleConverter.hpp"

namespace SushiDSP
{
    /// @brief ASIO-backed IAudioDevice. The only class in SushiDSP that touches ASIO SDK
    /// types - no ASIO header leaks past this class's own .cpp file. Windows-only; only
    /// built when the ASIO SDK is present (see CMakeLists.txt's SUSHIDSP_BUILD_ASIO block).
    /// Duplex: reports genuine input_channels alongside output_channels.
    class AsioAudioDevice : public IAudioDevice
    {
    public:
        /// @brief Lists the ASIO driver names currently registered on this machine.
        /// @return Driver names, suitable for passing to the constructor.
        static std::vector<std::string> enumerate_drivers();

        /// @param driver_name An exact name from enumerate_drivers().
        /// @param requested_format sample_rate/block_size are requests; the opened device's
        /// actual negotiated format is returned by format() and may differ. input_channels/
        /// output_channels select how many of the driver's hardware channels (starting from
        /// channel 0) this device presents through IAudioDevice.
        /// @throws std::runtime_error if the driver cannot be loaded or initialized.
        AsioAudioDevice(const std::string& driver_name, const DeviceFormat& requested_format);

        ~AsioAudioDevice() override;

        DeviceFormat format() const override;

        void start(DeviceCallback callback) override;

        void stop() override;

    private:
        DeviceFormat   format_;
        DeviceCallback callback_;

        AsioSampleFormat input_sample_format_;
        AsioSampleFormat output_sample_format_;

        std::vector<std::vector<float>> input_scratch_;   // [channel][frame], deinterleaved
        std::vector<std::vector<float>> output_scratch_;  // [channel][frame], deinterleaved
        std::vector<float*>             input_scratch_pointers_;
        std::vector<float*>             output_scratch_pointers_;

        bool running_ = false;

        // Trampoline target for ASIO's free-function callbacks (the SDK assumes one open
        // driver process-wide; a second concurrently-open AsioAudioDevice is a documented
        // programming error, not runtime-guarded, matching this codebase's existing
        // precedent of documenting rather than checking constructor-level preconditions).
        static AsioAudioDevice* active_instance_;

        void on_buffer_switch(long buffer_index, long direct_process);
        static void buffer_switch_trampoline(long buffer_index, long direct_process);
        static void sample_rate_did_change_trampoline(double sample_rate);
        static long asio_message_trampoline(long selector, long value, void* message, double* opt);
        static void* buffer_switch_time_info_trampoline(void* params, long buffer_index, long direct_process);
    };
}
```

- [ ] **Step 2: Write `src/device/AsioAudioDevice.cpp`**

Read `third_party/asio/common/iasiodrv.h`, `third_party/asio/common/asio.h`, and
`third_party/asio/host/asiodrivers.h` first — the classic ASIO host calling sequence is:

1. `AsioDrivers` (from `asiodrivers.h`) enumerates registered drivers (`asioGetDriverNames`)
   and loads one by name (`loadDriver`).
2. `ASIOInit(&driverInfo)` initializes the loaded driver.
3. `ASIOGetChannels(&numInputChannels, &numOutputChannels)` reports hardware channel counts.
4. `ASIOGetBufferSize(&minSize, &maxSize, &preferredSize, &granularity)` reports supported
   block sizes; request `preferredSize` unless the caller's `requested_format.block_size` is
   also within `[minSize, maxSize]`, in which case honor it.
5. `ASIOGetSampleRate(&currentRate)` / `ASIOCanSampleRate`/`ASIOSetSampleRate` negotiate the
   sample rate.
6. `ASIOGetChannelInfo(&channelInfo)` per channel reports each channel's native `ASIOSampleType`
   — map this to `AsioSampleFormat` (`ASIOSTInt16LSB` -> `Int16LSB`, `ASIOSTInt32LSB` ->
   `Int32LSB`, `ASIOSTFloat32LSB` -> `Float32LSB`; any other native type is unsupported for
   this pass — throw `std::runtime_error` naming the unsupported type rather than silently
   misreading it, matching `WavFile`'s established "reject clearly rather than misread"
   convention).
7. `ASIOCreateBuffers(bufferInfos, numChannels, bufferSize, &callbacks)` allocates the
   driver's double-buffers and registers the callback struct (whose function pointers are
   this class's four static trampolines).
8. `ASIOStart()`/`ASIOStop()` begin/end the real-time callback stream.
9. `ASIODisposeBuffers()`/`ASIOExit()` in the destructor, mirroring construction in reverse.

`on_buffer_switch` (called from `buffer_switch_trampoline`, which reads `active_instance_`)
is the real-time method: for each input channel, convert the driver's native buffer for
`buffer_index` into `input_scratch_[channel]` via `asio_sample_to_float`; call `callback_`
with `input_scratch_pointers_.data()`/`output_scratch_pointers_.data()`; then convert
`output_scratch_` back into the driver's native output buffers for `buffer_index` via
`float_to_asio_sample`. All scratch vectors are sized once in the constructor (never resized
in `on_buffer_switch`) — this is the RT-safety-critical property of this method; do not
introduce any allocation, lock, syscall, or exception into it.

Write the full implementation now, following this sequence exactly against the real SDK
headers. Use the license header from `src/device/Sdl2AudioDevice.cpp` verbatim.

- [ ] **Step 3: Add the CMake target**

In the root `CMakeLists.txt`, after the existing `SUSHIDSP_BUILD_SDL2` block (before
`enable_testing()`):

```cmake
# The Steinberg ASIO SDK is proprietary (accept-the-license, dual Steinberg/GPLv3) and cannot
# be committed or fetched automatically - each developer vendors it themselves at
# third_party/asio/ (gitignored). Absent SDK or non-Windows platform: this target is silently
# skipped, exactly like a missing SUSHIDSP_BUILD_SDL2 dependency - CI never sets
# ASIO_SDK_DIR, so it is always skipped there, which is correct (Windows-only, no SDK in CI).
set(ASIO_SDK_DIR "${CMAKE_SOURCE_DIR}/third_party/asio" CACHE PATH "Path to the Steinberg ASIO SDK")
option(SUSHIDSP_BUILD_ASIO "Build the ASIO device backend" ON)

if(SUSHIDSP_BUILD_ASIO AND WIN32 AND EXISTS "${ASIO_SDK_DIR}/common/asio.h")
    add_library(sushi_dsp_asio STATIC
        src/device/AsioAudioDevice.cpp
        ${ASIO_SDK_DIR}/common/asio.cpp
        ${ASIO_SDK_DIR}/common/asiodrvr.cpp
        ${ASIO_SDK_DIR}/common/combase.cpp
        ${ASIO_SDK_DIR}/common/register.cpp
        ${ASIO_SDK_DIR}/host/asiodrivers.cpp
        ${ASIO_SDK_DIR}/host/pc/asiolist.cpp
        ${ASIO_SDK_DIR}/host/ASIOConvertSamples.cpp
    )
    target_include_directories(sushi_dsp_asio PRIVATE
        ${ASIO_SDK_DIR}/common ${ASIO_SDK_DIR}/host ${ASIO_SDK_DIR}/host/pc)
    target_link_libraries(sushi_dsp_asio PUBLIC SushiDSP::sushi_dsp PRIVATE ole32 oleaut32)
    add_library(SushiDSP::sushi_dsp_asio ALIAS sushi_dsp_asio)
    message(STATUS "ASIO SDK found at ${ASIO_SDK_DIR} - sushi_dsp_asio target enabled")
else()
    message(STATUS "ASIO SDK not found at ${ASIO_SDK_DIR} (or not on Windows) - sushi_dsp_asio target skipped")
endif()
```

If, while writing Task 4 Step 2, any of the listed SDK `.cpp` files turn out to be unnecessary
(unreferenced) or another is required (e.g. `combase.cpp`/`register.cpp` provide the SDK's own
minimal COM base class implementation used by `asiodrvr.cpp` — confirm against what actually
fails to link), correct this source list to match reality; note the correction in your task
report.

- [ ] **Step 4: Build to verify**

Run: `sd build`
Expected: SUCCESS, and the configure step's output should include
`ASIO SDK found at .../third_party/asio - sushi_dsp_asio target enabled` (this machine has
the SDK vendored). If it instead skips the target, stop and investigate — that means
`ASIO_SDK_DIR` isn't resolving to the actual vendored path, which must be fixed before this
task can be considered done, since an untested "should build" claim is exactly the kind of
unverified assertion this project's honesty principle forbids.

Run: `sd test`
Expected: PASS — the full existing suite, unaffected (this target isn't linked into
`sushidsp_tests`).

- [ ] **Step 5: Commit**

```bash
git add include/SushiDSP/device/AsioAudioDevice.hpp src/device/AsioAudioDevice.cpp CMakeLists.txt
git commit -m "Add AsioAudioDevice: a duplex IAudioDevice backend on the Steinberg ASIO SDK"
```

---

### Task 5: `apps/host_asio` — the real playable application

**Files:**
- Create: `apps/host_asio/main.cpp`, `apps/host_asio/CMakeLists.txt`
- Modify: `CMakeLists.txt` (add `add_subdirectory(apps/host_asio)` inside the ASIO target's
  `if()` block from Task 4)

**Interfaces:**
- Consumes: `AsioAudioDevice` (Task 4), `RigGraph`'s root-input passthrough (Task 1),
  `TubeScreamer`/`Jcm800` (D3/D4, already built).

- [ ] **Step 1: Write `apps/host_asio/main.cpp`**

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
#include <cstdlib>
#include <memory>
#include <string>

#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/device/AsioAudioDevice.hpp"
#include "SushiDSP/jcm800/Jcm800.hpp"
#include "SushiDSP/tubescreamer/TubeScreamer.hpp"

int main(int argc, char** argv)
{
    const std::vector<std::string> drivers = SushiDSP::AsioAudioDevice::enumerate_drivers();
    if (drivers.empty())
    {
        std::fprintf(stderr, "No ASIO drivers are registered on this machine.\n");
        return 1;
    }

    if (argc < 2)
    {
        std::printf("Usage: %s <asio-driver-name>\nAvailable drivers:\n", argv[0]);
        for (const std::string& name : drivers)
        {
            std::printf("  %s\n", name.c_str());
        }
        return 1;
    }

    const std::string driver_name = argv[1];
    SushiDSP::DeviceFormat requested{ 48000.0, 256, 1, 1 };

    std::unique_ptr<SushiDSP::AsioAudioDevice> device;
    try
    {
        device = std::make_unique<SushiDSP::AsioAudioDevice>(driver_name, requested);
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Failed to open ASIO driver '%s': %s\n", driver_name.c_str(), error.what());
        return 1;
    }

    const SushiDSP::DeviceFormat actual = device->format();
    std::printf(
        "Opened '%s': %.0f Hz, %u frames/block, %u input / %u output channel(s)\n",
        driver_name.c_str(), actual.sample_rate, actual.block_size,
        actual.input_channels, actual.output_channels);

    SushiDSP::RigGraph rig(actual.block_size, 4);

    // Hardcoded demo settings: there is no rig-editor UI yet.
    auto tube_screamer = std::make_unique<SushiDSP::TubeScreamer>();
    tube_screamer->set_drive(0.5f);
    tube_screamer->set_tone(0.5f);
    tube_screamer->set_level(0.8f);

    auto jcm800 = std::make_unique<SushiDSP::Jcm800>();
    jcm800->set_preamp(0.6f);
    jcm800->set_master(0.6f);
    jcm800->set_bass(0.5f);
    jcm800->set_middle(0.5f);
    jcm800->set_treble(0.6f);
    jcm800->set_presence(0.5f);

    // No SineToneNode: tube_screamer_handle has no upstream edge, so RigGraph feeds it the
    // device's own live input directly (Task 1's root-node passthrough).
    const SushiDSP::NodeHandle tube_screamer_handle = rig.add_node(std::move(tube_screamer));
    const SushiDSP::NodeHandle jcm800_handle = rig.add_node(std::move(jcm800));
    rig.connect(tube_screamer_handle, jcm800_handle);

    SushiDSP::PrepareInfo info{ actual.sample_rate, actual.block_size, actual.output_channels };
    rig.compile(info);

    device->start([&rig](const float* const* inputs, float* const* outputs, std::uint32_t frames)
    {
        SushiDSP::ProcessContext context{ outputs, inputs, frames, 0.0 };
        rig.process(context);
    });

    std::printf("Playing live through TubeScreamer -> JCM800. Press Enter to stop.\n");
    std::getchar();

    device->stop();
    std::printf("Stopped.\n");

    return 0;
}
```

- [ ] **Step 2: Write `apps/host_asio/CMakeLists.txt`**

```cmake
add_executable(sushidsp_host_asio
    main.cpp
)
target_link_libraries(sushidsp_host_asio PRIVATE SushiDSP::sushi_dsp_asio SushiDSP::tubescreamer SushiDSP::jcm800)
```

- [ ] **Step 3: Wire it into the root `CMakeLists.txt`**

Inside the `if(SUSHIDSP_BUILD_ASIO AND WIN32 AND EXISTS ...)` block from Task 4, after the
`message(STATUS "ASIO SDK found...")` line, add:

```cmake
    add_subdirectory(apps/host_asio)
```

- [ ] **Step 4: Build to verify**

Run: `sd build`
Expected: SUCCESS — `sushidsp_host_asio` builds. **Do not run it** — it opens real audio
hardware; that is the user's step.

Run: `sd test --suite all`
Expected: PASS — the complete suite, confirming nothing in this task touched anything
test-linked.

- [ ] **Step 5: Commit**

```bash
git add apps/host_asio/main.cpp apps/host_asio/CMakeLists.txt CMakeLists.txt
git commit -m "Add apps/host_asio: the real live-guitar-through-the-rig application"
```

---

## Self-Review Notes

- **Spec coverage:** design spec section 1 (`IAudioDevice` duplex) -> Task 2. Section 2
  (`AsioAudioDevice`) -> Tasks 3 (its SDK-independent conversion half) and 4 (the SDK-dependent
  half). Section 3 (build integration) -> Task 4. Section 4 (`apps/host_asio`) -> Task 5.
  Licensing section -> already satisfied (`.gitignore` entry exists; Task 4 verifies rather
  than re-adds it). Testing section -> Task 3's converter tests + RT-safety trap;
  `AsioAudioDevice` itself intentionally has no automated test, matching the spec's own
  stated precedent.
- **Gap found and closed during planning, not silently carried:** the design spec claimed
  "`RigGraph` needs no change." That's incorrect for root nodes, as detailed in Task 1's
  rationale — closed as its own task, with its own test, before any ASIO-specific code
  depends on it. This plan corrects the spec's claim rather than quietly working around it.
- **Placeholder scan:** none. Every task has literal code, exact file paths, exact test
  assertions. Task 4 is the one task that cannot be TDD'd (no ASIO hardware in an automated
  test), which is stated explicitly as an accepted limitation, not glossed over — its
  acceptance criterion (a real `sd build` success with the SDK actually present on this
  machine) is spelled out rather than left as "verify it works."
- **Type consistency:** `DeviceFormat{ sample_rate, block_size, input_channels, output_channels }`
  used identically across Task 2 (interface + SDL2 + test), Task 4 (`AsioAudioDevice`), and
  Task 5 (`apps/host_asio`). `DeviceCallback`'s 3-argument
  `(inputs, outputs, frames)` shape is used identically everywhere it's constructed
  (Task 2's SDL2 callback and test lambdas, Task 5's device->start lambda). `AsioSampleFormat`/
  `asio_sample_to_float`/`float_to_asio_sample` (Task 3) are the exact names Task 4's sketch
  calls.
