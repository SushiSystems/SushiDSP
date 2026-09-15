# Peavey 5150 (block letter, 1992), lead channel — design-ready dossier

Compiled 2026-09-06 for phase DA1 of the high-gain-amplifier work
(`docs/superpowers/specs/2026-09-06-high-gain-amps-design.md`). Everything the later module may
use has to be in this file; a value that is not here does not exist for the implementer.

Confidence key, the same one `2026-08-26-boss-dd3-design-dossier.md` uses. **A** = read directly
off a schematic sheet at magnification, or quoted from a Peavey document. **B** = derived by me
from an A value, with the derivation shown. **C** = a single secondary source I could not
corroborate, or a reading the scan left ambiguous. Unmarked is A.

Method note, because it bounds §3 and §4. The schematic below is a raster scan; the vector text
layer is empty (`pdftotext` returns nothing), so every value was read by extracting the embedded
page images with `pypdf` and reading them at 3–4× magnification. Component references, resistor
values above 1 kΩ and every capacitor are unambiguous at that magnification. Three cathode
resistors and about six of the small labels sit at the edge of legibility; each is flagged where
it appears. Node connectivity was traced pixel by pixel and is stated as traced except where the
text says otherwise.

---

## 1. Sources

| # | Document | Origin | Revision / date | How far it was trusted |
|---|---|---|---|---|
| S1 | `Peavey-EVH5150-Schematic.pdf`, five sheets titled "EVH 5150 — Initial Preamp Stage / Intermediate Preamp Stages / Tone Stack & PI / Output Section" plus one full-amplifier sheet | <https://www.thetubestore.com/lib/thetubestore/schematics/Peavey/Peavey-EVH5150-Schematic.pdf>, accessed 2026-09-06 | No revision block or date on the sheets. The four titled sheets are a redraw of the fifth, and the fifth is a scan of a Peavey factory drawing. Matches the "Peavey 5150 EVH 120 2/92" schematic set listed elsewhere, which dates it to the block-letter production run. | The whole of §§2–5 comes from here. Every component reference, value and connection below was read off it directly. |
| S2 | Peavey *6505 1992 Original* Operating Guide, specification and block-diagram page | <https://assets.peavey.com/literature/manuals/119251_38557.pdf>, accessed 2026-09-06 | Peavey's own literature for the 2021 reissue of the block-letter amplifier, which Peavey sells as an unchanged reissue | Quoted verbatim in §6. It is the only manufacturer-published performance data I found. It describes the reissue, so a 1992 unit is assumed rather than proven to meet it. |
| S3 | Peavey 5150 II Operating Guide | <https://assets.peavey.com/literature/manuals/80304643.pdf>, accessed 2026-09-06 | 5150 II, later model | Read only to confirm that the II's published specification differs from the block letter's (its hum and noise figure is 63 dB below rated power against the original's 74 dB). Nothing from it is used as a 5150 value. |
| S4 | Judybox Revival, "Biasing a Peavey 5150" | <http://judyboxamp.blogspot.com/2013/10/biasing-peavey-5150.html>, accessed 2026-09-06 | Blog post, October 2013 | One measured number: "At 481 v on the plates I set it to about 34 ma on the hottest tube." Marked `[single source]` in §5. |
| S5 | Peavey support answers relayed on forums.peavey.com and ultimatemetal.com, found through web search on 2026-09-06 | <https://forums.peavey.com/viewtopic.php?t=38145>, <https://ultimatemetal.com/threads/peavey-5150-bias-values.701601/> | undated forum threads | Grid bias −55 V, idle 30 mA per tube in a 25–35 mA band. Second-hand relay of a manufacturer answer, not a document. **Both URLs are bot-gated as of 2026-09-06:** the Peavey forum returns HTTP 403 behind a Cloudflare challenge and the Ultimate Metal thread returns a 3893-byte browser check, so neither serves the figure to a reader who follows the citation. §7's `cathode_bias_volts` rests on them. Marked `[single source]` in §5 and §7. |
| S6 | `docs/reference/spice/models/12ax7_koren.sub` in this repository | — | — | The 12AX7 model used for the operating-point arithmetic in §3, so that the dossier's DC points and the module's triodes come from the same physics. |
| S7 | RCA 6L6-GC beam power tube data sheet, RCA Electron Tube Division, Harrison N.J. | <https://frank.pocnet.net/sheets/049/6/6L6GC.pdf>, accessed 2026-09-06 | Ten pages, footers "DATA 1" onward with date code 8-60 | The tube datasheet §7 argues from. Raster scan, no text layer; read at 0.7×–1.3× the same way S1 was. Every 6L6GC figure quoted in §5 and §7 is off this document. |
| S8 | Philips/Mullard EL34 A.F. output pentode data sheet | <https://frank.pocnet.net/sheets/010/e/EL34.pdf>, accessed 2026-09-06 | Dated January 1969 on page 1 | Read only for the EL34's transconductance, so that §7's comparison against `kEl34` has a source on both sides. |

Sources I looked for and could not read: the Peavey factory power-supply sheet. The five sheets
of S1 stop at the output transformer and carry no rectifier, filter chain, dropping resistors or
bias supply, and no marked node voltages. Elektrotanya, schematicsunlimited.com and
audioservicemanuals.com each hold a longer 5150 set, and each gates the download behind a CAPTCHA
or returns HTTP 403 to a programmatic fetch. This is the single largest gap in the dossier and it
is what §8 opens with.

No SPICE deck is committed with this dossier. The tone stack's topology is now closed off the scan
(§4) and a `p5150_fmv` netlist is worth writing, but every deck's usefulness turns on the rail
voltages, which are the gap above.

---

## 2. Block overview

Lead channel, footswitch in, relays K1–K4 energised as noted. Tube designators are S1's. The
sheet numbers five 12AX7 envelopes V1–V5 and the four 6L6GC envelopes V5–V8, so "V5" names two
different tubes; V5A/V5B below always mean the 12AX7.

```
J37 HIGH ─ R27 22K ─┐
J39 LOW  ─ R30 22K ─┴─ C15 .1µ/250 ─┬─ R25 1M ─ gnd
                                    └─ R22 68K ─ V1A grid   (C20 39p, jack node to gnd)
                                       │
   V1  ─ R2 220K ── V1A plate          │  V1A cathode: R12 1.02K 1% ∥ C3 1µ/50 NP
                    │
                    C6 .022/400
                    │
      node P ───────┴──────────────────────────────────────────────────────┐
        │                                                                  │
        │  RHYTHM branch (crunch/bright, K4A/K4B, S1C):                    │  LEAD branch:
        │  C43 .1/16 ─ R23 39K ─ K4B; R19 470K ∥ C51 270p; R43 2.2K +      │  R29 470K ∥ C17 .0022
        │  C13 470p; R21 1M; R78 2.2M; C10 470p; C27 180p                  │      │
        │      └── VR1 1M A  RHYTHM (CLEAN) PRE ── K2A pin 3               │  node Q ┬ C19 .001 ─ gnd
        │                                                                  │         ├ R40 1M ─ gnd
        └──────────────────────────────────────────────────────────────────┘         └ VR2 1M A LEAD
                                                                                       (ULTRA) PRE,
                                     K2A pin 2 (common) ◄── pin 4, 12 V relay ◄────────  R41 1M across
                                            │
   V1  ─ R3 100K ── V1B plate ◄─ R82 470K grid leak ◄─ K2A common
                    │              V1B cathode: R17 1.02K 1% ∥ C4 1µ/50 NP
                    │                           ∥ (C26 22µ/25 ─ R18 47K ─ gnd), R18's
                    │                             low end also to K1A pin 3
                    C2 .022/400
                    │
                    R6 470K ─┬─ R9 1M ─ gnd            interstage divider, 0.680
                             │
   V1  ─ R1 100K ── V2A plate ┘   V2A cathode: R15 39K, UNBYPASSED  (the cold clipper)
                    │
                    C1 .022/400
                    │
                    ├─ C48 .001 ─ gnd      ├─ R11 330K ─ gnd
                    ├─ R20 220K ─ K1B pin 6; pin 7 is gnd, pin 5 marked N/C
                    └─ R7 220K ── V2B grid
   V2  ─ R4 220K ── V2B plate            V2B cathode: R13 1.02K 1% ∥ C5 1µ/50 NP
                    │
                    R32 1M ─ C57 .022/400 ─┬─ R101 100K ─ gnd
                                           └── V5B grid
   V2  ─ R96 220K ─ V5B plate             V5B cathode: R102 2.2K, UNBYPASSED
                    │
                    R87 1M ─ C56 .022/400 ─ R88 1M ─┬─ R91 1M ─ gnd
                                                    └── V5A grid
   V2  ─ R86 100K ─ V5A plate             V5A cathode: R93 2.2K, UNBYPASSED
                    │
                    C58 .022/400
                    │
              ┌─────┴─────────────────────────────────────────────┐
              │  TONE STACK: R28 47K slope resistor; R24 33K and  │
              │  C59 .01 + R94 47K both to gnd;                   │
              │  C16 470p ∥ C60 100p, C18 .022, C21 .022;         │
              │  VR5 HIGH 250K B, VR3 LOW 1M A (push-pull),       │
              │  VR4 MID 50K B                                    │
              └─────┬─────────────────────────────────────────────┘
                    │
                    ├─ VR7 1M A  LEAD (ULTRA) POST ─┐
                    └─ VR6 1M A  RHYTHM POST ───────┴─ K2B ─┐
                                                            │  POST-GAIN NODE
                                                            ├─ R85 220K ─┬─ gnd
                                                            ├─ R81 2.2M ─┘
                                                            ├─ C52 .022/400 ─ V3B grid [C]
                                                            └─ C11 .047/400 ─ V3A grid

   V2  ── V3B plate direct, no plate resistor: CATHODE FOLLOWER
                    V3B grid: one return, R79 1M to the tap, bootstrapped
                    cathode ─ R80 1K ─ tap ─ R16 100K 1W ─ gnd
                    tap ─ C53 .33/400 ─ K3A ─ EFFECTS SEND   |   PREAMP OUT via C66 .1/250,
                          EFFECTS RETURN ─ K3B ─ R103 100K,  |   R104 100K, R100 1M
                          C65 .33/400, R33 100K

   V2  ─ R5 68K ─── V3A plate            V3A grid: R26 1M grid leak to gnd
                    │                    V3A cathode: R14 3.3K ∥ C61 .0047
                    │
                    C49 .022/400 ─ R48 100K ─ V4B grid
                    │
   V3  ─ R46 82K ── V4B plate ─ C29 .047/400 ─ R55 220K ─ BIAS ─ R206 2.2K ─ V5 g1
   V3  ─ R58 100K ─ V4A plate ─ C28 .047/400 ─ R54 220K ─ BIAS ─ R202 2.2K ─ V6 g1
                    LTP: cathodes ─ R52 470 ─ node X ─ R51 10K ─ node N ─ R57 4.7K ─ gnd
                         R50 1M node X→V4B grid,  R53 1M node X→V4A grid
                         V4A grid also ─ C31 .047/400 ─ node N   (AC ground)
                         node N ◄─ C30 22µ/25 ◄─ presence/resonance network ◄─ R56 33K ◄─ OT 8 Ω

   4 × 6L6GC V5–V8, cathodes to ground, fixed bias.
   Grid stoppers R206/R202/R208/R204 2.2K; C202/C201 100p grid to ground.
   Screen resistors R207/R203/R209/R205 100 Ω 5 W, all fed from SCREEN.
   PLATE ─ S1 standby ─ R210 400 Ω 10 W ─ SCREEN.
   T2 Peavey 70500207, primary BR/R/BL with R the centre tap, secondary W(8Ω)/GN/BK.
   FEEDBACK tapped at W. J1 8 Ω part 71466001, J2 4 Ω, selector TRANSFER 71466024.
```

---

## 3. Preamp, stage by stage

Five gain stages sit between the input jack and the tone stack: V1A, V1B, V2A, V2B, V5B, V5A —
six triodes, of which V2A is a starved unbypassed stage rather than a gain stage in the usual
sense. After the stack, V3B (cathode follower) and V3A (driver) hang off the post-gain node in
parallel rather than in series. All are 12AX7.

Supply nodes as drawn: V1 feeds V1A, V1B and V2A; V2 feeds V2B, V5B, V5A, V3B and V3A; V3 feeds
the phase inverter. **No node voltage is printed anywhere on S1 and the power-supply sheet is
missing, so V1, V2 and V3 are not established.** Every DC figure in this section is therefore
given as a function of the rail rather than as a single number.

### The DC cross-check

The brief asks that each stage's operating point be closed by hand and that a stage that does not
close be flagged. I did it numerically instead of graphically, with the Koren 12AX7 of
`docs/reference/spice/models/12ax7_koren.sub` (MU 100, KP 600, KVB 300, KG1 1060, KX 1.4), solving
`Ia = Ip(B − Ia·Ra − Ia·Rk, −Ia·Rk)` by bisection. Using the repository's own model means the
dossier's DC points and the module's triodes will agree by construction. Confidence **B**
throughout: the resistors are A, the model and the arithmetic are mine.

| Stage | Ra | Rk | B+ = 300 V | B+ = 340 V | B+ = 380 V |
|---|---|---|---|---|---|
| V1A | 220 k | 1.02 k | 0.77 mA, Va 130 V, bias −0.79 V | 0.88 mA, Va 146 V, bias −0.90 V | 0.99 mA, Va 162 V, bias −1.01 V |
| V1B | 100 k | 1.02 k | 1.15 mA, Va 185 V, bias −1.17 V | 1.32 mA, Va 209 V, bias −1.34 V | 1.48 mA, Va 232 V, bias −1.51 V |
| V2A | 100 k | 39 k | 0.090 mA, Va 291 V, bias −3.51 V | 0.102 mA, Va 330 V, bias −4.00 V | 0.115 mA, Va 369 V, bias −4.49 V |
| V2B | 220 k | 1.02 k | 0.77 mA, Va 130 V, bias −0.79 V | 0.88 mA, Va 146 V, bias −0.90 V | 0.99 mA, Va 162 V, bias −1.01 V |
| V5B | 220 k | 2.2 k | 0.60 mA, Va 169 V, bias −1.31 V | 0.68 mA, Va 191 V, bias −1.49 V | 0.76 mA, Va 212 V, bias −1.68 V |
| V5A | 100 k | 2.2 k | 0.80 mA, Va 221 V, bias −1.75 V | 0.91 mA, Va 249 V, bias −2.00 V | 1.02 mA, Va 278 V, bias −2.25 V |
| V3A | 68 k | 3.3 k | 0.67 mA, Va 254 V, bias −2.21 V | 0.77 mA, Va 288 V, bias −2.53 V | 0.86 mA, Va 322 V, bias −2.84 V |

Every stage closes. Two results are worth reading before writing any code.

V1A and V2B run a 220 kΩ plate against a 1.02 kΩ cathode, which puts the plate at 43 % of the
rail and biases the grid at under a volt. That is a deliberately asymmetric, low-headroom stage:
it reaches grid conduction on the positive half-cycle long before it reaches cutoff on the
negative one, and its plate cannot swing far upward. It must not be modelled as a generic
100 k / 1.5 k Marshall stage.

V2A is the opposite. A 39 kΩ unbypassed cathode starves the stage to 0.102 mA and puts the plate
within 15 V of the rail. This is the cold clipper. Its job is to survive the signal V1B hands it
and to clip asymmetrically rather than to amplify.

### Small-signal gain per stage

The gain figures below are differentiated out of the same Koren model at the same B+ = 340 V
operating points, so they are confidence **B** on the same footing as the DC table. `gm` and `ra`
are the numerical partial derivatives of `Ia` at each stage's solved point, `µ = gm·ra`, and the
gain is `µ·Ra/(ra + Ra)` for a bypassed cathode and `µ·Ra/(ra + Ra + (µ+1)·Rk)` for an unbypassed
one.

| Stage | gm (mA/V) | ra (kΩ) | µ | Cathode | Gain | dB |
|---|---|---|---|---|---|---|
| V1A | 1.919 | 49.4 | 94.8 | bypassed | 77.4 | +37.8 |
| V1B | 2.122 | 44.4 | 94.1 | bypassed | 65.2 | +36.3 |
| V2A | 0.236 | 300.7 | 70.9 | 39 k unbypassed | 2.21 | +6.9 |
| V2B | 1.919 | 49.4 | 94.8 | bypassed | 77.4 | +37.8 |
| V5B | 1.538 | 58.6 | 90.1 | 2.2 k unbypassed | 41.4 | +32.3 |
| V5A | 1.630 | 54.8 | 89.4 | 2.2 k unbypassed | 25.3 | +28.1 |
| V3A | 1.362 | 63.5 | 86.4 | 3.3 k unbypassed | 14.0 | +22.9 |

The `1 + gm·Rk` shorthand does not reproduce these and must not be used on this amplifier. It
assumes an ideal current source at the plate, and every stage here works into a plate resistor
comparable with or smaller than `ra`. V2A shows the size of the error: `1 + gm·Rk` is
`1 + 0.236 mA/V × 39 kΩ = 10.2`, which off `µ = 70.9` would suggest 7.0, and the full expression
gives `70.9 × 100k / (300.7k + 100k + 71.9 × 39k) = 7090/3204 = 2.21`. The honest way to read the
degeneration is as the ratio between the same stage bypassed and unbypassed: 8.0× at V2A, 1.7× at
V5B, 2.3× at V5A, 3.2× at V3A.

The three stages after V2A — V2B (220 k / 1.02 k), V5B (220 k / 2.2 k), V5A (100 k / 2.2 k) — run
progressively less starved and progressively more headroom, so the distortion is spread across the
chain instead of concentrated at one stage.

### Stage detail

**Input network.** J37 is the high-gain jack, J39 the low-gain jack, each through 22 kΩ (R27,
R30) into the same node; the manual calls J39 a −6 dB pad (S2). C20 39 pF from that node to
ground. C15 0.1 µF/250 V couples to the grid node, R25 1 MΩ is the grid leak to ground, R22 68 kΩ
is the grid stopper. S2 publishes the high-gain input impedance as 470 kΩ, which does not follow
from a 1 MΩ grid leak; the discrepancy is in §8.

**V1A.** Plate R2 220 kΩ from V1. Cathode R12 1.02 kΩ 1 %, bypassed by C3 1 µF 50 V non-polar.
The bypass corner is `1/(2π·1e-6·(1.02k ∥ 1/gm))`; with gm = 1.919 mA/V at 0.882 mA the parallel
combination is 345 Ω, giving 462 Hz (**B**). Below that corner the stage degenerates from a gain
of 77.4 toward 56.8, a 2.7 dB shelf whose corner sits above the open low E. Coupling out on
C6 0.022 µF/400 V.

**Lead pre-gain network.** From V1A's coupling node, R29 470 kΩ in parallel with C17 2.2 nF runs
in series into the pot node; at that node C19 1 nF and R40 1 MΩ go to ground, and VR2 (1 MΩ audio,
Peavey POT0752, panel legend LEAD PRE GAIN, schematic legend ULTRA PRE) sits from the node to
ground with R41 1 MΩ across it. The lead branch hangs directly on V1A's coupling node and does
not pass through the crunch relay K4, so CRUNCH and BRIGHT affect only the rhythm channel, which
matches S2's front-panel description.

That series network is the most consequential single thing in the preamp. The shunt at the pot
node is R40 ∥ R41 ∥ VR2 = 333 kΩ, so at DC the divider passes 333/(470 + 333) = 0.415, and above
`1/(2π·2.2n·(470k ∥ 333k))` = 371 Hz the 2.2 nF shorts the 470 kΩ and the divider approaches
unity (**B**). It is a 7.6 dB treble shelf with its corner right in the guitar's fundamental
range, applied before every stage that clips. Model it as a first-order shelving filter, not as a
flat attenuator.

**V1B.** Grid fed from K2A's common through R82 470 kΩ, which is the grid leak, not a stopper.
Plate R3 100 kΩ from V1. Cathode R17 1.02 kΩ 1 % bypassed by C4 1 µF 50 V non-polar. Coupling out
on C2 0.022 µF/400 V into R6 470 kΩ, then R9 1 MΩ to ground: an interstage divider of
1M/(1M + 470k) = 0.680, or −3.34 dB (**B**).

V1B carries a second, relay-switched cathode bypass that the rest of this section's stages do not.
From the R17/C4 node, C26 22 µF 25 V electrolytic (positive terminal to the cathode) runs down to
a node carrying R18 47 kΩ to ground, and that same node goes to K1A pin 3. K1A's pin 2 is ground
and pin 4 is marked N/C; K1B is the other half of the same relay and switches R20 220 kΩ at V2A's
output node the same way, pin 6 common, pin 7 ground, pin 5 marked N/C. Both armatures rest on the grounded
contacts as drawn, so in the position S1 draws, R18 and R20 are both grounded and moving the
armature lifts both. **S1 does not show K1's coil drive**, so which state the lead channel selects
is not established off these five sheets; K2A, whose lead position is established, is energised
for lead, and if K1 shares that coil the lead channel runs with R18 and R20 lifted. Confidence
**C** on that last step. Whether the drawn position is the de-energised one turns on how Peavey
meant `N/C` on pins 4 and 5; open question 14 carries both.

With R18 grounded the cathode sees 47 kΩ in parallel above C26's corner, `1/(2π·22µ·47k)` =
0.15 Hz, which is below the audio band by three decades, so C26 is a short everywhere it matters
and the branch reduces to R18 across R17: 1.02 k ∥ 47 k = 998 Ω, 2.2 % lower. The stage's
unbypassed-region gain moves from 39.0 to 39.3, under 0.1 dB (**B**). It is recorded because the
brief asks for each triode's bypass network and because it identifies K1's second contact, not
because it is audible.

**V2A.** Plate R1 100 kΩ from V1. Cathode R15 39 kΩ, no bypass capacitor. Coupling out on
C1 0.022 µF/400 V. The output node carries C48 1 nF to ground and R11 330 kΩ to ground, and R20
220 kΩ to relay K1B, grounded in the position S1 draws. R7 220 kΩ is V2B's grid stopper. With R20
grounded the node's shunt is 330 k ∥ 220 k = 132 kΩ instead of 330 kΩ.

The C48 / R11 pair sets a pole at `1/(2π·1n·330k)` = 482 Hz (**B**) once the source impedance is
taken as small, which it is not; with the 220 kΩ stopper and V2A's plate impedance the real corner
is lower. This is the second of the amplifier's two large fixed treble-shaping networks, and it
runs the opposite way to the one at the lead pre gain.

**V2B.** Plate R4 220 kΩ from V2. Cathode R13 1.02 kΩ 1 % bypassed by C5 1 µF 50 V non-polar.
R32 1 MΩ then C57 0.022 µF/400 V out, with R101 100 kΩ to ground at V5B's grid. R32 in series with
the 100 kΩ shunt is a divider of 100/(1000 + 100) = 0.0909, −20.8 dB (**B**), the largest single
interstage attenuation in the amplifier.

**V5B.** Plate R96 220 kΩ from V2. Cathode R102 2.2 kΩ, no bypass capacitor. R87 1 MΩ then
C56 0.022 µF/400 V, then R88 1 MΩ with R91 1 MΩ to ground at V5A's grid: another 0.5 divider,
−6 dB (**B**), preceded by a 1 MΩ series element.

**V5A.** Plate R86 100 kΩ from V2. Cathode R93 2.2 kΩ, no bypass capacitor. R89 470 kΩ from the
plate side down. Coupling out on C58 0.022 µF/400 V into the tone stack.

Both V5B and V5A run 2.2 kΩ unbypassed. The degeneration costs V5B a factor of 1.7 (71.2 bypassed
against 41.4 as drawn) and V5A a factor of 2.3 (57.7 against 25.3), so the pair contributes 60.4 dB
where fully bypassed cathodes would give 72.3 dB (**B**, from the small-signal table above).

**The post-gain node.** Both post-gain pots run from the tone stack's HIGH wiper node into K2B,
whose common lands on a node carrying R85 220 kΩ and R81 2.2 MΩ, both to ground, so the node's
shunt is 220 k ∥ 2.2 M = 200 kΩ. Two coupling capacitors leave it: C11 0.047 µF/400 V to V3A's
grid, and C52 0.022 µF/400 V to V3B's grid. The two triodes of V3 hang off the same node in
parallel; the follower is not in series ahead of the driver.

**V3B, cathode follower.** Plate connected straight to V2, no plate resistor. Grid from the
post-gain node through C52 0.022 µF/400 V. Cathode into R80 1 kΩ, then R16 100 kΩ 1 W to ground;
the output is taken at the R80/R16 junction, so the follower drives the effects loop and the
preamp-out jack from a low impedance while the 1 kΩ isolates it.

This grid has one return and it is not to ground. R79 1 MΩ runs from the grid down to the R80/R16
junction, traced at 3.2× on S1 page 2: the follower's own output bootstraps it, so at audio it
presents roughly `1M/(1 − 0.98)` ≈ 50 MΩ and the grid's load is set by C52 and whatever the DC
path through R79 leaves, not by a 1 MΩ leak to ground. Model V3B as a bootstrapped follower or
the input impedance it presents to the post-gain node will be an order of magnitude too low.

**V3A, driver.** Plate R5 68 kΩ from V2. Grid from the post-gain node through C11 0.047 µF/400 V,
with R26 1 MΩ from the grid to ground as the grid leak. R26's far end is the grounded tone-stack
node that R24 33 kΩ and the C59/R94 leg also land on, read at 7× on page 2; its near end climbs
from that horizontal to y ≈ 1295, steps right to x ≈ 851, climbs to y ≈ 628 and runs into V3A's
grid, which is a separate horizontal from V3B's — the two grid lines were read apart at 2× on
page 2 and neither carries the other's network. The input high-pass is therefore V3A's:
`1/(2π·0.047µ·1M)` = 3.4 Hz (**B**).

Cathode R14 3.3 kΩ, bypassed by C61 4.7 nF only. That capacitor is the interesting part:
`1/(2π·4.7n·(3.3k ∥ 1/gm))` with gm = 1.36 mA/V gives 3.3k ∥ 734 = 601 Ω and a corner near 56 kHz
(**B**), far above the audio band, so C61 is not an audio bypass at all. The figure moves about
2 % with the derivative step used to get gm, so two figures is all it carries. Below the corner
the stage is fully degenerated by 3.3 kΩ and runs a gain of 14.0. Treat R14 as unbypassed in the
audio band and treat C61 as an oscillation stopper.

---

## 4. Tone network

One passive stack, shared by both channels, sitting between V5A's plate and the two post-gain
pots. S2 calls it "Custom Low, Mid & High passive type EQ".

| Ref | Value | Role as drawn |
|---|---|---|
| VR5 | 250 kΩ linear (B) | HIGH |
| VR3 | 1 MΩ audio (A), push-pull switched | LOW; the switch is the panel's BRIGHT, rhythm channel only per S2 |
| VR4 | 50 kΩ linear (B) | MID |
| R28 | 47 kΩ | slope resistor: from V5A's coupling node to the slope node |
| R24 | 33 kΩ | from V5A's coupling node to ground |
| C16 ∥ C60 | 470 pF ∥ 100 pF = 570 pF | treble capacitor, V5A's coupling node into VR5's top terminal |
| C18 | 0.022 µF | bass capacitor, slope node into VR3 |
| C21 | 0.022 µF | mid capacitor, slope node into VR4 |
| C59 + R94 | 0.01 µF in series with 47 kΩ | from V5A's coupling node to ground, on the same node as R24 |
| VR7 | 1 MΩ audio | LEAD (ULTRA) POST GAIN, after the stack |
| VR6 | 1 MΩ audio | RHYTHM (CLEAN) POST GAIN, after the stack |
| R85 | 220 kΩ | post-gain node to ground |
| R81 | 2.2 MΩ | post-gain node to ground, across R85 |
| R83 (tone area) | 47 kΩ | between the two post-gain pots' ground legs |
| C52 | 0.022 µF/400 V | post-gain node into V3B's grid `[C]`, §3 and open question 15 |

**The topology is a Fender/Marshall FMV stack and R28 47 kΩ is the slope resistor.** Read at 3.4×
on S1 page 2 and confirmed against the factory sheet on page 4, the input bus from V5A's coupling
capacitor carries, top to bottom: C59 0.01 µF in series with R94 47 kΩ to ground, R24 33 kΩ to
ground, the C16 ∥ C60 treble pair into VR5's top terminal, and then R28 47 kΩ vertically down to a
second node. That second node — the slope node — carries VR5's bottom terminal, C18 0.022 µF into
VR3, and C21 0.022 µF into VR4. There is no wire bridging R28, and R24 with the C59/R94 leg lands
on a node that carries its own ground stub with nothing else on it; VR5's three terminals are
elsewhere on the sheet and none of them is that node.

That closes the question the earlier revision of this file left open, and it retires the reasoning
it was left open on. The ground symbol beside R24 belongs to R24's node, not to a VR5 terminal.
R26 1 MΩ leaves the same grounded node and is V3A's grid leak (§3), which is what the third
resistor on that horizontal run is doing there. It is not part of the stack.

The push-pull BRIGHT on VR3 is documented by S2 at +6 dB at 2 kHz, rhythm channel only. The
switched element is not identified on S1.

---

## 5. Power section

**Phase inverter.** Long-tailed pair, both halves of a 12AX7 (V4A/V4B).

| Ref | Value | Position |
|---|---|---|
| R46 | 82 kΩ | V4B plate load, from V3 |
| R58 | 100 kΩ | V4A plate load, from V3 |
| R48 | 100 kΩ | grid stopper, driven side (V4B) |
| R52 | 470 Ω | shared cathode stopper, cathodes to the tail node X |
| R51 | 10 kΩ 1 W | node X to node N |
| R57 | 4.7 kΩ | node N to ground |
| R50 | 1 MΩ | node X to V4B's grid |
| R53 | 1 MΩ | node X to V4A's grid |
| C31 | 0.047 µF/400 V | V4A's grid to node N, i.e. the undriven grid's AC ground |
| C49 | 0.022 µF/400 V | V3A's plate to R48 |
| C29, C28 | 0.047 µF/400 V each | V4B and V4A plates to the output-tube grids |

The tail is 470 Ω + 10 kΩ + 4.7 kΩ = 15.17 kΩ to ground, with the feedback injected at node N
between the 10 kΩ and the 4.7 kΩ. The 82 kΩ/100 kΩ plate asymmetry is the conventional
compensation for the driven half's extra loading. Both grid returns go to the tail node, so the
pair self-biases.

**Output stage.** Four 6L6GC (V5–V8 on S1) in push-pull parallel, cathodes grounded, fixed bias.

| Ref | Value | Position |
|---|---|---|
| R55, R54 | 220 kΩ each | grid leaks, returned to the BIAS node |
| R206, R202, R208, R204 | 2.2 kΩ each | grid stoppers |
| C202, C201 | 100 pF each | grid node to ground |
| R207, R203, R209, R205 | 100 Ω 5 W each | screen resistors |
| R210 | 400 Ω 10 W flameproof | PLATE node to SCREEN node, after the standby switch |
| S1 | — | standby, in the plate line |
| CR200, CR201 | SR2873 | across the primary halves, plate-to-rail clamps |
| T2 | Peavey 70500207 | output transformer, primary BR/R/BL with R the centre tap, secondary W/GN/BK |
| J1, J2 | 71466001 (8 Ω), 4 Ω | speaker jacks; the impedance selector is marked TRANSFER 71466024 |

The bias supply is on the missing power-supply sheet. Not established from S1; the numbers below
are the only ones I have.

Idle bias: −55 V on the grids, 30 mA per tube nominal in a 25–35 mA band, per Peavey support as
relayed on the Peavey and Ultimate Metal forums `[single source]` (S5). Plate voltage 481 V
measured by one technician, who then set 34 mA on the hottest tube `[single source]` (S4). A
second forum figure of 465 V appears in the same searches. Take 470 ± 15 V as the working range
and treat any of it as unconfirmed.

At 30 mA and 470 V a single 6L6GC dissipates 14.1 W, which is 47 % of the 30 W plate rating S7
prints. Two of S7's ratings are exceeded as the amplifier is drawn: the 470 V plate sits under the
500 V design maximum but the screen, fed from the plate node through R210, sits near it against a
450 V maximum, and the 220 kΩ grid leaks are more than twice S7's "Grid-No.1-Circuit Resistance,
for fixed-bias operation, 0.1 max. megohm". Both are worth recording because a model that follows
the datasheet's safe area will not reproduce what this circuit does.

**Screen supply.** One 400 Ω 10 W resistor feeds all four screens from the plate node, and each
tube then has its own 100 Ω. At 4 × 6 mA of screen current the common resistor drops about 10 V at
idle and far more under drive, so screen sag is a real part of the amplifier's compression and the
model should carry it as a shared node, not as four independent 100 Ω resistors.

**Negative feedback.** R56 33 kΩ from the transformer's 8 Ω tap (W) into the presence/resonance
network, then C30 22 µF/25 V into node N in the phase-inverter tail.

| Ref | Value | Position |
|---|---|---|
| R56 | 33 kΩ | feedback resistor, from the 8 Ω secondary tap |
| VR8 | 1 MΩ audio, Peavey POT0752 `[C, label at the edge of legibility]` | RESONANCE |
| C12 | 0.0068 µF | across VR8 |
| VR9 | 10 kΩ audio (A/B legend "10KAVB") | PRESENCE |
| C8 | 0.1 µF, rating printed `.1P10` `[C]` | presence branch. The `P` prefix marks 100 V parts elsewhere on the same sheet, so 10 V is unlikely for a cap fed from the 8 Ω tap; §8 carries it |
| C62 | 0.033 µF | presence branch, in parallel with C8's leg |
| C30 | 22 µF/25 V electrolytic | network output into the PI tail node N |

S2 describes both controls as active: "Active Presence: +10 dB 2 kHz. Active Resonance: +10 dB @
cabinet resonant frequency." They are not active circuits; they are frequency-selective reductions
of the global feedback, which raises the closed-loop gain at those frequencies. The +10 dB figure
is the loop's, and it is the number a test should assert.

**Output transformer.** Part 70500207. Primary impedance and turns ratio are **not established**.
No source I read prints either, and S1 marks only the winding colours and the 8 Ω and 4 Ω
secondary taps. S7 brackets it: RCA asks 6000 Ω plate-to-plate for two 6L6GC making 50 W at 450 V,
so four tubes making 120 W sit at or below half of that, 2500–3000 Ω. §7 carries the bracket into
`gain`.

---

## 6. Calibration targets

All from S2, Peavey's own specification table for the 6505 1992 Original, quoted with its own
wording. Measurement conditions as printed: 1 kHz, Low and High EQ at 0, Bright out, Lead and
Rhythm Post at 10, Presence and Resonance at 0 dB, nominal levels with Pre Gains at 5, minimum
levels with Pre Gains at 10.

| Target | Value | Note |
|---|---|---|
| Rated power and load | 120 W RMS into 16, 8 or 4 Ω | |
| Power at clipping | 130 W RMS into 16, 8 or 4 Ω, typically at 5 % THD, 1 kHz, 120 V AC line | The table adds "(Bias must be reduced to measure)" |
| Frequency response | +0, −3 dB, 50 Hz to 20 kHz at 100 W RMS into 8 Ω | Power-amp measurement, not a preamp measurement |
| Hum and noise | greater than 75 dB below rated power | |
| Presence | +10 dB at 2 kHz | |
| Resonance | +10 dB at the cabinet resonant frequency | |
| Power consumption | 400 W, 50/60 Hz, 120 V AC | |
| Preamp high-gain input impedance | "Very high Z, 470K ohms" | Does not follow from R25 1 MΩ; see §8 |
| Preamp low-gain input impedance | "High Z 44K ohms", a −6 dB pad, all levels increased by +6 dB | |
| Lead channel nominal input level | "−80 dBV, −1 mV RMS" | Printed exactly so. −80 dBV is 0.1 mV, so either the dBV figure or the mV figure is a typo in Peavey's table |
| Lead channel minimum input level | "92 dBV. /025 mV RMS" | Printed exactly so; almost certainly −92 dBV / 0.025 mV |
| Effects send | load 47 kΩ or greater, nominal output −10 dBV, 300 mV RMS | |
| Effects return | very high Z, 470 kΩ, designed level −10 dBV, 300 mV RMS | |
| Preamp out | load 47 kΩ or greater, nominal output +10 dBV, 3 V RMS | The one published preamp-chain level: 3 V RMS at V3B's follower output |
| Bright switch | +6 dB at 2 kHz, rhythm channel only | |
| Crunch switch | "Increases gain"; the input table adds "(Subtract 16 dB with Crunch switch in)" for the clean channel | Rhythm only, so it does not constrain the lead model |

The three targets a lead-channel test can assert directly are the preamp-out level of 3 V RMS at
nominal drive, the presence and resonance shelf heights, and the power-amp frequency response.
Preamp gain at a stated knob position and THD at a stated drive are **not published for the lead
channel** and are not established; §8 says what would produce them.

---

## 7. Proposed `PowerTubePreset` for the 6L6GC

`PentodeStage`'s constructor, from `include/SushiDSP/circuits/stages/PentodeStage.hpp`, takes
`source_resistance_ohms, grid_leak_resistance_ohms, cathode_bias_volts, gain,
saturation_current_amps, thermal_voltage_volts, plate_saturation_drive`. The reference point is
`kEl34{ 10000, 220000, -20, 8, 1e-9, 0.02585, 0.05 }`.

Before arguing from it, one thing has to be said about it. The comment block in
`modules/jcm800/src/Jcm800.cpp` states that the JCM800's constants are "physically-motivated
approximations for a 12AX7/EL34 JCM800-style signal path, not measured from a real amplifier".
`kEl34` is therefore a plausible starting point, not a calibrated datum, and two of the seven
fields below are derived from the 5150's own circuit rather than scaled from it.

The tube figures below come from S7, the RCA 6L6-GC sheet, and S8, the Philips EL34 sheet. The
three rows of S7 this section uses are the general-data column (plate 250 V, screen 250 V, grid
−14 V, plate current 72 mA, plate resistance 22 500 Ω, transconductance 6000 µmhos) and the
push-pull class AB₁ fixed-bias pair for two tubes (plate 450 V, screen 350 V, grid −30 V,
zero-signal plate current 95 mA, plate-to-plate load 6000 Ω, 50 W out; and plate 450 V, screen
400 V, grid −37 V, 116 mA, 5600 Ω, 55 W out). S8 gives the EL34's quick-reference transconductance
as 12.5 mA/V at 100 mA anode current.

```
k6L6GC{ 39000, 220000, -55, 6, 1e-9, 0.02585, 0.048 }
```

**`source_resistance_ohms` = 39000.** Derived, not scaled. The grid is driven from the phase
inverter's plate: the plate load in parallel with the triode's own plate resistance, plus the
2.2 kΩ grid stopper. A 12AX7 at the operating point of §3 has ra ≈ 62 kΩ, so the V4B side gives
82 k ∥ 62 k + 2.2 k = 37.6 kΩ and the V4A side 100 k ∥ 62 k + 2.2 k = 40.5 kΩ. 39 kΩ is the pair's
average; the two halves differ by 8 %, which is small enough to ignore and large enough to record.

This is the field that matters most and the one that departs furthest from `kEl34`. With
`grid_leak_resistance_ohms` unchanged at 220 kΩ, raising the source resistance from 10 kΩ to 39 kΩ
raises the voltage the grid-conduction current develops across the source by a factor of 3.9, so
the modelled stage blocks harder when it is overdriven. The circuit reason is a 100 kΩ grid
stopper on the driven PI half feeding a 2.2 kΩ stopper into a 220 kΩ leak, and S7 puts a number on
how far outside the datasheet that leak is: 220 kΩ against RCA's 0.1 MΩ maximum for fixed bias.

**`grid_leak_resistance_ohms` = 220000.** Read off S1: R54 and R55 are 220 kΩ. Identical to the
EL34's, so nothing to argue.

**`cathode_bias_volts` = −55.** The 5150's cathodes are grounded and the bias supply sits on the
grid leaks, so the field's meaning here is the grid's DC operating point relative to the cathode,
which is the bias-supply voltage. −55 V is S5's figure `[single source]`, and S5's two URLs no
longer serve it to a reader (§1).

S7 does not corroborate it and is worth walking through, because the gap is real. RCA's AB₁ rows
are −30 V at 450 V plate with a 350 V screen for 47.5 mA per tube, and −37 V at 450 V plate with a
400 V screen for 58 mA per tube. Bias tracks the screen voltage roughly proportionally, so the
5150's screen — the plate node less R210's drop, call it 460 V — scales the first row to
`−30 × 460/350` = −39 V for 47.5 mA. Going from 47.5 mA down to the 5150's 30 mA needs another
`17.5 mA / 4.5 mA/V` ≈ 3.9 V, giving about −43 V. That is 12 V shallower than S5's −55 V. Either
the forum figure is wrong, or the screen is running further below the plate than assumed, or the
bias-supply reading was taken somewhere other than the grid. The field keeps −55 V because it is
the only figure anyone has published for this amplifier, and it keeps `[single source]`; §8
carries the discrepancy. The class-AB conduction window that comes out of −55 V is 110 V
peak-to-peak of grid drive, against the EL34 preset's 40 V.

`PentodeStage`'s node equation is bias-invariant by the argument spelled out in
`modules/jcm800/src/Jcm800.cpp`, so this field does not by itself change the output; it is
recorded because the bias mismatch between the two halves is applied against it.

**`gain` = 6.** A modelling choice inside a bracket the datasheets set, not a computed value. The
EL34 preset's 8 is the linear grid-to-plate gain before saturation, which for a pentode into a
load is `gm · (Zaa/4)`, with `gm` the transconductance of everything driven in parallel on one
half of the primary.

Transconductance first. Neither datasheet prints `gm` at the idle current its amplifier runs, so
both are scaled from the printed point by `Ia^(1/3)`, which is the pentode's own current law.
S7: 6.0 mA/V at 72 mA gives `6.0 × (30/72)^(1/3)` = 4.48 mA/V at the 5150's 30 mA. S8: 12.5 mA/V
at 100 mA gives `12.5 × (45/100)^(1/3)` = 9.58 mA/V at the 2203's ~45 mA. The 5150 puts two 6L6GC
on each half of the primary and the 2203 puts one EL34, so per half the ratio is
`2 × 4.48 / 9.58` = 0.94, not the 0.5 a bare tube-for-tube comparison would give.

Load next, and this is where the earlier revision of this file had the sign wrong. Four tubes at
120 W run a *lower* plate-to-plate impedance than two at 100 W, not a higher one. S7 asks 6000 Ω
for two 6L6GC making 50 W at 450 V; paralleling a second pair to roughly double the power halves
that to about 3000 Ω, and the 5150's 120 W at 470 V sits at or below it. Against the 2203's
3400 Ω, `Zaa/4` therefore falls from 850 Ω to 625–750 Ω, a ratio of 0.74–0.88. The correction
pushes `gain` down.

**The 2203's 3400 Ω is an assumption, not a sourced figure.** It is the number
`docs/reference/research/2026-09-06-mesa-mark-iic-plus-design-dossier.md` §7.1 uses for the same
purpose, and it is the number that makes `kEl34`'s 8 come out of `gm · Zaa/4` for one EL34, so the
whole preset family rests on it. No Marshall specification for the 2203's output transformer was
found. It is at least consistent with S7 — RCA asks 6000 Ω for two 6L6GC making 50 W, and a
100 W pair scales to roughly half that — but that is a plausibility check, not a source. What
settles it: a measured turns ratio on a 2203 transformer, or Marshall winding data. Until then
every ratio in this section is a ratio of two assumptions and `gain` stays a modelling choice.

`8 × 0.94 × 0.74` = 5.5 at Zaa = 2500 Ω and `8 × 0.94 × 0.88` = 6.6 at Zaa = 3000 Ω. **`Zaa` is
not established** (§5), so 6 is the midpoint of that bracket and nothing stronger. When the
transformer's primary impedance is measured, recompute the field from it rather than keeping 6.

**`saturation_current_amps` = 1e-9** and **`thermal_voltage_volts` = 0.02585.** Unchanged. The
first is the grid-conduction diode's saturation current and the second is `kT/q` at room
temperature. A 6L6GC's grid-conduction knee sits within a few tens of millivolts of an EL34's, and
neither field carries tube-specific information worth changing on the evidence I have.

**`plate_saturation_drive` = 0.048.** The header says the output ceiling is `1 / plate_saturation_
drive`, so the EL34's 0.05 sets a 20 V ceiling. The ceiling scales with the plate swing the tube
can actually deliver, which scales with the rail: 470 V against the 2203's ~450 V is a factor of
1.044, giving 0.0479. A 6L6GC's knee sits at a slightly higher residual plate voltage than an
EL34's for the same current, which trims it back, so 0.048 rather than 0.0479 rounds in the
direction the physics runs. The whole field inherits `kEl34`'s own uncertainty, and the scaling
only holds if the gain staging around the stage is the same in both modules; the 5150 module
should check that before adopting it.

---

## 8. Open questions

**Blocking. These have to be answered before the module is written.**

1. **The power-supply sheet.** No rail voltage for V1, V2 or V3, no dropping resistors, no filter
   capacitors, no bias supply, no rectifier. §3's whole DC table is parametric because of this and
   §5's bias figure rests on one blog post. What settles it: the full Peavey factory schematic set
   for the block-letter 5150 — the 11-page "Peavey 5150 EVH 120 2/92" document listed on
   audioservicemanuals.com and elektrotanya. Both gate the download behind a CAPTCHA that a
   programmatic fetch cannot pass. Opening either by hand in a browser is ten minutes of work and
   closes the largest hole in this file.

2. **The output transformer's primary impedance and turns ratio.** §5. Nothing published names
   either for part 70500207. It sets the power stage's load line and therefore `gain` in §7,
   which is why that field is a bracket midpoint rather than a number. What settles it: measuring
   the DC resistance and the turns ratio on a real transformer, or a winding-data answer from
   Peavey.

3. **The output tubes' grid bias.** §7 shows S5's −55 V is about 12 V deeper than S7's AB₁ rows
   extrapolate to for 30 mA at 470 V, and S5's two URLs no longer resolve. What settles it: a
   meter on pin 5 of a real 5150 with the amplifier idling, taken at the same time as the plate
   current.

**Resolvable by measurement on a real amplifier**

4. Preamp gain at a stated lead pre-gain setting, and THD against drive at a stated setting.
   Neither is published for the lead channel (§6), so the module has no calibration ladder of the
   kind the DD-3 dossier had. What settles it: a swept-sine and a THD-versus-level sweep taken at
   the PREAMP OUT jack, whose nominal level Peavey does publish as 3 V RMS.

5. The tone stack's frequency response with all three controls at their detent, measured at the
   post-gain wiper. §4's topology is now closed off the scan, so this is a confirmation rather
   than a gap.

6. The screen node's sag under sustained drive. §5 argues the shared 400 Ω matters; nobody has
   plotted it.

7. Idle current spread across four tubes as shipped. S5's 25–35 mA band is Peavey's advice to
   owners, not a production figure.

**Documentation and legibility gaps**

8. **R12, R17 and R13, the three bypassed cathode resistors, read as `1.02K` with a `1 %`
   suffix.** Re-read at 9× on S1 page 1: the leading glyph is a single stroke with a serif foot,
   a `1`, not a `4`, so the −0.90 V against −2 V scenario an earlier revision of this file raised
   is not live. The residual doubt is in the second and third digits, where the scan smears
   `.02`. 1.02 kΩ is an E96 1 % value and fits the precision parts elsewhere on the board. What
   settles it: a meter on a real board. Any plausible misreading now moves V1A's bias by tens of
   millivolts, not volts.

9. **VR8's value.** Read as 1 MΩ audio from a label that is partly illegible; the Peavey part
   number POT0752 is shared with the 1 MΩ audio pots elsewhere on the sheet, which supports it.
   Marked C in §5.

10. **S2's 470 kΩ input impedance against S1's 1 MΩ grid leak (R25).** One of the two is wrong, or
    the specification is quoting something other than the grid leak. What settles it: an impedance
    measurement at J37, or the parts list from the factory set.

11. **Peavey's own typos in the lead-channel input-level specification** (§6): "−80 dBV, −1 mV RMS"
    and "92 dBV. /025 mV RMS". A measurement would replace both.

12. **The V5 designator collides** between the fifth 12AX7 and the first 6L6GC on S1's sheets. The
    factory set would say which is Peavey's and which is the redraw's.

13. **C8's voltage rating.** The sheet prints `.1P10` in the presence branch. `P` marks 100 V
    parts elsewhere on the same sheet, and a 10 V film capacitor fed from the 8 Ω tap would be
    odd, so 100 V is the likely reading. What settles it: the parts list from the factory set, or
    the part itself.

14. **Which coil state K1 sits in for the lead channel.** §3. K1A switches R18 47 kΩ under V1B's
    C26, K1B switches R20 220 kΩ at V2A's output, both drawn grounded in the de-energised
    position, and S1 carries no coil drive for K1. The effect on V1B is under 0.1 dB, but K1B's
    is a 330 kΩ-to-132 kΩ change at V2A's output. Against V2A's source impedance of
    100 k ∥ 301 k = 75 kΩ that moves the divider from 0.815 to 0.638, or 2.1 dB, which does
    matter. What settles it:
    the footswitch and relay-logic sheet from the factory set, or continuity across K1A with the
    amplifier in each channel.

    The sheet's own labelling makes this worse rather than better. K1A pin 4 and K1B pin 5 are
    marked `N/C`. Read as "no connection" the account in §3 holds and the drawn armature is the
    de-energised position; read as "normally closed" the drawn armature is the *energised*
    position and the resting state is the opposite of what §3 says. Nothing on the five sheets
    settles which convention Peavey used, and the same continuity check answers both questions at
    once.

15. **Where C52's far plate lands.** §3 reads it as the post-gain node, the same node C11 leaves,
    which makes V3B a buffer hung across the master volume in parallel with the driver rather
    than a stage in series ahead of it. The trace runs from V3B's grid left to x ≈ 385, down,
    right to x ≈ 502, down to C52, then right along y ≈ 1446 to the vertical at x ≈ 838 that
    carries the CLAMP node down to K2B's common. The last leg is the one I would not swear to at
    this scan resolution, so C52 is marked `[C]` in §2 and §4. What settles it: continuity
    between C52's far plate and K2B's common terminal on a real board. It matters because it
    decides whether the effects loop sees the master volume's setting.

16. **No SPICE deck is committed.** `docs/reference/spice/p5150_fmv/` for the tone stack and
    `p5150_ltp/` for the phase inverter are both worth writing; the values of both blocks are
    established, and only the decks' usefulness, not their correctness, waits on the rail voltage
    from question 1.
