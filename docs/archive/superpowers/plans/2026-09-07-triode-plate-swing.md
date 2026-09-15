# TriodeStage plate swing (DA4) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give `TriodeStage` a plate-referenced soft ceiling and a grid-excursion probe, then move the four amplifier modules onto it and delete their level-staging devices.

**Architecture:** One brick change (a seven-argument constructor, a `PlateSwing` struct, a peak counter; the six-argument path untouched), then four independent module tasks with disjoint files, each computing its `PlateSwing`s from its dossier and the Koren 12AX7 load line, deleting staging constants, adding the probe accessors and the grid-bound test, re-measuring unity and re-pinning goldens. Docs close it.

**Tech Stack:** C++17, gtest, `circuits/models/Triode12AX7.hpp` (Koren model, `current_and_jacobian`), `sd build` / `sd test`, `SUSHIDSP_REGENERATE_GOLDEN=1`.

**Spec:** `docs/superpowers/specs/2026-09-07-triode-plate-swing-design.md`

## Global Constraints

- C++17; Allman; naming rules; 76-column license box; `@brief` on every symbol; no reason-comments (cite the spec or dossier by path).
- Anything reachable from `process()` allocation-free, lock-free, syscall-free, throw-free; `peak_grid_excursion_volts()` is one compare per sample.
- No value outside a dossier (or the `jcm_*` SPICE READMEs for the JCM800); the plate bottom is computed from the Koren model and the arithmetic is pasted in the code comment and README.
- The six-argument `TriodeStage` path is bit-identical; Task 1 proves it before any module moves.
- Subagents run no `sd`/cmake/ninja/ctest and edit no CMake; the controller builds, tests and regenerates goldens.
- Commits end with the session's Co-Authored-By and Claude-Session trailers; no `--amend` in the shared checkout.

---

## Wave 1 (one task, serial)

### Task 1: `TriodeStage` plate swing and grid probe

**Files:**
- Modify: `include/SushiDSP/circuits/stages/TriodeStage.hpp`, `src/circuits/stages/TriodeStage.cpp`
- Modify: `tests/functional/unit/test_triode_stage.cpp`

**Interfaces:** as spec §3: `struct PlateSwing { float headroom_to_supply_volts; float headroom_to_bottom_volts; };`, the seven-argument constructor, `float peak_grid_excursion_volts() const`. New members: `PlateSwing plate_swing_{}`, `bool has_plate_swing_ = false`, `float peak_grid_excursion_ = 0.0f`. `reset()` clears the peak.

- [ ] **Step 1: Failing tests** appended to `test_triode_stage.cpp` (the five existing cases stay untouched):

```cpp
TEST(Unit_Dsp, TriodeStageSixArgumentPathIsUnchangedByThePlateSwingOverload)
{
    SushiDSP::TriodeStage before(10000.0f, 1000000.0f, -1.5f, 60.0f, 1e-9f, 0.02585f);
    SushiDSP::TriodeStage huge(10000.0f, 1000000.0f, -1.5f, 60.0f, 1e-9f, 0.02585f, SushiDSP::PlateSwing{ 1e9f, 1e9f });
    for (int i = 0; i < 4000; ++i)
    {
        const float x = 3.0f * static_cast<float>(std::sin(2.0 * kPi * 440.0 * i / 48000.0));
        const float a = before.process_sample(x), b = huge.process_sample(x);
        EXPECT_NEAR(a, b, 1e-3f * std::max(1.0f, std::fabs(a)));
    }
}

TEST(Unit_Dsp, TriodeStagePlateSwingBoundsBothSidesAsymmetrically)
{
    SushiDSP::TriodeStage stage(10000.0f, 1000000.0f, -1.5f, 60.0f, 1e-9f, 0.02585f, SushiDSP::PlateSwing{ 194.0f, 115.0f });
    float positive_peak = 0.0f, negative_peak = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        const float y = stage.process_sample(10.0f * static_cast<float>(std::sin(2.0 * kPi * 100.0 * i / 48000.0)));
        positive_peak = std::max(positive_peak, y);
        negative_peak = std::min(negative_peak, y);
    }
    EXPECT_LT(positive_peak, 194.0f);
    EXPECT_GT(positive_peak, 0.9f * 194.0f);
    EXPECT_GT(negative_peak, -115.0f);
    EXPECT_LT(negative_peak, -0.5f * 115.0f);
}

TEST(Unit_Dsp, TriodeStageSmallSignalGainIsUnchangedByThePlateSwing)
{
    SushiDSP::TriodeStage stage(10000.0f, 1000000.0f, -1.5f, 60.0f, 1e-9f, 0.02585f, SushiDSP::PlateSwing{ 194.0f, 115.0f });
    float peak = 0.0f;
    for (int i = 0; i < 4800; ++i)
    {
        peak = std::max(peak, std::fabs(stage.process_sample(0.01f * static_cast<float>(std::sin(2.0 * kPi * 1000.0 * i / 48000.0)))));
    }
    EXPECT_NEAR(peak, 0.6f, 0.006f);
}

TEST(Unit_Dsp, TriodeStageReportsThePeakGridExcursionSinceReset)
{
    SushiDSP::TriodeStage stage(10000.0f, 1000000.0f, -1.5f, 60.0f, 1e-9f, 0.02585f, SushiDSP::PlateSwing{ 194.0f, 115.0f });
    EXPECT_FLOAT_EQ(stage.peak_grid_excursion_volts(), 0.0f);
    for (int i = 0; i < 480; ++i) stage.process_sample(-2.0f);
    EXPECT_NEAR(stage.peak_grid_excursion_volts(), 2.0f, 0.05f);
    stage.reset();
    EXPECT_FLOAT_EQ(stage.peak_grid_excursion_volts(), 0.0f);
}
```

(A −2 V step through Rs into Rg = 1 MΩ lands the grid about 2 V below Vk, no conduction; that is what the third test measures.)

- [ ] **Step 2: Implement.** Seven-argument constructor delegates to the six-argument one, then sets `plate_swing_` and `has_plate_swing_ = true`. In `process_sample`, after the solve: `peak_grid_excursion_ = std::max(peak_grid_excursion_, std::fabs(grid_voltage - vk));` then the linear plate `p`; if `has_plate_swing_`, apply the two-sided tanh from spec §3 and return it, else return `p`. `reset()` zeroes the peak.
- [ ] **Step 3: Syntax-check**, paste. **Step 4: Controller runs `sd build && sd test`**: every existing test and every golden unchanged. **Step 5: Commit** `DA4: TriodeStage plate swing (two-sided tanh ceiling) and peak grid excursion probe`.

---

## Wave 2 (four tasks, parallel, disjoint files)

Common shape for Tasks 2–5. For each `TriodeStage` the module owns:

1. Read the stage's idle plate voltage `Va` and its supply node `B+` from the dossier's DC table (JCM800: `docs/reference/spice/jcm_v1a/README.md`, `jcm_v1b_cold`, `jcm_ltp`: B+ 360 V, the stage's `Ra`, `Rk`; solve the idle point with the Koren model the same way).
2. Compute the plate bottom: the plate voltage at `Vgk = 0` on the load line `Va = B+ − Ra·Ia(Vgk = 0, Vpk)` using `Triode12AX7::current_and_jacobian` (branch 0 is plate current, `v = { vgk, vpk }`) in a scratch program (C++ against `include/`, or Python reimplementing the same Koren equations with the header's constants); bisection over `Vpk` converges in 40 steps. For an unbypassed or partially bypassed cathode use the cathode voltage the dossier gives at idle for `Vk` and treat `Vgk = 0` as the conduction limit; note the simplification in the README.
3. `PlateSwing{ B+ − Va, Va − Va_bottom }`, each a named `constexpr` beside the stage's other constants with the arithmetic in its `@brief`.
4. Delete every `*LevelStaging*`, `kGridDriveNormalisation`, `k*GridDrive` constant and the JCM800's `kV1bToV2aAttenuation`, `kV2aToPhaseInverterAttenuation`, `kPhaseInverterToPowerTubeAttenuation` (which are staging, not dividers; replace them with the `jcm_*` README dividers if a divider exists, else 1.0). Keep every dossier divider. Keep `kPowerTubeToTransformerAttenuation` (label: output trim to full scale).
5. Add `std::size_t triode_stage_count() const` and `float grid_excursion_peak_volts(std::size_t stage) const` (out-of-range returns 0), documented as test hooks like the JCM800's bias-mismatch setter.
6. Add the test from spec §4, `<Amp>NoGridSitsOnItsBoundAtDefaultSettings`: default knobs, −12 dBFS 1 kHz, 200 blocks of 256 at 48 kHz, then for each stage `EXPECT_LT(peak, 0.9f * 4.14f)`, and `EXPECT_GT(max over stages, 0.2f)`.
7. Re-measure the unity constant with the existing default-peak test (build the test binary the way part 2's implementers did, or report the failing test's printed multiplier to the controller), update the header comment with the date and figure.
8. Update the module README: stage table gains a `PlateSwing` column with both headrooms; the level-staging section becomes one paragraph saying it was removed by this spec.
9. The controller regenerates the goldens and adds the `tests/golden/README.md` row; the module task's commit does not include goldens.

### Task 2: `modules/jcm800`
**Files:** `modules/jcm800/include/SushiDSP/jcm800/Jcm800.hpp`, `modules/jcm800/src/Jcm800.cpp`, `tests/functional/unit/test_jcm800.cpp`, `modules/jcm800/README.md` (create if absent, with the DD-3 README's shape). Stages: V1a, V1b (cold, Rk 10 k unbypassed), V2a, phase inverter. Note the JCM800 has no dossier; its facts are the `jcm_*` SPICE READMEs and `Jcm800.cpp`'s own constants; every gain the module keeps stays as it is (60, 40) since the goldens re-pin anyway, but the README states they are D6a approximations.

### Task 3: `modules/peavey5150`
**Files:** the module's four sources/headers, `tests/functional/unit/test_peavey5150.cpp`, `modules/peavey5150/README.md`. Stages: V1A, V1B, V2A (cold, 39 k), V2B, V5B, V5A, V3A, phase inverter. DC table: dossier §3 at the 340 V rail. The derived 28.62 power-grid lift also goes: the phase inverter's plate now swings physically and `kPhaseInverterToPowerTubeAttenuation` 0.8617 carries it to the 6L6GC grids; if the default setting no longer reaches the 3.47 V power-grid target, say so in the README rather than re-adding a lift.

### Task 4: `modules/mesa_mark2c`
**Files:** the module's four sources/headers, `tests/functional/unit/test_mesa_mark2c.cpp`, `modules/mesa_mark2c/README.md`. Stages: V1A, V1B, V3B, V4A, V2A, V2B, phase inverter. DC table: dossier §3 and §6 (plate voltages 222 / 255 / 294 / 294 / 292 / 188 V, rails B, C, D as §3 gives them).

### Task 5: `modules/diezel_vh4`
**Files:** the module's four sources/headers, `tests/functional/unit/test_diezel_vh4.cpp`, `modules/diezel_vh4/README.md`. Stages: V1a, V2a, V3a, V4a, phase inverter (the JCM800-gain stand-in stays a labelled modelling choice; give it the JCM800's `PlateSwing`). DC table: dossier §6 at +367 V.

---

## Wave 3 (one task, serial)

### Task 6: documentation
**Files:** `docs/design/sushidsp.md` (§5 "As built" note on the plate swing; §5a status column), `docs/reference/CHANGELOG.md` (one line each: brick, four modules, goldens), `docs/superpowers/specs/2026-09-07-triode-plate-swing-design.md` status line, `docs/superpowers/specs/2026-09-06-high-gain-amps-design.md` (§4 note that level staging is gone), `tests/golden/README.md` rows (controller writes them with the regeneration).

## Wave summary

| Wave | Tasks | Files | Waits on | Build the maintainer runs |
| --- | --- | --- | --- | --- |
| 1 | 1 | `TriodeStage.*`, `test_triode_stage.cpp` | none | `sd build && sd test`, everything unchanged |
| 2 | 2, 3, 4, 5 | one module directory + its unit test each | 1 | `sd build && sd test`, then `SUSHIDSP_REGENERATE_GOLDEN=1` for the four |
| 3 | 6 | docs | 2–5 | `sd build && sd test` |

## Self-review
- Spec §3 → Task 1; §4 → Tasks 2–5; §6 → Task 1 Step 4, Tasks 2–5 steps 6–9, controller golden rows.
- Names across tasks: `PlateSwing`, the two field names, `peak_grid_excursion_volts`, `triode_stage_count`, `grid_excursion_peak_volts`; defined in Task 1 / the common shape.
- The JCM800's "no dossier" case is stated with its source of truth named.
