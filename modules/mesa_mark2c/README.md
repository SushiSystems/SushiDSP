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
at 1 kHz runs 19.8 % with Treble at 0 and 34.7 % with Treble at 1.

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

| Stage | Constant | Dossier |
| --- | --- | --- |
| V1A, Rp 150 k, Rk 1.5 k, Vk 1.6 V | gain 70.6 | §3.1 |
| Tone stack on the plate | 250 pF / 0.1 µF / 0.047 µF, 100 k slope, 250 k / 10 k / 250 k pots | §4.1 |
| Volume 1, 1 M | audio taper, no ceiling (see below) | §2 |
| V1B, Rp 100 k, Vk 1.9 V | gain 61.5 | §3.2 |
| V1B coupling, 0.1 µF into 100 k | high-pass **15.92 Hz** (computed) | §3.2 |
| Lead Drive bright cap, 180 pF | high shelf at **1881 Hz** (computed from 470 k) | §3.3 |
| 680 k into Lead Drive 1 M | attenuation 0.595 at maximum | §3.3 |
| V3B, Rp 82 k, Vk 1.8 V | gain 56.7 | §3.3 |
| V3B coupling, 0.02 µF into 170 k + 68 k | high-pass **33.44 Hz** (computed) | §3.4 |
| Interstage divider | 0.286, high-pass 69.72 Hz | §3.4 |
| V4A, Rp 270 k, Rk 3.3 k, Vk 1.2 V | gain 70 | §3.4 |
| V4A cathode, 0.22 µF on 3.3 k | zero 219.22 Hz, pole **438.97 Hz**, loss below **0.4994** (−6.03 dB) | §3.4 |
| V4A coupling, 0.047 µF into 320 k | high-pass **10.58 Hz** (computed) | §3.5 |
| Lead output divider, 220 k ∥ 250 pF over 100 k ∥ 500 pF | 0.3125 at DC, zero **2893.7 Hz**, pole **3086.6 Hz**, 0.3333 above (+0.58 dB) | §3.5 |
| Lead Master, 250 k | audio taper | §2 |
| V2A, Rp 100 k | gain 61.5 | §3.6 |
| V2B, Rp 170 k, Rk 1 k | gain **73.118** (computed: 100 × 170 / 232.5) | §3.7 |
| V2B cathode, 0.47 µF on 1 k, PULL DEEP out | zero 338.63 Hz, pole **485.73 Hz**, loss below **0.6972** (−3.13 dB) | §3.7 |
| V2B coupling, 0.047 µF into 100 k | high-pass **33.86 Hz** (computed) | §3.7 |
| Graphic EQ | 71.5 / 371.8 / 979.5 / 1576 / 4823 Hz at Q 1.434 / 1.938 / 1.571 / 0.673 / 1.0, ±17 dB | §4.2 |
| V5A/V5B long-tailed pair, Rp 82 k | gain **28.374** (see below) | §5.1 |
| Simul-Class output | `kMesaMark2cPower6L6GC` ×2 and `kMesaMark2cPowerEl34Triode` ×2 | §7.2, §7.3 |
| Presence, 0.022 µF with 6.8 k | high shelf at **1063.9 Hz** (computed), ±9 dB | §5.2 |
| Master 1, 100 k | dB control, `kMasterUnityGain` 1.0041100 | §2 |

The six Miller poles land at 9.0 kHz (V1B), 14.5 kHz (V3B), 14.7 kHz (V4A), 5.0 kHz (V2A),
11.6 kHz (V2B) and 15.2 kHz (the inverter). Each is computed from the JCM800's 12AX7 capacitances
and the plate load that drives the grid, so V2A's 270 k feed is the darkest link in the chain and
the inverter's is the brightest.

## Where the model departs from the dossier

**The interstage drive normalisation.** Dossier §3.8 multiplies the lead chain out to about 1e6
and says plainly that the product is a headroom statement rather than a gain figure. Fed as
written, every stage from V3B on sits permanently against `TriodeStage`'s solver bound and the amp
becomes a square-wave generator with dead knobs. So each triode's output is divided by that
stage's own dossier gain and multiplied by one figure,
`kGridDriveNormalisation = 5.0`. The dossier's dividers and pots are then the only things that
change the drive as it moves down the cascade, which is what §3.4 and §3.5 say those dividers are
for. The 5.0 is measured, not derived: it is the value at which Lead Drive sweeps THD at 1 kHz
from 12.0 % to 27.5 % monotonically over its travel.

**Volume 1's ceiling.** The taper law is the JCM800's `preamp_attenuation()`, `n·√n`, but without
that amp's 1/16 ceiling. The 2203's ceiling stands in for a loss the IIC+ already has: its stack
sits ahead of the pot, and at every knob centred it measures −11.3 dB at 100 Hz, −14.9 dB at
1 kHz and −7.9 dB at 3 kHz.

**The 68 k ∥ 0.047 µF divider is modelled as a high-pass, not a low-pass.** The two readings of
that RC in §3.4 are mutually exclusive. As drawn — 68 k with 0.047 µF shunting the grid to ground,
fed through 170 k — the network is a one-pole *low-pass* at 69.7 Hz with a DC gain of 0.286, which
would remove the entire guitar band. The dossier's own prose calls the same corner the amp's
tightness mechanism and says "the low end is thrown away first", which is a high-pass. The model
takes the prose: attenuation 0.286 with a high-pass at 69.72 Hz. Dossier §8 does not list this as
an open question; it should.

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
  of 0.166 (−15.6 dBFS). `OutputTransformerStage` saturates downstream of its own low-frequency
  rolloff, so the rectification its tanh introduces is not blocked; a real secondary winding blocks
  it. `MesaMark2cIsDcStableAcrossManyBlocksAtHighGain` bounds it at 0.25 rather than the −40 dBFS
  an ideal transformer would give. The JCM800 has the same structure.
