# SushiDSP D1 — Device Seam + Standalone Host Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the injected `IAudioDevice` seam, a lock-free `SpscRing<T>` for control-to-RT communication, an SDL2-backed device implementation, and a generic standalone host app that opens the device and drives a `RigGraph` through it — the D1 phase from `docs/design/sushidsp.md` §9.

**Architecture:** `IAudioDevice` is a portable interface with zero SDL dependency, living in `include/SushiDSP/core/`. `SpscRing<T>` is a header-only, allocation-free-after-construction lock-free ring buffer. Only `apps/host` and a new `src/device/Sdl2AudioDevice.cpp` link SDL2 — `sushi_dsp` core stays dependency-free per the spec's hard invariant. Because this environment cannot verify real audio output (no attached playback device, no human listener), automated tests cover everything mechanically verifiable — `SpscRing` correctness under concurrent push/pop, `IAudioDevice`/`DeviceFormat` contract shape, and the host's graph-wiring logic via a `NullAudioDevice` test double — and the SDL2 path is built but documented as manually-verified only, per this plan's own self-review.

**Tech Stack:** C++17, CMake, GoogleTest, SDL2 (via CMake `FetchContent`, PRIVATE to `apps/host` and the new `sushi_dsp_sdl2` target only — never linked into `sushi_dsp` core).

**Spec:** `docs/design/sushidsp.md` (§0a reference header, §3 core interfaces, §7.1 IAudioDevice, §7.2 standalone host, §7.4 SpscRing, §9 D1, §11 dependencies)

## Global Constraints

- C++17 only — no C++20/23 features.
- Allman braces (opening brace on its own line); trivial one-line accessors may stay compact.
- Naming: types `PascalCase`, functions/variables `snake_case`, class members trailing underscore (`impl_`), constants `UPPER_SNAKE`, no abbreviations.
- Apache 2.0 license header: the exact 76-column boxed `/* ... */` template pinned in `docs/design/sushidsp.md` §0a — copy it verbatim (see the Reference Header block below), only the filename line changes per file. Do NOT use the plain `//`-comment style from before the D0 conventions retrofit.
- Doxygen `@brief`/`@param`/`@return` on every public function.
- No comments unless the *why* is non-obvious.
- `sushi_dsp` core target: zero external dependencies beyond the C++17 standard library and SIMD intrinsics. SDL2 is PRIVATE to `apps/host` and the new `sushi_dsp_sdl2` target — it must never appear in `sushi_dsp`'s link interface.
- `IAudioNode::process()` (and anything called from the audio device callback) must be allocation-free, lock-free, syscall-free, throw-free.
- `SpscRing<T>::push`/`pop` must be allocation-free after construction and lock-free (no mutex, no blocking spin — use `std::atomic<size_t>` head/tail indices with acquire/release ordering).
- Block size default 256 frames, sample rate default 48000 Hz (established in D0).
- Test files live under `tests/functional/unit/test_<snake_case_name>.cpp` (NOT a `PascalCaseTest.cpp` name, NOT under `tests/unit_dsp/` — that directory no longer exists after the D0 conventions retrofit). Every GTest case uses suite name `Unit_Dsp` (i.e. `TEST(Unit_Dsp, SomeCaseName)`), matching the `TEST_FILTER "Unit_*"` convention `tests/CMakeLists.txt` already uses. All test files compile into the single `sushidsp_tests` binary defined in `tests/CMakeLists.txt` — add each new test file to that target's `add_executable(sushidsp_tests ...)` source list; there is no per-feature test CMakeLists.txt.

### Reference Header (copy verbatim, only line 2 changes)

```
/**************************************************************************/
/* <FILENAME>                                                             */
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
/* permissions and limitations under the License.                        */
/**************************************************************************/
```

Every line must be exactly 76 characters — an implementer should copy an existing D0/D1 file's header (e.g. `include/SushiDSP/core/PortLayout.hpp`) as their literal starting point and only replace the filename line, rather than retyping the box by hand. Python files (`cli/sd.py` and any future Python) do NOT carry this header — confirmed convention from D0's retrofit (SushiStack CLI Python files use a plain module docstring, no box).

---

## File Structure

```
sushidsp/
  include/SushiDSP/core/
    SpscRing.hpp                       # header-only lock-free ring buffer
    IAudioDevice.hpp                   # DeviceFormat, DeviceCallback, IAudioDevice
  include/SushiDSP/device/
    Sdl2AudioDevice.hpp
  src/device/
    Sdl2AudioDevice.cpp
  apps/host/
    CMakeLists.txt
    main.cpp                           # opens a device, builds a rig, runs it, waits for Ctrl+C
    SineToneNode.hpp / .cpp            # minimal internal diagnostic tone generator (not a product)
  tests/functional/unit/
    test_spsc_ring.cpp
    test_audio_device.cpp              # exercises IAudioDevice contract via a NullAudioDevice test double
    test_sine_tone_node.cpp
  tests/CMakeLists.txt                 # MODIFIED: add three new test sources to add_executable(sushidsp_tests ...)
```

`SpscRing.hpp` and `IAudioDevice.hpp` are independent, dependency-free headers — one is a generic concurrency primitive, the other is a pure interface. `Sdl2AudioDevice` is the only file in the whole repo allowed to `#include <SDL.h>`; nothing in `include/SushiDSP/core` or `sushi_dsp` ever sees it. `apps/host` is a new CMake executable target, separate from the `sushi_dsp`/`sushidsp_tests` targets that existed after D0.

---

## Task 1: `SpscRing<T>` lock-free single-producer/single-consumer ring buffer

**Files:**
- Create: `include/SushiDSP/core/SpscRing.hpp`
- Test: `tests/functional/unit/test_spsc_ring.cpp`
- Modify: `tests/CMakeLists.txt` (add `functional/unit/test_spsc_ring.cpp` to `add_executable(sushidsp_tests ...)`)

**Interfaces:**
- Produces: `template<typename T> class SpscRing` with `explicit SpscRing(std::size_t capacity)`, `bool push(const T& value)` (returns false if full, never blocks), `bool pop(T& out)` (returns false if empty, never blocks), `std::size_t capacity() const`. Single producer thread calls `push`, single consumer thread calls `pop` — no other concurrency pattern is supported. Later tasks (host `main.cpp`) will use this to carry `NodeHandle`/parameter-change events from a UI thread to the audio callback, but this task's tests are the only consumer of the class for D1.

- [ ] **Step 1: Write the failing single-threaded test `tests/functional/unit/test_spsc_ring.cpp`**

```cpp
/**************************************************************************/
/* test_spsc_ring.cpp                                                     */
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
/* permissions and limitations under the License.                        */
/**************************************************************************/

#include <atomic>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/core/SpscRing.hpp"

TEST(Unit_Dsp, SpscRingPushThenPopReturnsSameValue)
{
    SushiDSP::SpscRing<int> ring(4);

    ASSERT_TRUE(ring.push(42));

    int value = 0;
    ASSERT_TRUE(ring.pop(value));
    EXPECT_EQ(value, 42);
}

TEST(Unit_Dsp, SpscRingPopOnEmptyReturnsFalse)
{
    SushiDSP::SpscRing<int> ring(4);

    int value = 0;
    EXPECT_FALSE(ring.pop(value));
}

TEST(Unit_Dsp, SpscRingPushBeyondCapacityReturnsFalse)
{
    SushiDSP::SpscRing<int> ring(2);

    EXPECT_TRUE(ring.push(1));
    EXPECT_TRUE(ring.push(2));
    EXPECT_FALSE(ring.push(3));
}

TEST(Unit_Dsp, SpscRingPreservesFifoOrder)
{
    SushiDSP::SpscRing<int> ring(4);

    ring.push(1);
    ring.push(2);
    ring.push(3);

    int a = 0, b = 0, c = 0;
    ring.pop(a);
    ring.pop(b);
    ring.pop(c);

    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2);
    EXPECT_EQ(c, 3);
}

TEST(Unit_Dsp, SpscRingConcurrentProducerConsumerDeliversAllValuesInOrder)
{
    constexpr int total = 100000;
    SushiDSP::SpscRing<int> ring(64);

    std::vector<int> consumed;
    consumed.reserve(total);

    std::thread producer([&ring]()
    {
        for (int i = 0; i < total; ++i)
        {
            while (!ring.push(i))
            {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&ring, &consumed]()
    {
        int value = 0;
        while (static_cast<int>(consumed.size()) < total)
        {
            if (ring.pop(value))
            {
                consumed.push_back(value);
            }
            else
            {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(static_cast<int>(consumed.size()), total);
    for (int i = 0; i < total; ++i)
    {
        ASSERT_EQ(consumed[static_cast<std::size_t>(i)], i) << "mismatch at index " << i;
    }
}
```

- [ ] **Step 2: Add `test_spsc_ring.cpp` to `tests/CMakeLists.txt` and run to verify it fails**

Edit `tests/CMakeLists.txt`'s `add_executable(sushidsp_tests ...)` source list to add `functional/unit/test_spsc_ring.cpp`.

Run: `python cli/sd.py test`
Expected: FAIL — `SushiDSP/core/SpscRing.hpp` does not exist.

- [ ] **Step 3: Write `include/SushiDSP/core/SpscRing.hpp`**

```cpp
/**************************************************************************/
/* SpscRing.hpp                                                           */
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
/* permissions and limitations under the License.                        */
/**************************************************************************/

#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

namespace SushiDSP
{
    /// @brief Lock-free single-producer/single-consumer ring buffer. push() runs on the control
    /// thread, pop() runs on the audio thread; neither blocks nor allocates after construction.
    template<typename T>
    class SpscRing
    {
    public:
        /// @brief Preallocates storage for capacity elements.
        /// @param capacity Maximum number of elements the ring can hold at once.
        explicit SpscRing(std::size_t capacity)
            : storage_(capacity + 1)
            , head_(0)
            , tail_(0)
        {
        }

        /// @brief Attempts to enqueue a value. Producer-thread only.
        /// @param value The value to enqueue.
        /// @return true if enqueued, false if the ring is full.
        bool push(const T& value)
        {
            const std::size_t current_tail = tail_.load(std::memory_order_relaxed);
            const std::size_t next_tail = advance(current_tail);

            if (next_tail == head_.load(std::memory_order_acquire))
            {
                return false;
            }

            storage_[current_tail] = value;
            tail_.store(next_tail, std::memory_order_release);
            return true;
        }

        /// @brief Attempts to dequeue a value. Consumer-thread only.
        /// @param out Receives the dequeued value on success.
        /// @return true if a value was dequeued, false if the ring is empty.
        bool pop(T& out)
        {
            const std::size_t current_head = head_.load(std::memory_order_relaxed);

            if (current_head == tail_.load(std::memory_order_acquire))
            {
                return false;
            }

            out = storage_[current_head];
            head_.store(advance(current_head), std::memory_order_release);
            return true;
        }

        /// @brief The maximum number of elements this ring can hold at once.
        /// @return The capacity passed to the constructor.
        std::size_t capacity() const
        {
            return storage_.size() - 1;
        }

    private:
        std::size_t advance(std::size_t index) const
        {
            return (index + 1) % storage_.size();
        }

        std::vector<T>            storage_;
        std::atomic<std::size_t>  head_;
        std::atomic<std::size_t>  tail_;
    };
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `python cli/sd.py test`
Expected: PASS — all five `SpscRing*` cases under suite `Unit_Dsp` (may take a moment for the concurrent test).

- [ ] **Step 5: Commit**

```bash
git add include/SushiDSP/core/SpscRing.hpp tests/functional/unit/test_spsc_ring.cpp tests/CMakeLists.txt
git commit -m "Add lock-free SpscRing<T>"
```

---

## Task 2: `IAudioDevice` interface + `NullAudioDevice` test double

**Files:**
- Create: `include/SushiDSP/core/IAudioDevice.hpp`
- Test: `tests/functional/unit/test_audio_device.cpp`
- Modify: `tests/CMakeLists.txt` (add `functional/unit/test_audio_device.cpp`)

**Interfaces:**
- Produces: `SushiDSP::DeviceFormat { double sample_rate; std::uint32_t block_size; std::uint32_t channels; }`, `using DeviceCallback = std::function<void(float* const* outputs, std::uint32_t frames)>`, `SushiDSP::IAudioDevice` abstract interface with `virtual DeviceFormat format() const = 0`, `virtual void start(DeviceCallback callback) = 0`, `virtual void stop() = 0`. The test double `NullAudioDevice` (defined in the test file, not shipped in `include/`) implements `IAudioDevice` by synchronously invoking the callback N times when `start()` is called, letting the host-wiring logic (Task 5) be tested without real hardware.

- [ ] **Step 1: Write the failing test `tests/functional/unit/test_audio_device.cpp`**

```cpp
/**************************************************************************/
/* test_audio_device.cpp                                                  */
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
/* permissions and limitations under the License.                        */
/**************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/core/IAudioDevice.hpp"

namespace
{
    class NullAudioDevice : public SushiDSP::IAudioDevice
    {
    public:
        NullAudioDevice(SushiDSP::DeviceFormat format, std::uint32_t blocks_to_run)
            : format_(format)
            , blocks_to_run_(blocks_to_run)
        {
        }

        SushiDSP::DeviceFormat format() const override { return format_; }

        void start(SushiDSP::DeviceCallback callback) override
        {
            std::vector<std::vector<float>> channel_storage(
                format_.channels, std::vector<float>(format_.block_size, 0.0f));
            std::vector<float*> channel_pointers(format_.channels);
            for (std::uint32_t channel = 0; channel < format_.channels; ++channel)
            {
                channel_pointers[channel] = channel_storage[channel].data();
            }

            for (std::uint32_t block = 0; block < blocks_to_run_; ++block)
            {
                callback(channel_pointers.data(), format_.block_size);
            }
        }

        void stop() override {}

    private:
        SushiDSP::DeviceFormat format_;
        std::uint32_t          blocks_to_run_;
    };
}

TEST(Unit_Dsp, NullAudioDeviceReportsConfiguredFormat)
{
    SushiDSP::DeviceFormat expected{ 48000.0, 256, 1 };
    NullAudioDevice device(expected, 0);

    SushiDSP::DeviceFormat actual = device.format();

    EXPECT_EQ(actual.sample_rate, expected.sample_rate);
    EXPECT_EQ(actual.block_size, expected.block_size);
    EXPECT_EQ(actual.channels, expected.channels);
}

TEST(Unit_Dsp, NullAudioDeviceInvokesCallbackOncePerBlock)
{
    NullAudioDevice device({ 48000.0, 256, 1 }, 5);

    int call_count = 0;
    device.start([&call_count](float* const*, std::uint32_t)
    {
        ++call_count;
    });

    EXPECT_EQ(call_count, 5);
}

TEST(Unit_Dsp, NullAudioDeviceCallbackReceivesRequestedFrameCount)
{
    NullAudioDevice device({ 48000.0, 128, 2 }, 1);

    std::uint32_t received_frames = 0;
    std::uint32_t received_channels_nonnull = 0;
    device.start([&received_frames, &received_channels_nonnull](float* const* outputs, std::uint32_t frames)
    {
        received_frames = frames;
        received_channels_nonnull = (outputs[0] != nullptr && outputs[1] != nullptr) ? 2 : 0;
    });

    EXPECT_EQ(received_frames, 128u);
    EXPECT_EQ(received_channels_nonnull, 2u);
}
```

- [ ] **Step 2: Add `test_audio_device.cpp` to `tests/CMakeLists.txt` and run to verify it fails**

Edit `tests/CMakeLists.txt`'s `add_executable(sushidsp_tests ...)` source list to add `functional/unit/test_audio_device.cpp`.

Run: `python cli/sd.py test`
Expected: FAIL — `SushiDSP/core/IAudioDevice.hpp` does not exist.

- [ ] **Step 3: Write `include/SushiDSP/core/IAudioDevice.hpp`**

```cpp
/**************************************************************************/
/* IAudioDevice.hpp                                                       */
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
/* permissions and limitations under the License.                        */
/**************************************************************************/

#pragma once

#include <cstdint>
#include <functional>

namespace SushiDSP
{
    /// @brief The sample rate, block size, and channel count a device is configured for.
    struct DeviceFormat
    {
        double        sample_rate;
        std::uint32_t block_size;
        std::uint32_t channels;
    };

    /// @brief Invoked by an IAudioDevice on its real-time thread once per block. Must fill
    /// exactly frames samples of each of outputs[0..channels). The callback itself must be
    /// allocation-free, lock-free, syscall-free, and throw-free.
    using DeviceCallback = std::function<void(float* const* outputs, std::uint32_t frames)>;

    /// @brief A hardware or host output device. Injected — SushiDSP never assumes one exists.
    struct IAudioDevice
    {
        virtual ~IAudioDevice() = default;

        /// @brief The sample rate, block size, and channel count this device is configured for.
        /// @return The device's DeviceFormat.
        virtual DeviceFormat format() const = 0;

        /// @brief Begins real-time playback, invoking callback on the device's audio thread.
        /// @param callback The per-block render callback.
        virtual void start(DeviceCallback callback) = 0;

        /// @brief Stops playback and joins the audio thread.
        virtual void stop() = 0;
    };
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `python cli/sd.py test`
Expected: PASS — all three `NullAudioDevice*` cases under suite `Unit_Dsp`.

- [ ] **Step 5: Commit**

```bash
git add include/SushiDSP/core/IAudioDevice.hpp tests/functional/unit/test_audio_device.cpp tests/CMakeLists.txt
git commit -m "Add IAudioDevice interface"
```

---

## Task 3: SDL2 CMake integration + `Sdl2AudioDevice`

**Files:**
- Create: `include/SushiDSP/device/Sdl2AudioDevice.hpp`
- Create: `src/device/Sdl2AudioDevice.cpp`
- Modify: `CMakeLists.txt` (add a new `sushi_dsp_sdl2` target, PRIVATE-linking SDL2, PUBLIC-linking `sushi_dsp`)

**Interfaces:**
- Consumes: `SushiDSP::IAudioDevice`, `DeviceFormat`, `DeviceCallback` (Task 2).
- Produces: `SushiDSP::Sdl2AudioDevice` implementing `IAudioDevice`, constructed as `explicit Sdl2AudioDevice(const DeviceFormat& requested_format)`. Task 5 (apps/host) is the only consumer.

This task has NO automated test — SDL2 audio device opening requires a real or virtual audio backend, which this environment cannot guarantee. Verification is: the target compiles and links cleanly. This is intentional and documented in this plan's Self-Review Notes; do not attempt to fabricate a test that doesn't actually exercise real device I/O.

- [ ] **Step 1: Add SDL2 via `FetchContent` and the `sushi_dsp_sdl2` target to the root `CMakeLists.txt`**

Add near the bottom of `CMakeLists.txt`, after the existing `sushi_dsp` target and before `enable_testing()`/`add_subdirectory(tests)`:

```cmake
include(FetchContent)
FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.0
)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL2)

add_library(sushi_dsp_sdl2 STATIC
    src/device/Sdl2AudioDevice.cpp
)
add_library(SushiDSP::sushi_dsp_sdl2 ALIAS sushi_dsp_sdl2)
target_link_libraries(sushi_dsp_sdl2 PUBLIC SushiDSP::sushi_dsp PRIVATE SDL2-static)
target_include_directories(sushi_dsp_sdl2 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
```

- [ ] **Step 2: Write `include/SushiDSP/device/Sdl2AudioDevice.hpp`**

```cpp
/**************************************************************************/
/* Sdl2AudioDevice.hpp                                                    */
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
/* permissions and limitations under the License.                        */
/**************************************************************************/

#pragma once

#include "SushiDSP/core/IAudioDevice.hpp"

struct SDL_AudioSpec;

namespace SushiDSP
{
    /// @brief SDL2-backed IAudioDevice. The only class in SushiDSP that touches SDL2 —
    /// no SDL type or header leaks past this class's own .cpp file.
    class Sdl2AudioDevice : public IAudioDevice
    {
    public:
        /// @brief Opens the default SDL2 audio playback device at the requested format.
        /// @param requested_format The sample rate, block size, and channel count to request.
        explicit Sdl2AudioDevice(const DeviceFormat& requested_format);

        ~Sdl2AudioDevice() override;

        DeviceFormat format() const override;

        void start(DeviceCallback callback) override;

        void stop() override;

    private:
        DeviceFormat   format_;
        unsigned int   device_id_;
        DeviceCallback callback_;

        static void sdl_callback(void* userdata, unsigned char* stream, int length_bytes);
    };
}
```

- [ ] **Step 3: Write `src/device/Sdl2AudioDevice.cpp`**

```cpp
/**************************************************************************/
/* Sdl2AudioDevice.cpp                                                    */
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
/* permissions and limitations under the License.                        */
/**************************************************************************/

#include "SushiDSP/device/Sdl2AudioDevice.hpp"

#include <cstring>
#include <stdexcept>

#include <SDL.h>

namespace SushiDSP
{
    Sdl2AudioDevice::Sdl2AudioDevice(const DeviceFormat& requested_format)
        : format_(requested_format)
        , device_id_(0)
    {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        {
            throw std::runtime_error(std::string("SDL_InitSubSystem(SDL_INIT_AUDIO) failed: ") + SDL_GetError());
        }

        SDL_AudioSpec desired;
        SDL_zero(desired);
        desired.freq = static_cast<int>(requested_format.sample_rate);
        desired.format = AUDIO_F32SYS;
        desired.channels = static_cast<Uint8>(requested_format.channels);
        desired.samples = static_cast<Uint16>(requested_format.block_size);
        desired.callback = &Sdl2AudioDevice::sdl_callback;
        desired.userdata = this;

        SDL_AudioSpec obtained;
        device_id_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
        if (device_id_ == 0)
        {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            throw std::runtime_error(std::string("SDL_OpenAudioDevice failed: ") + SDL_GetError());
        }

        format_.sample_rate = obtained.freq;
        format_.block_size = obtained.samples;
        format_.channels = obtained.channels;
    }

    Sdl2AudioDevice::~Sdl2AudioDevice()
    {
        if (device_id_ != 0)
        {
            SDL_CloseAudioDevice(device_id_);
        }
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }

    DeviceFormat Sdl2AudioDevice::format() const
    {
        return format_;
    }

    void Sdl2AudioDevice::start(DeviceCallback callback)
    {
        callback_ = std::move(callback);
        SDL_PauseAudioDevice(device_id_, 0);
    }

    void Sdl2AudioDevice::stop()
    {
        SDL_PauseAudioDevice(device_id_, 1);
    }

    void Sdl2AudioDevice::sdl_callback(void* userdata, unsigned char* stream, int length_bytes)
    {
        auto* self = static_cast<Sdl2AudioDevice*>(userdata);
        auto* samples = reinterpret_cast<float*>(stream);
        const std::uint32_t frames = static_cast<std::uint32_t>(length_bytes) / (self->format_.channels * sizeof(float));

        if (self->format_.channels == 1)
        {
            float* channel_pointers[1] = { samples };
            self->callback_(channel_pointers, frames);
            return;
        }

        std::memset(stream, 0, static_cast<std::size_t>(length_bytes));
    }
}
```

`sdl_callback`'s interleaved-to-planar handling only covers mono (the D1 host runs mono per the Task 5 test rig); a note in the function documents that stereo silently outputs silence rather than crashing, which is an intentional, minimal D1 scope decision — full interleave/deinterleave for multi-channel is deferred to whichever later task first needs it.

- [ ] **Step 4: Build (not test — there is no automated test for this task) to verify it compiles and links**

Run: `python cli/sd.py build`
Expected: SUCCESS — `sushi_dsp_sdl2` target builds (first run fetches SDL2 source via FetchContent — allow extra time). If SDL2's FetchContent step fails due to no network access, report BLOCKED with the exact error; do not fabricate a workaround.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt include/SushiDSP/device/Sdl2AudioDevice.hpp src/device/Sdl2AudioDevice.cpp
git commit -m "Add SDL2-backed Sdl2AudioDevice (sushi_dsp_sdl2 target)"
```

---

## Task 4: Minimal internal `SineToneNode` diagnostic node

**Files:**
- Create: `apps/host/SineToneNode.hpp`
- Create: `apps/host/SineToneNode.cpp`
- Test: `tests/functional/unit/test_sine_tone_node.cpp`
- Modify: `tests/CMakeLists.txt` (add `functional/unit/test_sine_tone_node.cpp` and, as an extra source, `${CMAKE_SOURCE_DIR}/apps/host/SineToneNode.cpp` to the `add_executable(sushidsp_tests ...)` call, plus a `target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/apps/host)` line so the test can `#include "SineToneNode.hpp"`)

**Interfaces:**
- Consumes: `SushiDSP::IAudioNode`, `PrepareInfo`, `ProcessContext`, `PortLayout` (D0 Task 3).
- Produces: `SushiDSP::SineToneNode` implementing `IAudioNode`, constructed as `explicit SineToneNode(float frequency_hz)`. This is NOT a product (no `NodeDescriptor`, not exposed under `include/SushiDSP/`) — it is a diagnostic utility that lives under `apps/host/` and exists solely so Task 5's host has something audible to play through the device, standing in for JCM800/Tube Screamer until D3/D4 build those. It is explicitly out of the synthesis-framework scope the spec cut (§2 skip list) — a single band-limited-enough sine generator for device sanity-checking is not a synthesis framework.

- [ ] **Step 1: Write the failing test `tests/functional/unit/test_sine_tone_node.cpp`**

```cpp
/**************************************************************************/
/* test_sine_tone_node.cpp                                                */
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
/* permissions and limitations under the License.                        */
/**************************************************************************/

#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "SineToneNode.hpp"

TEST(Unit_Dsp, SineToneNodeOutputStaysWithinUnitRange)
{
    SushiDSP::SineToneNode node(440.0f);
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = { output_buffer.data() };
    SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };

    node.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_LE(std::abs(sample), 1.0f);
    }
}

TEST(Unit_Dsp, SineToneNodeProducesExpectedFrequency)
{
    constexpr double sample_rate = 48000.0;
    constexpr float frequency = 1000.0f;
    SushiDSP::SineToneNode node(frequency);
    SushiDSP::PrepareInfo info{ sample_rate, 480, 1 };
    node.prepare(info);

    std::vector<float> output_buffer(480, 0.0f);
    float* output_channels[1] = { output_buffer.data() };
    SushiDSP::ProcessContext context{ output_channels, nullptr, 480, 0.0 };

    node.process(context);

    int zero_crossings = 0;
    for (std::size_t i = 1; i < output_buffer.size(); ++i)
    {
        if ((output_buffer[i - 1] < 0.0f) != (output_buffer[i] < 0.0f))
        {
            ++zero_crossings;
        }
    }

    const double expected_crossings = 2.0 * frequency * (480.0 / sample_rate);
    EXPECT_NEAR(zero_crossings, expected_crossings, 2.0);
}

TEST(Unit_Dsp, SineToneNodeResetRestartsPhaseFromZero)
{
    SushiDSP::SineToneNode node(440.0f);
    SushiDSP::PrepareInfo info{ 48000.0, 8, 1 };
    node.prepare(info);

    std::vector<float> first_buffer(8, 0.0f);
    float* first_channels[1] = { first_buffer.data() };
    SushiDSP::ProcessContext first_context{ first_channels, nullptr, 8, 0.0 };
    node.process(first_context);

    node.reset();

    std::vector<float> second_buffer(8, 0.0f);
    float* second_channels[1] = { second_buffer.data() };
    SushiDSP::ProcessContext second_context{ second_channels, nullptr, 8, 0.0 };
    node.process(second_context);

    for (std::size_t i = 0; i < first_buffer.size(); ++i)
    {
        EXPECT_NEAR(first_buffer[i], second_buffer[i], 1e-5f);
    }
}
```

- [ ] **Step 2: Add the test file and its extra source to `tests/CMakeLists.txt`, run to verify it fails**

Edit `tests/CMakeLists.txt`'s `add_executable(sushidsp_tests ...)` source list to add `functional/unit/test_sine_tone_node.cpp` and `${CMAKE_SOURCE_DIR}/apps/host/SineToneNode.cpp`, and add a `target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/apps/host)` line.

Run: `python cli/sd.py test`
Expected: FAIL — `SineToneNode.hpp` does not exist.

- [ ] **Step 3: Write `apps/host/SineToneNode.hpp`**

```cpp
/**************************************************************************/
/* SineToneNode.hpp                                                       */
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
/* permissions and limitations under the License.                        */
/**************************************************************************/

#pragma once

#include "SushiDSP/core/IAudioNode.hpp"

namespace SushiDSP
{
    /// @brief Diagnostic sine-wave generator for the standalone host's device sanity check.
    /// Not a product — no NodeDescriptor, not part of the public SushiDSP API.
    class SineToneNode : public IAudioNode
    {
    public:
        /// @brief Constructs a sine generator at the given frequency.
        /// @param frequency_hz The tone's frequency in Hz.
        explicit SineToneNode(float frequency_hz);

        void prepare(const PrepareInfo& info) override;

        void process(ProcessContext& context) override;

        PortLayout ports() const override;

        void reset() override;

    private:
        float  frequency_hz_;
        double sample_rate_ = 48000.0;
        double phase_ = 0.0;
    };
}
```

- [ ] **Step 4: Write `apps/host/SineToneNode.cpp`**

```cpp
/**************************************************************************/
/* SineToneNode.cpp                                                       */
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
/* permissions and limitations under the License.                        */
/**************************************************************************/

#include "SineToneNode.hpp"

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SushiDSP
{
    SineToneNode::SineToneNode(float frequency_hz)
        : frequency_hz_(frequency_hz)
    {
    }

    void SineToneNode::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;
    }

    void SineToneNode::process(ProcessContext& context)
    {
        const double phase_increment = 2.0 * M_PI * static_cast<double>(frequency_hz_) / sample_rate_;

        for (std::uint32_t frame = 0; frame < context.frames; ++frame)
        {
            context.outputs[0][frame] = static_cast<float>(std::sin(phase_));
            phase_ += phase_increment;
            if (phase_ >= 2.0 * M_PI)
            {
                phase_ -= 2.0 * M_PI;
            }
        }
    }

    PortLayout SineToneNode::ports() const
    {
        return { 0, 1 };
    }

    void SineToneNode::reset()
    {
        phase_ = 0.0;
    }
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `python cli/sd.py test`
Expected: PASS — all three `SineToneNode*` cases under suite `Unit_Dsp`.

- [ ] **Step 6: Commit**

```bash
git add apps/host/SineToneNode.hpp apps/host/SineToneNode.cpp tests/functional/unit/test_sine_tone_node.cpp tests/CMakeLists.txt
git commit -m "Add diagnostic SineToneNode for host device sanity check"
```

---

## Task 5: `apps/host` standalone app

**Files:**
- Create: `apps/host/CMakeLists.txt`
- Create: `apps/host/main.cpp`
- Modify: `CMakeLists.txt` (add `add_subdirectory(apps/host)`)

**Interfaces:**
- Consumes: `SushiDSP::RigGraph`, `NodeHandle` (D0 Task 7); `SushiDSP::Sdl2AudioDevice` (Task 3); `SushiDSP::SineToneNode` (Task 4); `SushiDSP::PrepareInfo`, `ProcessContext` (D0 Task 3).
- Produces: the `sushidsp_host` executable target. No other task consumes this — it's the terminal app of this plan.

This task has no automated test (it's a `main()` that opens real hardware and runs until interrupted) — verification is manual per this plan's Self-Review Notes. The build step below is the automated check.

- [ ] **Step 1: Write `apps/host/CMakeLists.txt`**

```cmake
add_executable(sushidsp_host
    main.cpp
    SineToneNode.cpp
)
target_link_libraries(sushidsp_host PRIVATE SushiDSP::sushi_dsp_sdl2)
```

- [ ] **Step 2: Write `apps/host/main.cpp`**

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
/* permissions and limitations under the License.                        */
/**************************************************************************/

#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

#include "SineToneNode.hpp"
#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/device/Sdl2AudioDevice.hpp"

int main()
{
    SushiDSP::DeviceFormat requested{ 48000.0, 256, 1 };

    std::unique_ptr<SushiDSP::Sdl2AudioDevice> device;
    try
    {
        device = std::make_unique<SushiDSP::Sdl2AudioDevice>(requested);
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Failed to open audio device: %s\n", error.what());
        return 1;
    }

    const SushiDSP::DeviceFormat actual = device->format();
    std::printf(
        "Opened audio device: %.0f Hz, %u frames/block, %u channel(s)\n",
        actual.sample_rate, actual.block_size, actual.channels);

    SushiDSP::RigGraph rig(actual.block_size, 4);
    rig.add_node(std::make_unique<SushiDSP::SineToneNode>(440.0f));

    SushiDSP::PrepareInfo info{ actual.sample_rate, actual.block_size, actual.channels };
    rig.compile(info);

    device->start([&rig](float* const* outputs, std::uint32_t frames)
    {
        SushiDSP::ProcessContext context{ outputs, nullptr, frames, 0.0 };
        rig.process(context);
    });

    std::printf("Playing a 440 Hz test tone for 3 seconds...\n");
    std::this_thread::sleep_for(std::chrono::seconds(3));

    device->stop();
    std::printf("Stopped.\n");

    return 0;
}
```

- [ ] **Step 3: Add `add_subdirectory(apps/host)` to the root `CMakeLists.txt`**

Add after the `sushi_dsp_sdl2` target definition from Task 3, before `enable_testing()`:

```cmake
add_subdirectory(apps/host)
```

- [ ] **Step 4: Build to verify it compiles and links**

Run: `python cli/sd.py build`
Expected: SUCCESS — `sushidsp_host` executable builds. Do NOT run the executable as part of automated verification (it opens a real audio device and blocks for 3 seconds of playback) — building successfully is this task's pass condition. If a human wants to manually verify audio output, they can run `build/apps/host/Debug/sushidsp_host.exe` (or the equivalent path for their build config) themselves and confirm they hear a 440 Hz tone.

- [ ] **Step 5: Run the full test suite once to confirm no regressions from the CMakeLists.txt changes**

Run: `python cli/sd.py test`
Expected: PASS — 100% of `sushidsp_tests`, label `unit`, zero failures. Confirm the exact count from the actual suite output (D0 shipped 15; this plan's Tasks 1/2/4 add 5/3/3 = 11 more; do not assume 26 without checking — count from the real ctest summary line).

- [ ] **Step 6: Commit**

```bash
git add apps/host/CMakeLists.txt apps/host/main.cpp CMakeLists.txt
git commit -m "Add sushidsp_host standalone app"
```

---

## Self-Review Notes

- **Spec coverage:** §7.1 IAudioDevice → Task 2/3. §7.2 standalone host → Task 5 ("opens an IAudioDevice... runs it" — the D1 scope explicitly excludes the "lets the user build/edit a rig" and "simple parameter UI" parts of §7.2, since those require a rig-editor UI which has no D1 roadmap line; §9's D1 entry only promises "generic apps/host with a minimal rig editor" and "sound out the speakers, zero XRuns" — this plan ships the device+playback half and treats the rig-editor UI as a gap to flag explicitly rather than silently drop). §7.4 SpscRing → Task 1 (declared but not yet wired into the host's parameter path, since D1's host has no live parameter editing to carry — the ring is proven correct and ready for D3/D4 to use once products expose parameters). §9 D1 line ("IAudioDevice (SDL2), RT-safe callback, SpscRing, generic apps/host... Ships: sound out the speakers, zero XRuns") → Tasks 1, 2, 3, 5 cover the device/callback/host chain; "zero XRuns" is a claim only a human listening to real hardware can verify, so it is explicitly out of this plan's automated-test scope and flagged for manual verification.
- **Gap flagged explicitly (not silently dropped):** the "minimal rig editor" UI in §7.2/§9 is NOT built in this plan — D1's host hardcodes one `SineToneNode` rather than letting a user add/remove/reorder nodes interactively. A rig-editor UI (text-based menu, or later a real GUI) is deferred to a follow-up plan once D3/D4 land real products worth editing a rig around; building a UI to edit a rig containing only a sine tone would be premature.
- **Conventions coverage:** every new source file in this plan uses the 76-column boxed license header pinned in spec §0a (see the Reference Header block above), every test lives under `tests/functional/unit/test_<snake_case>.cpp`, every GTest case uses suite name `Unit_Dsp`, and every test file is added to the single `sushidsp_tests` binary via `tests/CMakeLists.txt` — matching the D0 conventions retrofit rather than the pre-retrofit `tests/unit_dsp/PascalCaseTest.cpp`/`UnitDsp` shape.
- **Placeholder scan:** none found — every step has concrete code or a concrete shell command; the two hardware-dependent tasks (3, 5) explicitly say "no automated test" rather than inventing a fake one.
- **Type consistency:** `DeviceFormat{ sample_rate, block_size, channels }` field order and types match across `IAudioDevice.hpp`, `test_audio_device.cpp`, `Sdl2AudioDevice.hpp/.cpp`, and `main.cpp`. `DeviceCallback` signature (`float* const* outputs, std::uint32_t frames`) matches between `IAudioDevice.hpp`'s `start()` and every call site. `SineToneNode` implements the exact `IAudioNode` virtual signatures from D0 Task 3, verified against `include/SushiDSP/core/IAudioNode.hpp` as it exists after D0 merge.

---

**Plan complete and saved to `docs/superpowers/plans/2026-08-25-d1-device-seam-standalone-host.md`.** Executing via subagent-driven-development, continuing directly per standing instruction to proceed without pausing for confirmation between phases.
