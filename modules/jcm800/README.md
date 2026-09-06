# `modules/jcm800` — Marshall JCM800 2203

One `IAudioNode`, mono in and mono out, registered as `sushidsp.jcm800` with eight parameters:
preamp, master, bass, middle, treble, presence, input trim and output. The cabinet convolution
is optional and lives inside the node, behind `set_cabinet()`.

Intent and the phase rulings live in
[`docs/design/sushidsp.md`](../../docs/design/sushidsp.md); the plate-swing change this file
describes is specified in
[`docs/superpowers/specs/2026-09-07-triode-plate-swing-design.md`](../../docs/superpowers/specs/2026-09-07-triode-plate-swing-design.md).
The component values come from the `jcm_*` SPICE decks under
[`docs/reference/spice/`](../../docs/reference/spice/). This file records what the code in this
directory does.

## Which amp

The 2203, the master-volume 100 W head: bright cap, V1a into the preamp pot, V1b as the cold
clipper, the FMV tone stack, V2a, a phase inverter, an EL34 pair, output transformer, supply
sag, presence. There is no design dossier for it. Its facts are the six `jcm_*` SPICE decks and
this module's own constants, so several blocks below are approximations rather than transcribed
networks, and each one says so.

## The stages and their plate swings

Every triode runs on `TriodeStage`'s seven-argument constructor. The bias and swing figures come
from the Koren 12AX7 model in `include/SushiDSP/circuits/models/Triode12AX7.hpp` solved on each
block's own load line at B+ 360 V: the idle point from `Vk = Rk·Ia` with the grid at DC ground,
and the plate bottom from `Va = 360 − Ra·Ia` at `Vgk = 0` with the cathode held at its idle
voltage.

| Stage | Deck | Ra | Rk | Ia | Vk | Va | Va at Vgk = 0 | `PlateSwing` (supply, bottom) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| V1a | `jcm_v1a` | 100 kΩ | 2.7 kΩ (bypassed) | 0.855584 mA | 2.310076 V | 274.441625 V | 120.747068 V | 85.558375 V, 153.694557 V |
| V1b cold clipper | `jcm_v1b_cold` | 100 kΩ | 10 kΩ (unbypassed) | 0.340132 mA | 3.401321 V | 325.986790 V | 121.553151 V | 34.013210 V, 204.433638 V |
| V2a | `jcm_cf` | 100 kΩ | 820 Ω (bypassed) | 1.519346 mA | 1.245863 V | 208.065440 V | 119.960758 V | 151.934560 V, 88.104683 V |
| Phase inverter | `jcm_ltp` | 82 kΩ | 470 Ω + 10 kΩ tail | 0.187584 mA | 3.928014 V | 344.618092 V | 134.677465 V | 15.381908 V, 209.940627 V |

The two LTP halves share the tail, so each triode sees twice the 10.47 kΩ tail resistance as its
own self-bias resistor. The cathode bias each stage now carries replaces the single −1.5 V the
module used for all four before DA4; that number matched no block's DC point.

`triode_stage_count()` and `grid_excursion_peak_volts()` read each stage's peak grid excursion
in that order. Both are test hooks, like `set_power_tube_grid_bias_mismatch_volts()`; neither is
in the descriptor.

## Two derived factors that are not stage constants

- **`kPhaseInverterDegeneration`, 1/8.789641.** The inverter is a long-tailed pair, so its shared
  cathode follows the driven grid and the grid-to-cathode window is the input divided by
  `1 + (µ + 1)·R_tail/(r_a + R_a)`. With µ 100, the 2 × 10.47 kΩ shared tail, R_a 82 kΩ and
  r_a 189506.723 Ω read off the Koren Jacobian at the idle point above, that factor is 8.789641.
  Modelling the inverter as a plain common-cathode stage gives it a window it does not have.
- **`kPowerTubeGridScale`, 0.125.** `PentodeStage` is not volt-true: `kEl34`'s grid saturates at
  `1/(0.05 × 8)` = 2.5 model volts where the preset's own `cathode_bias_volts` field asks for
  |−20|, so the inverter-to-power-grid link carries 2.5/20. The 2203's real bias is nearer −35 V;
  the preset's field is what this scale has to match. It goes away when `PentodeStage` becomes
  volt-true in DA5.

## What is approximated rather than transcribed

- **The stage gains, 60 for the three 12AX7 stages and 40 for the inverter.** They are the D6a
  single-number approximations of each block's mid-band gain, not values read off a load line.
  D6d replaces them stage by stage with a DK solve.
- **The phase inverter is one triode with a gain of 40**, with the pair's tail degeneration
  applied at its input. A 2203's inverter is the long-tailed pair `jcm_ltp` describes; the plate
  swing above is the driven half's.
- **The tone stack is three biquads.** `JcmToneStack` shapes with a low shelf, a peak and a high
  shelf. The FMV network in `jcm_fmv` also loses 7.84 dB at 1 kHz with all three controls at
  0.5 (`jcm_fmv_ac.csv`, setting 1), and the biquad cascade passes that band at unity.
- **The Miller grid poles use a common 100 kΩ source impedance** for all three driven grids. The
  constants' comment in `Jcm800.cpp` gives the real source impedance at each grid and the
  direction of the error.
- **The EL34 bias mismatch is an input-referred 0.03 V grid offset** on the second tube, because
  `PentodeStage`'s node equation is bias-invariant and cannot express it as a cathode voltage.

## Level staging

The module used to carry three interstage scalars — `kV1bToV2aAttenuation`,
`kV2aToPhaseInverterAttenuation` and `kPhaseInverterToPowerTubeAttenuation`, at 1/50, 1/50 and
1/32 — whose job was to keep each grid inside its solver bound. They were not circuit values, and
the plate-swing spec removed them. `kPreampPotMaxAttenuation` stays because it is the preamp pot,
and `kPowerTubeToTransformerAttenuation` stays as the output trim from the EL34 plates to full
scale. What replaced the last two is not staging: `kPhaseInverterDegeneration` is the pair's own
tail feedback and `kPowerTubeGridScale` is a unit conversion, both derived above.

The V1b → V2a link is at unity, because neither the `jcm_*` decks nor the FMV sweep gives it a
divider. V1b's plate reaches V2a's grid through a coupling cap alone.

## Grid excursions at the default setting

Peak |Vg − Vk| per stage after 188 blocks of 256 at 48 kHz, default knobs, a −12 dBFS 82 Hz sine,
measured 2026-09-07. These are measurements, not assertions; no test pins them.

| Stage | Peak grid excursion |
| --- | --- |
| V1a | 0.2239 V |
| V1b | 0.1211 V |
| V2a | 4.1360 V |
| Phase inverter | 4.2892 V |

V1a and V1b stay below their conduction knees. V2a sits exactly on the solver's 160·Vt = 4.136 V
cutoff fence and the inverter is on its own bound, so both are in hard cutoff for part of every
cycle at the default setting. That is what the unity V1b → V2a link and V2a's gain of 60 produce,
and spec section 4 records the maintainer's ruling that the circuit stays and the acceptance
criterion moves to the clean end.

At silence the amp is silent: 2 s of an all-zero input renders an output whose peak and DC mean
over the last 0.5 s are both exactly 0, at the default setting and at preamp 0.0 alike.

## Acceptance figures

Measured 2026-09-07, all at 48 kHz with a 256-frame block:

- **A clean end exists.** Preamp 0.01 (the first position that passes signal;
  `preamp_attenuation(0)` is exactly zero), −40 dBFS 1 kHz: THD 0.110 %.
- **THD does not fall as the knob rises.** At a 0.2 input: 20.373 % at preamp 0.02, 43.650 % at
  0.1, then 44.705, 44.840, 44.887, 44.924, 44.950, 44.969, 44.982, 44.982 and 44.996 % at 1.0.
- **Default settings peak at −6.0000 dBFS** for a −12 dBFS 82 Hz sine.
- **The output tracks its input.** At preamp 0.02, a 0.01 input peaks at 0.065880 and a 0.251
  input at 0.303885.

## Calibration

`Jcm800::kMasterUnityGain` is 0.3106856, re-measured on 2026-09-07 after the staging scalars came
out and the two derived factors went in. It puts a −12 dBFS 82 Hz sine at default knobs out at
−6.0000 dBFS, which is what `Jcm800DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput`
pins. The previous value, 3.5825659, was calibrated in D5b against the staged chain.

## Signal path and rates

`prepare()` builds the bright cap and the presence shelf at the host rate. Everything from V1a
through the sag stage runs inside a single 8x oversampled region, so the coupling cap, the three
Miller poles, the tone stack, the output transformer and the sag stage are all derived at the
oversampled rate. The presence shelf, the master volume and the cabinet run after the decimator,
where they cost eight times less and generate nothing that could alias.
