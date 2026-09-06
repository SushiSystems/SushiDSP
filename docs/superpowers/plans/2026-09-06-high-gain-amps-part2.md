# High-gain amplifiers, part 2 (DA2 + DA3) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the Peavey 5150, Mesa/Boogie Mark IIC+ and Diezel VH4 product modules from their dossiers on the DA0/DA1 bricks, register them, give them panels, and document them.

**Architecture:** Three sibling modules shaped like `modules/jcm800`: one `IAudioNode` per amplifier owning `TriodeStage` / `PentodeStage` / `OutputTransformerStage` / `PowerSupplySagStage` instances from `circuits/stages`, a per-amplifier tone-stack class wrapping `FmvToneStack`, `Biquad`s for every cathode shelf, coupling capacitor and Miller corner, one 8× oversampled region from the first triode to the sag stage, and the JCM800's cabinet-convolution slot. Every constant traces to a dossier line. A short pre-wave fixes three brick defects part 1's reviews found (the rheostat tone-stack form, a preset-taking `PentodeStage` constructor, per-band EQ Q) before any module depends on them.

**Tech Stack:** C++17, gtest, `SushiDSP/math` (Biquad, Oversampler8x, SmoothedParameter, StreamingConvolution, RtSwapSlot), `tests/measure` (Thd, FrequencyResponse), `tests/golden/GoldenFile.hpp`, `sd build` / `sd test`.

**Spec:** `docs/superpowers/specs/2026-09-06-high-gain-amps-design.md`. Dossiers: `docs/reference/research/2026-09-06-peavey-5150-design-dossier.md`, `...-mesa-mark-iic-plus-design-dossier.md`, `...-diezel-vh4-design-dossier.md`. The JCM800 module (`modules/jcm800`) is the shape to mirror; read `Jcm800.hpp`, `Jcm800.cpp` and `tests/functional/unit/test_jcm800.cpp` before writing a module.

## Global Constraints

- C++17; Allman braces; `snake_case` functions, `PascalCase` types, `kPascalCase` constants, trailing underscore on members; no abbreviations.
- Every `.hpp`/`.cpp`, tests included, opens with the 76-column license box copied verbatim from `include/SushiDSP/core/PortLayout.hpp` with only the filename line changed. `@brief` on every symbol. No comment states a reason; a design document or a dossier does, and the comment cites it by path and section.
- A product module depends only on `SushiDSP::sushi_dsp`; never on another product module, a device or a host.
- Everything reachable from `process()` is allocation-free, lock-free, syscall-free, throw-free, and proven by an `operator new` trap case in `tests/functional/unit/test_rt_safety.cpp`.
- **No value outside a dossier.** Every resistor, capacitor, gain, corner, bias and preset field in module code carries a comment naming the dossier section it came from. Where a dossier says "not established" or "modelling choice", the code says so in the same words and the module README lists it.
- Parameter order in a `NodeDescriptor` is a contract: appended, never reordered.
- Builds and tests only through `sd build` and `sd test`; subagents run neither and never edit a CMake file. The controller edits CMake and regenerates golden files.
- Commit messages end with the session's Co-Authored-By and Claude-Session trailers.

---

## Wave P2-0: brick fixes (three tasks, parallel, disjoint files)

### Task 1: `FmvToneStack` rheostat form

**Files:**
- Modify: `include/SushiDSP/circuits/FmvToneStack.hpp`, `src/circuits/FmvToneStack.cpp`
- Modify: `tests/functional/unit/test_fmv_tone_stack.cpp`
- Modify: `docs/reference/spice/jcm_fmv/README.md` (the "rheostat approximation" judgment call becomes the model's own form), `docs/superpowers/specs/2026-09-06-high-gain-amps-design.md` §4 (the F1 sentence)

**Interfaces:**
- Consumes: the class as it stands (wiper form, Yeh & Smith coefficients, `FmvComponents` seven fields, `kJcm2203ToneStack`).
- Produces: the same public interface; the model becomes the **rheostat-wired middle pot** (C3 lands on the top of the middle rheostat, `R3·m` to ground), which is how the 2203 (`jcm_fmv` deck), the 5150 (dossier §4: VR4 50 kΩ "as a rheostat"), the IIC+ (§4.1: "Mid pot 10 K rheostat to ground") and the VH4 (§4) are wired. The `1e-4` middle clamp is removed if the rheostat form is finite at `m = 0`; the test proves whichever holds.

- [ ] **Step 1: Derive the rheostat coefficients.** Nodal analysis of the deck's netlist in `docs/reference/spice/jcm_fmv/README.md` (nodes `t1`, `out`, `n2`, `b1`, `n4`; C3 from `n2` to `n4`; `Rmid = R3·m` from `n4` to ground; `Rbass = R2·l` from `b1` to `n4`; `Rtreb_a = R4·(1−t)` from `t1` to `out`, `Rtreb_b = R4·t` from `out` to `b1`). Do it in sympy in your scratch directory, collect the third-order numerator and denominator in `s` as polynomials in `l, m, t`, and paste the six coefficient expressions into the report. Check them against the wiper-form coefficients in the current `recompute()`: they must differ only in the terms that carry `m`.
- [ ] **Step 2: Write the failing test change.** Tighten `FmvToneStackMatchesSpiceMidSweepWithinHalfDecibel` to `0.05` dB (the residual reported in part 1, 0.124 dB, was the wiper/rheostat difference; a correct rheostat form nulls the deck to LTspice's own precision). Add:

```cpp
TEST(Unit_Dsp, FmvToneStackAtEveryKnobZeroIsFiniteAndQuiet)
{
    SushiDSP::FmvToneStack stack(48000.0, SushiDSP::kJcm2203ToneStack);
    stack.set_bass(0.0f); stack.set_middle(0.0f); stack.set_treble(1.0f);
    float peak = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        const float y = stack.process_sample(static_cast<float>(std::sin(2.0 * kPi * 500.0 * i / 48000.0)));
        ASSERT_TRUE(std::isfinite(y));
        peak = std::max(peak, std::fabs(y));
    }
    EXPECT_LT(20.0f * std::log10(std::max(peak, 1e-12f)), -60.0f);
}
```

The final review measured the rheostat form at −141 dB and the wiper form at −23 dB at this corner, so the threshold separates the two forms with margin.

- [ ] **Step 3: Implement** the new coefficient expressions in `recompute()`, remove the `1e-4` clamp if Step 1 shows the denominator stays non-zero at `m = 0` (paste the `m = 0` limit of `a3` in the report), update the header's `@brief` to say "rheostat-wired middle pot", update the two documents named above.
- [ ] **Step 4: Syntax-check** via `build/compile_commands.json`'s clang++ with `-fsyntax-only`; paste output.
- [ ] **Step 5: Controller runs `sd build && sd test`.** The 0.05 dB null must pass at all three MID settings; report the worst deviation per setting.
- [ ] **Step 6: Commit** `DA2: FmvToneStack models the rheostat-wired middle pot; nulls jcm_fmv to 0.05 dB`.

### Task 2: `PentodeStage(const PowerTubePreset&)` and the four amplifier presets

**Files:**
- Modify: `include/SushiDSP/circuits/stages/PentodeStage.hpp`, `src/circuits/stages/PentodeStage.cpp`
- Modify: `include/SushiDSP/circuits/stages/PowerTubePresets.hpp`
- Modify: `tests/functional/unit/test_pentode_stage.cpp`

**Interfaces:**
- Produces:

```cpp
/// @brief Builds the stage from a named preset; identical to the seven-argument constructor.
explicit PentodeStage(const PowerTubePreset& preset);
```

and in `PowerTubePresets.hpp`, after `kEl34`, each with a `@brief` naming its dossier section and the field-by-field standing (fact / modelling choice) the dossier gives:

```cpp
/// @brief Peavey 5150 lead, 6L6GC quad, fixed bias; 5150 dossier §7 (gain and drive are modelling choices).
inline constexpr PowerTubePreset kPeavey5150Power6L6GC{ 39000.0f, 220000.0f, -55.0f, 6.0f, 1e-9f, 0.02585f, 0.048f };
/// @brief Mesa Mark IIC+ Simul-Class inner pair, 6L6GC pentode; IIC+ dossier §7.2.
inline constexpr PowerTubePreset kMesaMark2cPower6L6GC{ 37600.0f, 220000.0f, -45.0f, 5.0f, 1e-9f, 0.02585f, 0.049f };
/// @brief Mesa Mark IIC+ Simul-Class outer pair, EL34 triode-strapped; IIC+ dossier §7.3.
inline constexpr PowerTubePreset kMesaMark2cPowerEl34Triode{ 37600.0f, 220000.0f, -45.0f, 5.2f, 1e-9f, 0.02585f, 0.069f };
/// @brief Diezel VH4, EL34 quad at 500 V; VH4 dossier §7 EL34 row (−36 V) with the 500/450 V drive scaling.
inline constexpr PowerTubePreset kDiezelVh4PowerEl34{ 10000.0f, 220000.0f, -36.0f, 8.0f, 1e-9f, 0.02585f, 0.045f };
```

The VH4 preset takes the dossier's KT77 proposal (`{ 10000, 220000, -43, 8, 1e-9, 0.02585, 0.045 }`) with `cathode_bias_volts` replaced by the EL34 datasheet row the same section quotes (−36 V at 500 V), per the maintainer's EL34 decision; every other field is the dossier's inherited modelling choice and the `@brief` says so.

- [ ] **Step 1: Failing test:**

```cpp
TEST(Unit_Dsp, PentodeStagePresetConstructorMatchesTheSevenArgumentOne)
{
    SushiDSP::PentodeStage from_preset(SushiDSP::kEl34);
    SushiDSP::PentodeStage from_fields(10000.0f, 220000.0f, -20.0f, 8.0f, 1e-9f, 0.02585f, 0.05f);
    for (int i = 0; i < 2000; ++i)
    {
        const float x = 0.5f * static_cast<float>(std::sin(2.0 * kPi * 440.0 * i / 48000.0));
        EXPECT_FLOAT_EQ(from_preset.process_sample(x), from_fields.process_sample(x));
    }
}
```

- [ ] **Step 2: Implement** the delegating constructor (`: PentodeStage(preset.source_resistance_ohms, ...)`), add the four presets. Switch `Jcm800.cpp`'s two `el34_a_`/`el34_b_` initialisers to `el34_a_(kEl34)`; JCM800 goldens must stay bit-identical.
- [ ] **Step 3: Syntax-check**, paste. **Step 4: Controller builds and tests.** **Step 5: Commit** `DA2: PentodeStage takes a PowerTubePreset; the four amplifier presets from the dossiers`.

### Task 3: `GraphicEqualizer` per-band Q

**Files:**
- Modify: `include/SushiDSP/circuits/GraphicEqualizer.hpp`, `src/circuits/GraphicEqualizer.cpp`, `tests/functional/unit/test_graphic_equalizer.cpp`

**Interfaces:**
- Produces a second constructor, sibling-shaped:

```cpp
/// @param band_q One Q per band, same order as the centres; extras are ignored.
GraphicEqualizer(double sample_rate, std::initializer_list<float> centre_frequencies_hz,
                 std::initializer_list<float> band_q, float maximum_gain_db);
```

and `std::array<float, kMaximumBands> band_q_` replacing the single `band_q_`. The existing constructor fills every band with the shared Q.

- [ ] **Step 1: Failing test:**

```cpp
TEST(Unit_Dsp, GraphicEqualizerPerBandQNarrowsOneBandOnly)
{
    SushiDSP::GraphicEqualizer equalizer(48000.0, { 71.5f, 371.8f, 979.5f, 1576.0f, 4823.0f }, { 1.434f, 1.938f, 1.571f, 0.673f, 1.0f }, 17.0f);
    equalizer.set_band(1, 1.0f);
    const float at_centre = steady_gain_db(equalizer, 48000.0, 371.8);
    const float an_octave_up = steady_gain_db(equalizer, 48000.0, 743.6);
    EXPECT_NEAR(at_centre, 17.0f, 0.3f);
    EXPECT_LT(an_octave_up, at_centre - 8.0f);
}
```

- [ ] **Step 2: Implement.** **Step 3: Syntax-check.** **Step 4: Controller builds and tests.** **Step 5: Commit** `DA2: GraphicEqualizer takes one Q per band`.

---

## Wave P2-1: the three modules (three tasks, parallel, disjoint files)

Common shape for Tasks 4, 5, 6. Each module directory:

```
modules/<name>/CMakeLists.txt                       controller writes it, copying modules/jcm800/CMakeLists.txt
modules/<name>/README.md                            the module's facts: channel, stage order, every approximation
modules/<name>/include/SushiDSP/<name>/<Amp>.hpp    the IAudioNode and <amp>_descriptor()
modules/<name>/include/SushiDSP/<name>/<Amp>ToneStack.hpp
modules/<name>/src/<Amp>.cpp
modules/<name>/src/<Amp>ToneStack.cpp
tests/functional/unit/test_<name>.cpp
tests/functional/regression/test_<name>_regression.cpp   golden at tests/golden/<name>_48k.f32 and _96k.f32
```

Common node interface, mirroring `Jcm800.hpp` exactly in method names where the control exists (`prepare`, `process`, `ports`, `reset`, `latency_samples`, `type_id`, `set_cabinet`, `reclaim_cabinets`, `has_cabinet`, `set_input_trim`, `set_output`, `get_parameter`/`set_parameter` by id and by index, `tone_stack_is_dirty`), plus one `set_<knob>(float)` per front-panel control. `kOversamplingFactor = 8`. Input Trim ±24 dB and Output −60…+12 dB with the JCM800's constants. The descriptor's parameter array lists the front-panel knobs in panel order, then `input_trim`, then `output`.

Common stage mapping, the one the JCM800 uses. A dossier triode stage becomes:

| Dossier fact | Brick | Value source |
| --- | --- | --- |
| Small-signal gain at the solved operating point | `TriodeStage(source_resistance, grid_leak, cathode_bias, gain, 1e-9, 0.02585)` | gain from the dossier's per-stage gain table; source resistance = the driving network's Thevenin impedance the dossier gives (plate load in parallel with the divider) else the plate load; grid leak and cathode bias from the stage's own table row |
| Partially bypassed cathode (zero/pole) | one `Biquad` low shelf with the dossier's zero and pole (gain = 20·log10(pole/zero) of loss below the zero) | dossier §3 or §6 row |
| Coupling capacitor into the next grid | `Biquad::set_high_pass` first-order (Q 0.7071) at `1/(2π·C·R_load)` | R and C from the dossier; compute, paste the corner in the README |
| Interstage divider | a `constexpr float` attenuation | dossier ratio |
| Grid input capacitance | `Biquad::set_one_pole_low_pass` at the JCM800's `miller_corner_hz(gain)` with the dossier's plate load | `Jcm800.cpp` Miller constants, cited |
| Unbypassed cathode | folded into the gain figure the dossier already computed | dossier says which |

A cathode follower (5150 V3B, VH4 V3b) is unity gain into a `TriodeStage` clip at its grid with `gain = 1` is **not** right (the follower's plate has no resistor); model it as a pass-through with the dossier's output high-pass and the grid-conduction clip only: `TriodeStage` with `gain = -1.0f` (inverting twice cancels) is rejected; use the dossier's gain (0.986 for the VH4) as a scalar and state in the README that follower compression is not modelled.

Tone stack per amplifier: `<Amp>ToneStack` owns one `FmvToneStack` built from the dossier's `FmvComponents` and the passive elements around it the closed form does not contain (a series input resistor, a shunt to ground at the input node, a shift resistor across the treble pot). Each such element is a documented approximation: a series input resistor into the stack's input impedance becomes a frequency-independent attenuation at the dossier's stated input impedance, a shunt RC becomes a `Biquad` shelf, a 10 MΩ across a 250 kΩ pot is ignored (4 % of the pot). The README lists each with its arithmetic.

Common tests per module (names with the amplifier's prefix), copied from `test_jcm800.cpp` with the values changed:
`ProcessesABlockWithoutCrashingOrProducingNonFiniteOutput`, `PortsAreMonoInMonoOut`, `DescriptorExposes<N>ParametersInContractOrder`, `IndexedParameterSetMatchesTheDescriptorOrder`, `DecibelParametersClampToTheirDescriptorRange`, `GenericParameterRoundTrip`, `UnknownParameterIsSafeNoOp`, `IsDcStableAcrossManyBlocksAtHighGain` (every knob at both extremes, 200 blocks, output finite and DC below −40 dBFS), `ThdIncreasesWithGain` (using `tests/measure/Thd.hpp`), `NeverExceedsFullScaleWithoutALimiter`, `DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput` (calibrate the master unity gain as the JCM800's `kMasterUnityGain` was: the constant is measured and pinned, with the test that pins it), `DescriptorMakeProducesAWorkingNode`, `CabinetStageEngagesAtANonDefaultBlockSize`, `DryLatencyIsOneOversampledRegion`, plus the amplifier-specific ones listed in each task. Regression test: copy `test_jcm800_regression.cpp`, knobs at the values the task names; the controller regenerates the golden with `SUSHIDSP_REGENERATE_GOLDEN=1 sd test -s regression` and adds the `tests/golden/README.md` row.

### Task 4: `modules/peavey5150`

**Files:** as the common shape with `<name> = peavey5150`, `<Amp> = Peavey5150`, descriptor id `"sushidsp.peavey5150"`, name `"5150"`.

**Controls and descriptor order:** `pre_gain` (Linear 0–1, default 0.5), `low`, `mid`, `high` (Linear, 0.5), `post_gain` (Decibel, JCM800's master range, 0 dB), `resonance` (Linear, 0.5), `presence` (Linear, 0.5), `input_trim`, `output`. Nine parameters.

**Signal path** (dossier §2 diagram, §3 tables, §4, §5; K-relay states as the dossier's lead-channel reading, open question 14 noted):

| Stage | Bricks | Dossier source |
| --- | --- | --- |
| Input: R22 68 k grid stopper, C20 39 p | one-pole low-pass at 1/(2π·68k·39p) | §2 |
| V1A | `TriodeStage` gain 77.4, bypassed | §3 gain table row V1A |
| C6 22 n into R29 470 k ∥ C17 2.2 n, node Q, VR2 1 M PRE GAIN with R41 1 M across, C19 1 n | high-pass at C6/R29; `pre_gain` scales as the JCM800's `preamp_attenuation`; treble peaker R29∥C17 as a high shelf with corner 1/(2π·470k·2.2n) | §2 |
| V1B, R82 470 k grid leak | `TriodeStage` gain 65.2; cathode R17 1.02 k ∥ C4 1 µ, plus C26 22 µ + R18 47 k in the K1A state the dossier reads for lead | §3 V1B |
| C2 22 n, R6 470 k / R9 1 M divider 0.680 | high-pass, attenuation 0.680 | §2 |
| V2A cold clipper, R15 39 k unbypassed | `TriodeStage` gain 2.21 | §3 V2A |
| C1 22 n; C48 1 n shunt, R11 330 k, R7 220 k, R20 220 k (K1B) | high-pass; low-pass from C48 against the Thevenin of R7/R11/R20 as the dossier gives it | §2 |
| V2B | `TriodeStage` gain 77.4 | §3 V2B |
| R32 1 M, C57 22 n, R101 100 k | divider 100/(1000+100), high-pass | §2 |
| V5B, R102 2.2 k unbypassed | `TriodeStage` gain 41.4 | §3 V5B |
| R87 1 M, C56 22 n, R88 1 M / R91 1 M | divider, high-pass | §2 |
| V5A, R93 2.2 k unbypassed | `TriodeStage` gain 25.3 | §3 V5A |
| C58 22 n into the stack | `Peavey5150ToneStack`: `FmvComponents{ 570e-12, 22e-9, 22e-9, 47e3, 1e6, 50e3, 250e3 }` (C16∥C60, C18, C21, R28, VR3, VR4, VR5); R24 33 k and C59 10 n + R94 47 k to ground at the input node as the documented approximation described above | §4 |
| VR7 1 M POST GAIN, R85 220 k ∥ R81 2.2 M | `post_gain` in dB with a measured unity constant | §2 |
| V3B cathode follower (C52 `[C]`) | pass-through scalar with the C52 high-pass; README states the `[C]` reading and open question 15 | §2, §3 |
| C11 47 n, R26 1 M into V3A, R14 3.3 k ∥ C61 4.7 n | high-pass at C11/R26; `TriodeStage` gain 14.0; cathode shelf zero/pole from R14/C61 (the dossier's "near 56 kHz" corner) | §3 V3A |
| C49 22 n, R48 100 k into the LTP | high-pass | §2 |
| V4A/V4B long-tailed pair | `TriodeStage` gain from §5's phase-inverter row, attenuation into the power grids from R54/R55 and the 2.2 k stoppers | §5 |
| 6L6GC push-pull | two `PentodeStage(kPeavey5150Power6L6GC)`, opposite phase, the JCM800's bias mismatch offset (0.03 V, cited to `Jcm800.cpp`) | §7 |
| Output transformer, sag | `OutputTransformerStage` with the JCM800's constants (the 5150's primary impedance is not established, README says so), `PowerSupplySagStage` with the JCM800's constants | §5, §8 q2 |
| Feedback: R56 33 k from the 8 Ω tap; RESONANCE VR8 1 M; PRESENCE VR9 10 k, C8 0.1 µ ∥ C62 33 n | `ResonanceControl(rate, corner, 10.0f)` with the corner the dossier derives from the feedback network (§5) or, if it gives none, 100 Hz labelled a modelling choice; presence as a high shelf +10 dB at 2 kHz (§6: "Active Presence: +10 dB 2 kHz") | §5, §6 |
| Cabinet slot, output clamp | as JCM800 | |

Amplifier-specific tests: `Peavey5150PresenceAtMaximumLiftsTwoKilohertzByTenDecibels` (frequency_response at 2 kHz, presence 1.0 vs 0.5, +10 dB ± 1 dB), `Peavey5150ResonanceAtMaximumLiftsTheLowEndByTenDecibels` (at the corner), `Peavey5150ColdClipperStageClipsAsymmetrically` (V2A alone: a 1 V sine's positive and negative peaks differ by more than 20 %; this is what a 39 k unbypassed cathode does and the module exposes the stage through a test hook only if a private accessor is not needed; otherwise measure the whole node's even-harmonic content rising with pre_gain via `tests/measure/Thd.hpp`).

Regression knobs: pre_gain 0.6, low 0.6, mid 0.4, high 0.7, post_gain 0 dB, resonance 0.5, presence 0.5.

README sections: which channel and relay state; stage order table; every approximation (R24/C59 network, follower, transformer constants, resonance corner if chosen, K1 state, C52 reading, the 3400 Ω assumption behind `gain`); calibration points measured (the unity constant, the −6 dBFS default); what is not modelled (rhythm channel, crunch, effects loop, bright).

### Task 5: `modules/mesa_mark2c`

**Files:** `<name> = mesa_mark2c`, `<Amp> = MesaMark2c`, id `"sushidsp.mesa_mark2c"`, name `"Mark IIC+"`.

**Controls and descriptor order:** `volume_1`, `treble`, `bass`, `middle` (Linear, 0.5), `lead_drive` (Linear, 0.5), `lead_master` (Linear, 0.5), `master_1` (Decibel, JCM800 master range, 0 dB), `presence` (Linear, 0.5), `eq_80`, `eq_240`, `eq_750`, `eq_2200`, `eq_6600` (Linear −1…+1, default 0), `input_trim`, `output`. Fifteen parameters. Pull switches are not exposed (spec §2).

**Signal path** (dossier §2, §3.1–3.8, §4.1, §4.2, §5):

| Stage | Bricks | Dossier source |
| --- | --- | --- |
| 1 M grid leak, V1A Rp 150 k, Rk 1.5 k ∥ 15 µ + 0.47 µ | `TriodeStage` gain 70.6 | §3.1 |
| Tone stack on V1A's plate | `MesaMark2cToneStack`: `FmvComponents{ 250e-12, 100e-9, 47e-9, 100e3, 250e3, 10e3, 250e3 }`; second 250 pF and 10 M shift resistor not modelled (README) | §4.1 |
| VOLUME 1, 1 M audio | `volume_1` as an audio-taper attenuation (the JCM800's `preamp_attenuation` law, cited) | §2 |
| V1B Rp 100 k, Rk 1.5 k ∥ 15 µ | `TriodeStage` gain 61.5 | §3.2 |
| 0.047 µ, 100 k to ground; 680 k into LEAD DRIVE 1 M, 180 p bright cap | high-pass; `lead_drive` attenuation with maximum 0.595; the bright cap across the pot's top as a high shelf whose corner the dossier gives in §3.3 | §3.3 |
| V3B Rp 82 k, Rk 1.5 k ∥ 15 µ | `TriodeStage` gain 56.7 | §3.3 |
| 0.02 µ, 170 k / 68 k ∥ 0.047 µ | divider 0.286, high-pass corner 69.7 Hz, low-pass from 68 k ∥ 0.047 µ | §3.4, §6 |
| V4A Rp 270 k, Rk 3.3 k, 0.22 µ bypass | `TriodeStage` gain 70; cathode shelf corner 219 Hz | §3.4, §6 |
| 0.047 µ, 220 k ∥ 250 p, 100 k ∥ 500 p | divider 0.3125; the two RC pairs as the dossier derives them | §3.5 |
| LEAD MASTER 250 k | `lead_master` attenuation | §2 |
| V2A Rp 100 k | `TriodeStage` gain 61.5 | §3.6 |
| V2B loop return, Rp 170 k, Rk 1 k, 0.47 µ | `TriodeStage` gain from §3.7; cathode shelf 339 Hz (PULL DEEP out) | §3.7, §6 |
| MASTER 1, 100 k | `master_1` in dB with a measured unity constant | §2 |
| Five-band EQ | `GraphicEqualizer(rate, { 71.5, 371.8, 979.5, 1576, 4823 }, { 1.434, 1.938, 1.571, 0.673, 1.0 }, 17.0f)`; README: range is +17/−18 in the dossier, modelled symmetric; the 750 Hz and 80 Hz inductor readings are the dossier's blocking questions 1 and 2 and the centres change if they resolve the other way | §4.2, §6, §8 q1–2 |
| V5A/V5B LTP, Rp 82 k | `TriodeStage` gain from §5.1 | §5.1 |
| Presence in the feedback leg from the 8 Ω tap | high shelf; corner and depth from §5.2's network (0.022 µ with 6.8 k) computed and pasted; depth labelled a modelling choice if the dossier gives no dB figure | §5.2 |
| Simul-Class power section | inner pair two `PentodeStage(kMesaMark2cPower6L6GC)`, outer pair two `PentodeStage(kMesaMark2cPowerEl34Triode)`, both pairs driven from the same phase-inverted signal, summed at the transformer with equal weight (README: the 75 W / 25 W split is in the presets' drives, the summing weight is a modelling choice) | §5.3, §7 |
| Transformer, sag, cabinet, clamp | JCM800 constants (primary impedance not established, §8 q3) | |

Amplifier-specific tests: `MesaMark2cToneStackSitsBeforeTheLeadStages` (with lead_drive at maximum and treble at 0 the THD at 1 kHz is lower than with treble at 1: the stack shapes what the following stages clip), `MesaMark2cEqBandBoostsItsCentre` (eq_750 at +1 lifts 979.5 Hz by 17 dB ± 1 dB relative to 0, measured on the whole node at low gain), `MesaMark2cSimulClassPairsBothContribute` (zeroing either pair's output in a test-only hook is not available; instead assert that the node's output at a −20 dBFS input differs from a build with a single pair by comparing against the 5150's structure is not possible either; so: assert the node's THD at moderate drive is between the two presets' single-pair figures computed offline, and paste those figures in the report). If that third test cannot be made honest, drop it and say so in the report.

Regression knobs: volume_1 0.6, treble 0.7, bass 0.4, middle 0.5, lead_drive 0.7, lead_master 0.5, master_1 0 dB, presence 0.5, all five EQ sliders 0.

README sections as Task 4, plus: the stack-before-gain ordering and why it matters (one sentence, citing §2); the EQ's active stage not modelled; PULL switches fixed at the dossier's Lead-mode reading.

### Task 6: `modules/diezel_vh4`

**Files:** `<name> = diezel_vh4`, `<Amp> = DiezelVh4`, id `"sushidsp.diezel_vh4"`, name `"VH4 Ch3"`.

**Controls and descriptor order:** `gain`, `bass`, `middle`, `treble`, `volume` (Linear, 0.5), `deep`, `presence` (Linear, 0.5), `master` (Decibel, JCM800 master range, 0 dB), `input_trim`, `output`. Ten parameters.

**Signal path** (dossier §2, §3.1–3.6, §4, §5 "What follows for the model", §6 derived table). The README opens with the dossier's verdict sentence: a documented approximation.

| Stage | Bricks | Dossier source |
| --- | --- | --- |
| R1 1 M, C1 100–200 p | one-pole low-pass at the 100 p reading (README: 100–200 p bracket) | §3.1 |
| V1a Ra 120 k, Rk 2.2 k ∥ 47 µ | `TriodeStage` gain 61.4 (×56.8 into 470 k: use 56.8) | §3.1, §6 |
| C13 1 n + R14 220 k into GAIN 250 k | high-pass 314 Hz at gain max to 618 Hz at gain min: a `Biquad` high-pass whose corner follows the knob per the dossier's formula; `gain` attenuation | §3.2, §6 |
| R21 47 k stopper, R22 3.3 M grid leak (§8 q5: 1 M or 220 k on real boards; use 3.3 M as drawn, README) | Miller low-pass | §3.3 |
| V2a Ra 120 k, Rk 680 ∥ 1 µ | `TriodeStage` gain 67.2; cathode shelf zero 234 Hz, pole 603 Hz | §3.3, §6 |
| C18 4.7 n, relay, R26 470 k, 1 M ∥ 1 M | high-pass 63.8 Hz; divider 500 k/(500 k + 30.8 k + 470 k) as §6 gives it | §3.4, §6 |
| V3a Ra 120 k, Rk 680 ∥ 2.2 µ | `TriodeStage` gain 71.4; shelf zero 106 Hz, pole 274 Hz | §3.4, §6 |
| V3b DC-coupled follower, Rk 120 k | scalar 0.986; output high-pass 2.26 Hz (C20 220 n into R30 1 M); the §3.6 network is **not** implemented (dossier: "Do not implement this network from S1") | §3.5, §3.6 |
| Tone stack | `DiezelVh4ToneStack`: `FmvComponents{ 560e-12, 22e-9, 22e-9, 39e3, 1e6, 22e3, 220e3 }`; R40 10 k series input resistor as the documented approximation; README: the interconnection is the dossier's blocking question 1 and the module uses the standard FMV form | §4 |
| R42 470 k, VOLUME 470 k, R43 680 k into the mix node (48.1 k) | `volume` attenuation; mix-bus attenuation ×0.0660 | §2, §6 |
| V4a Ra 120 k, Rk 640 unbypassed | `TriodeStage` gain 28.7 | §6 |
| V4b follower, send pad | scalar; the send pad is not in the path to the power amp (README) | §2 |
| Loop return driver, phase inverter | not established (§5): one `TriodeStage` with the JCM800's phase-inverter gain 40, labelled a modelling choice in code and README | §5 |
| EL34 quad | two `PentodeStage(kDiezelVh4PowerEl34)` | §7 |
| Deep | `ResonanceControl(rate, 80.0f, 10.0f)`: 80 Hz from S2, 10 dB a modelling choice (README; §8 q8–9 on the LC form) | §5, §6 |
| Presence | high shelf at 4 kHz (S2), depth the JCM800's 9 dB, a modelling choice | §6 |
| Master, transformer, sag, cabinet, clamp | JCM800 constants; primary impedance not established | §5 |

Amplifier-specific tests: `DiezelVh4GainKnobMovesTheInputHighPassCorner` (frequency_response at 300 Hz relative to 1 kHz differs by more than 3 dB between gain 0 and gain 1 at low drive), `DiezelVh4DeepAtMaximumLiftsEightyHertzByTenDecibels`, `DiezelVh4MixBusAttenuationIsApplied` (the node's small-signal gain at a stated knob set matches the dossier's cascade product times 0.0660 within 1 dB; compute the expected figure from §6 and paste it).

Regression knobs: gain 0.6, bass 0.5, middle 0.5, treble 0.6, volume 0.5, deep 0.5, presence 0.5, master 0 dB.

README sections as Task 4, opening with the verdict; every "not established" from §5 listed; §3.6 network omitted with the dossier's sentence; EL34 decision and its −36 V row; the KT77 alternative.

---

## Wave P2-2: registration, panels, traps, documentation (one task, serial)

### Task 7: register the three amplifiers

**Files:**
- Modify: `modules/node_registry/src/NodeRegistry.cpp` (three includes, three entries under `NodeCategory{ "Amp" }`), `modules/node_registry/CMakeLists.txt` (controller: link the three modules)
- Modify: `apps/host_gui/ui/PanelLibrary.cpp` (three `k<Amp>Controls[]` tables and three `panel_table()` rows; knob positions drawn from each amplifier's front panel left to right, skin colours: 5150 black/silver, IIC+ black/cream with the EQ sliders as five knobs in a row, VH4 black/blue)
- Modify: `tests/functional/unit/test_rt_safety.cpp` (three `<Amp>ProcessPerformsNoHeapAllocationWithoutCabinet` cases copied from the JCM800's), `tests/functional/unit/test_node_registry.cpp` (the list test names the three ids), `tests/functional/unit/test_panel_library.cpp` (a panel exists for each id)
- Modify: `docs/design/sushidsp.md` §5 (a "§5a Amplifier family" subsection listing the four modules, their channel and their dossier), §8 layout (three module lines), §9 roadmap (DA row); `docs/README.md` (module README links); `docs/reference/CHANGELOG.md` (one line per module, one for registration, one for panels); `docs/superpowers/specs/2026-09-06-high-gain-amps-design.md` status line ("DA0–DA3 shipped")
- Controller: `tests/CMakeLists.txt` (link the three modules, add the three unit and three regression test files), root `CMakeLists.txt` (three `add_subdirectory` lines), `modules/*/CMakeLists.txt`, `apps/host_gui/CMakeLists.txt` if it links product modules directly.

- [ ] **Step 1: Failing tests** (registry lists the three ids and `make_node` builds each; panel_for returns non-null for each; RT traps).
- [ ] **Step 2: Implement** registry entries and panels.
- [ ] **Step 3: Documentation** as listed; every link resolves; changelog lines under 240 characters.
- [ ] **Step 4: Controller runs `sd build && sd test`**, then `SUSHIDSP_REGENERATE_GOLDEN=1 sd test -s regression` once for the three new goldens if not already pinned in P2-1, adds the `tests/golden/README.md` rows, and opens `host_gui` to load each amplifier and save/load a preset through `rig_preset`.
- [ ] **Step 5: Commit** in two: `DA3: register the 5150, Mark IIC+ and VH4 and give them panels` and `docs: amplifier family section, module links, DA changelog`.

---

## Wave summary

| Wave | Tasks | Files | Waits on | Build the maintainer runs |
| --- | --- | --- | --- | --- |
| P2-0 | 1, 2, 3 | one brick each (+ its test); Task 2 also touches `Jcm800.cpp`'s two initialisers | none | `sd build && sd test`; JCM800 goldens unchanged; jcm_fmv null 0.05 dB |
| P2-1 | 4, 5, 6 | `modules/<name>/**`, `tests/functional/unit/test_<name>.cpp`, `tests/functional/regression/test_<name>_regression.cpp`; controller adds CMake | 1, 2, 3 | `sd build && sd test`, then golden regeneration for the three |
| P2-2 | 7 | registry, panels, rt_safety, registry/panel tests, docs | 4, 5, 6 | `sd build && sd test`; `host_gui` loads all three |

## Self-review

- Spec §6 → Tasks 4–6; §7 → Task 7; §9 acceptance → common tests + Task 7 Step 4. Spec §3's preset list → Task 2. Final review F1 → Task 1; P1 → Task 2; the EQ's per-band Q → Task 3.
- Every value in the stage tables above is quoted from a dossier section named in the same row; the implementer computes corners from R and C and pastes them; anything the dossier calls not established is labelled a modelling choice in code and README.
- Names used across tasks: `PentodeStage(const PowerTubePreset&)`, the four preset names, `GraphicEqualizer` two-list constructor, `FmvComponents` field order `{ treble C, bass C, middle C, slope R, bass pot, middle pot, treble pot }`, `ResonanceControl(rate, corner, boost)`; each defined where first produced.
- Open: the Mark IIC+'s third amplifier-specific test may not be provable without a test hook; the task says so and allows dropping it with a stated reason.
