# `modules/mesa_mark2c` — Mesa/Boogie Mark IIC+, Lead mode

One `IAudioNode`, mono in and mono out, registered as `sushidsp.mesa_mark2c` with fifteen
parameters: Volume 1, Treble, Bass, Middle, Lead Drive, Lead Master, Master 1, Presence, the five
graphic-equaliser sliders, Input Trim and Output. The PULL switches on the pots are not exposed;
every one of them is fixed at the position the dossier reads for Lead mode.

Every component value, every gain and every operating point comes from
[`docs/reference/research/2026-09-06-mesa-mark-iic-plus-design-dossier.md`](../../docs/reference/research/2026-09-06-mesa-mark-iic-plus-design-dossier.md).
A number that is not in that file is marked below as a modelling choice. Nobody has published a
bench measurement of a Mark IIC+, so the whole module is calibrated against arithmetic on a 1983
drawing and on jrb32's trace of a real chassis (dossier S8).

## Which channel

Lead mode, the one the amp is bought for: V1A and V1B feed V3B, V4A and V2A through the two lead
pots, and V2B returns from the effects loop into Master 1. The Rhythm path taps off V1B's plate
and is not modelled. Reverb (V3A, V4B) is not modelled either.

## The tone stack sits on V1A's plate

Dossier §2: the FMV stack hangs directly on the first triode's plate, so the four lead gain
stages downstream of it distort a signal the tone controls have already shaped, which is what
separates a Mark from a Marshall. `MesaMark2cToneStack` is therefore instantiated between `v1a_`
and the Volume 1 attenuation in `MesaMark2c::process()`, and
`MesaMark2cToneStackSitsBeforeTheLeadStages` pins the consequence at Lead Drive 0.05 and a −40 dBFS
input, where the lead chain is on its knee: treble at 1 against 0 raises THD by 8.50 pp.

## The blocks and who owns each

| Block | Class |
| --- | --- |
| The FMV stack of §4.1 | `MesaMark2cToneStack`, wrapping `FmvToneStack` |
| Each of the seven 12AX7 halves | `TriodeStage` |
| The five EQ traps | `GraphicEqualizer` |
| Each Simul-Class output half | `PentodeStage`, from `kMesaMark2cPower6L6GC` / `kMesaMark2cPowerEl34Triode` |
| Transformer, sag, cabinet | `OutputTransformerStage`, `PowerSupplySagStage`, `StreamingConvolution` |
| Every coupling cap, cathode shelf, divider shelf, Miller pole and the presence shelf | `Biquad` |
| The chain's order and the constants | `SushiDSP::MesaMark2c` |

## Stage order and the numbers behind it

Read down; everything from V1A to the sag stage runs inside one 8x oversampled region.

| Stage | Constant | Plate swing (up / down, V) | Dossier |
| --- | --- | --- | --- |
| V1A, Rp 150 k, Rk 1.5 k, Vk 1.6 V, source 10 k | gain 70.6; §3.1 draws no grid stopper, so `kV1aSourceResistance` borrows the JCM800's 10 k | 160.0 / 120.961 (D 382 V, Va 222 V, bottom 101.039 V) | §3.1 |
| Tone stack on the plate | 250 pF / 0.1 µF / 0.047 µF, 100 k slope, 250 k / 10 k / 250 k pots | — | §4.1 |
| Volume 1, 1 M | audio taper, no ceiling (see below) | — | §2 |
| V1B, Rp 100 k, Vk 1.9 V | gain 61.5 | 127.0 / 128.851 (D 382 V, Va 255 V, bottom 126.149 V) | §3.2 |
| V1B coupling, 0.1 µF into 100 k | high-pass **15.92 Hz** (computed) | — | §3.2 |
| Lead Drive bright cap, 180 pF | high shelf at **1881 Hz** (computed from 470 k) | — | §3.3 |
| 680 k into Lead Drive 1 M | attenuation 0.595 at maximum | — | §3.3 |
| V3B, Rp 82 k, Vk 1.8 V | gain 56.7 | 98.0 / 151.554 (C 392 V, Va 294 V, bottom 142.446 V) | §3.3 |
| V3B coupling, 0.02 µF into 170 k + 68 k | high-pass **33.44 Hz** (computed) | — | §3.4 |
| Interstage network, 68 k with 0.047 µF across the 170 k | low shelf: 0.286 below, unity above, zero **19.939 Hz**, pole 69.717 Hz | — | §3.4, open question 18 |
| V4A, Rp 270 k, Rk 3.3 k, Vk 1.2 V | gain 70 | 98.0 / 221.216 (C 392 V, Va 294 V, bottom 72.784 V) | §3.4 |
| V4A cathode, 0.22 µF on 3.3 k | zero 219.22 Hz, pole **438.97 Hz**, loss below **0.4994** (−6.03 dB) | — | §3.4 |
| V4A coupling, 0.047 µF into 320 k | high-pass **10.58 Hz** (computed) | — | §3.5 |
| Lead output divider, 220 k ∥ 250 pF over 3.3 k ∥ 500 pF | 0.014778 at DC (−36.6 dB), zero **2893.7 Hz**, pole **65269.6 Hz**, 0.3333 above; the shunt is S8's traced R30, not S1's `100K` glyph (§8 question 21) | — | §3.5 |
| V2A, Rp 100 k, Rk 1.5 k unbypassed, Vk 1.935 V, grid 680 k ∥ 47 pF, source 220 k ∥ 3.3 k = 3.25 k | gain **22.005** = 91.561 × 58.746 / (58.746 + 46.848 + 92.561 × 1.5), the Koren model's local µ and r_a at 1.2902 mA into the loop network's 58.746 k AC load | 75.795 / 114.171 (C 392 V, Va 263.0 V, on the 58.746 k AC load line) | §3.6 |
| Loop network: 47 k (R105) and 0.047 µF (C9) to W; W to ground through the 250 k Lead Master rheostat; W ─ 150 k (R102) ─ P; P ─ 4.7 k (R101) to ground; P ─ 2.2 k (R12) ─ SEND, normalled to RETURN at V2B's grid with its 47 k (R103) | coupling high-pass **16.33 Hz**; at Lead Master maximum 0.6699 × 0.02770 × 0.9533 = **0.01769** (−35.0 dB) from the capacitor's node to V2B's grid; `loop_network_gain()` carries the rheostat's law, Z_W / (47 k + Z_W) with Z_W = pot ∥ 154.27 k | — | §3.6a |
| V2B, Rp 170 k, Rk 1 k, source 5.909 k | gain **73.118** (computed: 100 × 170 / 232.5) | 204.0 / 92.338 (C 392 V, Va 188 V, bottom 95.662 V) | §3.7 |
| V2B cathode, 0.47 µF on 1 k, PULL DEEP out | zero 338.63 Hz, pole **485.73 Hz**, loss below **0.6972** (−3.13 dB) | — | §3.7 |
| V2B coupling, 0.047 µF into 100 k | high-pass **33.86 Hz** (computed) | — | §3.7 |
| Master 1, 100 k, at node F | dB control, `kMasterUnityGain` 0.0038289 at 0 dB | — | §2 |
| Graphic EQ | 71.5 / 371.8 / 979.5 / 1576 / 4823 Hz at Q 1.434 / 1.938 / 1.571 / 0.673 / 1.0, ±17 dB | — | §4.2 |
| V5A/V5B long-tailed pair, Rp 82 k | gain **68.093628** = 96.378299 × 82 k / (82 k + 34.061 k), tail degeneration **2.2552703** (see below), divided by | 106.0 / 143.768 (C 392 V, Va 286 V, bottom 142.232 V) | §5.1 |
| Simul-Class output | `kMesaMark2cPower6L6GC` ×2 and `kMesaMark2cPowerEl34Triode` ×2, in real volts | inner 81.2 / 311.228 V, outer 132.288 / 132.288 V | §5.3, §7.2, §7.3, RCA 6L6-GC sheet |
| Presence, 0.022 µF with 6.8 k | high shelf at **1063.9 Hz** (computed), ±9 dB | — | §5.2 |

The six Miller poles land at 9.0 kHz (V1B), 14.5 kHz (V3B), 14.7 kHz (V4A), 11.6 kHz (V2A),
11.6 kHz (V2B) and 15.2 kHz (the inverter). Each is computed from the JCM800's 12AX7 capacitances
and the plate load that drives the grid. V2B's is the one that no longer describes the circuit:
its grid is driven from the loop's 5.9 k, which puts the real corner near 196 kHz, above the
oversampled Nyquist, so the 100 k pole the model keeps there is a placeholder the band does not
reach; deleting it is the maintainer's call.

## Where the model departs from the dossier

**What the plate bottoms assume.** Each `PlateSwing`'s conduction side is the plate voltage where
the load line meets the Koren model at `Vgk = 0`, and two simplifications go into that. Every
cathode is held at the idle `Vk` the dossier prints, including V4A's partially bypassed 3.3 k and
V2B's 1 k under PULL DEEP, so a cathode that would rise with signal does not. And `Vgk = 0` stands
in for the conduction limit rather than the grid being solved into conduction against its own
source resistance. Both push the bottom slightly low, which makes the conduction-side headroom
slightly generous; the D6d load-line solve replaces them stage by stage when it lands.

**Every coupling capacitor is a first-order section.** One capacitor into one resistance is one
pole, so the five coupling links use `Biquad::set_first_order_high_pass` rather than the biquad
`set_high_pass`. That is also the numerically safe path at this module's 384 kHz oversampled rate:
a conjugate pole pair placed at a corner this far below the rate lands within a float ulp of the
unit circle, which a loop-return corner at 16.33 Hz does not survive. The corners are unchanged
and still come from the dossier's R and C.

**The effects loop network and Master 1's place.** Until 2026-09-08 the model connected V2A's
plate to V2B's grid through a 0.022 µF stand-in and nothing else, and scaled the output by Master 1
after the power section; V2B's grid took 27.9 V against a 1.26 V window, the inverter's 21.1 V,
and the amp had no clean setting. The network of §3.6a is now in the chain as the table above
draws it, read from the factory sheet and jrb32's hardware trace, and Master 1 multiplies node F
ahead of the equaliser and the inverter, where S1 draws it. Measured at 48 kHz through a probe
compiled from source, before and after: THD at Lead Drive 0.02 and −40 dBFS 40.70 → 0.20 %; the
750 Hz slider's lift at 979.5 Hz 0.00 → 16.09 dB at Master 0 dB and 16.93 dB at −20 dB; treble 1
against 0 at Lead Drive 0.05 and −40 dBFS −0.13 → +8.50 pp of THD; the Lead Master from 1.0 to
0.05 at Lead Drive 0.5 moves the output −5.7 → −25.4 dBFS where it moved nothing; the alias floor
at the default and −12 dBFS −14.8 → −19.8 dB and at Lead Drive 0.02 −22.3 → −34.0 dB. The three
tests this cost are back: `MesaMark2cHasACleanEndAtMinimumGain` at its own point,
`MesaMark2cToneStackSitsBeforeTheLeadStages` at Lead Drive 0.05 and a 0.01 input, and
`MesaMark2cEqBandBoostsItsCentre` with Master 1 at −20 dB, where the power section is linear.
Design: `docs/superpowers/specs/2026-09-08-iicplus-loop-network-design.md`.

Four stand-ins remain in that network, each labelled in the design's section 5. The Lead
Master's taper: the pot is a rheostat and no source states its law, so it takes the model's
`audio_taper` (n·√n) like the other pots, and its top half moves the level by 1.9 dB, its bottom
fifth by 26 dB (§3.6a). The LDR4's on resistance: not read (§8 question 23), so the knob's floor
is the rheostat's 0 Ω, a hard mute, where a real IIC+ leaks a little signal. V2A's load, held at
the pot's maximum: the rheostat moves V2A's AC load from 58.75 k to 32.3 k across its travel,
which would take the gain from 22.0 to 13.6 and the up-swing from 75.8 to 41.7 V; `TriodeStage`
takes one gain and one `PlateSwing`, so the stage keeps the maximum's and `loop_network_gain()`
carries the whole law, an error under 4.2 dB of drive at the knob's bottom where the stage is
26 dB down. And the reverb take-off at W and its return at P are not in the chain.

**Three constants that are not the dossier's.** Driven single-ended, the long-tailed pair's shared
cathode follows the driven grid through the tail, so the grid-to-cathode window is the input divided
by `1 + (R_tail ∥ Z_shunt) / Z_k`. `Z_k` is the driven triode's *own* cathode impedance,
`(r_a + R_a) / (µ + 1)` = 1191.858 Ω, and it stays in the denominator; `Z_shunt` is the branch the
other cathode presents, which is that same `Z_k` plus §5.1's 470 Ω drawn between the cathodes, so
1661.858 Ω. Then `15000 ∥ 1661.858` = 1496.104 and the factor is
`1 + 1496.104 / 1191.858` = **2.2552703**, always divided by. Series resistance between the cathodes
has to *raise* the factor, and it does: without the 470 Ω the same form gives 1.9263915.

µ is the local `gm · r_a` from the Koren Jacobian at the solved point, **96.378299**, not the
model's nominal 100, and the same µ sets the gain: the inverter's `TriodeStage` carries the **full**
single-triode `96.378299 × 82000 / (82000 + 34061.086)` = **68.093628**, because the pair's split is
the degeneration factor's job and halving the gain as well counted it twice. R_tail is §5.1's shared
15 k tail glyph and r_a 34061.086 Ω, both at Vgk −1.5 V, Vpk 284.5 V. That point is the dossier's
printed operating point, not a solved point on the 82 kΩ load line: the Koren model draws 2.946 mA
there while the line from 392 V to the printed 286 V carries 1.293 mA, dossier question 6 holds the
disagreement, and a self-consistent solve would give gain 63.6 or 57.2 with degeneration 2.226 or
2.183.

Since DA5 part 2 `PentodeStage` is volt-true and the inverter's plate reaches both pairs' grids
unscaled; the two grid-scale bridges of 0.0907030 and 0.0619348 are gone. What drives each pair is
in "The output section" below.

**Where each grid sits.** `grid_excursion_peak_volts()` measured at the default setting, a
−12 dBFS 1 kHz sine, 200 blocks of 256 at 48 kHz, on the Norman grid law and the volt-true power
stage of DA5. No solver
fence holds a grid any more; each stage runs until its own plate ceiling saturates, so the
figures from V3B on are the plate swings divided by the gains, reached through the 0.5 V knee.

| Stage | Peak grid excursion |
| --- | --- |
| V1A | 0.2485 V |
| V1B | 0.9685 V |
| V3B | 9.6134 V |
| V4A | 31.6023 V |
| V2A | 13.0027 V |
| V2B | 1.1949 V |
| V5A/V5B phase inverter | 0.1154 V |

The dossier says the same thing in words: §3.8 records that from V3B onward the amp is in hard
grid conduction long before the signal reaches V2A. V2B is the one lead-chain stage that does not
clip, as §3.8 predicts: the network hands it 1.19 V against its 1.26 V window. Before the network
and Master 1's move (2026-09-08) V2A read 16.2602 V, V2B 27.9001 V and the inverter 21.1133 V.

**The interstage drive normalisation, removed.** The model used to divide each triode's output
by that stage's own dossier gain and multiply by one measured figure, `kGridDriveNormalisation`,
because a cascade fed dossier §3.8's numbers ran into `TriodeStage`'s solver bound instead of into
a plate. `TriodeStage` now takes a `PlateSwing` and bounds each plate by its own headroom, so the
spec at docs/superpowers/specs/2026-09-07-triode-plate-swing-design.md deleted
`kGridDriveNormalisation` and the seven `k*GridDrive` constants along with the JCM800's and the
VH4's equivalents. The dossier's dividers and pots are now the only things that change the drive
as it moves down the cascade. `kPhaseInverterToPowerTubeAttenuation` went with them, replaced by
the two per-pair grid scales below, and `kPowerTubeToTransformerAttenuation` 0.05 is the output
trim to full scale.

**Volume 1's ceiling.** The taper law is the JCM800's `preamp_attenuation()`, `n·√n`, but without
that amp's 1/16 ceiling. The 2203's ceiling stands in for a loss the IIC+ already has: its stack
sits ahead of the pot, and at every knob centred it measures −11.3 dB at 100 Hz, −14.9 dB at
1 kHz and −7.9 dB at 3 kHz.

**The 0.047 µF sits across the 170 k, not from the node to ground.** S1 draws 68 k in parallel
with 0.047 µF to ground, which as drawn is a 69.7 Hz low-pass no lead channel could have: with the
0.02 µF coupling cap in front it is a band-pass peaking at 0.171 near 48 Hz and sitting 29 dB down
at 2 kHz. The model places the capacitor across the 170 k, which gives the tightness §3.4's prose
describes — a low shelf, 0.286 below the corner and unity above it, with the pole at exactly the
69.717 Hz §3.4 prints and the zero at 19.939 Hz. It is the only reading under which the prose, the
0.286 and the 69.7 Hz are all true of one circuit, and it replaces both the flat divider and the
separate high-pass, so the low end is not rolled twice. Dossier §8 open question 18.

**The Lead Drive bright cap is read at 470 k.** §8 question 8 leaves its series resistor as either
47 k or 470 k. 47 k puts the corner at 18.81 kHz, where the cap does nothing audible; 470 k puts
it at 1881 Hz, where it works as a bright cap. The model takes 470 k. The shelf's height is
whatever the taper is throwing away at the current pot position, capped at 18 dB, because the cap
shorts the pot's top section above the corner.

**The phase inverter's cathode bias is −1.5 V**, which no source states: §5.1 gives 82 k plate
resistors and printed DC that does not close (§8 question 6). The gain is the full single-triode
68.093628 rather than the 56.75 the dossier's own rp figure would give, because it is taken from the
Koren r_a and local µ at that point, and the pair's split is carried by the degeneration factor
above instead of by halving the gain.

**Presence depth is ±9 dB.** §5.2 gives the network's corner but no depth in decibels. The 9 dB is
the JCM800's constant.

**The Simul-Class summing weight is 0.5 each.** Both pairs see the same phase-inverted grid signal
and are summed at the transformer with equal weight. The 75 W / 25 W split lives in the presets'
plate swing — 392.4 V against 264.6 V, 3.42 dB of staggered clipping where §7.3 computed 2.99 dB
from the same power figures — and not in the summing. §7.3 says as much: the audible content of
Simul-Class is the ceiling difference, not a level difference.

**The output section.** §5.3 establishes the tubes, the printed −45 V grid voltage, the 2.2 kΩ
stoppers and the 220 kΩ leak at V8, and leaves the idle current and the primary impedance
unprinted. The two presets (`include/SushiDSP/circuits/stages/PowerTubePresets.hpp`) therefore
stand on the tubes' datasheet operating points and on Mesa's own power figures, and the table
says which field is the amplifier's and which is the datasheet's:

| Field | Inner pair, 6L6GC pentode | Outer pair, EL34 triode-strapped | From |
| --- | --- | --- | --- |
| `cathode_bias_volts` | −45 V | −45 V | printed at V6 and V8 (§5.3) |
| load per tube | 1400 Ω | 1400 Ω | the RCA sheet's class AB₁ point, Raa 5600 Ω over four; the IIC+'s own primary is not established |
| `plate_swing.headroom_to_supply_volts` | 81.2 V | 132.2876 V | inner: the row's 58 mA idle × 1400 Ω (the IIC+'s idle is not printed); outer: half its swing, a class A stage's cut-off side equal to its conduction side (§5.3, "Class A") |
| `plate_swing.headroom_to_bottom_volts` | 311.2283 V | 132.2876 V | inner: √(2·55·5600)/2 = 392.4283 V at the row's 55 W less the idle drop; outer: √(2·25·5600)/2 = 264.5751 V at Mesa's 25 W (S2 p.28) on the same primary, split evenly |
| `gain` | 7.388139 | 6.635772 | inner: 6.0 mA/V × (58/72)^⅓ into 1400 ∥ 24.18 kΩ; outer: §7.3's 11 mA/V and 1.06 kΩ plate resistance into 1400 Ω |
| `source_resistance_ohms`, `grid_leak_resistance_ohms` | 37.6 kΩ, 220 kΩ | 37.6 kΩ, 220 kΩ | §7.2, §7.3 |
| `grid_conduction` | the corpus's Norman law | the same | `el34_koren.sub` through `PentodeEL34` |

The outer pair's even split rests on Mesa's word "Class A": the idle current it implies, 94.5 mA
into 1400 Ω, is above what a triode-strapped EL34 dissipates at 473 V, so a measured idle current
would move that split. The two pairs' full swings, 392.4 and 264.6 V, are 3.42 dB apart where §7.3
computed 2.99 dB from the same 25 and 50 W with the old ceilings.

**The output transformer, sag and the power-tube output trim are the JCM800's constants.** §5 could
not establish the IIC+'s primary impedance or turns ratio (§8 question 3), and §6 has no sag or
rolloff figure at all. `kPowerTubeToTransformerAttenuation` maps the two pairs' full swings,
weighted by `kSimulClassPairWeight`, onto the 1.7453 the transformer stage received at full clip
before DA5 part 2, so the transformer and the sag stage see the range they were calibrated for.

**Grid leaks other than V1A's and V2A's are 1 MΩ.** The sheet draws a grid leak at the input
jack and the 680 k shunt at V2A's grid (§3.6); V2B's 47 k (R103) is in the loop network's
Thevenin but the stage's solver still carries the 1 MΩ.

**Master 1 is applied at node F, inside the oversampled region.** It multiplies the sample between
V2B's coupling capacitor and the equaliser, ahead of the phase inverter, so turning it down takes
drive out of the power section as the amp's does. `kMasterUnityGain` is the pot fraction 0 dB
stands for: 0.0038289 puts a −12 dBFS 82 Hz sine at default knobs out at −6.000 dBFS, the point
all four amplifiers pin at (DA5 spec §6), and at that fraction the inverter's grid sits at 0.12 V
and the power section runs clean. While the control scaled the output, after the power section
(until 2026-09-08), the constant was 0.2349335; it was 0.2402319 before the transformer's
secondary stopped carrying a mean (DA5 part 3), 0.3343019 before the power stage became volt-true,
and 0.6907759 when the pin sat at 1 kHz.

## What is not modelled

- **The EQ's active stage.** Dossier §4.2 describes four PNP transistors on a −42 V rail whose
  interconnection §8 question 11 could not reconstruct. The five traps are five `Biquad` peaks, so
  the bank has no gain structure, no input or output impedance and no in-circuit Q. §4.2 says the
  table gives the trap and not the curve; the sliders' log taper is not modelled either.
- **The two EQ inductors §8 questions 1 and 2 leave open.** The 750 Hz band's 0.12 H puts it at
  979.5 Hz and the 80 Hz band's 1.5 H puts it at 71.5 Hz. If either reading resolves the other way
  those centres move to 723 Hz and 87.6 Hz, and the 750 Hz slider is the one Mesa's manual calls
  the most powerful of the five.
- **The PULL switches.** PULL BRIGHT's second 250 pF treble cap, its V4A cathode resistor
  (illegible, §8 question 9), and PULL DEEP's 15 µF across V2B's cathode are all fixed at the
  out position.
- **The 10 M treble-shift resistor and the optional 100 k between the treble wiper and Volume 1.**
  The 10 M is 4 % of the 250 k pot it sits across; §4.1 says the 100 k defaults to absent.
- **The Rhythm channel, the reverb with its take-off at W and return at P, the effects loop's
  jacks as a real insert point, and the Class A and 60 W power switches.**
- **DC at the extreme setting.** With every knob at its stop the settled output carried a DC term
  of −0.0506 (−25.9 dBFS) while `OutputTransformerStage` ran its tanh after its own low-frequency
  rolloff and the rectification the tanh introduced was never blocked. Since DA5 part 3 the
  stage's secondary carries no mean, as a real secondary winding does, and the same setting reads
  −0.00009 (−80.7 dBFS, 2026-09-07); `MesaMark2cIsDcStableAcrossManyBlocksAtHighGain` bounds it at
  the −40 dBFS an ideal transformer gives and checks that it has stopped moving.
