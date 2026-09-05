# AmpliTube-Style GUI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace `apps/host_gui`'s slider list with an AmpliTube-style layout: a stage drawing the selected node's own hardware panel with turnable knobs, a two-row chain strip showing parallel branches and a bypass switch per slot, a gear browser, a preset bar, and input/output level meters.

**Architecture:** Everything is drawn procedurally with `ImDrawList`; no image file, no new dependency. `main.cpp` shrinks to window setup, device wiring, the frame loop, and five region calls. The units that compute — knob math, panel tables, chain layout, the level probe — live in files free of ImGui types so `sushidsp_tests` compiles them without linking `imgui`.

**Tech Stack:** C++17, Dear ImGui v1.90.9, GLFW 3.4, OpenGL 3, GoogleTest v1.14.0, CMake, the `sd` CLI.

**Spec:** `docs/superpowers/specs/2026-09-05-amplitube-style-gui-design.md`

## Global Constraints

- `IAudioNode::process()` and everything reachable from it stays allocation-free, lock-free, syscall-free, and throw-free. `LevelProbe::report()` and `RigGraph::is_empty()` are called from the audio callback and are bound by the same rule.
- `RigController`'s methods remain GUI-thread only. Bypass is a structural edit and republishes.
- No new third-party dependency, no image file, no font file. ImGui stays at v1.90.9, GLFW at 3.4.
- `apps/host_gui/ui/Geometry.hpp`, `ui/KnobMath.*`, `ui/PanelLibrary.*`, `ui/ChainLayout.*`, and `ui/LevelProbe.hpp` must not include any ImGui header: `sushidsp_tests` compiles them and does not link `imgui`.
- Every new file opens with the project's license header box, copied verbatim from an existing file with only the filename line changed, per `docs/CONTRIBUTING.md` section 3. No `@file` block: no file in the tree carries one, and CONTRIBUTING.md asks for the box alone.
- Doxygen `@brief`, and `@param`/`@return` where they state a rule the caller must keep, on every public function. Ordinary comments only where the *why* is non-obvious; never a comment restating what the code does.
- Builds and tests run through `sd` only: `sd build`, `sd test -s unit`. Never `cmake` or `ctest` directly.
- The preset schema stays backward compatible: a file written before this change loads with every slot active.
- `sd test` has no name filter. To check one test, run `sd test -s unit 2>&1 | grep <TestName>`.

## File Structure

| File | Responsibility |
|---|---|
| `apps/host_gui/ui/Geometry.hpp` | `Vec2`, `Color`, `rgba()`. No behavior, no ImGui. |
| `apps/host_gui/ui/KnobMath.hpp/.cpp` | Value, normalized, and angle mapping; drag arithmetic. |
| `apps/host_gui/ui/PanelLibrary.hpp/.cpp` | The per-node panel table and the generic grid fallback. |
| `apps/host_gui/ui/ChainLayout.hpp/.cpp` | Turns slots and edges into rows and columns. |
| `apps/host_gui/ui/LevelProbe.hpp` | Carries a block peak from the audio thread to the GUI thread. |
| `apps/host_gui/ui/Theme.hpp/.cpp` | Palette and `ImGuiStyle` setup. |
| `apps/host_gui/ui/Widgets.hpp/.cpp` | `Knob()` and `ToggleSwitch()`. |
| `apps/host_gui/ui/StagePanel.hpp/.cpp` | Draws the selected node's panel. |
| `apps/host_gui/ui/ChainStrip.hpp/.cpp` | Draws the chain strip; selection, drag and drop, bypass. |
| `apps/host_gui/ui/GearBrowser.hpp/.cpp` | Search box, category headers, gear cards. |
| `apps/host_gui/ui/MeterBar.hpp/.cpp` | Two level bars with their ballistics. |
| `apps/host_gui/ui/TopBar.hpp/.cpp` | Preset path, Save, Load, settings button. |
| `apps/host_gui/ui/DeviceSettings.hpp/.cpp` | Device and channel selection window. |
| `apps/host_gui/RigController.hpp/.cpp` | Gains bypass and one shared graph builder. |
| `include/SushiDSP/core/RigGraph.hpp`, `src/core/RigGraph.cpp` | Gains `is_empty()`; one stale comment fixed. |
| `apps/host_gui/main.cpp` | Window, device, frame loop, region calls. Nothing else. |

## Waves

Tasks inside a wave touch disjoint files and may run in parallel. A wave ends with one build the user runs.

| Wave | Tasks | Waits on | Build after the wave |
|---|---|---|---|
| 0 | 1 | nothing | `sd build` |
| 1 | 2, 3, 4, 5, 6, 7, 8 | Task 1 | `sd build && sd test -s unit` |
| 2 | 9, 10, 11, 12, 13 | Tasks 2, 5, 8 | `sd build` |
| 3 | 14, 15 | Tasks 3, 4, 7, 9 | `sd build` |
| 4 | 16 | every earlier task | `sd build && sd test -s unit`, then run the app |

Files per wave:

- **Wave 0:** every new file as a skeleton, plus `apps/host_gui/CMakeLists.txt` and `tests/CMakeLists.txt`.
- **Wave 1:** T2 `ui/KnobMath.*` + `tests/functional/unit/test_knob_math.cpp`; T3 `ui/PanelLibrary.*` + `tests/functional/unit/test_panel_library.cpp`; T4 `ui/ChainLayout.*` + `tests/functional/unit/test_chain_layout.cpp`; T5 `ui/LevelProbe.hpp` + `tests/functional/unit/test_level_probe.cpp` + `tests/functional/unit/test_rt_safety.cpp`; T6 `include/SushiDSP/core/RigGraph.hpp` + `src/core/RigGraph.cpp` + `tests/functional/unit/test_rig_graph.cpp`; T7 `apps/host_gui/RigController.*` + `tests/functional/unit/test_rig_controller.cpp`; T8 `ui/Theme.*`.
- **Wave 2:** T9 `ui/Widgets.*`; T10 `ui/GearBrowser.*`; T11 `ui/MeterBar.*`; T12 `ui/TopBar.*`; T13 `ui/DeviceSettings.*`.
- **Wave 3:** T14 `ui/StagePanel.*`; T15 `ui/ChainStrip.*`.
- **Wave 4:** T16 `apps/host_gui/main.cpp`.

Tasks 8 to 15 draw pixels and carry no unit test. Their gate is that `sd build` passes and their interface matches what this plan declares; the visual check happens once, in Task 16.

---

### Task 1: Build skeleton

Create every new file with its license header, `@file` block, and an empty (but compiling) body, and wire both CMake files. Every later task then edits only files it owns, so no two tasks in a wave touch one file.

**Files:**
- Create: `apps/host_gui/ui/Geometry.hpp` (complete, not a skeleton)
- Create as skeletons: `apps/host_gui/ui/KnobMath.hpp`, `KnobMath.cpp`, `PanelLibrary.hpp`, `PanelLibrary.cpp`, `ChainLayout.hpp`, `ChainLayout.cpp`, `LevelProbe.hpp`, `Theme.hpp`, `Theme.cpp`, `Widgets.hpp`, `Widgets.cpp`, `StagePanel.hpp`, `StagePanel.cpp`, `ChainStrip.hpp`, `ChainStrip.cpp`, `GearBrowser.hpp`, `GearBrowser.cpp`, `MeterBar.hpp`, `MeterBar.cpp`, `TopBar.hpp`, `TopBar.cpp`, `DeviceSettings.hpp`, `DeviceSettings.cpp`
- Create as skeletons: `tests/functional/unit/test_knob_math.cpp`, `test_panel_library.cpp`, `test_chain_layout.cpp`, `test_level_probe.cpp`
- Modify: `apps/host_gui/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing.
- Produces: `ui::Vec2`, `ui::Color`, `ui::rgba()`; a compiling skeleton for every unit named in the File Structure table.

- [ ] **Step 1: Write `ui/Geometry.hpp` in full**

```cpp
#pragma once

#include <cstdint>

namespace ui
{
    /// @brief A point or a size, in panel-local units.
    struct Vec2
    {
        float x;
        float y;
    };

    /// @brief A color packed as 0xRRGGBBAA.
    using Color = std::uint32_t;

    /// @brief Packs four 8-bit channels into a Color.
    /// @param r Red.
    /// @param g Green.
    /// @param b Blue.
    /// @param a Alpha; opaque by default.
    /// @return The packed color.
    constexpr Color rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
    {
        return (static_cast<Color>(r) << 24) | (static_cast<Color>(g) << 16)
             | (static_cast<Color>(b) << 8) | static_cast<Color>(a);
    }
}
```

- [ ] **Step 2: Create every other header skeleton**

Each header is the license block, then:

```cpp
#pragma once

namespace ui
{
}
```

`LevelProbe.hpp` additionally includes `<atomic>` and `<cstdint>` so Task 5 has them.

- [ ] **Step 3: Create every `.cpp` skeleton**

Each is the license block, then the include of its own header, then an empty `namespace ui { }`.

- [ ] **Step 4: Create the four test file skeletons**

Each is the license block, then:

```cpp
#include <gtest/gtest.h>
```

A translation unit with no `TEST` compiles and links; the test count does not change.

- [ ] **Step 5: Add the GUI sources to `apps/host_gui/CMakeLists.txt`**

Extend the `add_executable(sushidsp_host_gui ...)` list with, in this order:

```cmake
    ui/KnobMath.cpp
    ui/PanelLibrary.cpp
    ui/ChainLayout.cpp
    ui/Theme.cpp
    ui/Widgets.cpp
    ui/StagePanel.cpp
    ui/ChainStrip.cpp
    ui/GearBrowser.cpp
    ui/MeterBar.cpp
    ui/TopBar.cpp
    ui/DeviceSettings.cpp
```

Then add the include directory, directly below the `add_executable` call. Without it a `ui/*.cpp`
file cannot write `#include "ui/KnobMath.hpp"` or `#include "RigController.hpp"`; only the
source file's own directory is searched by default, and `apps/host_gui/CMakeLists.txt` sets no
include path today:

```cmake
target_include_directories(sushidsp_host_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
```

- [ ] **Step 6: Add the testable sources and test files to `tests/CMakeLists.txt`**

In the `add_executable(sushidsp_tests ...)` list, directly below the existing
`${CMAKE_SOURCE_DIR}/apps/host_gui/RigController.cpp` line:

```cmake
    functional/unit/test_knob_math.cpp
    functional/unit/test_panel_library.cpp
    functional/unit/test_chain_layout.cpp
    functional/unit/test_level_probe.cpp
    ${CMAKE_SOURCE_DIR}/apps/host_gui/ui/KnobMath.cpp
    ${CMAKE_SOURCE_DIR}/apps/host_gui/ui/PanelLibrary.cpp
    ${CMAKE_SOURCE_DIR}/apps/host_gui/ui/ChainLayout.cpp
```

`tests/CMakeLists.txt` already has `target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/apps/host_gui)`, so `#include "ui/KnobMath.hpp"` resolves from a test file. Only these three `.cpp` files are added: the rest include ImGui headers and `sushidsp_tests` does not link `imgui`.

- [ ] **Step 7: Build and run the suite**

Run: `sd build`
Expected: PASS, both `sushidsp_host_gui` and `sushidsp_tests` link.

Run: `sd test -s unit`
Expected: the same test count as before this task, all passing.

- [ ] **Step 8: Commit**

```bash
git add apps/host_gui/ui apps/host_gui/CMakeLists.txt tests/CMakeLists.txt tests/functional/unit/test_knob_math.cpp tests/functional/unit/test_panel_library.cpp tests/functional/unit/test_chain_layout.cpp tests/functional/unit/test_level_probe.cpp
git commit -m "host_gui: add the ui/ skeleton and wire it into both builds"
```

---

### Task 2: Knob math

**Files:**
- Modify: `apps/host_gui/ui/KnobMath.hpp`, `apps/host_gui/ui/KnobMath.cpp`
- Test: `tests/functional/unit/test_knob_math.cpp`

**Interfaces:**
- Consumes: `SushiDSP::ParameterDescriptor` and `SushiDSP::ParamKind` from `SushiDSP/core/NodeDescriptor.hpp`.
- Produces: `ui::kKnobSweepRadians`, `ui::kKnobStartRadians`, `ui::kKnobDragPixels`, `ui::kKnobFineDragPixels`, `ui::value_to_normalized`, `ui::normalized_to_value`, `ui::normalized_to_angle`, `ui::apply_drag`. Task 9 calls all four functions.

- [ ] **Step 1: Write the failing tests**

Append to `tests/functional/unit/test_knob_math.cpp`:

```cpp
#include "ui/KnobMath.hpp"

#include <cmath>

namespace
{
    constexpr SushiDSP::ParameterDescriptor kLinear{
        "drive", "Drive", 0.0f, 1.0f, 0.5f, SushiDSP::ParamKind::Linear, "" };
    constexpr SushiDSP::ParameterDescriptor kDecibel{
        "master", "Master", -60.0f, 12.0f, 0.0f, SushiDSP::ParamKind::Decibel, "dB" };
    constexpr SushiDSP::ParameterDescriptor kLog{
        "freq", "Freq", 20.0f, 20000.0f, 1000.0f, SushiDSP::ParamKind::Logarithmic, "Hz" };
    constexpr SushiDSP::ParameterDescriptor kLogBadMin{
        "freq", "Freq", 0.0f, 20000.0f, 1000.0f, SushiDSP::ParamKind::Logarithmic, "Hz" };
}

TEST(Unit_Dsp, KnobMathMapsLinearParameterEndpointsAndMidpoint)
{
    EXPECT_FLOAT_EQ(ui::value_to_normalized(kLinear, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(ui::value_to_normalized(kLinear, 1.0f), 1.0f);
    EXPECT_FLOAT_EQ(ui::value_to_normalized(kLinear, 0.5f), 0.5f);
}

TEST(Unit_Dsp, KnobMathMapsDecibelParameterLinearlyInItsOwnUnit)
{
    EXPECT_FLOAT_EQ(ui::value_to_normalized(kDecibel, -60.0f), 0.0f);
    EXPECT_FLOAT_EQ(ui::value_to_normalized(kDecibel, 12.0f), 1.0f);
    EXPECT_FLOAT_EQ(ui::value_to_normalized(kDecibel, -24.0f), 0.5f);
}

TEST(Unit_Dsp, KnobMathClampsValuesOutsideTheParameterRange)
{
    EXPECT_FLOAT_EQ(ui::value_to_normalized(kLinear, -3.0f), 0.0f);
    EXPECT_FLOAT_EQ(ui::value_to_normalized(kLinear, 4.0f), 1.0f);
}

TEST(Unit_Dsp, KnobMathRoundTripsValueThroughNormalized)
{
    for (float value : { -60.0f, -33.0f, 0.0f, 11.5f })
    {
        const float normalized = ui::value_to_normalized(kDecibel, value);
        EXPECT_NEAR(ui::normalized_to_value(kDecibel, normalized), value, 1e-4f);
    }
}

TEST(Unit_Dsp, KnobMathGivesLogarithmicParameterAGeometricMidpoint)
{
    EXPECT_NEAR(ui::normalized_to_value(kLog, 0.5f), std::sqrt(20.0f * 20000.0f), 1e-2f);
    EXPECT_NEAR(ui::value_to_normalized(kLog, 20000.0f), 1.0f, 1e-5f);
}

TEST(Unit_Dsp, KnobMathFallsBackToLinearWhenALogarithmicRangeStartsAtZero)
{
    EXPECT_FLOAT_EQ(ui::value_to_normalized(kLogBadMin, 10000.0f), 0.5f);
    EXPECT_FLOAT_EQ(ui::normalized_to_value(kLogBadMin, 0.5f), 10000.0f);
}

TEST(Unit_Dsp, KnobMathDragMovesUpwardPointerTravelTowardTheMaximum)
{
    // ImGui's y grows downward, so an upward drag is a negative delta.
    EXPECT_GT(ui::apply_drag(0.5f, -20.0f, false), 0.5f);
    EXPECT_LT(ui::apply_drag(0.5f, 20.0f, false), 0.5f);
}

TEST(Unit_Dsp, KnobMathDragClampsAtBothEnds)
{
    EXPECT_FLOAT_EQ(ui::apply_drag(0.9f, -1000.0f, false), 1.0f);
    EXPECT_FLOAT_EQ(ui::apply_drag(0.1f, 1000.0f, false), 0.0f);
}

TEST(Unit_Dsp, KnobMathFineDragTravelsExactlyOneQuarterAsFar)
{
    const float coarse = ui::apply_drag(0.0f, -50.0f, false);
    const float fine = ui::apply_drag(0.0f, -50.0f, true);
    EXPECT_NEAR(fine * 4.0f, coarse, 1e-6f);
}

TEST(Unit_Dsp, KnobMathAngleSpansTheFullSweepAcrossTheNormalizedRange)
{
    EXPECT_FLOAT_EQ(ui::normalized_to_angle(0.0f), ui::kKnobStartRadians);
    EXPECT_FLOAT_EQ(ui::normalized_to_angle(1.0f), ui::kKnobStartRadians + ui::kKnobSweepRadians);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `sd test -s unit 2>&1 | grep KnobMath`
Expected: the build fails, because `ui/KnobMath.hpp` declares none of these functions.

- [ ] **Step 3: Write the header**

Replace `apps/host_gui/ui/KnobMath.hpp`'s body (keep the license and `@file` blocks):

```cpp
#pragma once

#include "SushiDSP/core/NodeDescriptor.hpp"

namespace ui
{
    /// @brief The knob's sweep, from the 7 o'clock position clockwise to 5 o'clock.
    inline constexpr float kKnobSweepRadians = 4.71238898f;

    /// @brief Where the sweep begins, measured clockwise from the positive x axis.
    inline constexpr float kKnobStartRadians = 2.35619449f;

    /// @brief Pointer travel, in pixels, that sweeps a knob end to end.
    inline constexpr float kKnobDragPixels = 200.0f;

    /// @brief Pointer travel, in pixels, that sweeps a knob end to end while Shift is held.
    inline constexpr float kKnobFineDragPixels = 800.0f;

    /// @brief Converts a parameter value to its position along the knob's sweep.
    /// @param param The parameter, whose kind selects the taper.
    /// @param value The value; clamped into the parameter's range first.
    /// @return The position, in [0, 1].
    float value_to_normalized(const SushiDSP::ParameterDescriptor& param, float value);

    /// @brief Converts a position along the sweep back to a parameter value.
    /// @param param The parameter, whose kind selects the taper.
    /// @param normalized The position; clamped into [0, 1] first.
    /// @return The value.
    float normalized_to_value(const SushiDSP::ParameterDescriptor& param, float normalized);

    /// @brief Converts a sweep position to the pointer's angle.
    /// @param normalized The position; clamped into [0, 1] first.
    /// @return The angle in radians, clockwise from the positive x axis.
    float normalized_to_angle(float normalized);

    /// @brief Applies one frame of pointer travel to a sweep position.
    /// @param normalized The current position.
    /// @param pixels_dy The frame's vertical pointer travel, positive downward.
    /// @param fine True while Shift is held.
    /// @return The new position, clamped into [0, 1].
    float apply_drag(float normalized, float pixels_dy, bool fine);
}
```

- [ ] **Step 4: Write the implementation**

Replace `apps/host_gui/ui/KnobMath.cpp`'s body:

```cpp
#include "ui/KnobMath.hpp"

#include <algorithm>
#include <cmath>

namespace ui
{
    namespace
    {
        /// @brief Reports whether a parameter's range admits a logarithmic taper.
        bool uses_log_taper(const SushiDSP::ParameterDescriptor& param)
        {
            return param.kind == SushiDSP::ParamKind::Logarithmic
                && param.min > 0.0f && param.max > param.min;
        }
    }

    float value_to_normalized(const SushiDSP::ParameterDescriptor& param, float value)
    {
        const float clamped = std::clamp(value, param.min, param.max);
        if (uses_log_taper(param))
        {
            return std::log(clamped / param.min) / std::log(param.max / param.min);
        }
        if (param.max <= param.min)
        {
            return 0.0f;
        }
        return (clamped - param.min) / (param.max - param.min);
    }

    float normalized_to_value(const SushiDSP::ParameterDescriptor& param, float normalized)
    {
        const float position = std::clamp(normalized, 0.0f, 1.0f);
        if (uses_log_taper(param))
        {
            return param.min * std::pow(param.max / param.min, position);
        }
        return param.min + position * (param.max - param.min);
    }

    float normalized_to_angle(float normalized)
    {
        return kKnobStartRadians + std::clamp(normalized, 0.0f, 1.0f) * kKnobSweepRadians;
    }

    float apply_drag(float normalized, float pixels_dy, bool fine)
    {
        const float travel = fine ? kKnobFineDragPixels : kKnobDragPixels;
        return std::clamp(normalized - pixels_dy / travel, 0.0f, 1.0f);
    }
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `sd test -s unit 2>&1 | grep KnobMath`
Expected: ten `Passed` lines.

- [ ] **Step 6: Commit**

```bash
git add apps/host_gui/ui/KnobMath.hpp apps/host_gui/ui/KnobMath.cpp tests/functional/unit/test_knob_math.cpp
git commit -m "host_gui: map parameter values onto a knob sweep"
```

---

### Task 3: Panel library

**Files:**
- Modify: `apps/host_gui/ui/PanelLibrary.hpp`, `apps/host_gui/ui/PanelLibrary.cpp`
- Test: `tests/functional/unit/test_panel_library.cpp`

**Interfaces:**
- Consumes: `ui::Vec2`, `ui::Color`, `ui::rgba()` from `ui/Geometry.hpp`; `SushiDSP::NodeDescriptor`, `SushiDSP::ParamKind` from `SushiDSP/core/NodeDescriptor.hpp`; `SushiDSP::node_registry()` from `SushiDSP/node_registry/NodeRegistry.hpp` (test only).
- Produces: `ui::ControlKind`, `ui::ControlPlacement`, `ui::PanelSkin`, `ui::PanelDescriptor`, `ui::panel_for()`, `ui::auto_panel()`. Task 14 calls `panel_for` and `auto_panel`.

- [ ] **Step 1: Write the failing tests**

Append to `tests/functional/unit/test_panel_library.cpp`:

```cpp
#include "ui/PanelLibrary.hpp"

#include <cstring>
#include <string>
#include <vector>

#include "SushiDSP/node_registry/NodeRegistry.hpp"

TEST(Unit_Dsp, PanelLibraryReturnsAHandAuthoredPanelForAKnownNodeType)
{
    const ui::PanelDescriptor* panel = ui::panel_for("sushidsp.jcm800");
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(panel->control_count, 8u);
    EXPECT_GT(panel->aspect.x, panel->aspect.y);
}

TEST(Unit_Dsp, PanelLibraryReturnsNullForANodeTypeWithNoEntry)
{
    EXPECT_EQ(ui::panel_for("sushidsp.not_a_real_node"), nullptr);
}

TEST(Unit_Dsp, PanelLibraryTableNamesOnlyParametersTheDescriptorDeclares)
{
    for (const SushiDSP::NodeEntry& entry : SushiDSP::node_registry())
    {
        const ui::PanelDescriptor* panel = ui::panel_for(entry.descriptor->id);
        if (panel == nullptr)
        {
            continue;
        }
        for (std::size_t c = 0; c < panel->control_count; ++c)
        {
            const char* param_id = panel->controls[c].param_id;
            bool found = false;
            for (std::size_t p = 0; p < entry.descriptor->parameter_count; ++p)
            {
                if (std::strcmp(entry.descriptor->parameters[p].id, param_id) == 0)
                {
                    found = true;
                    break;
                }
            }
            EXPECT_TRUE(found) << entry.descriptor->id << " has no parameter '" << param_id << "'";
        }
    }
}

TEST(Unit_Dsp, PanelLibraryEveryTableEntryNamesARegisteredNodeType)
{
    for (const ui::PanelDescriptor& panel : ui::panel_table())
    {
        bool found = false;
        for (const SushiDSP::NodeEntry& entry : SushiDSP::node_registry())
        {
            if (std::strcmp(entry.descriptor->id, panel.descriptor_id) == 0)
            {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "panel table names unregistered node type '" << panel.descriptor_id << "'";
    }
}

TEST(Unit_Dsp, PanelLibraryAutoPanelPlacesOneControlPerParameterInASquareGrid)
{
    const SushiDSP::NodeDescriptor& jcm = SushiDSP::jcm800_descriptor();
    std::vector<ui::ControlPlacement> storage;
    const ui::PanelDescriptor panel = ui::auto_panel(jcm, storage);

    EXPECT_EQ(panel.control_count, jcm.parameter_count);
    EXPECT_EQ(storage.size(), jcm.parameter_count);

    // 8 parameters fit a 3-column grid, so rows 0, 1 and 2 are used.
    std::size_t distinct_rows = 0;
    float previous_y = -1.0f;
    for (const ui::ControlPlacement& placement : storage)
    {
        if (placement.position.y != previous_y)
        {
            ++distinct_rows;
            previous_y = placement.position.y;
        }
    }
    EXPECT_EQ(distinct_rows, 3u);
}

TEST(Unit_Dsp, PanelLibraryAutoPanelPicksTheControlKindFromTheParameterKind)
{
    constexpr SushiDSP::ParameterDescriptor params[] = {
        { "a", "A", 0.0f, 1.0f, 0.0f, SushiDSP::ParamKind::Linear,  "" },
        { "b", "B", 0.0f, 1.0f, 0.0f, SushiDSP::ParamKind::Boolean, "" },
        { "c", "C", 0.0f, 3.0f, 0.0f, SushiDSP::ParamKind::Choice,  "" },
    };
    const SushiDSP::NodeDescriptor node{ "test.node", "Test", params, 3, { 1, 1 }, nullptr };

    std::vector<ui::ControlPlacement> storage;
    ui::auto_panel(node, storage);

    ASSERT_EQ(storage.size(), 3u);
    EXPECT_EQ(storage[0].kind, ui::ControlKind::Knob);
    EXPECT_EQ(storage[1].kind, ui::ControlKind::Switch);
    EXPECT_EQ(storage[2].kind, ui::ControlKind::Readout);
}

TEST(Unit_Dsp, PanelLibraryMixerShowsOneGainKnobPerConnectedInput)
{
    const ui::PanelDescriptor* panel = ui::panel_for("sushidsp.mixer");
    ASSERT_NE(panel, nullptr);
    ASSERT_NE(panel->is_control_visible, nullptr);

    EXPECT_TRUE(panel->is_control_visible(0, 2));
    EXPECT_TRUE(panel->is_control_visible(1, 2));
    EXPECT_FALSE(panel->is_control_visible(2, 2));
    EXPECT_FALSE(panel->is_control_visible(0, 0));
}

TEST(Unit_Dsp, PanelLibraryLeavesTheVisibilityRuleUnsetForEveryPanelButTheMixer)
{
    for (const ui::PanelDescriptor& panel : ui::panel_table())
    {
        if (std::strcmp(panel.descriptor_id, "sushidsp.mixer") == 0)
        {
            continue;
        }
        EXPECT_EQ(panel.is_control_visible, nullptr) << panel.descriptor_id;
    }
}
```

The test file also needs `#include "SushiDSP/jcm800/Jcm800.hpp"` for `jcm800_descriptor()`.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `sd test -s unit 2>&1 | grep PanelLibrary`
Expected: the build fails; `ui/PanelLibrary.hpp` declares none of these types.

- [ ] **Step 3: Write the header**

Replace `apps/host_gui/ui/PanelLibrary.hpp`'s body:

```cpp
#pragma once

#include <cstddef>
#include <vector>

#include "SushiDSP/core/NodeDescriptor.hpp"

#include "ui/Geometry.hpp"

namespace ui
{
    /// @brief Which widget a placement draws.
    enum class ControlKind
    {
        Knob,
        Switch,
        Readout
    };

    /// @brief One control's place on a panel. position is the control's center in [0, 1] on both
    /// axes and size is its diameter as a fraction of panel height, so a panel scales with the
    /// window without being laid out again.
    struct ControlPlacement
    {
        const char* param_id;
        ControlKind kind;
        Vec2        position;
        float       size;
        const char* caption;   ///< nullptr means use ParameterDescriptor::label.
    };

    /// @brief A panel's colors. texture_id is the hook a later bitmap-skin change fills in; every
    /// panel defined today leaves it nullptr and the drawing code ignores it.
    struct PanelSkin
    {
        Color       body;
        Color       face;
        Color       accent;
        Color       text;
        const char* texture_id;
    };

    /// @brief Everything the stage needs to draw one node type's front panel.
    struct PanelDescriptor
    {
        const char*             descriptor_id;
        Vec2                    aspect;   ///< Width and height in arbitrary units; only the ratio matters.
        PanelSkin               skin;
        const ControlPlacement* controls;
        std::size_t             control_count;

        /// @brief Optional per-control visibility rule; nullptr draws every control.
        /// @param control_index The control's index in controls.
        /// @param upstream_count How many slots feed this node.
        /// @return True to draw the control.
        bool (*is_control_visible)(std::size_t control_index, std::size_t upstream_count);
    };

    /// @brief Every hand-authored panel.
    /// @return The table, for callers that need to walk it rather than look one entry up.
    const std::vector<PanelDescriptor>& panel_table();

    /// @brief Looks up the hand-authored panel for a node type.
    /// @param descriptor_id The NodeDescriptor id.
    /// @return The panel, or nullptr when the table has no entry for it.
    const PanelDescriptor* panel_for(const char* descriptor_id);

    /// @brief Builds a generic grid panel for a node type with no hand-authored entry, laying its
    /// parameters out row-major across ceil(sqrt(parameter_count)) columns.
    /// @param node The node type to lay out.
    /// @param storage Receives the generated placements and must outlive the returned descriptor,
    /// which points into it.
    /// @return The generated panel.
    PanelDescriptor auto_panel(const SushiDSP::NodeDescriptor& node,
                               std::vector<ControlPlacement>& storage);
}
```

- [ ] **Step 4: Write the implementation**

Replace `apps/host_gui/ui/PanelLibrary.cpp`'s body:

```cpp
#include "ui/PanelLibrary.hpp"

#include <cmath>
#include <cstring>

namespace ui
{
    namespace
    {
        constexpr ControlPlacement kJcm800Controls[] = {
            { "presence",   ControlKind::Knob, { 0.34f, 0.78f }, 0.14f, "Presence"   },
            { "bass",       ControlKind::Knob, { 0.42f, 0.78f }, 0.14f, "Bass"       },
            { "middle",     ControlKind::Knob, { 0.50f, 0.78f }, 0.14f, "Middle"     },
            { "treble",     ControlKind::Knob, { 0.58f, 0.78f }, 0.14f, "Treble"     },
            { "master",     ControlKind::Knob, { 0.66f, 0.78f }, 0.14f, "Master"     },
            { "preamp",     ControlKind::Knob, { 0.74f, 0.78f }, 0.14f, "Pre Amp"    },
            { "input_trim", ControlKind::Knob, { 0.13f, 0.78f }, 0.10f, "Input Trim" },
            { "output",     ControlKind::Knob, { 0.21f, 0.78f }, 0.10f, "Output"     },
        };

        constexpr ControlPlacement kTubeScreamerControls[] = {
            { "level",      ControlKind::Knob, { 0.50f, 0.13f }, 0.13f, "Level"      },
            { "drive",      ControlKind::Knob, { 0.24f, 0.22f }, 0.13f, "Drive"      },
            { "tone",       ControlKind::Knob, { 0.76f, 0.22f }, 0.13f, "Tone"       },
            { "input_trim", ControlKind::Knob, { 0.50f, 0.36f }, 0.09f, "Input Trim" },
        };

        constexpr ControlPlacement kMixerControls[] = {
            { "gain_0", ControlKind::Knob, { 0.10f, 0.55f }, 0.16f, "1" },
            { "gain_1", ControlKind::Knob, { 0.21f, 0.55f }, 0.16f, "2" },
            { "gain_2", ControlKind::Knob, { 0.33f, 0.55f }, 0.16f, "3" },
            { "gain_3", ControlKind::Knob, { 0.44f, 0.55f }, 0.16f, "4" },
            { "gain_4", ControlKind::Knob, { 0.56f, 0.55f }, 0.16f, "5" },
            { "gain_5", ControlKind::Knob, { 0.67f, 0.55f }, 0.16f, "6" },
            { "gain_6", ControlKind::Knob, { 0.79f, 0.55f }, 0.16f, "7" },
            { "gain_7", ControlKind::Knob, { 0.90f, 0.55f }, 0.16f, "8" },
        };

        /// @brief Hides the gain knobs of mixer inputs nothing is connected to.
        bool mixer_control_visible(std::size_t control_index, std::size_t upstream_count)
        {
            return control_index < upstream_count;
        }
    }

    const std::vector<PanelDescriptor>& panel_table()
    {
        static const std::vector<PanelDescriptor> table{
            { "sushidsp.jcm800", { 900.0f, 420.0f },
              { rgba(26, 26, 28), rgba(158, 124, 58), rgba(214, 168, 74), rgba(28, 22, 8), nullptr },
              kJcm800Controls, 8, nullptr },
            { "sushidsp.tube_screamer", { 320.0f, 560.0f },
              { rgba(46, 112, 58), rgba(32, 78, 42), rgba(226, 214, 178), rgba(232, 232, 224), nullptr },
              kTubeScreamerControls, 4, nullptr },
            { "sushidsp.splitter", { 320.0f, 200.0f },
              { rgba(38, 38, 44), rgba(28, 28, 32), rgba(214, 168, 74), rgba(228, 228, 232), nullptr },
              nullptr, 0, nullptr },
            { "sushidsp.mixer", { 640.0f, 300.0f },
              { rgba(38, 38, 44), rgba(28, 28, 32), rgba(214, 168, 74), rgba(228, 228, 232), nullptr },
              kMixerControls, 8, &mixer_control_visible },
        };
        return table;
    }

    const PanelDescriptor* panel_for(const char* descriptor_id)
    {
        for (const PanelDescriptor& panel : panel_table())
        {
            if (std::strcmp(panel.descriptor_id, descriptor_id) == 0)
            {
                return &panel;
            }
        }
        return nullptr;
    }

    PanelDescriptor auto_panel(const SushiDSP::NodeDescriptor& node,
                               std::vector<ControlPlacement>& storage)
    {
        const std::size_t count = node.parameter_count;
        const std::size_t columns = count == 0
            ? 1
            : static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(count))));
        const std::size_t rows = count == 0 ? 1 : (count + columns - 1) / columns;

        storage.clear();
        storage.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            const SushiDSP::ParameterDescriptor& param = node.parameters[i];
            ControlKind kind = ControlKind::Knob;
            if (param.kind == SushiDSP::ParamKind::Boolean)
            {
                kind = ControlKind::Switch;
            }
            else if (param.kind == SushiDSP::ParamKind::Choice)
            {
                kind = ControlKind::Readout;
            }

            const std::size_t column = i % columns;
            const std::size_t row = i / columns;
            const float x = (static_cast<float>(column) + 0.5f) / static_cast<float>(columns);
            const float y = (static_cast<float>(row) + 0.5f) / static_cast<float>(rows);
            storage.push_back({ param.id, kind, { x, y }, 0.55f / static_cast<float>(rows), nullptr });
        }

        return PanelDescriptor{
            node.id,
            { 200.0f * static_cast<float>(columns), 160.0f * static_cast<float>(rows) },
            { rgba(38, 38, 44), rgba(28, 28, 32), rgba(214, 168, 74), rgba(228, 228, 232), nullptr },
            storage.data(),
            storage.size(),
            nullptr
        };
    }
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `sd test -s unit 2>&1 | grep PanelLibrary`
Expected: eight `Passed` lines.

- [ ] **Step 6: Commit**

```bash
git add apps/host_gui/ui/PanelLibrary.hpp apps/host_gui/ui/PanelLibrary.cpp tests/functional/unit/test_panel_library.cpp
git commit -m "host_gui: describe each node type's front panel"
```

---

### Task 4: Chain layout

**Files:**
- Modify: `apps/host_gui/ui/ChainLayout.hpp`, `apps/host_gui/ui/ChainLayout.cpp`
- Test: `tests/functional/unit/test_chain_layout.cpp`

**Interfaces:**
- Consumes: `RigController::SlotId` from `RigController.hpp`.
- Produces: `ui::ChainCell`, `ui::ChainLink`, `ui::ChainView`, `ui::layout_chain()`. Task 15 calls `layout_chain`.

- [ ] **Step 1: Write the failing tests**

Append to `tests/functional/unit/test_chain_layout.cpp`:

```cpp
#include "ui/ChainLayout.hpp"

#include <algorithm>

namespace
{
    /// @brief Finds a slot's cell, so a test asserts on identity rather than on cell order.
    const ui::ChainCell& cell_for(const ui::ChainView& view, RigController::SlotId slot)
    {
        const auto it = std::find_if(view.cells.begin(), view.cells.end(),
            [slot](const ui::ChainCell& cell) { return cell.slot == slot; });
        EXPECT_NE(it, view.cells.end());
        return *it;
    }
}

TEST(Unit_Dsp, ChainLayoutPlacesALinearChainOnOneRowWithAscendingColumns)
{
    const ui::ChainView view = ui::layout_chain({ 10, 11, 12 }, { { 10, 11 }, { 11, 12 } });

    EXPECT_EQ(view.cells.size(), 3u);
    EXPECT_EQ(view.row_count, 1);
    EXPECT_EQ(view.column_count, 3);
    EXPECT_EQ(cell_for(view, 10).column, 0);
    EXPECT_EQ(cell_for(view, 11).column, 1);
    EXPECT_EQ(cell_for(view, 12).column, 2);
    EXPECT_EQ(cell_for(view, 10).row, 0);
    EXPECT_EQ(cell_for(view, 11).row, 0);
    EXPECT_EQ(cell_for(view, 12).row, 0);
    EXPECT_EQ(view.links.size(), 2u);
}

TEST(Unit_Dsp, ChainLayoutPutsTheSecondBranchOnRowOneAndRejoinsAtRowZero)
{
    // 1 -> splitter 2 -> branches 3 and 4 -> mixer 5
    const ui::ChainView view = ui::layout_chain(
        { 1, 2, 3, 4, 5 },
        { { 1, 2 }, { 2, 3 }, { 2, 4 }, { 3, 5 }, { 4, 5 } });

    EXPECT_EQ(view.row_count, 2);
    EXPECT_EQ(view.column_count, 4);
    EXPECT_EQ(cell_for(view, 1).row, 0);
    EXPECT_EQ(cell_for(view, 2).row, 0);
    EXPECT_EQ(cell_for(view, 3).row, 0);
    EXPECT_EQ(cell_for(view, 4).row, 1);
    EXPECT_EQ(cell_for(view, 5).row, 0);
    EXPECT_EQ(cell_for(view, 5).column, 3);
    EXPECT_EQ(view.links.size(), 5u);
}

TEST(Unit_Dsp, ChainLayoutPushesTheRejoinRightOfTheLongestBranch)
{
    // Branch A is two nodes long (3, 6); branch B is one node (4).
    const ui::ChainView view = ui::layout_chain(
        { 2, 3, 4, 5, 6 },
        { { 2, 3 }, { 2, 4 }, { 3, 6 }, { 6, 5 }, { 4, 5 } });

    EXPECT_EQ(cell_for(view, 2).column, 0);
    EXPECT_EQ(cell_for(view, 3).column, 1);
    EXPECT_EQ(cell_for(view, 6).column, 2);
    EXPECT_EQ(cell_for(view, 4).column, 1);
    EXPECT_EQ(cell_for(view, 5).column, 3);
    EXPECT_EQ(view.column_count, 4);
}

TEST(Unit_Dsp, ChainLayoutGivesAnEmptyViewForAnEmptyRig)
{
    const ui::ChainView view = ui::layout_chain({}, {});

    EXPECT_TRUE(view.cells.empty());
    EXPECT_TRUE(view.links.empty());
    EXPECT_EQ(view.row_count, 0);
    EXPECT_EQ(view.column_count, 0);
}

TEST(Unit_Dsp, ChainLayoutPlacesAnUnconnectedSlotInTheFirstColumn)
{
    const ui::ChainView view = ui::layout_chain({ 7, 8 }, {});

    EXPECT_EQ(cell_for(view, 7).column, 0);
    EXPECT_EQ(cell_for(view, 8).column, 0);
    EXPECT_EQ(view.column_count, 1);
}

TEST(Unit_Dsp, ChainLayoutLinksIndexTheCellsTheirEdgeConnects)
{
    const ui::ChainView view = ui::layout_chain({ 10, 11 }, { { 10, 11 } });

    ASSERT_EQ(view.links.size(), 1u);
    EXPECT_EQ(view.cells[view.links[0].from_cell].slot, 10u);
    EXPECT_EQ(view.cells[view.links[0].to_cell].slot, 11u);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `sd test -s unit 2>&1 | grep ChainLayout`
Expected: the build fails; `ui/ChainLayout.hpp` declares none of these types.

- [ ] **Step 3: Write the header**

Replace `apps/host_gui/ui/ChainLayout.hpp`'s body:

```cpp
#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "RigController.hpp"

namespace ui
{
    /// @brief One slot's place on the strip's grid.
    struct ChainCell
    {
        RigController::SlotId slot;
        int                   row;
        int                   column;
    };

    /// @brief One edge, as a pair of indices into ChainView::cells.
    struct ChainLink
    {
        std::size_t from_cell;
        std::size_t to_cell;
    };

    /// @brief Where every slot sits and which cells each edge joins.
    struct ChainView
    {
        std::vector<ChainCell> cells;
        std::vector<ChainLink> links;
        int                    row_count;
        int                    column_count;
    };

    /// @brief Places every slot on a row and column grid for drawing. A node's column is one past
    /// its deepest upstream, so a rejoin lands right of the longest branch. Row is 0 along a
    /// linear run; a node's second downstream edge starts row 1 and everything after it on that
    /// branch stays on row 1, and a node with two or more upstream edges rejoins at row 0.
    /// @param slots Every live slot.
    /// @param edges Every edge, as (from, to) pairs.
    /// @return The grid; cells is empty and both counts are 0 for an empty rig. Cells come out in
    /// topological order.
    ChainView layout_chain(const std::vector<RigController::SlotId>& slots,
                           const std::vector<std::pair<RigController::SlotId, RigController::SlotId>>& edges);
}
```

- [ ] **Step 4: Write the implementation**

Replace `apps/host_gui/ui/ChainLayout.cpp`'s body:

```cpp
#include "ui/ChainLayout.hpp"

#include <algorithm>
#include <unordered_map>

namespace ui
{
    ChainView layout_chain(const std::vector<RigController::SlotId>& slots,
                           const std::vector<std::pair<RigController::SlotId, RigController::SlotId>>& edges)
    {
        ChainView view{ {}, {}, 0, 0 };
        if (slots.empty())
        {
            return view;
        }

        std::unordered_map<RigController::SlotId, std::vector<RigController::SlotId>> downstream;
        std::unordered_map<RigController::SlotId, std::size_t> in_degree;
        std::unordered_map<RigController::SlotId, std::size_t> upstream_count;
        for (RigController::SlotId slot : slots)
        {
            in_degree[slot] = 0;
            upstream_count[slot] = 0;
        }
        for (const auto& edge : edges)
        {
            if (in_degree.count(edge.first) == 0 || in_degree.count(edge.second) == 0)
            {
                continue;
            }
            downstream[edge.first].push_back(edge.second);
            ++in_degree[edge.second];
            ++upstream_count[edge.second];
        }

        // Kahn's algorithm, seeded in the caller's slot order so the result is deterministic.
        std::vector<RigController::SlotId> ready;
        for (RigController::SlotId slot : slots)
        {
            if (in_degree[slot] == 0)
            {
                ready.push_back(slot);
            }
        }

        std::unordered_map<RigController::SlotId, int> column;
        std::unordered_map<RigController::SlotId, int> row;
        std::vector<RigController::SlotId> ordered;
        ordered.reserve(slots.size());
        for (std::size_t head = 0; head < ready.size(); ++head)
        {
            const RigController::SlotId current = ready[head];
            ordered.push_back(current);

            const auto children = downstream.find(current);
            if (children == downstream.end())
            {
                continue;
            }
            for (std::size_t branch = 0; branch < children->second.size(); ++branch)
            {
                const RigController::SlotId child = children->second[branch];
                column[child] = std::max(column[child], column[current] + 1);
                // A rejoin returns to row 0; the first branch inherits, later branches drop to 1.
                if (upstream_count[child] < 2)
                {
                    row[child] = branch == 0 ? row[current] : 1;
                }
                if (--in_degree[child] == 0)
                {
                    ready.push_back(child);
                }
            }
        }

        view.cells.reserve(ordered.size());
        std::unordered_map<RigController::SlotId, std::size_t> cell_index;
        for (RigController::SlotId slot : ordered)
        {
            cell_index[slot] = view.cells.size();
            view.cells.push_back({ slot, row[slot], column[slot] });
            view.row_count = std::max(view.row_count, row[slot] + 1);
            view.column_count = std::max(view.column_count, column[slot] + 1);
        }

        for (const auto& edge : edges)
        {
            const auto from = cell_index.find(edge.first);
            const auto to = cell_index.find(edge.second);
            if (from != cell_index.end() && to != cell_index.end())
            {
                view.links.push_back({ from->second, to->second });
            }
        }

        return view;
    }
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `sd test -s unit 2>&1 | grep ChainLayout`
Expected: six `Passed` lines.

- [ ] **Step 6: Commit**

```bash
git add apps/host_gui/ui/ChainLayout.hpp apps/host_gui/ui/ChainLayout.cpp tests/functional/unit/test_chain_layout.cpp
git commit -m "host_gui: place a branching rig on a row and column grid"
```

---

### Task 5: Level probe

**Files:**
- Modify: `apps/host_gui/ui/LevelProbe.hpp`
- Test: `tests/functional/unit/test_level_probe.cpp`
- Test: `tests/functional/unit/test_rt_safety.cpp` (append one test; the allocation trap lives there)

**Interfaces:**
- Consumes: nothing.
- Produces: `ui::LevelProbe` with `void report(const float*, std::uint32_t) noexcept` and `float read_peak() noexcept`. Task 11 and Task 16 both use it.

- [ ] **Step 1: Write the failing tests**

Append to `tests/functional/unit/test_level_probe.cpp`:

```cpp
#include "ui/LevelProbe.hpp"

TEST(Unit_Dsp, LevelProbeReportsTheLargestMagnitudeIncludingNegativeSamples)
{
    ui::LevelProbe probe;
    const float block[] = { 0.1f, -0.8f, 0.3f };
    probe.report(block, 3);

    EXPECT_FLOAT_EQ(probe.read_peak(), 0.8f);
}

TEST(Unit_Dsp, LevelProbeKeepsTheHighestPeakAcrossSeveralBlocks)
{
    ui::LevelProbe probe;
    const float quiet[] = { 0.2f };
    const float loud[] = { 0.9f };
    probe.report(quiet, 1);
    probe.report(loud, 1);
    probe.report(quiet, 1);

    EXPECT_FLOAT_EQ(probe.read_peak(), 0.9f);
}

TEST(Unit_Dsp, LevelProbeResetsAfterEachRead)
{
    ui::LevelProbe probe;
    const float block[] = { 0.5f };
    probe.report(block, 1);

    EXPECT_FLOAT_EQ(probe.read_peak(), 0.5f);
    EXPECT_FLOAT_EQ(probe.read_peak(), 0.0f);
}

TEST(Unit_Dsp, LevelProbeReportsZeroForAnEmptyBlock)
{
    ui::LevelProbe probe;
    probe.report(nullptr, 0);

    EXPECT_FLOAT_EQ(probe.read_peak(), 0.0f);
}
```

Append to `tests/functional/unit/test_rt_safety.cpp`, alongside the existing trap tests:

```cpp
TEST(Unit_Dsp, LevelProbeReportPerformsNoHeapAllocation)
{
    ui::LevelProbe probe;
    std::vector<float> block(256, 0.25f);

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);
    for (int i = 0; i < 64; ++i)
    {
        probe.report(block.data(), 256);
    }
    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
}
```

`test_rt_safety.cpp` needs `#include "ui/LevelProbe.hpp"` added to its include block.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `sd test -s unit 2>&1 | grep LevelProbe`
Expected: the build fails; `ui/LevelProbe.hpp` declares no `LevelProbe`.

- [ ] **Step 3: Write the header**

Replace `apps/host_gui/ui/LevelProbe.hpp`'s body:

```cpp
#pragma once

#include <atomic>
#include <cmath>
#include <cstdint>

namespace ui
{
    /// @brief Carries a block's peak magnitude from the audio thread to the GUI thread.
    class LevelProbe
    {
    public:
        /// @brief Raises the stored peak to this block's largest magnitude. Audio thread only:
        /// allocation-free, lock-free, and never blocks.
        /// @param samples The block; may be nullptr when count is 0.
        /// @param count Frames in the block.
        void report(const float* samples, std::uint32_t count) noexcept
        {
            float block_peak = 0.0f;
            for (std::uint32_t frame = 0; frame < count; ++frame)
            {
                block_peak = std::fmax(block_peak, std::fabs(samples[frame]));
            }

            float seen = peak_.load(std::memory_order_relaxed);
            while (block_peak > seen
                && !peak_.compare_exchange_weak(seen, block_peak, std::memory_order_relaxed))
            {
            }
        }

        /// @brief Reads the largest peak reported since the previous call and resets the
        /// accumulator. GUI thread only.
        /// @return The peak magnitude.
        float read_peak() noexcept
        {
            return peak_.exchange(0.0f, std::memory_order_relaxed);
        }

    private:
        std::atomic<float> peak_{ 0.0f };
    };
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `sd test -s unit 2>&1 | grep LevelProbe`
Expected: five `Passed` lines, four from `test_level_probe.cpp` and one from `test_rt_safety.cpp`.

- [ ] **Step 5: Commit**

```bash
git add apps/host_gui/ui/LevelProbe.hpp tests/functional/unit/test_level_probe.cpp tests/functional/unit/test_rt_safety.cpp
git commit -m "host_gui: carry a block peak from the audio thread to the meters"
```

---

### Task 6: `RigGraph::is_empty()`

**Files:**
- Modify: `include/SushiDSP/core/RigGraph.hpp`
- Modify: `src/core/RigGraph.cpp`
- Test: `tests/functional/unit/test_rig_graph.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `bool SushiDSP::RigGraph::is_empty() const noexcept`. Task 16's audio callback calls it.

- [ ] **Step 1: Write the failing tests**

Append to `tests/functional/unit/test_rig_graph.cpp`:

```cpp
TEST(Unit_Dsp, RigGraphIsEmptyBeforeAnyNodeIsAdded)
{
    SushiDSP::RigGraph graph(256, 4);
    EXPECT_TRUE(graph.is_empty());
}

TEST(Unit_Dsp, RigGraphStaysEmptyAfterCompilingAGraphWithNoNode)
{
    SushiDSP::RigGraph graph(256, 4);
    const SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    EXPECT_TRUE(graph.is_empty());
}

TEST(Unit_Dsp, RigGraphIsNotEmptyOnceItHoldsACompiledNode)
{
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(SushiDSP::make_node("sushidsp.tube_screamer", SushiDSP::PrepareInfo{ 48000.0, 256, 1 }));
    const SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    EXPECT_FALSE(graph.is_empty());
}
```

If `test_rig_graph.cpp` does not already include `SushiDSP/node_registry/NodeRegistry.hpp`, add it.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `sd test -s unit 2>&1 | grep RigGraphIsEmpty`
Expected: the build fails; `RigGraph` has no `is_empty`.

- [ ] **Step 3: Declare the accessor and fix the stale comment**

In `include/SushiDSP/core/RigGraph.hpp`, directly below the `nodes()` declaration, add:

```cpp
        /// @brief Reports whether compile() produced an empty schedule, i.e. the graph holds no
        /// live node and process() would write nothing. Real-time safe: reads one vector's size
        /// and allocates nothing.
        /// @return True when process() would write nothing.
        bool is_empty() const noexcept;
```

Then replace the `latency_samples()` comment, which still describes the pre-DAG implementation:

```cpp
        /// @brief Longest path through the compiled graph, in base-rate samples: a node's latency
        /// plus the largest latency among its upstream paths, taken at the sink. Allocation-free;
        /// call after compile(), which caches the value.
        /// @return Total latency in base-rate samples.
        std::uint32_t latency_samples() const;
```

- [ ] **Step 4: Define the accessor**

In `src/core/RigGraph.cpp`, directly above `RigGraph::latency_samples()`:

```cpp
    bool RigGraph::is_empty() const noexcept
    {
        return topological_order_.empty();
    }
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `sd test -s unit 2>&1 | grep RigGraphIsEmpty`
Expected: three `Passed` lines.

Run: `sd test -s unit`
Expected: every test passes; no existing test changes behavior.

- [ ] **Step 6: Commit**

```bash
git add include/SushiDSP/core/RigGraph.hpp src/core/RigGraph.cpp tests/functional/unit/test_rig_graph.cpp
git commit -m "core: let the audio thread ask whether a graph is empty"
```

---

### Task 7: `RigController` bypass

**Files:**
- Modify: `apps/host_gui/RigController.hpp`, `apps/host_gui/RigController.cpp`
- Test: `tests/functional/unit/test_rig_controller.cpp`

**Interfaces:**
- Consumes: `SushiDSP::serialize_rig_to_json`, `SushiDSP::deserialize_rig_from_json`, `SushiDSP::AnnotateNodeFn`, `SushiDSP::OnNodeLoadedFn` from `SushiDSP/rig_preset/RigPreset.hpp`; `SushiDSP::RigGraph::kMaxFanInOut`.
- Produces: `void RigController::set_bypassed(SlotId, bool)` and `bool RigController::is_bypassed(SlotId) const`. Tasks 14, 15 and 16 call them.

- [ ] **Step 1: Write the failing tests**

Append to `tests/functional/unit/test_rig_controller.cpp`:

```cpp
TEST(Unit_Dsp, RigControllerBypassBridgesTheSlotsNeighbours)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    const RigController::SlotId first = controller.append("sushidsp.tube_screamer");
    const RigController::SlotId middle = controller.append("sushidsp.tube_screamer");
    const RigController::SlotId last = controller.append("sushidsp.jcm800");

    controller.set_bypassed(middle, true);

    EXPECT_TRUE(controller.is_bypassed(middle));
    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 2u);
    // The bypassed slot keeps its place in the rig; only the graph skips it.
    EXPECT_EQ(controller.slot_order().size(), 3u);
    EXPECT_EQ(controller.upstream_of(last).size(), 1u);
    EXPECT_EQ(controller.upstream_of(last)[0], middle);
    (void)first;
}

TEST(Unit_Dsp, RigControllerUnbypassRestoresTheOriginalGraph)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    controller.append("sushidsp.tube_screamer");
    const RigController::SlotId middle = controller.append("sushidsp.tube_screamer");
    controller.append("sushidsp.jcm800");

    controller.set_bypassed(middle, true);
    controller.set_bypassed(middle, false);

    EXPECT_FALSE(controller.is_bypassed(middle));
    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 3u);
}

TEST(Unit_Dsp, RigControllerBridgesTwoBypassedSlotsInARow)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    controller.append("sushidsp.tube_screamer");
    const RigController::SlotId second = controller.append("sushidsp.tube_screamer");
    const RigController::SlotId third = controller.append("sushidsp.tube_screamer");
    controller.append("sushidsp.jcm800");

    controller.set_bypassed(second, true);
    controller.set_bypassed(third, true);

    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 2u);
}

TEST(Unit_Dsp, RigControllerBypassingTheHeadSlotLeavesAWorkingGraph)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    const RigController::SlotId head = controller.append("sushidsp.tube_screamer");
    controller.append("sushidsp.jcm800");

    controller.set_bypassed(head, true);

    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 1u);
    EXPECT_FALSE(controller.active_graph()->is_empty());
}

TEST(Unit_Dsp, RigControllerBypassingEverySlotLeavesAnEmptyGraph)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    const RigController::SlotId only = controller.append("sushidsp.tube_screamer");

    controller.set_bypassed(only, true);

    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_TRUE(controller.active_graph()->is_empty());
}

TEST(Unit_Dsp, RigControllerSaveToJsonKeepsABypassedNodeAndFlagsIt)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    controller.append("sushidsp.tube_screamer");
    const RigController::SlotId middle = controller.append("sushidsp.tube_screamer");
    controller.append("sushidsp.jcm800");
    controller.set_bypassed(middle, true);

    const nlohmann::json parsed = nlohmann::json::parse(controller.save_to_json());

    ASSERT_EQ(parsed.at("nodes").size(), 3u);
    int flagged = 0;
    for (const auto& node : parsed.at("nodes"))
    {
        if (node.contains("bypassed") && node.at("bypassed").get<bool>())
        {
            ++flagged;
        }
    }
    EXPECT_EQ(flagged, 1);
}

TEST(Unit_Dsp, RigControllerLoadFromJsonRestoresTheBypassFlag)
{
    RigController source(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    source.append("sushidsp.tube_screamer");
    const RigController::SlotId middle = source.append("sushidsp.tube_screamer");
    source.append("sushidsp.jcm800");
    source.set_bypassed(middle, true);
    const std::string preset = source.save_to_json();

    RigController target(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    target.load_from_json(preset);

    const std::vector<RigController::SlotId> order = target.slot_order();
    ASSERT_EQ(order.size(), 3u);
    int bypassed = 0;
    for (RigController::SlotId slot : order)
    {
        if (target.is_bypassed(slot))
        {
            ++bypassed;
        }
    }
    EXPECT_EQ(bypassed, 1);
    ASSERT_NE(target.active_graph(), nullptr);
    EXPECT_EQ(target.active_graph()->nodes().size(), 2u);
}

TEST(Unit_Dsp, RigControllerLoadsAPresetWithNoBypassFieldWithEverySlotActive)
{
    const std::string preset = R"({"nodes":[
        {"id":0,"type":"sushidsp.tube_screamer","params":{},"inputs":[]},
        {"id":1,"type":"sushidsp.jcm800","params":{},"inputs":[0]}
    ]})";

    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    controller.load_from_json(preset);

    for (RigController::SlotId slot : controller.slot_order())
    {
        EXPECT_FALSE(controller.is_bypassed(slot));
    }
    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 2u);
}

TEST(Unit_Dsp, RigControllerIsBypassedIsFalseForASlotThatIsNotLive)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    EXPECT_FALSE(controller.is_bypassed(9999));
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `sd test -s unit 2>&1 | grep -i bypass`
Expected: the build fails; `RigController` has no `set_bypassed`.

- [ ] **Step 3: Declare the new members**

In `apps/host_gui/RigController.hpp`, after `publish()`:

```cpp
    /// @brief Marks a slot bypassed or active and republishes. A bypassed slot keeps its
    /// parameters, its cabinet path and its edges; the published graph simply leaves it out and
    /// connects its upstream neighbours straight to its downstream neighbours.
    /// @param slot The slot; a slot that is not live is ignored.
    /// @param bypassed True to bypass.
    /// @throws std::runtime_error if the bridged graph does not compile; the flag is left
    /// unchanged and the previous graph keeps playing.
    void set_bypassed(SlotId slot, bool bypassed);

    /// @brief Reports whether a slot is bypassed.
    /// @param slot The slot.
    /// @return True when the slot is live and bypassed.
    bool is_bypassed(SlotId slot) const;
```

Add to the private `Slot` struct:

```cpp
        bool                            bypassed = false;
```

Add the private helper declaration, next to `apply_cabinet_to_live_node`:

```cpp
    /// @brief Builds a fresh RigGraph from the current slots and edges and compiles it. Const: a
    /// slot whose descriptor_id the node registry does not know is skipped rather than erased,
    /// so handles_out names exactly the slots that reached the graph.
    /// @param apply_bypass True to leave bypassed slots out and bridge around them; false to add
    /// every live slot and honour every edge, which is the shape a preset records.
    /// @param handles_out Receives the slot-to-handle mapping for the graph that was built.
    /// @return The compiled graph.
    /// @throws std::runtime_error if bridging would exceed RigGraph::kMaxFanInOut, or if the
    /// graph does not compile.
    std::unique_ptr<SushiDSP::RigGraph> build_graph(bool apply_bypass,
        std::unordered_map<SlotId, SushiDSP::NodeHandle>& handles_out) const;
```

`RigController.hpp` needs `#include <memory>` if it does not have it.

- [ ] **Step 4: Write `build_graph` and rewrite `publish` on top of it**

In `apps/host_gui/RigController.cpp`, add `build_graph` and replace `publish()`'s graph-building
half with a call to it. `publish()` keeps its own pruning step and its own parameter seeding:

```cpp
std::unique_ptr<SushiDSP::RigGraph> RigController::build_graph(bool apply_bypass,
    std::unordered_map<SlotId, SushiDSP::NodeHandle>& handles_out) const
{
    handles_out.clear();

    auto graph = std::make_unique<SushiDSP::RigGraph>(
        info_.max_block, static_cast<std::uint32_t>(slot_order_.size() + 1));

    for (SlotId id : slot_order_)
    {
        const Slot& slot = slots_.at(id);
        if (apply_bypass && slot.bypassed)
        {
            continue;
        }
        auto node = SushiDSP::make_node(slot.descriptor_id.c_str(), info_);
        if (node == nullptr)
        {
            continue;
        }
        handles_out[id] = graph->add_node(std::move(node));
    }

    const std::vector<std::pair<SlotId, SlotId>> resolved =
        apply_bypass ? bridged_edges(handles_out) : direct_edges(handles_out);

    std::unordered_map<SlotId, std::size_t> fan_out;
    std::unordered_map<SlotId, std::size_t> fan_in;
    for (const auto& edge : resolved)
    {
        if (++fan_out[edge.first] > SushiDSP::RigGraph::kMaxFanInOut
            || ++fan_in[edge.second] > SushiDSP::RigGraph::kMaxFanInOut)
        {
            throw std::runtime_error(
                "RigController::build_graph: bridging a bypassed slot would exceed the graph's "
                "fan-in/fan-out limit");
        }
    }

    for (const auto& edge : resolved)
    {
        graph->connect(handles_out.at(edge.first), handles_out.at(edge.second));
    }

    graph->compile(info_);
    return graph;
}
```

Add the two private edge helpers to `RigController.cpp`'s anonymous-namespace-free member set,
declared in the header next to `build_graph`:

```cpp
    /// @brief Lists every edge whose endpoints both reached the graph, unchanged.
    /// @param live The slot-to-handle mapping for the graph being built.
    /// @return The edges, as (from, to) pairs.
    std::vector<std::pair<SlotId, SlotId>> direct_edges(
        const std::unordered_map<SlotId, SushiDSP::NodeHandle>& live) const;

    /// @brief Lists every edge with bypassed slots collapsed away: an edge into a bypassed slot
    /// is followed through to each live slot reachable past it, so a run of bypassed slots
    /// bridges in one step. Duplicate edges are dropped.
    /// @param live The slot-to-handle mapping for the graph being built.
    /// @return The bridged edges, as (from, to) pairs.
    std::vector<std::pair<SlotId, SlotId>> bridged_edges(
        const std::unordered_map<SlotId, SushiDSP::NodeHandle>& live) const;
```

```cpp
std::vector<std::pair<RigController::SlotId, RigController::SlotId>>
RigController::direct_edges(const std::unordered_map<SlotId, SushiDSP::NodeHandle>& live) const
{
    std::vector<std::pair<SlotId, SlotId>> result;
    for (const Edge& edge : edges_)
    {
        if (live.count(edge.from) != 0 && live.count(edge.to) != 0)
        {
            result.push_back({ edge.from, edge.to });
        }
    }
    return result;
}

std::vector<std::pair<RigController::SlotId, RigController::SlotId>>
RigController::bridged_edges(const std::unordered_map<SlotId, SushiDSP::NodeHandle>& live) const
{
    std::vector<std::pair<SlotId, SlotId>> result;
    std::set<std::pair<SlotId, SlotId>> seen;

    for (const Edge& edge : edges_)
    {
        if (live.count(edge.from) == 0)
        {
            continue;
        }

        // Walk forward from edge.to through bypassed slots until every live target is found.
        std::vector<SlotId> frontier{ edge.to };
        std::set<SlotId> visited;
        while (!frontier.empty())
        {
            const SlotId current = frontier.back();
            frontier.pop_back();
            if (!visited.insert(current).second)
            {
                continue;
            }
            if (live.count(current) != 0)
            {
                if (seen.insert({ edge.from, current }).second)
                {
                    result.push_back({ edge.from, current });
                }
                continue;
            }
            for (const Edge& next : edges_)
            {
                if (next.from == current)
                {
                    frontier.push_back(next.to);
                }
            }
        }
    }
    return result;
}
```

`RigController.cpp` needs `#include <set>` and `#include <utility>`.

`publish()` becomes:

```cpp
void RigController::publish()
{
    std::unordered_map<SlotId, SushiDSP::NodeHandle> fresh_handles;
    auto graph = build_graph(true, fresh_handles);

    // A slot the registry no longer knows is dropped here rather than inside build_graph, which
    // stays const so save_to_json() can call it too.
    std::vector<SlotId> surviving_order;
    surviving_order.reserve(slot_order_.size());
    for (SlotId id : slot_order_)
    {
        if (fresh_handles.count(id) != 0 || slots_.at(id).bypassed)
        {
            surviving_order.push_back(id);
        }
        else
        {
            slots_.erase(id);
        }
    }
    slot_order_ = std::move(surviving_order);

    edges_.erase(
        std::remove_if(edges_.begin(), edges_.end(),
            [this](const Edge& edge)
            { return slots_.count(edge.from) == 0 || slots_.count(edge.to) == 0; }),
        edges_.end());

    for (const auto& [id, handle] : fresh_handles)
    {
        SushiDSP::IAudioNode* node = graph->node(handle);
        if (node == nullptr)
        {
            continue;
        }
        for (const auto& [param_id, value] : slots_.at(id).parameters)
        {
            node->set_parameter(param_id.c_str(), value);
        }
    }

    slot_to_handle_ = fresh_handles;
    // The remainder of publish() - installing cabinets on fresh nodes and handing the graph to
    // graph_slot_ - stays exactly as it is today.
}
```

Keep every line of today's `publish()` that follows the parameter-seeding loop: the cabinet
installation pass, the `graph_slot_` publish, and the `active_graph_` assignment.

- [ ] **Step 5: Write `set_bypassed` and `is_bypassed`**

```cpp
void RigController::set_bypassed(SlotId slot, bool bypassed)
{
    const auto it = slots_.find(slot);
    if (it == slots_.end() || it->second.bypassed == bypassed)
    {
        return;
    }

    const bool previous = it->second.bypassed;
    it->second.bypassed = bypassed;
    try
    {
        publish();
    }
    catch (...)
    {
        it->second.bypassed = previous;
        throw;
    }
}

bool RigController::is_bypassed(SlotId slot) const
{
    const auto it = slots_.find(slot);
    return it != slots_.end() && it->second.bypassed;
}
```

- [ ] **Step 6: Route `save_to_json` through `build_graph` and add the flag**

```cpp
std::string RigController::save_to_json() const
{
    if (slot_order_.empty())
    {
        return "{\"nodes\":[]}";
    }

    std::unordered_map<SlotId, SushiDSP::NodeHandle> handles;
    const std::unique_ptr<SushiDSP::RigGraph> graph = build_graph(false, handles);

    std::unordered_map<SushiDSP::NodeHandle, SlotId> slot_by_handle;
    for (const auto& [slot, handle] : handles)
    {
        slot_by_handle[handle] = slot;
    }

    return SushiDSP::serialize_rig_to_json(*graph,
        [this, &slot_by_handle](SushiDSP::NodeHandle handle, nlohmann::json& node_json)
        {
            const auto it = slot_by_handle.find(handle);
            if (it == slot_by_handle.end())
            {
                return;
            }
            const Slot& slot = slots_.at(it->second);

            nlohmann::json params_json = nlohmann::json::object();
            for (const auto& [param_id, value] : slot.parameters)
            {
                params_json[param_id] = value;
            }
            node_json["params"] = params_json;

            if (!slot.cabinet_path.empty())
            {
                node_json["cabinet_ir"] = slot.cabinet_path;
            }
            if (slot.bypassed)
            {
                node_json["bypassed"] = true;
            }
        });
}
```

- [ ] **Step 7: Read the flag back in `load_from_json`**

Extend the existing `OnNodeLoadedFn` lambda, which already collects `cabinet_ir`, with a second
map, and apply it where the loader already applies the cabinet path:

```cpp
    std::unordered_map<SushiDSP::NodeHandle, bool> bypassed_by_handle;
```

Inside the lambda, next to the `cabinet_ir` branch:

```cpp
            if (node_json.contains("bypassed"))
            {
                bypassed_by_handle[handle] = node_json.at("bypassed").get<bool>();
            }
```

Inside the loop that builds each `Slot`, next to the cabinet-path assignment:

```cpp
        const auto bypass_it = bypassed_by_handle.find(handle);
        if (bypass_it != bypassed_by_handle.end())
        {
            slot.bypassed = bypass_it->second;
        }
```

- [ ] **Step 8: Run the tests to verify they pass**

Run: `sd test -s unit 2>&1 | grep -i "RigController"`
Expected: every `RigController*` test passes, the nine new ones included, and no existing one changed.

- [ ] **Step 9: Commit**

```bash
git add apps/host_gui/RigController.hpp apps/host_gui/RigController.cpp tests/functional/unit/test_rig_controller.cpp
git commit -m "host_gui: bypass a slot by bridging around it in the published graph"
```

---

### Task 8: Theme

No unit test: the deliverable is a palette and an `ImGuiStyle`, and asserting a constant equals
itself proves nothing. The gate is that `sd build` passes and the interface matches this plan;
the visual check happens in Task 16.

**Files:**
- Modify: `apps/host_gui/ui/Theme.hpp`, `apps/host_gui/ui/Theme.cpp`

**Interfaces:**
- Consumes: `ui::Color`, `ui::rgba()` from `ui/Geometry.hpp`; `imgui.h`.
- Produces: `ui::Palette`, `ui::palette()`, `ui::apply_theme()`, `ui::to_imu32()`. Tasks 9 to 16 all use `palette()` and `to_imu32()`.

- [ ] **Step 1: Write the header**

```cpp
#pragma once

#include <imgui.h>

#include "ui/Geometry.hpp"

namespace ui
{
    /// @brief Every color the GUI draws with.
    struct Palette
    {
        Color window_background;
        Color panel_background;
        Color accent;
        Color text;
        Color muted_text;
        Color meter_normal;
        Color meter_clipping;
    };

    /// @brief The palette, for drawing code that reaches ImDrawList directly.
    /// @return The palette.
    const Palette& palette();

    /// @brief Writes the palette, corner radii and spacing into ImGui's active style. Call once,
    /// after ImGui::CreateContext().
    void apply_theme();

    /// @brief Converts a Geometry Color to the packed form ImDrawList takes.
    /// @param color The 0xRRGGBBAA color.
    /// @return The ImU32 equivalent.
    ImU32 to_imu32(Color color);

    /// @brief Converts a Geometry Color to an ImVec4, for ImGui style slots.
    /// @param color The 0xRRGGBBAA color.
    /// @return The ImVec4 equivalent, each channel in [0, 1].
    ImVec4 to_imvec4(Color color);
}
```

- [ ] **Step 2: Write the implementation**

```cpp
#include "ui/Theme.hpp"

namespace ui
{
    const Palette& palette()
    {
        static const Palette colors{
            rgba(18, 18, 20),
            rgba(28, 28, 32),
            rgba(214, 168, 74),
            rgba(228, 228, 232),
            rgba(140, 140, 148),
            rgba(96, 200, 112),
            rgba(220, 72, 60),
        };
        return colors;
    }

    ImU32 to_imu32(Color color)
    {
        const ImU32 r = (color >> 24) & 0xFFu;
        const ImU32 g = (color >> 16) & 0xFFu;
        const ImU32 b = (color >> 8) & 0xFFu;
        const ImU32 a = color & 0xFFu;
        return IM_COL32(r, g, b, a);
    }

    ImVec4 to_imvec4(Color color)
    {
        return ImVec4(
            static_cast<float>((color >> 24) & 0xFFu) / 255.0f,
            static_cast<float>((color >> 16) & 0xFFu) / 255.0f,
            static_cast<float>((color >> 8) & 0xFFu) / 255.0f,
            static_cast<float>(color & 0xFFu) / 255.0f);
    }

    void apply_theme()
    {
        const Palette& colors = palette();
        ImGuiStyle& style = ImGui::GetStyle();

        style.WindowRounding = 0.0f;
        style.FrameRounding = 3.0f;
        style.GrabRounding = 3.0f;
        style.WindowBorderSize = 0.0f;
        style.FrameBorderSize = 1.0f;
        style.WindowPadding = ImVec2(12.0f, 10.0f);
        style.FramePadding = ImVec2(8.0f, 5.0f);
        style.ItemSpacing = ImVec2(8.0f, 7.0f);
        style.ScrollbarSize = 12.0f;

        style.Colors[ImGuiCol_WindowBg] = to_imvec4(colors.window_background);
        style.Colors[ImGuiCol_ChildBg] = to_imvec4(colors.panel_background);
        style.Colors[ImGuiCol_PopupBg] = to_imvec4(colors.panel_background);
        style.Colors[ImGuiCol_Text] = to_imvec4(colors.text);
        style.Colors[ImGuiCol_TextDisabled] = to_imvec4(colors.muted_text);
        style.Colors[ImGuiCol_FrameBg] = to_imvec4(colors.panel_background);
        style.Colors[ImGuiCol_FrameBgHovered] = to_imvec4(rgba(44, 44, 50));
        style.Colors[ImGuiCol_FrameBgActive] = to_imvec4(rgba(56, 56, 62));
        style.Colors[ImGuiCol_Border] = to_imvec4(rgba(58, 58, 64));
        style.Colors[ImGuiCol_Button] = to_imvec4(rgba(44, 44, 50));
        style.Colors[ImGuiCol_ButtonHovered] = to_imvec4(rgba(60, 60, 68));
        style.Colors[ImGuiCol_ButtonActive] = to_imvec4(colors.accent);
        style.Colors[ImGuiCol_Header] = to_imvec4(rgba(44, 44, 50));
        style.Colors[ImGuiCol_HeaderHovered] = to_imvec4(rgba(60, 60, 68));
        style.Colors[ImGuiCol_HeaderActive] = to_imvec4(colors.accent);
        style.Colors[ImGuiCol_Separator] = to_imvec4(rgba(48, 48, 54));
    }
}
```

- [ ] **Step 3: Build**

Run: `sd build`
Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add apps/host_gui/ui/Theme.hpp apps/host_gui/ui/Theme.cpp
git commit -m "host_gui: give the GUI one dark palette and style"
```

---

### Task 9: Knob and switch widgets

No unit test; the math they call is tested in Task 2. Gate: `sd build` passes.

**Files:**
- Modify: `apps/host_gui/ui/Widgets.hpp`, `apps/host_gui/ui/Widgets.cpp`

**Interfaces:**
- Consumes: `ui::value_to_normalized`, `ui::normalized_to_value`, `ui::normalized_to_angle`, `ui::apply_drag`, `ui::kKnobSweepRadians`, `ui::kKnobStartRadians` from Task 2; `ui::palette()`, `ui::to_imu32()` from Task 8.
- Produces: `ui::Knob()` and `ui::ToggleSwitch()`. Tasks 14 and 15 call them.

- [ ] **Step 1: Write the header**

```cpp
#pragma once

#include <imgui.h>

#include "SushiDSP/core/NodeDescriptor.hpp"

namespace ui
{
    /// @brief Draws a turnable knob for one parameter and applies pointer edits to *value. A
    /// vertical drag turns it, Shift makes the drag four times finer, and a double click restores
    /// the parameter's default.
    /// @param id The ImGui id, unique within the window.
    /// @param param The parameter, whose range and kind set the taper and the tooltip's unit.
    /// @param value In/out: the current value.
    /// @param diameter The knob's diameter in pixels.
    /// @return True on the frames where *value changed.
    bool Knob(const char* id, const SushiDSP::ParameterDescriptor& param, float* value, float diameter);

    /// @brief Draws a two-state rocker switch.
    /// @param id The ImGui id, unique within the window.
    /// @param on In/out: the current state.
    /// @param width The switch's width in pixels.
    /// @param height The switch's height in pixels.
    /// @return True on the frames where *on changed.
    bool ToggleSwitch(const char* id, bool* on, float width, float height);
}
```

- [ ] **Step 2: Write the implementation**

```cpp
#include "ui/Widgets.hpp"

#include <cmath>
#include <cstdio>

#include "ui/KnobMath.hpp"
#include "ui/Theme.hpp"

namespace ui
{
    namespace
    {
        /// @brief Tick marks drawn around a knob's sweep.
        constexpr int kKnobTickCount = 11;
    }

    bool Knob(const char* id, const SushiDSP::ParameterDescriptor& param, float* value, float diameter)
    {
        ImGui::PushID(id);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("knob", ImVec2(diameter, diameter));
        const bool active = ImGui::IsItemActive();
        const ImVec2 center(origin.x + diameter * 0.5f, origin.y + diameter * 0.5f);
        const float radius = diameter * 0.5f;

        bool changed = false;
        float normalized = value_to_normalized(param, *value);

        if (active)
        {
            const float delta_y = ImGui::GetIO().MouseDelta.y;
            if (delta_y != 0.0f)
            {
                normalized = apply_drag(normalized, delta_y, ImGui::GetIO().KeyShift);
                *value = normalized_to_value(param, normalized);
                changed = true;
            }
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            *value = param.default_value;
            normalized = value_to_normalized(param, *value);
            changed = true;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s: %.2f %s", param.label, *value, param.unit);
        }

        const Palette& colors = palette();
        ImDrawList* draw = ImGui::GetWindowDrawList();

        draw->AddCircleFilled(center, radius, to_imu32(rgba(20, 20, 24)), 48);
        draw->AddCircleFilled(center, radius * 0.86f, to_imu32(rgba(52, 52, 58)), 48);
        draw->AddCircle(center, radius * 0.86f, to_imu32(rgba(78, 78, 86)), 48, 1.5f);

        for (int tick = 0; tick < kKnobTickCount; ++tick)
        {
            const float position = static_cast<float>(tick) / static_cast<float>(kKnobTickCount - 1);
            const float angle = normalized_to_angle(position);
            const ImVec2 inner(center.x + std::cos(angle) * radius * 0.95f,
                               center.y + std::sin(angle) * radius * 0.95f);
            const ImVec2 outer(center.x + std::cos(angle) * radius * 1.12f,
                               center.y + std::sin(angle) * radius * 1.12f);
            draw->AddLine(inner, outer, to_imu32(colors.muted_text), 1.0f);
        }

        draw->PathArcTo(center, radius * 1.02f, kKnobStartRadians,
                        normalized_to_angle(normalized), 48);
        draw->PathStroke(to_imu32(colors.accent), 0, 3.0f);

        const float pointer_angle = normalized_to_angle(normalized);
        draw->AddLine(center,
                      ImVec2(center.x + std::cos(pointer_angle) * radius * 0.78f,
                             center.y + std::sin(pointer_angle) * radius * 0.78f),
                      to_imu32(colors.text), 2.5f);

        ImGui::PopID();
        return changed;
    }

    bool ToggleSwitch(const char* id, bool* on, float width, float height)
    {
        ImGui::PushID(id);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("switch", ImVec2(width, height));

        bool changed = false;
        if (ImGui::IsItemClicked())
        {
            *on = !*on;
            changed = true;
        }

        const Palette& colors = palette();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float radius = height * 0.5f;
        const ImVec2 bottom_right(origin.x + width, origin.y + height);

        draw->AddRectFilled(origin, bottom_right,
                            to_imu32(*on ? colors.accent : rgba(52, 52, 58)), radius);
        const float knob_x = *on ? bottom_right.x - radius : origin.x + radius;
        draw->AddCircleFilled(ImVec2(knob_x, origin.y + radius), radius * 0.78f,
                              to_imu32(rgba(230, 230, 234)), 24);

        ImGui::PopID();
        return changed;
    }
}
```

- [ ] **Step 3: Build**

Run: `sd build`
Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add apps/host_gui/ui/Widgets.hpp apps/host_gui/ui/Widgets.cpp
git commit -m "host_gui: draw a turnable knob and a rocker switch"
```

---

### Task 10: Gear browser

No unit test. Gate: `sd build` passes.

**Files:**
- Modify: `apps/host_gui/ui/GearBrowser.hpp`, `apps/host_gui/ui/GearBrowser.cpp`

**Interfaces:**
- Consumes: `SushiDSP::node_registry()`; `ui::palette()`, `ui::to_imu32()` from Task 8.
- Produces: `void ui::DrawGearBrowser()`. Task 16 calls it.

- [ ] **Step 1: Write the header**

```cpp
#pragma once

namespace ui
{
    /// @brief Draws the gear browser's search box, category headers and cards into the current
    /// window. Each card is a drag source carrying the GEAR_DESCRIPTOR_ID payload the chain strip
    /// accepts.
    void DrawGearBrowser();
}
```

- [ ] **Step 2: Write the implementation**

```cpp
#include "ui/GearBrowser.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

#include <imgui.h>

#include "SushiDSP/node_registry/NodeRegistry.hpp"

#include "ui/Theme.hpp"

namespace ui
{
    namespace
    {
        /// @brief Reports whether needle occurs in haystack, ignoring case.
        bool contains_ignoring_case(const char* haystack, const std::string& needle)
        {
            if (needle.empty())
            {
                return true;
            }
            std::string lowered(haystack);
            std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::string wanted = needle;
            std::transform(wanted.begin(), wanted.end(), wanted.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return lowered.find(wanted) != std::string::npos;
        }

        /// @brief Draws a card's icon: a filled rounded rectangle tinted by category.
        void draw_card_icon(ImVec2 top_left, float size, const char* category)
        {
            const ImU32 tint = std::strcmp(category, "Amp") == 0
                ? to_imu32(rgba(158, 124, 58))
                : std::strcmp(category, "Distortion") == 0
                    ? to_imu32(rgba(46, 112, 58))
                    : to_imu32(rgba(70, 84, 120));
            ImGui::GetWindowDrawList()->AddRectFilled(
                top_left, ImVec2(top_left.x + size, top_left.y + size), tint, 6.0f);
        }
    }

    void DrawGearBrowser()
    {
        static char search[128] = "";
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##gear_search", "Search gear", search, sizeof(search));
        ImGui::Separator();

        const std::string needle(search);
        const char* current_category = nullptr;
        bool category_open = false;

        for (const SushiDSP::NodeEntry& entry : SushiDSP::node_registry())
        {
            if (current_category == nullptr
                || std::strcmp(current_category, entry.category.name) != 0)
            {
                current_category = entry.category.name;
                category_open = ImGui::CollapsingHeader(current_category, ImGuiTreeNodeFlags_DefaultOpen);
            }
            if (!category_open || !contains_ignoring_case(entry.descriptor->name, needle))
            {
                continue;
            }

            ImGui::PushID(entry.descriptor->id);
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("card", ImVec2(ImGui::GetContentRegionAvail().x, 44.0f));

            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(origin,
                                ImVec2(origin.x + ImGui::GetItemRectSize().x, origin.y + 44.0f),
                                to_imu32(ImGui::IsItemHovered() ? rgba(48, 48, 54) : rgba(34, 34, 39)),
                                5.0f);
            draw_card_icon(ImVec2(origin.x + 6.0f, origin.y + 6.0f), 32.0f, entry.category.name);
            draw->AddText(ImVec2(origin.x + 48.0f, origin.y + 15.0f),
                          to_imu32(palette().text), entry.descriptor->name);

            if (ImGui::BeginDragDropSource())
            {
                ImGui::SetDragDropPayload("GEAR_DESCRIPTOR_ID", entry.descriptor->id,
                                          std::strlen(entry.descriptor->id) + 1);
                ImGui::Text("%s", entry.descriptor->name);
                ImGui::EndDragDropSource();
            }
            ImGui::PopID();
        }
    }
}
```

- [ ] **Step 3: Build**

Run: `sd build`
Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add apps/host_gui/ui/GearBrowser.hpp apps/host_gui/ui/GearBrowser.cpp
git commit -m "host_gui: browse gear by category with a search filter"
```

---

### Task 11: Meter bar

No unit test; `LevelProbe` is tested in Task 5. Gate: `sd build` passes.

**Files:**
- Modify: `apps/host_gui/ui/MeterBar.hpp`, `apps/host_gui/ui/MeterBar.cpp`

**Interfaces:**
- Consumes: `ui::LevelProbe` from Task 5; `ui::palette()`, `ui::to_imu32()` from Task 8.
- Produces: `void ui::DrawMeterBar(LevelProbe&, LevelProbe&)`. Task 16 calls it.

- [ ] **Step 1: Write the header**

```cpp
#pragma once

#include "ui/LevelProbe.hpp"

namespace ui
{
    /// @brief Draws the input and output level bars into the current window, reading and resetting
    /// both probes. Owns the ballistics: the displayed level jumps to a higher peak at once and
    /// falls by 15 percent per frame toward a lower one.
    /// @param input The probe fed from the device's input block.
    /// @param output The probe fed from the rig's output block.
    void DrawMeterBar(LevelProbe& input, LevelProbe& output);
}
```

- [ ] **Step 2: Write the implementation**

```cpp
#include "ui/MeterBar.hpp"

#include <algorithm>
#include <cmath>

#include <imgui.h>

#include "ui/Theme.hpp"

namespace ui
{
    namespace
    {
        /// @brief Fraction of the displayed level kept each frame while the signal is falling.
        constexpr float kFallPerFrame = 0.85f;

        /// @brief Level, in dBFS, that the left end of a bar represents.
        constexpr float kFloorDecibels = -60.0f;

        /// @brief Converts a magnitude to dBFS, flooring silence at kFloorDecibels.
        float to_decibels(float magnitude)
        {
            return magnitude <= 0.0f ? kFloorDecibels
                                     : std::max(kFloorDecibels, 20.0f * std::log10(magnitude));
        }

        /// @brief Draws one labelled bar and returns the level it displayed.
        void draw_bar(const char* label, float displayed, float width)
        {
            const Palette& colors = palette();
            const float decibels = to_decibels(displayed);
            const float fill = (decibels - kFloorDecibels) / (0.0f - kFloorDecibels);

            ImGui::TextUnformatted(label);
            ImGui::SameLine();

            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const float height = 14.0f;
            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height),
                                to_imu32(rgba(24, 24, 28)), 3.0f);
            draw->AddRectFilled(origin,
                                ImVec2(origin.x + width * std::clamp(fill, 0.0f, 1.0f), origin.y + height),
                                to_imu32(displayed >= 1.0f ? colors.meter_clipping : colors.meter_normal),
                                3.0f);
            ImGui::Dummy(ImVec2(width, height));
            ImGui::SameLine();
            ImGui::Text("%.1f dB", decibels);
        }
    }

    void DrawMeterBar(LevelProbe& input, LevelProbe& output)
    {
        static float displayed_input = 0.0f;
        static float displayed_output = 0.0f;

        const float input_peak = input.read_peak();
        const float output_peak = output.read_peak();
        displayed_input = std::max(input_peak, displayed_input * kFallPerFrame);
        displayed_output = std::max(output_peak, displayed_output * kFallPerFrame);

        const float bar_width = std::max(80.0f, ImGui::GetContentRegionAvail().x * 0.3f);
        draw_bar("In", displayed_input, bar_width);
        ImGui::SameLine();
        draw_bar("Out", displayed_output, bar_width);
    }
}
```

- [ ] **Step 3: Build**

Run: `sd build`
Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add apps/host_gui/ui/MeterBar.hpp apps/host_gui/ui/MeterBar.cpp
git commit -m "host_gui: show input and output levels along the bottom"
```

---

### Task 12: Top bar

No unit test. Gate: `sd build` passes.

**Files:**
- Modify: `apps/host_gui/ui/TopBar.hpp`, `apps/host_gui/ui/TopBar.cpp`

**Interfaces:**
- Consumes: `RigController::save_to_json()`, `RigController::load_from_json()`; `ui::palette()` from Task 8.
- Produces: `void ui::DrawTopBar(RigController&, const std::string&, bool&, std::string&)`. Task 16 calls it.

- [ ] **Step 1: Write the header**

```cpp
#pragma once

#include <string>

#include "RigController.hpp"

namespace ui
{
    /// @brief Draws the preset path box, Save, Load, the connected-device label and the settings
    /// button into the current window.
    /// @param rig The rig to save from and load into.
    /// @param device_label Text naming the connected device and its channel selection.
    /// @param open_device_settings Out: set to true on the frame the settings button is clicked.
    /// @param status Out: a message describing the last Save or Load, empty when neither has run.
    void DrawTopBar(RigController& rig, const std::string& device_label,
                    bool& open_device_settings, std::string& status);
}
```

- [ ] **Step 2: Write the implementation**

```cpp
#include "ui/TopBar.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>

#include <imgui.h>

#include "ui/Theme.hpp"

namespace ui
{
    void DrawTopBar(RigController& rig, const std::string& device_label,
                    bool& open_device_settings, std::string& status)
    {
        static char preset_path[256] = "preset.json";

        ImGui::SetNextItemWidth(280.0f);
        ImGui::InputText("##preset_path", preset_path, sizeof(preset_path));
        ImGui::SameLine();

        if (ImGui::Button("Save"))
        {
            try
            {
                std::ofstream out(preset_path);
                if (!out)
                {
                    throw std::runtime_error("could not open '" + std::string(preset_path)
                                             + "' for writing");
                }
                out << rig.save_to_json();
                status = std::string("Saved ") + preset_path;
            }
            catch (const std::exception& error)
            {
                status = std::string("Save failed: ") + error.what();
            }
        }
        ImGui::SameLine();

        if (ImGui::Button("Load"))
        {
            try
            {
                std::ifstream in(preset_path);
                if (!in)
                {
                    throw std::runtime_error("could not open '" + std::string(preset_path)
                                             + "' for reading");
                }
                const std::string json_text((std::istreambuf_iterator<char>(in)),
                                            std::istreambuf_iterator<char>());
                rig.load_from_json(json_text);
                status = std::string("Loaded ") + preset_path;
            }
            catch (const std::exception& error)
            {
                status = std::string("Load failed: ") + error.what();
            }
        }

        ImGui::SameLine();
        ImGui::TextColored(to_imvec4(palette().muted_text), "%s", device_label.c_str());

        ImGui::SameLine(ImGui::GetWindowWidth() - 96.0f);
        if (ImGui::Button("Settings"))
        {
            open_device_settings = true;
        }

        if (!status.empty())
        {
            ImGui::SameLine();
            ImGui::TextColored(to_imvec4(palette().muted_text), "%s", status.c_str());
        }
    }
}
```

- [ ] **Step 3: Build**

Run: `sd build`
Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add apps/host_gui/ui/TopBar.hpp apps/host_gui/ui/TopBar.cpp
git commit -m "host_gui: move preset save and load into a top bar"
```

---

### Task 13: Device settings window

No unit test. Gate: `sd build` passes.

**Files:**
- Modify: `apps/host_gui/ui/DeviceSettings.hpp`, `apps/host_gui/ui/DeviceSettings.cpp`

**Interfaces:**
- Consumes: `SushiDSP::ChannelSelection`, `SushiDSP::channel_selection_label` from `SushiDSP/device/ChannelSelection.hpp`; `ui::palette()` from Task 8.
- Produces: `ui::DeviceChoice` and `bool ui::DrawDeviceSettings(...)`. Task 16 calls it and owns the actual device construction, which stays in `main.cpp` where the callback's captured state lives.

- [ ] **Step 1: Write the header**

```cpp
#pragma once

#include <string>
#include <vector>

#include "SushiDSP/device/ChannelSelection.hpp"

namespace ui
{
    /// @brief What the settings window's controls currently select.
    struct DeviceChoice
    {
        int                          device_index;   ///< 0 is SDL2; 1..N index asio_drivers.
        SushiDSP::ChannelSelection   input;
        SushiDSP::ChannelSelection   output;
    };

    /// @brief Draws the device and channel pickers as a modal window.
    /// @param asio_drivers Every ASIO driver name, or empty when ASIO is not built in.
    /// @param choice In/out: the current selection.
    /// @param already_running True when a rig is playing, which the window warns will be replaced.
    /// @param error Text from the last failed connection attempt, or empty.
    /// @param open In/out: false closes the window; Connect and Close both clear it.
    /// @return True on the frame Connect is clicked, meaning the caller should open the device.
    bool DrawDeviceSettings(const std::vector<std::string>& asio_drivers, DeviceChoice& choice,
                            bool already_running, const std::string& error, bool& open);
}
```

- [ ] **Step 2: Write the implementation**

```cpp
#include "ui/DeviceSettings.hpp"

#include <cstddef>

#include <imgui.h>

#include "ui/Theme.hpp"

namespace ui
{
    bool DrawDeviceSettings(const std::vector<std::string>& asio_drivers, DeviceChoice& choice,
                            bool already_running, const std::string& error, bool& open)
    {
        if (!open)
        {
            return false;
        }

        bool connect_clicked = false;
        ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);
        if (ImGui::Begin("Audio device", &open, ImGuiWindowFlags_NoCollapse))
        {
            ImGui::TextUnformatted("Device");
            if (ImGui::RadioButton("SDL2 (no input)", choice.device_index == 0))
            {
                choice.device_index = 0;
            }
            for (std::size_t i = 0; i < asio_drivers.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::RadioButton(asio_drivers[i].c_str(),
                                       choice.device_index == static_cast<int>(i) + 1))
                {
                    choice.device_index = static_cast<int>(i) + 1;
                }
                ImGui::PopID();
            }

            if (choice.device_index > 0)
            {
                ImGui::Separator();
                ImGui::TextUnformatted("Input");
                if (ImGui::RadioButton("L##input", choice.input == SushiDSP::ChannelSelection::Left))
                {
                    choice.input = SushiDSP::ChannelSelection::Left;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("R##input", choice.input == SushiDSP::ChannelSelection::Right))
                {
                    choice.input = SushiDSP::ChannelSelection::Right;
                }

                ImGui::TextUnformatted("Output");
                if (ImGui::RadioButton("L##output", choice.output == SushiDSP::ChannelSelection::Left))
                {
                    choice.output = SushiDSP::ChannelSelection::Left;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("R##output", choice.output == SushiDSP::ChannelSelection::Right))
                {
                    choice.output = SushiDSP::ChannelSelection::Right;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("stereo##output", choice.output == SushiDSP::ChannelSelection::Stereo))
                {
                    choice.output = SushiDSP::ChannelSelection::Stereo;
                }
            }

            ImGui::Separator();
            if (already_running)
            {
                ImGui::TextColored(to_imvec4(rgba(214, 168, 74)),
                    "Connecting replaces the current rig with an empty one.");
            }
            if (!error.empty())
            {
                ImGui::TextColored(to_imvec4(rgba(220, 72, 60)), "%s", error.c_str());
            }

            if (ImGui::Button("Connect"))
            {
                connect_clicked = true;
            }
            if (already_running)
            {
                ImGui::SameLine();
                if (ImGui::Button("Close"))
                {
                    open = false;
                }
            }
        }
        ImGui::End();

        return connect_clicked;
    }
}
```

- [ ] **Step 3: Build**

Run: `sd build`
Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add apps/host_gui/ui/DeviceSettings.hpp apps/host_gui/ui/DeviceSettings.cpp
git commit -m "host_gui: make device selection reachable while running"
```

---

### Task 14: Stage panel

No unit test; `PanelLibrary` and `KnobMath` are tested in Tasks 2 and 3. Gate: `sd build` passes.

**Files:**
- Modify: `apps/host_gui/ui/StagePanel.hpp`, `apps/host_gui/ui/StagePanel.cpp`

**Interfaces:**
- Consumes: `ui::panel_for`, `ui::auto_panel`, `ui::PanelDescriptor`, `ui::ControlKind` from Task 3; `ui::Knob`, `ui::ToggleSwitch` from Task 9; `ui::palette()`, `ui::to_imu32()` from Task 8; `RigController::get_parameter`, `set_parameter`, `descriptor_id`, `upstream_of`, `cabinet_path`, `set_cabinet`.
- Produces: `void ui::DrawStagePanel(RigController&, std::optional<RigController::SlotId>)`. Task 16 calls it.

- [ ] **Step 1: Write the header**

```cpp
#pragma once

#include <optional>

#include "RigController.hpp"

namespace ui
{
    /// @brief Draws the selected slot's front panel into the current window and writes knob edits
    /// back through the controller. Draws a hint and nothing else when no slot is selected.
    /// @param rig The rig that owns the slot.
    /// @param selected The slot to draw.
    void DrawStagePanel(RigController& rig, std::optional<RigController::SlotId> selected);
}
```

- [ ] **Step 2: Write the implementation**

```cpp
#include "ui/StagePanel.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <imgui.h>

#include "SushiDSP/node_registry/NodeRegistry.hpp"

#include "ui/PanelLibrary.hpp"
#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"

namespace ui
{
    namespace
    {
        /// @brief Finds a node type's registry entry.
        const SushiDSP::NodeDescriptor* descriptor_for(const std::string& id)
        {
            for (const SushiDSP::NodeEntry& entry : SushiDSP::node_registry())
            {
                if (id == entry.descriptor->id)
                {
                    return entry.descriptor;
                }
            }
            return nullptr;
        }

        /// @brief Finds one parameter by id within a node type.
        const SushiDSP::ParameterDescriptor* parameter_for(const SushiDSP::NodeDescriptor& node,
                                                           const char* param_id)
        {
            for (std::size_t p = 0; p < node.parameter_count; ++p)
            {
                if (std::strcmp(node.parameters[p].id, param_id) == 0)
                {
                    return &node.parameters[p];
                }
            }
            return nullptr;
        }

        /// @brief Fits a panel's aspect inside a region, centred.
        ImVec2 fit_panel(Vec2 aspect, ImVec2 region, ImVec2 origin, ImVec2& size_out)
        {
            const float scale = std::min(region.x / aspect.x, region.y / aspect.y);
            size_out = ImVec2(aspect.x * scale, aspect.y * scale);
            return ImVec2(origin.x + (region.x - size_out.x) * 0.5f,
                          origin.y + (region.y - size_out.y) * 0.5f);
        }

        /// @brief Draws the JCM800 cabinet field below the panel.
        void draw_cabinet_field(RigController& rig, RigController::SlotId slot)
        {
            static char cabinet_input[512] = "";
            static std::optional<RigController::SlotId> field_slot;
            static std::string cabinet_error;

            const std::string cabinet = rig.cabinet_path(slot);
            if (field_slot != std::optional<RigController::SlotId>(slot))
            {
                field_slot = slot;
                std::snprintf(cabinet_input, sizeof(cabinet_input), "%s", cabinet.c_str());
                cabinet_error.clear();
            }

            ImGui::Separator();
            ImGui::Text("Cabinet: %s", cabinet.empty() ? "(none)" : cabinet.c_str());
            ImGui::SetNextItemWidth(360.0f);
            ImGui::InputText("##cabinet_ir", cabinet_input, sizeof(cabinet_input));
            ImGui::SameLine();
            if (ImGui::Button("Load##cabinet"))
            {
                cabinet_error.clear();
                try
                {
                    if (!rig.set_cabinet(slot, cabinet_input))
                    {
                        cabinet_error = "Cabinet not installed: sample rate mismatch or unprepared amp";
                    }
                }
                catch (const std::exception& error)
                {
                    cabinet_error = error.what();
                }
            }
            if (!cabinet_error.empty())
            {
                ImGui::TextColored(to_imvec4(rgba(220, 72, 60)), "%s", cabinet_error.c_str());
            }
        }
    }

    void DrawStagePanel(RigController& rig, std::optional<RigController::SlotId> selected)
    {
        if (!selected.has_value())
        {
            ImGui::TextColored(to_imvec4(palette().muted_text),
                "Drag gear from the browser onto the chain, then click it to edit.");
            return;
        }

        const RigController::SlotId slot = *selected;
        const std::string id = rig.descriptor_id(slot);
        const SushiDSP::NodeDescriptor* node = descriptor_for(id);
        if (node == nullptr)
        {
            ImGui::TextColored(to_imvec4(rgba(220, 72, 60)), "Unknown node type '%s'", id.c_str());
            return;
        }

        std::vector<ControlPlacement> generated;
        const PanelDescriptor* table_entry = panel_for(id.c_str());
        const PanelDescriptor panel = table_entry != nullptr ? *table_entry
                                                             : auto_panel(*node, generated);

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 region = ImGui::GetContentRegionAvail();
        const ImVec2 panel_region(region.x, std::max(80.0f, region.y - 90.0f));
        ImVec2 panel_size;
        const ImVec2 panel_origin = fit_panel(panel.aspect, panel_region, origin, panel_size);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(panel_origin,
                            ImVec2(panel_origin.x + panel_size.x, panel_origin.y + panel_size.y),
                            to_imu32(panel.skin.body), 8.0f);
        draw->AddRectFilled(ImVec2(panel_origin.x + panel_size.x * 0.04f,
                                   panel_origin.y + panel_size.y * 0.58f),
                            ImVec2(panel_origin.x + panel_size.x * 0.96f,
                                   panel_origin.y + panel_size.y * 0.95f),
                            to_imu32(panel.skin.face), 4.0f);
        draw->AddText(ImVec2(panel_origin.x + panel_size.x * 0.06f,
                             panel_origin.y + panel_size.y * 0.10f),
                      to_imu32(panel.skin.accent), node->name);

        const std::size_t upstream_count = rig.upstream_of(slot).size();
        for (std::size_t c = 0; c < panel.control_count; ++c)
        {
            const ControlPlacement& placement = panel.controls[c];
            if (panel.is_control_visible != nullptr && !panel.is_control_visible(c, upstream_count))
            {
                continue;
            }
            const SushiDSP::ParameterDescriptor* param = parameter_for(*node, placement.param_id);
            if (param == nullptr)
            {
                continue;
            }

            const float diameter = placement.size * panel_size.y;
            const ImVec2 center(panel_origin.x + placement.position.x * panel_size.x,
                                panel_origin.y + placement.position.y * panel_size.y);
            ImGui::SetCursorScreenPos(ImVec2(center.x - diameter * 0.5f, center.y - diameter * 0.5f));

            float value = rig.get_parameter(slot, param->id);
            bool changed = false;
            if (placement.kind == ControlKind::Switch)
            {
                bool on = value >= 0.5f;
                changed = ToggleSwitch(param->id, &on, diameter, diameter * 0.5f);
                value = on ? 1.0f : 0.0f;
            }
            else
            {
                changed = Knob(param->id, *param, &value, diameter);
            }
            if (changed)
            {
                rig.set_parameter(slot, param->id, value);
            }

            const char* caption = placement.caption != nullptr ? placement.caption : param->label;
            const ImVec2 text_size = ImGui::CalcTextSize(caption);
            draw->AddText(ImVec2(center.x - text_size.x * 0.5f, center.y + diameter * 0.62f),
                          to_imu32(panel.skin.text), caption);
        }

        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + panel_region.y + 8.0f));
        if (id == "sushidsp.jcm800")
        {
            draw_cabinet_field(rig, slot);
        }
        if (rig.dropped_parameter_changes() > 0)
        {
            ImGui::TextColored(to_imvec4(rgba(255, 178, 76)), "Dropped parameter changes: %zu",
                               rig.dropped_parameter_changes());
        }
    }
}
```

- [ ] **Step 3: Build**

Run: `sd build`
Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add apps/host_gui/ui/StagePanel.hpp apps/host_gui/ui/StagePanel.cpp
git commit -m "host_gui: draw the selected node as its own front panel"
```

---

### Task 15: Chain strip

No unit test; `ChainLayout` is tested in Task 4. Gate: `sd build` passes.

**Files:**
- Modify: `apps/host_gui/ui/ChainStrip.hpp`, `apps/host_gui/ui/ChainStrip.cpp`

**Interfaces:**
- Consumes: `ui::layout_chain`, `ui::ChainView` from Task 4; `ui::ToggleSwitch` from Task 9; `ui::palette()`, `ui::to_imu32()` from Task 8; `RigController::slot_order`, `descriptor_id`, `upstream_of`, `is_bypassed`, `set_bypassed`, `move_before`, `insert_before`, `append`, `remove`; `slot_selection::still_valid` from `SlotSelection.hpp`.
- Produces: `void ui::DrawChainStrip(RigController&, std::optional<RigController::SlotId>&, std::optional<RigController::SlotId>&)`. Task 16 calls it.

- [ ] **Step 1: Write the header**

```cpp
#pragma once

#include <optional>

#include "RigController.hpp"

namespace ui
{
    /// @brief Draws the chain strip into the current window and handles selection, drag-reorder,
    /// gear drops, bypass toggles and removal.
    /// @param rig The rig to draw and edit.
    /// @param selected In/out: the selected slot, cleared when that slot stops existing.
    /// @param created_slot Out: the slot a gear drop just created, or std::nullopt when this frame
    /// created none. The caller uses it to run its own post-insert step.
    void DrawChainStrip(RigController& rig, std::optional<RigController::SlotId>& selected,
                        std::optional<RigController::SlotId>& created_slot);
}
```

- [ ] **Step 2: Write the implementation**

```cpp
#include "ui/ChainStrip.hpp"

#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include "SlotSelection.hpp"
#include "ui/ChainLayout.hpp"
#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"

namespace ui
{
    namespace
    {
        constexpr float kCardWidth = 132.0f;
        constexpr float kCardHeight = 78.0f;
        constexpr float kColumnGap = 46.0f;
        constexpr float kRowGap = 22.0f;

        /// @brief Collects the rig's edges as the pairs layout_chain takes.
        std::vector<std::pair<RigController::SlotId, RigController::SlotId>> collect_edges(
            const RigController& rig, const std::vector<RigController::SlotId>& slots)
        {
            std::vector<std::pair<RigController::SlotId, RigController::SlotId>> edges;
            for (RigController::SlotId slot : slots)
            {
                for (RigController::SlotId upstream : rig.upstream_of(slot))
                {
                    edges.push_back({ upstream, slot });
                }
            }
            return edges;
        }

        /// @brief Draws an arrow from one card's right edge to another card's left edge.
        void draw_link(ImDrawList* draw, ImVec2 from, ImVec2 to, ImU32 color)
        {
            if (from.y == to.y)
            {
                draw->AddLine(from, to, color, 2.0f);
            }
            else
            {
                const float mid_x = (from.x + to.x) * 0.5f;
                draw->AddBezierCubic(from, ImVec2(mid_x, from.y), ImVec2(mid_x, to.y), to,
                                     color, 2.0f);
            }
            draw->AddTriangleFilled(ImVec2(to.x, to.y),
                                    ImVec2(to.x - 8.0f, to.y - 5.0f),
                                    ImVec2(to.x - 8.0f, to.y + 5.0f), color);
        }
    }

    void DrawChainStrip(RigController& rig, std::optional<RigController::SlotId>& selected,
                        std::optional<RigController::SlotId>& created_slot)
    {
        created_slot = std::nullopt;

        const std::vector<RigController::SlotId> slots = rig.slot_order();
        const ChainView view = layout_chain(slots, collect_edges(rig, slots));

        const Palette& colors = palette();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImDrawList* draw = ImGui::GetWindowDrawList();

        // Links are drawn first so cards sit on top of them.
        for (const ChainLink& link : view.links)
        {
            const ChainCell& from = view.cells[link.from_cell];
            const ChainCell& to = view.cells[link.to_cell];
            const ImVec2 from_point(
                origin.x + static_cast<float>(from.column) * (kCardWidth + kColumnGap) + kCardWidth,
                origin.y + static_cast<float>(from.row) * (kCardHeight + kRowGap) + kCardHeight * 0.5f);
            const ImVec2 to_point(
                origin.x + static_cast<float>(to.column) * (kCardWidth + kColumnGap),
                origin.y + static_cast<float>(to.row) * (kCardHeight + kRowGap) + kCardHeight * 0.5f);
            draw_link(draw, from_point, to_point, to_imu32(colors.muted_text));
        }

        for (const ChainCell& cell : view.cells)
        {
            const ImVec2 card_origin(
                origin.x + static_cast<float>(cell.column) * (kCardWidth + kColumnGap),
                origin.y + static_cast<float>(cell.row) * (kCardHeight + kRowGap));
            const ImVec2 card_end(card_origin.x + kCardWidth, card_origin.y + kCardHeight);
            const bool bypassed = rig.is_bypassed(cell.slot);

            ImGui::PushID(static_cast<int>(cell.slot));
            ImGui::SetCursorScreenPos(card_origin);
            ImGui::InvisibleButton("card", ImVec2(kCardWidth, kCardHeight));
            if (ImGui::IsItemClicked())
            {
                selected = cell.slot;
            }

            const bool is_selected = selected == std::optional<RigController::SlotId>(cell.slot);
            draw->AddRectFilled(card_origin, card_end,
                                to_imu32(rgba(38, 38, 44, bypassed ? 115 : 255)), 6.0f);
            draw->AddRect(card_origin, card_end,
                          to_imu32(is_selected ? colors.accent : rgba(62, 62, 70)), 6.0f, 0,
                          is_selected ? 2.5f : 1.0f);
            const std::string label = rig.descriptor_id(cell.slot);
            draw->AddText(ImVec2(card_origin.x + 10.0f, card_origin.y + 10.0f),
                          to_imu32(bypassed ? colors.muted_text : colors.text), label.c_str());

            if (ImGui::BeginDragDropSource())
            {
                const RigController::SlotId payload = cell.slot;
                ImGui::SetDragDropPayload("RACK_SLOT_ID", &payload, sizeof(RigController::SlotId));
                ImGui::Text("%s", label.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("RACK_SLOT_ID"))
                {
                    const RigController::SlotId moved =
                        *static_cast<const RigController::SlotId*>(payload->Data);
                    rig.move_before(moved, cell.slot);
                }
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GEAR_DESCRIPTOR_ID"))
                {
                    const char* descriptor_id = static_cast<const char*>(payload->Data);
                    created_slot = rig.insert_before(cell.slot, std::string(descriptor_id));
                    selected = created_slot;
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::SetCursorScreenPos(ImVec2(card_origin.x + 10.0f, card_end.y - 26.0f));
            bool active = !bypassed;
            if (ToggleSwitch("bypass", &active, 34.0f, 16.0f))
            {
                rig.set_bypassed(cell.slot, !active);
            }

            ImGui::SetCursorScreenPos(ImVec2(card_end.x - 26.0f, card_end.y - 26.0f));
            if (ImGui::SmallButton("x"))
            {
                rig.remove(cell.slot);
            }
            ImGui::PopID();
        }

        // The drop zone past the last column appends.
        const ImVec2 tail(origin.x + static_cast<float>(view.column_count) * (kCardWidth + kColumnGap),
                          origin.y);
        ImGui::SetCursorScreenPos(tail);
        ImGui::InvisibleButton("append_zone", ImVec2(kCardWidth, kCardHeight));
        draw->AddRect(tail, ImVec2(tail.x + kCardWidth, tail.y + kCardHeight),
                      to_imu32(rgba(62, 62, 70)), 6.0f, 0, 1.0f);
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GEAR_DESCRIPTOR_ID"))
            {
                const char* descriptor_id = static_cast<const char*>(payload->Data);
                created_slot = rig.append(std::string(descriptor_id));
                selected = created_slot;
            }
            ImGui::EndDragDropTarget();
        }

        if (!slot_selection::still_valid(selected, rig.slot_order()))
        {
            selected = std::nullopt;
        }
    }
}
```

- [ ] **Step 3: Build**

Run: `sd build`
Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add apps/host_gui/ui/ChainStrip.hpp apps/host_gui/ui/ChainStrip.cpp
git commit -m "host_gui: draw the rig as a two-row chain with bypass switches"
```

---

### Task 16: Rewire `main.cpp`

**Files:**
- Modify: `apps/host_gui/main.cpp`

**Interfaces:**
- Consumes: every `ui::` entry point from Tasks 8 to 15; `SushiDSP::RigGraph::is_empty()` from Task 6; `RigController::set_bypassed` from Task 7.
- Produces: the running application.

- [ ] **Step 1: Replace the two-state screen with the region layout**

`main.cpp` keeps `executable_directory()` and `apply_default_cabinet()` unchanged. `AppState` and
every `ImGui::` call in the old `SelectingDevice` and `Running` branches go. The frame body
becomes:

```cpp
        const ImGuiWindowFlags kRegionFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

        int display_w = 0;
        int display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        const float width = static_cast<float>(display_w);
        const float height = static_cast<float>(display_h);
        const float top_height = 48.0f;
        const float meter_height = 40.0f;
        const float browser_width = 300.0f;
        const float body_height = height - top_height - meter_height;
        const float stage_height = body_height * 0.58f;

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(width, top_height));
        ImGui::Begin("##top", nullptr, kRegionFlags);
        if (rig_controller_storage.has_value())
        {
            ui::DrawTopBar(*rig_controller_storage, device_label, open_device_settings, status);
        }
        else
        {
            ImGui::TextColored(ui::to_imvec4(ui::palette().muted_text), "No device connected.");
            ImGui::SameLine(width - 96.0f);
            if (ImGui::Button("Settings"))
            {
                open_device_settings = true;
            }
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(0.0f, top_height));
        ImGui::SetNextWindowSize(ImVec2(width - browser_width, stage_height));
        ImGui::Begin("##stage", nullptr, kRegionFlags);
        if (rig_controller_storage.has_value())
        {
            ui::DrawStagePanel(*rig_controller_storage, selected_slot);
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(0.0f, top_height + stage_height));
        ImGui::SetNextWindowSize(ImVec2(width - browser_width, body_height - stage_height));
        ImGui::Begin("##chain", nullptr, kRegionFlags | ImGuiWindowFlags_HorizontalScrollbar);
        if (rig_controller_storage.has_value())
        {
            std::optional<RigController::SlotId> created_slot;
            ui::DrawChainStrip(*rig_controller_storage, selected_slot, created_slot);
            if (created_slot.has_value())
            {
                apply_default_cabinet(*rig_controller_storage, *created_slot, default_ir_path);
            }
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(width - browser_width, top_height));
        ImGui::SetNextWindowSize(ImVec2(browser_width, body_height));
        ImGui::Begin("##gear", nullptr, kRegionFlags);
        ui::DrawGearBrowser();
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(0.0f, height - meter_height));
        ImGui::SetNextWindowSize(ImVec2(width, meter_height));
        ImGui::Begin("##meters", nullptr, kRegionFlags);
        ui::DrawMeterBar(input_probe, output_probe);
        ImGui::End();

        if (ui::DrawDeviceSettings(asio_drivers, device_choice, rig_controller_storage.has_value(),
                                   connect_error, open_device_settings))
        {
            connect_device();
        }
```

- [ ] **Step 2: Declare the new outer-scope state**

Next to the existing `mono_output` and `callback_silence` declarations, and for the same reason
they live there — the audio callback captures them by reference for the device's whole lifetime:

```cpp
    ui::LevelProbe input_probe;
    ui::LevelProbe output_probe;
```

Alongside them:

```cpp
    ui::DeviceChoice device_choice{ 0, SushiDSP::ChannelSelection::Left,
                                    SushiDSP::ChannelSelection::Stereo };
    std::optional<RigController::SlotId> selected_slot;
    std::string device_label = "not connected";
    std::string status;
    bool open_device_settings = true;
```

- [ ] **Step 3: Move the connect sequence into a lambda**

The old `if (ImGui::Button("Connect")) { ... }` body moves verbatim into a `connect_device`
lambda declared after `input_probe`/`output_probe`, capturing everything it needs by reference.
Three changes to that body:

1. It reads `device_choice.device_index`, `device_choice.input` and `device_choice.output`
   instead of the old `selected_device_index`, `selected_input`, `selected_output`.
2. On success it sets `device_label`, clears `selected_slot`, and closes the window:

```cpp
                    device_label = std::string("Connected: ")
                        + (device_choice.device_index == 0
                            ? "SDL2"
                            : asio_drivers[static_cast<std::size_t>(device_choice.device_index - 1)])
                        + " (input " + SushiDSP::channel_selection_label(device_choice.input)
                        + ", output " + SushiDSP::channel_selection_label(device_choice.output) + ")";
                    selected_slot = std::nullopt;
                    open_device_settings = false;
```

3. The callback body gains the two probe reports and the empty-graph passthrough:

```cpp
                        input_probe.report(selected_channel[0], frames);

                        float* mono_output_ptr[1] = { mono_output.data() };
                        SushiDSP::ProcessContext context{ mono_output_ptr, selected_channel, frames, 0.0 };
                        if (active == nullptr)
                        {
                            std::fill(mono_output.begin(), mono_output.begin() + frames, 0.0f);
                        }
                        else if (active->is_empty())
                        {
                            // Every slot is bypassed, so the rig is a wire.
                            std::memcpy(mono_output.data(), selected_channel[0], frames * sizeof(float));
                        }
                        else
                        {
                            active->process(context);
                        }

                        output_probe.report(mono_output.data(), frames);
```

The lambda must also capture `input_probe` and `output_probe` by reference, alongside
`mono_output` and `callback_silence`.

- [ ] **Step 4: Call `apply_theme()` and trim the includes**

After `ImGui::CreateContext()`:

```cpp
    ui::apply_theme();
```

Add these includes:

```cpp
#include "ui/ChainStrip.hpp"
#include "ui/DeviceSettings.hpp"
#include "ui/GearBrowser.hpp"
#include "ui/LevelProbe.hpp"
#include "ui/MeterBar.hpp"
#include "ui/StagePanel.hpp"
#include "ui/Theme.hpp"
#include "ui/TopBar.hpp"
```

Drop the ones only the deleted code used: `SushiDSP/node_registry/NodeRegistry.hpp`,
`SushiDSP/rig_preset/RigPreset.hpp`, `SlotSelection.hpp`, and `<iterator>`. Keep `<fstream>`
out too, since preset file access now lives in `ui/TopBar.cpp`.

- [ ] **Step 5: Build and run the whole suite**

Run: `sd build`
Expected: PASS.

Run: `sd test -s unit`
Expected: every test passes.

- [ ] **Step 6: Run the application and check it by eye**

Run: `build/debug/apps/host_gui/sushidsp_host_gui.exe` (or wherever `sd build` puts it; the path
is printed at the end of the build).

Confirm, in this order:

1. The device window opens by itself. Pick SDL2, click Connect; the window closes and the top bar
   names the device.
2. Drag a TubeScreamer from the browser onto the append zone. A card appears; the stage draws a
   green pedal with three large knobs and one small one.
3. Turn Drive by dragging up. The value in the tooltip rises. Hold Shift and drag the same
   distance; it moves about a quarter as far. Double click; it returns to 0.50.
4. Drag a JCM800 onto the append zone. The stage draws a wide panel with six knobs on a gold
   plate and two smaller ones at the left.
5. Flip the TubeScreamer card's bypass switch. The card dims and the chain still plays.
6. Drag a Splitter and a Mixer in, wire a second branch, and confirm the strip draws two rows with
   curved links.
7. Save to `preset.json`, add another pedal, then Load. The saved rig comes back, bypass included.
8. Resize the window. Every region follows and the panel keeps its proportions.

- [ ] **Step 7: Commit**

```bash
git add apps/host_gui/main.cpp
git commit -m "host_gui: lay the window out as stage, chain, browser and meters"
```

---

## Documentation

The last commit of Task 16 also updates the manual, because a page that stops being true is a
defect in the change that made it false:

- `docs/reference/CHANGELOG.md` gains one line:
  `- 2026-09-05 — Rebuilt the rig GUI as a stage, chain strip, gear browser and meters (`apps/host_gui/ui/`).`
- If `docs/design/sushidsp.md` describes the GUI's rack strip or its slider panel, that passage is
  rewritten to describe what the tree now holds.
