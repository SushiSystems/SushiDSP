# `modules/peavey5150` — Peavey 5150 lead channel

One `IAudioNode`, mono in and mono out, registered as `sushidsp.peavey5150` with nine parameters:
PRE GAIN, LOW, MID, HIGH, POST GAIN, RESONANCE, PRESENCE, plus the input trim and output level
every amplifier module carries.

Every component value comes from
[`docs/reference/research/2026-09-06-peavey-5150-design-dossier.md`](../../docs/reference/research/2026-09-06-peavey-5150-design-dossier.md),
and the code cites the section each one is read from. This file records what the module does and,
more to the point, every place where it does something the dossier does not say outright.

## Which amplifier and which channel

The block-letter 1992 5150, lead channel: the schematic set the dossier's source S1 covers.
The rhythm channel, the CRUNCH and BRIGHT switches and the effects loop are not modelled at all:
they hang off relays K4 and K3, which the lead path does not pass through.

The relay reading is K2A energised for lead, and the module follows the dossier's confidence-**C**
step from there: if K1 shares that coil, the lead channel runs with **R18 47 kΩ and R20 220 kΩ
lifted**. That is dossier open question 14, and it is not settled by the five sheets. What it costs
if the reading is wrong: R18 moves V1B by under 0.1 dB, but R20 moves V2A's output divider from
0.815 to 0.638 — 2.1 dB the model would be carrying the wrong way.

## Stage order

The whole chain from the grid stopper to the sag stage runs inside one 8× oversampled region, so
every filter in it is built at the oversampled rate. The presence shelf, the resonance shelf, the
unity-gain trim, the cabinet and the clamp run after the decimator. Every coupling capacitor is a
first-order high-pass at the corner its own R and C give.

| # | Stage | Bricks | Dossier |
| --- | --- | --- | --- |
| 1 | R22 68 kΩ grid stopper against C20 39 pF | `Biquad` one-pole low-pass, **60.01 kHz** | §2 |
| 2 | V1A, R2 220 kΩ / R12 1.02 kΩ ∥ C3 1 µF | `TriodeStage` gain 77.4 + cathode shelf, `PlateSwing` 194.0 / 72.3 V | §3 |
| 3 | C6 22 nF into R29 470 kΩ | `Biquad` first-order high-pass, **15.39 Hz** | §2 |
| 4 | R29 470 kΩ ∥ C17 2.2 nF into the pot node's 333 kΩ | `Biquad` first-order shelf, 0.4147 below **153.92 Hz** to unity above **371.17 Hz** | §3 |
| 5 | VR2 1 MΩ PRE GAIN | `pre_gain` × √`pre_gain` | §2 |
| 6 | V1B, R3 100 kΩ / R17 1.02 kΩ ∥ C4 1 µF | `TriodeStage` gain 65.2 + cathode shelf, `PlateSwing` 131.5 / 93.7 V | §3 |
| 7 | C2 22 nF, R6 470 kΩ / R9 1 MΩ | first-order high-pass **4.92 Hz**, attenuation 0.680 | §2, §3 |
| 8 | V2A cold clipper, R15 39 kΩ unbypassed | `TriodeStage` gain 2.21, `PlateSwing` 10.3 / 213.0 V | §3 |
| 9 | C1 22 nF; C48 1 nF against R11 330 kΩ | first-order high-pass **17.86 Hz**, low-pass **2603 Hz**, attenuation 0.8147 | §2, §3 |
| 10 | V2B, R4 220 kΩ / R13 1.02 kΩ ∥ C5 1 µF | `TriodeStage` gain 77.4 + cathode shelf, `PlateSwing` 194.0 / 72.3 V | §3 |
| 11 | R32 1 MΩ, C57 22 nF, R101 100 kΩ | first-order high-pass **6.58 Hz**, attenuation 0.0909 | §3 |
| 12 | V5B, R102 2.2 kΩ unbypassed | `TriodeStage` gain 41.4, `PlateSwing` 149.3 / 116.5 V | §3 |
| 13 | R87 1 MΩ, C56 22 nF, R88 1 MΩ / R91 1 MΩ | first-order high-pass **2.41 Hz**, attenuation 0.3333 | §3 |
| 14 | V5A, R93 2.2 kΩ unbypassed | `TriodeStage` gain 25.3, `PlateSwing` 90.7 / 134.0 V | §3 |
| 15 | C58 22 nF (**105.76 Hz**), R24 33 kΩ, C59 10 nF + R94 47 kΩ, the FMV deck | `Peavey5150ToneStack` | §4 |
| 16 | VR7 1 MΩ POST GAIN into R85 ∥ R81 = 200 kΩ | `post_gain` in dB | §2 |
| 17 | V3B cathode follower, C52 22 nF | scalar 0.98, first-order high-pass **0.145 Hz** | §3 |
| 18 | C11 47 nF into R26 1 MΩ, V3A, R14 3.3 kΩ ∥ C61 4.7 nF | first-order high-pass **3.39 Hz**, `TriodeStage` gain 14.0 + cathode shelf, `PlateSwing` 52.0 / 148.1 V | §3 |
| 19 | C49 22 nF, R48 100 kΩ + R50 1 MΩ; the pair's tail degeneration | first-order high-pass **6.58 Hz**, divide by 1.914527 | §5 |
| 20 | V4A/V4B long-tailed pair, R54/R55 220 kΩ, 2.2 kΩ stoppers | `TriodeStage` gain 63.512682, Vgk −1.174528 V, `PlateSwing` 113.7 / 79.6 V, attenuation 0.8617, grid scale 0.0631313 | §5 |
| 21 | 6L6GC quad, push-pull | two `PentodeStage(kPeavey5150Power6L6GC)`, opposite phase, 0.03 V bias mismatch | §7 |
| 22 | Output transformer, screen and rail sag | `OutputTransformerStage`, `PowerSupplySagStage` | §5 |
| 23 | PRESENCE VR9 10 kΩ with C8 ∥ C62 | `Biquad` high shelf, ±10 dB plateau, corner 600 Hz | §6 |
| 24 | RESONANCE VR8 1 MΩ with C12 | `ResonanceControl`, +10 dB plateau, corner 330 Hz | §6 |
| 25 | Cabinet slot, output level, full-scale clamp | `StreamingConvolution`, `std::clamp` | — |

Each grid after V1A also carries a one-pole Miller rolloff, built the way
`modules/jcm800/src/Jcm800.cpp` builds its own: `C_in = Cgk + Cgp·(1 + |A|) + stray` with the
12AX7 model's 1.6 pF and 1.7 pF and 10 pF of stray wiring, against the driving stage's plate load.
The corners land at 5.83 kHz (V1B), 93.3 kHz (V2A), 11.0 kHz (V2B), 8.65 kHz (V5B), 12.8 kHz
(V5A), 42.9 kHz (V3A) and 36.7 kHz (the pair).

## Approximations, and what each one costs

Everything here is a place the model departs from the dossier or supplies something the dossier
does not carry.

**The tone stack's input network.** The FMV closed form contains the deck and nothing else, so the
two legs on V5A's coupling node are modelled around it. R24 33 kΩ becomes a fixed 0.4825 loss,
33 k against V5A's own source impedance of R86 100 k ∥ ra 54.8 k = 35.4 kΩ. That assumes the
deck's input impedance is high next to 33 kΩ. The C59 10 nF + R94 47 kΩ leg becomes a first-order
shelf on the same node: open at DC and 47 kΩ above its corner, so the shunt falls from 33 kΩ to
19.36 kΩ and the divider from 0.4825 to 0.3539, a 2.71 dB treble cut with its zero at
1/(2π·10n·47k) = 338.63 Hz and its pole at 248.37 Hz. C58's own high-pass sits at
1/(2π·22n·(35.4 k + 33 k)) = 105.76 Hz. R83 47 kΩ between the two post-gain pots' ground legs is
not modelled; only one of those pots is in the lead path.

**The R87 divider.** §3 reads R88/R91 as a 0.5 divider and records R87 1 MΩ beside it as a series
element. Folded together the way the same section folds R32 into V2B's divider, the three 1 MΩ
resistors give 0.3333. The module uses 0.3333; the difference from the dossier's printed 0.5 is
3.5 dB, and it is a reconciliation of the dossier against its own method, not a new reading.

**C49's load.** The brief that specified this module reads C49 22 nF against R48 100 kΩ alone,
which would put the corner at 72.3 Hz. R50 1 MΩ from node X is that grid's DC return (§5), so the
capacitor works into the two in series and the corner is 6.58 Hz. A 72 Hz pole at the phase
inverter is a bass cut no 22 nF coupling capacitor of this size performs, so the module uses the
series figure.

**The cathode bypass shelves.** Both corners are computed from R, C and the stage's own
small-signal figures, and neither is §3's printed 462 Hz. That figure is 1/(2π·C·(Rk ∥ 1/gm)), the
plate-follower form; a plate-loaded stage degenerates against (ra + Ra)/(µ + 1) instead, and only
that form reproduces the shelf depth §3 states in the same paragraph.

    zero = 1/(2π·C·Rk)
    pole = 1/(2π·C·(Rk ∥ (ra + Ra)/(µ + 1)))

V1A and V2B share a 220 kΩ / 1.02 kΩ stage at ra 49.4 kΩ and µ 94.8: (49.4 + 220)/95.8 = 2.812 kΩ,
1.02 k ∥ 2.812 k = 748.7 Ω, zero 156.03 Hz, pole 212.60 Hz. The depth that falls out,
156.03/212.60 = 0.7339, is §3's own figure (77.4 falling to 56.8) to four places. V1B is
100 kΩ / 1.02 kΩ at ra 44.4 kΩ and µ 94.1: 1.5184 kΩ, 610.0 Ω, zero 156.03 Hz, pole 260.90 Hz,
depth 0.5980 against §3's 0.5982. V3A's C61 4.7 nF is the same construction: 1.5046 kΩ, 1.0332 kΩ,
zero 10.261 kHz, pole 32.777 kHz, and the lift of 3.194 is the ratio between the stage bypassed
(µRa/(ra+Ra) = 44.68) and as drawn (14.0), which is 3.191. §3's "near 56 kHz" is again the 1/gm
form. That shelf is unity below its zero, so the stage runs §3's 14.0 through the audio band, which
is what §3 means by calling C61 an oscillation stopper rather than an audio bypass.

**V1A's and V2B's shared numbers.** §3 prints V1A's bypass corner and shelf depth and not V2B's.
The two run the same 220 kΩ / 1.02 kΩ stage at the same gm, so V2B is given V1A's shelf.

**The cathode follower.** §3 reads V3B as hanging off the post-gain node *in parallel* with V3A,
buffering the effects loop rather than sitting in series ahead of the driver. The module puts it in
series, which costs only its scalar. **Follower compression is not modelled**: V3B is a gain of
0.98 (§3's own figure, from its bootstrap arithmetic) and the C52 high-pass, nothing else. C52's
far plate is confidence **[C]** in the dossier (§2, §4 and open question 15), so where it lands
decides whether the effects loop sees the master volume, which this module does not have.

**The phase inverter's operating point.** §5 gives the pair's resistors and no gain figure, and
§3's DC table has no row for it, so the point is solved: §5 returns both grid leaks, R50 and R53,
to node X below R52 470 Ω, which fixes Vgk at −2·Ia·470 and closes the Koren model on the tail.
The solve gives Vgk −1.174528 V, and the pair's gain 63.512682, its tail degeneration 1.914527
and its `PlateSwing` all follow from that one point; the arithmetic is under "The pair's
operating point, its gain and its tail degeneration" below. Until DA5 the bias was a chosen
−2.0 V.

**The output transformer and the sag stage.** §5 leaves the primary impedance and turns ratio
**not established** (open question 2), so `OutputTransformerStage` and `PowerSupplySagStage` carry
the JCM800's constants unchanged (60 Hz, 8 kHz, 0.3 drive; 0.3 sag over 50 ms), cited to
`modules/jcm800/src/Jcm800.cpp`. `kPowerTubeToTransformerAttenuation = 0.05` comes from the same
place for the same reason. The **screen sag** §5 argues for, one shared 400 Ω feeding four screens,
is not modelled as a shared node; the generic sag stage stands in for it.

**The power tubes.** `kPeavey5150Power6L6GC` is §7's preset verbatim. Two of its seven fields are
flagged there and stay flagged here: `gain = 6` is the midpoint of a 5.5–6.6 bracket that rests on
**the 2203's 3400 Ω primary impedance, which §7 states is an assumption and not a sourced figure**,
and `cathode_bias_volts = −55` is a single forum-relayed source whose two URLs no longer resolve
(§7, open question 3). The 0.03 V grid bias mismatch is not a dossier value at all; it is the
JCM800's, adopted for the argument its comment carries.

**The two feedback shelves' corners.** §5 gives the feedback network's components and derives no
corner from them; §6 names 2 kHz for the presence and "the cabinet resonant frequency", taken here
as 100 Hz, for the resonance. Both corners are modelling choices, and each sits on the plateau side
of the frequency §6 names — the presence shelf below its 2 kHz, the resonance shelf above its
100 Hz — for a reason that is the shelf's shape rather than the circuit's: an RBJ shelf gives half
its decibel gain at its own corner and the full amount only well past it, so a shelf cornered at
2 kHz is +5 dB at 2 kHz, not the +10 dB §6 publishes. §6 states the boost measured *at* a
frequency, so each corner is placed where the plateau has already arrived. Presence corners at
600 Hz and reads 9.90 dB at 2 kHz; resonance corners at 330 Hz and reads 9.90 dB at 100 Hz.

**Interstage level staging, removed.** The module carried seven scalars, one per link, plus an
eighth that lifted the phase inverter's output by 28.62 into the power tubes. All eight are gone,
deleted by `docs/superpowers/specs/2026-09-07-triode-plate-swing-design.md`. Each `TriodeStage` now
runs the seven-argument constructor with a `PlateSwing` solved from §3's DC table, so the plate
saturates where the real plate does instead of running to ±320 V, and the dossier's dividers reach
the 6L6GC grids at face value in volts. `kPhaseInverterToPowerTubeAttenuation` 0.8617 is the last
factor before the power tubes, and `kPowerTubeToTransformerAttenuation` 0.05 is the output trim to
full scale, the one number left in the chain that is a level choice rather than a component.

**The plate swings.** Each is `B+ − Va` on the cutoff side and `Va − Va_bottom` on the conduction
side. `Va` and the idle current are §3's DC table at the 340 V rail; `Va_bottom` is the plate
voltage at `Vgk = 0` on the same load line, `Va = 340 − Ra·Ip(0, Va − Vk)` solved by bisection with
the Koren 12AX7 of `include/SushiDSP/circuits/models/Triode12AX7.hpp`. Solving the idle point the
same way reproduces §3's three columns to their printed precision, so both sides of every swing
come from one model. `Vk` is held at the idle cathode voltage through the swing; on the five
unbypassed cathodes the real `Vk` rises with plate current, so the true bottom sits a few volts
above the figure below and the conduction side is modelled slightly wider than the stage's. The
pair has no DC row in §5, so its idle point is solved with the grid leaks on node X against the
average of R46 82 kΩ and R58 100 kΩ, the same average `kPhaseInverterGain` is taken over; its Vk
is the 37.910 V the tail carries, so its Va and Va_bottom sit 37.9 V higher than a
cathode-grounded stage's would.

| Stage | Ra | Rk | Ia (mA) | Vk (V) | Va (V) | Va_bottom (V) | `PlateSwing` up / down (V) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| V1A | 220 k | 1.02 k | 0.8818 | 0.899 | 145.998 | 73.699 | 194.002 / 72.300 |
| V1B | 100 k | 1.02 k | 1.3155 | 1.342 | 208.452 | 114.776 | 131.548 / 93.677 |
| V2A | 100 k | 39 k | 0.1025 | 3.997 | 329.750 | 116.728 | 10.250 / 213.022 |
| V2B | 220 k | 1.02 k | 0.8818 | 0.899 | 145.998 | 73.699 | 194.002 / 72.300 |
| V5B | 220 k | 2.2 k | 0.6788 | 1.493 | 190.670 | 74.195 | 149.330 / 116.474 |
| V5A | 100 k | 2.2 k | 0.9074 | 1.996 | 249.258 | 115.257 | 90.742 / 134.001 |
| V3A | 68 k | 3.3 k | 0.7654 | 2.526 | 287.955 | 139.854 | 52.045 / 148.101 |
| Pair | 91 k | 15.17 k tail | 1.2495 | 37.910 (Vgk −1.1745) | 226.296 | 146.699 | 113.704 / 79.597 |

**The pair's operating point, its gain and its tail degeneration.** §5 gives the long-tailed
pair's resistors and no gain row, so all three come from one solved point. Both grid leaks return
to node X, the junction below R52 470 Ω, so with the pair's 2·Ia through the tail the driven
grid sits at Vgk = −2·Ia·470 and the cathode at Vk = 2·Ia·15170. The Koren 12AX7 of
`include/SushiDSP/circuits/models/Triode12AX7.hpp` closes on that against the average of R46
82 kΩ and R58 100 kΩ, 91 kΩ, at B+ 340 V: Ia 1.249497 mA, Vk 37.910 V, Vgk −1.174528 V, plate
226.296 V above ground and 188.386 V above the cathode. The Jacobian there gives `gm` 2.12024 mA/V
and `r_a` 44.655 kΩ, so `µ` = `gm`·`r_a` = 94.679.

`kPhaseInverterGain` is the full single-triode figure `µ·Ra/(r_a + Ra)` =
94.679 × 91 / (44.655 + 91) = **63.512682**, not half of it. Driven single-ended, the shared cathode
follows the grid through the tail, so the driven triode's grid-to-cathode signal is the input
divided by `1 + R_tail / (R_tail + (r_a + R_a)/(µ + 1))`, where `(r_a + R_a)/(µ + 1)` is the
impedance the other triode's cathode presents in parallel with the tail. §5's tail is R52 470 Ω +
R51 10 kΩ + R57 4.7 kΩ = 15.17 kΩ and the shunt is (44.655 k + 91 k)/95.679 = 1417.81 Ω, so
`kPhaseInverterDegeneration` = 1 + 15170/(15170 + 1417.81) = **1.914527**, applied at the
inverter's input as a divide. The tail is the DC path: §5 also returns the feedback loop into node
N through C30, whose shunt the module does not model, so the AC tail sits below 15.17 kΩ. The grid
leak's return to node X also bootstraps R50 against the tail's own signal; the degeneration keeps
the plain form because R48 100 kΩ, not R50, sets the grid's signal.

The module carried a chosen −2.0 V bias until DA5 (Ia 0.9614 mA, gain 56.5186, degeneration
1.90500, swing 87.486 / 131.561 V). Both points solve the pair on the 91 kΩ average while the
driven half, V4B, actually sits on R46's 82 kΩ; the average is a modelling choice, not a dossier
figure.

The two are one split, counted once. The first ruling of 2026-09-07 used
`1 + (µ + 1)·R_tail/(r_a + R_a)` = 10.45784 without the cathode shunt, on top of a halved 29.7
stage gain, which counted the pair's split twice and left the power tubes about 20 dB low; the
module review caught it.

**The power-grid scale bridge.** `PentodeStage` is not volt-true. Its grid saturates at
`1 / (plate_saturation_drive × gain)` = 1/(0.048 × 6) = 3.4722 model volts, where the real 6L6GC
grid needs `|cathode_bias_volts|` = 55 V. `kPowerTubeGridScale` = 3.4722 / 55 = **0.0631313** is
the bridge between the volt-true preamp and those grid units, and it comes out when `PentodeStage`
becomes volt-true in DA5. It sits beside the circuit's own grid-leak divider
`kPhaseInverterToPowerTubeAttenuation` 0.8617, which stays.

**Grid excursions at the default setting.** `triode_stage_count()` and
`grid_excursion_peak_volts()` read each stage's largest |Vg − Vk| since the last `reset()`. These
are measurements, not assertions. A −12 dBFS 1 kHz sine, 200 blocks of 256 at 48 kHz, every knob at
its descriptor default, measured 2026-09-07 on the Norman grid law and the audio-taper PRE GAIN:

| V1A | V1B | V2A | V2B | V5B | V5A | V3A | Pair |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0.2302 V | 0.9390 V | 27.0407 V | 11.3859 V | 7.1229 V | 27.3660 V | 27.9728 V | 17.9026 V |

No solver fence holds a grid since DA5; each stage runs until its plate ceiling saturates, and the
figures from V2A on are the swings over the gains. Six of the eight stages square both half-cycles
at the shipped default, and the tail degeneration does not change that: at the default setting the
pair is driven nine times past its own window. The circuit says the same in two independent places:
§3's gain column multiplies to 77.4 × 65.2 × 2.21 × 77.4 × 41.4 × 25.3 × 14.0 before the pair, and
§6 gives Peavey's own nominal lead input as −80 dBV, a tenth of a millivolt. An amplifier built to
turn 0.1 mV into 130 W has no operating point at which its later stages stay linear.

**The clean end.** Spec §4 asks that a player turning PRE GAIN down reach a setting the amplifier
does not square: below 10 % THD at a −40 dBFS input, and below 5 % at Peavey's nominal −80 dBV.
That is 1e−4 V RMS, so 1.4142e−4 V peak, which against `kFullScaleVolts` 2.0 is an amplitude of
7.071e−5 full scale, −83.01 dBFS. Only the second holds. Measured 2026-09-07 at 220 Hz over 300
blocks of 256:

| PRE GAIN | Input | THD |
| --- | --- | --- |
| 0.02 | −40 dBFS | 21.729 % |
| 0.02 | −83.01 dBFS | 0.499 % |

The readings are taken at 0.02, not at 0.00. `pre_gain_attenuation()` is exactly zero at n = 0,
so the bottom of VR2's travel mutes the amplifier; 0.02 passes 0.00115 of the pot node. Before
DA5, on the Shockley knee and the `n·√n` taper, the two rows read 25.652 % and 1.340 %.

`Peavey5150HasACleanEndAtMinimumGain` asserts the nominal figure alone. A 20 mV guitar sits 46 dB
above the −80 dBV nominal §6 prints for this channel, so the −40 dBFS reading is recorded and
printed rather than gated. At that level the amplifier squares from V5A on. Nothing was added to
move it.

**The THD sweep.** Spec §4 asks that THD not fall as the knob rises, within 1.0 percentage point.
Measured 2026-09-07 at 220 Hz, 0.02 amplitude:

| PRE GAIN | 0.02 | 0.05 | 0.10 | 0.20 | 0.30 | 0.40 | 0.50 | 0.70 | 1.00 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| THD | 25.24 % | 26.81 % | 27.07 % | 27.32 % | 27.20 % | 27.12 % | 27.16 % | 27.93 % | 28.09 % |

Measured on the Norman grid law and the audio taper; the largest fall is 0.20 pp from 0.20 to
0.40, and
`Peavey5150ThdDoesNotFallAsPreGainRises` passes at 1.0 pp. That easing is what the tolerance is
for: past the point where the phase inverter clips, the stages ahead of it square into a wave whose
harmonics fall off faster.

**Measuring the two shelves.** `measure::frequency_response()` sweeps at −40 dBFS, and at the
default setting that squares every stage from V2A on, so a shelf read there reads the amplifier's
compression rather than the shelf: 6.74 dB for presence and 5.75 dB for resonance against §6's
+10 dB. Both shelves are linear sections after the decimator, so
`Peavey5150PresenceAtMaximumLiftsTwoKilohertzByTenDecibels` and
`Peavey5150ResonanceAtMaximumLiftsTheLowEndByTenDecibels` read them with the path ahead of them
small-signal: PRE GAIN 0.02, the input trim at its −24 dB floor, and POST GAIN at −40 dB. The trim
alone is not enough, because the power section is still saturating at its floor; POST GAIN sits
ahead of the driver, the phase inverter and the output tubes, which is where that compression is.
There the shelves measure 9.90 dB and 9.90 dB. The shelf constants did not move, only the level the
sweep is taken at.

**The cold clipper's asymmetry.** `Peavey5150EvenHarmonicsRiseFromLowToFullPreGain` compares the
amplifier's even-harmonic content at PRE GAIN 0.2 and 1.0 and reads 0.00865 against 0.09843, a
factor of 11.4 against a gate of 4. It was written to assert V2A's asymmetry and is now named for
what it measures, because across the whole knob the measure is not monotonic — 0.0095, 0.0385,
0.0086, 0.1338, 0.0984 at 0.05, 0.10, 0.20, 0.50 and 1.00 — because at that stimulus every
stage from V5A on is already on its bound, so the even harmonics leaving the amplifier are the
push-pull pair's as much as V2A's. V2A's own grid does move from 2.81 V to 4.26 V over the same
span, past the 4.00 V at which it conducts, so V2A's asymmetry is real; what an output-side
measure cannot do is attribute it to V2A, which is why the test is named for the two points it
compares rather than for the stage it was written to prove. A probe on V2A's plate would attribute
it.

**The PRE GAIN pot law.** `pre_gain_attenuation()` is an exponential audio taper,
(b^n − 1)/(b − 1) with b = ((1 − m)/m)² = 81 for a midpoint fraction m of 0.10, the datasheet
figure for a 10 % "A" taper: 0 at n = 0, 0.00115 (−58.8 dB) at 0.02, 0.00690 (−43.2 dB) at 0.10,
0.100 (−20.0 dB) at noon and 1 wide open. The `n·√n` law it replaces in DA5 (spec §6, decision 7)
sat at −30.0 dB at 0.10 and −9.0 dB at noon, 8 to 11 dB hotter than an audio pot across the middle
of the knob, so the first half of the travel changed little. The midpoint fraction is the one
constant of the law; `Jcm800.cpp` and `MesaMark2c.cpp` still carry `n·√n`.

**Behaviour at silence.** An all-zero input for 2 s leaves the output in the denormals over the
last half second, peak and DC mean alike: 6.8e−43 at the default setting and 4.7e−41 at PRE GAIN
0.0 on one run, an order either way between runs. Both
are −600 dBFS, so nothing self-sustains and no DC offset reaches the output. The grid probes do
read non-zero after that silence, 7.3 µV at V1A rising through the cascade, which is the startup
transient of the coupling high-passes rather than a standing offset; V1A's 7.3 µV is the Norman
law's rest current at Vgk −0.9 V, 88 pA, through Rs ∥ Rg = 90 kΩ ∥ 1 MΩ, and C6 22 nF into R29
470 kΩ strips it before V1B.

**The Miller rolloffs.** The driving impedance is the previous stage's plate load alone, as
`Jcm800.cpp` takes it. The real Thevenin at every one of these grids is higher; the module's own
`kV*SourceResistance` constants are that figure, and they run from 90 kΩ to 673 kΩ. All seven
errors run the same way and this model is brighter than the amplifier, never darker.

**The rail voltage.** §3's whole DC table is parametric because the power-supply sheet is missing
(open question 1). Every bias and gain figure the module uses is the B+ = 340 V column, the middle
of the three the table prints.

**The input impedance.** §6 publishes 470 kΩ and §2 draws a 1 MΩ grid leak; open question 10 says
one of them is wrong. The module uses the drawn R25 1 MΩ.

## Calibration

Two points are measured rather than derived, and each is pinned by the test that measures it.

- **`kPostGainUnityGain = 0.2400876`** — the linear trim on the finished signal. Re-measured
  2026-09-07 by `Peavey5150DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput`, which is
  also the test that pins it: an untouched amp turns a −12 dBFS 82 Hz sine into a −6.00 dBFS peak,
  the point all four amplifiers pin at (DA5 spec §6). Until DA5 the pin sat at 1 kHz, at
  0.6114945; the resonance shelf lifts everything below 330 Hz, so at 82 Hz that value put the low
  E on the output clamp, 10 dB RMS louder than the JCM800 at the same default. It read 0.7038242
  with the level staging in the chain, 0.6020224 once that came out, 0.6771799 with the first tail
  degeneration and the grid-scale bridge, and 0.6660669 with the coupling capacitors at their own
  corners.

  It is applied after the decimator and both feedback shelves, outside the nonlinearity, where
  `Jcm800.cpp` applies its `kMasterUnityGain` inside the amplifier alongside the master pot. That
  is a deliberate departure from the sibling. This amplifier's POST GAIN is a real mid-chain
  control — VR7 sits between the tone stack and V3A, and it drives the driver, the phase inverter
  and the output tubes — so it is applied where the schematic puts it, and the calibration constant
  is left outside as a pure level trim. Folded together, a recalibration would be a tone change.
- **The presence and resonance shelf heights**: §6's +10 dB at 2 kHz and +10 dB at the cabinet
  resonance, asserted by `Peavey5150PresenceAtMaximumLiftsTwoKilohertzByTenDecibels` and
  `Peavey5150ResonanceAtMaximumLiftsTheLowEndByTenDecibels`, both read on the small-signal path.
  Measured 2026-09-07: presence 3.32 dB flat against 13.22 dB full at 2 kHz, resonance −7.83 dB
  flat against 2.07 dB full at 100 Hz.

The three targets §6 lists that a lead-channel test could assert are the presence and resonance
heights, the power-amp frequency response, and the 3 V RMS preamp-out level. The first two are
asserted. The third is not: the module has no preamp-out tap, and PREAMP OUT is taken at V3B's
cathode, which this module models as a scalar rather than as a node with an output impedance.

## Not modelled

The rhythm channel, the CRUNCH and BRIGHT switches, the effects loop and its send and return
jacks, the preamp-out jack, the low-gain input's −6 dB pad, the standby switch, the impedance
selector, and the screen node's shared sag.
