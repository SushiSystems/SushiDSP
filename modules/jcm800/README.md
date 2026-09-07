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
| Phase inverter | `jcm_ltp`, grid leaks to the tail junction | 82 kΩ | 470 Ω + 10 kΩ shared tail | 1.431927 mA | Vgk −1.346012 V (cathodes at 29.984558 V) | 242.581959 V | Vpk 212.597400 → 123.049256 V | 117.418041 V, 89.548144 V |

The two LTP halves share the tail. The 2203 returns both grid leaks to the junction of the 470 Ω
and the 10 kΩ, so each grid sits at the 10 kΩ's top and Vgk is the drop across the 470 Ω alone,
−2·Ia·470; the `jcm_ltp` deck grounds them (`Rg1 g1-0`, `Rg2 g2-0`), which had the pair
self-biasing against the whole 10.47 kΩ at 0.187584 mA, Vgk −3.928 V, and left its plate
15.38 V from the supply. Re-solved on the 2203's wiring the pair draws 1.432 mA per half and its
plate sits 117.4 V under the supply; the headroom toward the supply is `Ia·Ra`, the plate's own
travel to B+, and the 147.4 V the DA5 diagnosis printed for this point was B+ − Vpk, which adds
the 30 V cathode to it. The cathode bias each stage carries replaces the single −1.5 V the module
used for all four before DA4; that number matched no block's DC point.

`triode_stage_count()` and `grid_excursion_peak_volts()` read each stage's peak grid excursion
in that order. Both are test hooks, like `set_power_tube_grid_bias_mismatch_volts()`; neither is
in the descriptor.

## Derived factors that are not stage constants

- **`kV1aGain`, `kV1bGain`, `kV2aGain`: 55.432497, 7.774305, 67.001549.** Each is the Koren
  Jacobian at the stage's own idle point above: `µ·R_a/(r_a + R_a)` for the two bypassed
  cathodes (V1a gm 1.5025 mA/V, r_a 58.465 kΩ, µ 87.84; V2a gm 2.2682 mA/V, r_a 41.923 kΩ,
  µ 95.09), and `µ·R_a/(r_a + R_a + (µ + 1)·R_k)` for V1b's unbypassed 10 kΩ (gm 0.6812 mA/V,
  r_a 115.768 kΩ, µ 78.86), the cold clipper's 17.8 dB against the 35.6 dB the module gave it
  until DA5 part 2; `jcm_v1b_cold`'s own AC sweep reads +16.89 dB at 1 kHz. The Miller pole on
  each driven grid reads its stage's gain, so V1b's corner moves from 13.8 kHz to 60.0 kHz
  and V2a's to 12.5 kHz, the inverter's from 31.0 to 13.4 kHz.
- **`kPhaseInverterDegeneration`, 1.888775, divided into the inverter's input.** The inverter is
  a long-tailed pair, so its shared cathode follows the driven grid. The other triode's cathode
  presents `(r_a + R_a)/(µ + 1)` = 125055.615/95.443529 = 1310.2577 Ω in parallel with the tail,
  so the grid-to-cathode signal is the input over `1 + R_tail/(R_tail + that)` =
  1 + 10470/(10470 + 1310.2577). µ is 94.443529 = gm·r_a from the Koren Jacobian at the idle
  point above, with gm 2.193524 mA/V and r_a 43055.615 Ω. R_tail is the 10.47 kΩ both cathodes
  share — `jcm_ltp`'s 470 Ω and 10 kΩ in series to ground, both in the tail, so neither goes in
  the shunt branch. The undriven grid's return to the same junction is not in the factor; it
  keeps the plain form the 5150's pair uses.
- **`kPhaseInverterGain`, 61.927402.** One triode's full load-line gain,
  `µ·R_a/(r_a + R_a)` = 94.443529 × 82000/125055.615. It is the whole single-triode gain: the
  pair's split already lives in the degeneration factor, and halving it here would count that
  split twice. It was 22.426327 on the deck's grounded grid leaks and 40 in D6a.
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

- **The stage gains are small-signal figures at the idle point.** Each is the Koren Jacobian
  on its load line, so a stage's gain does not move with its plate current as the real tube's
  does; D6d replaces them stage by stage with a DK solve. V1b's cathode is unbypassed, so its
  grid-to-cathode window is wider than |Vk| by the cathode's own signal, and the model, like the
  5150's V2A, does not widen it.
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
- **The EL34 bias mismatch is an input-referred 0.24 V grid offset** on the second tube. It was
  0.03 V in the model volts of the bias-referenced `PentodeStage`, which could not express it
  as a cathode voltage; through the 0.125 grid scale that stage needed, that is 0.24 real volts,
  and the number was carried over rather than re-derived. The volt-true stage can express a bias
  difference directly, and the module still does not.

## Level staging

The module used to carry three interstage scalars — `kV1bToV2aAttenuation`,
`kV2aToPhaseInverterAttenuation` and `kPhaseInverterToPowerTubeAttenuation`, at 1/50, 1/50 and
1/32 — whose job was to keep each grid inside its solver bound. They were not circuit values, and
the plate-swing spec removed them. `kPreampPotMaxAttenuation` stays because it is the preamp pot,
and `kPowerTubeToTransformerAttenuation` stays as the output trim from the EL34 plates to the
transformer's units: the pair's full swing, `kEl34`'s two headrooms summed (305.778 V), lands at
the 2.0 the transformer stage received at full clip before DA5 part 2 (2 × 20 model volts × 0.05),
so the transformer and the sag stage see the range they were calibrated for. What replaced the
staging is not staging: `kPhaseInverterDegeneration` is the pair's own tail feedback, derived
above.

## The EL34 pair

`kEl34` (`include/SushiDSP/circuits/stages/PowerTubePresets.hpp`) is the Philips EL34
datasheet's operating point for the tube in the 2203's arrangement, and every one of its fields
is that row or the deck. The January 1969 sheet's page 3, "Class B, two tubes in push-pull",
first column: grid No. 2 series resistor 1000 Ω, Vg1 −38 V, Vb 425 V, Va 420 V, Ia 2 × 30 mA at
zero signal and 2 × 120 mA at full, Raa 3.4 kΩ, Vi 27 Vrms, Wo 55 W. That is the 2203's power
stage as `jcm_pp_ot_nfb` draws it — 1 kΩ screen stoppers off the plate supply, −38 V fixed bias,
220 kΩ grid leaks, 5.6 kΩ grid stoppers, a 3.4 kΩ primary — with the datasheet's supply in
place of the deck's 360 V, which the deck's README calls the D6a brief's figure.

| Field | Value | From |
| --- | --- | --- |
| `cathode_bias_volts` | −38 V | the row's Vg1; the grid conducts 38 V above rest, and the row's 27 Vrms drive (38.2 V peak) says full output is reached exactly there |
| `plate_swing.headroom_to_supply_volts` | 25.5 V | Ia0·R_L = 30 mA × 850 Ω, the cut-off tube's travel; R_L is Raa/4, one tube's load with the other cut off |
| `plate_swing.headroom_to_bottom_volts` | 280.2777 V | √(2·Wo·Raa)/2 − 25.5 = 305.7777 − 25.5, the plate's peak swing at 55 W into 3.4 kΩ less the idle drop; the 359.7 mA peak this implies averages 114.5 mA over a half-sine, against the row's 2 × 120 mA at full signal |
| `gain` | 6.882361 | gm·(R_L ∥ Ri) at 30 mA: the quick-reference 12.5 mA/V at 100 mA scaled by (30/100)^⅓ = 8.3679 mA/V, Ri 17 kΩ at 100 mA scaled by (100/30)^⅓ = 25.4 kΩ; the class A point at 70 mA (11 mA/V, 20 kΩ) gives 6.830539 |
| `source_resistance_ohms` | 33831.9 Ω | (82 kΩ ∥ 43.056 kΩ) + 5.6 kΩ: the corrected inverter's plate impedance and the deck's grid stopper |
| `grid_leak_resistance_ohms` | 220 kΩ | the deck's grid leaks |
| `grid_conduction` | IG0 1 µA, VGAMMA 0.5 V, VTG 0.15 V | `el34_koren.sub` through `PentodeEL34`, the corpus's grid law |

The pair's composite, `el34_a(u) − el34_b(−u)`, is odd in the drive while both grids are inside
their windows and reaches ±305.8 V: the conducting tube's 280 V and the cut-off tube's 25.5 V.
The cut-off side saturates at 25.5/6.88 = 3.7 V of grid drive, which is the class-AB handover; past
it one tube carries the signal alone. The repository's Koren EL34 fit reads gm 5.61 mA/V and
Ip 40.7 mA at this point, against the datasheet's 30 mA and the 8.37 mA/V scaled from its printed
transconductance, so the preset takes the datasheet and not the fit; the design's D6b section
already records that fit as running hot. The −20 V, gain 8, ceiling 20 preset the module carried
until DA5 part 2 had been fitted to the golden files with no operating point behind it.

The V1b → V2a link is at unity, because neither the `jcm_*` decks nor the FMV sweep gives it a
divider. V1b's plate reaches V2a's grid through a coupling cap alone.

## Grid excursions at the default setting

Peak |Vg − Vk| per stage after 188 blocks of 256 at 48 kHz, default knobs, a −12 dBFS 82 Hz sine,
measured 2026-09-07. These are measurements, not assertions; no test pins them.

| Stage | Peak grid excursion |
| --- | --- |
| V1a | 0.2655 V |
| V1b | 0.2741 V |
| V2a | 0.8480 V |
| Phase inverter | 18.9606 V |

V1a, V1b and V2a stay below the 0.5 V knee of the Norman grid law `TriodeStage` carries since
DA5 (`docs/superpowers/specs/2026-09-07-grid-knee-antialiasing-design.md` §3), V2a because V1b
now drives it at the cold clipper's 7.77 rather than 60. The inverter's grid travels 19.0 V,
where it travelled 63.6 V against the deck-biased pair's 15.38 V plate ceiling: with 117.4 V of
headroom the pair's ceiling is reached at a fifth of the drive it took before, and the grid's
10 kΩ source still drives the 150 kΩ conducting slope of the law, so the grid follows the source
and the plate ceiling does the clipping. Nothing holds a grid short of its plate ceiling; the
4.136 V solver fence went with the Shockley knee.

At silence the amp is silent: 2 s of an all-zero input renders an output whose peak and DC mean
over the last 0.5 s are both exactly 0, at the default setting and at preamp 0.0 alike.

## Acceptance figures

Measured 2026-09-07, all at 48 kHz with a 256-frame block:

- **A clean end exists.** Preamp 0.01 (the first position that passes signal;
  `preamp_attenuation(0)` is exactly zero), −40 dBFS 1 kHz: THD 0.174 %.
- **THD does not fall as the knob rises.** At a 0.2 input: 6.113 % at preamp 0.02, 19.754 % at
  0.1, then 33.207, 38.318, 40.526, 41.596, 42.162, 42.483, 42.677, 42.800 and 42.881 % at 1.0.
  On the deck-biased inverter and V1b at 60 the same sweep read 16.2 % at 0.02 and 42.7 % at
  0.1: the knob now has a travel.
- **Default settings peak at −6.0000 dBFS** for a −12 dBFS 82 Hz sine.
- **The output tracks its input.** At preamp 0.02, a 0.01 input peaks at 0.007612 and a 0.251
  input at 0.123296, a factor of 16.2 where it was 5.95.

## Known issue: the alias floor

`Regression_Dsp.Jcm800AliasFloorImprovedAtLeast30dBOverD5b` is skipped, and the measurement still
runs and still prints its figure. The amp measures −13.4 dB at preamp 1.0 and −12 dBFS, against
−43.4 dB before DA4 removed the level staging. Every stage now clips at the level its own circuit
puts it at, and the 8× region folds the harmonics that clipping generates. The −33.8 dB bound
stands and is not relaxed; `docs/reference/KNOWN_ISSUES.md` carries the item until DA5's
antialiasing work (ADAA in `TriodeStage`, or 16× oversampling) restores the floor.

Measured through `measure::alias_floor()` at −12 dBFS, in the order the chain changed:

| Build | Preamp 0.5 | Preamp 1.0 | Preamp 0.02 |
| --- | --- | --- | --- |
| Shockley knee (DA4) | −18.4 dB | −6.7 dB | −24.4 dB |
| Norman grid law (DA5 part 1) | −11.2 dB | −15.1 dB | −24.6 dB |
| Volt-true EL34 pair, cold clipper at 7.77, inverter at its 2203 bias (DA5 part 2) | −16.3 dB | −13.4 dB | −48.0 dB |

Part 1 cost the default setting 7.2 dB because the inverter's 10 kΩ source barely loads the
Norman slope and the grid struck the deck-biased pair's 15.38 V ceiling; part 2's inverter
correction recovers 5.1 dB of it and does not close it. Halving the transformer's drive and
removing the pair's cut-off ceiling each move the figure by under 0.5 dB, so what remains is the
preamp's own fold at the drive the circuit delivers.

The two losses on this page each moved the DA4 figure the wrong way:

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

`Jcm800::kMasterUnityGain` is 0.2534701, re-measured on 2026-09-07 on the volt-true EL34 pair
and the corrected inverter. It puts a −12 dBFS 82 Hz sine at default knobs out at −6.000 dBFS,
which is what `Jcm800DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput` pins; 82 Hz is
the frequency all four amplifiers pin at (DA5 spec §6). It read 0.3105599 on the Norman grid law
alone, 0.3101991 on the Shockley knee and 3.5825659 in D5b against the staged chain.

## Filter shapes and their rates

| Filter | Shape | Corner | Rate |
| --- | --- | --- | --- |
| Bright cap | second-order high-pass, Q 0.707 | 60 Hz | host rate |
| V1a → V1b coupling cap | first-order high-pass | 120 Hz | 8 × host rate |
| V1b treble peaker | high shelf, Q 0.707 | 873.541 Hz | 8 × host rate |
| Three Miller grid poles | one-pole low-pass | 60.0 kHz (V1b), 12.5 kHz (V2a), 13.4 kHz (inverter) | 8 × host rate |
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
