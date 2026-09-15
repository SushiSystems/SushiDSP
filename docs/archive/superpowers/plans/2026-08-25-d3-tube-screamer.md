# SushiDSP D3 — Tube Screamer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first SushiDSP product — a white-box Tube Screamer overdrive pedal (`modules/tubescreamer`) — culminating in it running standalone in the rig host and satisfying D3's roadmap "ships" bar: TS in the rig host, plausibility-tested (DC stability, THD sweep). This is the D3 phase from `docs/design/sushidsp.md` §6/§9.

**Architecture:** `TubeScreamer` is one composite `IAudioNode` (spec §6) wiring four stages: an input DC-blocking highpass (`Biquad`, extended with a new `set_high_pass`), an 8x-oversampled nonlinear op-amp clipping stage (`TubeScreamerClippingStage`, a Newton-Raphson-solved inverting-op-amp circuit in the same family as D2's `DiodeClipperCircuit` but a different topology), a fixed-crossover tone-blend stage (two `Biquad`s crossfaded by the `tone` parameter — a documented simplification of spec §6's "via DK-method" passive network, since a full 2-port DK-method solve for a 3-terminal passive network is materially more work than this product needs to sound and behave correctly; the crossfade reproduces the described "low-pass/high-pass blend" behavior), and an output level stage. It exposes `tube_screamer_descriptor()` (Drive/Tone/Level, `NodeDescriptor`) and lives in `modules/tubescreamer`, a new CMake target depending only on `sushi_dsp`.

This phase also opens `tests/functional/regression/` (empty since D0) and wires its CTest `Regression_*` label — the first product's end-to-end output is the first thing in this repo worth snapshotting against unintended drift.

**Tech Stack:** C++17, CMake, GoogleTest.

**Spec:** `docs/design/sushidsp.md` (§6 Tube Screamer, §7 products/wrappers, §8 repository layout, §9 D3, §0a reference header, §10 SOLID)

## Global Constraints

- C++17 only — no C++20/23 features.
- Allman braces; trivial one-line accessors may stay compact.
- Naming: `PascalCase` types, `snake_case` functions/variables, trailing-underscore members, `UPPER_SNAKE` constants, no abbreviations.
- Apache 2.0 license header: the exact 76-column boxed template pinned in `docs/design/sushidsp.md` §0a — copy an existing file (e.g. `include/SushiDSP/math/Biquad.hpp`) verbatim, only the filename line changes. This applies to every `.hpp`/`.cpp` **and every new test file**, with no exceptions — D2's final review caught a test file that skipped this; do not repeat that gap.
- Doxygen `@brief`/`@param`/`@return` on every public function.
- No comments unless the *why* is non-obvious.
- `sushi_dsp` core stays dependency-free; `SushiDSP::tubescreamer` (the new product target) depends only on `SushiDSP::sushi_dsp` — never on SDL2, never on `apps/host` internals beyond what `apps/host` itself links against it (the dependency direction is host → product, never the reverse).
- Real-time safety: `TubeScreamer::process()` and everything it calls must be allocation-free, lock-free, syscall-free, throw-free. Every class that lands on this path gets a `*ProcessPerformsNoHeapAllocation` case in `tests/functional/unit/test_rt_safety.cpp` — do not leave this as an inspection-only claim (this was a D2 final-review finding; do not regress it).
- Unit tests live under `tests/functional/unit/test_<snake_case_name>.cpp`, GTest suite `Unit_Dsp`, added to `tests/CMakeLists.txt`'s `add_executable(sushidsp_tests ...)`. **New in this plan:** regression tests live under `tests/functional/regression/test_<snake_case_name>.cpp`, GTest suite `Regression_Dsp`, added to the same binary; `tests/CMakeLists.txt` gains a second `gtest_discover_tests(... TEST_FILTER "Regression_*" PROPERTIES LABELS "regression")` call (its own comment already anticipated this — see the file's header comment).
- `Drive`, `Tone`, `Level` are all `[0, 1]` normalized parameters (spec §6, "All smoothed, click-free") — smoothing is a per-block linear slew toward the last-set target, not per-sample (cheap, inaudible for a ~5ms time constant at 256-frame blocks).

---

## File Structure

```
sushidsp/
  include/SushiDSP/math/
    Biquad.hpp                          # MODIFIED: add set_high_pass
  src/math/
    Biquad.cpp                          # MODIFIED
  modules/tubescreamer/
    CMakeLists.txt                      # new SushiDSP::tubescreamer target
    include/SushiDSP/tubescreamer/
      TubeScreamerClippingStage.hpp
      TubeScreamer.hpp
    src/
      TubeScreamerClippingStage.cpp
      TubeScreamer.cpp
  apps/host/
    main.cpp                            # MODIFIED: chain TubeScreamer after the SineToneNode source
    CMakeLists.txt                      # MODIFIED: link SushiDSP::tubescreamer
  tests/functional/unit/
    test_biquad.cpp                     # MODIFIED: add set_high_pass cases
    test_tube_screamer_clipping_stage.cpp
    test_tube_screamer.cpp
  tests/functional/regression/
    test_tube_screamer_regression.cpp
  tests/CMakeLists.txt                  # MODIFIED: new sources + Regression_* discovery call
  CMakeLists.txt                        # MODIFIED: add_subdirectory(modules/tubescreamer)
```

`modules/tubescreamer` gets its own `include/SushiDSP/tubescreamer/` namespace directory (matching the pattern `include/SushiDSP/device/` and `include/SushiDSP/circuits/` already established) — a product module owns a distinct include root so its headers are visibly separate from the core/math library, even though both live under the same `SushiDSP::` C++ namespace.

---

## Task 1: `Biquad::set_high_pass`

**Files:**
- Modify: `include/SushiDSP/math/Biquad.hpp` (add `set_high_pass` declaration)
- Modify: `src/math/Biquad.cpp` (add `set_high_pass` implementation)
- Modify: `tests/functional/unit/test_biquad.cpp` (add new test cases; do not remove or weaken existing ones)
- Modify: `tests/CMakeLists.txt` (no new file, `test_biquad.cpp` is already listed — nothing to change here)

**Interfaces:**
- Produces: `void Biquad::set_high_pass(double sample_rate, double cutoff_hz, double q)`, using the same private coefficient members (`b0_`, `b1_`, `b2_`, `a1_`, `a2_`) `set_low_pass` already writes — `process_sample`/`reset` are unchanged and already correct for any Direct-Form-I biquad regardless of which `set_*` configured it.

- [ ] **Step 1: Write the failing test cases in `tests/functional/unit/test_biquad.cpp`**

Add these `TEST(Unit_Dsp, ...)` cases to the existing file (keep every existing case unmodified):

```cpp
TEST(Unit_Dsp, BiquadHighPassBlocksDcCompletely)
{
    SushiDSP::Biquad filter;
    filter.set_high_pass(48000.0, 1000.0, 0.707);

    float output = 0.0f;
    for (int i = 0; i < 10000; ++i)
    {
        output = filter.process_sample(1.0f);
    }

    EXPECT_NEAR(output, 0.0f, 1e-3f);
}

TEST(Unit_Dsp, BiquadHighPassPassesWellAboveCutoffAtUnityGain)
{
    SushiDSP::Biquad filter;
    constexpr double sample_rate = 48000.0;
    constexpr double cutoff = 500.0;
    filter.set_high_pass(sample_rate, cutoff, 0.707);

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

    EXPECT_NEAR(max_output, 1.0f, 0.1f);
}
```

(`test_biquad.cpp` already includes `<cmath>` for `M_PI`/`std::sin` per the existing low-pass tests — confirm this before adding, do not duplicate an include.)

- [ ] **Step 2: Run to verify the new cases fail**

Run: `sd test`
Expected: FAIL — `Biquad::set_high_pass` does not exist (compile error).

- [ ] **Step 3: Add the declaration to `include/SushiDSP/math/Biquad.hpp`**

Add this public method declaration next to `set_low_pass`:

```cpp
        /// @brief Configures this filter as a high-pass with the given cutoff and Q.
        /// @param sample_rate Sample rate in Hz.
        /// @param cutoff_hz Cutoff frequency in Hz.
        /// @param q Quality factor (0.707 = Butterworth).
        void set_high_pass(double sample_rate, double cutoff_hz, double q);
```

- [ ] **Step 4: Add the implementation to `src/math/Biquad.cpp`**

Add this method (RBJ cookbook high-pass — same `omega`/`alpha` derivation `set_low_pass` uses, different numerator):

```cpp
    void Biquad::set_high_pass(double sample_rate, double cutoff_hz, double q)
    {
        const double omega = 2.0 * M_PI * cutoff_hz / sample_rate;
        const double sin_omega = std::sin(omega);
        const double cos_omega = std::cos(omega);
        const double alpha = sin_omega / (2.0 * q);

        const double b0 = (1.0 + cos_omega) / 2.0;
        const double b1 = -(1.0 + cos_omega);
        const double b2 = (1.0 + cos_omega) / 2.0;
        const double a0 = 1.0 + alpha;
        const double a1 = -2.0 * cos_omega;
        const double a2 = 1.0 - alpha;

        b0_ = static_cast<float>(b0 / a0);
        b1_ = static_cast<float>(b1 / a0);
        b2_ = static_cast<float>(b2 / a0);
        a1_ = static_cast<float>(a1 / a0);
        a2_ = static_cast<float>(a2 / a0);
    }
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `sd test`
Expected: PASS — the 2 new cases plus all 55 pre-existing tests.

- [ ] **Step 6: Commit**

```bash
git add include/SushiDSP/math/Biquad.hpp src/math/Biquad.cpp tests/functional/unit/test_biquad.cpp
git commit -m "Add Biquad::set_high_pass (RBJ cookbook)"
```

---

## Task 2: `TubeScreamerClippingStage` — the nonlinear op-amp clipper

**Files:**
- Create: `modules/tubescreamer/include/SushiDSP/tubescreamer/TubeScreamerClippingStage.hpp`
- Create: `modules/tubescreamer/src/TubeScreamerClippingStage.cpp`
- Test: `tests/functional/unit/test_tube_screamer_clipping_stage.cpp`
- Modify: `tests/CMakeLists.txt` (add the test file; the module's own `.cpp` is added via a `target_include_directories`/direct source reference — see Step 2)
- Create: `modules/tubescreamer/CMakeLists.txt` (introduced fully in Task 4; for this task, add the file to `tests/CMakeLists.txt`'s `add_executable` directly by relative path from the repo root, matching the pattern `apps/host/SineToneNode.cpp` already uses for a not-yet-target-wrapped source — see Step 2's exact line)

**Interfaces:**
- Consumes: `SushiDSP::NewtonRaphsonSolver::solve` (D2, `include/SushiDSP/math/NewtonRaphsonSolver.hpp`).
- Produces: `SushiDSP::TubeScreamer::TubeScreamerClippingStage` (namespace `SushiDSP::TubeScreamer` — see note below) with `TubeScreamerClippingStage(float input_resistance_ohms, float feedback_resistance_ohms, float saturation_current_amps, float thermal_voltage_volts)`, `float process_sample(float input_voltage)`, `void reset()`, `void set_feedback_resistance(float feedback_resistance_ohms)` (the `drive` knob changes this live). Task 3 (the composite `TubeScreamer` node) is the only consumer.

**Note on namespacing:** both this class and Task 3's `TubeScreamer` `IAudioNode` live directly in `namespace SushiDSP { ... }` — flat, no nested `SushiDSP::TubeScreamer` sub-namespace. This matches how `DiodeClipperCircuit` sits directly in `SushiDSP` in D2 (not in a `SushiDSP::Circuits` sub-namespace), and avoids a type and a namespace ever sharing the name `TubeScreamer` in the same scope.

**Circuit derivation (inverting op-amp with antiparallel-diode feedback — the core TS nonlinearity):** virtual-ground KCL at the op-amp's inverting input gives `Vin/R1 = -Vout/Rf - Is*(exp(Vout/Vt) - exp(-Vout/Vt))`, i.e. `f(Vout) = Vout/Rf + Is*(exp(Vout/Vt) - exp(-Vout/Vt)) + Vin/R1 = 0`, `f'(Vout) = 1/Rf + (Is/Vt)*(exp(Vout/Vt) + exp(-Vout/Vt))`. This is solved by `NewtonRaphsonSolver::solve`, warm-started from the previous sample, with the same `±60*Vt` bounds D2's `DiodeClipperCircuit` established (verified there to sit safely above any physically realistic clipped output while keeping `exp()` finite) — **reuse that exact bound value and reasoning, do not re-derive or pick a different one.** `R1` is fixed (`input_resistance_ohms`); `Rf` is the drive-controlled feedback resistance, changeable live via `set_feedback_resistance`.

- [ ] **Step 1: Write the failing test `tests/functional/unit/test_tube_screamer_clipping_stage.cpp`**

```cpp
#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/tubescreamer/TubeScreamerClippingStage.hpp"

namespace
{
    constexpr float kInputResistance = 4700.0f;      // R1, TS808-typical
    constexpr float kFeedbackResistance = 51000.0f;  // Rf at a mid-drive setting
    constexpr float kSaturationCurrent = 1e-12f;
    constexpr float kThermalVoltage = 0.02585f;
}

TEST(Unit_Dsp, TubeScreamerClippingStageIsDcStableForConstantInput)
{
    SushiDSP::TubeScreamerClippingStage stage(
        kInputResistance, kFeedbackResistance, kSaturationCurrent, kThermalVoltage);

    float output = 0.0f;
    for (int i = 0; i < 1000; ++i)
    {
        output = stage.process_sample(0.05f);
    }

    EXPECT_TRUE(std::isfinite(output));
}

TEST(Unit_Dsp, TubeScreamerClippingStageIsInvertingAndApproximatelyOddSymmetric)
{
    SushiDSP::TubeScreamerClippingStage positive_stage(
        kInputResistance, kFeedbackResistance, kSaturationCurrent, kThermalVoltage);
    SushiDSP::TubeScreamerClippingStage negative_stage(
        kInputResistance, kFeedbackResistance, kSaturationCurrent, kThermalVoltage);

    float positive_output = 0.0f;
    float negative_output = 0.0f;
    for (int i = 0; i < 50; ++i)
    {
        positive_output = positive_stage.process_sample(0.02f);
        negative_output = negative_stage.process_sample(-0.02f);
    }

    EXPECT_LT(positive_output, 0.0f);  // inverting: positive in -> negative out
    EXPECT_NEAR(positive_output, -negative_output, 1e-3f);
}

TEST(Unit_Dsp, TubeScreamerClippingStageHigherFeedbackResistanceGivesMoreGainBeforeClipping)
{
    SushiDSP::TubeScreamerClippingStage low_feedback(
        kInputResistance, 1000.0f, kSaturationCurrent, kThermalVoltage);
    SushiDSP::TubeScreamerClippingStage high_feedback(
        kInputResistance, 200000.0f, kSaturationCurrent, kThermalVoltage);

    float low_output = 0.0f;
    float high_output = 0.0f;
    for (int i = 0; i < 50; ++i)
    {
        low_output = low_feedback.process_sample(0.01f);
        high_output = high_feedback.process_sample(0.01f);
    }

    EXPECT_LT(std::abs(low_output), std::abs(high_output));
}

TEST(Unit_Dsp, TubeScreamerClippingStageSetFeedbackResistanceChangesBehaviorLive)
{
    SushiDSP::TubeScreamerClippingStage stage(
        kInputResistance, 1000.0f, kSaturationCurrent, kThermalVoltage);

    float before = 0.0f;
    for (int i = 0; i < 50; ++i)
    {
        before = stage.process_sample(0.01f);
    }

    stage.set_feedback_resistance(200000.0f);

    float after = 0.0f;
    for (int i = 0; i < 50; ++i)
    {
        after = stage.process_sample(0.01f);
    }

    EXPECT_LT(std::abs(before), std::abs(after));
}

TEST(Unit_Dsp, TubeScreamerClippingStageResetClearsWarmStartState)
{
    SushiDSP::TubeScreamerClippingStage stage_a(
        kInputResistance, kFeedbackResistance, kSaturationCurrent, kThermalVoltage);
    SushiDSP::TubeScreamerClippingStage stage_b(
        kInputResistance, kFeedbackResistance, kSaturationCurrent, kThermalVoltage);

    stage_a.process_sample(0.1f);
    stage_a.reset();

    const float from_reset = stage_a.process_sample(0.005f);
    const float from_fresh = stage_b.process_sample(0.005f);

    EXPECT_NEAR(from_reset, from_fresh, 1e-5f);
}
```

- [ ] **Step 2: Add the test file and the (not-yet-target-wrapped) implementation source to `tests/CMakeLists.txt`**

Add `functional/unit/test_tube_screamer_clipping_stage.cpp` and `${CMAKE_SOURCE_DIR}/modules/tubescreamer/src/TubeScreamerClippingStage.cpp` to `add_executable(sushidsp_tests ...)`, and add a `target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/modules/tubescreamer/include)` line (matching the existing `apps/host` include-directory pattern). Task 4 will introduce the real `modules/tubescreamer/CMakeLists.txt` target and this direct-source-reference approach is superseded there — for now it is how `tests/CMakeLists.txt` compiles the class before the module target exists, exactly matching how `apps/host/SineToneNode.cpp` was handled in D1 Task 4 before `apps/host`'s own target existed.

Run: `sd test`
Expected: FAIL — `SushiDSP/tubescreamer/TubeScreamerClippingStage.hpp` does not exist.

- [ ] **Step 3: Write `modules/tubescreamer/include/SushiDSP/tubescreamer/TubeScreamerClippingStage.hpp`**

```cpp
#pragma once

namespace SushiDSP
{
    /// @brief The TS-family inverting op-amp clipping stage: virtual-ground KCL against an
    /// antiparallel diode pair in the feedback path, solved per sample by Newton-Raphson.
    class TubeScreamerClippingStage
    {
    public:
        /// @param input_resistance_ohms R1, the fixed input resistor.
        /// @param feedback_resistance_ohms Rf, the drive-controlled feedback resistance.
        /// @param saturation_current_amps Diode saturation current Is (Shockley model).
        /// @param thermal_voltage_volts Diode thermal voltage Vt (~0.02585 V at room temperature).
        TubeScreamerClippingStage(
            float input_resistance_ohms, float feedback_resistance_ohms,
            float saturation_current_amps, float thermal_voltage_volts);

        /// @brief Processes one sample through the inverting clipper.
        /// @param input_voltage The stage's input voltage for this sample.
        /// @return The clipped, inverted output voltage.
        float process_sample(float input_voltage);

        /// @brief Changes the feedback resistance (the drive control) for subsequent samples.
        /// @param feedback_resistance_ohms The new Rf value.
        void set_feedback_resistance(float feedback_resistance_ohms);

        /// @brief Clears the Newton-Raphson warm-start state.
        void reset();

    private:
        float input_resistance_;
        float feedback_resistance_;
        float saturation_current_;
        float thermal_voltage_;
        float previous_voltage_ = 0.0f;
    };
}
```

- [ ] **Step 4: Write `modules/tubescreamer/src/TubeScreamerClippingStage.cpp`**

```cpp
#include "SushiDSP/tubescreamer/TubeScreamerClippingStage.hpp"

#include "SushiDSP/math/NewtonRaphsonSolver.hpp"

namespace SushiDSP
{
    TubeScreamerClippingStage::TubeScreamerClippingStage(
        float input_resistance_ohms, float feedback_resistance_ohms,
        float saturation_current_amps, float thermal_voltage_volts)
        : input_resistance_(input_resistance_ohms)
        , feedback_resistance_(feedback_resistance_ohms)
        , saturation_current_(saturation_current_amps)
        , thermal_voltage_(thermal_voltage_volts)
    {
    }

    float TubeScreamerClippingStage::process_sample(float input_voltage)
    {
        const float r1 = input_resistance_;
        const float rf = feedback_resistance_;
        const float is = saturation_current_;
        const float vt = thermal_voltage_;
        const float vin = input_voltage;

        auto residual = [r1, rf, is, vt, vin](float v)
        {
            return v / rf + is * (std::exp(v / vt) - std::exp(-v / vt)) + vin / r1;
        };
        auto derivative = [rf, is, vt](float v)
        {
            return 1.0f / rf + (is / vt) * (std::exp(v / vt) + std::exp(-v / vt));
        };

        const float voltage_limit = 60.0f * vt;
        const float voltage = NewtonRaphsonSolver::solve(
            residual, derivative, previous_voltage_, 4, -voltage_limit, voltage_limit);
        previous_voltage_ = voltage;
        return voltage;
    }

    void TubeScreamerClippingStage::set_feedback_resistance(float feedback_resistance_ohms)
    {
        feedback_resistance_ = feedback_resistance_ohms;
    }

    void TubeScreamerClippingStage::reset()
    {
        previous_voltage_ = 0.0f;
    }
}
```

Add `#include <cmath>` at the top for `std::exp`. **Confirm `NewtonRaphsonSolver::solve`'s exact signature in `include/SushiDSP/math/NewtonRaphsonSolver.hpp` before writing this** — D2 Task 4 added optional `minimum_x`/`maximum_x` trailing parameters beyond the plan's original 4-argument form (a documented, reviewed deviation); this task's call above assumes that extended signature exists. If it does not (i.e. you're looking at an older/different signature), use whatever bounded-solve mechanism D2 actually shipped, and note the discrepancy in your report.

- [ ] **Step 5: Run tests to verify they pass**

Run: `sd test`
Expected: PASS — all `TubeScreamerClippingStage*` tests plus all pre-existing tests.

- [ ] **Step 6: Add the license header to both new files, then commit**

```bash
git add modules/tubescreamer/include/SushiDSP/tubescreamer/TubeScreamerClippingStage.hpp \
        modules/tubescreamer/src/TubeScreamerClippingStage.cpp \
        tests/functional/unit/test_tube_screamer_clipping_stage.cpp tests/CMakeLists.txt
git commit -m "Add TubeScreamerClippingStage (op-amp Newton-Raphson clipper)"
```

---

## Task 3: `TubeScreamer` — the composite `IAudioNode`

**Files:**
- Create: `modules/tubescreamer/include/SushiDSP/tubescreamer/TubeScreamer.hpp`
- Create: `modules/tubescreamer/src/TubeScreamer.cpp`
- Test: `tests/functional/unit/test_tube_screamer.cpp`
- Modify: `tests/CMakeLists.txt` (add the test file and `modules/tubescreamer/src/TubeScreamer.cpp`)

**Interfaces:**
- Consumes: `SushiDSP::IAudioNode`, `PrepareInfo`, `ProcessContext`, `PortLayout` (D0); `SushiDSP::NodeDescriptor`, `ParameterDescriptor`, `ParamKind` (D0, `include/SushiDSP/core/NodeDescriptor.hpp`); `SushiDSP::Biquad` (D0 + Task 1's `set_high_pass`); `SushiDSP::Oversampler8x` (D2, `include/SushiDSP/math/Oversampler.hpp`); `SushiDSP::TubeScreamerClippingStage` (Task 2).
- Produces: `SushiDSP::TubeScreamer` implementing `IAudioNode`, with `void set_drive(float normalized)`, `void set_tone(float normalized)`, `void set_level(float normalized)` (each `[0, 1]`, slewed toward over subsequent blocks — not applied instantaneously). `const SushiDSP::NodeDescriptor& tube_screamer_descriptor()` (free function) — the product registration entry point. Task 4 (module CMake target, host wiring, regression/plausibility tests) is the consumer.

**Signal chain per block:** input → `input_highpass_` (`Biquad::set_high_pass`, fixed ~20 Hz, DC/subsonic blocker — not user-controllable, matches spec's "input buffer stage... high-pass (input cap)") → 8x oversample (`Oversampler8x`) → per-oversampled-sample `clipping_stage_.process_sample(...)` (its `Rf` set from the current slewed `drive_` value, mapped `Rf = 51 + drive*500000` ohms, matching TS808-typical component values) → downsample back to base rate → tone crossfade (`tone_lowpass_`/`tone_highpass_`, both fixed ~720 Hz crossover, blended `y = (1-tone_)*lowpass_out + tone_*highpass_out`) → output scaled by `level_` (linear `[0, 1]`, unity at 1).

- [ ] **Step 1: Write the failing test `tests/functional/unit/test_tube_screamer.cpp`**

```cpp
#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/tubescreamer/TubeScreamer.hpp"

TEST(Unit_Dsp, TubeScreamerProcessesABlockWithoutCrashingOrProducingNonFiniteOutput)
{
    SushiDSP::TubeScreamer node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_drive(0.5f);
    node.set_tone(0.5f);
    node.set_level(0.8f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    for (int block = 0; block < 20; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    for (float sample : output)
    {
        EXPECT_TRUE(std::isfinite(sample));
    }
}

TEST(Unit_Dsp, TubeScreamerPortsAreMonoInMonoOut)
{
    SushiDSP::TubeScreamer node;
    SushiDSP::PortLayout ports = node.ports();
    EXPECT_EQ(ports.input_channels, 1u);
    EXPECT_EQ(ports.output_channels, 1u);
}

TEST(Unit_Dsp, TubeScreamerLevelZeroProducesSilenceAfterSettling)
{
    SushiDSP::TubeScreamer node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_drive(0.5f);
    node.set_tone(0.5f);
    node.set_level(0.0f);

    std::vector<float> input(256, 0.2f);
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    for (int block = 0; block < 20; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    for (float sample : output)
    {
        EXPECT_NEAR(sample, 0.0f, 1e-3f);
    }
}

TEST(Unit_Dsp, TubeScreamerDescriptorExposesDriveToneLevel)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::tube_screamer_descriptor();

    EXPECT_STREQ(descriptor.id, "sushidsp.tubescreamer");
    ASSERT_EQ(descriptor.parameter_count, 3u);

    bool has_drive = false, has_tone = false, has_level = false;
    for (std::size_t i = 0; i < descriptor.parameter_count; ++i)
    {
        const std::string param_id = descriptor.parameters[i].id;
        if (param_id == "drive") has_drive = true;
        if (param_id == "tone") has_tone = true;
        if (param_id == "level") has_level = true;
    }
    EXPECT_TRUE(has_drive);
    EXPECT_TRUE(has_tone);
    EXPECT_TRUE(has_level);
}

TEST(Unit_Dsp, TubeScreamerDescriptorMakeProducesAWorkingNode)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::tube_screamer_descriptor();
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };

    std::unique_ptr<SushiDSP::IAudioNode> node = descriptor.make(info);
    ASSERT_NE(node, nullptr);
    node->prepare(info);

    std::vector<float> input(256, 0.0f);
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };
    SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
    node->process(context);

    for (float sample : output)
    {
        EXPECT_TRUE(std::isfinite(sample));
    }
}
```

(`<string>` and `<memory>` are needed for the descriptor tests — add those includes alongside `<cmath>`/`<vector>`.)

- [ ] **Step 2: Add the test file and `modules/tubescreamer/src/TubeScreamer.cpp` to `tests/CMakeLists.txt`**

Run: `sd test`
Expected: FAIL — `SushiDSP/tubescreamer/TubeScreamer.hpp` does not exist.

- [ ] **Step 3: Write `modules/tubescreamer/include/SushiDSP/tubescreamer/TubeScreamer.hpp`**

```cpp
#pragma once

#include <memory>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"
#include "SushiDSP/math/Biquad.hpp"
#include "SushiDSP/math/Oversampler.hpp"
#include "SushiDSP/tubescreamer/TubeScreamerClippingStage.hpp"

namespace SushiDSP
{
    /// @brief The Tube Screamer overdrive pedal: input highpass, 8x-oversampled op-amp
    /// clipping stage, tone crossfade, output level. Exposes tube_screamer_descriptor().
    class TubeScreamer : public IAudioNode
    {
    public:
        TubeScreamer();

        void prepare(const PrepareInfo& info) override;
        void process(ProcessContext& context) override;
        PortLayout ports() const override;
        void reset() override;

        /// @brief Sets the drive control, slewed toward this value over subsequent blocks.
        /// @param normalized Drive amount in [0, 1].
        void set_drive(float normalized);

        /// @brief Sets the tone control, slewed toward this value over subsequent blocks.
        /// @param normalized Tone position in [0, 1] (0 = dark, 1 = bright).
        void set_tone(float normalized);

        /// @brief Sets the output level, slewed toward this value over subsequent blocks.
        /// @param normalized Output level in [0, 1].
        void set_level(float normalized);

        // Public so the free function feedback_resistance_for_drive() in TubeScreamer.cpp's
        // anonymous namespace can reference them without befriending the class.
        static constexpr float kMinFeedbackResistance = 51.0f;
        static constexpr float kMaxFeedbackResistance = 500051.0f;

    private:
        double sample_rate_ = 48000.0;

        float drive_target_ = 0.5f, drive_current_ = 0.5f;
        float tone_target_ = 0.5f, tone_current_ = 0.5f;
        float level_target_ = 0.7f, level_current_ = 0.7f;

        Biquad input_highpass_;
        Biquad tone_lowpass_;
        Biquad tone_highpass_;
        TubeScreamerClippingStage clipping_stage_;
        Oversampler8x oversampler_;

        std::vector<float> oversampled_scratch_;
    };

    /// @brief The Tube Screamer's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.tubescreamer".
    const NodeDescriptor& tube_screamer_descriptor();
}
```

`Oversampler8x` has no default constructor (it requires `block_size` — D2 Task 2); `TubeScreamer` must construct it with the block size known at `prepare()` time, not at `TubeScreamer()` construction time. Since `Oversampler8x` is a non-default-constructible member, either (a) make it a `std::unique_ptr<Oversampler8x>` constructed in `prepare()`, or (b) require `TubeScreamer`'s constructor to take a `max_block_size` parameter. **Use (a)** — it matches the constructor-takes-no-arguments shape the `NodeDescriptor::make` factory needs (`std::unique_ptr<IAudioNode> (*)(const PrepareInfo&)` calls `prepare()` immediately after construction in every existing call site, e.g. D1's `main.cpp`, so deferring the oversampler's construction to `prepare()` is both correct and consistent). Adjust the header's `oversampler_` member to `std::unique_ptr<Oversampler8x> oversampler_;` and add `#include <memory>` (already present above) and `#include <vector>` for `oversampled_scratch_`.

- [ ] **Step 4: Write `modules/tubescreamer/src/TubeScreamer.cpp`**

```cpp
#include "SushiDSP/tubescreamer/TubeScreamer.hpp"

namespace SushiDSP
{
    namespace
    {
        constexpr float kInputHighpassCutoffHz = 20.0f;
        constexpr float kToneCrossoverHz = 720.0f;
        constexpr float kInputResistanceOhms = 4700.0f;
        constexpr float kSaturationCurrentAmps = 1e-12f;
        constexpr float kThermalVoltageVolts = 0.02585f;
        constexpr float kSlewPerBlock = 0.1f;  // reaches target in ~10 blocks (~53ms @ 256/48k)

        float slew_toward(float current, float target)
        {
            return current + (target - current) * kSlewPerBlock;
        }

        float feedback_resistance_for_drive(float drive)
        {
            return TubeScreamer::kMinFeedbackResistance
                + drive * (TubeScreamer::kMaxFeedbackResistance - TubeScreamer::kMinFeedbackResistance);
        }
    }

    TubeScreamer::TubeScreamer()
        : clipping_stage_(
            kInputResistanceOhms,
            feedback_resistance_for_drive(0.5f),
            kSaturationCurrentAmps,
            kThermalVoltageVolts)
    {
    }

    void TubeScreamer::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;
        input_highpass_.set_high_pass(sample_rate_, kInputHighpassCutoffHz, 0.707);
        tone_lowpass_.set_low_pass(sample_rate_, kToneCrossoverHz, 0.707);
        tone_highpass_.set_high_pass(sample_rate_, kToneCrossoverHz, 0.707);
        oversampler_ = std::make_unique<Oversampler8x>(info.max_block);
        oversampled_scratch_.assign(static_cast<std::size_t>(info.max_block) * 8, 0.0f);
    }

    void TubeScreamer::process(ProcessContext& context)
    {
        drive_current_ = slew_toward(drive_current_, drive_target_);
        tone_current_ = slew_toward(tone_current_, tone_target_);
        level_current_ = slew_toward(level_current_, level_target_);
        clipping_stage_.set_feedback_resistance(feedback_resistance_for_drive(drive_current_));

        const float* input = context.inputs[0];
        float* output = context.outputs[0];

        for (std::uint32_t i = 0; i < context.frames; ++i)
        {
            oversampled_scratch_[i] = input_highpass_.process_sample(input[i]);
        }

        const float* upsampled = oversampler_->upsample(oversampled_scratch_.data(), context.frames);
        std::vector<float> clipped(static_cast<std::size_t>(context.frames) * 8, 0.0f);
        for (std::uint32_t i = 0; i < context.frames * 8; ++i)
        {
            clipped[i] = clipping_stage_.process_sample(upsampled[i]);
        }
        oversampler_->downsample(clipped.data(), context.frames * 8, oversampled_scratch_.data());

        for (std::uint32_t i = 0; i < context.frames; ++i)
        {
            const float low = tone_lowpass_.process_sample(oversampled_scratch_[i]);
            const float high = tone_highpass_.process_sample(oversampled_scratch_[i]);
            const float toned = (1.0f - tone_current_) * low + tone_current_ * high;
            output[i] = toned * level_current_;
        }
    }

    PortLayout TubeScreamer::ports() const
    {
        return { 1, 1 };
    }

    void TubeScreamer::reset()
    {
        input_highpass_.reset();
        tone_lowpass_.reset();
        tone_highpass_.reset();
        clipping_stage_.reset();
    }

    void TubeScreamer::set_drive(float normalized) { drive_target_ = normalized; }
    void TubeScreamer::set_tone(float normalized) { tone_target_ = normalized; }
    void TubeScreamer::set_level(float normalized) { level_target_ = normalized; }

    namespace
    {
        constexpr ParameterDescriptor kTubeScreamerParameters[] = {
            { "drive", "Drive", 0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
            { "tone",  "Tone",  0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
            { "level", "Level", 0.0f, 1.0f, 0.7f, ParamKind::Linear, "" },
        };

        std::unique_ptr<IAudioNode> make_tube_screamer(const PrepareInfo&)
        {
            return std::make_unique<TubeScreamer>();
        }
    }

    const NodeDescriptor& tube_screamer_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.tubescreamer",
            "Tube Screamer",
            kTubeScreamerParameters,
            3,
            PortLayout{ 1, 1 },
            &make_tube_screamer,
        };
        return descriptor;
    }
}
```

**This step has a known bug for the implementer to find and fix, not to transcribe blindly:** `clipped` is declared as a local `std::vector<float>` inside `process()`, which allocates on every call — a direct violation of this plan's RT-safety Global Constraint. Fix it by making `clipped` a preallocated member (sized in `prepare()`, alongside `oversampled_scratch_`) instead of a local. Do this before writing Task 4's RT-safety trap test, not after — the trap test should catch this if you miss it, but the fix belongs here. Also double-check `oversampled_scratch_` is reused correctly for both the pre-oversample highpass output AND the post-downsample tone-stage input (it is, by design — the buffer is fully overwritten each time before being read back), and add `#include <cstdint>` if `std::uint32_t` isn't already visible transitively.

- [ ] **Step 5: Run tests to verify they pass**

Run: `sd test`
Expected: PASS — all `TubeScreamer*` tests (not `TubeScreamerClippingStage*`, which was Task 2) plus every pre-existing test.

- [ ] **Step 6: Add the license header to both new files, then commit**

```bash
git add modules/tubescreamer/include/SushiDSP/tubescreamer/TubeScreamer.hpp \
        modules/tubescreamer/src/TubeScreamer.cpp \
        tests/functional/unit/test_tube_screamer.cpp tests/CMakeLists.txt
git commit -m "Add the TubeScreamer composite IAudioNode and tube_screamer_descriptor()"
```

---

## Task 4: `modules/tubescreamer` target, rig-host wiring, regression suite, RT-safety, plausibility

**Files:**
- Create: `modules/tubescreamer/CMakeLists.txt`
- Modify: `CMakeLists.txt` (add `add_subdirectory(modules/tubescreamer)`; remove the two direct `modules/tubescreamer/src/*.cpp` source references and the extra include-directory line from `tests/CMakeLists.txt` that Tasks 2/3 added as a stopgap, replacing them with `target_link_libraries(sushidsp_tests PRIVATE SushiDSP::tubescreamer)`)
- Modify: `apps/host/main.cpp`, `apps/host/CMakeLists.txt` (chain a `TubeScreamer` after the `SineToneNode` source in the rig)
- Modify: `tests/functional/unit/test_rt_safety.cpp` (add `TubeScreamerProcessPerformsNoHeapAllocation`)
- Create: `tests/functional/regression/test_tube_screamer_regression.cpp` (new directory — first regression test in the repo)
- Modify: `tests/CMakeLists.txt` (switch to linking the new target instead of raw sources; add the regression test file; add the `Regression_*` `gtest_discover_tests` call)
- Create/Modify plausibility tests for DC stability and THD sweep (added to `test_tube_screamer.cpp` from Task 3, or a new `tests/functional/unit/test_tube_screamer_plausibility.cpp` — your call on the split, but the cases below must exist somewhere and be wired into the build)

**Interfaces:**
- Consumes: everything from Tasks 1-3.
- Produces: the `SushiDSP::tubescreamer` CMake target (exported alongside `sushi_dsp`), and the D3-phase "ships" proof: `sushidsp_host` running a rig of `[SineToneNode → TubeScreamer]`, plus the regression/plausibility test suite.

- [ ] **Step 1: Write `modules/tubescreamer/CMakeLists.txt`**

```cmake
add_library(tubescreamer STATIC
    src/TubeScreamerClippingStage.cpp
    src/TubeScreamer.cpp
)
add_library(SushiDSP::tubescreamer ALIAS tubescreamer)
target_link_libraries(tubescreamer PUBLIC SushiDSP::sushi_dsp)
target_include_directories(tubescreamer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
```

- [ ] **Step 2: Wire it into the root `CMakeLists.txt`**

Add `add_subdirectory(modules/tubescreamer)` after the `sushi_dsp` target definition and before `enable_testing()`/`add_subdirectory(tests)` (i.e. alongside where `sushi_dsp_sdl2`/`apps/host` are added — the module does not depend on `SUSHIDSP_BUILD_SDL2` and must be added unconditionally, since tests need it regardless of whether SDL2 is enabled).

- [ ] **Step 3: Simplify `tests/CMakeLists.txt`'s source list — remove the Task 2/3 stopgap, link the real target**

Remove these two lines (added as a stopgap in Tasks 2/3) from `add_executable(sushidsp_tests ...)`:
```
${CMAKE_SOURCE_DIR}/modules/tubescreamer/src/TubeScreamerClippingStage.cpp
${CMAKE_SOURCE_DIR}/modules/tubescreamer/src/TubeScreamer.cpp
```
and this line from below the executable:
```
target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/modules/tubescreamer/include)
```
Then add `SushiDSP::tubescreamer` to `target_link_libraries(sushidsp_tests PRIVATE ...)` (alongside the existing `SushiDSP::sushi_dsp` and `GTest::gtest`). Add `functional/unit/test_tube_screamer_clipping_stage.cpp` and `functional/unit/test_tube_screamer.cpp` to a new `# --- Products ---` comment group in the source list (they were already added by Tasks 2/3 — just regroup/relabel if they landed under `# --- Circuits ---` or similar; use your judgment on the tidiest grouping, this is cosmetic).

- [ ] **Step 4: Build to verify the refactor didn't break anything**

Run: `sd test`
Expected: PASS — all tests from Tasks 1-3 still pass, now compiled via the `SushiDSP::tubescreamer` target instead of raw source references.

- [ ] **Step 5: Add the RT-safety trap test**

Add to `tests/functional/unit/test_rt_safety.cpp` (add `#include "SushiDSP/tubescreamer/TubeScreamer.hpp"` if not already present):

```cpp
TEST(Unit_Dsp, TubeScreamerProcessPerformsNoHeapAllocation)
{
    SushiDSP::TubeScreamer node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_drive(0.6f);
    node.set_tone(0.4f);
    node.set_level(0.9f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int block = 0; block < 1000; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
}
```

If this fails (nonzero trapped allocations), the most likely cause is the `clipped` local-vector bug flagged in Task 3 Step 4 not having been fixed — go back and fix it there (make it a preallocated member), don't patch around it here.

Run: `sd test`
Expected: PASS after any needed fix from the paragraph above.

- [ ] **Step 6: Write the plausibility tests (DC stability, THD sweep) — satisfies D3's roadmap "ships" bar**

Add to `tests/functional/unit/test_tube_screamer.cpp` (or a new file, your call — see this task's Files note):

```cpp
TEST(Unit_Dsp, TubeScreamerIsDcStableAcrossManyBlocksAtHighDrive)
{
    SushiDSP::TubeScreamer node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_drive(1.0f);
    node.set_tone(0.5f);
    node.set_level(1.0f);

    std::vector<float> input(256, 0.1f);
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    for (int block = 0; block < 200; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
        for (float sample : output)
        {
            ASSERT_TRUE(std::isfinite(sample)) << "block " << block;
        }
    }
}

TEST(Unit_Dsp, TubeScreamerThdIncreasesWithDrive)
{
    auto measure_thd_proxy = [](float drive) -> float
    {
        SushiDSP::TubeScreamer node;
        SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
        node.prepare(info);
        node.set_drive(drive);
        node.set_tone(0.5f);
        node.set_level(1.0f);

        constexpr float frequency = 220.0f;
        constexpr double sample_rate = 48000.0;
        std::vector<float> output;
        double global_sample_index = 0.0;

        for (int block = 0; block < 30; ++block)
        {
            std::vector<float> input(256, 0.0f);
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                input[i] = 0.2f * std::sin(2.0 * M_PI * frequency * (global_sample_index + i) / sample_rate);
            }
            global_sample_index += 256.0;

            std::vector<float> block_output(256, 0.0f);
            const float* input_channels[1] = { input.data() };
            float* output_channels[1] = { block_output.data() };
            SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
            node.process(context);
            output.insert(output.end(), block_output.begin(), block_output.end());
        }

        // THD proxy: RMS deviation of the (settled, tail-half) output from a best-fit sine at
        // the fundamental, normalized by the output's own RMS. Not a true THD measurement (that
        // needs an FFT-based harmonic decomposition), but monotonic with clipping severity for
        // this purpose, and cheap.
        const std::size_t settle_start = output.size() / 2;
        double sum_sq = 0.0, sum_sin = 0.0, sum_cos = 0.0;
        for (std::size_t i = settle_start; i < output.size(); ++i)
        {
            const double phase = 2.0 * M_PI * frequency * i / sample_rate;
            sum_sq += static_cast<double>(output[i]) * output[i];
            sum_sin += output[i] * std::sin(phase);
            sum_cos += output[i] * std::cos(phase);
        }
        const std::size_t n = output.size() - settle_start;
        const double fundamental_amplitude = 2.0 * std::sqrt(sum_sin * sum_sin + sum_cos * sum_cos) / n;
        const double fundamental_power = 0.5 * fundamental_amplitude * fundamental_amplitude * n;
        const double residual_power = std::max(0.0, sum_sq - fundamental_power);
        return static_cast<float>(std::sqrt(residual_power / std::max(1e-12, sum_sq)));
    };

    const float thd_low_drive = measure_thd_proxy(0.05f);
    const float thd_high_drive = measure_thd_proxy(1.0f);

    EXPECT_LT(thd_low_drive, thd_high_drive);
}
```

If the THD-proxy test is flaky or the low/high drive values don't clearly separate, it is acceptable to adjust the drive values or the settle window — but do not remove the underlying claim (higher drive must measurably increase harmonic content); that is the actual plausibility bar D3's roadmap line names.

- [ ] **Step 7: Open `tests/functional/regression/` and write the first regression test**

Create `tests/functional/regression/test_tube_screamer_regression.cpp`. A regression test pins a fixed input against a fixed, previously-verified expected output — run the test once against your own implementation, record what it actually produces at a handful of sample indices, and bake those exact values in as the expectation (this is the standard "snapshot" pattern; you are not deriving the expected values analytically, you are capturing your own verified-correct implementation's output so a *later* accidental change gets caught):

```cpp
#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/tubescreamer/TubeScreamer.hpp"

TEST(Regression_Dsp, TubeScreamerFixedInputProducesStableSnapshot)
{
    SushiDSP::TubeScreamer node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_drive(0.6f);
    node.set_tone(0.4f);
    node.set_level(0.8f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.25f * std::sin(2.0f * static_cast<float>(M_PI) * 440.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    std::vector<float> last_block(256, 0.0f);
    for (int block = 0; block < 10; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
        last_block = output;
    }

    // Snapshot: these five values are this implementation's own verified output at these
    // indices of the 10th block, captured once and pinned here to catch future drift. If you
    // are implementing this step, RUN THE TEST FIRST with EXPECT_NEAR placeholders wide enough
    // to always pass (or print the values), read off what your implementation actually
    // produced at these five indices, and replace the placeholders below with those real
    // numbers before committing — do not invent plausible-looking numbers.
    EXPECT_NEAR(last_block[0], /* captured value */ 0.0f, 1e-4f);
    EXPECT_NEAR(last_block[64], /* captured value */ 0.0f, 1e-4f);
    EXPECT_NEAR(last_block[128], /* captured value */ 0.0f, 1e-4f);
    EXPECT_NEAR(last_block[192], /* captured value */ 0.0f, 1e-4f);
    EXPECT_NEAR(last_block[255], /* captured value */ 0.0f, 1e-4f);
}
```

**This is the one step in this plan with an intentional placeholder** — the snapshot values genuinely cannot be known until the real implementation runs, by the nature of a regression/snapshot test. Follow the in-test instructions: implement everything else first, run this test once with wide/no-op tolerances (or a temporary `std::printf` of `last_block[...]`) to capture real values, then replace the five `0.0f` placeholders with the actual captured floats and tighten `1e-4f` if the values are well clear of zero. Do not commit this file with the placeholders still in it — that would violate this plan's own "No Placeholders" rule; the placeholder is a scaffold for you to fill in during this step, not a deliverable.

- [ ] **Step 8: Wire the regression test and the `Regression_*` discovery call into `tests/CMakeLists.txt`**

Add `functional/regression/test_tube_screamer_regression.cpp` to `add_executable(sushidsp_tests ...)` under a new `# --- Regression ---` comment group, and add this second `gtest_discover_tests` call after the existing `unit` one:

```cmake
gtest_discover_tests(sushidsp_tests
    DISCOVERY_MODE PRE_TEST
    TEST_FILTER "Regression_*"
    PROPERTIES LABELS "regression"
)
```

Run: `sd test --suite all` (or `sd test` for the default `unit`-only run, then `sd test --suite regression` to confirm the new label filters correctly)
Expected: PASS — the regression test, correctly labeled and discoverable via `-L regression`.

- [ ] **Step 9: Wire `TubeScreamer` into the rig host — the "ships" proof**

Modify `apps/host/CMakeLists.txt` to link `SushiDSP::tubescreamer`:

```cmake
target_link_libraries(sushidsp_host PRIVATE SushiDSP::sushi_dsp_sdl2 SushiDSP::tubescreamer)
```

Modify `apps/host/main.cpp`: after `rig.add_node(std::make_unique<SushiDSP::SineToneNode>(440.0f));`, add a second node and connect them:

```cpp
    auto source_handle = rig.add_node(std::make_unique<SushiDSP::SineToneNode>(440.0f));
    auto tube_screamer_handle = rig.add_node(std::make_unique<SushiDSP::TubeScreamer>());
    rig.connect(source_handle, tube_screamer_handle);
```

(Replace the existing single `rig.add_node(...)` call and add `#include "SushiDSP/tubescreamer/TubeScreamer.hpp"` at the top.) Read `RigGraph::add_node`'s actual return type/`connect`'s signature from `include/SushiDSP/core/RigGraph.hpp` before writing this — it should already match `NodeHandle add_node(...)` / `void connect(NodeHandle, NodeHandle)` per D0 Task 7, but confirm before assuming.

To actually hear the effect, a human would set `TubeScreamer`'s drive/tone/level via a live setter call before/during playback — this plan does not add a text-based rig-editor UI (still deferred, per D1's own scope note), so `main.cpp` can call `set_drive(0.7f)` etc. once, right after constructing the node and before `rig.compile(...)`, as a hardcoded demonstration value. Update the `printf` message to mention both nodes are playing.

Run: `sd build`
Expected: SUCCESS — `sushidsp_host` builds with both nodes chained. Do NOT run the resulting executable as part of automated verification (same reasoning as D1: it opens real audio hardware and blocks). Run `sd test` once more to confirm the full suite (all of D0-D3) still passes.

- [ ] **Step 10: Commit**

```bash
git add modules/tubescreamer/CMakeLists.txt CMakeLists.txt tests/CMakeLists.txt \
        tests/functional/unit/test_rt_safety.cpp tests/functional/unit/test_tube_screamer.cpp \
        tests/functional/regression/test_tube_screamer_regression.cpp \
        apps/host/main.cpp apps/host/CMakeLists.txt
git commit -m "Add modules/tubescreamer target, regression suite, and rig-host wiring"
```

---

## Self-Review Notes

- **Spec coverage:** §6 (input buffer/highpass, op-amp clipping stage, tone stack, output level, `tube_screamer_descriptor()`, Drive/Tone/Level parameters) → Tasks 1-3. §9 D3 roadmap line ("op-amp clipping + tone stack; `tube_screamer_descriptor()`. Ships: TS standalone in the rig host, plausibility-tested (DC stability, THD sweep)") → Task 4 delivers all three: rig-host wiring (Step 9), DC stability (Step 6), THD sweep (Step 6).
- **Gap flagged explicitly (not silently dropped):** the tone stack is a fixed-crossover Biquad crossfade, not the full DK-method 2-port passive network §6 describes. This is a deliberate, documented scope simplification (this plan's Architecture section) — the crossfade reproduces the described behavior (a controllable low-pass/high-pass blend) without the materially larger engineering cost of a general linear-network DK-method solve, which nothing else in D3 needs and which would be better derived from JCM800's tone stack (D4, which the spec says genuinely needs a 3-port DK-method network) if a shared abstraction is ever worth building.
- **Placeholder scan:** the only placeholder is Task 4 Step 7's regression-snapshot values, which are placeholders *by the nature of a snapshot test* and the step's own text mandates filling them with real captured numbers before commit — this is not a "TBD" left for someone else, it's this task's own execution step.
- **Type consistency:** `TubeScreamerClippingStage`'s constructor/`process_sample`/`set_feedback_resistance`/`reset` signatures match between Task 2's declaration and Task 3's usage. `NodeDescriptor{id, name, parameters, parameter_count, ports, make}` field order and types match `include/SushiDSP/core/NodeDescriptor.hpp` as it exists after D0. `Oversampler8x::upsample`/`downsample` signatures match D2 Task 2's declaration (`const float* upsample(const float*, std::uint32_t)`, `void downsample(const float*, std::uint32_t, float*)`).
