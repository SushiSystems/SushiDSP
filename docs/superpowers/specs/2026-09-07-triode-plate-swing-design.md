# TriodeStage plate swing and the end of level staging

**Status:** approved 2026-09-07, in execution (plan `docs/superpowers/plans/2026-09-07-triode-plate-swing.md`). Roadmap phase **DA4**.

## 1. Goal

`TriodeStage` gains a plate-referenced output: a soft, asymmetric ceiling set by how far the plate can swing toward the supply on the cutoff side and toward its bottom on the conduction side. With that ceiling in place the four amplifier modules (JCM800, 5150, Mark IIC+, VH4) drop the level-staging devices they carry today and run their dossiers' dividers at face value in volts. Every grid then sits where its circuit puts it, and a test can read each grid's excursion and prove it.

## 2. Why

Part 2's final review measured the defect: `TriodeStage` returns `-gain × (Vg − Vk)` with `Vg` bounded to `[Vk − 4.14 V, Vk + ~0.2 V]`. For the 5150's V1A (gain 77) that is +320 V toward cutoff and −15 V toward conduction where the real stage swings about +194 V / −115 V. The 20:1 asymmetry and the unbounded cutoff side are why every module needed a level-staging device to keep the next grid off the bound: JCM800 five scalars, 5150 seven `k*LevelStaging` and a derived power-grid lift, VH4 four staging constants, IIC+ a per-stage gain normalisation. None of them is a circuit rule; the 5150's cancels the dossier's dividers exactly. Three modules, three devices, is a SOLID defect on its own.

## 3. The change to `TriodeStage`

```cpp
/// @brief How far a stage's plate can swing from its idle point before it saturates.
struct PlateSwing
{
    float headroom_to_supply_volts;   ///< B+ minus the idle plate voltage; the cutoff side.
    float headroom_to_bottom_volts;   ///< Idle plate voltage minus the plate at Vgk = 0 on the load line; the conduction side.
};

TriodeStage(float source_resistance_ohms, float grid_leak_resistance_ohms, float cathode_bias_volts,
            float gain, float saturation_current_amps, float thermal_voltage_volts,
            const PlateSwing& plate_swing);

/// @brief The largest |Vg − Vk| the grid has reached since reset(). Audio-thread safe.
float peak_grid_excursion_volts() const;
```

The six-argument constructor stays and keeps today's behaviour bit for bit (no ceiling), so nothing outside this phase moves. The seven-argument path applies, after the existing grid solve, a two-sided soft saturation to the linear plate output `p = -gain × (Vg − Vk)`:

```
p > 0:  out =  up · tanh(p / up)      up = headroom_to_supply_volts
p < 0:  out = -dn · tanh(-p / dn)     dn = headroom_to_bottom_volts
```

`tanh` is the same shape `PentodeStage` already uses for its plate. The two bounds come from the dossier's DC table (idle plate voltage `Va` and the stage's supply node) and from the Koren 12AX7 model in `include/SushiDSP/circuits/models/Triode12AX7.hpp` solved on the stage's load line at `Vgk = 0` for the plate bottom. Both are physics from numbers the dossiers already hold; neither is a free knob. `peak_grid_excursion_volts()` is one `std::max` per sample on a member, cleared by `reset()`.

## 4. The change to the modules

Each amplifier node:

- constructs every `TriodeStage` through the seven-argument constructor with a `PlateSwing` computed from its dossier (JCM800 from the `jcm_*` SPICE READMEs: B+ 360 V, the D6d block values);
- deletes its level-staging constants and passes the dossier's dividers unchanged, in volts, from the first grid to the power tubes; `kPowerTubeToTransformerAttenuation` stays as the one output trim to full scale and is labelled so;
- exposes, beside the JCM800's existing bias-mismatch test hook, `std::size_t triode_stage_count() const` and `float grid_excursion_peak_volts(std::size_t stage) const`, forwarding to each stage's `peak_grid_excursion_volts()` in signal order;
- re-measures its master unity constant and re-pins its golden files with a `tests/golden/README.md` row naming this spec.

A new test per module, `<Amp>NoGridSitsOnItsBoundAtDefaultSettings`, renders the default setting at a −12 dBFS input and asserts every stage's peak grid excursion is below 90 % of the 4.14 V cutoff bound and that at least one stage exceeds the 0.2 V conduction knee (the amp distorts somewhere). The IIC+, whose written cascade product is 1e6, is the case that proves the ceiling works: without staging its later grids must land inside the bound because the plate ahead of them can no longer deliver more than its headroom.

## 5. Out of scope, stated

The full DK load-line solve (the D6d path) is not this phase; the tanh ceiling is the documented approximation and the D6d rebuild replaces it stage by stage when it lands. The shelf-corner rule, the first-order coupling brick and the transformer's DC pass (final review F2, F5, F7) are separate DA5 items.

## 6. Acceptance

- Six-argument `TriodeStage` bit-identical: the existing `test_triode_stage.cpp` cases and every golden not re-pinned in this phase pass unchanged before the modules switch.
- Four modules build on the seven-argument path with zero `*LevelStaging*` / `GridDriveNormalisation` symbols left (`grep` clean).
- The grid-bound test passes on all four; THD sweeps stay monotonic; default settings peak at −6 dBFS ± 1 dB with the re-measured unity constants.
- Goldens re-pinned once, with README rows, in the same commit as each module's switch.
