# `modules/nodes/mesa_mark2c`, Mesa/Boogie Mark IIC+, Lead mode

One `AudioNode`, mono in and mono out, registered as `sushidsp.mesa_mark2c` with fifteen
parameters: Volume 1, Treble, Bass, Middle, Lead Drive, Lead Master, Master 1, Presence, the five
graphic-equaliser sliders, Input Trim and Output. The PULL switches on the pots are not exposed;
every one of them is fixed at the position the dossier reads for Lead mode.

Every component value, every gain and every operating point comes from
the mesa mark iic plus design dossier research note in the version history.
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
input, where the lead chain is on its knee: treble at 1 against 0 raises THD by 3.32 pp.

Sitting on the plate also means the deck is driven through V1A's output impedance and loaded by
Volume 1, and until 2026-09-20 the model gave it neither. It now receives 150 k ∥ 62.5 k = 44.12 k
as its source and Volume 1's 1 M as its load, which costs 4.13 dB at 1 kHz with the controls at
noon and tilts the band by 0.87 dB between 41 Hz and 10 kHz. So the omission was a gain-staging
error rather than a tone error: the four lead stages were driven 4 dB harder than the circuit
drives them. The output calibration did not move with it, because the chain compresses, the
default peak stays inside the −7 to −5 dBFS window `K_MASTER_UNITY_GAIN` is pinned against, but the
distortion's distribution between the stages did, which is what took the THD figure above from
8.50 pp to 3.32 pp.

The same termination is not a fixed figure. `FmvToneStackLoadingCostIsTheDecksOwnAndNotOneFigure`
measures it on both decks: the slope resistor sets how large a perturbation a 44 k source is, 100 k
here against the JCM's 33 k, so the identical termination tilts the JCM's band by 3.89 dB and this
one by 0.87 dB. The other three amplifiers pass no termination and are unchanged.

## The blocks and who owns each

| Block | Class |
| --- | --- |
| The FMV stack of §4.1 | `MesaMark2cToneStack`, wrapping `FmvToneStack` |
| Each of the seven 12AX7 halves | `TriodeStage` |
| The five EQ traps | `GraphicEqualizer` |
| Each Simul-Class output half | `PentodeStage`, from `K_MESA_MARK2C_POWER6_L6_GC` / `K_MESA_MARK2C_POWER_EL34_TRIODE` |
| Transformer, sag | `OutputTransformerStage`, `PowerSupplySagStage` |
| Every coupling cap, cathode shelf, divider shelf and Miller pole | `Biquad` |
| The chain's order and the constants | `SushiDSP::MesaMark2c` |

Cabinet convolution is not part of this node; a rig that wants one places the standalone
`sushidsp.cabinet` node after this one (see [`modules/nodes/cabinet/README.md`](../cabinet/README.md)).

## Stage order and the numbers behind it

Read down; everything from V1A to the sag stage runs inside one 8x oversampled region.

A host block is rendered as a run of 32-frame control periods, and every knob is advanced and
every coefficient that moved is rebuilt at each period's start. Rebuilding per sample would mean
re-solving the tone stack's and the equaliser's matrices at 384 kHz, so a knob's travel is
quantised to 0.67 ms at 48 kHz instead. What this buys is that the output does not depend on
the host's block size, which `MesaMark2cSweepSoundsTheSameWhateverTheHostBlockSize` pins by
rendering the same slider sweep in 256-frame and 32-frame blocks and comparing them.

| Stage | Constant | Plate swing (up / down, V) | Dossier |
| --- | --- | --- | --- |
| V1A, Rp 150 k, Rk 1.5 k, Vk 1.6 V, source 10 k | gain 70.6; §3.1 draws no grid stopper, so `K_V1A_SOURCE_RESISTANCE` borrows the JCM800's 10 k | 160.0 / 120.961 (D 382 V, Va 222 V, bottom 101.039 V) | §3.1 |
| Tone stack on the plate | 250 pF / 0.1 µF / 0.047 µF, 100 k slope, 250 k / 10 k / 250 k pots; driven through V1A's 44.12 k and loaded by Volume 1's 1 M, which costs 4.13 dB at 1 kHz |, | §4.1 |
| Volume 1, 1 M | audio taper, no ceiling (see below) |, | §2 |
| V1B, Rp 100 k, Vk 1.9 V | gain 61.5 | 127.0 / 128.851 (D 382 V, Va 255 V, bottom 126.149 V) | §3.2 |
| V1B coupling, 0.1 µF into 100 k | high-pass **15.92 Hz** (computed) |, | §3.2 |
| Lead Drive bright cap, 180 pF | high shelf at **1881 Hz** (computed from 470 k) |, | §3.3 |
| 680 k into Lead Drive 1 M | attenuation 0.595 at maximum |, | §3.3 |
| V3B, Rp 82 k, Vk 1.8 V | gain 56.7 | 98.0 / 151.554 (C 392 V, Va 294 V, bottom 142.446 V) | §3.3 |
| V3B coupling, 0.02 µF into 170 k + 68 k | high-pass **33.44 Hz** (computed) |, | §3.4 |
| Interstage network, 68 k with 0.047 µF across the 170 k | low shelf: 0.286 below, unity above, zero **19.939 Hz**, pole 69.717 Hz |, | §3.4, open question 18 |
| V4A, Rp 270 k, Rk 3.3 k, Vk 1.2 V | gain 70 | 98.0 / 221.216 (C 392 V, Va 294 V, bottom 72.784 V) | §3.4 |
| V4A cathode, 0.22 µF on 3.3 k | zero 219.22 Hz, pole **438.97 Hz**, loss below **0.4994** (−6.03 dB) |, | §3.4 |
| V4A coupling, 0.047 µF into 320 k | high-pass **10.58 Hz** (computed) |, | §3.5 |
| Lead output divider, 220 k ∥ 250 pF over 3.3 k ∥ 500 pF | 0.014778 at DC (−36.6 dB), zero **2893.7 Hz**, pole **65269.6 Hz**, 0.3333 above; the shunt is S8's traced R30, not S1's `100K` glyph (§8 question 21) |, | §3.5 |
| V2A, Rp 100 k, Rk 1.5 k unbypassed, Vk 1.935 V, grid 680 k ∥ 47 pF, source 220 k ∥ 3.3 k = 3.25 k | gain **22.005** = 91.561 × 58.746 / (58.746 + 46.848 + 92.561 × 1.5), the Koren model's local µ and r_a at 1.2902 mA into the loop network's 58.746 k AC load | 75.795 / 114.171 (C 392 V, Va 263.0 V, on the 58.746 k AC load line) | §3.6 |
| Loop network: 47 k (R105) and 0.047 µF (C9) to W; W to ground through the 250 k Lead Master rheostat; W ─ 150 k (R102) ─ P; P ─ 4.7 k (R101) to ground; P ─ 2.2 k (R12) ─ SEND, normalled to RETURN at V2B's grid with its 47 k (R103) | coupling high-pass **16.33 Hz**; at Lead Master maximum 0.6699 × 0.02770 × 0.9533 = **0.01769** (−35.0 dB) from the capacitor's node to V2B's grid; `loop_network_gain()` carries the rheostat's law, Z_W / (47 k + Z_W) with Z_W = pot ∥ 154.27 k |, | §3.6a |
| V2B, Rp 170 k, Rk 1 k, source 5.909 k | gain **73.118** (computed: 100 × 170 / 232.5) | 204.0 / 92.338 (C 392 V, Va 188 V, bottom 95.662 V) | §3.7 |
| V2B cathode, 0.47 µF on 1 k, PULL DEEP out | zero 338.63 Hz, pole **485.73 Hz**, loss below **0.6972** (−3.13 dB) |, | §3.7 |
| V2B coupling, 0.047 µF into 100 k | high-pass **33.86 Hz** (computed) |, | §3.7 |
| Master 1, 100 k, at node F | dB control, `K_MASTER_UNITY_GAIN` 0.0038289 at 0 dB |, | §2 |
| Graphic EQ | 87.61 / 371.74 / 723.43 / 1575.87 / 4822.88 Hz at Q 1.171 / 1.938 / 2.128 / 0.673 / 1.0, ±17 dB |, | §4.2, on S8's inductors |
| V5A/V5B long-tailed pair, Rp 82.5 k and 90.9 k | `LongTailedPair`: gains **68.214959** and **70.108125** on each plate, shared cathode following **0.49678** of the two grids through the 27.27 k tail | 106.646 / 144.645 and 117.503 / 159.365, scaled per load from (C 392 V, Va 286 V, bottom 142.232 V) | §5.1, plates and tail off S8 |
| Simul-Class output | `K_MESA_MARK2C_POWER6_L6_GC` ×2 and `K_MESA_MARK2C_POWER_EL34_TRIODE` ×2, in real volts | inner 81.2 / 311.228 V, outer 132.288 / 132.288 V | §5.3, §7.2, §7.3, RCA 6L6-GC sheet |
| Presence, a 250 k rheostat in the feedback leg | `PowerAmpFeedback`, the leg solved on DK; opening the pot removes **14.15 dB** of feedback at 80 Hz against **34.38 dB** at 10 kHz |, | §5.2 as S8 corrects it |

The six Miller poles land at 9.0 kHz (V1B), 14.5 kHz (V3B), 14.7 kHz (V4A), 11.6 kHz (V2A),
11.6 kHz (V2B) and 15.2 kHz (the inverter). Each is computed from the 12AX7 Koren model's capacitances
and the plate load that drives the grid. V2B's is the one that does not describe the circuit:
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
Design: the iicplus loop network design spec in the version history.

Four stand-ins remain in that network, each labelled in the design's section 5. The Lead
Master's taper: the pot is a rheostat and no source states its law, so it takes the model's
`audio_taper_pot` (n·√n) like the other pots, and its top half moves the level by 1.9 dB, its bottom
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

**The interstage drive normalisation, removed.** The model does not divide each triode's output
by that stage's own dossier gain and multiply by one measured figure. That normalisation existed
because a cascade fed dossier §3.8's numbers ran into `TriodeStage`'s solver bound instead of into
a plate. `TriodeStage` now takes a `PlateSwing` and bounds each plate by its own headroom, so the
spec at the triode plate swing design spec in the version history deleted
`kGridDriveNormalisation` and the seven `k*GridDrive` constants along with the JCM800's and the
VH4's equivalents. The dossier's dividers and pots are now the only things that change the drive
as it moves down the cascade. `K_PHASE_INVERTER_TO_POWER_TUBE_ATTENUATION` went with them, replaced by
the two per-pair grid scales below, and `K_POWER_TUBE_TO_TRANSFORMER_ATTENUATION` 0.05 is the output
trim to full scale.

**Volume 1's ceiling.** The taper law is the corpus's `audio_taper_pot()`, `n·√n`, but without
the 2203's 1/16 ceiling. The 2203's ceiling stands in for a loss the IIC+ already has: its stack
sits ahead of the pot, and at every knob centred it measures −11.3 dB at 100 Hz, −14.9 dB at
1 kHz and −7.9 dB at 3 kHz.

**The 0.047 µF sits across the 170 k, not from the node to ground.** S1 draws 68 k in parallel
with 0.047 µF to ground, which as drawn is a 69.7 Hz low-pass no lead channel could have: with the
0.02 µF coupling cap in front it is a band-pass peaking at 0.171 near 48 Hz and sitting 29 dB down
at 2 kHz. The model places the capacitor across the 170 k, which gives the tightness §3.4's prose
describes, a low shelf, 0.286 below the corner and unity above it, with the pole at exactly the
69.717 Hz §3.4 prints and the zero at 19.939 Hz. It is the only reading under which the prose, the
0.286 and the 69.7 Hz are all true of one circuit, and it replaces both the flat divider and the
separate high-pass, so the low end is not rolled twice. Dossier §8 open question 18.

**The Lead Drive bright cap is read at 470 k.** §8 question 8 leaves its series resistor as either
47 k or 470 k. 47 k puts the corner at 18.81 kHz, where the cap does nothing audible; 470 k puts
it at 1881 Hz, where it works as a bright cap. The model takes 470 k. The shelf's height is
whatever the taper is throwing away at the current pot position, capped at 18 dB, because the cap
shorts the pot's top section above the corner.

**The phase inverter's cathode bias is −1.5 V**, which no source states: §5.1's printed DC does not
close (§8 question 6), and the µ and r_a the gains come from are the Koren Jacobian's at that
unresolved point. `LongTailedPair` derives each half's gain and the split from those two figures and
the plate loads, so no hand-set degeneration constant remains.

**How deep the feedback loop runs is not established, and the output transformer is why.** The
leg's own response is solved from S8's components, and the forward gain is measured with the loop
open: 38.33 dB at 80 Hz, 39.56 at 600 Hz, 61.59 at 3 kHz, 53.31 at 10 kHz. Depth still cannot be
set from them. At the depth that sweeps presence 18 dB the amplifier oscillates at 11.8 kHz, the
pole of a transformer whose corners are stand-ins and which sits inside the loop. `K_TAP_TO_FEEDBACK_VOLTS`
is 14.0, the deepest that does not ring, and at that depth a 9 dB resonance sits at 10 kHz and
presence runs backwards above it. The calibration waits on the transformer.

**The Simul-Class summing weight is 0.5 each.** Both pairs see the same phase-inverted grid signal
and are summed at the transformer with equal weight. The 75 W / 25 W split lives in the presets'
plate swing, 392.4 V against 264.6 V, 3.42 dB of staggered clipping where §7.3 computed 2.99 dB
from the same power figures, and not in the summing. §7.3 says as much: the audible content of
Simul-Class is the ceiling difference, not a level difference.

**The output section.** §5.3 establishes the tubes, the printed −45 V grid voltage, the 2.2 kΩ
stoppers and the 220 kΩ leak at V8, and leaves the idle current and the primary impedance
unprinted. The two presets (`modules/foundation/circuits/include/SushiDSP/foundation/circuits/stages/power_tube_presets.hpp`) therefore
stand on the tubes' datasheet operating points and on Mesa's own power figures, and the table
says which field is the amplifier's and which is the datasheet's:

| Field | Inner pair, 6L6GC pentode | Outer pair, EL34 triode-strapped | From |
| --- | --- | --- | --- |
| `cathode_bias_volts` | −45 V | −45 V | printed at V6 and V8 (§5.3) |
| load per tube | 1400 Ω | 1400 Ω | the RCA sheet's class AB₁ point, Raa 5600 Ω over four; the IIC+'s own primary is not established |
| `plate_swing.headroom_to_supply_volts` | 81.2 V | 132.2876 V | inner: the row's 58 mA idle × 1400 Ω (the IIC+'s idle is not printed); outer: half its swing, a class A stage's cut-off side equal to its conduction side (§5.3, "Class A") |
| `plate_swing.headroom_to_bottom_volts` | 311.2283 V | 132.2876 V | inner: √(2·55·5600)/2 = 392.4283 V at the row's 55 W less the idle drop; outer: √(2·25·5600)/2 = 264.5751 V at Mesa's 25 W (S2 p.28) on the same primary, split evenly |
| `gain` | 7.388139 | 6.635772 | inner: 6.0 mA/V × (58/72)^⅓ into 1400 ∥ 24.18 kΩ; outer: §7.3's 11 mA/V and 1.06 kΩ plate resistance into 1400 Ω |
| `source_resistance_ohms`, `grid_leak_resistance_ohms` | 26.6424 kΩ, 220 kΩ | 26.6424 kΩ, 220 kΩ | §7.2 defines the field as the driver's output impedance plus the 2.2 k stopper, and puts the driver at 35.4 kΩ. That figure read both inverter plates as 82 k. On S8's 82.5 k and 90.9 k the halves present r_a ∥ R_a = 24.108 kΩ and 24.777 kΩ, whose mean plus the stopper is the value here; the 2.5 % between the halves is 0.03 dB at the grid |
| `grid_conduction` | the corpus's Norman law | the same | `el34_koren.sub` through `PentodeEL34` |

The outer pair's even split rests on Mesa's word "Class A": the idle current it implies, 94.5 mA
into 1400 Ω, is above what a triode-strapped EL34 dissipates at 473 V, so a measured idle current
would move that split. The two pairs' full swings, 392.4 and 264.6 V, are 3.42 dB apart where §7.3
computed 2.99 dB from the same 25 and 50 W with the old ceilings.

**The output transformer, sag and the power-tube output trim are stand-ins.** §5 could
not establish the IIC+'s primary impedance or turns ratio (§8 question 3), and §6 has no sag or
rolloff figure at all. Since 2026-09-21 one of them carries more than tone: the 8 kHz leakage
corner sets the phase of the global feedback loop, and it is what the amplifier oscillated at when
the loop was taken to the depth presence needs. `K_POWER_TUBE_TO_TRANSFORMER_ATTENUATION` maps the two pairs' full swings,
weighted by `K_SIMUL_CLASS_PAIR_WEIGHT`, onto the 1.7453 the transformer stage received at full clip
before DA5 part 2, so the transformer and the sag stage see the range they were calibrated for.

**Grid leaks other than V1A's and V2A's are 1 MΩ.** The sheet draws a grid leak at the input
jack and the 680 k shunt at V2A's grid (§3.6); V2B's 47 k (R103) is in the loop network's
Thevenin but the stage's solver still carries the 1 MΩ.

**Master 1 is applied at node F, inside the oversampled region.** It multiplies the sample between
V2B's coupling capacitor and the equaliser, ahead of the phase inverter, so turning it down takes
drive out of the power section as the amp's does. `K_MASTER_UNITY_GAIN` is the pot fraction 0 dB
stands for: 0.0038289 puts a −12 dBFS 82 Hz sine at default knobs out at −6.000 dBFS, the point
all four amplifiers pin at (DA5 spec §6), and at that fraction the inverter's grid sits at 0.12 V
and the power section runs clean. While the control scaled the output, after the power section
(until 2026-09-08), the constant was 0.2349335; it was 0.2402319 before the transformer's
secondary stopped carrying a mean (DA5 part 3), 0.3343019 before the power stage became volt-true,
and 0.6907759 when the pin sat at 1 kHz.

## The panel switches

Three of the front panel's switches are modelled, and each is one component going in or out.

| Switch | What moves | Measured |
| --- | --- | --- |
| PULL BRIGHT | A second 250 pF beside the tone stack's treble capacitor, and 180 pF across Volume 1's top section. The deck's netlist is rebuilt because a capacitance is a component value, not a pot position; Volume 1's corner moves with its wiper and is taken at the pot's midpoint, 1768 Hz, the way the lead drive's is | 6 kHz rises more than 200 Hz does, so the switch tilts the amplifier rather than lifting it |
| PULL DEEP | 15 µF beside the 0.47 µF on V2B's cathode, taking the shelf's zero from 339 Hz to 10.3 Hz and its pole from 486 Hz to 14.8 Hz. The shelf keeps its 0.6972 depth, because the resistances do not move | +3.13 dB at 80 Hz, the shelf's full depth, and nothing at 3 kHz |
| EQ OUT / IN / AUTO | LDR5 lifts the whole slider bus off ground, which takes the five traps out of the path rather than centring them. Auto is the In position here: this node is the lead chain, and nothing else selects a channel | Out, the 80 Hz slider's two extremes give one answer; In, they differ by more than 6 dB |

The three are appended after the fifteen knobs in the descriptor rather than placed beside the
controls they sit next to, because a saved preset addresses parameters by index.

## What is not modelled

- **The EQ's active stage.** Four PNP transistors on a −42 V rail. S8 draws the stage in full and
  closes §8 question 11, but modelling it is a Newton solve of its own size, so the five traps are
  still five `Biquad` peaks and the bank has no gain structure, no input or output impedance and no
  in-circuit Q. §4.2 says the table gives the trap and not the curve.
- **The sliders' log taper.** The five slider positions map straight onto the peak gain.
- **PULL BRIGHT's V4A cathode resistor.** The resistor is illegible on the sheet (§8 question 9),
  so this third leg of the switch does nothing; the other two work.
- **BASS SHIFT, on V1A's cathode.** §3.1 reads the second bypass capacitor as 0.41 or 0.47 µF and
  does not say which leg the switch lifts. The two readings are different amplifiers: lifting the
  15 µF leaves a 226 Hz corner and takes real bass out, while adding 0.47 µF beside it is
  inaudible. Nothing here is a guess worth making.
- **CLASS A.** §8 question 12 does not settle which pair the switch takes out.
- **The 10 M treble-shift resistor and the optional 100 k between the treble wiper and Volume 1.**
  The 10 M is 4 % of the 250 k pot it sits across; §4.1 says the 100 k defaults to absent.
- **The Rhythm channel, the reverb with its take-off at W and return at P, the effects loop's
  jacks as a real insert point, and the 60 W power switch.**
- **DC at the extreme setting.** With every knob at its stop the settled output carried a DC term
  of −0.0506 (−25.9 dBFS) while `OutputTransformerStage` ran its tanh after its own low-frequency
  rolloff and the rectification the tanh introduced was never blocked. Since DA5 part 3 the
  stage's secondary carries no mean, as a real secondary winding does, and the same setting reads
  −0.00009 (−80.7 dBFS, 2026-09-07); `MesaMark2cIsDcStableAcrossManyBlocksAtHighGain` bounds it at
  the −40 dBFS an ideal transformer gives and checks that it has stopped moving.

## Every constant that is not read off the schematic

`mesa_mark2c.cpp` declares 89 named constants. Most cite a dossier section in their comment and come
from a component value printed on the 1983 factory sheet. The ones below do not, and each is
either a choice this module made or a reading the dossier could not settle. The right-hand column
names what would close it.

No bench measurement of a Mark IIC+ appears to have been published, ever (§8 item 15). This model
is calibrated against arithmetic, and the table is the list of places where the arithmetic started
from something other than a printed value.

| Constant | Value | What it stands in for | What settles it |
|---|---|---|---|
| `K_V1A_SOURCE_RESISTANCE` | 10 000 | S1 draws no grid stopper at V1A. It drives `TriodeStage`'s grid-conduction divider, which is V1A's grid, not the plate impedance the deck sits on | A higher-resolution scan of sheet 1 |
| `K_V1B_SOURCE_RESISTANCE` | 250 000 | Volume 1's wiper at its worst case, fixed where the real value tracks the knob | Solving the wiper position into the source impedance |
| `K_V3B_SOURCE_RESISTANCE` | 250 000 | Lead Drive's wiper, the same way | The same |
| `K_PHASE_INVERTER_SOURCE_RESISTANCE` | 25 000 | Master 1's wiper, the same way | The same |
| `K_TRIODE_GRID_LEAK` | 1 000 000 | No grid leak is drawn at those stages | A higher-resolution scan of sheet 1 |
| `K_LEAD_DRIVE_BRIGHT_MAX_DB` | 18 | The bright shelf's ceiling | §8 item 8: the series resistor reads 47 k or 470 k |
| `K_TAP_TO_FEEDBACK_VOLTS` | 14.0 | The deepest the loop runs without ringing, not a calibration | The Mesa's own output transformer, which sets the loop's phase |
| `K_SIMUL_CLASS_PAIR_WEIGHT` | 0.5 | Both output pairs summing equally at the transformer | The transformer's winding arrangement, which no source states |
| `K_TRANSFORMER_FULL_CLIP_UNITS` | 1.745265 | The level the transformer stage receives, a unit convention the corpus shares | A volt-true drive figure from §7 |
| `K_OUTPUT_TRANSFORMER_ROLLOFF_HZ` | 60 | A stand-in low corner | §8 item 3: the IIC+'s primary impedance is not published |
| `K_OUTPUT_TRANSFORMER_LEAKAGE_HZ` | 8000 | A stand-in high corner, and the pole the feedback loop's phase margin hangs on | The same |
| `K_OUTPUT_TRANSFORMER_SATURATION_DRIVE` | 0.3 | The JCM800's figure | The same |
| `K_SAG_AMOUNT` | 0.3 | The JCM800's figure; no IIC+ supply measurement exists | A bench reading of the B+ under load |
| `K_SAG_RECOVERY_TIME_SECONDS` | 0.05 | The same | The same |
| `K_TRIODE12_AX7_MILLER_CAPACITANCE.stray_wiring_picofarads` | 10 | Point-to-point wiring, a corpus assumption | A capacitance measurement on a chassis |
| `K_PHASE_INVERTER_CATHODE_BIAS` | −1.5 | §5.1's DC numbers do not close | §8 item 6: three voltmeter readings |
| `K_PHASE_INVERTER_GAIN` | 68.214959 | Derived at that unresolved point, not on the 82.5 k load line | The same |
| `K_PHASE_INVERTER_PLATE_ASWING`, `K_PHASE_INVERTER_PLATE_BSWING` | 106.646 / 144.645, 117.503 / 159.365 | Scaled from the 82 k load line rather than re-solved on each plate | The same |
| `K_LEAD_OUTPUT_DIVIDER` | 0.014778 | S8's traced 3.3 k shunt, against S1's 100 k | §8 item 21: an ohmmeter across R30, worth 26.5 dB of V2A drive |
| `K_EQUALIZER_BAND_Q` | 1.171 … 1.0 | Each trap's own `(1/R)·√(L/C)`, standing in for the in-circuit Q | The EQ's active stage, drawn on S8 but not modelled |
| `K_EQUALIZER_MAX_GAIN_DB` | 17 | S6's Mark IV figure, modelled symmetric where the amp gives +17 / −18 dB | A measurement of a IIC+ EQ at its stops |
| `K_V4A_INTERSTAGE_ZERO_HZ` | 19.939 | The 0.047 µF taken as sitting across the 170 k | §8 item 18: a photograph of that corner, worth 10.9 dB |
| `K_V4A_INTERSTAGE_POLE_HZ` | 69.717 | The same | The same |
| `K_V4A_INTERSTAGE_LOW_GAIN` | 0.286 | The same | The same |
| `K_V2B_PLATE_LOAD_OHMS` | 170 000 | S1's value, against S8's 120 k | §8 item 22: an ohmmeter, worth 7.3 of V2B's 73.1 gain |
| `K_LEAD_MASTER_POT_OHMS` | 250 000 | The pot's value is read; its law is not | §8 item 14, and item 23 for the LDR4's floor |
| `K_FILTER_Q` | 0.707 | The shelves' Q, an implementation choice rather than a circuit value | Nothing; it is not a component |
| `K_SMOOTHING_MS` | 20 | Parameter smoothing, likewise | Nothing |
| `K_COEFFICIENT_EPSILON` | 1e−4 | The threshold below which the audio thread skips a rebuild | Nothing |
| `K_MASTER_UNITY_GAIN` | 0.0038289 | The calibration that puts a −12 dBFS 82 Hz sine at −6 dBFS with default knobs | Nothing; it is the pin all four amplifiers share |
| `K_MIN_MASTER_DB` … `K_MAX_OUTPUT_DB` | −60 / +12 dB on Master 1 and Output, ±24 dB on Input Trim | Control ranges, where the amp has a 100 k pot | Nothing; the ranges are the host's, not the amp's |

## Implementation notes

### The 0.047 uF ahead of V4A

S1 draws 68 k in parallel with 0.047 uF to ground ahead of V4A, which as drawn is a 69.7 Hz
low-pass no lead channel could have. The model places the capacitor across the 170 k, which
gives the tightness dossier 3.4's prose describes. This is open question 18.

### Miller corners

The Miller corners land at 9.0, 14.5, 14.7, 11.6, 11.6 and 15.2 kHz, each set by the plate load
that drives the grid and not by one shared figure. V2A's pole uses V4A's 270 k, the load that
drives the grid, while `K_V2A_SOURCE_RESISTANCE` is the divider's 3.25 k Thevenin that sets where
that grid conducts.

### The bright shelves

The Volume 1 bright shelf is flat until PULL BRIGHT switches the capacitor in. Then, as on the
lead drive, the shelf gives back what the taper is throwing away at the knob's position. On the
lead drive the 180 pF shorts the pot's top section above the corner, so the shelf's height is
whatever the taper is throwing away at that position.

### The feedback loop's delay

The negative feedback loop closes on the previous oversampled sample, 2.6 us at 48 kHz, because
an implicit solve through the nonlinear power section would iterate.

### Simul-Class

Simul-Class: both power pairs hang on the same transformer, each taking the plate of its own
side, and they clip apart because their plate swings do (dossier 7.3).

### The output clamp

Nothing upstream of the output is normalised to the DAC's range, so a hot combination of Lead
Drive, Master 1 and Output can still land outside [-1, 1]. The clamp is a safety rail.

### Parameter order

The order of the parameter table's rows is a contract. A host addresses the parameters by index
and a saved preset reads them back by index, so rows are appended, never reordered or removed.
For the same reason the panel switches are appended after the knobs and not placed beside the
knobs they sit next to on the panel.

### Master 1 and EQ OUT

Master 1 sits at node F, between V2B's coupling capacitor and the equaliser, ahead of the phase
inverter (dossier 2). EQ OUT lifts the whole slider bus off ground, which takes the five traps out
of the path; it does not centre them.

### Tone stack values

`mesa_mark2c_tone_stack.cpp` takes its values from dossier section 4.1: 250 pF treble, 0.1 uF bass,
0.047 uF mid, 100 k slope, 250 k bass, 10 k mid, 250 k treble.

## Profiler regions and counters

`MesaMark2c::process` records its regions under the group `mesa_mark2c` and its solves and clamp as per-control-period
counters; the names, the cost of a zone and why no stage inside the oversampled loop carries one are in
[`modules/foundation/profile/README.md`](../../foundation/profile/README.md).

## Designs

<!-- designs:begin -->
- [SUSHIDSP](../../../docs/design/SUSHIDSP/README.md): The standalone amp and pedal simulation framework: the rig graph, the DSP math library, the amplifiers and the studio, timeline and room paths.
<!-- designs:end -->
