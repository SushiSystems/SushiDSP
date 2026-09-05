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
| S5 | Peavey support answers relayed on forums.peavey.com and ultimatemetal.com, found through web search on 2026-09-06 | <https://forums.peavey.com/viewtopic.php?t=38145>, <https://ultimatemetal.com/threads/peavey-5150-bias-values.701601/> | undated forum threads | Grid bias −55 V, idle 30 mA per tube in a 25–35 mA band. Second-hand relay of a manufacturer answer, not a document. Marked `[single source]` in §5. |
| S6 | `docs/reference/spice/models/12ax7_koren.sub` in this repository | — | — | The 12AX7 model used for the operating-point arithmetic in §3, so that the dossier's DC points and the module's triodes come from the same physics. |

Sources I looked for and could not read: the Peavey factory power-supply sheet. The five sheets
of S1 stop at the output transformer and carry no rectifier, filter chain, dropping resistors or
bias supply, and no marked node voltages. Elektrotanya, schematicsunlimited.com and
audioservicemanuals.com each hold a longer 5150 set, and each gates the download behind a CAPTCHA
or returns HTTP 403 to a programmatic fetch. This is the single largest gap in the dossier and it
is what §8 opens with.

No SPICE deck is committed with this dossier. The one block that would most repay simulation is
the tone stack, and the tone stack is the one block whose interconnection I could not close off
the scan (§4). Committing a netlist for it would freeze a guess in a file that reads as a fact.

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
                    C2 .022/400
                    │
                    R6 470K ─┬─ R9 1M ─ gnd            interstage divider, 0.680
                             │
   V1  ─ R1 100K ── V2A plate ┘   V2A cathode: R15 39K, UNBYPASSED  (the cold clipper)
                    │
                    C1 .022/400
                    │
                    ├─ C48 .001 ─ gnd      ├─ R11 330K ─ gnd
                    ├─ R20 220K ─ K1B ─ gnd when the crunch relay closes
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
              │  TONE STACK: R28 47K, R24 33K, C59 .01 + R94 47K,  │
              │  C16 470p ∥ C60 100p, C18 .022, C21 .022,          │
              │  VR5 HIGH 250K B, VR3 LOW 1M A (push-pull),        │
              │  VR4 MID 50K B                                     │
              └─────┬─────────────────────────────────────────────┘
                    │
                    ├─ VR7 1M A  LEAD (ULTRA) POST ─┐
                    ├─ VR6 1M A  RHYTHM POST ───────┴─ K2B ─ R85 220K, R81 2.2M
                    │                                          │
                    C11 .047/400 ── V3B grid (R79 1M grid leak) │
   V2  ── V3B plate direct, no plate resistor: CATHODE FOLLOWER
                    cathode ─ R80 1K ─ tap ─ R16 100K 1W ─ gnd
                    │
                    tap ─ C53 .33/400 ─ K3A ─ EFFECTS SEND   |   PREAMP OUT via C66 .1/250,
                          EFFECTS RETURN ─ K3B ─ R103 100K,  |   R104 100K, R100 1M
                          C65 .33/400, R33 100K
                    │
                    └── V3A grid
   V2  ─ R5 68K ─── V3A plate            V3A cathode: R14 3.3K ∥ C61 .0047
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
   FEEDBACK tapped at W. J1 8 Ω, J2 4 Ω, impedance selector 71466024.
```

---

## 3. Preamp, stage by stage

Five gain stages sit between the input jack and the tone stack: V1A, V1B, V2A, V2B, V5B, V5A —
six triodes, of which V2A is a starved unbypassed stage rather than a gain stage in the usual
sense. After the stack come V3B (cathode follower) and V3A (driver). All are 12AX7.

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
negative one, and its plate cannot swing far upward. It is the source of the 5150's early,
compressed, even-harmonic-heavy break-up, and it must not be modelled as a generic 100 k / 1.5 k
Marshall stage.

V2A is the opposite. A 39 kΩ unbypassed cathode starves the stage to about 0.1 mA and puts the
plate within 10 V of the rail. Local degeneration of `1 + gm·Rk` with gm ≈ 0.35 mA/V at that
current gives a factor of about 14.7, so the stage's gain collapses to roughly 5 and its transfer
curve is stretched flat and made very linear over a wide input range before it cuts off hard on
one side. This is the cold clipper. Its job is to survive the huge signal V1B hands it and to
clip asymmetrically rather than to amplify.

The three stages after it, V2B (220 k / 1.02 k) then V5B (220 k / 2.2 k) then V5A (100 k / 2.2 k),
run progressively less starved and progressively more headroom, so the distortion is spread across
the chain instead of concentrated at one stage.

### Stage detail

**Input network.** J37 is the high-gain jack, J39 the low-gain jack, each through 22 kΩ (R27,
R30) into the same node; the manual calls J39 a −6 dB pad (S2). C20 39 pF from that node to
ground. C15 0.1 µF/250 V couples to the grid node, R25 1 MΩ is the grid leak to ground, R22 68 kΩ
is the grid stopper. S2 publishes the high-gain input impedance as 470 kΩ, which does not follow
from a 1 MΩ grid leak; the discrepancy is in §8.

**V1A.** Plate R2 220 kΩ from V1. Cathode R12 1.02 kΩ 1 %, bypassed by C3 1 µF 50 V non-polar.
The bypass corner is `1/(2π·1e-6·(1.02k ∥ 1/gm))`; with gm ≈ 1.6 mA/V at 0.88 mA the parallel
combination is about 385 Ω, giving 413 Hz (**B**). Below that corner the stage degenerates, so
the very bottom of the guitar's range is attenuated at the first stage — part of why the amp
stays tight under a low-tuned riff. Coupling out on C6 0.022 µF/400 V.

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

**V2A.** Plate R1 100 kΩ from V1. Cathode R15 39 kΩ, no bypass capacitor. Coupling out on
C1 0.022 µF/400 V. The output node carries C48 1 nF to ground and R11 330 kΩ to ground, and R20
220 kΩ to relay K1B, which grounds it when the crunch relay closes. R7 220 kΩ is V2B's grid
stopper.

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

Two unbypassed 2.2 kΩ cathodes in a row is unusual for a high-gain preamp and is worth stating
plainly: these two stages contribute local negative feedback, not gain. With gm ≈ 1.35 mA/V the
degeneration factor is about 4, so each stage runs a gain near 15 instead of near 60, and each
one's transfer curve is much straighter than an unbypassed-cathode stage's. They keep the last two
stages from turning the accumulated distortion into mush.

**V3B, cathode follower.** Plate connected straight to V2, no plate resistor. Grid from the post-
gain wiper through C11 0.047 µF/400 V, with R79 1 MΩ as the grid leak. Cathode into R80 1 kΩ, then
R16 100 kΩ 1 W to ground; the output is taken at the R80/R16 junction, so the follower drives the
effects loop and the preamp-out jack from a low impedance while the 1 kΩ isolates it. R85 220 kΩ
and R81 2.2 MΩ sit at the post-gain node ahead of it.

**V3A, driver.** Plate R5 68 kΩ from V2. Cathode R14 3.3 kΩ, bypassed by C61 4.7 nF only. That
capacitor is the interesting part: `1/(2π·4.7n·(3.3k ∥ 1/gm))` with gm ≈ 1.5 mA/V gives a corner
near 68 kHz (**B**), far above the audio band, so C61 is not an audio bypass at all. Below the
corner the stage is fully degenerated by 3.3 kΩ and runs a gain near 12. Treat R14 as unbypassed
in the audio band and treat C61 as an oscillation stopper.

---

## 4. Tone network

One passive stack, shared by both channels, sitting between V5A's plate and the two post-gain
pots. S2 calls it "Custom Low, Mid & High passive type EQ".

| Ref | Value | Role as drawn |
|---|---|---|
| VR5 | 250 kΩ linear (B) | HIGH |
| VR3 | 1 MΩ audio (A), push-pull switched | LOW; the switch is the panel's BRIGHT, rhythm channel only per S2 |
| VR4 | 50 kΩ linear (B) | MID |
| R28 | 47 kΩ | in series from V5A's coupling node into the stack's lower branch |
| R24 | 33 kΩ | from V5A's coupling node to the grounded node beside VR5 |
| C16 ∥ C60 | 470 pF ∥ 100 pF = 570 pF | treble capacitor, V5A's coupling node into the HIGH pot |
| C18 | 0.022 µF | bass capacitor |
| C21 | 0.022 µF | mid capacitor |
| C59 + R94 | 0.01 µF in series with 47 kΩ | from V5A's coupling node to the same grounded node as R24 |
| VR7 | 1 MΩ audio | LEAD (ULTRA) POST GAIN, after the stack |
| VR6 | 1 MΩ audio | RHYTHM (CLEAN) POST GAIN, after the stack |
| R85 | 220 kΩ | post-gain node to V3B's grid network |
| R81 | 2.2 MΩ | post-gain node |
| R83 (tone area) | 47 kΩ | between the two post-gain pots' ground legs |
| C52 | 0.022 µF/400 V | from the stack output onward |

**The topology is not fully established and the component values are.** Every value above was read
at magnification and is A. The interconnection is a Fender/Marshall FMV stack, which the parts
list makes certain: a treble capacitor into a treble pot, a slope resistor into a bass capacitor
and bass pot, a mid capacitor into a mid pot to ground. What I could not settle from the scan is
which of R28 47 kΩ and R24 33 kΩ is the slope resistor. Traced literally, R24's far end lands on a
node carrying a ground symbol, which cannot be right for a slope resistor and suggests the symbol
belongs to the adjacent VR5 terminal rather than to R24's node. R28 sits where a slope resistor
sits, in series from the input into the bass/mid branch. My reading is that R28 47 kΩ is the slope
resistor and that R24 33 kΩ with the C59/R94 leg forms a fixed shunt network across the input, but
I did not close it, and I am not going to write it into a SPICE deck as though I had. §8 says what
would settle it.

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

The bias supply is on the missing power-supply sheet. Not established from S1; the numbers below
are the only ones I have.

Idle bias: −55 V on the grids, 30 mA per tube nominal in a 25–35 mA band, per Peavey support as
relayed on the Peavey and Ultimate Metal forums `[single source]` (S5). Plate voltage 481 V
measured by one technician, who then set 34 mA on the hottest tube `[single source]` (S4). A
second forum figure of 465 V appears in the same searches. Take 470 ± 15 V as the working range
and treat any of it as unconfirmed.

At 30 mA and 470 V a single 6L6GC dissipates 14.1 W, which is 47 % of the 30 W plate rating: a
cool, firmly class-AB bias, consistent with the amplifier's tight low end and its reputation for
staying clean in the power stage until very late.

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
| C8 | 0.1 µF/10 V | presence branch |
| C62 | 0.033 µF | presence branch, in parallel with C8's leg |
| C30 | 22 µF/25 V electrolytic | network output into the PI tail node N |

S2 describes both controls as active: "Active Presence: +10 dB 2 kHz. Active Resonance: +10 dB @
cabinet resonant frequency." They are not active circuits; they are frequency-selective reductions
of the global feedback, which raises the closed-loop gain at those frequencies. The +10 dB figure
is the loop's, and it is the number a test should assert.

**Output transformer.** Part 70500207. Primary impedance and turns ratio are **not established**.
No source I read prints either, and S1 marks only the winding colours and the 8 Ω and 4 Ω
secondary taps.

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

```
k6L6GC{ 39000, 220000, -55, 5, 1e-9, 0.02585, 0.048 }
```

**`source_resistance_ohms` = 39000.** Derived, not scaled. The grid is driven from the phase
inverter's plate: the plate load in parallel with the triode's own plate resistance, plus the
2.2 kΩ grid stopper. A 12AX7 at the operating point of §3 has ra ≈ 62 kΩ, so the V4B side gives
82 k ∥ 62 k + 2.2 k = 37.6 kΩ and the V4A side 100 k ∥ 62 k + 2.2 k = 40.5 kΩ. 39 kΩ is the pair's
average; the two halves differ by 8 %, which is small enough to ignore and large enough to record.

This is the field that matters most and the one that departs furthest from `kEl34`. With
`grid_leak_resistance_ohms` unchanged at 220 kΩ, raising the source resistance from 10 kΩ to 39 kΩ
raises the voltage the grid-conduction current develops across the source by a factor of 3.9, so
the modelled stage blocks far harder when it is overdriven. That is the correct direction: a 5150
driven past the point where the phase inverter clips does audibly block, and the circuit reason is
exactly this — a 100 kΩ grid stopper on the driven PI half feeding a 2.2 kΩ stopper into a 220 kΩ
leak.

**`grid_leak_resistance_ohms` = 220000.** Read off S1: R54 and R55 are 220 kΩ. Identical to the
EL34's, so nothing to argue.

**`cathode_bias_volts` = −55.** The 5150's cathodes are grounded and the bias supply sits on the
grid leaks, so the field's meaning here is the grid's DC operating point relative to the cathode,
which is the bias-supply voltage. −55 V is S5's figure `[single source]`. It is deep for a 6L6GC
against the EL34's −20 V, and it should be: 470 V on the plate against the 2203's ~450 V, and a
6L6GC's lower transconductance, both push the grid further negative for the same idle current. The
class-AB conduction window that comes out of it is 110 V peak-to-peak of grid drive, against the
EL34 preset's 40 V.

`PentodeStage`'s node equation is bias-invariant by the argument spelled out in
`modules/jcm800/src/Jcm800.cpp`, so this field does not by itself change the output; it is
recorded because the bias mismatch between the two halves is applied against it.

**`gain` = 5.** Scaled, with a caveat. The EL34 preset's 8 is the linear grid-to-plate gain before
saturation, which for a pentode into a load is `gm · (Zaa/4)`. The 6L6GC's transconductance at
30 mA and 470 V is about 5.2 mA/V against the EL34's 10–11 mA/V at its own idle, a ratio of 0.5,
which gives 4. The 5150 runs four tubes rather than two, so `Zaa/4` per half is larger than the
2203's for the same tube count, which pushes the number back up. 5 is the compromise. **`Zaa` is
not established** (§5), so this field cannot be derived properly until it is; 5 is the honest
midpoint of the range the argument allows, not a computed value.

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

2. **Which of R28 47 kΩ and R24 33 kΩ is the tone stack's slope resistor.** §4. The values are
   certain and the arrangement is not. What settles it: the same factory sheet at higher scan
   resolution, or a continuity check on a real board between V5A's coupling capacitor, the treble
   pot's lower terminal and the bass capacitor.

3. **The output transformer's primary impedance and turns ratio.** §5. Nothing published names
   either for part 70500207. It sets the power stage's load line and therefore `gain` in §7. What
   settles it: measuring the DC resistance and the turns ratio on a real transformer, or a
   winding-data answer from Peavey.

**Resolvable by measurement on a real amplifier**

4. Preamp gain at a stated lead pre-gain setting, and THD against drive at a stated setting.
   Neither is published for the lead channel (§6), so the module has no calibration ladder of the
   kind the DD-3 dossier had. What settles it: a swept-sine and a THD-versus-level sweep taken at
   the PREAMP OUT jack, whose nominal level Peavey does publish as 3 V RMS.

5. The tone stack's frequency response with all three controls at their detent, measured at the
   post-gain wiper. It would settle question 2 as a side effect.

6. The screen node's sag under sustained drive. §5 argues the shared 400 Ω matters; nobody has
   plotted it.

7. Idle current spread across four tubes as shipped. S5's 25–35 mA band is Peavey's advice to
   owners, not a production figure.

**Documentation and legibility gaps**

8. **R12, R17 and R13, the three bypassed cathode resistors, read as `1.02K` with a `1 %` suffix
   at the limit of what the scan carries.** All three render the same glyph sequence, and 1.02 kΩ
   is an E96 1 % value, which fits Peavey's use of precision parts elsewhere on the board. The
   leading digit could be a 4. What settles it: a higher-resolution scan, or a meter on a real
   board. A 4.02 kΩ reading would move V1A's bias from −0.90 V to roughly −2 V and change the
   stage's character completely, so this is worth ten minutes before any code is written.

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

13. **No SPICE deck is committed.** Once question 2 is answered, `docs/reference/spice/p5150_fmv/`
    for the tone stack and `p5150_ltp/` for the phase inverter are both worth writing; the phase
    inverter's values are fully established already and only its deck's usefulness, not its
    correctness, waits on the rail voltage from question 1.
