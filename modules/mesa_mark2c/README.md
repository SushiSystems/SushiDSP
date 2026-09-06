# `modules/mesa_mark2c` — Mesa/Boogie Mark IIC+, Lead mode

One `IAudioNode`, mono in and mono out, registered as `sushidsp.mesa_mark2c` with fifteen
parameters: Volume 1, Treble, Bass, Middle, Lead Drive, Lead Master, Master 1, Presence, the five
graphic-equaliser sliders, Input Trim and Output. The PULL switches on the pots are not exposed;
every one of them is fixed at the position the dossier reads for Lead mode.

Every component value, every gain and every operating point comes from
[`docs/reference/research/2026-09-06-mesa-mark-iic-plus-design-dossier.md`](../../docs/reference/research/2026-09-06-mesa-mark-iic-plus-design-dossier.md).
A number that is not in that file is marked below as a modelling choice. Nobody has published a
bench measurement of a Mark IIC+, so the whole module is calibrated against arithmetic on a 1983
drawing.

## Which channel

Lead mode, the one the amp is bought for: V1A and V1B feed V3B, V4A and V2A through the two lead
pots, and V2B returns from the effects loop into Master 1. The Rhythm path taps off V1B's plate
and is not modelled. Reverb (V3A, V4B) is not modelled either.

## The tone stack sits on V1A's plate

Dossier §2: the FMV stack hangs directly on the first triode's plate, so the four lead gain
stages downstream of it distort a signal the tone controls have already shaped, which is what
separates a Mark from a Marshall. `MesaMark2cToneStack` is therefore instantiated between `v1a_`
and the Volume 1 attenuation in `MesaMark2c::process()`, and
`MesaMark2cToneStackSitsBeforeTheLeadStages` pins the consequence: with Lead Drive wide open, THD
at 1 kHz runs 31.5 % with Treble at 0 and 39.7 % with Treble at 1.

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
| V1A, Rp 150 k, Rk 1.5 k, Vk 1.6 V | gain 70.6 | 160.0 / 120.961 (D 382 V, Va 222 V, bottom 101.039 V) | §3.1 |
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
| Lead output divider, 220 k ∥ 250 pF over 100 k ∥ 500 pF | 0.3125 at DC, zero **2893.7 Hz**, pole **3086.6 Hz**, 0.3333 above (+0.58 dB) | — | §3.5 |
| Lead Master, 250 k | audio taper | — | §2 |
| V2A, Rp 100 k | gain 61.5 | 100.0 / 163.581 (C 392 V, Va 292 V, bottom 128.419 V) | §3.6 |
| Loop-return coupling, 0.022 µF into V2B's 1 M grid return | high-pass **7.2343 Hz** (computed); the capacitor's value is a modelling choice, dossier open questions 19 and 20 | — | §2, §3.7 |
| V2B, Rp 170 k, Rk 1 k | gain **73.118** (computed: 100 × 170 / 232.5) | 204.0 / 92.338 (C 392 V, Va 188 V, bottom 95.662 V) | §3.7 |
| V2B cathode, 0.47 µF on 1 k, PULL DEEP out | zero 338.63 Hz, pole **485.73 Hz**, loss below **0.6972** (−3.13 dB) | — | §3.7 |
| V2B coupling, 0.047 µF into 100 k | high-pass **33.86 Hz** (computed) | — | §3.7 |
| Graphic EQ | 71.5 / 371.8 / 979.5 / 1576 / 4823 Hz at Q 1.434 / 1.938 / 1.571 / 0.673 / 1.0, ±17 dB | — | §4.2 |
| V5A/V5B long-tailed pair, Rp 82 k | gain **28.374** (see below), tail degeneration **0.0711568** = 1 / (1 + 101 × 15 k / (34.061 k + 82 k)) | 106.0 / 143.768 (C 392 V, Va 286 V, bottom 142.232 V) | §5.1 |
| Simul-Class output | `kMesaMark2cPower6L6GC` ×2 at grid scale **0.0907030**, `kMesaMark2cPowerEl34Triode` ×2 at **0.0619348** | — | §7.2, §7.3 |
| Presence, 0.022 µF with 6.8 k | high shelf at **1063.9 Hz** (computed), ±9 dB | — | §5.2 |
| Master 1, 100 k | dB control, `kMasterUnityGain` 0.7040127 | — | §2 |

The six Miller poles land at 9.0 kHz (V1B), 14.5 kHz (V3B), 14.7 kHz (V4A), 5.0 kHz (V2A),
11.6 kHz (V2B) and 15.2 kHz (the inverter). Each is computed from the JCM800's 12AX7 capacitances
and the plate load that drives the grid, so V2A's 270 k feed is the darkest link in the chain and
the inverter's is the brightest.

## Where the model departs from the dossier

**Every coupling capacitor is a first-order section.** One capacitor into one resistance is one
pole, so the five coupling links use `Biquad::set_first_order_high_pass` rather than the biquad
`set_high_pass`. That is also the numerically safe path at this module's 384 kHz oversampled rate:
a conjugate pole pair placed at a corner this far below the rate lands within a float ulp of the
unit circle, which the 7.2343 Hz loop-return corner does not survive. The corners are unchanged
and still come from the dossier's R and C.

**Known issue: the amp has no clean setting.** Dossier §2 puts EFFECTS SEND / RETURN between
V2A's plate and V2B's grid and §3.7 prints no value for any of it, so the model has nothing on that
link: V2A's plate, held to 87 V by its own ceiling, meets V2B's 4.14 V grid window directly and
drives it twenty-one times past it. Every stage ahead of that link runs clean at a −40 dBFS input,
and the node still limits at every input level, which costs three tests. They are skipped, not
weakened, and each re-asserts its own figure once the network is read off the schematic; dossier
open questions 19 and 20.

| Skipped test | What it measures now | What it will re-assert |
| --- | --- | --- |
| `MesaMark2cHasACleanEndAtMinimumGain` | 41.4 % THD at Lead Drive 0.02 and −40 dBFS | THD below 10 % somewhere on the knob |
| `MesaMark2cToneStackSitsBeforeTheLeadStages` | bright minus dark 0.36 pp at −80 dBFS, 0.61 pp at −100 dBFS | a brighter stack drives the lead stages harder, by more than 5 pp |
| `MesaMark2cEqBandBoostsItsCentre` | the 750 Hz slider lifts 979.5 Hz by 0.052 dB at −80 dBFS, 6.17 dB at −100 dBFS | the bank's full 17 dB lift |

Lowering the test input does not rescue the last two: at −80 dBFS the output still sits on its
ceiling, and by −100 dBFS, where the EQ boost finally starts to show, the level is 60 dB below
anything a guitar produces.

**Three constants that are not the dossier's.** The long-tailed pair degenerates its own input:
only `1 / (1 + (µ + 1) · R_tail / (r_a + R_a))` of what arrives reaches the grid-cathode window, which
is `1 / (1 + 101 × 15000 / (34061 + 82000))` = **0.0711568**, using §5.1's 82 k plate and 15 k tail
glyph and r_a 34.061 k from the Koren model at the point §5.1's plate voltage puts the stage.

`PentodeStage` is not volt-true: its grid saturates at `1 / (drive × gain)` in its own units, not at
the preset's −45 V. Two scales bridge the volt-true preamp to it until `PentodeStage` becomes
volt-true in DA5 — inner 6L6GC `1 / (0.049 × 5.0) / 45` = **0.0907030**, outer EL34 triode
`1 / (0.069 × 5.2) / 45` = **0.0619348**.

**Where each grid sits.** `grid_excursion_peak_volts()` measured at the default setting, a
−12 dBFS 1 kHz sine, 200 blocks of 256 at 48 kHz. 4.1360 V is `TriodeStage`'s cutoff fence,
`160 × Vt`, so the last five stages reach it and stop there.

| Stage | Peak grid excursion |
| --- | --- |
| V1A | 0.2485 V |
| V1B | 0.9684 V |
| V3B | 4.1360 V |
| V4A | 4.1360 V |
| V2A | 4.1360 V |
| V2B | 4.1360 V |
| V5A/V5B phase inverter | 4.1360 V |

The dossier says the same thing in words: §3.8 records that from V3B onward the amp is in hard
grid conduction long before the signal reaches V2A.

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

**The phase inverter's gain is 28.374.** §5.1 gives 82 k plate resistors and nothing else, and its
printed DC does not close (§8 question 6). A common-cathode 12AX7 with an 82 k load gives 56.75;
a long-tailed pair splits that between its two plates, so the model uses half. The cathode bias is
−1.5 V, which no source states.

**Presence depth is ±9 dB.** §5.2 gives the network's corner but no depth in decibels. The 9 dB is
the JCM800's constant.

**The Simul-Class summing weight is 0.5 each.** Both pairs see the same phase-inverted grid signal
and are summed at the transformer with equal weight. The 75 W / 25 W split lives in the presets'
`plate_saturation_drive` — 0.049 against 0.069, which is the 2.99 dB of staggered clipping §7.3
computes — and not in the summing. §7.3 says as much: the audible content of Simul-Class is the
ceiling difference, not a level difference.

**The output transformer, sag and the power-tube output trim are the JCM800's constants.** §5 could
not establish the IIC+'s primary impedance or turns ratio (§8 question 3), and §6 has no sag or
rolloff figure at all. Every gain in §7 already rests on an assumed 850 Ω per-half load; if that
assumption falls, both presets and these constants move together.

**Grid leaks other than V1A's are 1 MΩ.** The sheet draws a grid leak at the input jack and
nowhere else.

**Master 1 is applied after the oversampled region.** In the amp it sits at node F, ahead of the
EQ and the inverter, so turning it down would take drive out of the power section. The model
scales the output instead, as the JCM800 does, and `kMasterUnityGain` calibrates that scaling.

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
- **The Rhythm channel, the reverb, the effects loop as a real send and return, and the Class A
  and 60 W power switches.**
- **DC at the extreme setting.** With every knob at its stop the settled output carries a DC term
  of −0.0506 (−25.9 dBFS). `OutputTransformerStage` runs its tanh after its own low-frequency
  rolloff, so the rectification the tanh introduces is never blocked; a real secondary winding
  blocks it. `MesaMark2cIsDcStableAcrossManyBlocksAtHighGain` bounds it at 0.25 rather than the
  −40 dBFS an ideal transformer would give, and checks that it has stopped moving. The JCM800 has
  the same structure, and the fix belongs in the brick.
