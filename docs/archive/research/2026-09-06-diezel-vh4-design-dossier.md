# Diezel VH4, channel 3 ("Mega") — Design Dossier

Compiled 2026-09-06 for spec `docs/superpowers/specs/2026-09-06-high-gain-amps-design.md`, task
DA1f. Every URL in this file was fetched on 2026-09-06.

---

## Verdict

**Channel 3's gain chain can be written as a white-box module. Its tone stack, its power section
and its supply voltages cannot, and the module as a whole is therefore a documented
approximation.**

Diezel has never published a VH4 schematic and says so in writing: the factory service manual
tells authorised repair centres that "We send schematics only without values of the components"
(§1, source S3). What exists instead is one circulating reverse-engineering drawing of the
preamp, titled `VH4`, credited to "Kojak", revision v1.0, dated 4 December 2012 (source S1). I
read it off the vector PDF at 7× to 16× magnification, not from OCR.

Three independent things say the channel-3 gain chain in that drawing is close to right.

1. Its stage assignment matches Diezel's own published tube layout. The factory manual lists
   `V1 = input stage for all channels`, `V3 = channel 2,3,4`, `V4 = all channels`,
   `V5 = mixer and send buffer` (S3). The drawing's ch3 path runs input stage → channel gain pot
   → a triode shared with ch4 → a shared gain stage and cathode follower → stack → volume →
   mixer, in that order.
2. Its DC operating points close. I solved every stage from the drawn resistor values against
   the repo's own Koren 12AX7 parameters (§3). The DC-coupled cathode follower is the sharp test,
   because its cathode voltage is fixed by the preceding plate and nothing was fitted: V3a's plate
   lands at 186.2 V and V3b's cathode at 187.1 V, giving Vgk = −0.95 V and 1.56 mA. A wrong plate
   resistor or a wrong cathode resistor would put that stage into cutoff or grid conduction.
3. Its send network reproduces two factory numbers that the tracer had no reason to reverse-fit.
   The drawn 100 k / 4.7 k output pad gives a source impedance of 4.5 kΩ at the send jack, and the
   owner's manual states "The output impedance is 4.7 kΩ" (S2). The cathode follower feeding it
   computes to 420 Ω out, and the service manual states "Buffer impedance is approximately 600
   ohm" (S3).

Four things block a white-box claim.

1. **The stack's interconnection is drawn in a form no production amp uses**, with the bass and
   mid capacitors landing on their pots' wiper terminals (§4). The amp tech `bmxguitarsbmx`, who
   repairs these for a living, read the same drawing and wrote "You are correct that the bass
   control is weird. Wire it normally." (S4). The five component values are legible and probably
   right; how they connect is a reconstruction.
2. **The supply is drawn as one undecoupled +367 V node feeding all eight triodes.** Real amps
   decouple. The same tech: "I've seen preamp voltages down to 385V, nothing as low as shown. The
   best ones I have seen use 400V on the preamp." (S4). Every operating point and every stage gain
   in §3 moves with that number, and I give both 367 V and 400 V columns because of it.
3. **The drawing is one triode short in two places.** Diezel's layout gives channel 4 a whole
   twin-triode (`V2 = channel 4`) and the very early VH4 gives the clean channel its own
   (`V2 = clean channel`, S3); the drawing has one triode for each. Channel 3 does not use those
   triodes, so this is evidence about the trace's completeness rather than a hole in channel 3.
4. **No VH4 power-amp schematic is publicly readable.** One exists — a member of
   music-electronics-forum posted an attachment named `DIEZEL VH4 Power Amp.pdf` on 2019-03-13
   (S5) and it is behind a permissions wall — and another circulates well enough that an Amp
   Garage poster asked about the inductor in its Deep control (S6). I could reach neither. §5 is
   therefore built from Diezel's own prose and its biasing procedure, and it carries no component
   values at all.

**A fifth point, for the maintainer rather than for part 2's implementer.** The spec picks the KT77
as this module's power tube (`docs/superpowers/specs/2026-09-06-high-gain-amps-design.md` §2, the
amplifier table). No source in this dossier puts a KT77 in a VH4. Diezel's own owner's manual,
service manual and product page name **no power-tube type at all**; the service manual only rules
types out ("We do not recommend the 'big bulbs' like KT88, 6550 or KT66", S3). What evidence there
is points to the EL34: Diezel sells the amplifier under the model name "VH4 EL34 Head" (S15), the
bias selector is "labelled 'EL34 and 6L6'" (S7), and the factory biasing table lists 5881, 6L6,
EL34 and 6550/KT66/KT88 without mentioning a KT77 (S7). The KT77 is a drop-in EL34 replacement, so
the spec's choice is defensible as a voicing decision, but it is a scope decision and not a fact
about the amplifier. §7 states which of the seven preset fields move if the module is built for the
EL34 instead. **Revisit the choice before part 2 is planned.**

What that means for part 2: write the four gain stages and the cathode follower from §3 and treat
them as white-box. Treat the tone stack as an FMV instance whose topology is the reconstruction in
§4 and whose acceptance test is a sweep, not a null. Treat the power section as a parameterised
block whose numbers come from §5's targets and §7's preset, not from a circuit. Say all of this in
the module's own README.

---

## 1. Sources

| Ref | Source | Origin, revision, date | How far it was trusted |
|---|---|---|---|
| **S1** | Reverse-engineered VH4 preamp schematic. Title block reads `VH4`, `Document Number: Kojak`, `Rev v1.0`, `Tuesday, December 04, 2012`. <https://stremelguitars.files.wordpress.com/2022/06/vh4preamp.pdf> | Anonymous trace, 2012, no serial number, no PCB revision, no methodology stated. Mirrored at <https://schems.com/bmampscom/diezel/Diezel%20VH4%20preamp.pdf> and <https://elektrotanya.com/diezel_vh4_preamp_sch.pdf/download.html>. | Every component value in §3 and §4 comes from here, read visually off the vector PDF at 7×–16×. The embedded font's cmap is scrambled, so the extracted text layer is garbage (`12A7V` for `12AX7`, `4V00` for `470k`); nothing here was taken from it. Values trusted; topology trusted where §3's DC solve closes; stack topology not trusted (§4). |
| **S2** | Diezel VH4/VH4S Owner's Manual, Peter Diezel, revision dated 26 Oct 2016. <https://www.diezelamplification.com/manuals/en/VH4&VH4S_Manual_26102016.pdf> | Manufacturer. | Trusted for control descriptions, the Presence and Deep centre frequencies, the channel-3 design intent and the send impedance. |
| **S3** | Diezel Tube Amp Service Manual V1.3. <https://www.diezelamplification.com/manuals/service/service-manual_V1-3.pdf> (V1.1 also fetched, same content for the pages used) | Manufacturer, undated internally. | Trusted for the tube layouts, the biasing procedure, input and loop impedances, power consumption, the negative-feedback wiring and the schematic-availability policy. |
| **S4** | freestompboxes.org thread "Diezel VH4- Channel 3 Preamp Idea", opened 2022-06-12. <https://www.freestompboxes.org/viewtopic.php?t=32114> | Builder `Bow` posts a hand reduction of S1 to channel 3 only (<https://stremelguitars.files.wordpress.com/2022/06/d-ch3pre.jpg>, drawing dated 14 JUN 2022); amp tech `bmxguitarsbmx` corrects it from bench experience. | The reduction agrees with S1 value-for-value where I checked it, which is a second reading of the same drawing rather than a second source. The tech's corrections are quoted as opinion of a named practitioner, and marked `[single source]`. |
| **S5** | music-electronics-forum thread "Diezel VH4", 2019-03-13. <https://music-electronics-forum.com/forum/schematic-requests/49843-> | Member `akaThermionick` posts an attachment named `DIEZEL VH4 Power Amp.pdf`. | Existence only. The attachment needs forum permissions; I could not read it. |
| **S6** | The Amp Garage, "Diezel VH4 Deep Inductor?", 2022-08-30. <https://ampgarage.com/forum/viewtopic.php?t=35666> | A poster describes a VH4 power-amp schematic he found showing an inductor from the Deep pot to ground. | The one public statement about the Deep control's topology. `[single source]`, and the schematic itself is attached behind permissions. |
| **S7** | Rig-Talk, "How to bias your VH4 w/DMM", `duesentrieb` (Olaf, Diezel-affiliated), 2008-01-08. <https://www.rig-talk.com/forum/threads/how-to-bias-your-vh4-w-dmm-attention-lethal-volts-inside.24410/> | Reproduces the procedure Diezel published on its own tech-corner page. | Trusted for the idle-current targets and the ~500 V figure on the OT primary. |
| **S8** | Diezel product page for the VH4. <https://www.diezelamplification.com/vh4/> | Manufacturer, marketing. | Presence and Deep centres, output power, control complement. Note it says Deep is centred "around 90 Hz" where the owner's manual (S2) says 80 Hz. |
| **S9** | Aion FX DZ4 Preamp documentation v1.0.3, 2022-08-15. <https://aionfx.com/app/files/docs/dz4_preamp_documentation.pdf> | Analysis of the *Diezel VH4 pedal* (2016), an op-amp circuit. | Voicing hints only, never values. Its Deep is "an active bass boost/cut centered around 115 Hz". |
| **S10** | Koren 12AX7 parameters, `docs/reference/spice/models/12ax7_koren.sub` in this repository. | mu 100, kp 600, kvb 300, kg1 1060, kx 1.4. | Used for every DC solve and every small-signal figure in §3. The numbers in §3 are properties of *this model plus S1's resistors*, not measurements of an amp. |
| **S11** | Rig-Talk, "Phase inverter tube question", 2009-12-29. <https://www.rig-talk.com/forum/viewtopic.php?f=14&t=60070> | `steve_k`: "The phase inverter tube is already a 12AX7". `duesentrieb`: "You can use 12AT7, 5751 and ECC81 w/out any problems." | Corroborates the V7 position in S3. `[single source]` for the substitution list. |
| **S12** | Genalex KT77 datasheet, New Sensor Corporation (the M-O Valve sheet reissued). PDF: <https://www.thetubestore.com/lib/thetubestore/genalex-kt77.pdf>, linked from <https://www.thetubestore.com/genalex-gold-lion-kt77> | Manufacturer datasheet, undated. Beam tetrode. | The whole of §7's KT77 argument. Pages 1–3 give the maximum ratings, the fixed-bias grid-resistor limits, the tetrode characteristics and the ultra-linear class-AB1 fixed-bias table including a Vb = 500 V column. |
| **S13** | Philips/Mullard EL34 datasheet, January 1969. <https://frank.pocnet.net/sheets/010/e/EL34.pdf> | Manufacturer datasheet, scanned; the operating tables are on page 3. | Used in §7 as the cross-check on S12's bias figure, and for the EL34 anode-dissipation rating. |
| **S14** | `escaroda/goya1746`, a KiCad pedal PCB based on VH4 channel 3, MIT, created 2025-10-23. <https://github.com/escaroda/goya1746> | A builder's op-amp adaptation with a full published bill of materials. | Pot tapers only, and cited as the builder's choice rather than as a reading of the amp. Its component values are a 12–18 V op-amp circuit's, not the amplifier's. |
| **S15** | Diezel VH4 EL34 Head, dealer listing. <https://www.thomannmusic.com/diezel_vh4_el34_head.htm> | Retail listing carrying Diezel's own current model name. | The only reason to say the stock power tube is an EL34, and the reason it is marked `[single source, dealer listing]` in §5. Diezel's own documents name no power-tube type. |

Values marked `[single source]` rest on one uncorroborated statement. Values marked `[derived]`
are mine, computed from S1's components and S10's tube model.

---

## 2. Block overview

Channel 3's path through the drawn preamp, with Diezel's own tube-position names (S3) beside the
drawing's references (S1). Component references are S1's.

```
IN ─ R1 1M ∥ C1 100-200pF ─ ferrite bead ─┐
                                          │
  V1a  12AX7  [Diezel V1, input stage, all channels]
    Ra R3 120k → +367V ;  Rk R2 2k2 ∥ C2 47µF (fully bypassed)
    plate ──┬─ C3 220nF ──────────────► CH1 network
            ├─ C8  10nF ──────────────► CH2 network → V2b
            ├─ C14 1nF + R15 220k ────► CH4 gain 250k → V1b
            └─ C13 1nF + R14 220k ────► CH3 GAIN 250k pot to gnd   ◄── the ch3 voicing split
                                          │
  R21 47k grid stopper ; R22 3M3 grid leak to gnd
                                          │
  V2a  12AX7  [Diezel V3, channels 2/3/4 — shared with ch4]
    Ra R24 120k → +367V ;  Rk R23 680R ∥ C17 1µF (partial bypass, 234 Hz zero)
    plate ─ C18 4.7nF ─┬─ channel-select relay ─ R26 470k ─┐
                       └─ R24' 1M, R25' 1M to gnd          │
                                                           │
  V3a  12AX7  [Diezel V4, all channels]
    Ra R28 120k → +367V ;  Rk R27 680R ∥ C19 2.2µF (partial bypass, 106 Hz zero)
    plate ─── DC-coupled ───┐
                            │
  V3b  12AX7 CATHODE FOLLOWER  [Diezel V4b]
    plate direct to +367V ;  Rk R29 120k to gnd, unbypassed
    cathode ─ C20 220nF ─┬─ R30 1M to gnd
                         ├─ C20' 10µF + R31 470k to gnd     ◄── see §3.6, value doubted
                         │
  TONE STACK (ch3 copy: R40, R41, C27, C28, C29 — all four channels' stacks are identical)
    R40 10k ─┬─ C27 560pF ─ TREBLE 220k ─┐
             └─ R41 39k ─┬─ C28 22nF ─ BASS 1M ─┤ (pots in one series column to gnd)
                         └─ C29 22nF ─ MID 22k ─┘
                         │
    stack out ─ R42 470k ─ CH3 VOLUME 470k ─ relay ─ R43 680k ─┐
                                                               │
  MIX NODE at V4a grid: ch1 through R35 56k, ch2 R39 680k, ch3 R43 680k, ch4 R47 680k.
    The SPDT relays ground the three unselected legs, so ch3 sees 680k into 48.1k. [S4]
                                                               │
  V4a  12AX7  [Diezel V5, mixer]
    Ra 120k → +367V ;  Rk 640R UNBYPASSED (7.95 dB of local degeneration)
    plate ─ DC-coupled ─┐
  V4b  12AX7 CATHODE FOLLOWER  [Diezel V5b, send buffer]
    plate to +367V ;  Rk 120k
    cathode ─ C 220nF ─ R 100k ─┬─ R 4.7k to gnd ─► FX SEND jack (4.5 kΩ source)
                                └──────────────────► serial loop → return driver → PI → power amp
```

Everything past the send jack — the loop return driver (Diezel V6), the phase inverter (Diezel V7)
and the output stage — is absent from S1. §5 covers it from prose alone.

---

## 3. Preamp, stage by stage

All resistor and capacitor values are S1's. All currents, voltages, gains and corner frequencies
are `[derived]`: I solved each stage's self-bias by bisection against the Koren 12AX7 of S10, then
took gm and rp by central difference at the solved point. Both supply columns are given because
S1's +367 V is contradicted by S4.

**Supply node.** S1 draws a single `+367V` rail on every plate and on both cathode-follower
plates, with no decoupling resistors and no filter capacitors anywhere on the sheet. That is a
tracer's simplification of a real decoupled chain, and it is the single largest uncertainty in
this section. `bmxguitarsbmx`: "I've seen preamp voltages down to 385V, nothing as low as shown.
The best ones I have seen use 400V on the preamp." `[single source, S4]` He adds that he built a
channel-3 preamp at 350 V and "it was way to compressed sounding and any gain setting above 1.5
(out of 10) was way too much to be useable ... in the end it didnt sound like a Diezel at those
low voltages."

### 3.1 V1a — input stage, shared by all four channels

| | |
|---|---|
| Grid leak | R1 1 MΩ to ground (Diezel's own spec says "Impedance more than 500 kohm", S3) |
| Input capacitor | C1, printed as the range `100-200pF`, across R1, plus a ferrite bead in series with the grid |
| Grid stopper | none drawn |
| Plate resistor | R3 120 kΩ |
| Cathode | R2 2k2 with C2 47 µF across it |
| Coupling to ch3 | C13 1 nF into R14 220 kΩ |

C1's label is a range, not a value: the tracer printed `100-200pF` rather than commit to one
figure, which is what someone does after seeing both across several units. The rendered glyphs are
authoritative here — S1's scrambled text layer reads `1001200pF`, because the hyphen maps to `1` in
the same substitution that turns `470k` into `4V00`. C1 feeds no calculation in this dossier.

| Supply | Ia | Va | Vk | Bias Vgk | gm | rp | A (unloaded) |
|---|---|---|---|---|---|---|---|
| 367 V | 0.931 mA | 255.3 V | 2.05 V | −2.05 V | 1.64 mA/V | 54.6 kΩ | 61.4 (35.8 dB) |
| 400 V | 1.020 mA | 277.6 V | 2.24 V | −2.24 V | — | — | — |

**Closes.** 0.931 mA through 120 kΩ drops 111.7 V, leaving 255.3 V on the plate and 253.2 V across
the tube — a conventional 12AX7 point with room on both sides.

C2 at 47 µF puts the cathode zero at 1.5 Hz, so the stage is bypassed flat across the audio band.
The 2k2 is a cold-ish bias for a 12AX7 input stage (a Marshall uses 820 Ω) and it is what keeps
this stage clean while four channels hang off its plate.

**The one number that voices channel 3 more than any other is C13.** The four channels tap V1a's
plate through four different capacitors into four different resistors:

| Channel | Coupling | Series R | −3 dB corner into its gain pot `[derived]` |
|---|---|---|---|
| 1, clean | C3 220 nF | R4/R5/R6 470k ladder + shunts | below 10 Hz |
| 2, crunch | C8 10 nF | R8 220k, R9 3M3 ∥ C9 680pF | ~30 Hz |
| 3, mega | **C13 1 nF** | R14 220 kΩ | **314 Hz** (gain at max) to **618 Hz** (gain at min) |
| 4, lead | C14 1 nF | R15 220 kΩ | same as ch3 |

A 1 nF coupling capacitor into a third of a megohm is a first-order high-pass with its corner in
the middle of the guitar's range. Everything below the low E's second harmonic is rolled off
*before* the first distorting stage. That is the mechanism behind Diezel's own description of the
channel: "The distortion is noticeably tighter than in channel 2. The side effect of this added
compression is that he signal gets somewhat limited in it's dynamic range. This limitation hits
mostly the lower frequencies. This loss is corrected by a negative feedback loop to the power
amps ... The control for this low end is in the master section and bears the 'Deep' designation."
(S2). A model that puts a gentle 30 Hz high-pass here has not modelled channel 3.

### 3.2 The channel-3 GAIN control

A 250 kΩ pot, wired as a plain voltage divider to ground, fed from R14 and feeding R21. S1 does
not mark a taper; the panel behaves as an audio taper on every other Diezel control, and the pedal
adaptations use `A250K` (S9 and the goya1746 build, §8 open question 6). Taper: **not established**.

There is no interstage divider anywhere else in the channel. The gain pot *is* the divider, which
is why the channel is so sensitive to it. Diezel: "guitars with very high output (active EMG® etc.)
might overdrive this channel too much and become too compressed ... It is best to start with a
12:00 o-clock gain setting" (S2). `bmxguitarsbmx` says the 3M3 grid leak at the next stage exists
"just to tame gain. There is a shit ton of gain in the vh4 ch3", and that he has "only seen 1M or
220k" in that position on real boards `[single source, S4]`.

### 3.3 V2a — first distorting stage, shared with channel 4

| | |
|---|---|
| Grid stopper | R21 47 kΩ |
| Grid leak | R22 3M3 to ground (disputed: see above) |
| Plate resistor | R24 120 kΩ |
| Cathode | R23 680 Ω with C17 1 µF across it |
| Coupling out | C18 4.7 nF |
| Load seen | **500 kΩ to ground** (R24' 1M ∥ R25' 1M), then R26 470 kΩ in series into V3a's grid |

**The load is a shunt, not a divider.** I re-read this corner of S1 at 18× to settle it. C18's
right-hand plate is node A, which carries R24' 1 MΩ to ground. A relay contact joins node A to
node B, which carries R25' 1 MΩ to ground and a vertical bus down from the CH1 gain pot. A second
relay contact joins node B to node C, which carries a vertical bus down from channel 2's V2b
network and then R26 470 kΩ into V3a's grid. With channel 3 selected, both contacts close, A, B
and C are one node, and V2a's AC load is R24' ∥ R25' = **500 kΩ**. R26 terminates on a DC-coupled
grid that draws no current at midband, so it contributes nothing to the load; it is a grid stopper,
and its only audible effect is the Miller rolloff computed in §3.4.

| Supply | Ia | Va | Vk | Bias Vgk | gm | rp | A (unloaded) | A (into 500 kΩ) |
|---|---|---|---|---|---|---|---|---|
| 367 V | 1.507 mA | 186.2 V | 1.02 V | −1.02 V | 2.32 mA/V | 41.4 kΩ | 71.4 (37.1 dB) | 67.2 (36.6 dB) |
| 400 V | 1.652 mA | 201.7 V | 1.12 V | −1.12 V | — | — | — | — |

**Closes.** −1.02 V of bias against a 12AX7's grid-conduction knee at about −0.5 V leaves half a
volt of clean headroom on the grid, which is the point: the stage clips into grid conduction on
anything but the smallest signal, asymmetrically, and that is what a hot-biased high-gain stage is
for.

The 680 Ω cathode resistor with only 1 µF across it is a deliberate bass cut inside the stage.
`[derived]` The zero sits at 234 Hz, the pole at 603 Hz, and the loss below the zero is
**8.22 dB**. Combined with C13's 314 Hz corner ahead of it, channel 3 has thrown away most of its
low end before it distorts. The 47 kΩ grid stopper is large for a preamp — it rolls the top against
the following Miller capacitance and stops the stage oscillating at this much gain.

### 3.4 V3a — second distorting stage, shared by all four channels

| | |
|---|---|
| Grid stopper | R26 470 kΩ (fed from the relay node, R24'/R25' = 500 kΩ to ground) |
| Grid leak | the 500 kΩ pair above |
| Plate resistor | R28 120 kΩ |
| Cathode | R27 680 Ω with C19 2.2 µF across it |
| Coupling out | none — DC-coupled to V3b's grid |

| Supply | Ia | Va | Vk | Bias Vgk | gm | rp | A (unloaded) |
|---|---|---|---|---|---|---|---|
| 367 V | 1.507 mA | 186.2 V | 1.02 V | −1.02 V | 2.32 mA/V | 41.4 kΩ | 71.4 (37.1 dB) |
| 400 V | 1.652 mA | 201.7 V | 1.12 V | −1.12 V | — | — | — |

**Closes**, identically to V2a — same resistors. Its cathode network differs: 2.2 µF instead of
1 µF moves the zero down to 106 Hz and the pole to 274 Hz, same 8.22 dB of shelf. `[derived]`

C18 (4.7 nF) into V2a's 30.8 kΩ plate impedance plus the 500 kΩ shunt of §3.3 gives a −3 dB corner
at **63.8 Hz** `[derived]`, a third bass cut in series with the first two.

A 470 kΩ grid stopper is very large. At the ~115 pF of Miller capacitance a 12AX7 at this gain
presents, it rolls the top off at about 2.9 kHz `[derived]`. In this drawing that pole, and not the
tone stack, is what limits the channel's top end.

### 3.5 V3b — cathode follower, DC-coupled

| | |
|---|---|
| Plate | direct to +367 V, no resistor |
| Grid | V3a's plate, DC-coupled, no coupling capacitor and no grid resistor |
| Cathode | R29 120 kΩ to ground, unbypassed |
| Coupling out | C20 220 nF |

| Supply | Vg (= V3a plate) | Ia | Vk | Bias Vgk | Gain | Zout |
|---|---|---|---|---|---|---|
| 367 V | 186.2 V | 1.560 mA | 187.1 V | −0.95 V | 0.986 | 420 Ω |
| 400 V | 201.7 V | 1.690 mA | 202.8 V | −1.07 V | — | — |

**Closes, and this is the strongest single check on the whole trace.** Nothing about this stage is
free. The cathode voltage is whatever 120 kΩ and the tube agree on given a grid held at V3a's plate
voltage, and the answer has to land inside a window a few volts wide or the stage does not work.
It lands at −0.95 V of bias and 1.56 mA — within 4 % of V3a's own current, which is exactly what a
DC-coupled follower pair does. Change R28 or R29 by one E12 step and the point moves out of the
window.

The follower has 180 V across the tube and 187 V on the cathode, so it swings hard in both
directions and drives the tone stack from 420 Ω. Diezel's service manual gives "Buffer impedance is
approximately 600 ohm" for the send buffer (S3), which is the same topology one stage later; 420 Ω
against 600 Ω is agreement at the precision either number deserves.

### 3.6 The network between the follower and the stack

S1 draws, on the follower's output side of C20: `R30 1 MΩ` to ground, and a second capacitor also
labelled C20 at `10 µF` in series with `R31 470 kΩ` to ground. Two components carry the reference
C20 on this sheet, which is itself a sign of a hand-assembled drawing.

As drawn the network does nothing audible. 10 µF in series with 470 kΩ has a corner at 0.034 Hz, so
it is simply a 470 kΩ shunt across the whole band, and the follower's total AC load becomes 320 kΩ
— against its 420 Ω output impedance, that is no load at all. The coupling corner through C20's
220 nF is 2.26 Hz. `[derived]`

`bmxguitarsbmx` says this position is real but the values are not: "the 10uF+470k is all over the
place or non-existent. A value of 2n2+47k to 100k is good. This network gives a lot of compression
and so you may want to leave it removed at first. 10uF + 470k won't do anything though, so you can
skip that." `[single source, S4]` A 2.2 nF in series with 47 kΩ to ground is a treble shunt with a
corner near 1.5 kHz, which is a completely different circuit from what is drawn. **Do not implement
this network from S1.** §8 lists what would settle it.

---

## 4. Tone network

Channel 3's stack is `R40, R41, C27, C28, C29` plus the Treble, Bass and Mid pots. All four
channels' stacks are drawn with identical values in S1; only the reference designators differ
(ch1 R32/R33/C21-23, ch2 R36/R37/C24-26, ch3 R40/R41/C27-29, ch4 R44/R45/C30-32).

### Position in the chain

The stack sits **after** the shared cathode follower and **before** the channel volume, so it is
driven from 420 Ω and loaded by R42 470 kΩ into the 470 kΩ volume pot. That is a low-impedance
drive into a light load, which makes the stack's own loss the whole story and leaves the pots
interacting with each other rather than with the stage on either side.

### Values

| Ref | Value | Role |
|---|---|---|
| R40 | 10 kΩ | series input resistor, from the follower bus into the stack node |
| C27 | 560 pF | treble capacitor |
| R41 | 39 kΩ | slope resistor |
| C28 | 22 nF | bass capacitor |
| C29 | 22 nF | mid capacitor |
| Treble | 220 kΩ | linear taper on the goya1746 adaptation (`B250K`), unmarked in S1 |
| Bass | 1 MΩ | audio taper on the goya1746 adaptation (`A1M`), unmarked in S1 |
| Mid | 22 kΩ | linear taper on the goya1746 adaptation (`B25K`), unmarked in S1 |
| R42 | 470 kΩ | series resistor into the channel volume |
| CH3 volume | 470 kΩ | channel volume, feeding the mix bus through R43 680 kΩ |

Two of these are worth naming against the Marshall stack the JCM800 module already implements
(`docs/reference/spice/jcm_fmv`): the VH4's slope path is **10 kΩ + 39 kΩ split into two** where a
2203 has a single 33 kΩ, and the mid pot is **22 kΩ** against the 2203's 25 kΩ. The treble
capacitor is 560 pF against the 2203's 470 pF. Everything else is the same family.

### Topology, and why it is the weak point

As literally drawn in S1:

- The follower bus feeds R40 10 kΩ into node S.
- From node S, C27 560 pF runs to the top terminal of the Treble pot.
- From node S, R41 39 kΩ runs down to node B.
- From node B, C28 22 nF runs to the **wiper** of the Bass pot.
- From node B, C29 22 nF runs to the **wiper** of the Mid pot.
- The three pots form one series column: Treble bottom to Bass top, Bass bottom to Mid top, Mid
  bottom to ground.
- The output leaves from the Treble pot's top/wiper node into R42.

Capacitors landing on wiper terminals is not how a production FMV stack is built. In the standard
arrangement the bass capacitor feeds the bass pot's *top*, the mid capacitor feeds the mid pot's
*top*, the treble pot's *bottom* joins the bass/mid junction rather than the bass top, and the
output leaves from the treble *wiper*. Read that way, S1's five values slot into an ordinary FMV
stack and everything is consistent. Read literally, the treble pot's upper section is shorted and
the control does not work.

`bmxguitarsbmx`, reading the same sheet: "The way that dude drew the schematic shows all the
channel switching before the tonestack driver as an SPST relay. It is actually a SPDT relay ... You
are correct that the bass control is weird. Wire it normally." `[single source, S4]` He is right
about the relays — the SPDT reading is what makes the 48.1 kΩ mix-bus shunt in §2 come out — which
is a reason to take his tone-stack remark seriously too.

**Recommendation for part 2.** Implement `FmvToneStack` with R = 10 k + 39 k, C_treble = 560 pF,
C_bass = 22 nF, C_mid = 22 nF, pots 220 k / 1 M / 22 k, in the standard FMV interconnection, and
record in the module README that the interconnection is reconstructed. The flat-position response
is then a prediction to be checked against a real amp, not an asserted target.

### The Deep and Presence controls are not here

Both live in the power amp's negative-feedback loop and are global, not per-channel (§5). Channel
3's panel has Gain, Bass, Middle, Treble and Volume; Deep, Presence and Master sit in the master
section (S8).

---

## 5. Power section

**No component values are established.** Nothing in this section comes from a schematic. S5 and S6
show that a power-amp trace exists in private hands; neither is readable.

### What Diezel states

| Fact | Source |
|---|---|
| 100 W, four power tubes, "four tube power amp section with bias control" | S8 |
| **Diezel's own documents name no power-tube type.** The product page (S8), the owner's manual (S2) and the service manual (S3) contain no occurrence of `EL34`, `6L6` or `KT77`. The service manual only excludes types: "We do not recommend the 'big bulbs' like KT88, 6550 or KT66 in our amplifiers. These tubes need a higher bias rating." | S2, S3, S8 |
| Stock tube is the EL34, on the strength of Diezel's model name "VH4 EL34 Head" and the bias selector switch "labelled 'EL34 and 6L6'" | S15 `[single source, dealer listing]`, S7 |
| Tube positions: `V6 = return driver serial and parallel`, `V7 = phase inverter` | S3 |
| Phase inverter is a 12AX7; a 12AT7, 5751 or ECC81 substitutes without trouble | S11 `[single source]` |
| Bias is **fixed**, from one trimmer per pair, with a panel selector switch "labelled 'EL34 and 6L6'" | S7 |
| **~500 V on the OT primary.** Olaf's warning is "keep in mind that there are 500 Volts on the OT". A 50 W VH4S owner separately estimates "480VDC or so"; that is one owner's assumption about a different model, so this dossier uses 500 V throughout and never 480 V. | S7 |
| Idle current, factory text: **"On our recommended tubes you should have a bias current of 35 milliamps each tube. Most of our amps adjust a pair of output power tubes with one bias trim pot, or 70 milliamps for each pair."** | S3 |
| Idle current, the published biasing procedure: EL34 **60–70 mA per pair**, i.e. 30–35 mA per tube. Olaf recommends the cold end: "the colder you set the tubes, the less often you need to check the bias" | S7 |
| Idle current, other types: 5881 50 mA/pair, 6L6 60–70 mA/pair, 6550/KT66/KT88 80 mA/pair. **No KT77 row exists.** | S7 |
| Each power tube has its own 250 mA slow-blow fuse and a fault LED | S2, S3 |
| The output transformer has a centre-tapped primary; the biasing procedure reads current in the red, blue and yellow primary leads | S7 |
| **Negative feedback is taken from the 8 Ω speaker jack through a white wire.** The factory mod for an early VH4 reads: "If you also want to add more low end to the power amp insert a 0,1µF / 250 volt cap in the white feedback wire coming from the 8 ohm speaker out jacks. If the white wire is connected to the 4 ohm jacks rewire it to the 8 ohm speaker outputs." | S3 |
| Presence "controls the treble part of the power amps tone spectrum. Its frequency centers at about 4kHz." | S2 |
| Deep is "an active bass control for the power amp that adds low end punch to the power amp centered at 80Hz. It does not alter the dynamic behavior of the power amp." | S2. S8 says "centered at around 90 Hz" — the two factory documents disagree by 10 Hz. |
| Deep works by correcting channel 3's deliberate low-end loss: "This loss is corrected by a negative feedback loop to the power amps, which adds fat low end to this tone" | S2 |
| The Deep pot has an **inductor to ground**, forming an LC bandpass in the feedback loop rather than the usual RC | S6 `[single source]`. Roberto at Amp Garage suggests 500–600 mH and ~15 Ω for a substitute, "like wah inductors" — that is his recommendation, not a reading of the amp. |
| Master volume: "full volume is achieved at the 1:00 o'clock knob setting. Turning the knob further will overdrive the power amp(s)" | S2 |
| Loop return: "−10 dB minimum voltage for full power amp output"; return DC resistance and impedance about 47 kΩ | S3 |
| Speaker outputs: 4, 8 and 16 Ω, "the impedance of the loudspeaker ... should match the designation of the output jacks" | S2 |
| Maximum mains power consumption, 100 W models: 370 W | S3 |

### What follows for the model

Both master-section controls are shelves inside one global feedback loop taken from the 8 Ω
secondary. That is the same place the JCM800 module's presence already lives, so
`ResonanceControl` (spec §4) is the right brick for Deep, with the corner at 80 Hz and the
existing presence brick at 4 kHz.

The LC form of the Deep network matters if it is real. An LC bandpass in the feedback path has a
bounded, resonant lift with skirts on both sides, unlike an RC shelf which lifts everything below
its corner. Roberto's warning is the audible consequence: "If the resonance of the speaker/cab is
out of the LC filter, you get (almost) no depth effect." A first-order shelf will not reproduce
that interaction with the cabinet's resonance. Since `ResonanceControl`'s constructor takes a
corner frequency and a depth range, a first pass gets the shelf; §8 records what a second pass
needs.

**Output transformer primary impedance and turns ratio: not established.** A 100 W quad of EL34s
at ~500 V plate conventionally wants 3.4–4 kΩ plate-to-plate, but no VH4 measurement supports any
figure and I am not going to write one into a dossier that a module will read.

**Screen resistors: not established.** **Phase-inverter type and values: not established** beyond
the tube being a 12AX7 in the V7 position, which for a 100 W four-tube amp of this era makes a
long-tailed pair the overwhelmingly likely form — an inference, not a fact.

---

## 6. Calibration targets

Figures a test can assert, each with where it comes from. The `[derived]` ones are properties of
S1's components plus S10's tube model, so they test the model against the *documented circuit*, not
against a real amplifier.

### From the manufacturer

| Target | Value | Source |
|---|---|---|
| Guitar input impedance | > 500 kΩ (S1 draws 1 MΩ) | S3 |
| Recommended cable capacitance | ≤ 300 pF | S3 |
| FX send source impedance | 4.7 kΩ | S2 |
| FX send buffer output impedance | ~600 Ω | S3 |
| FX send level | "up to +4 dB depending on channel volume and guitar output", volumes at noon | S3 |
| Loop return input impedance | ~47 kΩ | S3 |
| Loop return sensitivity | −10 dB for full power-amp output | S3 |
| Presence centre | ~4 kHz | S2 |
| Deep centre | 80 Hz (S2) / ~90 Hz (S8) | S2, S8 |
| Rated output | 100 W | S8 |
| Master volume clean ceiling | 1 o'clock | S2, S8 |
| Idle current | 35 mA per tube, 70 mA per pair (factory text); the biasing procedure's EL34 range is 30–35 mA per tube. At 500 V that is 15.0–17.5 W of plate dissipation. | S3, S7 |

### Derived from S1 and S10 — what a correct channel-3 model must hit

| Target | Value at +367 V | Value at +400 V |
|---|---|---|
| V1a operating point | 0.931 mA, Va 255.3 V, Vgk −2.05 V | 1.020 mA, 277.6 V, −2.24 V |
| V2a operating point | 1.507 mA, Va 186.2 V, Vgk −1.02 V | 1.652 mA, 201.7 V, −1.12 V |
| V3a operating point | 1.507 mA, Va 186.2 V, Vgk −1.02 V | 1.652 mA, 201.7 V, −1.12 V |
| V3b cathode voltage | 187.1 V, Ia 1.560 mA, Vgk −0.95 V | 202.8 V, 1.690 mA, −1.07 V |
| V4a operating point | 1.533 mA, Va 183.0 V, Vgk −0.98 V | 1.681 mA, 198.3 V, −1.08 V |
| V1a midband gain | ×61.4 (35.8 dB) unloaded, ×56.8 into 470 kΩ | — |
| V2a midband gain | ×71.4 (37.1 dB) unloaded, ×67.2 (36.6 dB) into the 500 kΩ shunt | — |
| V3a midband gain | ×71.4 (37.1 dB) | — |
| V4a midband gain | **×28.7 (29.1 dB)** — the 640 Ω cathode is unbypassed, costing 7.95 dB | — |
| V3b follower gain | 0.986, Zout 420 Ω | — |
| Channel-3 input high-pass (C13) | 314 Hz at gain max, 618 Hz at gain min | — |
| V2a cathode shelf | zero 234 Hz, pole 603 Hz, 8.22 dB of LF loss | — |
| V3a cathode shelf | zero 106 Hz, pole 274 Hz, 8.22 dB of LF loss | — |
| V3a coupling high-pass (C18) | 63.8 Hz (4.7 nF into 30.8 kΩ + 500 kΩ) | — |
| Follower output high-pass (C20) | 2.26 Hz | — |
| Mix-bus attenuation, ch3 selected | 680 kΩ into 48.1 kΩ = ×0.0660, **−23.6 dB** | — |
| Send pad | 4.7 kΩ / (100 kΩ + 4.7 kΩ) = ×0.0449, −27.0 dB; 4.5 kΩ source | — |

The mix-bus and send figures are the two that can be checked against the factory: 4.5 kΩ derived
against 4.7 kΩ stated (S2), and 420 Ω derived against ~600 Ω stated (S3).

**Cascade gain, and why it is a warning rather than a target.** With the gain pot at maximum, the
midband product V1a × pot × V2a × V3a is roughly 56.8 × 1 × 67.2 × 71.4 ≈ 2.7 × 10⁵, or 108.7 dB,
before the stack's insertion loss and before the −23.6 dB mix divider. Nothing survives that
linearly; every stage after the first is in grid conduction on any real input. Do not test against
a small-signal cascade gain figure. Test against the operating points, the corner frequencies and
the two impedance numbers above, and let the nonlinearity fall out of the stages.

### Not available at any price

**No bench measurement of a VH4 has been published.** No frequency-response plot, no THD figure, no
input-sensitivity measurement, no oscilloscope capture of any node. I searched the general web,
Rig-Talk, The Gear Page, freestompboxes, Amp Garage, music-electronics-forum and ampgarage. The
closest anyone comes is Diezel's own statement of the Presence and Deep centre frequencies, and the
two impedance numbers above.

---

## 7. Proposed `PowerTubePreset`

**Which tube this preset is for, first.** The spec chooses the KT77 for this module
(`docs/superpowers/specs/2026-09-06-high-gain-amps-design.md` §2). Nothing in §1 or §5 puts a KT77
in a VH4, and Diezel's own documents name no power-tube type at all; the evidence that exists points
to the EL34 (§5, and the verdict paragraph). The preset below is therefore **the KT77 in a VH4's
operating conditions** — the spec's tube, the amplifier's supply voltage and idle current — and not
a reading of a stock amplifier. The KT77 is a beam tetrode sold as a drop-in EL34 replacement, so
the substitution is physically sound; it is a voicing decision the maintainer made, and it should be
revisited before part 2 is planned.

The reference point, from `include/SushiDSP/circuits/stages/PowerTubePresets.hpp` as the spec
defines it:

```
kEl34{ 10000, 220000, -20, 8, 1e-9, 0.02585, 0.05 }
```

Field order is `source_resistance_ohms, grid_leak_resistance_ohms, cathode_bias_volts, gain,
saturation_current_amps, thermal_voltage_volts, plate_saturation_drive`
(`include/SushiDSP/circuits/stages/PentodeStage.hpp`).

**Proposal:**

```
kKt77{ 10000, 220000, -43, 8, 1e-9, 0.02585, 0.045 }
```

The two operating points the argument stands on, both read off datasheet tables:

| Tube | Source | Supply | Anode | Screen | Idle current | Grid bias | Load a-a | Output |
|---|---|---|---|---|---|---|---|---|
| KT77, UL 43 %, AB1, fixed bias | S12 page 3 | Vb 500 V | 493 V | 493 V | 2 × 57 mA | **−43 V** (±25 % adjustment range, per the sheet's own footnote) | 5.5 kΩ | 67 W |
| EL34, push-pull, fixed bias | S13 page 3 | Vba 500 V | 495 V | 400 V | 2 × 30 mA | **−36 V** | 4 kΩ | 70 W |

The VH4 sits between them: 500 V on the OT primary (S7) and 35 mA per tube (S3), with its screen
supply unknown.

Field by field.

**`source_resistance_ohms = 10000`, unchanged.** The impedance the phase inverter presents to the
power tube's grid, which sets how hard the grid-conduction diode loads the driver past cutoff. The
VH4's phase inverter is a 12AX7 in the V7 position (S3, S11) and nothing else about it is known —
not its plate resistors, not its tail, not the grid stoppers on the power tubes. 10 kΩ is the
JCM800's value for the same tube in the same role. **A modelling choice inherited from `kEl34`.**

**`grid_leak_resistance_ohms = 220000`, unchanged, and now sourced.** S12 page 1 gives the KT77's
grid-cathode resistor limit for fixed bias as **0.5 MΩ** when Pa+g2 ≤ 28 W, halving to 0.25 MΩ above
it. At 500 V × 35 mA the VH4 idles at 17.5 W, so the 0.5 MΩ limit applies and 220 kΩ sits well
inside it. **Consistent with the tube's rating and the confirmed fixed-bias topology; not read from
a VH4.**

**`cathode_bias_volts = -43`.** S12's own Vb = 500 V fixed-bias column is the closest published
KT77 operating point to the VH4's supply, and it gives −43 V. Three caveats, stated rather than
smoothed:

- That row idles at 57 mA per tube where Diezel specifies 35 mA (S3). A colder idle at the same
  anode voltage needs a more negative grid, so the true figure is on the more-negative side of −43 V.
- That row runs the screen at 493 V, ultra-linear. The VH4's screen supply is not established, and
  a lower screen pulls the bias back toward zero. The two errors work against each other.
- S12's footnote — "A bias adjustment range of ±25% about these values should be available for each
  valve" — puts the working window at −32 V to −54 V, which is the bracket the real value sits in.

The EL34 cross-check (S13) lands at −36 V for 500 V anode, 400 V screen and 30 mA, so a VH4 built
with its stock tube would want roughly −36 V rather than −43 V. **That is the field that moves most
if the maintainer revisits the KT77 choice.** Dissipation check: 500 V × 35 mA = **17.5 W**, which
is 70 % of the KT77's 25 W design-maximum anode dissipation and 55 % of its 32 W absolute maximum
(S12 page 1), and 70 % of the EL34's 25 W (S13). Olaf's "the colder you set the tubes, the less
often you need to check the bias" (S7) is the design intent this number encodes.

**`gain = 8`, unchanged.** The stage's linear grid-to-plate gain before the tanh. The KT77's
published tetrode-connection figures are gm 10.5 mA/V, ra 23 kΩ, mu 11.5 at 250 V and 110 mA
(S12 page 2); the repo's Koren EL34 carries mu 11.0 (`docs/reference/spice/models/el34_koren.sub`).
A few percent apart is not a reason to move the constant. What would actually set this field is the
output transformer's primary impedance, which §5 records as not established. **Left at the reference
value deliberately. Its acceptance test is power-section output level at clipping.**

**`saturation_current_amps = 1e-9`, unchanged.** The grid-conduction diode's saturation current. No
KT77-specific figure exists in S12 or anywhere else I found, so this is `kEl34`'s number carried
over. **A modelling choice inherited from `kEl34`, not a measurement.**

**`thermal_voltage_volts = 0.02585`, unchanged.** kT/q at 300 K. Physics, not a tube parameter.

**`plate_saturation_drive = 0.045`.** The plate output ceiling is 1 / drive, so 0.05 gives 20 and
0.045 gives 22. The argument is the supply ratio alone: the VH4 runs 500 V (S7) where the JCM800
model's EL34 pair runs nearer 450 V, and 500/450 = 1.111, so 0.05 / 1.111 = 0.045. **Derived from
one ratio, with no VH4 or KT77 measurement behind it.**

**Summary of standing.**

| Field | Standing |
|---|---|
| `thermal_voltage_volts` | Physics. kT/q at 300 K. |
| `cathode_bias_volts` | Datasheet row (S12, Vb 500 V) plus Diezel's stated supply and idle current (S3, S7), with the bracket above. The best-sourced field in the preset, and still an interpolation. |
| `grid_leak_resistance_ohms` | Inside the tube's published fixed-bias limit (S12) given the confirmed fixed-bias topology. Not read from a VH4. |
| `plate_saturation_drive` | Derived from the 500 V / 450 V supply ratio. |
| `source_resistance_ohms` | Modelling choice inherited from `kEl34`; the VH4's phase inverter is undocumented. |
| `gain` | Modelling choice inherited from `kEl34`; the OT primary impedance that would set it is not established. |
| `saturation_current_amps` | Modelling choice inherited from `kEl34`; no KT77 figure exists. |

Four of the seven are choices or interpolations rather than facts about this amplifier. The
module's README must say so, and must say which tube the preset is for.

## 8. Open questions

### Blocking — these change what the module is

1. **Is the tone stack's interconnection the standard FMV one?** §4. The five values are legible;
   the wiring as drawn shorts the treble pot. A photograph of a VH4 main board around the channel-3
   pots settles it in one minute, and so does continuity-checking the three pots on a real amp.
2. **What is the real preamp supply voltage, and is the rail decoupled per stage?** §3. S1 draws
   367 V undecoupled on all eight triodes; S4 says 385–400 V and every real amp decouples. Every
   operating point and every gain in §3 and §6 moves with this. A DMM on the plates of V1, V3 and
   V4 settles it.
3. **What is in the network between the cathode follower and the tone stacks?** §3.6. S1's
   10 µF + 470 kΩ does nothing; S4 says a real one is around 2.2 nF + 47–100 kΩ, which is a treble
   shunt and audible. These are not the same circuit.
4. **What is the power amp?** §5. Phase-inverter topology and values, screen resistors, OT primary
   impedance and turns ratio, and the whole feedback network including the Deep inductor. The
   attachment at S5 would settle most of it; it needs a music-electronics-forum account with
   attachment permissions.

### Structural, resolvable from a board or a bench

5. **Is R22 3.3 MΩ, 1 MΩ or 220 kΩ?** `bmxguitarsbmx` has "only seen 1M or 220k" on real boards
   (S4). It sets V2a's grid-leak loading and therefore how the stage recovers from blocking
   distortion, which at this gain is audible.
6. **What tapers do the channel-3 Gain, tone and Volume pots use?** S1 marks none. The goya1746
   pedal adaptation (S14) uses A250K gain, B250K treble, A1M bass, B25K mid, A10K volume, but it is
   a 12–18 V op-amp circuit and its pot choices are the builder's.
7. **Which VH4 generation is being modelled?** Diezel documents at least two preamp layouts (S3):
   the current one, and a "very early VH4" whose V2 is the clean channel and whose V3 is channels
   3 and 4. Rig-Talk owners describe a script change in late 1996 or early 1997 and further
   changes around 2007. S1 is undated as to which board it traces. The channel-3 chain is the same
   shape in both layouts, so this affects the tone stack and the power amp more than the gain
   stages.
8. **Does the amp's Deep control really use an inductor?** §5, `[single source, S6]`. It decides
   whether `ResonanceControl` needs a resonant form or a first-order shelf.
9. **Is Deep centred at 80 Hz or 90 Hz?** Diezel's own two documents disagree (S2 vs S8).
10. **What is the VH4's screen-grid supply voltage?** §7's `cathode_bias_volts` is bracketed rather
    than pinned because of this. S12's 500 V KT77 row runs the screen at 493 V ultra-linear and
    S13's EL34 row at 400 V, and the bias figures differ by 7 V across that gap. One DMM reading on
    a power tube's pin 4 settles it. It would also say whether the output stage is ultra-linear or
    a plain tetrode connection, which S12 warns matters: "For tetrode connection the fixed screen
    supply must not exceed 300 V."

11. **What power tube does a stock VH4 actually ship with, and does Diezel state it anywhere?**
    §5 and the verdict. Every source that names EL34 is a dealer listing or a model name; every
    factory document is silent. The answer decides whether §7's preset should be a KT77 at all.
12. **What is the input sensitivity?** Nobody has published one. Diezel only says a 12 o'clock gain
    setting is the starting point and that active pickups overdrive the channel (S2).

### Scope

13. **The mix bus is a four-way summing node, and the model will have one channel.** §2. The
    −23.6 dB divider at V4a's grid depends on all four legs existing and on the SPDT relays
    grounding the unselected three. A single-channel module must hard-code the 48.1 kΩ shunt to
    keep V4a's drive level right, and say why.
14. **Channels 1, 2 and 4 are out of scope but share silicon with channel 3.** V1a, V3a, V3b, V4a
    and V4b are shared; V2a is shared with channel 4. If channels 2 and 4 are ever added, they are
    a different input network and a different tone stack in front of the same chain, not a new
    module.
15. **No SPICE deck is committed for this amplifier.** The jcm_fmv precedent allows an unsimulated
    netlist with its caveats in the README, but the one block worth a deck here is the tone stack,
    and committing a netlist for a topology that §4 says is reconstructed would harden a guess into
    a file. Write the deck once question 1 is answered.
