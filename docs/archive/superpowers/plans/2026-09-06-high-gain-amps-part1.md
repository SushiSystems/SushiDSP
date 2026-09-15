# High-gain amplifiers, part 1 (DA0 + DA1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move the JCM800's four stage bricks into the core, add the three shared bricks and the three research dossiers the 5150, Mark IIC+ and VH4 modules need, so that part 2 can build the three amplifiers in parallel.

**Architecture:** DA0 is a pure file move plus one new presets header; the JCM800 golden files must stay bit-identical. DA1 adds `ResonanceControl`, `GraphicEqualizer` and `FmvToneStack` as core `circuits/` classes, each with its own gtest, and writes three dossiers under `docs/reference/research/`. Part 2 (DA2 modules, DA3 registration) is planned after the dossiers exist, because the module code is written from dossier values only.

**Tech Stack:** C++17, gtest, `SushiDSP/math/Biquad`, `tests/measure/SpiceCsv.hpp`, `sd build` / `sd test`.

**Spec:** `docs/superpowers/specs/2026-09-06-high-gain-amps-design.md`

## Global Constraints

- C++17 only; Allman braces; `snake_case` functions, `PascalCase` types, `kPascalCase` constants, trailing underscore on members; no abbreviations.
- Every `.hpp`/`.cpp`, tests included, opens with the 76-column license box copied verbatim from `include/SushiDSP/core/PortLayout.hpp` with only the filename line changed.
- `@brief` on every symbol; no comment explains *why*, a design document does.
- Core (`include/SushiDSP`, `src/`) depends on the C++17 standard library and SIMD intrinsics only; never on a product module, a device or a host.
- Anything reachable from `process()` is allocation-free, lock-free, syscall-free, throw-free.
- Builds and tests run only through `sd build` and `sd test`; never cmake or ctest directly.
- One logical change per commit; the commit message ends with the session's Co-Authored-By and Claude-Session trailers.
- Subagents do not run `sd`, `cmake`, `ninja` or `ctest`; the maintainer runs the build named at the end of each wave. Subagents syntax-check with `clang -fsyntax-only` through `build/compile_commands.json`.

---

## Wave DA0 (one task, serial)

### Task 1: Move the four stage bricks to `circuits/stages` and add `PowerTubePresets.hpp`

**Files:**
- Move: `modules/jcm800/include/SushiDSP/jcm800/{TriodeStage,PentodeStage,OutputTransformerStage,PowerSupplySagStage}.hpp` → `include/SushiDSP/circuits/stages/`
- Move: `modules/jcm800/src/{TriodeStage,PentodeStage,OutputTransformerStage,PowerSupplySagStage}.cpp` → `src/circuits/stages/`
- Create: `include/SushiDSP/circuits/stages/PowerTubePresets.hpp`
- Modify: `CMakeLists.txt:14-31` (add the four sources to `sushi_dsp`), `modules/jcm800/CMakeLists.txt` (remove them)
- Modify: every include of the four headers: `modules/jcm800/include/SushiDSP/jcm800/Jcm800.hpp`, `modules/jcm800/src/Jcm800.cpp`, the four moved `.cpp` files, `tests/benchmark/main.cpp`, `tests/functional/unit/test_{triode_stage,pentode_stage,output_transformer_stage,power_supply_sag_stage}.cpp`
- Modify: `modules/jcm800/src/Jcm800.cpp:54-60,228-229` (read EL34 constants from the presets header)
- Modify: `tests/CMakeLists.txt:65-69` (the four stage tests move from the Products block to a Circuits block, same file names)
- Modify: `docs/design/sushidsp.md` §5 "As built" (one sentence: the four stages live in `circuits/stages`), `docs/reference/CHANGELOG.md`

**Interfaces:**
- Produces: the same four classes at `SushiDSP/circuits/stages/<Name>.hpp`, namespace `SushiDSP`, unchanged signatures. Plus:

```cpp
// include/SushiDSP/circuits/stages/PowerTubePresets.hpp
namespace SushiDSP
{
    /// @brief The constructor constants one PentodeStage takes for a named power tube.
    struct PowerTubePreset
    {
        float source_resistance_ohms;
        float grid_leak_resistance_ohms;
        float cathode_bias_volts;
        float gain;
        float saturation_current_amps;
        float thermal_voltage_volts;
        float plate_saturation_drive;
    };

    /// @brief EL34 as the JCM800 2203 runs it; the values Jcm800.cpp used before DA0, verbatim.
    inline constexpr PowerTubePreset kEl34{ 10000.0f, 220000.0f, -20.0f, 8.0f, 1e-9f, 0.02585f, 0.05f };
}
```

`k6L6GC` and `kKt77` are **not** added in this task: their values come from the dossiers (Tasks 5–7) and are added by part 2. Adding guessed values here would violate the spec's "no value outside a dossier" rule.

- [ ] **Step 1: Move the files with git so history follows**

```bash
cd D:/Projects/sushidsp
mkdir -p include/SushiDSP/circuits/stages src/circuits/stages
for n in TriodeStage PentodeStage OutputTransformerStage PowerSupplySagStage; do
  git mv modules/jcm800/include/SushiDSP/jcm800/$n.hpp include/SushiDSP/circuits/stages/$n.hpp
  git mv modules/jcm800/src/$n.cpp src/circuits/stages/$n.cpp
done
```

- [ ] **Step 2: Rewrite every include path**

```bash
grep -rl 'SushiDSP/jcm800/\(TriodeStage\|PentodeStage\|OutputTransformerStage\|PowerSupplySagStage\)\.hpp' include src modules tests apps \
  | xargs sed -i 's#SushiDSP/jcm800/\(TriodeStage\|PentodeStage\|OutputTransformerStage\|PowerSupplySagStage\)\.hpp#SushiDSP/circuits/stages/\1.hpp#g'
grep -rn 'jcm800/\(Triode\|Pentode\|OutputTransformer\|PowerSupplySag\)' include src modules tests apps   # expect no output
```

Fix the `@file` line in each moved header and source so it names the file only (the name does not change). Update the doc comment on `TriodeStage` (`"Used for V1a, V1b, V2a, and the phase-inverter triode in the JCM800 model."`) to `"Used by the JCM800 and by every cascaded-triode amplifier module."`, and the one on `PentodeStage` (`"Task 7 instantiates two of these"`) to `"An amplifier module instantiates one per push-pull half."`

- [ ] **Step 3: Move the sources between CMake targets**

In `CMakeLists.txt`, inside `add_library(sushi_dsp STATIC ...)`, after `src/circuits/DiodeClipperCircuit.cpp` add:

```cmake
    src/circuits/stages/TriodeStage.cpp
    src/circuits/stages/PentodeStage.cpp
    src/circuits/stages/OutputTransformerStage.cpp
    src/circuits/stages/PowerSupplySagStage.cpp
```

In `modules/jcm800/CMakeLists.txt` the library becomes:

```cmake
add_library(jcm800 STATIC
    src/JcmToneStack.cpp
    src/Jcm800.cpp
)
```

In `tests/CMakeLists.txt` move the four lines `functional/unit/test_{triode_stage,pentode_stage,output_transformer_stage,power_supply_sag_stage}.cpp` out of `# --- Products ---` into a new `# --- Circuits: stages ---` block placed just above it.

- [ ] **Step 4: Write `PowerTubePresets.hpp`** with the license box, `@file`, and the struct and `kEl34` above.

- [ ] **Step 5: Make `Jcm800.cpp` read the preset**

Delete the seven `kPentode*` constants at lines 54–60 and add `#include "SushiDSP/circuits/stages/PowerTubePresets.hpp"`. Replace the two `el34_a_` / `el34_b_` initialisers with:

```cpp
        , el34_a_(kEl34.source_resistance_ohms, kEl34.grid_leak_resistance_ohms, kEl34.cathode_bias_volts, kEl34.gain, kEl34.saturation_current_amps, kEl34.thermal_voltage_volts, kEl34.plate_saturation_drive)
        , el34_b_(kEl34.source_resistance_ohms, kEl34.grid_leak_resistance_ohms, kEl34.cathode_bias_volts, kEl34.gain, kEl34.saturation_current_amps, kEl34.thermal_voltage_volts, kEl34.plate_saturation_drive)
```

Search `Jcm800.cpp` for any other use of a `kPentode*` name and repoint it to the preset field.

- [ ] **Step 6: Syntax-check without building**

```bash
for f in src/circuits/stages/*.cpp modules/jcm800/src/Jcm800.cpp; do clang -fsyntax-only -std=c++17 -Iinclude -Imodules/jcm800/include $f; done
```

Expected: no output. Paste the command and output in the report.

- [ ] **Step 7: Documentation**

`docs/design/sushidsp.md` §5, at the end of "As built in D6a", add one sentence: *"Since DA0 the four stage classes live in `include/SushiDSP/circuits/stages/`, owned by the core, so any amplifier module can use them; `modules/jcm800` keeps only `JcmToneStack` and `Jcm800`."* In `docs/reference/CHANGELOG.md` open a new section `## DA — High-gain amplifiers` above `## DR2` with a one-line pointer to the spec and the entry:

```
- 2026-09-06 — Moved `TriodeStage`, `PentodeStage`, `OutputTransformerStage`, `PowerSupplySagStage` to `include/SushiDSP/circuits/stages/`; added `PowerTubePresets.hpp` (`kEl34`).
```

- [ ] **Step 8: Maintainer builds and tests**

```
sd build && sd test
```

Expected: every test passes, including `Unit_Dsp.Jcm800*` regression snapshots against `tests/golden/jcm800_48k.f32` and `jcm800_96k.f32` unchanged. If a snapshot differs, the move changed behaviour and the task is not done.

- [ ] **Step 9: Commit**

```bash
git add -A include/SushiDSP/circuits/stages src/circuits/stages modules/jcm800 CMakeLists.txt tests/CMakeLists.txt tests docs/design/sushidsp.md docs/reference/CHANGELOG.md
git commit -m "DA0: move the four stage bricks to circuits/stages and read the EL34 constants from PowerTubePresets"
```

---

## Wave DA1 (six tasks, parallel, disjoint files)

Tasks 2–4 wait on Task 1 (they include `SushiDSP/math/Biquad.hpp` only, but they add tests to `tests/CMakeLists.txt`, which Task 1 also edits; running them after Task 1 keeps that file conflict-free, and each of them appends exactly one line inside `# --- Circuits: stages ---`). Tasks 5–7 wait on nothing.

### Task 2: `ResonanceControl`

**Files:**
- Create: `include/SushiDSP/circuits/stages/ResonanceControl.hpp`, `src/circuits/stages/ResonanceControl.cpp`
- Create: `tests/functional/unit/test_resonance_control.cpp`
- Modify: `CMakeLists.txt` (one source line), `tests/CMakeLists.txt` (one test line)

**Interfaces:**
- Consumes: `SushiDSP::Biquad::set_low_shelf(double sample_rate, double cutoff_hz, double gain_db, double q)`, `Biquad::process_sample(float)`, `Biquad::reset()` (`include/SushiDSP/math/Biquad.hpp`).
- Produces:

```cpp
namespace SushiDSP
{
    /// @brief The low-frequency shelf a Resonance or Deep knob places in a power section's
    /// negative-feedback path, mirrored to the forward path as a low shelf of the same corner.
    class ResonanceControl
    {
    public:
        /// @param sample_rate The rate the stage runs at, in hertz.
        /// @param corner_hz Shelf corner frequency in hertz.
        /// @param maximum_boost_db Boost at depth 1; depth 0 is flat and depth 0.5 is half the boost.
        ResonanceControl(double sample_rate, float corner_hz, float maximum_boost_db);
        /// @brief Sets the knob position.
        /// @param depth In [0, 1]; values outside are clamped.
        void set_depth(float depth);
        /// @brief Processes one sample. Audio-thread safe.
        float process_sample(float input);
        /// @brief Clears the filter state.
        void reset();
        /// @brief Returns the boost in decibels the current depth applies.
        float boost_db() const;
    private:
        double sample_rate_;
        float corner_hz_;
        float maximum_boost_db_;
        float depth_ = 0.0f;
        Biquad shelf_;
    };
}
```

Shelf Q is `0.7071067811865476` (`kShelfQ`), the JCM800 presence shelf's value.

- [ ] **Step 1: Write the failing tests**

```cpp
#include <cmath>
#include <gtest/gtest.h>
#include "SushiDSP/circuits/stages/ResonanceControl.hpp"

namespace
{
    float steady_gain_db(SushiDSP::ResonanceControl& control, double sample_rate, double frequency_hz)
    {
        const int settle = 48000, measure = 48000;
        double sum_squares_out = 0.0, sum_squares_in = 0.0;
        for (int i = 0; i < settle + measure; ++i)
        {
            const float x = static_cast<float>(std::sin(2.0 * M_PI * frequency_hz * i / sample_rate));
            const float y = control.process_sample(x);
            if (i >= settle) { sum_squares_out += y * y; sum_squares_in += x * x; }
        }
        return static_cast<float>(10.0 * std::log10(sum_squares_out / sum_squares_in));
    }
}

TEST(Unit_Dsp, ResonanceControlAtZeroDepthIsUnity)
{
    SushiDSP::ResonanceControl control(48000.0, 120.0f, 9.0f);
    control.set_depth(0.0f);
    EXPECT_NEAR(steady_gain_db(control, 48000.0, 40.0), 0.0f, 0.05f);
    EXPECT_NEAR(steady_gain_db(control, 48000.0, 4000.0), 0.0f, 0.05f);
}

TEST(Unit_Dsp, ResonanceControlAtFullDepthBoostsLowsByMaximumAndLeavesHighsAlone)
{
    SushiDSP::ResonanceControl control(48000.0, 120.0f, 9.0f);
    control.set_depth(1.0f);
    EXPECT_NEAR(steady_gain_db(control, 48000.0, 20.0), 9.0f, 0.5f);
    EXPECT_NEAR(steady_gain_db(control, 48000.0, 4000.0), 0.0f, 0.1f);
}

TEST(Unit_Dsp, ResonanceControlClampsDepthAndReportsBoost)
{
    SushiDSP::ResonanceControl control(48000.0, 120.0f, 9.0f);
    control.set_depth(2.0f);
    EXPECT_FLOAT_EQ(control.boost_db(), 9.0f);
    control.set_depth(-1.0f);
    EXPECT_FLOAT_EQ(control.boost_db(), 0.0f);
    control.set_depth(0.5f);
    EXPECT_FLOAT_EQ(control.boost_db(), 4.5f);
}
```

- [ ] **Step 2: Add `functional/unit/test_resonance_control.cpp` to `tests/CMakeLists.txt`** under `# --- Circuits: stages ---`, and `src/circuits/stages/ResonanceControl.cpp` to `sushi_dsp` in `CMakeLists.txt`.

- [ ] **Step 3: Implement**

```cpp
// ResonanceControl.cpp
#include "SushiDSP/circuits/stages/ResonanceControl.hpp"
#include <algorithm>

namespace SushiDSP
{
    namespace
    {
        constexpr double kShelfQ = 0.7071067811865476;
    }

    ResonanceControl::ResonanceControl(double sample_rate, float corner_hz, float maximum_boost_db)
        : sample_rate_(sample_rate), corner_hz_(corner_hz), maximum_boost_db_(maximum_boost_db)
    {
        set_depth(0.0f);
    }

    void ResonanceControl::set_depth(float depth)
    {
        depth_ = std::clamp(depth, 0.0f, 1.0f);
        shelf_.set_low_shelf(sample_rate_, corner_hz_, boost_db(), kShelfQ);
    }

    float ResonanceControl::process_sample(float input) { return shelf_.process_sample(input); }
    void ResonanceControl::reset() { shelf_.reset(); }
    float ResonanceControl::boost_db() const { return depth_ * maximum_boost_db_; }
}
```

- [ ] **Step 4: Syntax-check** `clang -fsyntax-only -std=c++17 -Iinclude src/circuits/stages/ResonanceControl.cpp` and the test with `-Ibuild/_deps/googletest-src/googletest/include` (or the include path `compile_commands.json` shows for another test). Paste output.
- [ ] **Step 5: Maintainer runs `sd build && sd test`**; expected the three new tests pass.
- [ ] **Step 6: Commit** `DA1: ResonanceControl, the Resonance/Deep low shelf in the feedback path`.

### Task 3: `GraphicEqualizer`

**Files:**
- Create: `include/SushiDSP/circuits/GraphicEqualizer.hpp`, `src/circuits/GraphicEqualizer.cpp`
- Create: `tests/functional/unit/test_graphic_equalizer.cpp`
- Modify: `CMakeLists.txt` (one source line), `tests/CMakeLists.txt` (one test line under `# --- Circuits: stages ---`)

**Interfaces:**
- Consumes: `Biquad::set_peaking(double sample_rate, double center_hz, double gain_db, double q)`.
- Produces:

```cpp
namespace SushiDSP
{
    /// @brief A bank of fixed-centre peaking bands, each ±maximum_gain_db, in series.
    class GraphicEqualizer
    {
    public:
        static constexpr std::size_t kMaximumBands = 8;
        /// @param sample_rate The rate the bank runs at, in hertz.
        /// @param centre_frequencies_hz Band centres, ascending; at most kMaximumBands, extras are ignored.
        /// @param band_q Peaking Q shared by every band.
        /// @param maximum_gain_db Gain at slider ±1.
        GraphicEqualizer(double sample_rate, std::initializer_list<float> centre_frequencies_hz, float band_q, float maximum_gain_db);
        /// @brief Sets one slider.
        /// @param band Band index; out-of-range indices are ignored.
        /// @param position In [-1, 1]; 0 is flat. Values outside are clamped.
        void set_band(std::size_t band, float position);
        /// @brief Processes one sample through every band in series. Audio-thread safe.
        float process_sample(float input);
        /// @brief Clears every band's state.
        void reset();
        /// @brief Returns the number of active bands.
        std::size_t band_count() const;
    private:
        double sample_rate_;
        float band_q_;
        float maximum_gain_db_;
        std::size_t band_count_ = 0;
        std::array<float, kMaximumBands> centres_hz_{};
        std::array<float, kMaximumBands> positions_{};
        std::array<Biquad, kMaximumBands> bands_{};
    };
}
```

- [ ] **Step 1: Write the failing tests** (reuse the `steady_gain_db` helper from Task 2 by copy; the helper takes a `GraphicEqualizer&`):

```cpp
TEST(Unit_Dsp, GraphicEqualizerHoldsFiveBandsAndIsFlatAtRest)
{
    SushiDSP::GraphicEqualizer equalizer(48000.0, { 80.0f, 240.0f, 750.0f, 2200.0f, 6600.0f }, 1.4f, 12.0f);
    EXPECT_EQ(equalizer.band_count(), 5u);
    EXPECT_NEAR(steady_gain_db(equalizer, 48000.0, 750.0), 0.0f, 0.05f);
}

TEST(Unit_Dsp, GraphicEqualizerBoostsOneBandAtItsCentre)
{
    SushiDSP::GraphicEqualizer equalizer(48000.0, { 80.0f, 240.0f, 750.0f, 2200.0f, 6600.0f }, 1.4f, 12.0f);
    equalizer.set_band(2, 1.0f);
    EXPECT_NEAR(steady_gain_db(equalizer, 48000.0, 750.0), 12.0f, 0.3f);
    EXPECT_NEAR(steady_gain_db(equalizer, 48000.0, 80.0), 0.0f, 0.5f);
}

TEST(Unit_Dsp, GraphicEqualizerCutsOneBandAndClampsSliders)
{
    SushiDSP::GraphicEqualizer equalizer(48000.0, { 80.0f, 240.0f, 750.0f, 2200.0f, 6600.0f }, 1.4f, 12.0f);
    equalizer.set_band(0, -3.0f);
    EXPECT_NEAR(steady_gain_db(equalizer, 48000.0, 80.0), -12.0f, 0.3f);
    equalizer.set_band(7, 1.0f);
    EXPECT_EQ(equalizer.band_count(), 5u);
}
```

- [ ] **Step 2: Register the test and the source in the two CMake lists.**
- [ ] **Step 3: Implement** with `std::clamp` on the position, `set_peaking(sample_rate_, centres_hz_[band], positions_[band] * maximum_gain_db_, band_q_)` on each `set_band`, series `process_sample` over `band_count_` bands, constructor copying at most `kMaximumBands` centres and setting every band flat.
- [ ] **Step 4: Syntax-check** and paste output.
- [ ] **Step 5: Maintainer runs `sd build && sd test`.**
- [ ] **Step 6: Commit** `DA1: GraphicEqualizer, a fixed-centre peaking bank for the Mark IIC+`.

### Task 4: `FmvToneStack`

**Files:**
- Create: `include/SushiDSP/circuits/FmvToneStack.hpp`, `src/circuits/FmvToneStack.cpp`
- Create: `tests/functional/unit/test_fmv_tone_stack.cpp`
- Modify: `CMakeLists.txt` (one source line), `tests/CMakeLists.txt` (one test line under `# --- Circuits: stages ---`)
- Do **not** modify `modules/jcm800/*`. Whether `JcmToneStack` later delegates to this class is decided from this task's report, in a separate task.

**Interfaces:**
- Consumes: `tests/measure/SpiceCsv.hpp` (`AcSweep`, loaded by the helper used at `tests/functional/unit/test_dk_triode_stage.cpp:173`), `docs/reference/spice/jcm_fmv/jcm_fmv_ac.csv` (MID stepped 0.1 / 0.5 / 0.9 with BASS = TREBLE = 0.5, MASTER = 1; probe at the master wiper, which at MASTER = 1 is the stack's output).
- Produces:

```cpp
namespace SushiDSP
{
    /// @brief Component values of a Fender/Marshall/Vox passive tone stack.
    struct FmvComponents
    {
        float treble_capacitor_farads;   ///< C1, the series cap into the treble pot.
        float bass_capacitor_farads;     ///< C2, into the bass pot.
        float middle_capacitor_farads;   ///< C3, into the middle pot.
        float slope_resistor_ohms;       ///< R1, the slope resistor from input to the bass/mid node.
        float bass_potentiometer_ohms;   ///< R2, bass pot as a rheostat.
        float middle_potentiometer_ohms; ///< R3, middle pot as a rheostat.
        float treble_potentiometer_ohms; ///< R4, treble pot end to end.
    };

    /// @brief The JCM800 2203's values: 470 pF, 22 nF, 22 nF, 33 kΩ, 1 MΩ, 25 kΩ, 220 kΩ.
    inline constexpr FmvComponents kJcm2203ToneStack{ 470e-12f, 22e-9f, 22e-9f, 33e3f, 1e6f, 25e3f, 220e3f };

    /// @brief The passive FMV stack as one third-order section, coefficients from Yeh & Smith
    /// (DAFx 2006) discretised with the bilinear transform, recomputed on every knob move.
    class FmvToneStack
    {
    public:
        FmvToneStack(double sample_rate, const FmvComponents& components);
        /// @param normalized Bass pot position in [0, 1].
        void set_bass(float normalized);
        /// @param normalized Middle pot position in [0, 1].
        void set_middle(float normalized);
        /// @param normalized Treble pot position in [0, 1].
        void set_treble(float normalized);
        /// @brief Processes one sample. Audio-thread safe.
        float process_sample(float input);
        /// @brief Clears the delay line.
        void reset();
    private:
        void recompute();
        double sample_rate_;
        FmvComponents components_;
        float bass_ = 0.5f, middle_ = 0.5f, treble_ = 0.5f;
        double b_[4]{}, a_[4]{};   ///< Discrete numerator and denominator, a_[0] normalised to 1.
        double state_x_[3]{}, state_y_[3]{};
    };
}
```

Pot taper: linear position maps straight to the fraction (`l = bass`, `m = middle`, `t = treble`); a `float` position of exactly 0 for the middle rheostat is clamped to `1e-4` so `R3·m` never reaches zero. Audio taper is a per-amplifier concern the module applies before calling the stack.

- [ ] **Step 1: Write the failing tests.** The SPICE test is the authority; the analytic identities catch a transcription slip early.

```cpp
#include <cmath>
#include <string>
#include <gtest/gtest.h>
#include "SushiDSP/circuits/FmvToneStack.hpp"
#include "measure/SpiceCsv.hpp"

namespace
{
    // Same steady_gain_db helper as test_resonance_control.cpp, taking FmvToneStack&.

    std::string spice_path(const std::string& block, const std::string& kind)
    {
        return std::string(SUSHIDSP_SPICE_DIR) + "/" + block + "/" + block + "_" + kind + ".csv";
    }
}

TEST(Unit_Dsp, FmvToneStackMatchesSpiceMidSweepWithinHalfDecibel)
{
    const auto sweep = SushiDSP::measure::load_ac_sweep(spice_path("jcm_fmv", "ac"));
    const float mid_positions[3] = { 0.1f, 0.5f, 0.9f };
    const double sample_rate = 192000.0;   // Keeps bilinear warping below 0.05 dB at 20 kHz.
    for (int setting = 0; setting < 3; ++setting)
    {
        SushiDSP::FmvToneStack stack(sample_rate, SushiDSP::kJcm2203ToneStack);
        stack.set_bass(0.5f); stack.set_treble(0.5f); stack.set_middle(mid_positions[setting]);
        for (std::size_t row = 0; row < sweep.frequency_hz.size(); ++row)
        {
            if (sweep.setting[row] != setting) continue;
            const double f = sweep.frequency_hz[row];
            if (f < 20.0 || f > 20000.0) continue;
            stack.reset();
            EXPECT_NEAR(steady_gain_db(stack, sample_rate, f), sweep.magnitude_db[row], 0.5)
                << "setting " << setting << " at " << f << " Hz";
        }
    }
}

TEST(Unit_Dsp, FmvToneStackMidScoopDeepensAsMiddleCloses)
{
    SushiDSP::FmvToneStack open(48000.0, SushiDSP::kJcm2203ToneStack), closed(48000.0, SushiDSP::kJcm2203ToneStack);
    open.set_middle(0.9f); closed.set_middle(0.1f);
    EXPECT_LT(steady_gain_db(closed, 48000.0, 500.0), steady_gain_db(open, 48000.0, 500.0) - 6.0f);
}

TEST(Unit_Dsp, FmvToneStackIsStableAtEveryKnobExtreme)
{
    const float positions[2] = { 0.0f, 1.0f };
    for (float b : positions) for (float m : positions) for (float t : positions)
    {
        SushiDSP::FmvToneStack stack(48000.0, SushiDSP::kJcm2203ToneStack);
        stack.set_bass(b); stack.set_middle(m); stack.set_treble(t);
        float y = 0.0f;
        for (int i = 0; i < 48000; ++i) y = stack.process_sample(i == 0 ? 1.0f : 0.0f);
        EXPECT_LT(std::fabs(y), 1e-6f) << b << m << t;
    }
}
```

If `SpiceCsv.hpp` names its loader differently from `load_ac_sweep`, use the name it has; read the header first.

- [ ] **Step 2: Register the test and the source in the two CMake lists.**

- [ ] **Step 3: Implement the continuous-time coefficients.** With `C1, C2, C3, R1, R2, R3, R4` from `FmvComponents` and `l, m, t` the three positions, the transfer function is `H(s) = (b1 s + b2 s² + b3 s³) / (1 + a1 s + a2 s² + a3 s³)` with

```
b1 = t·C1·R1 + m·C3·R3 + l·(C1·R2 + C2·R2) + (C1·R3 + C2·R3)
b2 = t·(C1·C2·R1·R4 + C1·C3·R1·R4) − m²·(C1·C3·R3² + C2·C3·R3²)
   + m·(C1·C3·R1·R3 + C1·C3·R3² + C2·C3·R3²)
   + l·(C1·C2·R1·R2 + C1·C2·R2·R4 + C1·C3·R2·R4)
   + l·m·(C1·C3·R2·R3 + C2·C3·R2·R3)
   + (C1·C2·R1·R3 + C1·C2·R3·R4 + C1·C3·R3·R4)
b3 = l·m·(C1·C2·C3·R1·R2·R3 + C1·C2·C3·R2·R3·R4) − m²·(C1·C2·C3·R1·R3² + C1·C2·C3·R3²·R4)
   + m·(C1·C2·C3·R1·R3² + C1·C2·C3·R3²·R4) + t·C1·C2·C3·R1·R3·R4
   − t·m·C1·C2·C3·R1·R3·R4 + t·l·C1·C2·C3·R1·R2·R4
a1 = (C1·R1 + C1·R3 + C2·R3 + C2·R4 + C3·R4) + m·C3·R3 + l·(C1·R2 + C2·R2)
a2 = m·(C1·C3·R1·R3 − C2·C3·R3·R4 + C1·C3·R3² + C2·C3·R3²)
   + l·m·(C1·C3·R2·R3 + C2·C3·R2·R3) − m²·(C1·C3·R3² + C2·C3·R3²)
   + l·(C1·C2·R2·R4 + C1·C2·R1·R2 + C1·C3·R2·R4 + C2·C3·R2·R4)
   + (C1·C2·R1·R4 + C1·C3·R1·R4 + C1·C2·R3·R4 + C1·C2·R1·R3 + C1·C3·R3·R4 + C2·C3·R3·R4)
a3 = l·m·(C1·C2·C3·R1·R2·R3 + C1·C2·C3·R2·R3·R4) − m²·(C1·C2·C3·R1·R3² + C1·C2·C3·R3²·R4)
   + m·(C1·C2·C3·R3²·R4 + C1·C2·C3·R1·R3² − C1·C2·C3·R1·R3·R4)
   + l·C1·C2·C3·R1·R2·R4 + C1·C2·C3·R1·R3·R4
```

These are transcribed from D. T. Yeh and J. O. Smith, "Discretization of the '59 Fender Bassman Tone Stack", DAFx-06, and **the SPICE test in Step 1 is the check on the transcription**, not this text. If the SPICE test fails by a consistent shape at one knob, re-derive the offending coefficient from the netlist in `docs/reference/spice/jcm_fmv/README.md` (nodal analysis, three unknown node voltages) rather than tuning a term to fit.

- [ ] **Step 4: Discretise with the bilinear transform** `s = c·(1 − z⁻¹)/(1 + z⁻¹)`, `c = 2·sample_rate`, in `double`, normalise so `a_[0] = 1`, and run the section as transposed direct form II or direct form I in `double` (the state arrays above are direct form I). Prewarping is not applied; the 192 kHz test rate keeps warping error below the tolerance.

- [ ] **Step 5: Syntax-check** and paste output.
- [ ] **Step 6: Maintainer runs `sd build && sd test`.** If `FmvToneStackMatchesSpiceMidSweepWithinHalfDecibel` fails, report the worst frequency and setting with both values; do not widen the tolerance.
- [ ] **Step 7: Report** whether `JcmToneStack`'s three-biquad approximation is within 1 dB of this class across the MID sweep (compute once in the test binary or a scratch program, paste the numbers). The maintainer decides from that whether `JcmToneStack` delegates to `FmvToneStack` in a later task.
- [ ] **Step 8: Commit** `DA1: FmvToneStack, the passive FMV stack as one third-order section nulled against jcm_fmv`.

### Tasks 5, 6, 7: Research dossiers

One task per amplifier, same shape, disjoint files, no code. Model for the dispatch: `sonnet` is not enough here; these run on `opus` because a wrong value propagates into a module.

**Files:**
- Task 5 creates `docs/reference/research/2026-09-06-peavey-5150-design-dossier.md`
- Task 6 creates `docs/reference/research/2026-09-06-mesa-mark-iic-plus-design-dossier.md`
- Task 7 creates `docs/reference/research/2026-09-06-diezel-vh4-design-dossier.md`
- Each may add `docs/reference/spice/<prefix>_<block>/` directories (`p5150_`, `mk2c_`, `vh4_`) containing `<block>.asc` and `README.md` in the `jcm_fmv` shape; no CSVs (LTspice is not on this machine).

**Interfaces:**
- Produces, for part 2: the numbers. Every dossier has these sections in this order, mirroring `docs/reference/research/2026-08-26-boss-dd3-design-dossier.md`:

1. **Sources.** Each schematic or trace with its origin, revision, date, and how far it was trusted. A value that appears in only one uncorroborated source is marked `[single source]`.
2. **Block overview.** ASCII signal-path diagram in the DD-3 dossier's style, one line per stage.
3. **Preamp, stage by stage.** For each triode: plate resistor, cathode resistor and bypass capacitor, grid leak, grid stopper, coupling capacitor to the next stage, interstage divider, and the resulting DC operating point (plate voltage, cathode voltage, bias). State the supply node voltage for each stage.
4. **Tone network.** Topology, every component value, where in the chain it sits, pot values and tapers. For the IIC+, the graphic EQ: inductor/capacitor values or the active band topology, Q and gain range per band.
5. **Power section.** Phase inverter type and values, power tubes and their bias (fixed or cathode, idle current), screen resistors, output transformer primary impedance and turns ratio, negative-feedback network with presence and resonance/deep values.
6. **Calibration targets.** Measured or documented figures a test can assert: input sensitivity, preamp gain at a stated knob position, THD at a stated drive, frequency response of the tone stack at flat, power-section output at clipping. Each with its source.
7. **Proposed `PowerTubePreset`.** The seven `PentodeStage` constructor values for this amplifier's power tube, with the reasoning from the datasheet curves and the bias point; the field order is `source_resistance_ohms, grid_leak_resistance_ohms, cathode_bias_volts, gain, saturation_current_amps, thermal_voltage_volts, plate_saturation_drive`, and the EL34 preset `kEl34{ 10000, 220000, -20, 8, 1e-9, 0.02585, 0.05 }` is the reference point to argue from.
8. **Open questions.** Everything not established, each with what would settle it.

Task 7 (VH4) adds a **verdict** paragraph at the top: whether channel 3's preamp topology and component values are established well enough to write a white-box module, or whether the module is a documented approximation. The spec's risk note commits the maintainer to reading this before part 2 is planned.

- [ ] **Step 1: Gather sources** with web search (schematics on manufacturer sites, service manuals, the tube-amp schematic archives, published analyses). Record every URL and access date in §1.
- [ ] **Step 2: Write §§1–8** as above. No value without a source line. Prose through the humanizer rules in English.
- [ ] **Step 3: Cross-check** each stage's DC operating point by hand from the values (Ohm's law on the cathode resistor against the plate load line) and note the result in §3; a stage whose numbers do not close is flagged, not smoothed over.
- [ ] **Step 4: Commit** the dossier alone: `DA1: <amp> design dossier`.

---

## Wave summary

| Wave | Tasks | Files | Waits on | Build the maintainer runs |
| --- | --- | --- | --- | --- |
| DA0 | 1 | `circuits/stages/*`, `modules/jcm800/*`, root and `tests` CMake, spec §5, changelog | none | `sd build && sd test`; JCM800 goldens unchanged |
| DA1 | 2, 3, 4 | one class + one test each, one line each in root and `tests` CMake | 1 | `sd build && sd test` once for all three |
| DA1 | 5, 6, 7 | one dossier each (+ optional `.asc` decks) | none | none |

Tasks 5–7 start at the same time as Task 1. Tasks 2–4 start when Task 1's build is green. Part 2's plan is written when Tasks 4–7 have reported.

## Self-review

- Spec §3 → Task 1. §4 → Tasks 2–4. §5 → Tasks 5–7. §6, §7 → part 2, stated. §9 first bullet → Task 1 Step 8.
- The `k6L6GC` / `kKt77` presets the spec names in §3 are deferred to part 2 with the reason given in Task 1.
- Names used across tasks: `PowerTubePreset`, `kEl34`, `ResonanceControl`, `GraphicEqualizer`, `FmvToneStack`, `FmvComponents`, `kJcm2203ToneStack`; each defined where it is first produced.
