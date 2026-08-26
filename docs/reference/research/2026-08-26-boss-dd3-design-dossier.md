# Boss DD-3 (and DD-2) — Design-Ready Dossier

Compiled 2026-08-26. Read together with `2026-08-26-boss-dd2-dd3.md`, which holds the
revision-history narrative and the prior-art survey. This file is the one to build from: it
carries the schematic values read **visually off a high-resolution scan** rather than from OCR,
the filter topology and order, the compander time constant, and a modelling architecture.

Confidence key: **A** = official Roland/Boss service notes (read directly). **B** = corroborated
secondary source or a value derived by me from A. **C** = single unverified source or an inference
I did not actually read off the page. Unmarked = A.

**Method note, because it changes how much you can trust §1.** The archive.org `_djvu.txt` OCR of
the DD-2/3 service notes mangles the schematic page. The values below were obtained instead by
pulling the synfo.nl PDF, extracting the embedded 6614×4672 bilevel TIFF of page 3, and reading it
at magnification. The prose sections of the OCR (adjustment procedure, switching tables, parts
list) are clean and are quoted verbatim where used.

Primary sources:
* DD-2/DD-3 Service Notes, Sep 1986 — <https://archive.org/details/boss_DD-2_DD-3_SERVICE_NOTES>,
  scan used: <https://www.synfo.nl/servicemanuals/Boss/BOSS_DD-3_SERVICE_NOTES.pdf>
* DD-3A Service Notes, Apr 1991 — <https://archive.org/details/boss_DD-3A_SERVICE_NOTES>
* Roland SDE-1000/SDE-3000 Service Notes (same LSI, far better documented) —
  <https://archive.org/details/roland_SDE-1000_SDE-3000_SERVICE_NOTES>
* Boss DE-200, RDD-10/20, DSD-2/3 Service Notes (same LSI family)

---

## 1. Signal chain with component values

### 1.0 Block overview

```
IN ─ R1 10K ─ C1 0.027µ ─ Q1 2SK30A-0 (source flwr, R2 1M to ½Vcc)
   └─ Q2 2SC732 (emitter flwr, R3 33K / R4 10K)
      │
      ├──── C3 1µ ─ R6 1K ──────────────────────────── DIRECT jack (pure analog, 10 Hz–60 kHz)
      │                                                 Q7 2SC2603F mutes it
      │
      └─ C2 1µ ─ R7 47K ─ IC1b M5218 PRE-EMPHASIS ─── [CP1] 620 mVp-p
         │  fb: R12 33K ∥ (R8 10K + C4 0.0047µ) ∥ C5 33p   → ≈ +10.4 dB shelf above ~3.4 kHz
         │        ▲
         │        └──────────────────────── F.BACK injected here (see §1.6)
         │
         ├─ ANTI-ALIAS LPF — 6th order, 3 × emitter-follower Sallen-Key, fc = 7 kHz ─ [CP2] 400 mV
         │
         ├─ C18 10µ/16 ─ NE570 IC2a COMPRESSOR   (CRECT = C32 0.68µ → τ = 6.8 ms)
         │
         ├─ S/H: IC4 HD14066BP + IC6 NJM5534D, C40 330p ─ [CP3] 2.3 Vp-p
         ├─ comparator IC5 µPC271C / NJM311D (R51 1K, R52 82K, R53 1.5K, R55 2.7K)
         └─ R73 R-2R ladder RKM14L492-103F (10 k) ◄──► D0..D11
                                   ▲  ONE converter, time-shared A/D and D/A
                                   │
                     IC7 RDD63H101 custom gate array (= Fujitsu MB63H101)
                     ├─ DIN / RAM1..RAM3 / A0..A7 / RAS / MSCK / HDCK / SAH / HOLD / DB4 / DB5
                     └─ IC11-13: 3 × 4164 64 Kbit DRAM, 1 bit wide each,
                        3 bits parallel × 4 cycles = 12-bit word
                                   │
         ┌─────────────────────────┘
         ├─ NE570 IC2b EXPANDER   (CRECT = C33 0.68µ → τ = 6.8 ms; R42 47K; C20 0.0027 ∥ R41 22K)
         │
         ├─ RECONSTRUCTION LPF — 4th order, 2 × emitter-follower Sallen-Key, fc = 7 kHz ─ [CP4] 2.4 V
         │    (R50 330K biases the input node to ½Vcc)
         │
         └─ C29 1µ ─ Q8 2SK30A-Y (delay-path mute FET, R37/R38 1M, D4) ─ C28 1µ ─ node 12
              │
              ├─ VR2 50 kB  F.BACK  ─ wiper node 11 ──► back to pre-emphasis summing node
              └─ VR3 50 kB  E.LEVEL ─ wiper node 13 ─ R14 33K + C13 1µ ─┐
                                                                        │
   DIRECT branch ─ R13 33K ─────────────────────────────────────────────┤
                                       IC1a M5218 DE-EMPHASIS / MIXER  ─┘
                                       fb: R16 47K ∥ (R15 10K + C10 0.0047µ) ∥ C12 33p
                                       Q5 2SK30A-Y mutes the summing node (C24 0.047, D1)
                                       out ─ C11 1µ ─ R17 1K ─ MONO jack (R18 100K to gnd)
```

Check-point levels for 1 kHz / 100 mVp-p in (page 2 of the service notes): [1] 620 mVp-p,
[2] 400 mVp-p, [3] 2.3 Vp-p sampled-and-held, [4] 2.4 Vp-p, [5] 140 mVp-p delay signal.
These are a free calibration ladder — a correct model should hit them.

### 1.1 Input buffer

JFET source follower into a BJT emitter follower. `R1 10K` series, `C1 0.027 µF` coupling
(→ ~590 Hz with 10 k, but the 1 MΩ bias resistor dominates: the real corner is ~5.9 Hz),
`R2 1M` to ½Vcc, Q1 = 2SK30A-0. Q2 = 2SC732 TM-GR with `R3 33K` base-to-ground and
`R4 10K` emitter load. Input impedance 1 MΩ; the DD-3A deletes Q2 and takes the signal straight
off the JFET, and raises C1 to 0.047 µF.

### 1.2 Pre-emphasis (IC1b, M5218)

Inverting stage. Input `C2 1µ/50 + R7 47K`. Feedback is `R12 33K`, shunted at HF by the series
`R8 10K + C4 0.0047 µF`, with `C5 33 pF` across R12 limiting the top.

Gain at DC/LF = 33K/47K ≈ −3.1 dB. Gain at HF (C4 shorted) = (33K ∥ 10K)/47K ≈ −13.5 dB.
Wait — that is the wrong direction if you read the shunt as reducing feedback impedance, which it
does. Read it correctly: shunting the feedback resistor **lowers** HF gain. So this network as
drawn is a **treble cut**, not a boost, unless R8+C4 sits in the *input* leg. **Confidence C on the
sign.** The agent that read the scan reported it as a ~+10.4 dB shelf above ~3.4 kHz (i.e. R8/C4 in
the input leg, giving 33K/10K at HF). Companding theory requires pre-emphasis to *boost* treble
before the converter and de-emphasis to cut it after, so the +10.4 dB shelf is what the circuit
must be doing. **Confirm the leg from the scan before implementing.** Either way the numbers are
33K / 10K / 0.0047 µF / 33 pF and the corner is ~3.4 kHz.

The de-emphasis stage (IC1a) is the exact mirror: `R16 47K` main feedback shunted by
`R15 10K + C10 0.0047 µF`, `C12 33 pF`. Same corner, opposite sign.

### 1.3 Anti-alias LPF — 6th order, discrete Sallen-Key, fc = 7 kHz

Schematic legend, verbatim: `LPF  Q3-Q6: 2SC732TM-GR  fc=7KHz`.

Not an op-amp filter and not multiple-feedback. Three cascaded **unity-gain Sallen-Key low-pass
sections whose buffer is a bare NPN emitter follower**. Despite the "Q3-Q6" label there are only
three transistors in the filter — Q5 is the 2SK30A-Y mixer mute FET elsewhere on the board.
Each section: two 10 k series resistors, shunt cap to ground at the base, bridging cap
bootstrapped to the emitter, 10 k emitter load.

| Section | Buffer | R series | R series | C bridge→emitter | C shunt→gnd | Emitter load | f₀ (derived) | Q (derived) |
|---|---|---|---|---|---|---|---|---|
| 1 | Q3 | R11 10K | R10 10K | C9 0.0047 | C8 0.0027 | R9 10K | 4.47 kHz | 0.66 |
| 2 | Q4 | R22 10K | R21 10K | C15 0.0068 | C14 470p | R20 10K | 8.90 kHz | 1.90 |
| 3 | Q6 | R25 10K | R24 10K | C17 0.018 | C16 220p | R23 10K | 8.00 kHz | 4.52 |

f₀ and Q are **mine** (confidence B), from equal-R Sallen-Key: f₀ = 1/(2πR√(C₁C₂)),
Q = ½√(C₁/C₂). Q6's emitter is check point [2]; `C18 10µ/16` couples into the compressor.

Those Q values sit well above 6th-order Butterworth (0.518 / 0.707 / 1.932), so this is a
**Chebyshev- or elliptic-flavoured alignment**, not maximally flat — which is exactly what a
20 kHz worst-case sample rate demands from a 7 kHz brickwall. Expect passband ripple and a
pronounced group-delay peak near the corner. Both are audible and both are part of the pedal's
character; do not substitute a Butterworth.

### 1.4 Reconstruction LPF — only 4th order, fc = 7 kHz

Legend: `LPF  Q9-Q10: 2SC732TM-GR  fc=7KHz`. Same emitter-follower Sallen-Key form, **two**
sections instead of three.

| Section | Buffer | R | R | C bridge→emitter | C shunt→gnd | Emitter load | f₀ (derived) | Q (derived) |
|---|---|---|---|---|---|---|---|---|
| 1 | Q10 | R48 10K | R49 10K | C38 0.0027 | C37 0.0012 | R47 10K | 8.84 kHz | 0.75 |
| 2 | Q9 | R46 10K | R45 10K | C35 0.0068 | C36 150p | R44 10K | 15.8 kHz | 3.37 |

`R50 330K` biases the R48 node to ½Vcc. Q9's emitter is check point [4].

**The asymmetry is real and it matters.** 6th order going in, 4th order coming out, both nominally
"7 kHz". The output filter is the weaker one, and section 2 sits at 15.8 kHz with Q 3.37 — which
is above Nyquist at the slow clock (10 kHz) and does very little to suppress the reconstruction
image there. Modelling both filters as one shared 7 kHz response, which is the obvious shortcut,
throws away a genuine difference between the forward path and the feedback path.

### 1.5 The compander

NE570, both halves. Datasheet: <https://www.experimentalistsanonymous.com/diy/Datasheets/NE570.pdf>
(mirror <https://www.onsemi.com/download/data-sheet/pdf/ne570-d.pdf>; app note AN174 and ON Semi's
Compandor Cookbook AND8160 are useful).

The datasheet gives **one** time constant, not separate attack and release, verbatim:

> `G(t) = (Ginitial − Gfinal)·e^(−t/τ) + Gfinal ;  τ = 10k · CRECT`

where 10 k is the internal rectifier resistor R1. In the DD-2/DD-3, **CRECT = C32 = C33 =
0.68 µF**, identical for compressor and expander. Therefore:

* **τ = 10 kΩ × 0.68 µF = 6.8 ms**, symmetric, both halves.
* Rectifier ripple filter corner = 1/(2π·10k·0.68µ) = **23.4 Hz** (derived, B).

The datasheet's own application circuits use 1 µF (10 ms) to 2.2 µF (22 ms). Boss chose **faster
than typical**, trading the datasheet's stated penalty — ripple modulating the gain cell into
third-harmonic distortion on low-frequency material — for tighter transient tracking. That
trade-off is a large part of what people hear as the DD-2's "grain", and a model that uses a lazy
30 ms envelope will not reproduce it.

Surrounding values — compressor: `R28 47K`, `C19 0.0027 + R29 22K` into the summing node,
`R31 22K + C25 0.0027` in parallel with `R33 47K` on the output into `C26 1µ`, `R32 10K`,
`R39 10K`, `C30 47µ/6.3`, `C22 100p`. Expander: `R42 47K` feedback, `C20 0.0027 ∥ R41 22K`
output network, `C21 100p`, `C34 10µ/16`.

**Unresolved:** `R40` reads as 3.3K on the compressor side and 2.7K on the expander side of the
same page. Both values are in the parts-list inventory. Could be two references, could be a
mis-read. Confidence C; check on a board.

### 1.6 Feedback path — analog, wrapping the whole converter

Established with confidence:

* **F.BACK (VR2 50 kB) and E.LEVEL (VR3 50 kB) share one source node** (node 12), fed via
  `C29 1µ → Q8 2SK30A-Y → C28 1µ`, with R37/R38 1 M biasing to ½Vcc and D4 as the usual Boss
  FET-switch bootstrap. Q8 is the delay-path mute FET, confirmed by the service note's own
  switching table.
* **E.LEVEL is a passive attenuator, not a VCA**: wiper → `R14 33K + C13 1µ` → mixer inverting
  node. Traced end to end.
* **F.BACK injects at the pre-emphasis summing node**, i.e. *upstream of the anti-alias filter*.
  It has to — the anti-alias filter must see the recirculated signal.
* Both mixer branches enter through 33 k (`R13` direct, `R14` delay) into the 47K/10K-shelved
  feedback. So dry and wet are summed at identical impedance; the mix law is purely the two pot
  positions.

**One thing not resolved.** Whether the C29/Q8 takeoff sits **before or after** the Q10/Q9
reconstruction filter. On the scan the expander-output wire (at R41) and the Q9-emitter wire run
within ~0.4 mm of each other. Reading (a) — takeoff *after* the filter, so both pots carry
band-limited signal — is near-certain on electrical grounds, because reading (b) would put raw
stepped DAC output onto the MONO jack and into the regeneration loop, contradicting the
published 40 Hz–7 kHz delay-path spec. **But it is an inference, not something read.** Confidence
C. It is worth ten minutes with a meter on a real unit, because it decides whether the feedback
loop contains the reconstruction filter once or zero times per pass.

### 1.7 The clock — how D.TIME sets fs

Legend, verbatim: `74HC00  IC8 VCO  (1.28MHz—5.18MHz)`.

```
Effect board:  NAND8b (6→4,5) ─ NAND8a (3→1,2)   oscillator pair
               C46 33p  timing cap
               NAND8c (9,10→8) → D10  steering back into the loop
               R56 22K → +5V, D8, R68 2.7K, C52 0.027  control node
               RT-1 2.2K trimmer  = "CLOCK HIGH" (fast end)

Volume board:  node 9 ── VR-1 pin 1
               VR-1 = 10 kA  D.TIME, LOG taper
               wiper (pin 2) ── node 8, C1 0.1 to ground
               pin 3 ── RT-1 10K trimmer ("CLOCK LOW") ── R1 3.3K ── ground
```

D.TIME is the variable leg of the VCO control network. The **log taper** is what makes delay time
feel roughly linear on the knob against an exponential-ish VCO law — model the taper, not a linear
pot.

Factory adjustment, verbatim from the notes (this part of the OCR is clean):

> `This adjustment is to set the range of Master Clock (MSCK) frequency at IC8 VCO.`
> `Connect the scope to pin 13 of IC4 (or IC7 pin 18 SAH).`
> `Adjust RT-1 on Effect Board for T=12.12us (82.5K ± 0.4kHz). The MSCK should be 5.18M ± 25.6kHz.`
> `Adjust RT-1 on Volume Board for T=50us (20K ± 0.1 kHz). The MSCK should be 1.28M ± 6.4kHz.`

Because the scope point is the sample-and-hold, **T is genuinely the sample period**, not a divided
clock. So **fs sweeps 20.0 kHz → 82.5 kHz**, a 4.125:1 range, continuously and glitch-free.

**An arithmetic discrepancy worth knowing.** MSCK/fs should be constant:
1.28 MHz ÷ 20 kHz = 64.0 exactly, but 5.18 MHz ÷ 82.5 kHz = 62.79. The **DSD-2/DSD-3** notes —
same LSI family, same procedure — print `5.28MHz` for the same 82.5 kHz point
(<https://archive.org/stream/boss_DSD-2-3_SERVICE_NOTES/DSD-2-3_SERVICE_NOTES_djvu.txt>), and
5.28 ÷ 82.5 k = 64.0 exactly. **"5.18M" in the DD-2/3 notes is almost certainly a typo for 5.28M.**
The fs figures themselves are solid (82.5 k × 12.12 µs = 1.000, 20 k × 50 µs = 1.000). Model it as
**MSCK = 64 × fs**. Confidence B.

The DD-3A restates the same scheme with a slightly different fast endpoint, verbatim:
> `Adjust RT1 (1kB potentiometer) on MT board for T = 12.5 ± 0.5uS`
> `Adjust RT2 (47kB potentiometer) on MT board for T = 50 ± 2uS`

That is **80 kHz**, not 82.5 kHz — a small but real generational difference in the top rate.

### 1.8 Buffer length per range, and HOLD

The mode switch `S1` (SBM1024 rotary, 4 positions) drives **DB4 and DB5** on the LSI, with
`R70 10K` / `R72 10K` pull-downs. Truth table printed on the schematic:

| Mode | Delay range | DB4 | DB5 |
|---|---|---|---|
| S (SHORT) | 12.5 – 50 ms | L | L |
| M (MIDDLE) | 50 – 200 ms | H | L |
| L (LONG) | 200 – 800 ms | L | H |
| H (HOLD) | 200 – 800 ms | H | H |

IC7 also has a dedicated `HOLD` pin (pin 7). **HOLD is a mode bit into the LSI, not an analog
trick** — it shares L's clock range with writes inhibited.

**Sample counts are fixed per range; delay time is set purely by sweeping fs.** Derived (B):

| Mode | N (derived) | Check |
|---|---|---|
| S | ~1000 (1024) | 12.5 ms × 80 kHz = 1000; 50 ms × 20 kHz = 1000 |
| M | ~4000 (4096) | 50 × 80 k = 4000; 200 × 20 k = 4000 |
| L / HOLD | ~16000 (16384) | 200 × 80 k = 16000; 800 × 20 k = 16000 |

Memory arithmetic closes it exactly: 3 × 64 Kbit = 196 608 bits; 16 384 × 12 = 196 608 bits.
Confidence B — the N values are derived, never printed. Measure them (§3).

### 1.9 The converter, from the SDE-3000 documentation

The DD-2/3 notes are 8 KB with no theory of operation. The **Roland SDE-1000/SDE-3000 notes are
68 KB with a real one**, and they cover the same LSI. The Boss RDD-10/RDD-20 notes give the
cross-reference: `RDD63H101P-G-SH (or MB63H101)`, C-MOS gate array
(<https://archive.org/stream/boss_RDD-10_RDD-20_SERVICE_NOTES/RDD-10_RDD-20_SERVICE_NOTES_djvu.txt>).
So **RDD63H101 (Roland) = MB63H101 (Fujitsu)**.

Pin functions, verbatim from the SDE manual:

> `D0–D11 — During A/D conversion cycle these pins represent comparison data from internal SAR.
> During D/A cycle, simultaneously output 12-bit RAM stored data which have been read from each
> RAM in time sequence (3-bits x 4) and temporarily buffered in internal three registers.`

> `RAM1–RAM3 — 12-bit data read from three RAMs are fed through these pins to internal three shift
> registers for temporary storage. All the buffered bits are placed on D0-D11 at a time to form a
> 12-bit parallel data to be applied to D/A converter.`

> `A0–A7 — These pins feed RAMs with addresses: Refresh, Row and Column. Since the number of RAM
> cells involved in data storage is varied with DELAY TIME setting, the numbers of Row and Column
> addresses are also changed accordingly while Refresh addresses are issued for all the RAM cells.`

> `IC12 63H-101 a specially designed gate array for controlling digital delay system. It clocks
> most of delay related sequences such as A/D, D/A, S/H, RAM accessing, etc.`

So: **12-bit successive approximation, one R-2R ladder time-shared between A/D and D/A**, external
comparator, external S/H, three 1-bit-wide DRAMs each holding 4 of the 12 bits. This is settled,
confidence A, and it kills the "sigma-delta" claim in the falseelectronics writeup.

**Two architectural differences from the SDE worth carrying into the model:**

1. **The SDE uses a fixed clock with stepped delay time** (it varies how much RAM is addressed);
   the **DD-2/3 uses a swept VCO with a fixed sample count**. The Boss DE-200 notes describe a
   third option — RNG pins selecting an internal ÷2. Boss/Roland used all three approaches across
   the family; the pedal's swept-VCO choice is what gives it the tape-like pitch bend.
2. **The SDE stores companding state digitally**, in dedicated companding RAMs (IC41/IC43,
   addressed via DB0–DB3), so its expander tracks the compressor block-for-block. **The DD-2/DD-3
   has nothing of the sort** — its NE570 expander runs open-loop off its own rectifier, tracking
   only the reconstructed audio. That mismatch between compression and expansion is a genuine,
   documented architectural difference and a plausible primary source of the pedal's character.
   It is also easy to get wrong in a model by cancelling the compander exactly.

### 1.10 The "16-bit equivalent" folklore, and where it came from

Forum lore says the DD-2/3 is "16k of 12-bit words, companded to yield 16-bit equivalent dynamic
range". The likely origin is visible in the SDE manual, verbatim:

> `DATA & SIFT — IC12 controls RAMs accessing sequence in accordance with DELAY TIME DATA received
> at SHIFT rate. The DATA is a serial stream of 16 bits.`

That 16 is the **CPU's serial delay-length word**, not the audio word width. The audio is 12 bits,
full stop. The same forum post's claim that "maximum sampling rate is around 42 kHz" is flatly
contradicted by the service notes. Discard both.

---

## 2. Revisions, and which one to target

### The terminology, settled

* **Long chip** = **RDD63H101**, a large custom gate array in a long DIP with a foil shield
  (Roland part 15229811). DD-2 and DD-3 v1.
* **Short chip / square chip** = **MN51010RBA**, a compact Matsushita gate array in a square
  package mounted on the **back** of the board. DD-3A onward.

Stompbox Electronics states it directly
(<https://stompboxelectronics.com/2023/07/20/history-of-the-boss-dd-3/>); Stinkfoot's photo guide
distinguishes v1 "large delay chip with foil shield" from v2 "square delay chip on circuit board
back" (<https://stinkfoot.se/archives/516>).

**Correction to a source in circulation:** professornigels.com claims the long chip is an MN3205.
That is an analog bucket-brigade device from the DM-2/DM-3 family, not a digital delay LSI.
Discard. Analogman also dates the DD-2 to 1982 (<https://www.analogman.com/boss.htm>), which is
off by a year or two.

### The revision table

| Version | Date | Change |
|---|---|---|
| DD-2 | 1983 | RDD63H101, 3 × 4164 |
| DD-3 v1 | 1986 | Identical circuit; the 1986 notes say DD-2 and DD-3 differ **only in the pedal treadle part number** (2218053300 vs 2218054700). External DC jack. |
| **DD-3A** | Dec 1990, notes Apr 1991 | Long chip → **MN51010RBA**; 3 × 64K DRAM → single **M5M4464L-12**; Q2 BJT buffer deleted; **C1 0.027 → 0.047 µF**; **C18 removed** (pre-emphasis direct-coupled to compressor); some 10 µF/50 V → 1 µF/16 V; op-amps M5218L+NJM5534D+µPC271C → M5218AL+**µPC4072HA**+NJM311D (note: an op-amp is *added*); clock endpoints 82.5 → 80 kHz |
| DD-3A v2b | — | PCB-mounted DC jack; MIJ or Taiwan |
| **DD-3B** | Aug 2001, manual May 2002 | DRAM → SMD **M11B416256A-35T**; most of the circuit goes SMD, double-sided board; gate array unchanged. Boundary given as serial **SO91200** |
| v4 | ~2014 | DRAM abandoned for a Roland DSP; nearly all SMD; Taiwan |
| DD-3T | 2019 | Tap tempo, relocated direct out, Malaysia; otherwise v4's circuit |

The DD-3A notes tell technicians how to tell the generations apart, verbatim:

> `For DD-3, there are 2 types, old type and new one, of DD-3 having a little difference of
> circuitry. Usualy, old type is written "DD-3" and new one is written "DD-3A" for the model
> number. But they are inscribed "DD-3" on the panel both types. If you wish to make sure of the
> model number, you ought to see the type of adaptor jack.`

**No "2007 revision" exists in any source.** The documented breaks are 1990/91, 2001/02, ~2014,
2019. DD-3B ran 2001–2014 without a version letter, so a quiet board respin in that window is
possible but undocumented — absence of evidence, not evidence of absence.

Also could not verify: any serial cutoff for long→short, and **any "silver label" DD-3** (observed
labels are blue MIJ, black, green, pink Taiwan; consensus is label colour tracks era and factory,
not circuit). One clean rule did hold: the long→short change **predates** the move to Taiwan, so
**there is no Taiwanese long-chip DD-3**.

### Target: the DD-2 / DD-3 v1 (long chip)

Reasons, in order:

1. **It is the only version whose full schematic is legible and whose LSI is documented** — via the
   SDE-3000, DE-200 and RDD-10/20 notes, which give pin functions, conversion sequence and memory
   organisation. The MN51010RBA has none of that.
2. **One model covers two pedals.** Boss's own parts list says DD-2 and DD-3 v1 differ only in the
   treadle. That is a free doubling of what the model claims to be.
3. It is the version the reputation attaches to, and the one the market prices as collectible.
4. The DD-3A's deltas are small, enumerated and mostly component-level — it is cheap to add later
   as a variant (delete Q2, C1 → 0.047, delete C18, top clock 80 kHz), *except* for the gate
   array's internal conversion timing, which is unknown either way.

---

## 3. Measurable targets

What a correct model must reproduce, with the value and where it comes from.

### Published (A)

| Target | Value |
|---|---|
| Delay ranges | S 12.5–50 ms, M 50–200 ms, L 200–800 ms, HOLD 200–800 ms |
| Sample rate | 20.0 – 82.5 kHz continuous (DD-3A: 20 – 80 kHz), MSCK = 64 × fs |
| Delay-path response | 40 Hz – 7 kHz, +1 / −3 dB, **in every range and at every time setting** |
| Dry-path response | 10 Hz – 60 kHz, +1 / −3 dB (never digitised) |
| Residual noise | < −95 dBm IHF-A (0 dBm = 0.775 Vrms) |
| Input impedance / output load | 1 MΩ / ≥ 10 kΩ |
| Power | 9 VDC, 45–65 mA, µA78L05 for +5 V, no boost |
| Check-point levels | [1] 620 mV, [2] 400 mV, [3] 2.3 V, [4] 2.4 V, [5] 140 mV p-p @ 1 kHz/100 mV in |

### Derived (B), and the point most models get wrong

**Bandwidth does not change with delay time or range.** The anti-alias and reconstruction filters
are fixed analog networks at 7 kHz while fs sweeps 4:1. At the *slowest* clock Nyquist is 10 kHz
against a 6th-order near-elliptic 7 kHz wall, so **there is essentially no in-band aliasing even at
800 ms**. What changes with fs is the quantisation-noise spectrum, the reconstruction stair-step
image content, and residual clock feedthrough. The DD-2/3's lo-fi character is **12 bits +
companding + the 7 kHz wall + repeated re-conversion**, not aliasing. A model that adds aliasing
at long delays is modelling a different pedal.

**Compander behaviour.** τ = 6.8 ms both directions, single time constant, symmetric. The noise
floor pumps with the signal at that rate rather than sitting still, audible on every repeat tail.
The rectifier's 23.4 Hz ripple corner means low-frequency material will ripple the gain cell into
third-harmonic distortion — the datasheet says so explicitly, and at 0.68 µF Boss is well into
that regime. **This is a feature to reproduce, not an artefact to filter out.**

**Self-oscillation.** The feedback loop is analog and wraps the *entire* chain: pre-emphasis →
anti-alias → compress → 12-bit SAR → DRAM → 12-bit DAC → expand → (probably) reconstruction filter
→ back to the pre-emphasis node. Every repeat is re-band-limited to 7 kHz and re-quantised.
Consequences to hit: repeats darken **cumulatively and rapidly** (each pass is another 6th-order
7 kHz filter, so after n passes the effective response is the filter to the nth power); grain
accumulates as 12-bit quantisation noise is itself re-compressed and re-quantised; and at high
F.BACK the loop builds toward a narrow, dark, buzzing oscillation rather than a bright ringing one,
with the NE570 pair setting the level at which it stabilises rather than clipping the op-amps.
Confidence B on the mechanism (it follows from the topology), **C on the specific character** —
nobody has measured it.

**12-bit clip behaviour.** Unresolved and important. The SAR converts against a fixed reference,
so an input past full scale simply rails the SAR at all-ones/all-zeros — **hard digital clipping,
not soft saturation**. But the compressor sits *before* the converter, so in normal use the
compander's gain reduction is what keeps the converter in range, and the clip point is
level-dependent and time-dependent (6.8 ms τ). Fast transients that outrun the compressor will
clip the converter hard. **Whether the LSI's SAR wraps or saturates on overflow is not documented.**
Must be measured.

### Not measurable from paper — a verified void

**No independent bench measurement of a DD-2 or DD-3 appears to have ever been published.**
Searched: general web, YouTube, Audio Science Review, gearspace, freestompboxes, diystompboxes,
modwiggler, music-electronics-forum. Specifically absent: frequency-response plots, THD or THD+N,
any noise-floor figure independent of the spec, spectrograms or aliasing plots, delay-time accuracy
data, **any video showing an analyser or scope screen**, and **any independent scope measurement of
MSCK** (tap-tempo modders cite the service note's numbers as a design target rather than measuring
them: <https://www.diystompboxes.com/smfforum/index.php?topic=93642.0>,
<https://modwiggler.com/forum/viewtopic.php?t=95964>).

Claims of "slight aliasing at long delays" appear only as listening impressions on retailer pages,
never as plots, and §3 above gives a structural reason to doubt them.

**Measuring one would be a genuinely novel contribution.** The closest existing work is
falseelectronics' repair writeup (<http://falseelectronics.blogspot.com/2017/08/boss-dd-2.html>) —
correct on the LSI, the 12 bits, the DRAM and the compander, but with no measurements — and
hobby-hour's schematic transcription
(<https://www.hobby-hour.com/electronics/s/dd2-delay.php>). There is no ElectroSmash-grade
analysis and no reverse-engineering project anywhere.

---

## 4. Proposed modelling architecture

The central decision: **this is a variable-rate loop, not a fixed-rate delay line with an
interpolating read tap.** A single free-running VCO clocks writes, reads and the converter
together, so turning D.TIME resamples everything already in the buffer — smooth, glitch-free
tape-style pitch bend. Fixed-rate-with-moving-tap gets the delay time right and the pitch behaviour
wrong.

### 4.1 Topology

```
host rate (analog domain, oversampled ×4 or ×8)
────────────────────────────────────────────────────────────────────────────
 in ─ input buffer (JFET+BJT, mild nonlinearity, HP ~5.9 Hz)
    ├───────────────────────────────────────────► dry ─┐
    │                                                   │
    └─►(+)◄──────────────── feedback (§4.4) ────────┐   │
        │                                            │   │
        ├─ pre-emphasis shelf (33K/10K/4.7n/33p)     │   │
        ├─ anti-alias LPF: 3 cascaded SK biquads     │   │
        │     (4.47k Q0.66 / 8.90k Q1.90 / 8.00k Q4.52)  │
        ├─ NE570 compressor: RMS-ish detector, τ 6.8 ms, │
        │     ripple corner 23.4 Hz, gain-cell distortion │
        │                                            │   │
   ═════╪══ RESAMPLE to fs(t) ∈ [20, 82.5] kHz ══════╪═══╪═════
        │                                            │   │
        ├─ quantise to 12 bits (converter model, §4.3)│  │
        ├─ circular buffer, fixed N = 1024/4096/16384 │  │
        │     write and read both at fs(t)            │  │
        ├─ HOLD: inhibit writes                       │  │
        │                                            │   │
   ═════╪══ RESAMPLE back to host rate ══════════════╪═══╪═════
        │                                            │   │
        ├─ zero-order-hold shaping (DAC staircase)    │   │
        ├─ NE570 expander: independent detector,      │   │
        │     τ 6.8 ms — deliberately NOT the inverse │   │
        ├─ reconstruction LPF: 2 SK biquads           │   │
        │     (8.84k Q0.75 / 15.8k Q3.37)             │   │
        └─┬──────────────────────────────────────────┘   │
          ├─ F.BACK pot (50 kB) ───────────────────────► loop
          └─ E.LEVEL pot (50 kB) ─ 33K ─┐                │
                                        ├─ mixer (33K/33K, 47K/10K de-emphasis shelf)
   dry ─ 33K ──────────────────────────┘                │
                                        └─────────────► out
```

### 4.2 The variable-rate core

* **fs(t) is a smooth control signal**, not a parameter. Drive it from the D.TIME pot through the
  10 kA log taper and the VCO's control law, with a slew that matches the physical R/C at the VCO
  control node (`R56 22K`, `C52 0.027 µF`, plus the volume-board network). Getting that slew right
  is what makes knob sweeps sound like the pedal instead of like a modulated delay.
* **Resample in and out with a fractional-delay interpolator good enough to be transparent** —
  the interpolation is *your* artefact, not the pedal's, so it must not colour anything. Polyphase
  windowed-sinc or a high-order Lagrange. This is the one place to spend cycles freely.
* **Fixed N per range.** Never vary buffer length. Range switching changes N (and therefore
  discontinuously changes delay time), which is what the real pedal does too.
* **HOLD is a write-inhibit on the L-range buffer.** One boolean.

### 4.3 Where the nonlinearities live

Ranked by how much they matter:

1. **The NE570 pair.** Two independent envelope followers, each τ = 6.8 ms, each with a real
   rectifier that leaves 23.4 Hz-corner ripple on the control voltage, feeding a gain cell whose
   ripple-induced distortion is third-harmonic. Critically: **the expander is open-loop.** It does
   not know what the compressor did — it tracks the *reconstructed, quantised, band-limited* signal.
   Model them as two separate blocks that happen to be roughly inverse, never as an exact
   cancelling pair. The residual mismatch is the pedal's voice.
2. **12-bit quantisation, applied once per pass through the loop.** Plus R-2R ladder DNL — a cheap
   10 k network has real integral and differential nonlinearity, and that is part of the grain.
   Model as a static nonlinear transfer on the code word plus a per-code offset table; calibrate
   the table from a measured low-level sine staircase.
3. **The converter's clip behaviour.** Hard rail at full scale. Whether it wraps or saturates is
   unknown — implement it switchable and pick by measurement.
4. **The JFET/BJT input buffer.** Mild, mostly second-harmonic, at high input levels only.
5. **The op-amps.** The DD-2/3's NJM5534 is a plausible contributor to the "grainier" reputation
   versus the DD-3A's µPC4072, but this is a listening-report inference, confidence C. Model as
   linear with a slew limit; revisit only if measurement demands it.

### 4.4 What can be linear, and what needs oversampling

**Linear, no oversampling needed:** the pre/de-emphasis shelves, the mixer, the pots, the dry path,
the DIRECT-jack branch, the mute switching. All of these are gentle first-order networks well below
any rate concern.

**Needs oversampling (×4 minimum, ×8 preferred):** everything between the pre-emphasis node and the
reconstruction filter output. Two reasons — the compander's gain cell generates harmonics that must
not fold, and the anti-alias filter's section 3 (Q 4.52 at 8 kHz) plus the reconstruction filter's
section 2 (Q 3.37 at 15.8 kHz) are high-Q states whose behaviour degrades badly at host rate. Run
the whole analog block oversampled and decimate once at the output.

**Needs care but not oversampling:** the fs(t) resampler itself, which should be transparent by
construction.

**Do not oversample:** the 12-bit quantiser or the buffer. They live at fs(t) by definition; that
is the entire point.

### 4.5 The feedback loop

Close it **in the analog domain around the whole chain**, at the pre-emphasis summing node.
Everything inside the loop — both 7 kHz filters, both compander halves, the converter, the buffer
— runs once per repeat. This is not an optimisation target; it is the model's central claim, and
it is what separates this from every "digital delay with a lowpass in the feedback path".

Consequence for the implementation: the loop has real latency (the delay line) so it is not an
algebraic loop, but the oversampled analog blocks inside it mean the per-sample cost is
substantial. Budget accordingly — this is not a cheap effect, and it should not be.

### 4.6 Ship one model for DD-2 and DD-3 v1

Boss says they are electrically identical. Treat the DD-3A as a variant with the four enumerated
deltas plus an unknown gate-array timing difference, and be honest in the copy that the DD-3A
variant is less well characterised.

### 4.7 Measurements needed from a real unit to calibrate

None of this has been done by anyone, so all of it is original work. In priority order:

1. **Wet-path swept sine and impulse, at both clock extremes in each range.** Use the documented
   STEREO trick: plugging DIRECT and engaging the footswitch puts the **wet signal alone** on MONO
   (per the service note's own switching table). Capture at 192 kHz. Gives: the true anti-alias +
   reconstruction cascade response and phase; the actual N per range (N = fs·τ should come out
   constant); confirmation or refutation of the fixed-bandwidth claim.
2. **THD+N versus input level**, ~20 points from −60 to 0 dBFS-equivalent, wet only, F.BACK zero.
   Exposes the compander knee, the gain-cell distortion, and the converter clip point.
3. **Low-level sine, wet only.** Exposes the quantisation staircase and the R-2R ladder's DNL
   directly. Build the per-code offset table from this.
4. **Tone bursts at several levels**, measuring the wet envelope. Confirms τ = 6.8 ms and reveals
   whether attack and release really are symmetric in situ.
5. **F.BACK sweep to and past self-oscillation.** Record the spectrum of the tail at 5–10 feedback
   settings, and the per-pass band-limiting. This is the loop's signature and the hardest thing to
   get right by construction.
6. **Scope MSCK and SAH directly** across the D.TIME range. Settles the 5.18/5.28 MHz question and
   gives the actual VCO control law versus knob position, including the log taper.
7. **Continuity check on the F.BACK/E.LEVEL takeoff** — pre or post reconstruction filter (§1.6).
   Ten minutes with a meter; resolves the one structural ambiguity in the whole circuit.
8. **Photograph the board** at high resolution to resolve `R40` and confirm the pre-emphasis leg
   sign (§1.2).

---

## 5. Open questions

**Blocking — these change the architecture**

1. **Is the F.BACK/E.LEVEL takeoff before or after the reconstruction filter?** Decides whether the
   feedback loop contains the 4th-order filter once or zero times per pass. Reading (a), post-filter,
   is near-certain electrically but was not actually read off the scan. §1.6.
2. **Is the pre-emphasis network a treble boost (R8+C4 in the input leg) or a cut (in the feedback
   leg)?** Companding theory says boost. Confirm from the scan. §1.2.
3. **Does the 12-bit SAR wrap or saturate on overflow?** Determines what happens on transients that
   outrun the compressor — the loudest, most audible failure mode in the whole circuit.

**Structural, resolvable by measurement**

4. Are the sample counts really 1024 / 4096 / 16384, or 1000 / 4000 / 16000, or something else?
   Derived, never printed.
5. What is the LSI's actual SAR settling behaviour and DAC output hold/glitch shape? Is there any
   internal dithering, oversampling or interpolation? Unknowable from paper.
6. What does the R-2R ladder's DNL actually look like, and how much of "the grain" is it versus the
   compander?
7. Is the bandwidth genuinely invariant across range and time setting, as the spec claims and the
   topology predicts? Nobody has ever plotted it.
8. What does self-oscillation actually sound like, spectrally, and where does the NE570 pair
   stabilise it?

**Documentation gaps**

9. `R40` — 3.3K or 2.7K, and is it one reference or two? §1.5.
10. MSCK at the fast end: 5.18 MHz as printed, or 5.28 MHz as the ×64 arithmetic and the DSD-2/3
    notes both suggest? §1.7.
11. Is there an undocumented DD-3B board respin between 2001 and 2014? No evidence either way.
12. Serial-number cutoff for the long→short transition. Only DD-3B's SO91200 is documented.
13. **bossareaforum.com (expired TLS cert), freestompboxes.org (403) and thegearpage (402) could
    not be read programmatically.** These are the most likely places a cleaner schematic trace or
    an actual measurement is hiding. Worth opening by hand in a browser.
14. Roland **SDE-2000** service manual — could not be found at all. If it exists it is another
    RDD63H101 data point.

**Scope**

15. Is the DD-3A worth shipping as a variant at all, given that its gate array is undocumented and
    the audible difference is reported as small? Decide after the DD-2 model measures well.
