# TriodeStage plate swing and the end of level staging

**Status:** DA4 shipped 2026-09-07 with two known issues (alias floor, IIC+ loop return); DA5 follows. Plan `docs/superpowers/plans/2026-09-07-triode-plate-swing.md`.

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

On the seven-argument path the grid-conduction diode is referenced to the cathode, not to the bias
point: conduction begins at `Vgk = 0`, which is `|cathode_bias_volts|` above the grid's resting point,
so a stage biased at −1.5 V has 1.5 V of clean positive swing before the grid draws current and its
plate reaches the bottom of the load line, as the real stage does. The six-argument path keeps the
bias-referenced diode it has today (maintainer decision 2026-09-07). The solver's upper bound moves
with the reference; the lower bound stays `160·Vt` below the resting point.

`tanh` is the same shape `PentodeStage` already uses for its plate. The two bounds come from the dossier's DC table (idle plate voltage `Va` and the stage's supply node) and from the Koren 12AX7 model in `include/SushiDSP/circuits/models/Triode12AX7.hpp` solved on the stage's load line at `Vgk = 0` for the plate bottom. Both are physics from numbers the dossiers already hold; neither is a free knob. `peak_grid_excursion_volts()` is one `std::max` per sample on a member, cleared by `reset()`.

## 4. The change to the modules

Each amplifier node:

- constructs every `TriodeStage` through the seven-argument constructor with a `PlateSwing` computed from its dossier (JCM800 from the `jcm_*` SPICE READMEs: B+ 360 V, the D6d block values);
- deletes its level-staging constants and passes the dossier's dividers unchanged, in volts, from the first grid to the power tubes; `kPowerTubeToTransformerAttenuation` stays as the one output trim to full scale and is labelled so;
- exposes, beside the JCM800's existing bias-mismatch test hook, `std::size_t triode_stage_count() const` and `float grid_excursion_peak_volts(std::size_t stage) const`, forwarding to each stage's `peak_grid_excursion_volts()` in signal order;
- re-measures its master unity constant and re-pins its golden files with a `tests/golden/README.md` row naming this spec.

The four modules' first pass (2026-09-07) showed that the dossiers' dividers at face value put
two to six grids per amplifier on the solver's 4.14 V cutoff fence at the default setting, and that a
72 to 220 V plate ceiling cannot hold the grid behind it inside 4 V. The dossiers say the same in
words (the IIC+'s cascade product is 1e6; Peavey's nominal lead input is −80 dBV), so the circuits
are this hot and a "no grid on its bound" criterion contradicts them. The maintainer ruled that the
circuit stays and the criterion changes. Per module the acceptance is:

- **A clean end exists.** Gain knob at minimum, −40 dBFS input: THD below 10 %. The 5150 also at
  Peavey's own nominal point, −80 dBV (0.1 mV) with PRE GAIN at minimum: THD below 5 %.
- **THD does not fall as the gain knob rises**, within 1.0 percentage point across the knob's
  travel (the metric saturates once every stage clips and eases slightly as earlier stages square
  the wave into faster-falling harmonics; the 5150 measured a 0.68 pp ease at 0.1 to 0.2).
- **Default settings peak at −6 dBFS ± 1 dB** with the re-measured unity constant.
- **Grid probe readings** at the default setting are recorded in the module README as measurements,
  not asserted. The test is named `<Amp>HasACleanEndAtMinimumGain`.

Two circuit facts the first pass also exposed, both derived from dossier values and applied by every
module:

- **The phase inverter is a long-tailed pair.** Driven single-ended, its cathode follows the grid
  through the tail, so the driven triode's grid-to-cathode signal is the input divided by
  `1 + R_tail / (R_tail + (r_a + R_a) / (µ + 1))`, where `(r_a + R_a) / (µ + 1)` is the impedance
  the other triode's cathode presents in parallel with the tail. For any real tail the factor sits
  near 2 (the 5150: about 1.9). The first ruling of 2026-09-07 used `1 + (µ + 1)·R_tail / (r_a + R_a)`
  without the cathode shunt and on top of a "half the common-cathode gain" figure, which counted the
  pair's split twice and left the power tubes 20 dB low; the four module reviews caught it. Each module
  therefore applies the corrected factor at the inverter's input as `kPhaseInverterDegeneration` (a
  number ≥ 1, always used as a divide, in every module) and gives the inverter's `TriodeStage` the
  **full** single-triode gain from its load line, not half of it. Modules whose inverter is a JCM800
  stand-in copy the JCM800's constants and cite them.
- **`PentodeStage` is not volt-true.** Its grid saturates at `1 / (plate_saturation_drive × gain)`
  model volts (about 2.5 to 3.5) where the real grid needs `|cathode_bias_volts|` (20 to 55 V). The
  inverter-to-power-grid link therefore carries `kPowerTubeGridScale = (1 / (drive × gain)) /
  |preset cathode_bias_volts|`, labelled the bridge until `PentodeStage` becomes volt-true (DA5),
  beside the circuit's own grid-leak divider. It replaces the 1/32 and 1.0 constants the modules
  carried at that link.

## 5. Out of scope, stated

The full DK load-line solve (the D6d path) is not this phase; the tanh ceiling is the documented approximation and the D6d rebuild replaces it stage by stage when it lands. The shelf-corner rule, the first-order coupling brick and the transformer's DC pass (final review F2, F5, F7) are separate DA5 items. DA5 also deletes the six-argument `TriodeStage` path and moves `PlateSwing::apply` to a shared helper `PentodeStage` can call, once `PentodeStage` becomes volt-true and needs the same ceiling.

## 6. Acceptance

- Six-argument `TriodeStage` bit-identical: the existing `test_triode_stage.cpp` cases and every golden not re-pinned in this phase pass unchanged before the modules switch.
- Four modules build on the seven-argument path with zero `*LevelStaging*` / `GridDriveNormalisation` symbols left (`grep` clean).
- The clean-end test passes on the JCM800, 5150 and VH4 and is skipped on the IIC+ under the loop-return known issue; THD does not fall across the gain knob within 1.0 pp; default settings peak at −6 dBFS ± 1 dB with the re-measured unity constants; grid probe readings recorded in each README.
- Goldens re-pinned once, with README rows, in the same commit as each module's switch.
