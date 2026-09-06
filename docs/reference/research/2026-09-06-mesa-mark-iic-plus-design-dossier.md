# Mesa/Boogie Mark IIC+, Lead Mode — Design-Ready Dossier

Compiled 2026-09-06 for the SushiDSP high-gain amplifier work. Everything a later task needs to
write the module is here; a value that is not in this file does not exist for the model.

Confidence key, the same one the DD-3 dossier uses. **A** = read directly off the factory
schematic scan or stated in Mesa's own manual. **B** = corroborated by a second source, or
derived by me from an A value with the arithmetic shown. **C** = one uncorroborated source, or
a glyph on the scan I could not resolve with certainty. Values marked `[single source]` rest on
exactly one uncorroborated document.

**Read this before trusting §3 and §5.** The factory schematic is a hand-lettered 1983 drawing,
scanned at 3125x2193 for the preamp sheet and 1100x690 for the power sheet. I extracted both
page images out of the PDF and read them at magnification rather than through OCR. The preamp
sheet is legible almost everywhere. The power sheet is a third of the resolution and several of
its numbers are guesses I have marked as such. Mesa is also on record, through the forum
schematic-tracers cited below, as having shipped drawings with deliberate errors, so a printed
node voltage on this sheet is evidence, not proof.

---

## 1. Sources

| # | Source | What it gives | Trust |
|---|---|---|---|
| S1 | MESA/Boogie Mark IIC+ Guitar Preamplifier, factory schematic, (c) 1983 Mesa Boogie Ltd, 2 sheets. <https://schematicheaven.net/boogieamps/boogie_mkiic_plus.pdf> — accessed 2026-09-06 | Every preamp component value, the graphic EQ, the driver, the power section, the supply rails and the printed DC node voltages | A for the preamp sheet, C for several power-sheet glyphs |
| S2 | MESA/Boogie, *Owner's Manual MARK IIC+*, document 135420-MarkIICplus_241127, 46 pp. <https://mesa-boogie.imgix.net/135420-MarkIICplus_241127.pdf> — accessed 2026-09-06 | Power ratings, the tube-role table, the control complement, the EQ band labels and the EQ switching modes | A |
| S3 | jrb32, "Mark IIC+ Schematic (correct!)", Boogie Board forum thread 74680, started 2018-02-20. <https://boogieforum.com/threads/mark-iic-schematic-correct.74680/> — accessed 2026-09-06 | A traced RP10A/IIC+ schematic and the RP10A-vs-RP11A board difference; the statement that the factory drawing contains deliberate errors | B |
| S4 | chipaudette and Boogiebabies, "Differences in IIC(+) compared to IIC+", Boogie Board thread 40649, 2009-10-25. <https://boogieforum.com/threads/differences-in-iic-compared-to-iic.40649/> — accessed 2026-09-06 | Real-unit component deviations from the factory drawing: 180 pF bright cap, 0.1 uF after V1B, the 100 k between treble and Volume 1 | B |
| S5 | gugabrandao and elvis, "Mesa Boogie Graphic Eq REAL FREQUENCIES!!", Boogie Board thread 69355, 2015-02-27. <https://boogieforum.com/threads/mesa-boogie-graphic-eq-real-frequencies.69355/> — accessed 2026-09-06 | Mark IV/V EQ band R, L and C values and the observation that the pots and source impedance shift the in-circuit Q | C |
| S6 | Fractal Audio Wiki, "Mesa/Boogie graphic equalizer". <https://wiki.fractalaudio.com/gen1/index.php?title=Mesa%2FBoogie_graphic_equalizer> — accessed 2026-09-06 | Mark IV band centres 87.61 / 371.74 / 723.43 / 1575.87 / 4822.88 Hz, about 18 dB cut and 17 dB boost, log slider taper, and one overall Q figure (1.414 ideal, 1.3 actual) | C |
| S7 | bancika, "Mesa Mark IIc+ Pre PtP", DIY Fever. <http://diy-fever.com/amps/mesa-mark-iic-preamp/> — accessed 2026-09-06 | An independent builder's reading of the lead chain, the 15 uF cathode bypass caps and the purpose of the last-stage divider | C |

Not obtained: any Mesa service manual with a voltage chart or a bias procedure, any bench
measurement of a IIC+ (frequency response, THD, output power at clipping), and any statement of
the output-transformer primary impedance. Sources searched for those and found empty are listed
in §8.

---

## 2. Block overview

Signal path in Lead mode, read off S1 sheet 1 and confirmed against the tube-role table in S2
page 39. Stage numbering is Mesa's own.

```
INPUT ─ 1M grid leak ─ V1A 12AX7  (Rp 150K → D, Rk 1500 ∥ 15u + 0.47u, Vk 1.6 V)   1st gain
   │
   ├─ TONE STACK, passive FMV, sitting directly on V1A's plate     <-- ahead of everything
   │    100K slope · TREBLE 250K · BASS 250K · MIDDLE 10K
   │    250 pF treble cap + a second 250 pF switched by LDR1 (PULL BRIGHT)
   │    10M across the treble pot (the IIC+ "treble shift" resistor)
   │    0.1u bass cap, 0.047u mid cap
   │
   ├─ VOLUME 1  1M audio  (180 pF bright cap top-to-wiper via PULL BRIGHT)
   │
   ├─ V1B 12AX7  (Rp 100K → D, Rk 1500 ∥ 15u, Vk 1.9 V)                       2nd gain
   │    plate ─ 0.047u ─ 100K to ground ──┬──► RHYTHM path (V2A direct)
   │                                      └──► LEAD INPUT
   │
   ├─ LDR2 ─ 680K ─ LEAD DRIVE 1M  (180 pF bright cap across the top section)
   │
   ├─ V3B 12AX7  (Rp 82K → C, Rk 1500 ∥ 15u)                                  3rd gain, lead
   │    plate ─ 0.02u ─ 170K ─┬─ V4A grid
   │                          └─ 68K ∥ 0.047u to ground      interstage divider 68/(170+68)
   │
   ├─ V4A 12AX7  (Rp 270K → C with 0.001u across it, deleted on Simul-Class;
   │              Rk 3.3K, Vk 1.2 V, bypass 0.22u in series with a switched
   │              resistor on PULL BRIGHT)                                     4th gain, lead
   │    plate ─ 0.047u ─ (220K ∥ 250 pF) ─ (100K ∥ 500 pF to ground) ─ LDR4 ─ LEAD OUTPUT
   │
   ├─ LEAD MASTER 250K ─ LDR1 ─ V2A 12AX7 (Rp 100K → C)                       5th gain, lead
   │
   ├─ EFFECTS SEND / RETURN, then V2B 12AX7 (Rp 170K → C, Rk 1K,
   │    bypass 0.47u with 15u switched in on PULL DEEP)                       loop return
   │
   ├─ MASTER 1  100K   ──►  node F
   │
   ├─ 5-BAND GRAPHIC EQ, discrete, four PNP transistors on a -42 V rail,
   │    five series R-L-C traps onto 50K slider pots              in/out/auto via LDR5
   │
   ├─ V5A/V5B 12AX7 long-tailed pair (Rp 82K each → C)           driver / phase inverter
   │    PRESENCE 250K in the negative-feedback leg from the 8 ohm tap
   │
   └─ V6 V7  6L6GC pentode, Class AB1, fixed bias, 470R 2W screens ─┐
      V8 V9  6L6GC or EL34 triode-strapped, "Class A"              ─┴─ OT ─ 8 / 4 ohm
```

The tone stack sits between the first and second triodes, at the very front, and every lead
gain stage is downstream of it. That single placement is what separates a Mark from a Marshall:
the Marshall's stack is a passive loss after the last gain stage, so it only shapes the finished
distortion, while the IIC+ stack shapes what the four following stages are given to distort
with. A model that moves it later gets the harmonic structure wrong even with identical values.

---

## 3. Preamp, stage by stage

Supply rails, read off S1 sheet 2, "HIGH VOLTAGE SUPPLY":

| Node | Voltage | Feeds | Dropper into it |
|---|---|---|---|
| A | 480 V | Power-tube screens, after the choke | choke from the standby switch |
| B | 415 V | Output-tube screen network | — |
| C | 392 V | V2, V3B, V4A, V5 | 5.6K 2W from B |
| D | 374 V (sheet 2) / 382 V (sheet 1) | V1A, V1B | 1K 2W from C |

Sheet 2 prints its own tolerance under the driver circuit: **"VOLTAGES MEASURED @ 117 A.C. MAIN —
D.C. VOLTAGES MAY VARY ±20 V"**. Every printed node voltage in this dossier carries that band, and
it decides how much weight the two non-closures below can bear.

**The D node is printed twice and the two printings disagree by 8 V.** Sheet 1 labels it 382 V at
the V1 plate feed; sheet 2's supply table labels it 374 V. The 8 V gap sits well inside Mesa's own
±20 V, so this is not a contradiction to resolve, only a choice to make. The model should carry
382 V, because that is the value printed where the stage actually connects, and the difference
moves V1A's plate by under 4 %.

**The B-to-C-to-D dropper arithmetic does not close as printed.** 415 - 392 = 23 V across 5.6 K
is 4.1 mA, which is right for the five triode halves on C. 392 - 374 = 18 V across 1 K would be
18 mA, which is ten times what two 12AX7 halves draw. An 18 V drop at the 1.8 mA those two
halves actually pull needs 10 K, not 1 K. Either the resistor is 10 K and the leading digit was
lost in the scan, or the printed D voltage is wrong. Flagged, not smoothed over; §8 carries it.

### 3.1 V1A — first gain stage, clean and lead (S2 p.39)

| Item | Value |
|---|---|
| Grid leak | 1 M to ground at the input jack |
| Grid stopper | none drawn |
| Plate resistor | 150 K to D |
| Cathode resistor | 1500 |
| Cathode bypass | 15 uF, plus a second cap read as 0.41 or 0.47 uF, with BASS SHIFT switching one leg |
| Coupling to next | none; the tone stack hangs directly on the plate |
| Printed cathode voltage | 1.6 V |

Cross-check. 1.6 V across 1500 is 1.07 mA. Dropping 1.07 mA through 150 K leaves the plate at
382 - 160 = **222 V**. A 12AX7 at 222 V plate and -1.6 V grid sits comfortably on the 150 K load
line. The stage closes.

Small-signal gain, mu 100 and rp 62.5 K, cathode fully bypassed:
A = 100 x 150 / (150 + 62.5) = **70.6**, or 37.0 dB. Derived (B).

The second cathode cap is the one glyph on the preamp sheet I could not settle: it reads as
`.41` but no such value exists and 0.47 uF is the obvious intent. Treat it as 0.47 uF (C).

### 3.2 V1B — second gain stage, clean and lead

| Item | Value |
|---|---|
| Grid feed | Volume 1 wiper, no series stopper drawn |
| Plate resistor | 100 K to D |
| Cathode resistor | 1500 |
| Cathode bypass | 15 uF |
| Coupling out | 0.047 uF into a 100 K shunt to ground |
| Printed cathode voltage | 1.9 V |

Cross-check. 1.9 / 1500 = 1.27 mA; 382 - 127 = **255 V** at the plate. Closes.

Gain: 100 x 100 / 162.5 = **61.5**, or 35.8 dB. Derived (B).

S4 reports that a factory IIC+ carries **0.1 uF** here, not the 0.047 uF the drawing shows, and
that the shunt is 91 K on some units rather than 100 K. Boogiebabies, who has opened more of
these than anyone else quoted in that thread, states the 0.1 uF flatly: "The coupling cap off
V1B is 0.1u in the actual circuit. So much for an accurate schematic from Mesa." The difference
is a corner at 15.9 Hz instead of 33.9 Hz into 100 K, which decides how much low end the four
following lead stages are asked to distort. Model 0.1 uF and make 0.047 uF the variant.

### 3.3 V3B — third lead gain stage

| Item | Value |
|---|---|
| Grid feed | 680 K in series from the V1B coupling node into the top of LEAD DRIVE 1 M; wiper to the grid |
| Bright cap | 180 pF from the pot's top section to the wiper, in series with a resistor read as 47 K or 470 K (C) |
| Plate resistor | 82 K to C (392 V) |
| Cathode resistor | 1500 |
| Cathode bypass | 15 uF |
| Coupling out | 0.02 uF |
| Printed cathode voltage | an ambiguous glyph, read as either `8v` or `1.8v` |

Cross-check, and it settles the glyph. 8 V across 1500 is 5.3 mA, which through 82 K would drop
437 V from a 392 V rail. That is impossible, so the printed value is not 8 V. Read as 1.8 V:
1.2 mA, plate at 392 - 98 = **294 V**, which sits exactly where a 12AX7 with an 82 K load off a
392 V rail belongs. **Take Vk = 1.8 V.** (B, by elimination.)

Gain: 100 x 82 / 144.5 = **56.7**, or 35.1 dB. Derived (B).

Lead Drive at maximum makes the 680 K / 1 M divider 1M / 1.68M = **0.595**, or -4.5 dB, before
the pot's own taper. The pot is drawn without a taper marking; Mesa's practice on the Mark gain
controls is audio taper, but the drawing does not say so, so the taper is not established.

### 3.4 V4A — fourth lead gain stage

| Item | Value |
|---|---|
| Grid feed | 170 K in series from the V3B coupling cap |
| Divider at the grid | 68 K in parallel with 0.047 uF, to ground |
| Plate resistor | 270 K to C, with 0.001 uF across it, marked "delete on Simul-Class" |
| Cathode resistor | 3.3 K (printed glyph reads `3.1k`) |
| Cathode bypass | 0.22 uF in series with a switched resistor on PULL BRIGHT |
| Coupling out | 0.047 uF |
| Printed cathode voltage | 1.2 V |

Cross-check. 1.2 V across 3.3 K is 0.36 mA; 392 - 0.36 mA x 270 K = **294 V**. Closes cleanly,
and it is the reason to read the glyph as 3.3 K rather than 3.1 K: with 3.1 K the current is
0.387 mA and the plate lands at 288 V, still plausible, but 3.3 K is a stock value and 3.1 K is
not. Take **3.3 K** (B).

Interstage divider: 68 / (170 + 68) = **0.286**, or -10.9 dB, with the 0.047 uF putting the
divider's corner at 1 / (2 pi x (170K ∥ 68K) x 0.047u) = **69.7 Hz** (derived, B). This is the
IIC+'s tightness mechanism. Seventy-one per cent of the signal is thrown away between the third
and fourth lead stages, and the low end is thrown away first.

The series resistor is **170 K, not 270 K**, and the distinction matters because V4A's plate
resistor 300 px away on the same sheet *is* 270 K. Read side by side at 8x, the grid resistor's
leading glyph is a single thin stroke of the same shape that opens V2B's 170 K plate resistor,
while the plate resistor's is a filled blob. A first pass through this dossier had 270 K in both
places and put the divider at 0.201 and the corner at 62.4 Hz, which is 3.0 dB and 7 Hz wrong.

The 0.22 uF partial bypass on a 3.3 K cathode gives a bass-cut shelf rather than a flat lift:
the corner is 1 / (2 pi x 3.3K x 0.22u) = **219 Hz**. Together with the 69.7 Hz divider corner,
this is where the Mark's absence of flab is manufactured. The switched resistor in series with
that cap is the PULL BRIGHT control; its value is the one place on the preamp sheet the ink is
too heavy to read, and it is not established.

Gain, before loading: 100 x 270 / 332.5 = 81.2. Loaded by the output divider that follows
(220 K + 100 K = 320 K to ground through the coupling cap), the effective plate load is
270K ∥ 320K = 146 K and the gain becomes 100 x 146 / 208.5 = **70**, or 36.9 dB (B).

### 3.5 Lead output divider

`V4A plate — 0.047u — (220 K ∥ 250 pF) — node — (100 K ∥ 500 pF to ground) — LDR4 — LEAD OUTPUT`

Attenuation at DC: 100 / 320 = **0.3125**, or -10.1 dB. The 250 pF across the series resistor
and the 500 pF across the shunt make it frequency-dependent; at high frequency the ratio tends
toward 250 / (250 + 500) = 0.333, a mild treble tilt of about +0.6 dB rather than a bright cap
in the usual sense. Derived (B). S7 describes this divider's purpose as bringing the lead signal
down to a level the effects loop can take.

### 3.6 V2A — fifth lead gain stage

Fed from LEAD MASTER 250 K through LDR1, with a 100 K to ground at the switch node. Plate
resistor 100 K to C. The cathode network glyph on the scan reads as either `1.5K` or `1.5M`;
1.5 K is the only value that works, and it matches every other gain stage on the sheet. Taking
1.5 K bypassed and a cathode near 1.5 V: 1 mA, plate at 392 - 100 = **292 V**, gain
100 x 100 / 162.5 = **61.5** (B). The printed cathode voltage for this stage was not legible.

### 3.7 V2B — loop return and Master 1 driver

Plate resistor 170 K to C. Cathode 1 K, bypassed by 0.47 uF with a **15 uF switched in parallel
by the PULL DEEP function on Master 1**. Output through 0.047 uF to the Master 1 100 K pot and
then to node F. A 120 pF is drawn across the plate.

Cross-check. At 1 K and a cathode near 1.2 V the current is 1.2 mA and the plate lands at
392 - 204 = **188 V**. That is a deliberately cold, low-headroom stage, which is what a 170 K
plate load is for.

The two bypass caps are the whole of "PULL DEEP". 0.47 uF on 1 K corners at **339 Hz**; adding
15 uF takes the corner to **10.3 Hz** (derived, B). Deep does not add bass, it stops removing
it. S4 records that the same 15 uF appears on real units as a 22 uF, and that Mike B's
IIC-to-IIC+ conversions use a 1 K cathode here where a stock IIC used 2.2 K.

### 3.8 What the whole lead chain multiplies to

Everything wide open, small-signal, no clipping, derived (B) and stated as a product so each
factor can be checked separately:

```
V1A 70.6  x  tone stack (see §4)  x  Vol 1  x  V1B 61.5  x  0.595 (680K/1M)
    x  V3B 56.7  x  0.286 (170/68)  x  V4A 70  x  0.3125 (220/100)  x  Lead Master  x  V2A 61.5
```

With the tone stack at its flat insertion loss and both master controls at unity the product is
of order 1e6, or 120 dB, which is why every stage from V3B onward is in hard grid conduction long
before the signal reaches V2A. The numbers are only useful as a statement of how much headroom
each stage has, never as a gain figure.

---

## 4. Tone network

### 4.1 The FMV stack, on V1A's plate

Topology as drawn on S1 sheet 1, reading from the plate node:

| Component | Value | Where |
|---|---|---|
| Treble cap | 250 pF | plate to the top of the treble pot |
| Second treble cap | 250 pF | in parallel, switched in by LDR1 / PULL BRIGHT |
| Treble-shift resistor | 10 M | across the treble pot |
| Slope resistor | 100 K | plate to the bass/mid junction |
| Treble pot | 250 K | wiper is the stack output |
| Bass cap | 0.1 uF | junction to the bass pot |
| Bass pot | 250 K | rheostat to the mid node |
| Mid cap | 0.047 uF | junction to the mid pot |
| Mid pot | 10 K | rheostat to ground |
| Output | treble wiper into VOLUME 1, 1 M audio |

Pot tapers are not marked on the drawing. Volume 1 is drawn as 1 M and is audio taper on every
Mark; the three tone pots on a Fender-derived FMV stack are conventionally linear. Neither is
stated by a source, so both are **not established** and go to §8.

The **10 M across the treble pot** is the IIC+ marker. S3's tracer notes it as a feature of the
RP11A board redesign, and it is absent from the earlier IIC. Electrically it puts a permanent
10 M leak across the treble control, which barely moves the response but does stop the stack
floating when the treble is at zero.

Two component values here are contradicted by units in the field. S4 reports **180 pF** as the
Volume 1 bright cap on every C+ its author has opened, against the 100 pF the factory sheet
shows, and reports a stock IIC at 250 pF. Model 180 pF. S4 also reports a **100 K resistor
between the treble wiper and the top of Volume 1** on factory IIC-to-IIC+ conversions, which the
drawing omits; chipaudette's own SPICE run put its effect at about -1 dB, and Boogiebabies
describes it as changing the load the treble branch presents to the bass and mid caps. It is
optional in the model and should default to absent.

The stack's flat-setting insertion loss and its response curves have not been published for
these exact values, and I did not simulate them. **Not established**; §8.

### 4.2 The five-band graphic EQ

**It is neither an active gyrator design nor a passive LC network.** It is a discrete
four-transistor amplifier running on a negative rail, with five entirely passive series R-L-C
traps hung off it onto slider pots. Real iron-cored inductors, hand-wound, exactly as S5's
respondents describe. Any model built on gyrators is modelling a DIY reimplementation, not the
amp.

Active section, read off S1 sheet 1. Four PNP transistors Q1 to Q4 on a rail printed at
-42.2 V, with printed operating points -47.2 V, -41.6 V, -27.7 V and -27.1 V at various
collectors and emitters, and the band-network reference nodes at -21.6 V (green wire) and
-27.6 V (blue wire). Around them: 1 M at the input to ground, 470 K and 3300 in the Q1 feedback
path, 3300 and 3300 at Q1's emitter, 22 K at Q2/Q3, 3300 and 3300 with a 10 pF at the output
node, 10 pF at Q4, and 0.047 uF coupling to the EQ output. The exact interconnection of Q1 to Q4
is legible only in part, and I have not reconstructed it; see §8.

Band networks, all five read off the sheet, each a series R + L + C from the pot's low end to
the shared bus, with the pot as the boost/cut element:

| Panel label | Slider pot | Series R | L | C | f0 derived | Q derived |
|---|---|---|---|---|---|---|
| 80 Hz | 50 K | 470 | 1.5 H | 3.3 uF | **71.5 Hz** | 1.434 |
| 240 Hz | 50 K | 470 | 0.39 H | 0.47 uF | **371.8 Hz** | 1.938 |
| 750 Hz | 50 K | 470 | 0.12 H | 0.22 uF | **979.5 Hz** | 1.571 |
| 2200 Hz | 50 K | 1000 | 0.068 H | 0.15 uF | **1576 Hz** | 0.673 |
| 6600 Hz | 50 K | 1000 | 0.033 H | 0.033 uF | **4823 Hz** | 1.000 |

f0 = 1 / (2 pi sqrt(LC)) and Q = (1/R) sqrt(L/C), both computed by me (B). Every printed panel
label is wrong by between 11 % and 37 %, and the direction of the error is not consistent: the
80 Hz band resonates below its label, the others above or below by turns.

The three bands the IIC+ shares with the Mark IV land on S6's independently derived Mark IV
centres to three digits — 371.8 against 371.74, 1576 against 1575.87, 4823 against 4822.88 —
which is strong confirmation that I read the L and C values correctly. **Two bands differ from
the Mark IV.** S5 gives the Mark IV a 1 H inductor on the 80 Hz band where the IIC+ sheet shows
1.5 H, and 0.22 H on the 750 Hz band where the IIC+ shows 0.12 H. Those two differences move the
IIC+'s low band from 87.6 Hz down to 71.5 Hz and its mid band from 723 Hz up to 980 Hz. Since the
750 Hz slider is the one S2 calls the most sonically powerful of the five, and since 980 Hz is
most of an octave above the label, this is the most consequential number in the whole dossier. It
rests on my reading of one hand-drawn glyph on one scan. `[single source]`, and §8 asks for it to
be checked against a board.

Range and taper: S6 states about 18 dB of cut and 17 dB of boost for the Mark IV, "probably
+/- 15 dB" as the design target, and a logarithmic slider taper.

S6 also gives a **single** Q for the whole equalizer, 1.414 as the octave-equalizer ideal and
1.3 as the figure its author attributes to the components actually used. That 1.3 sits below
three of the five per-band Q values in the table above, and the two numbers are not measuring
the same thing: mine is each trap's own Q from its own R, L and C, S6's is one figure for the
finished response. S5's elvis names the reason they differ — the 50 K sliders and the source and
load impedances shift both the frequency and the Q with slider position, so a trap's resonance is
not the response peak the amplifier produces. **The table gives the trap, not the curve.** The
in-circuit Q per band cannot be computed until the EQ's active stage is traced (§8 item 11), and
no source I found measured a IIC+, so both S5's and S6's figures are `[single source]` here.

Switching: LDR5 lifts the whole slider bus off ground to bypass the EQ. S2 page 19 gives the
three panel positions — EQ OUT (centre, bypassed), EQ IN (down, always), EQ AUTO (up, engaged
whenever Lead mode is selected) — plus a footswitch.

---

## 5. Power section

Everything in this section comes from S1 sheet 2, which is the low-resolution sheet. Read the
confidence marks.

### 5.1 Phase inverter

A 12AX7 long-tailed pair, V5A and V5B, with 82 K plate resistors to the C rail (392 V). The
input arrives at V5A's grid from node F, the EQ output or the Master 1 wiper. V5B's grid is fed
through a 22 K from the feedback node. Printed voltages: V5A plate 286 V, a cathode-region node
at 18 V, and a further node printed 191 V that I could not attach to a pin with confidence.
Between the two cathodes sits a resistor read as 470.

Cross-check, and it fails. 392 - 286 = 106 V across 82 K is 1.29 mA in V5A. For a balanced
long-tailed pair both halves draw about the same, so the tail carries near 2.6 mA, and a cathode
sitting at 18 V would need a tail resistor near 6.9 K. The sheet's tail-resistor glyph reads as
15 K, which at 2.6 mA would put the cathodes at 39 V, not 18 V. **The phase inverter's DC numbers
as printed do not close and I could not resolve which of them is wrong.** Sheet 2's own
"D.C. VOLTAGES MAY VARY ±20 V" does not rescue them: a 21 V gap between the implied 39 V and the
printed 18 V is right at the edge of the band, and the tolerance is a spread across units rather
than a licence for two figures on one drawing to disagree with each other. §8.

### 5.2 Negative feedback and presence

From the OT's 8 ohm tap: `8 ohm — PRESENCE 250 K — 3300 — (6.8 K ∥ 0.022 uF) — 22 K — V5B grid`,
with a further `3300 ∥ caps` and a `1500` to ground on the way. The presence control is a
variable shunt in the feedback leg, the standard Fender/Marshall arrangement, and the 0.022 uF
sets where the feedback starts falling away. The IIC+ has **no resonance or depth control in the
feedback network**; its "PULL DEEP" is the V2B cathode cap of §3.7, in the preamp, not a
power-amp control. Confidence B on the network's function, C on the individual resistor readings.

### 5.3 Output tubes

S2 page 39 gives the sockets: V6, V7, V8 and V9 all 6L6. S1 sheet 2 adds, as far as the scan
allows: "SIMUL-CLASS power design may use MESA types STR-415 6L6GC or SIMUL 6CA7/EL-34 in V8 and
V9 sockets."

S2 page 27 gives the operating arrangement in Mesa's own words: SIMUL-CLASS "brings on-line all
four of the output tubes and the full 75 watts", with "two of the four octal tube sockets wired
in Class AB" and "the other two octal sockets wired Class A". Page 28 adds that the CLASS A
position "harnesses two of the four output tubes (outer pair/far left and far right)", "wired in
Class A Triode", producing "approximately 25 watts, versus the 'standard' more efficient AB-style
operating mode, which might produce roughly 50 watts". So:

- **V6, V7** — inner pair, 6L6GC, pentode, Class AB1, fixed bias.
- **V8, V9** — outer pair, 6L6GC **or triode-strapped EL34**, "Class A".

Values read off the sheet, with confidence:

| Item | Value | Trust |
|---|---|---|
| Plate supply | 473 V printed at the OT centre tap | B |
| Screen supply | 480 V at node A, after the choke | B |
| Screen resistors | 470 ohm 2 W, printed twice | B |
| Grid stoppers | 2.2 K on each control grid | B |
| Grid leaks | 220 K at V8, 560 K at V9; V6 and V7 not legible | C |
| Bias supply node E | Simul-Class -61 V, 100 W -52 V, 60 W -43 V (export: -53 / -47 / -41) | C |
| Printed grid voltages | -45 V at V6 and at V8; further printed values -46.6, -54 and -33.6 attached to alternate operating modes | C |
| Bias adjustment | none; the sheet marks the divider "factory select" | B |
| Idle current | not printed anywhere | — |
| OT primary impedance | not printed anywhere | — |
| OT secondary | 8 ohm and 4 ohm taps, plus a second 4 ohm jack on the rear panel per S2 p.40 | A |
| Slave out | taken off the 8 ohm tap through a level pot and a 4.7 K | C |

The **output-transformer primary impedance and turns ratio are not established**, which is the
largest single hole in this dossier, because it sets the load line for every power-tube
calculation in §7. §8 says what would settle it.

---

## 6. Calibration targets

What a test can assert, and where each figure comes from.

| Target | Value | Source |
|---|---|---|
| Rated output, Simul-Class, all four tubes | **75 W** | S2 p.27, A |
| Rated output, Class A, outer pair only | **approximately 25 W** | S2 p.28, A |
| Class AB pair alone, Mesa's own figure | **roughly 50 W** | S2 p.28, A |
| Speaker taps | 8 ohm and 4 ohm, with a second 4 ohm jack | S2 p.40, A |
| Tube complement | five 12AX7 (V1–V5), four 6L6 (V6–V9); V8/V9 may be EL34 | S2 p.39 + S1, A |
| Preamp stage roles | V1A 1st gain, V1B 2nd gain, V3B 3rd lead, V4A 4th lead, V2A 5th lead, V2B loop return, V3A reverb return, V4B reverb drive, V5A/B driver | S2 p.39, A |
| V1A plate | 222 V at 1.07 mA | derived from S1, B |
| V1B plate | 255 V at 1.27 mA | derived from S1, B |
| V3B plate | 294 V at 1.2 mA | derived from S1, B |
| V4A plate | 294 V at 0.36 mA | derived from S1, B |
| V2B plate | 188 V at 1.2 mA | derived from S1, B |
| V1A gain, cathode bypassed | 70.6 (37.0 dB) | derived, B |
| V1B gain | 61.5 (35.8 dB) | derived, B |
| V3B gain | 56.7 (35.1 dB) | derived, B |
| V4A gain, loaded | 70 (36.9 dB) | derived, B |
| Interstage divider, V3B to V4A | 0.286 (-10.9 dB), corner 69.7 Hz | derived, B |
| Lead output divider | 0.3125 (-10.1 dB) | derived, B |
| Lead Drive at maximum | 0.595 (-4.5 dB) from the 680 K | derived, B |
| V4A cathode shelf corner | 219 Hz | derived, B |
| PULL DEEP corner, V2B | 339 Hz out, 10.3 Hz in | derived, B |
| EQ band resonances | 71.5 / 371.8 / 979.5 / 1576 / 4823 Hz | derived, B; three of five confirmed by S6 |
| EQ band Q, trap alone | 1.434 / 1.938 / 1.571 / 0.673 / 1.000 | derived, B |
| EQ range | about +17 / -18 dB, log slider taper | S6, C, measured on a Mark IV rather than a IIC+ |

**Not available, and worth saying plainly:** input sensitivity, preamp gain at a stated knob
position measured rather than derived, THD at any stated drive, the tone stack's frequency
response at flat, and the power section's output at the onset of clipping. Nobody has published
a bench measurement of a Mark IIC+. Everything in the derived rows above is arithmetic on a 1983
drawing, and the first real unit put on an analyser will correct some of it.

---

## 7. Proposed `PowerTubePreset` values

Field order, from `include/SushiDSP/circuits/stages/PowerTubePresets.hpp`:
`source_resistance_ohms, grid_leak_resistance_ohms, cathode_bias_volts, gain,
saturation_current_amps, thermal_voltage_volts, plate_saturation_drive`.

Reference point: `kEl34{ 10000, 220000, -20, 8, 1e-9, 0.02585, 0.05 }`, the JCM800 2203's EL34.

### 7.1 Calibrating the reference before extending it

`PentodeStage`'s `gain` is documented as "the stage's inverting linear gain from grid to plate
before saturation", so for a pentode it is g_m times the effective plate load of one push-pull
half. Taking the 2203's transformer at Raa = 3.4 kohm, one half sees Raa/4 = 850 ohm. An EL34 at
460 V with 70 mA of plate current has g_m near 11 mA/V and a plate resistance near 15 kohm, so

    A = g_m x (R_L parallel r_p) = 11 mA/V x (850 parallel 15000) = 11 mA/V x 803 = 8.8

which lands on `kEl34`'s 8 to within 10 %. That agreement is the whole basis for extending the
preset family by the same arithmetic, and it means the two presets below are derived on a stated
rule rather than picked. If part 2 finds that `kEl34`'s 8 came from somewhere else, both values
here have to be rebuilt.

Two of `kEl34`'s fields do **not** follow from the 2203's circuit and I could not reconstruct
where they came from. Its `source_resistance_ohms` of 10 000 sits above the 2203's 5.6 kohm grid
stopper but far below the phase inverter's own 35 kohm output impedance. Its `cathode_bias_volts`
of -20 sits well inside the 2203's real fixed bias of about -35 V. Both are listed in §8, because
the IIC+ presets have to adopt whichever convention `kEl34` used and I am guessing at it.

### 7.2 `6L6GC, pentode` — sockets V6 and V7, Class AB1

```
{ 37600, 220000, -45, 5.0, 1e-9, 0.02585, 0.049 }
```

| Field | Value | Reasoning |
|---|---|---|
| `source_resistance_ohms` | 37600 | `PentodeStage.hpp` defines the field as the Thevenin source feeding the grid, driver output impedance plus stopper: the phase inverter's 35 400 ohm output impedance plus the 2.2 K grid stopper read at every power-tube grid on S1 sheet 2. The stopper alone, 2200, is the alternative if a later measurement shows the driver impedance is already inside `gain`. |
| `grid_leak_resistance_ohms` | 220000 | Read at V8. V6 and V7's leaks were not legible and this value is carried across from the other pair. `[single source]` and an assumption; §8. |
| `cathode_bias_volts` | -45 | The grid voltage printed at V6 in Simul-Class. Under `kEl34`'s apparent convention of running about 0.57 of the physical bias, the preset value would instead be -25.6; §8 asks which. |
| `gain` | 5.0 | 6L6GC at 473 V plate and 480 V screen: g_m about 6 mA/V, r_p about 22.5 kohm from the RCA datasheet family. A = 6 mA/V x (850 parallel 22500) = 6 mA/V x 819 = 4.9. The 6L6 pentode pair is 20 log10(8 / 4.9) = 4.3 dB quieter than the 2203's EL34 into the same load, which is the arithmetic behind the Mark's headroom before the power stage contributes. |
| `saturation_current_amps` | 1e-9 | Unchanged from `kEl34`. No datasheet distinguishes 6L6 from EL34 grid-conduction saturation current, and changing it without evidence would be invention. |
| `thermal_voltage_volts` | 0.02585 | Room temperature, unchanged. |
| `plate_saturation_drive` | 0.049 | `kEl34`'s 0.05 puts the ceiling at 1/0.05 = 20. The IIC+ runs 473 V where the 2203 runs about 460 V, so the ceiling scales to 20 x 473/460 = 20.6 and the drive to 1/20.6 = 0.0486. |

### 7.3 `EL34, triode-strapped` — sockets V8 and V9, "Class A"

```
{ 37600, 220000, -45, 5.2, 1e-9, 0.02585, 0.069 }
```

| Field | Value | Reasoning |
|---|---|---|
| `source_resistance_ohms` | 37600 | Same Thevenin source as §7.2: 35 400 ohm phase-inverter output impedance plus the 2.2 K stopper. The stopper alone, 2200, is the alternative. |
| `grid_leak_resistance_ohms` | 220000 | Read directly at V8. V9's leak reads 560 K on the same sheet, a genuine disagreement between two sockets of the same pair; §8. |
| `cathode_bias_volts` | -45 | The grid voltage printed at V8. The two pairs share one bias supply, so the physical value is the same as §7.2 even though a triode-strapped EL34 at 473 V would conventionally want -35 to -40 V. |
| `gain` | 5.2 | Strapping screen to plate leaves g_m at about 11 mA/V but collapses the plate resistance from 15 kohm to about 1.06 kohm at mu 10.5. A = 11 mA/V x (850 parallel 1060) = 11 mA/V x 471 = 5.2. The low plate resistance, not the transconductance, is what makes the Class A pair quiet. |
| `saturation_current_amps` | 1e-9 | Unchanged. |
| `thermal_voltage_volts` | 0.02585 | Unchanged. |
| `plate_saturation_drive` | 0.069 | Anchored on Mesa's own power figures rather than on a load line, because the transformer impedance is unknown. S2 p.28 gives the triode pair about 25 W against the pentode pair's roughly 50 W, so the voltage ceiling ratio is sqrt(25/50) = 0.707, the ceiling becomes 20.6 x 0.707 = 14.6, and the drive becomes 1/14.6 = 0.0685. |

The two presets differ in `gain` by 4 %, which is the honest result: in Simul-Class the two pairs
contribute almost equally in the linear region and then clip at ceilings 20 log10(20.6 / 14.6) =
**2.99 dB** apart, which is the 3.01 dB that sqrt(25/50) guarantees by construction. That
staggered clipping, not any difference in level, is the audible content of "Simul-Class", and a
model that gets the ceilings right and the gains equal will sound closer than one that does the
reverse.

**The 850 ohm per-half load underneath every gain figure above is an assumption**, because §5
could not establish the transformer's primary impedance. If Raa turns out to be 1.9 kohm, which
is the other common Mesa value, all three gain figures fall by a factor near 2 and both presets
have to be recomputed. This is the highest-leverage open item in the dossier.

---

## 8. Open questions

**Blocking, because they change the model's numbers**

1. **The 750 Hz EQ inductor: 0.12 H or 0.22 H?** My reading of S1 gives 0.12 H, which resonates
   at 980 Hz; S5's Mark IV value of 0.22 H resonates at 723 Hz. The two differ by most of an
   octave on the band S2 calls the most powerful of the five. An LCR meter on an IIC+ EQ board,
   or a clean photograph of the inductor's marking, settles it in minutes.
2. **The 80 Hz EQ inductor: 1.5 H or 1 H?** Same question, same fix; 71.5 Hz against 87.6 Hz.
3. **Output transformer primary impedance and turns ratio.** Not printed on either sheet and not
   stated in any source I found. Everything in §7 rests on an assumed Raa of 3.4 kohm. A
   secondary-to-primary turns ratio measured with a signal generator on a real unit settles it,
   as does a Mesa part number with a published spec.
4. **What convention does `kEl34` use for `source_resistance_ohms` and `cathode_bias_volts`?**
   Its 10 000 and -20 do not equal the 2203's 5.6 kohm stopper or its roughly -35 V bias, and the
   JCM800 module carries no note saying where they came from. Until that is answered, §7's 2200
   and -45 may be on the wrong scale. Reading the JCM800 dossier or the original commit settles
   it without touching a bench.
5. **V6 and V7's grid-leak resistors.** Not legible; §7 carries V8's 220 K across to them. A
   higher-resolution scan of sheet 2, or a meter on a chassis, settles it.

**Structural, resolvable by looking at a real unit**

6. **The phase inverter's DC operating point does not close** (§5.1). The printed plate voltage
   implies 1.29 mA per half; the printed cathode voltage and the tail-resistor glyph imply two
   different tail resistances, neither of which is the one drawn. Three voltmeter readings on a
   powered chassis resolve it.
7. **The B-to-C-to-D supply dropper does not close** (§3). Either the 1 K is a 10 K or the
   printed 374 V is wrong.
8. **The 180 pF lead-drive bright cap's series resistor** (§3.3) reads as either 47 K or 470 K.
   A tenfold difference in where the bright cap starts working.
9. **V4A's PULL BRIGHT cathode resistor** (§3.4) is illegible. It sets how much of the 219 Hz
   shelf the switch removes.
10. **V2A's printed cathode voltage** was not legible, so §3.6's operating point is inferred from
    the stage's twin rather than read.
11. **The graphic EQ's active stage.** I read every band network and every printed node voltage,
    but I did not reconstruct how Q1 to Q4 connect to each other. Without that the EQ's gain
    structure, its input and output impedances, and therefore the in-circuit Q of all five bands
    cannot be computed. A high-resolution scan of that corner of sheet 1, or a trace off a board,
    is needed before the EQ can be modelled as anything better than five biquads.
12. **Which pair the CLASS A switch actually disables.** S2 says the Class A position uses the
    outer pair V8 and V9, and the switch on S1 is drawn near V6/V7, which is consistent with it
    disabling the inner pair. Consistent is not the same as read.
13. **Idle current per tube.** Not printed, not adjustable, factory-selected. Only a bench
    measurement gives it.
14. **Pot tapers.** Volume 1, Lead Drive, Lead Master, Master 1, the three tone controls and the
    five sliders. S6 states a log taper for the Mark IV sliders `[single source]`; nothing states
    any of the others.

**Coverage gaps**

15. **No bench measurement of a Mark IIC+ appears to have been published, ever.** Searched: the
    general web, the Boogie Board forum, DIY builder sites, and the Fractal Audio wiki. Absent:
    frequency response of the tone stack or the EQ, THD against drive, output power at clipping,
    input sensitivity, and any spectrum of the lead channel. This matches what the DD-3 dossier
    found for that pedal, and it means the IIC+ module will ship calibrated against arithmetic
    rather than against measurement until someone puts one on an analyser.
16. **jrb32's traced schematic (S3) and Dan__O's SLOCLONES version are both hosted on Google
    Drive and Google Sites links that I could not fetch programmatically.** Those two documents
    are the most likely place the answers to items 1, 2, 5, 8 and 9 are already written down.
    Opening them by hand in a browser is the cheapest next step in this whole list.
17. **No `mk2c_` SPICE deck is committed with this dossier.** The two blocks worth simulating are
    the tone stack and the EQ; the EQ's active stage is unreconstructed (item 11) and the tone
    stack's treble-cap interconnection sits at the edge of what the scan resolves. Writing a
    netlist I could not verify would put a guess into the repository wearing the clothes of a
    measurement. The decks belong in a follow-up task, after item 16.
