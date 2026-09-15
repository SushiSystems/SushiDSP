# Host GUI Live Audio Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `apps/host_gui` a fully working MVP: an in-window device-selection screen lets
the user pick a real ASIO driver (e.g. Rubix22) with L/R input/output channel selection, or
fall back to silent SDL2, before the existing rack-editor UI runs against real captured audio.

**Architecture:** Extract the input/output channel-selection logic already working in
`apps/host_asio` into a small shared header/source pair in `include/SushiDSP/device/`, refactor
`apps/host_asio` to use it (behavior-preserving), then add an `AppState` state machine to
`apps/host_gui/main.cpp`: a `SelectingDevice` screen (device combo box, channel selectors,
Connect button) that lazily constructs the chosen `IAudioDevice` and transitions to the existing
`Running` screen (rack strip / gear panel / parameters / presets, unchanged).

**Tech Stack:** C++17, existing GLFW/Dear ImGui/GoogleTest already in the tree.

**Spec:** `docs/superpowers/specs/2026-08-25-imgui-rig-gui-design.md` (this plan closes the gap
that spec's "Device selection" section described but no task of the original 7-task plan
implemented — see that plan's final-review ledger,
`.superpowers/sdd/2026-08-25-imgui-rig-gui/progress.md`, for the disclosed scope gap this
closes).

## Global Constraints

- Everything reachable from `IAudioNode::process()` — including the device callback lambda in
  both `apps/host_asio/main.cpp` and `apps/host_gui/main.cpp` — stays allocation-free,
  lock-free, syscall-free, throw-free. `fan_out_mono_to_stereo` (Task 8) must meet this bar.
- Builds/tests go through `sd build` / `sd test`, never raw cmake/ctest.
- `apps/host_asio`'s existing CLI behavior (`<driver> [--input=L|R] [--output=L|R|stereo]
  [cabinet-ir.wav]`) must be unchanged after Task 8's refactor — this is a regression-safety
  requirement, verified by manual re-run, not just unit tests (main.cpp isn't unit tested).
- New shared code lives in `include/SushiDSP/device/ChannelSelection.hpp` /
  `src/device/ChannelSelection.cpp`, added to the existing unconditional `sushi_dsp` static
  library's source list in the root `CMakeLists.txt` (same list `AsioSampleConverter.cpp`
  already lives in, despite being device-specific — this code has no ASIO SDK dependency
  either, so it belongs there, not behind `SUSHIDSP_BUILD_ASIO`).
- Test files register in `tests/CMakeLists.txt`'s `sushidsp_tests` executable under the
  matching `# --- Section ---` comment, following the existing `Unit_*` GTest naming convention.

---

### Task 8: Extract shared `ChannelSelection` and refactor `apps/host_asio` onto it

**Files:**
- Create: `include/SushiDSP/device/ChannelSelection.hpp`
- Create: `src/device/ChannelSelection.cpp`
- Modify: `CMakeLists.txt` (add `src/device/ChannelSelection.cpp` to the `sushi_dsp` target)
- Modify: `apps/host_asio/main.cpp` (use the shared header instead of its local copy)
- Test: `tests/functional/unit/test_channel_selection.cpp`
- Modify: `tests/CMakeLists.txt` (register the new test file)

**Interfaces:**
- Produces: `SushiDSP::ChannelSelection` (enum: `Left`, `Right`, `Stereo`),
  `SushiDSP::parse_channel_selection(const std::string&, const char*)`,
  `SushiDSP::channel_selection_label(ChannelSelection)`,
  `SushiDSP::channel_selection_input_index(ChannelSelection)`,
  `SushiDSP::fan_out_mono_to_stereo(const float*, float* const*, std::uint32_t, ChannelSelection)`
  — consumed by Task 9's `apps/host_gui` and by this task's refactored `apps/host_asio`.

- [ ] **Step 1: Write the failing tests**

Create `tests/functional/unit/test_channel_selection.cpp`:

```cpp
/**************************************************************************/
/* test_channel_selection.cpp                                             */
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

#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/device/ChannelSelection.hpp"

TEST(Unit_Dsp, ParseChannelSelectionAcceptsLRAndStereo)
{
    EXPECT_EQ(SushiDSP::parse_channel_selection("L", "--input"), SushiDSP::ChannelSelection::Left);
    EXPECT_EQ(SushiDSP::parse_channel_selection("l", "--input"), SushiDSP::ChannelSelection::Left);
    EXPECT_EQ(SushiDSP::parse_channel_selection("R", "--input"), SushiDSP::ChannelSelection::Right);
    EXPECT_EQ(SushiDSP::parse_channel_selection("r", "--input"), SushiDSP::ChannelSelection::Right);
    EXPECT_EQ(SushiDSP::parse_channel_selection("stereo", "--output"), SushiDSP::ChannelSelection::Stereo);
}

TEST(Unit_Dsp, ParseChannelSelectionRejectsGarbage)
{
    EXPECT_THROW(SushiDSP::parse_channel_selection("banana", "--input"), std::runtime_error);
}

TEST(Unit_Dsp, ChannelSelectionInputIndexMatchesLeftRight)
{
    EXPECT_EQ(SushiDSP::channel_selection_input_index(SushiDSP::ChannelSelection::Left), 0u);
    EXPECT_EQ(SushiDSP::channel_selection_input_index(SushiDSP::ChannelSelection::Right), 1u);
}

TEST(Unit_Dsp, ChannelSelectionLabelsAreHumanReadable)
{
    EXPECT_STREQ(SushiDSP::channel_selection_label(SushiDSP::ChannelSelection::Left), "L");
    EXPECT_STREQ(SushiDSP::channel_selection_label(SushiDSP::ChannelSelection::Right), "R");
    EXPECT_STREQ(SushiDSP::channel_selection_label(SushiDSP::ChannelSelection::Stereo), "stereo");
}

TEST(Unit_Dsp, FanOutMonoToStereoWritesBothChannelsForStereo)
{
    std::vector<float> mono{ 1.0f, 2.0f, 3.0f };
    std::vector<float> left(3, -1.0f), right(3, -1.0f);
    float* outputs[2] = { left.data(), right.data() };

    SushiDSP::fan_out_mono_to_stereo(mono.data(), outputs, 3, SushiDSP::ChannelSelection::Stereo);

    EXPECT_EQ(left, mono);
    EXPECT_EQ(right, mono);
}

TEST(Unit_Dsp, FanOutMonoToStereoWritesOnlyLeftForLeft)
{
    std::vector<float> mono{ 1.0f, 2.0f, 3.0f };
    std::vector<float> left(3, -1.0f), right(3, -1.0f);
    float* outputs[2] = { left.data(), right.data() };

    SushiDSP::fan_out_mono_to_stereo(mono.data(), outputs, 3, SushiDSP::ChannelSelection::Left);

    EXPECT_EQ(left, mono);
    for (float sample : right) { EXPECT_FLOAT_EQ(sample, 0.0f); }
}

TEST(Unit_Dsp, FanOutMonoToStereoWritesOnlyRightForRight)
{
    std::vector<float> mono{ 1.0f, 2.0f, 3.0f };
    std::vector<float> left(3, -1.0f), right(3, -1.0f);
    float* outputs[2] = { left.data(), right.data() };

    SushiDSP::fan_out_mono_to_stereo(mono.data(), outputs, 3, SushiDSP::ChannelSelection::Right);

    for (float sample : left) { EXPECT_FLOAT_EQ(sample, 0.0f); }
    EXPECT_EQ(right, mono);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `sd test -s unit` (filter by reading output for `Unit_Dsp.ParseChannelSelection*` /
`Unit_Dsp.ChannelSelection*` / `Unit_Dsp.FanOutMonoToStereo*` — this project's `sd test` has no
`--filter` flag; use `-s unit` and read the relevant lines, or grep the output)
Expected: build failure — `ChannelSelection.hpp` doesn't exist yet, and the test file isn't
registered in `tests/CMakeLists.txt` yet either (do Step 3a below before this will even
attempt to compile).

- [ ] **Step 3a: Register the test file**

In `tests/CMakeLists.txt`, add `functional/unit/test_channel_selection.cpp` to the
`# --- Core ---` group, immediately after `functional/unit/test_rt_graph_slot.cpp`.

- [ ] **Step 3b: Create ChannelSelection.hpp**

```cpp
/**************************************************************************/
/* ChannelSelection.hpp                                                   */
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

#include <cstdint>
#include <stdexcept>
#include <string>

namespace SushiDSP
{
    /// @brief Which physical device channel(s) a host app reads from / writes to. A rig node
    /// itself is always mono; this only picks which of a (stereo-capable) device's channels
    /// feeds it or receives its output.
    enum class ChannelSelection { Left, Right, Stereo };

    /// @brief Parses "L"/"l"/"R"/"r"/"stereo" into a ChannelSelection.
    /// @param value The flag's value, e.g. from "--input=L".
    /// @param flag_name The flag's name, used only in the exception message (e.g. "--input").
    /// @return The parsed selection.
    /// @throws std::runtime_error if value matches none of the accepted spellings.
    ChannelSelection parse_channel_selection(const std::string& value, const char* flag_name);

    /// @brief A short human-readable label for a selection: "L", "R", or "stereo".
    /// @param selection The selection to label.
    /// @return A statically-allocated label string.
    const char* channel_selection_label(ChannelSelection selection);

    /// @brief Maps a single-channel selection to a device input-array index: Left -> 0, Right
    /// -> 1. Not meaningful for Stereo (callers only pass Left/Right here, since capturing one
    /// mono signal always reads exactly one physical channel).
    /// @param selection Left or Right.
    /// @return 0 for Left, 1 for Right.
    std::size_t channel_selection_input_index(ChannelSelection selection);

    /// @brief Fans a processed mono signal out to up to 2 physical output channels. Real-time
    /// safe: no allocation, no lock, no syscall, no throw.
    /// @param mono The mono signal to fan out, `frames` samples.
    /// @param outputs Pointer to at least 2 output channel buffers (index 0 = left/channel A,
    /// index 1 = right/channel B). Both are always written (zeroed where not selected), so the
    /// caller never needs a separate memset.
    /// @param frames Sample count per channel.
    /// @param output_selection Left writes only outputs[0]; Right writes only outputs[1];
    /// Stereo writes both.
    void fan_out_mono_to_stereo(const float* mono, float* const* outputs, std::uint32_t frames, ChannelSelection output_selection);
}
```

- [ ] **Step 3c: Create ChannelSelection.cpp**

```cpp
/**************************************************************************/
/* ChannelSelection.cpp                                                   */
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

#include "SushiDSP/device/ChannelSelection.hpp"

#include <cstring>

namespace SushiDSP
{
    ChannelSelection parse_channel_selection(const std::string& value, const char* flag_name)
    {
        if (value == "L" || value == "l") return ChannelSelection::Left;
        if (value == "R" || value == "r") return ChannelSelection::Right;
        if (value == "stereo") return ChannelSelection::Stereo;
        throw std::runtime_error(std::string(flag_name) + " must be one of L, R, stereo (got '" + value + "')");
    }

    const char* channel_selection_label(ChannelSelection selection)
    {
        switch (selection)
        {
            case ChannelSelection::Left:   return "L";
            case ChannelSelection::Right:  return "R";
            case ChannelSelection::Stereo: return "stereo";
        }
        return "?";
    }

    std::size_t channel_selection_input_index(ChannelSelection selection)
    {
        return selection == ChannelSelection::Right ? 1u : 0u;
    }

    void fan_out_mono_to_stereo(const float* mono, float* const* outputs, std::uint32_t frames, ChannelSelection output_selection)
    {
        const bool write_left = output_selection != ChannelSelection::Right;
        const bool write_right = output_selection != ChannelSelection::Left;

        if (write_left)
        {
            std::memcpy(outputs[0], mono, frames * sizeof(float));
        }
        else
        {
            std::memset(outputs[0], 0, frames * sizeof(float));
        }
        if (write_right)
        {
            std::memcpy(outputs[1], mono, frames * sizeof(float));
        }
        else
        {
            std::memset(outputs[1], 0, frames * sizeof(float));
        }
    }
}
```

- [ ] **Step 4: Wire into the root CMakeLists.txt**

Add `src/device/ChannelSelection.cpp` to the `sushi_dsp` target's source list in
`CMakeLists.txt`, alongside `src/device/AsioSampleConverter.cpp`.

- [ ] **Step 5: Run tests to verify they pass**

Run: `sd test -s unit`, confirm the new `Unit_Dsp.ParseChannelSelection*` /
`Unit_Dsp.ChannelSelection*` / `Unit_Dsp.FanOutMonoToStereo*` tests all pass.

- [ ] **Step 6: Refactor apps/host_asio/main.cpp onto the shared header**

Read the current `apps/host_asio/main.cpp` in full first — it has its own local
`ChannelSelection` enum, `parse_channel_flag`, `match_flag`, and inline fan-out logic in the
device callback (`write_left`/`write_right` with `memcpy`/`memset`). Replace all of that with
the shared header:

- Remove the local `enum class ChannelSelection { Left, Right, Stereo };` and
  `parse_channel_flag(...)` function from the anonymous namespace; keep `match_flag(...)` (it's
  a generic `--name=value` parser, not channel-selection-specific, and stays local to this
  file).
- Add `#include "SushiDSP/device/ChannelSelection.hpp"`.
- Replace calls to the removed local `parse_channel_flag(value, "--input")` /
  `parse_channel_flag(value, "--output")` with `SushiDSP::parse_channel_selection(value,
  "--input")` / `SushiDSP::parse_channel_selection(value, "--output")`.
- Replace `ChannelSelection` type references with `SushiDSP::ChannelSelection` throughout.
- Replace the printf using `input_selection == ChannelSelection::Left ? "L" : "R"` /
  the output ternary chain with `SushiDSP::channel_selection_label(input_selection)` /
  `SushiDSP::channel_selection_label(output_selection)`.
- Replace `const std::size_t input_index = input_selection == ChannelSelection::Left ? 0u : 1u;`
  with `const std::size_t input_index = SushiDSP::channel_selection_input_index(input_selection);`.
- Replace the device callback's inline `write_left`/`write_right`/`memcpy`/`memset` block (the
  part that fans `mono_output` out to `outputs[0]`/`outputs[1]`) with a single call:
  `SushiDSP::fan_out_mono_to_stereo(mono_output.data(), outputs, frames, output_selection);`
  This also means the callback's own `#include <cstring>` may become unused — check and remove
  it if nothing else in the file needs `memcpy`/`memset`/`strlen`/etc. (the file also uses
  `std::strlen` in `match_flag`, so `<cstring>` likely stays needed — verify before removing).

**This step's behavior must be IDENTICAL to before** — same CLI usage, same defaults
(`input=L`, `output=stereo`), same error messages. This is a pure refactor.

- [ ] **Step 7: Manual regression check**

Run: `sd build`, then manually run `sushidsp_host_asio.exe` (no args, to see the usage/driver
list) and confirm the output matches what it printed before this refactor (driver list, usage
line with `--input=L|R`/`--output=L|R|stereo`). If an ASIO-capable machine/driver is available,
also do a real run with `--input=L --output=stereo` (or whatever the previous manual testing
used) and confirm no behavior change. Describe exactly what you ran and saw in the report.

- [ ] **Step 8: Commit**

```bash
git add include/SushiDSP/device/ChannelSelection.hpp src/device/ChannelSelection.cpp CMakeLists.txt apps/host_asio/main.cpp tests/functional/unit/test_channel_selection.cpp tests/CMakeLists.txt
git commit -m "device: extract shared ChannelSelection, refactor host_asio onto it"
```

---

### Task 9: `apps/host_gui` device-selection screen and real capture wiring

**Files:**
- Modify: `apps/host_gui/main.cpp`

**Interfaces:**
- Consumes: Task 8's `SushiDSP::ChannelSelection` / `parse_channel_selection` /
  `channel_selection_label` / `channel_selection_input_index` / `fan_out_mono_to_stereo`;
  `SushiDSP::AsioAudioDevice::enumerate_drivers()` / `AsioAudioDevice` constructor (both
  pre-existing, unchanged); `SushiDSP::Sdl2AudioDevice` (pre-existing, unchanged).
- No new files, no new library targets — this is app-local UI + wiring.

This task has no unit-testable business logic of its own (device selection and the audio
callback live in `main.cpp`, which this project's test suite has never unit-tested — same
scope boundary as Tasks 5-7). Acceptance is a successful `sd build` plus a manual smoke test:
launch the app, confirm the SelectingDevice screen appears, select SDL2 and confirm it
transitions to the working rack editor exactly as before, and (if an ASIO-capable machine is
available) select an ASIO driver with a channel selection and confirm the transition succeeds
and the format line reports real ASIO channel counts.

- [ ] **Step 1: Read the current file in full**

Read `apps/host_gui/main.cpp` in full before starting — you need its exact current structure
(the `#include` block, the `std::optional<RigController> rig_controller_storage` /
`std::unique_ptr<SushiDSP::IAudioDevice> device` declaration-order pattern from the prior fix
wave, the `silence` substitution, the device callback, the `while (!glfwWindowShouldClose...)`
render loop with its Save/Load/Rack/Gear/Parameters sections) to know exactly what to preserve
versus restructure.

- [ ] **Step 2: Add the state machine and selection-screen state**

Add near the top of `main()`, after the GLFW/ImGui setup and before the current device/
rig_controller declarations:

```cpp
    enum class AppState { SelectingDevice, Running };
    AppState app_state = AppState::SelectingDevice;

    const std::vector<std::string> asio_drivers =
#if defined(SUSHIDSP_HAVE_ASIO)
        SushiDSP::AsioAudioDevice::enumerate_drivers();
#else
        std::vector<std::string>{};
#endif

    int selected_device_index = 0;  // 0 = "SDL2 (no input)", 1..N = asio_drivers[index-1]
    SushiDSP::ChannelSelection selected_input = SushiDSP::ChannelSelection::Left;
    SushiDSP::ChannelSelection selected_output = SushiDSP::ChannelSelection::Stereo;
    std::string connect_error;

    // Both buffers are captured BY REFERENCE in the device callback (Step 5), and that
    // callback keeps firing on the audio thread for as long as the device runs - i.e. for
    // every frame after the one where Connect was clicked. They MUST therefore live in this
    // outer main()-body scope, not inside the `if (ImGui::Button("Connect"))` block: a
    // same-scope-as-the-button declaration would be destroyed the instant that block exits,
    // leaving the callback's captured references dangling on the very next audio callback.
    // Both start empty and are `.assign()`-ed once, inside the Connect handler, once
    // `actual.block_size` is known - never resized again afterward, so the callback that
    // captures them by reference stays allocation-free.
    std::vector<float> mono_output;
    std::vector<float> callback_silence;
```

- [ ] **Step 3: Move device/rig_controller construction into a deferred, lazily-invoked block**

The existing code unconditionally constructs `device`, `rig_controller_storage`, and `silence`
right after their declarations, then calls `device->start(...)`. Restructure so:

- Add the declarations `std::optional<RigController> rig_controller_storage;` and
  `std::unique_ptr<SushiDSP::IAudioDevice> device;` AFTER Step 2's `mono_output`/
  `callback_silence` declarations, in this relative order: `rig_controller_storage`, then
  `device` (matching the existing fix-wave-verified destruction-order pattern — `device` must
  be destroyed before `rig_controller_storage`). Since `mono_output`/`callback_silence` are now
  declared even earlier (Step 2), before both of these, they are destroyed LAST of all four —
  i.e. after `device` is already destroyed, which is exactly correct: nothing may reference
  them once they're gone, and `device` (whose callback references them) is already torn down
  by that point. This also fully closes the prior plan's final-review parked finding about
  `silence`'s destruction order — mention this explicitly in your report.
- Do NOT construct `rig_controller_storage` or `device` here, and do not `.assign()`
  `mono_output`/`callback_silence` here either. All of that only happens inside the Connect
  button's handler (Step 5), after a device has been chosen.
- Do NOT call `device->start(...)` here either — that also moves into the Connect handler.

- [ ] **Step 4: Wrap the existing per-frame UI in the AppState branch**

The current frame loop does, in order: `glfwPollEvents()`, `reclaim()`, ImGui new-frame calls,
`ImGui::Begin("SushiDSP Rig")`, the Save/Load/Rack/Gear/Parameters UI, `ImGui::End()`, then the
render/swap calls. Restructure so:

- `glfwPollEvents()` and the ImGui new-frame calls (`ImGui_ImplOpenGL3_NewFrame()` /
  `ImGui_ImplGlfw_NewFrame()` / `ImGui::NewFrame()`) always run, every frame, regardless of
  `app_state` (needed for the selection screen to render at all).
- `rig_controller.graph_slot().reclaim()` only runs when `app_state == AppState::Running` (it
  would be invalid to call before `rig_controller_storage` has a value).
- Everything from `ImGui::Begin("SushiDSP Rig")` through the Save/Load/Rack/Gear/Parameters
  block and `ImGui::End()` only runs when `app_state == AppState::Running`.
- When `app_state == AppState::SelectingDevice`, render the selection screen instead (Step 5),
  inside its own `ImGui::Begin("Connect a Device")` / `ImGui::End()`.
- The render/swap calls (`ImGui::Render()` through `glfwSwapBuffers(window)`) always run,
  regardless of state, same as today.

- [ ] **Step 5: Write the SelectingDevice screen**

```cpp
        if (app_state == AppState::SelectingDevice)
        {
            ImGui::Begin("Connect a Device");

            ImGui::Text("Device");
            if (ImGui::RadioButton("SDL2 (no input)", selected_device_index == 0))
            {
                selected_device_index = 0;
            }
            for (std::size_t i = 0; i < asio_drivers.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::RadioButton(asio_drivers[i].c_str(), selected_device_index == static_cast<int>(i) + 1))
                {
                    selected_device_index = static_cast<int>(i) + 1;
                }
                ImGui::PopID();
            }

            if (selected_device_index > 0)
            {
                ImGui::Separator();
                ImGui::Text("Input");
                if (ImGui::RadioButton("L##input", selected_input == SushiDSP::ChannelSelection::Left)) selected_input = SushiDSP::ChannelSelection::Left;
                ImGui::SameLine();
                if (ImGui::RadioButton("R##input", selected_input == SushiDSP::ChannelSelection::Right)) selected_input = SushiDSP::ChannelSelection::Right;

                ImGui::Text("Output");
                if (ImGui::RadioButton("L##output", selected_output == SushiDSP::ChannelSelection::Left)) selected_output = SushiDSP::ChannelSelection::Left;
                ImGui::SameLine();
                if (ImGui::RadioButton("R##output", selected_output == SushiDSP::ChannelSelection::Right)) selected_output = SushiDSP::ChannelSelection::Right;
                ImGui::SameLine();
                if (ImGui::RadioButton("stereo##output", selected_output == SushiDSP::ChannelSelection::Stereo)) selected_output = SushiDSP::ChannelSelection::Stereo;
            }

            ImGui::Separator();
            if (ImGui::Button("Connect"))
            {
                try
                {
                    if (selected_device_index == 0)
                    {
                        SushiDSP::DeviceFormat requested{ 48000.0, 256, 1, 1 };
                        device = std::make_unique<SushiDSP::Sdl2AudioDevice>(requested);
                    }
#if defined(SUSHIDSP_HAVE_ASIO)
                    else
                    {
                        SushiDSP::DeviceFormat requested{ 48000.0, 256, 2, 2 };
                        device = std::make_unique<SushiDSP::AsioAudioDevice>(
                            asio_drivers[static_cast<std::size_t>(selected_device_index - 1)], requested);
                    }
#endif

                    const SushiDSP::DeviceFormat actual = device->format();
                    SushiDSP::PrepareInfo info{ actual.sample_rate, actual.block_size, actual.output_channels };
                    rig_controller_storage.emplace(info);

                    const std::size_t input_index = SushiDSP::channel_selection_input_index(selected_input);
                    const SushiDSP::ChannelSelection output_selection = selected_output;
                    const bool is_multi_channel_input = actual.input_channels >= 2;
                    const bool is_multi_channel_output = actual.output_channels >= 2;

                    // Sized once, here, off the audio thread - the outer-scope vectors declared
                    // in Step 2 (mono_output, callback_silence) are never resized again after
                    // this, so the callback that captures them by reference stays
                    // allocation-free (mirrors apps/host_asio/main.cpp's own pattern).
                    mono_output.assign(actual.block_size, 0.0f);
                    callback_silence.assign(actual.block_size, 0.0f);

                    RigController& rig_controller_ref = *rig_controller_storage;
                    device->start([&rig_controller_ref, &mono_output, &callback_silence, input_index,
                                    output_selection, is_multi_channel_input, is_multi_channel_output]
                        (const float* const* inputs, float* const* outputs, std::uint32_t frames)
                    {
                        if (frames > mono_output.size())
                        {
                            return;
                        }
                        SushiDSP::RigGraph* active = rig_controller_ref.graph_slot().acquire_for_audio_thread();

                        const float* selected_channel[1];
                        if (inputs == nullptr)
                        {
                            const float* silence_channel = callback_silence.data();
                            selected_channel[0] = silence_channel;
                        }
                        else if (is_multi_channel_input)
                        {
                            selected_channel[0] = inputs[input_index];
                        }
                        else
                        {
                            selected_channel[0] = inputs[0];
                        }

                        float* mono_output_ptr[1] = { mono_output.data() };
                        SushiDSP::ProcessContext context{ mono_output_ptr, selected_channel, frames, 0.0 };
                        if (active != nullptr)
                        {
                            active->process(context);
                        }
                        else
                        {
                            std::fill(mono_output.begin(), mono_output.begin() + frames, 0.0f);
                        }

                        if (is_multi_channel_output)
                        {
                            SushiDSP::fan_out_mono_to_stereo(mono_output.data(), outputs, frames, output_selection);
                        }
                        else
                        {
                            std::memcpy(outputs[0], mono_output.data(), frames * sizeof(float));
                        }
                    });

                    app_state = AppState::Running;
                }
                catch (const std::exception& error)
                {
                    connect_error = error.what();
                }
            }
            if (!connect_error.empty())
            {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", connect_error.c_str());
            }

            ImGui::End();
        }
```

**Design notes (binding — read before implementing):**

- `mono_output` and `callback_silence` are declared in Step 2's outer `main()` scope specifically
  so they outlive the `if (ImGui::Button("Connect"))` block — the device callback captures both
  by reference and keeps running on the audio thread for the device's entire lifetime, long
  after that block's single frame has finished. Declaring either of them fresh inside the
  Connect handler (as an earlier draft of this plan mistakenly did) would leave the callback
  holding a dangling reference from the very next audio callback onward. Do not "simplify" this
  back to a local declaration.
- `is_multi_channel_input`/`is_multi_channel_output` distinguish "ASIO with 2 real channels, use
  `input_index`/`fan_out_mono_to_stereo` to pick/route L or R" from "SDL2 with only 1 channel
  (or `inputs == nullptr`)" — reuse `apps/host_asio/main.cpp`'s exact established pattern here
  rather than inventing a new one; if that file's approach differs from this sketch in any
  detail once you re-read it in Step 1, follow the established file's pattern, not this plan
  text, and note the deviation in your report.
- The output-channel check uses `actual.output_channels >= 2` (captured into
  `is_multi_channel_output` before `device->start(...)`), NOT a runtime `outputs[1] != nullptr`
  probe — the latter would be an out-of-bounds read of the `outputs` array itself when the
  device only reports 1 output channel (the SDL2 branch, `requested.output_channels == 1`).

- [ ] **Step 6: Add a "Connected: ..." status line to the Running screen**

Inside the existing `ImGui::Begin("SushiDSP Rig")` block (now gated on `app_state ==
AppState::Running`), add near the top, before the Save/Load row:

```cpp
        ImGui::Text("Connected: %s (input %s, output %s)",
            selected_device_index == 0 ? "SDL2" : asio_drivers[static_cast<std::size_t>(selected_device_index - 1)].c_str(),
            SushiDSP::channel_selection_label(selected_input),
            SushiDSP::channel_selection_label(selected_output));
        ImGui::Separator();
```

- [ ] **Step 7: Handle window close from either state**

Confirm the existing teardown (`device->stop();` then the ImGui/GLFW shutdown calls, after the
`while` loop) still runs correctly whether the window closes from `SelectingDevice` (where
`device` may be null — guard the `device->stop()` call: only call it `if (device) { device->stop();
}`) or from `Running` (where `device` is always non-null once that state is reached).

- [ ] **Step 8: Add the missing includes**

Add `#include "SushiDSP/device/ChannelSelection.hpp"` and `#include <algorithm>` (for
`std::fill`, if used) to `apps/host_gui/main.cpp`'s includes.

- [ ] **Step 9: Build and smoke-test manually**

Run: `sd build`. Expected: `sushidsp_host_gui.exe` builds.

Run manually:
1. Launch the app. Confirm a "Connect a Device" window appears with "SDL2 (no input)" selected
   by default, plus any ASIO drivers found (may be zero on this machine).
2. Click Connect with SDL2 selected. Confirm it transitions to the rack editor exactly as
   before this task (rack strip, gear panel, parameters, save/load all still work — this is a
   regression check against Task 6/7's already-verified behavior). Confirm the new "Connected:
   SDL2 ..." line appears.
3. If an ASIO driver is listed, restart the app, select it with an input/output channel
   choice, click Connect, and confirm no crash and the status line reports the real driver
   name and selected channels. If no ASIO driver is available on this machine, say so plainly
   in the report rather than fabricating a result — this part of the manual test may be
   deferred to the user (who has a Rubix22) to verify themselves.
4. Close the window from the SelectingDevice screen (without ever clicking Connect) and confirm
   clean exit, no crash (this exercises the `device` still being null at teardown).

- [ ] **Step 10: Commit**

```bash
git add apps/host_gui/main.cpp
git commit -m "host_gui: add device-selection screen, wire real ASIO/SDL2 capture"
```
