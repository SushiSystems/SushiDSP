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
unity-gain trim, the cabinet and the clamp run after the decimator.

| # | Stage | Bricks | Dossier |
| --- | --- | --- | --- |
| 1 | R22 68 kΩ grid stopper against C20 39 pF | `Biquad` one-pole low-pass, **60.01 kHz** | §2 |
| 2 | V1A, R2 220 kΩ / R12 1.02 kΩ ∥ C3 1 µF | `TriodeStage` gain 77.4 + cathode shelf | §3 |
| 3 | C6 22 nF into R29 470 kΩ | `Biquad` high-pass, **15.39 Hz** | §2 |
| 4 | R29 470 kΩ ∥ C17 2.2 nF into the pot node's 333 kΩ | `Biquad` first-order shelf, 0.4147 below **153.92 Hz** to unity above **371.17 Hz** | §3 |
| 5 | VR2 1 MΩ PRE GAIN | `pre_gain` × √`pre_gain`, ceiling 1.0 | §2 |
| 6 | V1B, R3 100 kΩ / R17 1.02 kΩ ∥ C4 1 µF | `TriodeStage` gain 65.2 + cathode shelf | §3 |
| 7 | C2 22 nF, R6 470 kΩ / R9 1 MΩ | high-pass **4.92 Hz**, attenuation 0.680 | §2, §3 |
| 8 | V2A cold clipper, R15 39 kΩ unbypassed | `TriodeStage` gain 2.21 | §3 |
| 9 | C1 22 nF; C48 1 nF against R11 330 kΩ | high-pass **17.86 Hz**, low-pass **2603 Hz**, attenuation 0.8147 | §2, §3 |
| 10 | V2B, R4 220 kΩ / R13 1.02 kΩ ∥ C5 1 µF | `TriodeStage` gain 77.4 + cathode shelf | §3 |
| 11 | R32 1 MΩ, C57 22 nF, R101 100 kΩ | high-pass **6.58 Hz**, attenuation 0.0909 | §3 |
| 12 | V5B, R102 2.2 kΩ unbypassed | `TriodeStage` gain 41.4 | §3 |
| 13 | R87 1 MΩ, C56 22 nF, R88 1 MΩ / R91 1 MΩ | high-pass **2.41 Hz**, attenuation 0.3333 | §3 |
| 14 | V5A, R93 2.2 kΩ unbypassed | `TriodeStage` gain 25.3 | §3 |
| 15 | C58 22 nF, R24 33 kΩ, C59 10 nF + R94 47 kΩ, the FMV deck | `Peavey5150ToneStack` | §4 |
| 16 | VR7 1 MΩ POST GAIN into R85 ∥ R81 = 200 kΩ | `post_gain` in dB | §2 |
| 17 | V3B cathode follower, C52 22 nF | scalar 0.98, high-pass **0.145 Hz** | §3 |
| 18 | C11 47 nF into R26 1 MΩ, V3A, R14 3.3 kΩ ∥ C61 4.7 nF | high-pass **3.39 Hz**, `TriodeStage` gain 14.0 + cathode shelf | §3 |
| 19 | C49 22 nF, R48 100 kΩ + R50 1 MΩ | high-pass **6.58 Hz** | §5 |
| 20 | V4A/V4B long-tailed pair, R54/R55 220 kΩ, 2.2 kΩ stoppers | `TriodeStage` gain 29.7, attenuation 0.8617 | §5 |
| 21 | 6L6GC quad, push-pull | two `PentodeStage(kPeavey5150Power6L6GC)`, opposite phase, 0.03 V bias mismatch | §7 |
| 22 | Output transformer, screen and rail sag | `OutputTransformerStage`, `PowerSupplySagStage` | §5 |
| 23 | PRESENCE VR9 10 kΩ with C8 ∥ C62 | `Biquad` high shelf, ±10 dB at 2 kHz | §6 |
| 24 | RESONANCE VR8 1 MΩ with C12 | `ResonanceControl`, +10 dB at 100 Hz | §6 |
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

**The cathode bypass shelves.** §3 gives each bypassed cathode one corner and one shelf depth, not
two corners. The module places the pole at the dossier's corner and derives the zero from the depth
so the shelf is exactly what §3 states: V1A and V2B at 461.45 Hz / 338.64 Hz with a 0.7339 floor
(77.4 falling to 56.8), V1B at 493.76 Hz / 295.35 Hz with a 0.5982 floor (65.2 falling to 39.0).
V3A's C61 4.7 nF is the same construction upside down: the dossier's "near 56 kHz" is the pole, the
fully-bypassed gain of 44.68 against the tabulated 14.0 puts the zero at 17.67 kHz, so the stage is
flat at 14.0 through the audio band, which is what §3 means by calling C61 an oscillation stopper
rather than an audio bypass.

**V1A's and V2B's shared numbers.** §3 prints V1A's bypass corner and shelf depth and not V2B's.
The two run the same 220 kΩ / 1.02 kΩ stage at the same gm, so V2B is given V1A's shelf.

**The cathode follower.** §3 reads V3B as hanging off the post-gain node *in parallel* with V3A,
buffering the effects loop rather than sitting in series ahead of the driver. The module puts it in
series, which costs only its scalar. **Follower compression is not modelled**: V3B is a gain of
0.98 (§3's own figure, from its bootstrap arithmetic) and the C52 high-pass, nothing else. C52's
far plate is confidence **[C]** in the dossier (§2, §4 and open question 15), so where it lands
decides whether the effects loop sees the master volume, which this module does not have.

**The phase inverter's gain.** §5 gives the pair's resistors and no gain figure, so 29.7 is derived
here and is a modelling choice: µ = 100 from §3's Koren 12AX7, ra = 62 kΩ from §7's own
phase-inverter arithmetic, per-plate gain of a long-tailed pair being half the common-cathode
figure, and the two halves' 28.47 and 30.86 averaged the way §7 averages them for
`source_resistance`. Its cathode bias, −2.0 V, has no row in §3's DC table and is a choice inside
the range the other 12AX7 rows occupy.

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

**The resonance corner.** §5 gives the feedback network's components and derives no corner from
them; §6 names only "the cabinet resonant frequency". 100 Hz is a modelling choice.

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

- **`kPostGainUnityGain`** — the linear trim on the finished signal. Pinned by
  `Peavey5150DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput`: an untouched amp turns a
  −12 dBFS 1 kHz sine into a −6 dBFS peak. It multiplies after the last nonlinearity, so it moves
  the level and nothing else — unlike POST GAIN, which sits ahead of the driver, the phase inverter
  and the output tubes and therefore drives the power section.
- **The presence and resonance shelf heights**: §6's +10 dB at 2 kHz and +10 dB at the cabinet
  resonance, asserted by `Peavey5150PresenceAtMaximumLiftsTwoKilohertzByTenDecibels` and
  `Peavey5150ResonanceAtMaximumLiftsTheLowEndByTenDecibels`.

The three targets §6 lists that a lead-channel test could assert are the presence and resonance
heights, the power-amp frequency response, and the 3 V RMS preamp-out level. The first two are
asserted. The third is not: the module has no preamp-out tap, and PREAMP OUT is taken at V3B's
cathode, which this module models as a scalar rather than as a node with an output impedance.

## Not modelled

The rhythm channel, the CRUNCH and BRIGHT switches, the effects loop and its send and return
jacks, the preamp-out jack, the low-gain input's −6 dB pad, the standby switch, the impedance
selector, and the screen node's shared sag.
