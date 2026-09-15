# SushiDSP D0 — Skeleton + Math Kernels Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stand up the SushiDSP repository skeleton — build system, `sd` CLI, the core `IAudioNode`/rig-graph/`NodeDescriptor` types, the transient buffer pool, and the first math kernels (FFT, biquad/SVF filters) — with golden-vector unit tests proving numerical correctness. This is the D0 phase from `docs/design/sushidsp.md` §9.

**Architecture:** A CMake C++17 project with zero third-party runtime dependencies. `include/SushiDSP/core` holds the graph/node/descriptor types; `include/SushiDSP/math` holds FFT/filter kernels; `src/` holds their implementations; `tests/` holds GoogleTest-based `Unit_Dsp` golden-vector tests. A headless test target proves a graph of nodes processes to a verified float buffer with no allocation on the processing thread.

**Tech Stack:** C++17, CMake, GoogleTest (fetched via CMake `FetchContent`, test-only dependency), Python Typer for the `sd` CLI.

**Spec:** `docs/design/sushidsp.md` (§0, §3, §4.1, §4.3, §8, §9 — D0)

## Global Constraints

- C++17 only — no C++20/23 features.
- Allman braces (opening brace on its own line); trivial one-line accessors may stay compact.
- Naming: types `PascalCase`, functions/variables `snake_case`, class members trailing underscore (`impl_`), constants `UPPER_SNAKE`, no abbreviations.
- Apache 2.0 license header box on every source file; line 2 of the header carries the file's own name.
- Doxygen `@brief`/`@param`/`@return` on every public function.
- No comments unless the *why* is non-obvious — never comment *what* the code does.
- `sushi_dsp` core target: zero external dependencies beyond the C++17 standard library and SIMD intrinsics. Test targets may depend on GoogleTest.
- `IAudioNode::process()` must be allocation-free, lock-free, syscall-free, and throw-free — this is verified in Task 8's RT-safety test.
- Block size default 256 frames, sample rate default 48000 Hz (spec §4, "Fixed capacities" table).

---

## File Structure

```
sushidsp/
  CMakeLists.txt
  cli/
    sd.py                              # Typer CLI: sd build / sd test
  include/SushiDSP/
    core/
      PortLayout.hpp                   # PortLayout POD
      IAudioNode.hpp                   # PrepareInfo, ProcessContext, IAudioNode
      NodeDescriptor.hpp               # ParameterDescriptor, ParamKind, NodeDescriptor
      BufferPool.hpp                   # transient buffer pool
      RigGraph.hpp                     # graph compile + process
    math/
      Fft.hpp                          # split-radix real FFT
      Biquad.hpp                       # RBJ biquad filter
  src/
    core/
      BufferPool.cpp
      RigGraph.cpp
    math/
      Fft.cpp
      Biquad.cpp
  tests/
    unit_dsp/
      CMakeLists.txt
      FftTest.cpp
      BiquadTest.cpp
      RigGraphTest.cpp
      RtSafetyTest.cpp
```

Each header/source pair has one responsibility: `PortLayout` is a POD with no logic; `IAudioNode.hpp` defines only the node contract (no implementation); `NodeDescriptor.hpp` defines the product-registration POD; `BufferPool` owns transient-buffer lifetime; `RigGraph` owns topological sort + per-block replay; `Fft`/`Biquad` are independent, dependency-free math kernels each testable in isolation.

---

## Task 1: Repository scaffold + CMake skeleton

**Files:**
- Create: `CMakeLists.txt`
- Create: `.gitignore`
- Create: `LICENSE` (Apache 2.0 full text)
- Create: `include/SushiDSP/core/PortLayout.hpp`

**Interfaces:**
- Produces: `SushiDSP::PortLayout { std::uint32_t input_channels; std::uint32_t output_channels; }`, target `sushi_dsp` (CMake `INTERFACE`/`STATIC` library, C++17, no deps).

- [ ] **Step 1: Create the top-level CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.20)
project(SushiDSP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(sushi_dsp
    src/core/BufferPool.cpp
    src/core/RigGraph.cpp
    src/math/Fft.cpp
    src/math/Biquad.cpp
)
add_library(SushiDSP::sushi_dsp ALIAS sushi_dsp)
target_include_directories(sushi_dsp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

enable_testing()
add_subdirectory(tests/unit_dsp)
```

- [ ] **Step 2: Create `.gitignore`**

```
build/
*.obj
*.o
CMakeCache.txt
CMakeFiles/
```

- [ ] **Step 3: Add the Apache 2.0 `LICENSE` file at the repo root**

Fetch the standard Apache 2.0 license text (unmodified, no header box — that box is per-source-file only) and save it verbatim to `LICENSE`.

- [ ] **Step 4: Create `include/SushiDSP/core/PortLayout.hpp`**

```cpp
// PortLayout.hpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <cstdint>

namespace SushiDSP
{
    /// @brief Describes the fixed input/output channel counts of an IAudioNode.
    struct PortLayout
    {
        std::uint32_t input_channels;
        std::uint32_t output_channels;
    };
}
```

- [ ] **Step 5: Verify the build configures with no source files yet compiling against it**

Run: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
Expected: configure succeeds (the `add_library` step will fail to compile until Tasks 2-6 add the referenced `.cpp` files — that's expected at this point; do not run `cmake --build` yet). If CMake errors on missing source files, comment out the not-yet-created `.cpp` entries in `add_library` and note them for the corresponding task to re-add.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt .gitignore LICENSE include/SushiDSP/core/PortLayout.hpp
git commit -m "Add SushiDSP repo scaffold and CMake skeleton"
```

---

## Task 2: `sd` developer CLI

**Files:**
- Create: `cli/sd.py`
- Create: `cli/requirements.txt`

**Interfaces:**
- Produces: `sd build` (runs `cmake --build build`), `sd test` (runs `ctest --test-dir build --output-on-failure`), `sd configure` (runs `cmake -S . -B build`).

- [ ] **Step 1: Write `cli/requirements.txt`**

```
typer[all]>=0.12
```

- [ ] **Step 2: Write `cli/sd.py`**

```python
# sd.py
#
# Copyright 2026 SushiDSP Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import subprocess
import sys
from pathlib import Path

import typer

app = typer.Typer()
ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = ROOT / "build"


def run(cmd: list[str]) -> None:
    result = subprocess.run(cmd, cwd=ROOT)
    if result.returncode != 0:
        raise typer.Exit(result.returncode)


@app.command()
def configure() -> None:
    run(["cmake", "-S", str(ROOT), "-B", str(BUILD_DIR), "-DCMAKE_BUILD_TYPE=Debug"])


@app.command()
def build() -> None:
    if not BUILD_DIR.exists():
        configure()
    run(["cmake", "--build", str(BUILD_DIR)])


@app.command()
def test() -> None:
    build()
    run(["ctest", "--test-dir", str(BUILD_DIR), "--output-on-failure"])


if __name__ == "__main__":
    app()
```

- [ ] **Step 3: Verify `sd configure` runs**

Run: `python cli/sd.py configure`
Expected: CMake configure step succeeds (same as Task 1 Step 5).

- [ ] **Step 4: Commit**

```bash
git add cli/sd.py cli/requirements.txt
git commit -m "Add sd developer CLI (configure/build/test)"
```

---

## Task 3: `IAudioNode` and `NodeDescriptor` core headers

**Files:**
- Create: `include/SushiDSP/core/IAudioNode.hpp`
- Create: `include/SushiDSP/core/NodeDescriptor.hpp`
- Test: `tests/unit_dsp/RigGraphTest.cpp` (compile-only smoke test added here; graph logic tested in Task 6)
- Modify: `tests/unit_dsp/CMakeLists.txt` (created in this task)

**Interfaces:**
- Consumes: `SushiDSP::PortLayout` (Task 1).
- Produces: `SushiDSP::PrepareInfo`, `SushiDSP::ProcessContext`, `SushiDSP::IAudioNode` (abstract), `SushiDSP::ParamKind`, `SushiDSP::ParameterDescriptor`, `SushiDSP::NodeDescriptor`. Later tasks (5, 6) implement and consume `IAudioNode`.

- [ ] **Step 1: Write `include/SushiDSP/core/IAudioNode.hpp`**

```cpp
// IAudioNode.hpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <cstdint>

#include "SushiDSP/core/PortLayout.hpp"

namespace SushiDSP
{
    /// @brief Parameters supplied to IAudioNode::prepare(), fixed for the node's lifetime.
    struct PrepareInfo
    {
        double        sample_rate;
        std::uint32_t max_block;
        std::uint32_t channels;
    };

    /// @brief Per-block processing context. process() must fill exactly [0, frames) of each output channel.
    struct ProcessContext
    {
        float* const*       outputs;
        const float* const* inputs;
        std::uint32_t       frames;
        double               time_seconds;
    };

    /// @brief One DSP graph node. Implementations must be real-time safe in process().
    struct IAudioNode
    {
        virtual ~IAudioNode() = default;

        /// @brief Acquires all memory the node will need. Never called on the processing thread.
        /// @param info Sample rate, max block size, and channel count for this node's lifetime.
        virtual void prepare(const PrepareInfo& info) = 0;

        /// @brief Fills context.outputs with exactly context.frames of audio.
        /// @param context The per-block input/output buffers and frame count.
        /// No allocation, no lock, no syscall, no exception.
        virtual void process(ProcessContext& context) = 0;

        /// @brief Returns this node's fixed input/output channel counts.
        /// @return The node's PortLayout.
        virtual PortLayout ports() const = 0;

        /// @brief Clears internal state for a click-free restart.
        virtual void reset() = 0;
    };
}
```

- [ ] **Step 2: Write `include/SushiDSP/core/NodeDescriptor.hpp`**

```cpp
// NodeDescriptor.hpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <memory>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/PortLayout.hpp"

namespace SushiDSP
{
    /// @brief How a ParameterDescriptor's value maps to a host automation control.
    enum class ParamKind
    {
        Linear,
        Logarithmic,
        Decibel,
        Boolean,
        Choice
    };

    /// @brief One automatable parameter exposed by a product's NodeDescriptor.
    struct ParameterDescriptor
    {
        const char* id;
        const char* label;
        float       min;
        float       max;
        float       default_value;
        ParamKind   kind;
        const char* unit;
    };

    /// @brief Everything a host needs to instantiate and drive a product, with no knowledge of its internals.
    struct NodeDescriptor
    {
        const char*                id;
        const char*                name;
        const ParameterDescriptor* parameters;
        std::size_t                parameter_count;
        PortLayout                 ports;
        std::unique_ptr<IAudioNode> (*make)(const PrepareInfo&);
    };
}
```

- [ ] **Step 3: Create `tests/unit_dsp/CMakeLists.txt` with GoogleTest via FetchContent**

```cmake
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(unit_dsp
    RigGraphTest.cpp
)
target_link_libraries(unit_dsp PRIVATE SushiDSP::sushi_dsp GTest::gtest_main)
target_include_directories(unit_dsp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

include(GoogleTest)
gtest_discover_tests(unit_dsp)
```

- [ ] **Step 4: Write a compile-only smoke test in `tests/unit_dsp/RigGraphTest.cpp`**

```cpp
// RigGraphTest.cpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"

TEST(UnitDsp, PortLayoutIsPlainOldData)
{
    SushiDSP::PortLayout layout{ 1, 2 };
    EXPECT_EQ(layout.input_channels, 1u);
    EXPECT_EQ(layout.output_channels, 2u);
}
```

- [ ] **Step 5: Build and run**

Run: `python cli/sd.py test`
Expected: `unit_dsp` builds (this will pull GoogleTest on first run — allow extra time) and `PortLayoutIsPlainOldData` passes.

- [ ] **Step 6: Commit**

```bash
git add include/SushiDSP/core/IAudioNode.hpp include/SushiDSP/core/NodeDescriptor.hpp tests/unit_dsp/CMakeLists.txt tests/unit_dsp/RigGraphTest.cpp
git commit -m "Add IAudioNode and NodeDescriptor core contracts"
```

---

## Task 4: Transient buffer pool

**Files:**
- Create: `include/SushiDSP/core/BufferPool.hpp`
- Create: `src/core/BufferPool.cpp`
- Test: `tests/unit_dsp/BufferPoolTest.cpp`
- Modify: `tests/unit_dsp/CMakeLists.txt:9` (add `BufferPoolTest.cpp` to `add_executable(unit_dsp ...)`)

**Interfaces:**
- Produces: `SushiDSP::BufferPool` with `float* acquire(std::uint32_t frames)`, `void release(float* buffer)`, constructed with `BufferPool(std::uint32_t block_size, std::uint32_t max_live_buffers)`. Preallocates all storage in the constructor; `acquire`/`release` never allocate. Task 6 (RigGraph) consumes this.

- [ ] **Step 1: Write the failing test `tests/unit_dsp/BufferPoolTest.cpp`**

```cpp
// BufferPoolTest.cpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>

#include "SushiDSP/core/BufferPool.hpp"

TEST(UnitDsp, BufferPoolAcquireReturnsDistinctBuffersUpToCapacity)
{
    SushiDSP::BufferPool pool(256, 2);

    float* a = pool.acquire(256);
    float* b = pool.acquire(256);

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_NE(a, b);
}

TEST(UnitDsp, BufferPoolReusesReleasedBuffer)
{
    SushiDSP::BufferPool pool(256, 1);

    float* a = pool.acquire(256);
    pool.release(a);
    float* b = pool.acquire(256);

    EXPECT_EQ(a, b);
}
```

- [ ] **Step 2: Add the test file to CMake and run to verify it fails**

Edit `tests/unit_dsp/CMakeLists.txt` line with `add_executable(unit_dsp RigGraphTest.cpp)` to:

```cmake
add_executable(unit_dsp
    RigGraphTest.cpp
    BufferPoolTest.cpp
)
```

Run: `python cli/sd.py test`
Expected: FAIL — compile error, `SushiDSP/core/BufferPool.hpp` does not exist.

- [ ] **Step 3: Write `include/SushiDSP/core/BufferPool.hpp`**

```cpp
// BufferPool.hpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <cstdint>
#include <vector>

namespace SushiDSP
{
    /// @brief Fixed-capacity pool of block-sized float buffers with allocation-free acquire/release.
    class BufferPool
    {
    public:
        /// @brief Preallocates max_live_buffers buffers of block_size floats each.
        /// @param block_size Number of floats per buffer.
        /// @param max_live_buffers Maximum number of buffers live at once.
        BufferPool(std::uint32_t block_size, std::uint32_t max_live_buffers);

        /// @brief Returns a buffer of at least frames floats, or nullptr if the pool is exhausted.
        /// @param frames Number of frames the caller needs.
        /// @return Pointer to a block_size-float buffer, or nullptr.
        float* acquire(std::uint32_t frames);

        /// @brief Returns a buffer previously obtained from acquire() to the pool.
        /// @param buffer Pointer previously returned by acquire().
        void release(float* buffer);

    private:
        std::uint32_t         block_size_;
        std::vector<float>    storage_;
        std::vector<bool>     slot_live_;
    };
}
```

- [ ] **Step 4: Write `src/core/BufferPool.cpp`**

```cpp
// BufferPool.cpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "SushiDSP/core/BufferPool.hpp"

namespace SushiDSP
{
    BufferPool::BufferPool(std::uint32_t block_size, std::uint32_t max_live_buffers)
        : block_size_(block_size)
        , storage_(static_cast<std::size_t>(block_size) * max_live_buffers, 0.0f)
        , slot_live_(max_live_buffers, false)
    {
    }

    float* BufferPool::acquire(std::uint32_t frames)
    {
        if (frames > block_size_)
        {
            return nullptr;
        }

        for (std::size_t slot = 0; slot < slot_live_.size(); ++slot)
        {
            if (!slot_live_[slot])
            {
                slot_live_[slot] = true;
                return storage_.data() + slot * block_size_;
            }
        }

        return nullptr;
    }

    void BufferPool::release(float* buffer)
    {
        const std::size_t offset = static_cast<std::size_t>(buffer - storage_.data());
        const std::size_t slot   = offset / block_size_;
        slot_live_[slot] = false;
    }
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `python cli/sd.py test`
Expected: PASS — both `BufferPoolAcquireReturnsDistinctBuffersUpToCapacity` and `BufferPoolReusesReleasedBuffer`.

- [ ] **Step 6: Commit**

```bash
git add include/SushiDSP/core/BufferPool.hpp src/core/BufferPool.cpp tests/unit_dsp/BufferPoolTest.cpp tests/unit_dsp/CMakeLists.txt
git commit -m "Add allocation-free transient BufferPool"
```

---

## Task 5: FFT kernel (split-radix real FFT)

**Files:**
- Create: `include/SushiDSP/math/Fft.hpp`
- Create: `src/math/Fft.cpp`
- Test: `tests/unit_dsp/FftTest.cpp`
- Modify: `tests/unit_dsp/CMakeLists.txt` (add `FftTest.cpp`)

**Interfaces:**
- Produces: `SushiDSP::Fft` class with `explicit Fft(std::uint32_t size)` (size must be a power of two), `void forward(const float* time_domain, float* real_out, float* imag_out) const`, `void inverse(const float* real_in, const float* imag_in, float* time_domain_out) const`.

- [ ] **Step 1: Write the failing golden-vector test `tests/unit_dsp/FftTest.cpp`**

```cpp
// FftTest.cpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/math/Fft.hpp"

TEST(UnitDsp, FftOfDcSignalHasEnergyOnlyInBinZero)
{
    constexpr std::uint32_t size = 64;
    SushiDSP::Fft fft(size);

    std::vector<float> time_domain(size, 1.0f);
    std::vector<float> real_out(size, 0.0f);
    std::vector<float> imag_out(size, 0.0f);

    fft.forward(time_domain.data(), real_out.data(), imag_out.data());

    EXPECT_NEAR(real_out[0], static_cast<float>(size), 1e-3f);
    for (std::uint32_t bin = 1; bin < size; ++bin)
    {
        EXPECT_NEAR(real_out[bin], 0.0f, 1e-3f);
        EXPECT_NEAR(imag_out[bin], 0.0f, 1e-3f);
    }
}

TEST(UnitDsp, FftOfSingleBinSineMatchesKnownBin)
{
    constexpr std::uint32_t size = 64;
    constexpr std::uint32_t target_bin = 4;
    SushiDSP::Fft fft(size);

    std::vector<float> time_domain(size);
    for (std::uint32_t n = 0; n < size; ++n)
    {
        time_domain[n] = std::cos(2.0f * static_cast<float>(M_PI) * target_bin * n / size);
    }
    std::vector<float> real_out(size, 0.0f);
    std::vector<float> imag_out(size, 0.0f);

    fft.forward(time_domain.data(), real_out.data(), imag_out.data());

    const float expected_magnitude = static_cast<float>(size) / 2.0f;
    const float magnitude_at_target =
        std::sqrt(real_out[target_bin] * real_out[target_bin] + imag_out[target_bin] * imag_out[target_bin]);
    EXPECT_NEAR(magnitude_at_target, expected_magnitude, 1e-2f);
}

TEST(UnitDsp, ForwardThenInverseFftIsIdentity)
{
    constexpr std::uint32_t size = 64;
    SushiDSP::Fft fft(size);

    std::vector<float> original(size);
    for (std::uint32_t n = 0; n < size; ++n)
    {
        original[n] = std::sin(2.0f * static_cast<float>(M_PI) * 3.0f * n / size) + 0.5f;
    }

    std::vector<float> real_out(size, 0.0f);
    std::vector<float> imag_out(size, 0.0f);
    std::vector<float> reconstructed(size, 0.0f);

    fft.forward(original.data(), real_out.data(), imag_out.data());
    fft.inverse(real_out.data(), imag_out.data(), reconstructed.data());

    for (std::uint32_t n = 0; n < size; ++n)
    {
        EXPECT_NEAR(reconstructed[n], original[n], 1e-3f);
    }
}
```

- [ ] **Step 2: Add `FftTest.cpp` to CMake and run to verify it fails**

Edit `tests/unit_dsp/CMakeLists.txt`:

```cmake
add_executable(unit_dsp
    RigGraphTest.cpp
    BufferPoolTest.cpp
    FftTest.cpp
)
```

Run: `python cli/sd.py test`
Expected: FAIL — `SushiDSP/math/Fft.hpp` does not exist.

- [ ] **Step 3: Write `include/SushiDSP/math/Fft.hpp`**

```cpp
// Fft.hpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <cstdint>
#include <vector>

namespace SushiDSP
{
    /// @brief Power-of-two, in-place-capable real FFT. Precomputes twiddle factors and bit-reversal in the constructor.
    class Fft
    {
    public:
        /// @brief Constructs an FFT plan for the given power-of-two size.
        /// @param size Transform size; must be a power of two.
        explicit Fft(std::uint32_t size);

        /// @brief Computes the forward DFT of a real time-domain signal.
        /// @param time_domain Input, size() samples.
        /// @param real_out Output real part, size() bins.
        /// @param imag_out Output imaginary part, size() bins.
        void forward(const float* time_domain, float* real_out, float* imag_out) const;

        /// @brief Computes the inverse DFT back to a real time-domain signal.
        /// @param real_in Input real part, size() bins.
        /// @param imag_in Input imaginary part, size() bins.
        /// @param time_domain_out Output, size() samples.
        void inverse(const float* real_in, const float* imag_in, float* time_domain_out) const;

        /// @brief The transform size this plan was constructed for.
        /// @return The FFT size.
        std::uint32_t size() const;

    private:
        std::uint32_t              size_;
        std::vector<std::uint32_t> bit_reversal_;
        std::vector<float>         twiddle_real_;
        std::vector<float>         twiddle_imag_;
    };
}
```

- [ ] **Step 4: Write `src/math/Fft.cpp` (radix-2 Cooley-Tukey; "split-radix" structure documented for later optimization, correctness first)**

```cpp
// Fft.cpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "SushiDSP/math/Fft.hpp"

#include <cassert>
#include <cmath>

namespace SushiDSP
{
    namespace
    {
        std::uint32_t reverse_bits(std::uint32_t value, std::uint32_t bit_count)
        {
            std::uint32_t result = 0;
            for (std::uint32_t bit = 0; bit < bit_count; ++bit)
            {
                result = (result << 1) | (value & 1u);
                value >>= 1;
            }
            return result;
        }

        std::uint32_t log2_of_power_of_two(std::uint32_t value)
        {
            std::uint32_t log = 0;
            while ((1u << log) < value)
            {
                ++log;
            }
            return log;
        }

        void transform(float* real, float* imag, std::uint32_t size, const std::vector<std::uint32_t>& bit_reversal, bool inverse)
        {
            for (std::uint32_t i = 0; i < size; ++i)
            {
                const std::uint32_t j = bit_reversal[i];
                if (j > i)
                {
                    std::swap(real[i], real[j]);
                    std::swap(imag[i], imag[j]);
                }
            }

            const float sign = inverse ? 1.0f : -1.0f;
            for (std::uint32_t span = 1; span < size; span <<= 1)
            {
                const float angle_step = sign * static_cast<float>(M_PI) / static_cast<float>(span);
                for (std::uint32_t group_start = 0; group_start < size; group_start += span * 2)
                {
                    for (std::uint32_t k = 0; k < span; ++k)
                    {
                        const float angle = angle_step * static_cast<float>(k);
                        const float cos_a = std::cos(angle);
                        const float sin_a = std::sin(angle);

                        const std::uint32_t even = group_start + k;
                        const std::uint32_t odd  = even + span;

                        const float odd_real = real[odd] * cos_a - imag[odd] * sin_a;
                        const float odd_imag = real[odd] * sin_a + imag[odd] * cos_a;

                        real[odd]  = real[even] - odd_real;
                        imag[odd]  = imag[even] - odd_imag;
                        real[even] = real[even] + odd_real;
                        imag[even] = imag[even] + odd_imag;
                    }
                }
            }

            if (inverse)
            {
                for (std::uint32_t i = 0; i < size; ++i)
                {
                    real[i] /= static_cast<float>(size);
                    imag[i] /= static_cast<float>(size);
                }
            }
        }
    }

    Fft::Fft(std::uint32_t size)
        : size_(size)
        , bit_reversal_(size)
    {
        assert((size & (size - 1)) == 0 && "Fft size must be a power of two");
        const std::uint32_t bit_count = log2_of_power_of_two(size);
        for (std::uint32_t i = 0; i < size; ++i)
        {
            bit_reversal_[i] = reverse_bits(i, bit_count);
        }
    }

    void Fft::forward(const float* time_domain, float* real_out, float* imag_out) const
    {
        for (std::uint32_t i = 0; i < size_; ++i)
        {
            real_out[i] = time_domain[i];
            imag_out[i] = 0.0f;
        }
        transform(real_out, imag_out, size_, bit_reversal_, false);
    }

    void Fft::inverse(const float* real_in, const float* imag_in, float* time_domain_out) const
    {
        std::vector<float> imag_scratch(imag_in, imag_in + size_);
        std::vector<float> real_scratch(real_in, real_in + size_);
        transform(real_scratch.data(), imag_scratch.data(), size_, bit_reversal_, true);
        for (std::uint32_t i = 0; i < size_; ++i)
        {
            time_domain_out[i] = real_scratch[i];
        }
    }

    std::uint32_t Fft::size() const
    {
        return size_;
    }
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `python cli/sd.py test`
Expected: PASS — `FftOfDcSignalHasEnergyOnlyInBinZero`, `FftOfSingleBinSineMatchesKnownBin`, `ForwardThenInverseFftIsIdentity`.

- [ ] **Step 6: Commit**

```bash
git add include/SushiDSP/math/Fft.hpp src/math/Fft.cpp tests/unit_dsp/FftTest.cpp tests/unit_dsp/CMakeLists.txt
git commit -m "Add golden-vector-tested real FFT kernel"
```

---

## Task 6: Biquad filter (RBJ cookbook)

**Files:**
- Create: `include/SushiDSP/math/Biquad.hpp`
- Create: `src/math/Biquad.cpp`
- Test: `tests/unit_dsp/BiquadTest.cpp`
- Modify: `tests/unit_dsp/CMakeLists.txt` (add `BiquadTest.cpp`)

**Interfaces:**
- Produces: `SushiDSP::Biquad` with `void set_low_pass(double sample_rate, double cutoff_hz, double q)`, `float process_sample(float input)`, `void reset()`.

- [ ] **Step 1: Write the failing test `tests/unit_dsp/BiquadTest.cpp`**

```cpp
// BiquadTest.cpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/math/Biquad.hpp"

TEST(UnitDsp, BiquadLowPassPassesDcAtUnityGain)
{
    SushiDSP::Biquad filter;
    filter.set_low_pass(48000.0, 1000.0, 0.707);

    float output = 0.0f;
    for (int i = 0; i < 10000; ++i)
    {
        output = filter.process_sample(1.0f);
    }

    EXPECT_NEAR(output, 1.0f, 1e-3f);
}

TEST(UnitDsp, BiquadLowPassAttenuatesWellAboveCutoff)
{
    SushiDSP::Biquad filter;
    constexpr double sample_rate = 48000.0;
    constexpr double cutoff = 500.0;
    filter.set_low_pass(sample_rate, cutoff, 0.707);

    const double test_freq = 10000.0;
    float max_output = 0.0f;
    for (int i = 0; i < 2000; ++i)
    {
        const float input = static_cast<float>(std::sin(2.0 * M_PI * test_freq * i / sample_rate));
        const float output = filter.process_sample(input);
        if (i > 1000)
        {
            max_output = std::max(max_output, std::abs(output));
        }
    }

    EXPECT_LT(max_output, 0.1f);
}

TEST(UnitDsp, BiquadResetClearsState)
{
    SushiDSP::Biquad filter;
    filter.set_low_pass(48000.0, 1000.0, 0.707);

    for (int i = 0; i < 100; ++i)
    {
        filter.process_sample(1.0f);
    }
    filter.reset();

    const float output = filter.process_sample(0.0f);
    EXPECT_NEAR(output, 0.0f, 1e-3f);
}
```

- [ ] **Step 2: Add `BiquadTest.cpp` to CMake and run to verify it fails**

Edit `tests/unit_dsp/CMakeLists.txt`:

```cmake
add_executable(unit_dsp
    RigGraphTest.cpp
    BufferPoolTest.cpp
    FftTest.cpp
    BiquadTest.cpp
)
```

Run: `python cli/sd.py test`
Expected: FAIL — `SushiDSP/math/Biquad.hpp` does not exist.

- [ ] **Step 3: Write `include/SushiDSP/math/Biquad.hpp`**

```cpp
// Biquad.hpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

namespace SushiDSP
{
    /// @brief Direct Form I biquad filter with RBJ cookbook coefficient derivation.
    class Biquad
    {
    public:
        /// @brief Configures this filter as a low-pass with the given cutoff and Q.
        /// @param sample_rate Sample rate in Hz.
        /// @param cutoff_hz Cutoff frequency in Hz.
        /// @param q Quality factor (0.707 = Butterworth).
        void set_low_pass(double sample_rate, double cutoff_hz, double q);

        /// @brief Processes one sample through the filter.
        /// @param input The input sample.
        /// @return The filtered output sample.
        float process_sample(float input);

        /// @brief Clears the filter's internal state.
        void reset();

    private:
        float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;
        float a1_ = 0.0f, a2_ = 0.0f;
        float x1_ = 0.0f, x2_ = 0.0f;
        float y1_ = 0.0f, y2_ = 0.0f;
    };
}
```

- [ ] **Step 4: Write `src/math/Biquad.cpp`**

```cpp
// Biquad.cpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "SushiDSP/math/Biquad.hpp"

#include <cmath>

namespace SushiDSP
{
    void Biquad::set_low_pass(double sample_rate, double cutoff_hz, double q)
    {
        const double omega = 2.0 * M_PI * cutoff_hz / sample_rate;
        const double sin_omega = std::sin(omega);
        const double cos_omega = std::cos(omega);
        const double alpha = sin_omega / (2.0 * q);

        const double b0 = (1.0 - cos_omega) / 2.0;
        const double b1 = 1.0 - cos_omega;
        const double b2 = (1.0 - cos_omega) / 2.0;
        const double a0 = 1.0 + alpha;
        const double a1 = -2.0 * cos_omega;
        const double a2 = 1.0 - alpha;

        b0_ = static_cast<float>(b0 / a0);
        b1_ = static_cast<float>(b1 / a0);
        b2_ = static_cast<float>(b2 / a0);
        a1_ = static_cast<float>(a1 / a0);
        a2_ = static_cast<float>(a2 / a0);
    }

    float Biquad::process_sample(float input)
    {
        const float output = b0_ * input + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;

        x2_ = x1_;
        x1_ = input;
        y2_ = y1_;
        y1_ = output;

        return output;
    }

    void Biquad::reset()
    {
        x1_ = x2_ = y1_ = y2_ = 0.0f;
    }
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `python cli/sd.py test`
Expected: PASS — all three `Biquad*` tests.

- [ ] **Step 6: Commit**

```bash
git add include/SushiDSP/math/Biquad.hpp src/math/Biquad.cpp tests/unit_dsp/BiquadTest.cpp tests/unit_dsp/CMakeLists.txt
git commit -m "Add RBJ-cookbook Biquad filter"
```

---

## Task 7: RigGraph — topological compile + per-block replay

**Files:**
- Create: `include/SushiDSP/core/RigGraph.hpp`
- Create: `src/core/RigGraph.cpp`
- Modify: `tests/unit_dsp/RigGraphTest.cpp` (extend the Task 3 smoke test with real graph tests)

**Interfaces:**
- Consumes: `SushiDSP::IAudioNode`, `SushiDSP::PrepareInfo`, `SushiDSP::ProcessContext`, `SushiDSP::PortLayout` (Task 3); `SushiDSP::BufferPool` (Task 4).
- Produces: `SushiDSP::RigGraph` with `NodeHandle add_node(std::unique_ptr<IAudioNode> node)`, `void connect(NodeHandle from, NodeHandle to)`, `void compile(const PrepareInfo& info)`, `void process(ProcessContext& context)`. `NodeHandle` is `std::uint32_t`. `compile()` performs the topological sort and must be called before the first `process()` and after any `add_node`/`connect` call. Later tasks (D1+ device/host work, out of this plan's scope) call `process()` per audio callback.

- [ ] **Step 1: Write a minimal test double node and the failing test in `tests/unit_dsp/RigGraphTest.cpp`**

Replace the file's contents (keeping the existing `PortLayoutIsPlainOldData` test) with:

```cpp
// RigGraphTest.cpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"
#include "SushiDSP/core/RigGraph.hpp"

namespace
{
    class GainNode : public SushiDSP::IAudioNode
    {
    public:
        explicit GainNode(float gain) : gain_(gain) {}

        void prepare(const SushiDSP::PrepareInfo&) override {}

        void process(SushiDSP::ProcessContext& context) override
        {
            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
            {
                context.outputs[0][frame] = context.inputs[0][frame] * gain_;
            }
        }

        SushiDSP::PortLayout ports() const override { return { 1, 1 }; }

        void reset() override {}

    private:
        float gain_;
    };

    class ConstantSourceNode : public SushiDSP::IAudioNode
    {
    public:
        explicit ConstantSourceNode(float value) : value_(value) {}

        void prepare(const SushiDSP::PrepareInfo&) override {}

        void process(SushiDSP::ProcessContext& context) override
        {
            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
            {
                context.outputs[0][frame] = value_;
            }
        }

        SushiDSP::PortLayout ports() const override { return { 0, 1 }; }

        void reset() override {}

    private:
        float value_;
    };
}

TEST(UnitDsp, PortLayoutIsPlainOldData)
{
    SushiDSP::PortLayout layout{ 1, 2 };
    EXPECT_EQ(layout.input_channels, 1u);
    EXPECT_EQ(layout.output_channels, 2u);
}

TEST(UnitDsp, RigGraphProcessesTwoNodeChainInOrder)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source_handle = graph.add_node(std::make_unique<ConstantSourceNode>(2.0f));
    auto gain_handle    = graph.add_node(std::make_unique<GainNode>(3.0f));
    graph.connect(source_handle, gain_handle);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = { output_buffer.data() };

    SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 6.0f);
    }
}

TEST(UnitDsp, RigGraphRecompileAfterAddingNodeProducesUpdatedOutput)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source_handle = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    auto gain_handle_a  = graph.add_node(std::make_unique<GainNode>(2.0f));
    graph.connect(source_handle, gain_handle_a);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    auto gain_handle_b = graph.add_node(std::make_unique<GainNode>(5.0f));
    graph.connect(gain_handle_a, gain_handle_b);
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = { output_buffer.data() };
    SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 10.0f);
    }
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `python cli/sd.py test`
Expected: FAIL — `SushiDSP/core/RigGraph.hpp` does not exist.

- [ ] **Step 3: Write `include/SushiDSP/core/RigGraph.hpp`**

```cpp
// RigGraph.hpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "SushiDSP/core/BufferPool.hpp"
#include "SushiDSP/core/IAudioNode.hpp"

namespace SushiDSP
{
    using NodeHandle = std::uint32_t;

    /// @brief A user-editable directed graph of IAudioNode instances, compiled to a topological order and replayed per block.
    class RigGraph
    {
    public:
        /// @brief Constructs an empty rig graph.
        /// @param block_size Frames per process() call.
        /// @param max_live_buffers Maximum transient buffers live at once during a block.
        RigGraph(std::uint32_t block_size, std::uint32_t max_live_buffers);

        /// @brief Adds a node to the graph, unconnected. Not real-time safe; call only off the processing thread.
        /// @param node The node to take ownership of.
        /// @return A handle identifying the node for connect() calls.
        NodeHandle add_node(std::unique_ptr<IAudioNode> node);

        /// @brief Connects one node's output to another's input. Not real-time safe.
        /// @param from The upstream node.
        /// @param to The downstream node.
        void connect(NodeHandle from, NodeHandle to);

        /// @brief Topologically sorts the graph and calls prepare() on every node. Not real-time safe; call after every add_node/connect.
        /// @param info Sample rate, block size, and channel count for every node.
        void compile(const PrepareInfo& info);

        /// @brief Processes one block through every node in topological order. Real-time safe.
        /// @param context The block's output buffers and frame count.
        void process(ProcessContext& context);

    private:
        struct Edge { NodeHandle from; NodeHandle to; };

        std::uint32_t                            block_size_;
        BufferPool                                buffer_pool_;
        std::vector<std::unique_ptr<IAudioNode>>  nodes_;
        std::vector<Edge>                         edges_;
        std::vector<NodeHandle>                   topological_order_;
    };
}
```

- [ ] **Step 4: Write `src/core/RigGraph.cpp`**

```cpp
// RigGraph.cpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "SushiDSP/core/RigGraph.hpp"

#include <queue>

namespace SushiDSP
{
    RigGraph::RigGraph(std::uint32_t block_size, std::uint32_t max_live_buffers)
        : block_size_(block_size)
        , buffer_pool_(block_size, max_live_buffers)
    {
    }

    NodeHandle RigGraph::add_node(std::unique_ptr<IAudioNode> node)
    {
        nodes_.push_back(std::move(node));
        return static_cast<NodeHandle>(nodes_.size() - 1);
    }

    void RigGraph::connect(NodeHandle from, NodeHandle to)
    {
        edges_.push_back({ from, to });
    }

    void RigGraph::compile(const PrepareInfo& info)
    {
        std::vector<std::uint32_t> in_degree(nodes_.size(), 0);
        for (const Edge& edge : edges_)
        {
            ++in_degree[edge.to];
        }

        std::queue<NodeHandle> ready;
        for (NodeHandle handle = 0; handle < nodes_.size(); ++handle)
        {
            if (in_degree[handle] == 0)
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
                    if (--in_degree[edge.to] == 0)
                    {
                        ready.push(edge.to);
                    }
                }
            }
        }

        for (auto& node : nodes_)
        {
            node->prepare(info);
        }
    }

    void RigGraph::process(ProcessContext& context)
    {
        std::vector<float*> node_output(nodes_.size(), nullptr);

        for (NodeHandle handle : topological_order_)
        {
            float* node_buffer = buffer_pool_.acquire(context.frames);

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
            nodes_[handle]->process(node_context);

            node_output[handle] = node_buffer;
        }

        if (!topological_order_.empty())
        {
            const NodeHandle last = topological_order_.back();
            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
            {
                context.outputs[0][frame] = node_output[last][frame];
            }
        }

        for (float* buffer : node_output)
        {
            if (buffer != nullptr)
            {
                buffer_pool_.release(buffer);
            }
        }
    }
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `python cli/sd.py test`
Expected: PASS — `RigGraphProcessesTwoNodeChainInOrder` and `RigGraphRecompileAfterAddingNodeProducesUpdatedOutput`, plus all prior tests still passing.

- [ ] **Step 6: Commit**

```bash
git add include/SushiDSP/core/RigGraph.hpp src/core/RigGraph.cpp tests/unit_dsp/RigGraphTest.cpp
git commit -m "Add RigGraph topological compile and per-block replay"
```

---

## Task 8: RT-safety soak test (no-alloc trap)

**Files:**
- Create: `tests/unit_dsp/RtSafetyTest.cpp`
- Modify: `tests/unit_dsp/CMakeLists.txt` (add `RtSafetyTest.cpp`)

**Interfaces:**
- Consumes: `SushiDSP::RigGraph`, `SushiDSP::IAudioNode` (Tasks 3, 7). No new production interfaces — this task adds a test-only global `operator new`/`operator delete` override scoped to the test binary.

- [ ] **Step 1: Write `tests/unit_dsp/RtSafetyTest.cpp`**

```cpp
// RtSafetyTest.cpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <atomic>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/RigGraph.hpp"

namespace
{
    std::atomic<bool> g_trap_allocations{ false };
    std::atomic<int>  g_trapped_allocation_count{ 0 };

    class SumNode : public SushiDSP::IAudioNode
    {
    public:
        void prepare(const SushiDSP::PrepareInfo&) override {}

        void process(SushiDSP::ProcessContext& context) override
        {
            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
            {
                context.outputs[0][frame] = 1.0f;
            }
        }

        SushiDSP::PortLayout ports() const override { return { 0, 1 }; }

        void reset() override {}
    };
}

void* operator new(std::size_t size)
{
    if (g_trap_allocations.load(std::memory_order_relaxed))
    {
        g_trapped_allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    return std::malloc(size);
}

void operator delete(void* ptr) noexcept
{
    std::free(ptr);
}

TEST(UnitDsp, RigGraphProcessPerformsNoHeapAllocationAcrossManyBlocks)
{
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(std::make_unique<SumNode>());

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = { output_buffer.data() };

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int block = 0; block < 7000; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };
        graph.process(context);
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
}
```

- [ ] **Step 2: Add `RtSafetyTest.cpp` to CMake**

Edit `tests/unit_dsp/CMakeLists.txt`:

```cmake
add_executable(unit_dsp
    RigGraphTest.cpp
    BufferPoolTest.cpp
    FftTest.cpp
    BiquadTest.cpp
    RtSafetyTest.cpp
)
```

- [ ] **Step 3: Run and confirm the test currently passes cleanly (the `RigGraph::process` implementation from Task 7 uses only `BufferPool`, which preallocates, and a `std::vector<float*>` sized once per call — this vector allocation is the one expected gap)**

Run: `python cli/sd.py test`
Expected outcome to check: if `RigGraphProcessPerformsNoHeapAllocationAcrossManyBlocks` FAILS because `process()`'s local `std::vector<float*> node_output` allocates every call, proceed to Step 4 to fix it. If it unexpectedly passes, skip to Step 6.

- [ ] **Step 4: Fix the allocation — move `node_output` storage into `RigGraph` as a preallocated member**

In `include/SushiDSP/core/RigGraph.hpp`, add a member to the private section:

```cpp
        std::vector<float*>                       node_output_scratch_;
```

In `src/core/RigGraph.cpp`, in `RigGraph::compile`, after building `topological_order_`, add:

```cpp
        node_output_scratch_.assign(nodes_.size(), nullptr);
```

In `RigGraph::process`, replace the line `std::vector<float*> node_output(nodes_.size(), nullptr);` with:

```cpp
        auto& node_output = node_output_scratch_;
        std::fill(node_output.begin(), node_output.end(), nullptr);
```

Add `#include <algorithm>` to the top of `src/core/RigGraph.cpp` if not already present.

- [ ] **Step 5: Re-run to verify the fix**

Run: `python cli/sd.py test`
Expected: PASS — `RigGraphProcessPerformsNoHeapAllocationAcrossManyBlocks` reports zero trapped allocations, and every earlier test (Tasks 3-7) still passes.

- [ ] **Step 6: Commit**

```bash
git add tests/unit_dsp/RtSafetyTest.cpp tests/unit_dsp/CMakeLists.txt include/SushiDSP/core/RigGraph.hpp src/core/RigGraph.cpp
git commit -m "Add RT-safety no-alloc trap test and eliminate per-block RigGraph allocation"
```

---

## Self-Review Notes

- **Spec coverage:** §0.1 (portable dependency-free core) → Tasks 1, 3-7. §0.2 (RT-safe processing) → Task 8. §3 core interfaces (`IAudioNode`, `NodeDescriptor`, `PortLayout`) → Task 3. §3 graph mechanics (transient buffer pool, recompile) → Tasks 4, 7. §4.1 FFT → Task 5. §4.3 filters → Task 6 (biquad only; SVF/Moog ladder are D2+ scope per spec §9, not needed until the WDF/DK solver work). §8/§9 D0 roadmap line ("Repo, `sd` CLI, `sushi_dsp` target, `IAudioNode`/graph/`NodeDescriptor`, transient pool, FFT + filters with golden-vector tests. Ships: headless graph processes a verified tone/filter sweep.") → covered end-to-end by Tasks 1-8; a combined FFT+filter "tone sweep" is exercised across `FftTest.cpp` and `BiquadTest.cpp` rather than one combined test, since each kernel is independently golden-vector-testable and the spec's `Unit_Dsp` test bar (§8) is per-kernel golden-vector tests, not one monolithic scenario.
- **Placeholder scan:** none found — every step has concrete code or a concrete shell command.
- **Type consistency:** `NodeHandle` used consistently as `std::uint32_t` across Task 7's header/impl/tests. `ProcessContext{ outputs, inputs, frames, time_seconds }` field order matches Task 3's definition everywhere it's constructed (Tasks 7, 8). `BufferPool::acquire`/`release` signatures match between Task 4's declaration and Task 7's usage.

---

**Plan complete and saved to `docs/superpowers/plans/2026-08-25-d0-skeleton-math-kernels.md`.** Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
