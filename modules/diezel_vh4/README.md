# `modules/diezel_vh4` — Diezel VH4, channel 3

> **Channel 3's gain chain can be written as a white-box module. Its tone stack, its power section
> and its supply voltages cannot, and the module as a whole is therefore a documented
> approximation.**

That is the verdict of
[`docs/reference/research/2026-09-06-diezel-vh4-design-dossier.md`](../../docs/reference/research/2026-09-06-diezel-vh4-design-dossier.md),
and this module is built from that dossier and nothing else. Every section number below points
into it. One `IAudioNode`, mono in and mono out, registered as `sushidsp.diezel_vh4` with ten
parameters: Gain, Bass, Middle, Treble, Volume, Deep, Presence, Master, Input Trim and Output.

Diezel has never published a schematic. What the dossier reads is one anonymous reverse-engineering
drawing of the preamp from 2012, checked against Diezel's own tube layout and against two impedance
figures in the factory manuals. Nothing at all exists in public for the power amp, so everything
this module does after the effects-send jack is invented on the JCM800's constants and labelled as
such, here and in the source.

## The chain

Every stage below cites the dossier section its numbers come from. `TriodeStage`, `PentodeStage`,
`FmvToneStack`, `ResonanceControl`, `OutputTransformerStage`, `PowerSupplySagStage` and `Biquad`
are the repo's own bricks; `DiezelVh4` owns the order they run in and nothing else knows it.

| Stage | What runs | Plate swing, V up / V down | Numbers from |
| --- | --- | --- | --- |
| R1 1 M, C1 100–200 pF | not modelled; C1's corner depends on the source impedance | — | 3.1 |
| V1a, Ra 120 k, Rk 2k2 bypassed by 47 µF | `TriodeStage`, gain 56.8, Vgk −2.05 V, grid leak 1 M | 111.7 / 144.167 | 3.1, 6 |
| C13 1 nF, R14 220 k, GAIN 250 k | first-order high-pass whose corner tracks the knob, plus the pot's own division | — | 3.1, 3.2, 6 |
| R21 47 k stopper, R22 3M3 grid leak | Miller low-pass, 26.5 kHz | — | 3.3 |
| V2a, Ra 120 k, Rk 680 ∥ 1 µF | `TriodeStage`, gain 67.2, Vgk −1.02 V; cathode shelf, zero 234 Hz, pole 603 Hz, −8.22 dB | 180.8 / 75.857 | 3.3, 6 |
| C18 4.7 nF into the relay node | first-order high-pass, 63.8 Hz; divider ×0.4996 | — | 3.4, 6 |
| R26 470 k stopper | Miller low-pass, 2.51 kHz | — | 3.4 |
| V3a, Ra 120 k, Rk 680 ∥ 2.2 µF | `TriodeStage`, gain 71.4, Vgk −1.02 V; cathode shelf, zero 106 Hz, pole 274 Hz, −8.22 dB | 180.8 / 75.857 | 3.4, 6 |
| V3b cathode follower, Rk 120 k | scalar ×0.986, then a first-order high-pass at 2.26 Hz | — | 3.5, 6 |
| Tone stack | `DiezelVh4ToneStack`: 560 pF, 22 nF, 22 nF, 39 k slope, pots 1 M / 22 k / 220 k | — | 4 |
| R42 470 k, VOLUME 470 k, R43 680 k into the 48.1 k mix node | knob × 0.5, then ×0.0660 | — | 2, 6 |
| V4a, Ra 120 k, Rk 640 unbypassed | `TriodeStage`, gain 28.7, Vgk −0.98 V | 184.0 / 72.688 | 6 |
| V4b send buffer | scalar ×0.986 | — | 2 |
| Loop return driver, phase inverter | one `TriodeStage` at gain 61.927402, input divided by the tail degeneration 1.888775 | 117.418041 / 89.548144 | 5 — nothing established, `Jcm800.cpp` copied |
| EL34 quad | two `PentodeStage(kDiezelVh4PowerEl34)`, in real volts | 35 / 339.166 V | 5, 7, Philips EL34 sheet |
| Output transformer, supply sag | JCM800 constants | — | 5 — nothing established |
| Deep | `ResonanceControl` at 80 Hz, 10 dB | — | 5, 6 |
| Presence | high shelf at 4 kHz, ±9 dB | — | 5, 6 |
| Cabinet, master, clamp | as the JCM800 | — | — |

V1a through the sag stage runs inside one 8× oversampled region, so every corner in that span is
derived at the oversampled rate. Presence, Deep, the master volume and the
cabinet run at the host rate.

### The gain knob is a filter

C13's 1 nF is the number that voices this channel (§3.1). It works against V1a's plate impedance —
120 k in parallel with rp 54.6 k, so 37.53 k — plus R14's 220 k plus whatever the pot's shunt leg
adds. The knob therefore moves the corner and the level together:

```
corner = 1 / (2*pi * 1e-9 * (37530 + 220000 + knob * 250000))
level  = knob * 250000 / (37530 + 220000 + knob * 250000)
```

At the top of the travel that is 313.6 Hz and ×0.4925; as the knob approaches zero it is 617.9 Hz
and nothing. The dossier's §6 gives 314 Hz and 618 Hz, which is where the plate impedance figure
comes from. Everything below the low E's second harmonic is gone before the first distorting stage,
which is what Diezel means by channel 3 being "noticeably tighter than in channel 2" (S2).

A coupling capacitor rolls off at 6 dB per octave, and at 314 Hz the difference between six and
twelve is most of this amp's low end. `Biquad` has no first-order high-pass, so the three coupling
corners are built from `set_first_order_shelf()` with the zero four decades below the pole: the
slope through the audio band is first order and the response settles at −80 dB at DC.

## Approximations, one by one

### The tone stack's interconnection is a reconstruction

The five component values are legible in the drawing. Their wiring is not. The drawing lands the
bass and mid capacitors on their pots' wiper terminals, which shorts the treble pot's upper section
and leaves the control doing nothing; an amp tech who repairs these read the same sheet and wrote
"You are correct that the bass control is weird. Wire it normally." (S4). This module builds the
standard FMV interconnection the dossier recommends. That is the dossier's blocking question 1, and
a photograph of a VH4 main board settles it in a minute.

The flat-position response of this deck is therefore a prediction, not a target anyone has measured.

### R40 is a fixed 0.0815 dB

`FmvToneStack`'s closed form has no series input resistor, and the dossier states no input impedance
for the deck. R40's 10 k is applied as one frequency-independent factor against the resistive path
the stack presents at midband — R41 39 k plus the bass pot's 1 M plus the mid pot's 22 k, so
1.061 M:

```
1.061e6 / (1.061e6 + 1e4) = 0.990664, or -0.0815 dB
```

The real divider moves with frequency as C27's 560 pF bypasses R41. The error is under a tenth of a
decibel at midband and grows toward the top of the band.

### Follower compression is not modelled

V3b and V4b are a gain scalar of 0.986 and, for V3b, the 2.26 Hz output high-pass of §3.5 and §6.
Neither carries a `TriodeStage`: a cathode follower has no plate resistor, so the
common-cathode model would be the wrong circuit, and what that costs is the follower's own
compression when its grid is driven past the cathode. The dossier gives a gain figure only for V3b;
V4b reuses it, on the grounds that §2 draws it as the same topology on the same 120 k cathode.

### The §3.6 network is not implemented, but its resistor still loads the follower

The drawing puts a 10 µF in series with R31's 470 k on the follower's output. The tech who reads the
same sheet says the position is real but the values are not, and that a real one is nearer 2n2 into
47–100 k — a treble shunt around 1.5 kHz, which is a different circuit. The dossier's ruling is
quoted here in full: **"Do not implement this network from S1."** Its blocking question 3 is what
would settle it.

What that ruling rejects is the branch as a *voicing* element, and the same dossier paragraph says
why there is nothing there to voice: 10 µF against 470 k corners at 0.034 Hz, so across the audio
band the branch is a plain 470 kΩ shunt and "the follower's total AC load becomes 320 kΩ". That
load is kept. It is what puts C20's coupling corner at 2.26 Hz, which is the figure §6 lists:

```
1 / (2*pi * 220e-9 * 320e3) = 2.261 Hz     R30 1 M in parallel with R31 470 k
1 / (2*pi * 220e-9 * 1e6)   = 0.723 Hz     R30 alone, if R31's branch were cut out entirely
```

The module uses 2.26 Hz. Either corner is far below anything a guitar plays, so the choice matters
for the arithmetic being consistent rather than for the sound.

### The send pad is not in the signal path

The 100 k / 4.7 k pad at the send jack (×0.0449, §6) feeds the jack, not the power amp. The loop's
through-path leaves V4b's cathode ahead of it, so the pad is absent here. Its two figures — 4.5 kΩ
of source impedance against Diezel's stated 4.7 kΩ, and 420 Ω at the follower against a stated
~600 Ω — are part of why the dossier trusts the drawing at all.

### The mix bus is hard-coded

Channel 3 reaches V4a's grid through R43's 680 k into the 48.1 kΩ the other three grounded legs
make, which is ×0.0660, or −23.6 dB (§6). That shunt exists only because all four channels do. A
single-channel module has to write the number down or drive the mixer triode 24 dB too hard; the
dossier's §8 question 13 says so in those terms.

### Level staging is gone

The four staging factors this module carried until 2026-09-07 —
`kV1aToV2aLevelStaging`, `kV2aToV3aLevelStaging`, `kFollowerToMixBusLevelStaging` and
`kV4bToPhaseInverterLevelStaging` — were removed by
[the plate-swing spec](../../docs/superpowers/specs/2026-09-07-triode-plate-swing-design.md), which
gave `TriodeStage` the plate ceiling the table above lists and left the dossier's own dividers
(×0.4996, the volume divider, ×0.0660 and the two follower scalars) as the only interstage factors
in the chain. `kPowerTubeToTransformerAttenuation` stays, this chain's one output trim to full
scale, and keeps `Jcm800.cpp`'s name because the name is the citation.
`kPhaseInverterToPowerTubeAttenuation`, the 1/32 it sat beside, was replaced on the same day by the
two constants below.

### The phase inverter's tail

A long-tailed pair's cathode follows its driven grid through the tail, so the grid-to-cathode window
is the input divided by `1 + R_tail / (R_tail + (r_a + Ra) / (µ + 1))`, where `(r_a + Ra) / (µ + 1)`
is the impedance the other triode's cathode puts in parallel with the tail. Modelling the inverter
as a plain common-cathode `TriodeStage` hands it a window it does not have.

The VH4's inverter is not established (§5), so it stands in for the JCM800's pair, and since
2026-09-07 it **copies that module's four constants rather than re-deriving them**:
`Jcm800.cpp`'s `kPhaseInverterCathodeBias` −1.346012 V, `kPhaseInverterPlateSwing`
{ 117.418041, 89.548144 }, `kPhaseInverterGain` 61.927402 — one triode's full load-line gain, not
half of it, because the pair's split is already in the factor — and `kPhaseInverterDegeneration`
**1.888775**, which `DiezelVh4.cpp` divides the inverter's input by. Its tail is the 10.47 kΩ both
cathodes share, `jcm_ltp`'s 470 Ω and 10 kΩ in series to ground, and its grid leaks return to
their junction as the 2203's do, which is what puts the pair at 1.432 mA per half with 117.4 V of
plate headroom where the deck's grounded leaks gave 0.188 mA and 15.4 V (DA5 part 2). A product
module may not include another, so the values travel by copy and cite the JCM800's symbol names;
the derivation lives beside them in `Jcm800.cpp`, from `jcm_ltp`'s 82 kΩ plate, its shared
470 Ω + 10 kΩ tail and the Koren Jacobian at that pair's idle point. Before this the module derived
its own figure, got 4.140398 out of a formula with no cathode shunt in it, and disagreed with the
JCM800 by 6.5 dB on the same stage. Like everything else here, the numbers are a modelling choice
inherited with the stand-in rather than a VH4 figure.

The `kPowerTubeGridScale` of 0.077160 that bridged the volt-true preamp into the bias-referenced
`PentodeStage`'s grid units is gone since DA5 part 2; the inverter's plate reaches the EL34 grids
in volts.

### Where the plate swings come from

Each `PlateSwing` is `{ 367 − Va, Va − Va_bottom }`. `Va` is the dossier's §6 idle plate voltage;
`Va_bottom` is the plate solved at `Vgk = 0` on the stage's 120 kΩ load line against the Koren
12AX7 of `include/SushiDSP/circuits/models/Triode12AX7.hpp`, by bisection, with the cathode held at
its idle voltage. V4a's 640 Ω cathode is unbypassed, so holding its cathode at the idle 0.98 V and
reading `Vgk = 0` as the conduction limit is a simplification: the real cathode rises as the stage
conducts, which would put the bottom a little higher. The phase inverter is a modelling choice like
the rest of that stage — it takes the JCM800 long-tailed pair's swing, computed from
[`jcm_ltp`](../../docs/reference/spice/jcm_ltp/README.md) at B+ 360 V, where the 82 kΩ half
self-biases to `Vk` 3.928014 V and `Va` 344.618092 V, the matched-pair figures this module
copies by value. That leaves only 15.8 V toward the supply, which
`jcm_ltp_tran.csv` corroborates: its differential probe reaches −20.7 V to +22.5 V at a 1 V drive.

### Where the grids sit at the default setting

`triode_stage_count()` and `grid_excursion_peak_volts()` are test hooks that read each stage's
largest `|Vg − Vk|` since the last `reset()`. After 200 blocks of 256 samples at 48 kHz with the
knobs untouched and a −12 dBFS 1 kHz sine in, they read (2026-09-07):

| Stage | Peak grid excursion |
| --- | --- |
| V1a | 0.2485 V |
| V2a | 3.7076 V |
| V3a | 25.3221 V |
| V4a | 1.1443 V |
| Phase inverter | 15.9062 V |

Since DA5 (`docs/superpowers/specs/2026-09-07-grid-knee-antialiasing-design.md` §3) no solver
fence holds a grid: V3a's 25.3 V is 180.8 V of headroom to the supply over its gain of 71.4, the
point at which its plate ceiling has saturated, and the inverter's 15.9 V is its grid pulled
through the 470 kΩ source into the 150 kΩ conducting slope of the Norman law, with its 117.4 V
plate ceiling reached at 61.9 of gain. It read 13.4 V against the deck-biased pair's 15.38 V
ceiling before DA5 part 2.

That is the circuit rather than the model. §6 puts the midband product V1a × pot × V2a × V3a at
2.7 × 10⁵ before the stack's loss and the −23.6 dB mix divider, and says in terms that "every stage
after the first is in grid conduction on any real input". A plate ceiling of 72 to 210 V cannot hold
the grid behind it inside four, so these are recorded here as measurements and no test asserts them;
what the tests assert is the clean end of the knob, `DiezelVh4HasACleanEndAtMinimumGain`.
`DiezelVh4GridProbesReportEveryStageInSignalOrder` pins the hooks themselves — five stages, every
one moving after a render, zero outside the range.

### Silence in, silence out

Each grid carries a small resting offset, `Is·(Rs ∥ Rg)` from the cathode-referenced conduction
diode, and each plate multiplies it: at silence the five grids sit at 10 µV, 119 µV, 1.35 mV,
391 µV and 6.296 mV (default knobs, 2 s of all-zero input, 2026-09-07). None of it reaches the
output. C18 and C20 are in the two places the offset could accumulate — C20, 220 nF at 2.26 Hz, is
the capacitor the drawing puts on V3b's output, and the module applies it immediately after the
follower scalar through `Biquad::set_first_order_high_pass()`, whose real pole stays put at a
corner that far down — and the output transformer's 60 Hz rolloff takes what is left. Two seconds of
digital silence come back at **3.4e-44 peak, −869.5 dBFS**, DC mean −6.1e-48, with the gain knob at
its default, and at 3.5e-44, −869.1 dBFS with the knob at 0.0. Both are denormals.

The one DC-coupled link with no capacitor after it is V4a → V4b → the phase inverter, which is why
the inverter's grid shows the largest resting offset of the five. §5 establishes nothing past the
send jack, so there is no capacitor in the drawing to add there.

### C1 is not modelled

The drawing prints C1 as the range `100-200pF` rather than a value, which is what a tracer does
after seeing both. Either reading sits as a shunt across the input, so its corner is set by
whatever drives the jack, not by R1: a guitar and cable put it above 100 kHz, a low-impedance host
higher still. The node is handed samples and never sees that impedance, so it models no input
rolloff at all.

### Miller poles

The grid capacitances are `Jcm800.cpp`'s: Cgk 1.6 pF and Cgp 1.7 pF from
`docs/reference/spice/models/12ax7_koren.sub`, plus 10 pF of stray wiring. Unlike the JCM800's,
the driving resistance is per grid, because §3.3 and §3.4 draw the two stoppers this channel has and
§3.4 computes V3a's pole from R26 itself. The dossier reads that pole at about 2.9 kHz from 115 pF;
this module's 134.7 pF puts it at 2.51 kHz, the difference being the stray capacitance and Cgk that
the dossier's figure leaves out.

### V1a's source resistance is the JCM800's

`TriodeStage`'s source resistance is what loads the grid-conduction diode, so it sets how hard the
stage clips once the grid goes positive. V2a and V3a get the stoppers the drawing gives them, R21's
47 k and R26's 470 k. §3.1 draws none on V1a, and the drawing offers nothing else to use, so V1a
takes `Jcm800.cpp`'s `kTriodeSourceResistance` of 10 kΩ. On the Norman law the grid current is
1 µA × softplus((Vgk − 0.5) / 0.15), a 150 kΩ slope above the 0.5 V knee, so a 10 kΩ source barely
loads it and the plate ceiling, not the grid, clips V1a. V1a is the stage the gain knob cannot reach,
so it is also the amp's THD floor at any knob position.

## The power section

**No component values are established.** §5 is built from Diezel's prose and its biasing procedure,
and it carries none. What that leaves unpinned, in the dossier's own words:

- **Output transformer primary impedance and turns ratio: not established.**
- **Screen resistors: not established.**
- **Phase-inverter type and values: not established** beyond the tube being a 12AX7 in the V7
  position.

So the phase inverter here is one `TriodeStage` on the JCM800's four long-tailed-pair constants,
and the output transformer and the sag stage carry the JCM800's constants unchanged.
`kPowerTubeToTransformerAttenuation` maps the pair's full swing, `kDiezelVh4PowerEl34`'s two
headrooms summed (374.166 V), onto the 2.2222 the transformer stage received at full clip before
DA5 part 2 (2 × 22.222 model volts × 0.05). All of it is a modelling choice with nothing VH4-specific
behind it.

### The pair's DC and where it goes

At the default and a 0.2 220 Hz input the inverter's plate carries a mean of +5.5 to +13 V on a
±117 V swing, and the pair's difference carries a mean of 2.3 to 4.7 % of its full clip (2026-09-07,
`.superpowers/sdd/amp-gain-staging/da5-part3-report.md`). Both are the circuit's: every stage past
V1a clips asymmetrically, each coupling corner removes the mean the stage before it made, and the
square that reaches the power tubes has two halves of unequal length. A real amplifier's coupling
capacitors and output transformer pass none of it to the speaker. This module has no coupling
capacitor between the inverter and the EL34 grids (§5 establishes none, and adding the 2203's
0.022 µF into 230 kΩ moves the pair's mean from 4.5 to 6.4 % without curing it), so the mean
reaches the grids as a shift of both tubes' operating points; `OutputTransformerStage` removes the
pair's mean with its rolloff, and since DA5 part 3 its secondary carries no mean either, so the
saturation's own rectification of the unequal halves — 0.021 of full scale here, the source of the
0.0152 `DiezelVh4IsDcStableAcrossManyBlocksAtHighGain` read at bass 0 — no longer reaches the
output. What remains at that setting is 0.0010, made where the ±1 output clamp cuts the unequal
halves of a 1.06 peak.

Deep is a first-order low shelf at 80 Hz. The 80 Hz is Diezel's owner's manual (S2); the product
page says 90 Hz, and §8 question 9 records that the two factory documents disagree. The 10 dB of
depth is a modelling choice — no source gives one. The first-order form is also a choice: §5 and §8
question 8 say the real control is an LC section in the feedback loop, whose lift is bounded and
resonant and whose interaction with the cabinet's own resonance a shelf will not reproduce. Presence
is a high shelf at 4 kHz, which is Diezel's figure (S2), at the JCM800's ±9 dB, which is not.

### The power tube is an EL34

`kDiezelVh4PowerEl34` (`include/SushiDSP/circuits/stages/PowerTubePresets.hpp`) is that row of the
Philips EL34 datasheet in full, since DA5 part 2 made `PentodeStage` volt-true: page 3, "Class B,
two tubes in push-pull", the Vba 500 V column — Va 495 V, Vg2 400 V, Vg1 −36 V, Ia 2 × 30 mA at
zero signal, Raa 4 kΩ, Vi 25.8 Vrms, Wo 70 W. The VH4 runs ~500 V on the OT primary (S7) and
Diezel specifies 35 mA per tube (S3), so that row is the closest published operating point to this
amplifier, and the idle current is Diezel's.

| Field | Value | From |
| --- | --- | --- |
| `cathode_bias_volts` | −36 V | the row's Vg1; its 25.8 Vrms drive (36.5 V peak) reaches full output exactly at conduction |
| `plate_swing.headroom_to_supply_volts` | 35 V | Diezel's 35 mA × 1000 Ω (Raa/4), the cut-off tube's travel |
| `plate_swing.headroom_to_bottom_volts` | 339.1657 V | √(2·70·4000)/2 = 374.1657 V at the row's 70 W less the idle drop |
| `gain` | 8.458480 | 12.5 mA/V × (35/100)^⅓ = 8.8091 mA/V into 1000 ∥ 24.1 kΩ, the sheet's quick-reference transconductance and 17 kΩ internal resistance scaled to 35 mA |
| `source_resistance_ohms`, `grid_leak_resistance_ohms` | 10 kΩ, 220 kΩ | §7, unchanged: the JCM800's source for an undocumented inverter, and a leak inside the tube's fixed-bias limit |
| `grid_conduction` | the corpus's Norman law | `el34_koren.sub` through `PentodeEL34` |

The 4 kΩ primary is the datasheet's; the VH4's is not established (§5). The
`{ 10000, 220000, -36, 8, 1e-9, 0.02585, 0.045 }` preset this module carried before had its gain
and ceiling scaled from `kEl34`, which had been fitted to golden files with no operating point
behind it.

The spec's original choice was the KT77, and §7 works that case out in full: −43 V from the Genalex
sheet's Vb 500 V ultra-linear row, with a ±25 % adjustment window putting the real figure somewhere
between −32 V and −54 V. Nothing in any source puts a KT77 in a VH4. Diezel's own documents name no
power-tube type at all; the service manual only rules types out. What evidence exists points to the
EL34 — the model name is "VH4 EL34 Head", the bias selector is labelled "EL34 and 6L6", and the
factory biasing table has rows for 5881, 6L6, EL34 and 6550/KT66/KT88 and none for the KT77. The
KT77 is a drop-in EL34 replacement, so building this module for it would be a voicing decision
rather than an error, and swapping the preset is a one-line change.

Two of the preset's fields are inherited rather than the VH4's: `source_resistance_ohms` (the
VH4's phase inverter is undocumented, so the JCM800's 10 kΩ stands in) and the 4 kΩ primary behind
`gain` and both headrooms (the datasheet row's, because the VH4's is not established). The grid law
is the corpus's Norman law, the same for every tube in it.

## Calibration

`DiezelVh4::kMasterUnityGain` uses the calibration point all four amplifiers share (DA5 spec §6):
with every knob at its default and a −12 dBFS 82 Hz sine in, the output peaks at −6.00 dBFS.
`DiezelVh4DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput` is the test that reads it.

The shipped value is one measurement. With the four staging factors removed on 2026-09-07 the
chain runs a great deal hotter into the power section than it did, so the constant fell from
2.148178 to 0.816610; the phase inverter's tail degeneration and the power tubes' grid scale then
moved it to 0.624524; copying the JCM800's four long-tailed-pair constants moved it again to
0.627263, at which the test read −6.005 dBFS at 1 kHz; DA5's Norman grid law and the move of the
pin to 82 Hz, where this value read −1.04 dBFS, set it to 0.3543949; the volt-true power stage and
the corrected inverter set it to 0.2365060; the transformer whose secondary carries no mean (DA5
part 3), at which that value read −5.83 dBFS, set it to **0.2319680**. The measurement is direct: the
constant multiplies the signal after the last nonlinearity, so it is set, the test run, and the
constant scaled by the decibels the reading misses.

There is no measured target to calibrate anything else against. **No bench measurement of a VH4 has
been published** — no frequency response, no THD figure, no input sensitivity, no capture of any
node. The dossier searched for one and says so. What the tests assert instead are the operating
points, the corner frequencies and the two impedance figures §6 lists, which test this module
against the documented circuit rather than against an amplifier.

`DiezelVh4HasACleanEndAtMinimumGain` is this module's acceptance test since the level staging
came out. With the GAIN knob at 0.01 and a −40 dBFS 220 Hz sine in, THD reads **1.843 %**
(2026-09-07, on the volt-true power stage; 0.0940 % before it, when the power grids' clamp sat
25 dB too early and a −40 dBFS input never reached the pair's class-AB handover), under the spec's
10 %. The knob is read at 0.01 rather than 0.0 because the pot
is a plain divider to ground: at 0.0 it shorts the signal and the node is silent, which leaves THD
without a denominator. `DiezelVh4ThdDoesNotFallAsGainRises` asks only that the reading not fall as the
knob rises, within 0.5 percentage points, because once every stage clips the metric saturates.

`DiezelVh4DeepAtMaximumIsATenDecibelShelfAtEightyHertz` asserts the 80 Hz point alone, **4.813 dB**
against a nominal 5, with the master 20 dB under unity. The sweep runs at −40 dBFS, and since the
volt-true power stage this channel turns that into a full-scale output: at unity the flat response
at 80 Hz reads +39.8 dB over the sweep, the output sits on the ±1 rail, and the shelf's lift of a
clamped signal reads 2.18 dB (2026-09-07). The shelf is linear and sits after the amp, so once
nothing clamps the figure stops depending on the level — 4.813 dB with the master 10, 20 and 40 dB
down alike. The 20 Hz point reads 6.840 dB where the shelf is 10 dB deep: C13's input high-pass,
314 Hz with the gain knob at its top and 618 Hz at its bottom, has thrown the fundamental away
before the first distorting stage, so at 20 Hz the swept-sine estimate is measuring the saturated
chain's own product rather than the shelf.

`DiezelVh4MixBusAttenuationIsApplied` measures at **−120 dBFS**, which is where this chain is still
linear end to end: the same reading is 70.853 dB at −80, 72.965 at −100 and 73.114 at −120. At
−80 dBFS the EL34 grids already carry 4.9 V, past the 4.1 V at which the cut-off tube's 35 V plate
headroom saturates, so the pair is compressing there by 2.3 dB; before DA5 part 2 the reading was
60.862 dB at −80 and flat to −120. Its comment writes the cascade out factor by factor to the EL34
grids, ×49062, each stage's own `Rg / (Rs + Rg)` grid divider on the row with its gain, and leaves
one block measured: the EL34 pair, the transformer trim, the transformer and the sag stage are
worth ×0.09225 together.

## Not modelled

- **Channels 1, 2 and 4.** V1a, V3a, V3b, V4a and V4b are shared silicon and V2a is shared with
  channel 4, so the other channels are a different input network and a different stack in front of
  the same chain (§8, question 14). They are not a new module when they arrive.
- **Per-stage supply decoupling.** The drawing gives one undecoupled +367 V node to all eight
  triodes, which is a tracer's simplification; the tech quoted in §3 reports 385–400 V on real
  boards and calls 350 V unusably compressed. Every operating point in the table above moves with
  that number. The dossier's §3 carries a +400 V column, and the module uses the +367 V one.
- **Power-tube bias mismatch.** The JCM800 models a grid-offset mismatch between its EL34s; the pair
  here is matched, because nothing about this amplifier's output stage is documented enough to
  justify picking an asymmetry.
- **R22's real value.** The drawing says 3M3 and the module uses it. The tech has "only seen 1M or
  220k" in that position, and the resistor sets how V2a recovers from blocking distortion (§8,
  question 5).
- **Pot tapers.** The drawing marks none. Every knob here is linear in position (§8, question 6).
