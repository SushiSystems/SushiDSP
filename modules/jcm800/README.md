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
| Phase inverter | `jcm_ltp` | 82 kΩ | 20.94 kΩ (the shared 470 Ω + 10 kΩ tail) | 0.187584 mA | 3.928014 V | 344.618092 V | 134.677465 V | 15.381908 V, 209.940627 V |

The two LTP halves share the tail, so each triode sees twice the 10.47 kΩ tail resistance as its
own self-bias resistor. The cathode bias each stage now carries replaces the single −1.5 V the
module used for all four before DA4; that number matched no block's DC point.

`triode_stage_count()` and `grid_excursion_peak_volts()` read each stage's peak grid excursion
in that order. Both are test hooks, like `set_power_tube_grid_bias_mismatch_volts()`; neither is
in the descriptor.

## Derived factors that are not stage constants

- **`kPhaseInverterDegeneration`, 1.853029, divided into the inverter's input.** The inverter is
  a long-tailed pair, so its shared cathode follows the driven grid. The other triode's cathode
  presents `(r_a + R_a)/(µ + 1)` = 271506.723/75.254859 = 3607.8298 Ω in parallel with the tail,
  so the grid-to-cathode signal is the input over `1 + R_tail/(R_tail + that)` =
  1 + 20940/(20940 + 3607.8298). µ is 74.254859 = gm·r_a from the Koren Jacobian at the idle
  point above, with gm 391.832 µS and r_a 189506.723 Ω.
- **`kPhaseInverterGain`, 22.426327.** One triode's full load-line gain,
  `µ·R_a/(r_a + R_a)` = 74.254859 × 82000/271506.723. It is the whole single-triode gain: the
  pair's split already lives in the degeneration factor, and halving it here would count that
  split twice. It replaces the D6a approximation of 40, which is 78 % above it.
- **`kPowerTubeGridScale`, 0.125.** `PentodeStage` is not volt-true: `kEl34`'s grid saturates at
  `1/(0.05 × 8)` = 2.5 model volts where the preset's own `cathode_bias_volts` field asks for
  |−20|, so the inverter-to-power-grid link carries 2.5/20. The 2203's real bias is nearer −35 V;
  the preset's field is what this scale has to match. It goes away when `PentodeStage` becomes
  volt-true in DA5.
- **`kV1bTreblePeakerDivider`, 0.680272, with a 3.34635 dB shelf at 873.541 Hz.** The 2203 feeds
  V1b's grid through a 470 kΩ resistor bypassed by 470 pF, into a 1 MΩ grid leak
  (`jcm_v1b_cold`). At mid frequencies that is 1 M/(470 k + 1 M); the 470 pF puts a zero at
  1/(2π·470k·470p) = 720.484 Hz and the network's pole at 1.47 times that, 1059.112 Hz, so the
  divider returns to unity over the top end. The shelf's corner is the geometric mean of the two.
- **`kToneStackInsertionLoss`, 0.4030488.** The passive 2203 stack loses 7.8928 dB at 1 kHz with
  all three controls at 0.5, measured here on `FmvToneStack(kJcm2203ToneStack)` and matching the
  −7.84 dB `jcm_fmv`'s own sweep reads. The loss belongs to the passive stack; `JcmToneStack`'s
  three biquads are unity at flat and do not carry it, so it is applied at the stack's output. It
  goes away when `FmvToneStack` replaces `JcmToneStack`.

## What is approximated rather than transcribed

- **The stage gains, 60 for the three 12AX7 stages and 40 for the inverter.** They are the D6a
  single-number approximations of each block's mid-band gain, not values read off a load line.
  D6d replaces them stage by stage with a DK solve.
- **The phase inverter is one triode**, with the pair's tail degeneration divided into its input.
  A 2203's inverter is the long-tailed pair `jcm_ltp` describes; the plate swing above is the
  driven half's, and the second half's plate is not in the model at all.
- **The treble peaker's shelf is a Q 0.707 biquad** where the network has one zero and one pole
  1.47 apart. Over 3.34635 dB the shape error is small, but the transition is second-order where
  the circuit's is first.
- **The plate bottom is solved with the cathode pinned at its idle voltage.** For V1a, V2a and
  the inverter that is what the bypass capacitor or the tail does anyway. V1b is the exception:
  its 10 kΩ cathode is unbypassed, so the real cathode rises with the plate current and the true
  bottom sits above the 121.553151 V in the table.
- **The tone stack is three biquads.** `JcmToneStack` shapes with a low shelf, a peak and a high
  shelf, and its flat insertion loss is carried as the constant above rather than by the network.
  Its shape through the mids is not the FMV stack's: part 1's Task 4 measured it 6.35 / 8.68 /
  15.93 dB above `FmvToneStack` at MID 0.1 / 0.5 / 0.9.
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
| V1b | 0.1112 V |
| V2a | 2.6627 V |
| Phase inverter | 4.3314 V |

V1a and V1b stay below their conduction knees, V2a stays inside the solver's 160·Vt = 4.136 V
cutoff fence, and the inverter is on its bound, so it is in hard cutoff for part of every cycle.

At silence the amp is silent: 2 s of an all-zero input renders an output whose peak and DC mean
over the last 0.5 s are both exactly 0, at the default setting and at preamp 0.0 alike.

## Acceptance figures

Measured 2026-09-07, all at 48 kHz with a 256-frame block:

- **A clean end exists.** Preamp 0.01 (the first position that passes signal;
  `preamp_attenuation(0)` is exactly zero), −40 dBFS 1 kHz: THD 0.190 %.
- **THD does not fall as the knob rises.** At a 0.2 input: 15.324 % at preamp 0.02, 42.589 % at
  0.1, then 44.548, 44.790, 44.855, 44.886, 44.915, 44.937, 44.957, 44.972 and 44.982 % at 1.0.
- **Default settings peak at −6.0000 dBFS** for a −12 dBFS 82 Hz sine.
- **The output tracks its input.** At preamp 0.02, a 0.01 input peaks at 0.045959 and a 0.251
  input at 0.283594.

## Known issue: the alias floor

`Regression_Dsp.Jcm800AliasFloorImprovedAtLeast30dBOverD5b` is skipped, and the measurement still
runs and still prints its figure. The amp measures −6.7 dB at preamp 1.0 and −12 dBFS, against
−43.4 dB before DA4 removed the level staging. Every stage now clips at the level its own circuit
puts it at, and the 8× region folds the harmonics that clipping generates. The −33.8 dB bound
stands and is not relaxed; `docs/reference/KNOWN_ISSUES.md` carries the item until DA5's
antialiasing work (ADAA in `TriodeStage`, or 16× oversampling) restores the floor.

The two losses on this page each moved the figure the wrong way:

| Build | Alias floor |
| --- | --- |
| Before the first-order coupling | −12.6 dB |
| First-order coupling only | −11.3 dB |
| + treble peaker only | −13.6 dB |
| + stack insertion loss only | −6.8 dB |
| Both losses | −3.0 dB |
| Both losses, corrected long-tailed pair (shipped) | −6.7 dB |

Attenuating ahead of V2a moves the later stages out of hard limiting into the region where they
generate more distinct harmonics to fold, and the fundamental shrinks with them.

## Calibration

`Jcm800::kMasterUnityGain` is 0.3108168, re-measured on 2026-09-07. It puts a −12 dBFS 82 Hz sine
at default knobs out at −6.0000 dBFS, which is what
`Jcm800DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput` pins. The previous value,
3.5825659, was calibrated in D5b against the staged chain.

## Filter shapes and their rates

| Filter | Shape | Corner | Rate |
| --- | --- | --- | --- |
| Bright cap | second-order high-pass, Q 0.707 | 60 Hz | host rate |
| V1a → V1b coupling cap | first-order high-pass | 120 Hz | 8 × host rate |
| V1b treble peaker | high shelf, Q 0.707 | 873.541 Hz | 8 × host rate |
| Three Miller grid poles | one-pole low-pass | 13.8 kHz (×2), 19.6 kHz | 8 × host rate |
| Presence | high shelf, Q 0.707 | 3 kHz | host rate |

The coupling capacitor is first-order because that is what a cap into a grid leak is, and because
`Biquad::set_high_pass` documents a floor of `cutoff_hz / sample_rate >= 1e-4`: 120 Hz against
8 × 192 kHz is 7.8e-5, under it. The bright cap is the module's one remaining second-order
high-pass; at the host rate its ratio is 1.25e-3 at 48 kHz and 1.56e-4 at 384 kHz, both above the
floor. It is a treble bypass across the pot rather than a coupling cap, so this round left its
shape alone.

## Signal path and rates

`prepare()` builds the bright cap and the presence shelf at the host rate. Everything from V1a
through the sag stage runs inside a single 8x oversampled region, so the coupling cap, the three
Miller poles, the tone stack, the output transformer and the sag stage are all derived at the
oversampled rate. The presence shelf, the master volume and the cabinet run after the decimator,
where they cost eight times less and generate nothing that could alias.
