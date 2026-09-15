# Yamaha SPX90 — Design-Ready Dossier

Compiled 2026-08-26. Supersedes nothing; **read together with**
`2026-08-26-yamaha-spx90.md`, which holds the preset/parameter tables and the prior-art survey.
This file adds hardware detail from the service manual, the patent record, the measurement
situation, and a re-verification of the thrash-usage premise.

Confidence key: **A** = Yamaha service/owner's manual or a primary datasheet/patent document.
**B** = credible secondary source, corroborated or internally consistent with A.
**C** = single unverified source, forum lore, or my own derivation. Anything not marked is A.

**Headline correction to the earlier note.** The prior dossier said the SPX90's DSP microcode is
mask ROM inside the YM3804 and therefore unobtainable. That is **wrong**. The service-manual parts
list puts the DSP program in **eight external TBP28L22N bipolar PROMs** labelled "DSP V1.0". These
are standard, readable, socketed-era parts. The algorithm is in principle *recoverable*, not merely
inferrable. See §1.4 — this is the single most decision-relevant finding in this document.

---

## 1. Hardware chain

### 1.1 Service manual availability

The SPX90 service manual is public and complete: specifications, block diagram, panel layout,
**Room Size Chart**, signal flow, preset program list, circuit-board layout, checks & adjustments,
test program, MIDI data format, **LSI Data Table (pin-by-pin for YM3804 and YM3807)**, circuit
boards, parts list, and full analog + digital schematics.

| Copy | URL | Note |
|---|---|---|
| archive.org (OCR text, ~185 KB) | <https://archive.org/stream/yamaha_yamaha_spx90/yamaha_spx90_djvu.txt> | The one actually parsed for this document |
| Elektrotanya, full SM | <https://elektrotanya.com/yamaha_spx90_sm.pdf/download.html> | HTTP 200, not visually inspected |
| Elektrotanya, schematic only | <https://elektrotanya.com/yamaha_spx90_r_sch.pdf/download.html> | |
| audioservicemanuals.com | <https://www.audioservicemanuals.com/y/yamaha/yamaha-sp/785323-yamaha-spx90-service-manual> | 39 pages, no login |
| synthxl | <https://www.synthxl.com/yamaha-spx-90/> | |
| **SPX90II** SM | <https://elektrotanya.com/yamaha_spx90ii_multi-effect_processor_sm.pdf/download.html> | HTTP 200; contents not inspected. Not on archive.org. |

Host-CPU firmware ROM images for both SPX90 (`Yamaha_SPX90_ROM_V1.1_XB410B0_IC151_TMM24256AP`)
and SPX90II circulate in the archive.org item `httpsdbwbp.com` ("EPROM Pack 90s Vintage Synths"):
<https://archive.org/details/httpsdbwbp.com>. Confidence B — listing seen, images not verified.

**Caveat on the OCR.** Quantity columns in the parts list are unreliable (it reports 21 x
TC5565L-12 SRAM, which is implausible for an 8 KB-class control CPU). Trust part *numbers*, not
counts, until someone opens the PDF visually. Schematic pages are images, so no component values
survived OCR — all R/C values below are absent for that reason, not because they don't exist.

### 1.2 Signal chain

```
IN (1/4" unbal, 10 kΩ, −20 dBm / +4 dBm switch, up to +12 dB gain)
 │
 ├──────────────────────── BYPASS path (analog, 20 Hz – 20 kHz, THD < 0.01 %)
 │
 └─ input amp (NJM4558DV / NJM4556 / M5238P class)
    └─ anti-alias LPF — PFB-4 hybrid filter module (LPF101/102/103)
       └─ sample & hold (TC4053BP analog switch)
          └─ SAR ADC: HD14549B + HD14559BP successive-approximation registers,
             comparator, **feeding the PCM54HP back as the ladder**   ← 16-bit linear, 1 ch
             │
        ┌────┴──────────────────────────────────────────────┐
        │  YM3804 "DSP"  (64-pin shrink DIP)                │
        │   • 64 instructions per sample @ 31.25 kHz  [B]   │
        │   • MDAT0–23  → 24-bit external memory word       │
        │   • MADR0–7   → 8-bit multiplexed row/col addr    │
        │   • RAS/CAS/OT/WT/REF DRAM control                │
        │   • SI0/SI1 serial audio in, SO0/SO1 out          │
        │   • MOD0–7 parallel modulation input  ←───────┐   │
        │   • CDI/CDO serial command in/out (cascadable)│   │
        └───────────────┬───────────────────────────────┼───┘
                        │                               │
             10 × M5M4416 DRAM (64 Kbit, 16 K × 4)   YM3807 "MOD"
             delay memory, ~16 kWords ≈ 524 ms       (24-pin, ~16 LFOs,
                                                      arbitrary waveforms,
                                                      MD0–7 / MDSO0-1 out)
                        │
             8 × TBP28L22N bipolar PROM = **DSP microcode, "DSP V1.0"**
                        │
        └─ D/A: PCM54HP (Burr-Brown 16-bit monolithic), 2 ch
           └─ reconstruction LPF (PFB-4 module)
              └─ output amp → 2 × unbal out, 600 Ω, −20 dBm / +4 dBm

Control: HD6303RP (Hitachi 8-bit CPU) + HD6350P ACIA (MIDI); TMM24256 EPROM at IC151;
         TC5565L SRAM for the 60 user programs. CPU loads coefficients into the DSP over CDI.
Clock:   X101, 4 MHz ceramic resonator (the only clock part in the list).
Power:   NJM7815A / NJM7915A ±15 V analog rails.
```

Source for all silicon part numbers and pin names: the archive.org service-manual text above.
Confidence A for part numbers and pin lists; B for the 64-instruction and 16-LFO figures, which
come from acreil on Gearspace
(<https://gearspace.com/board/electronic-music-instruments-and-electronic-music-production/1044381-yamaha-fx-units-ym3413.html>)
and are not stated in the manual.

### 1.3 Facts and the things that follow from them

| Parameter | Value | Conf. |
|---|---|---|
| Sample rate | 31.25 kHz, SPX90 **and** SPX90II | A |
| Word length | linear 16-bit, 1 ch in / 2 ch out | A |
| Master clock | 4 MHz ceramic resonator; 4 MHz / 128 = 31.25 kHz **exactly** | A (part) / C (divider chain) |
| Effect bandwidth | 20 Hz – 12 kHz | A |
| Bypass bandwidth | 20 Hz – 20 kHz | A |
| Output LPF check spec | cut-off **12.7 kHz**, −6 dB/oct verification point | A |
| Dynamic range | > 75 dB reverb, > 81 dB delay | A |
| THD | < 0.01 % bypass, < 0.03 % effect | A |
| DSP external memory word | **24 bits** (MDAT0–23) | A |
| DSP address bus | 8-bit multiplexed → 64 K addressable words | A |
| Delay RAM | 10 × M5M4416; ~16 kWords ≈ 524 ms at 31.25 kHz | A (parts) / B (kWord figure) |
| ADC | discrete SAR, reusing the PCM54HP as its ladder | A |
| ER tap count | 13 taps per channel | C (acreil only) |
| Instruction budget | 64 per sample ≈ 2.0 MHz instruction rate | B |

Three consequences worth stating explicitly:

**The ADC and DAC share one converter.** The digital schematic labels a "Successive Approximation
Register" with ADCK/ADST/ADLD control, an HD14549B/HD14559BP SAR pair, a TC4053BP sample-and-hold,
and no monolithic ADC part anywhere in the list. The PCM54HP is in the SAR loop. So the input and
output converters have **identical** transfer characteristics, DNL and gain error — a coupling a
model can exploit (one converter model, used twice) and a real unit's calibration will show.
Confidence A on the topology from the parts list and signal names; **B** on "the PCM54HP is the
ladder" specifically, since the schematic page is an image and I am reading the net names, not the
traces. Verify visually before relying on it.

**The 24-bit memory word contradicts "16-bit delay memory".** Audio is 16-bit at the converters,
but the delay RAM is written 24 bits wide. Either the DSP stores extended-precision samples in the
delay line (very plausible — it removes the biggest source of tail quantisation grain and is
consistent with the >75 dB reverb figure), or it packs sample-plus-tag. This directly undermines
the earlier note's recommendation to model truncation to 16 bits inside the feedback loops.
**Unresolved. Do not assume 16-bit-in-the-loop until measured or until the microcode is read.**

**10 × 16K×4 = 40 bits wide, not 24.** Ten M5M4416 do not divide evenly into a 24-bit bus.
Possible: two banks (6 chips = 24 bits, 4 spare/second bank), or a wider physical word with only
24 lines to the DSP, or the OCR count is wrong. **Unresolved; needs the board layout page.**

**The filters are hybrid modules, not discrete networks.** "PFB-4", designators LPF101/102/103.
No transfer function is published. The only numeric handle is the adjustment spec's 12.7 kHz
−6 dB/oct check point, which against a 15.625 kHz Nyquist implies a steep multi-pole brickwall.
The check point does not tell you the order. **Must be measured.**

### 1.4 The microcode is external — the biggest lever in this project

`TBP28L22N` is a **256-bit (32 × 8) bipolar PROM**, eight of them, annotated "DSP V1.0" in the
parts list. That is a 32-word × 64-bit control store: 32 instruction slots of 64 bits, or — read
against acreil's "64 instructions per sample" — plausibly a 64 × 32-bit store across the eight
devices. Either way it is **64 instruction-cycles' worth of horizontal microcode sitting in
readable, standard, socketed parts on the board.**

What that means:

* The YM3804 is a datapath, not a closed algorithm. The *reverb topology itself* is in the PROMs.
* Reading eight 28L22s is a trivial bench job (any cheap universal programmer; they are 5 V TTL,
  no protection, no security fuse).
* Recovering the *encoding* is the hard part, and it is exactly the problem that the SPX90 LSI
  Data Table plus the YM3413 reverse-engineering work (below) makes tractable rather than hopeless.
* If this succeeds, the SPX90 model stops being "fit an IR grid and hope" and becomes a
  transcription. That is a categorically different accuracy claim.

Adjacent work that lowers the risk: the YM3413 ("LDSP", the later repackaging of this same DSP
family) has been partly decoded by the VGMRips community — 40-pin pinout, 16-bit data bus,
A0–A16, 32 clocks per stereo sample, partly-decoded command format, 8-bit signed coefficients,
32 delay addresses per bank: <https://vgmrips.net/forum/viewtopic.php?t=5745>, with a skeleton
core at <https://github.com/michelgerritse/TritonCore/blob/master/Devices/Sound/DSP/YM3413.cpp>.
Confidence B (community reverse engineering, self-consistent, no vendor document).

**Negatives, verified.** No YM3804 or YM3807 datasheet exists publicly — a 2024 GroupDIY request
for the YM3807 datasheet went unanswered
(<https://groupdiy.com/threads/looking-for-a-yamaha-chip-datasheet-ym3807.89328/>). No published
28L22 dump from an SPX90. No MAME driver, no emulator, no open-source reimplementation for
YM3804/YM3807/YM3413 beyond the skeleton above. The internal arithmetic width of the YM3804 is
undocumented; only the 24-bit external word is known.

Third-party chip list, for cross-reference only (confidence C):
<http://www.dtech.lv/techarticles_yamaha_chips.html> — "YM3804 DSP in 64-pin shrink DIP, uses
DRAM, supports up to 192 kbytes".

---

## 2. Presets and which ones are structurally distinct engines

The full 30-preset table with names, engine types and per-engine parameter ranges is in
`2026-08-26-yamaha-spx90.md` §2.1–2.2, taken verbatim from the owner's manual
(<https://archive.org/stream/SPX-90_owners_manual/SPX-90_owners_manual_djvu.txt>) and not
duplicated here. What matters for architecture is the engine-type grouping, because Yamaha's own
type column tells you how many distinct DSP structures you actually have to build.

| Engine type | Programs | Structure | Distinct? |
|---|---|---|---|
| **REV** | 1 HALL, 2 ROOM, 3 VOCAL, 4 PLATE | Recirculating: ER section + comb/allpass tail. Params: R/T 0.3–99 s, HIGH 0.1–1.0, pre-delay 0.1–**50** ms, HPF, LPF | **Yes** — the only recirculating reverb |
| **E/R 1**, **E/R 2** | 5, 6, **16 GATE REVERB**, **17 REVERSE GATE** | Finite tapped-delay FIR. TYPE ∈ {HALL, RANDOM, PLATE, REVERSE}; ROOM SIZE 1.0–20.0; LIVENESS 0–10; DELAY 0.1–400 ms; LPF | **Yes** — FIR, no feedback |
| **DELAY / ECHO** | 7, 8 | Two independent delay lines, feedback −99…+99 % (note the *negative* polarity range), one-pole HF damping in the loop | Yes, but trivial |
| **MOD** | 9–15 | Modulated short delay + feedback, YM3807 driving read addresses | Yes |
| **PITCH** | 21–24 | Time-varying resample with crossfade | Yes |
| **GATE / R&G** | 18, 19, 20 | Envelope dynamics; 20 = REV feeding a gate | Reuses REV |
| **FREEZE / PAN / VIB / PEQ** | 25–30 | Sampler / amplitude LFO / delay LFO / biquad chain | Yes, all simple |

**Four programs' engines account for the SPX90's reputation, and only two of them are hard.**
GATE REVERB (16) and REVERSE GATE (17) are typed **E/R 2** by Yamaha — they are early-reflection
tap patterns with shaped envelopes, *not* a reverb tail through a gate. They are FIR, therefore
LTI at fixed parameters, therefore exactly recoverable by deconvolution. Modelling them as
"reverb + gate" will never null. Confidence A (manual's type column), independently corroborated
by the Vintage Digital review's description of the ER programs as first-reflection cluster only
(<https://www.vintagedigital.com.au/product-review/yamaha-spx90-review/>) and by a player's
reconstruction as "pre-delay plus about three taps, no tail, no feedback"
(<https://forum.fractalaudio.com/threads/spx-90-early-reflections-reverse-shoegaze-classic-block.212534/>,
confidence C).

The only genuinely hard engine is **REV**, because it recirculates and its coefficient set is in
the PROMs. Everything else is either FIR (solvable exactly) or structurally obvious.

**SPX90II** is the same engine with more delay RAM: delays and Freeze to ~2 s, pitch to ±2 octaves
at 1-cent resolution, same 31.25 kHz / 16-bit, same 30 preset names.
<https://www.vintagedigital.com.au/yamaha-spx90ii-multi-effects-processor/>

**The Room Size Chart in the service manual is a direct calibration table** for the ER programs
and has not yet been transcribed. It should be, before any tap-fitting work begins.

---

## 3. Reverb topology from the Yamaha patent record

None of these names the SPX90. They are the design school it came out of, from the same company,
the same years, and — in one case — with the SPX90's own DSP part number in the text. Structural
evidence, not a spec. All verified by fetching the patent document.

| Patent | Title | Assignee | Filed → Granted | Priority | Inventors |
|---|---|---|---|---|---|
| [US4570523](https://patents.google.com/patent/US4570523A/en) | Reverberation tone generating apparatus | Nippon Gakki | 1984-09-25 → 1986-02-18 | 1981-07-30 | Futamase, Kato |
| [US4586417](https://patents.google.com/patent/US4586417A/en) | Electronic musical instruments provided with reverberation tone generating apparatus | Nippon Gakki | 1984-10-05 → 1986-05-06 | 1981-07-28 | Kato, Futamase |
| [US4706291](https://patents.google.com/patent/US4706291A/en) | Reverberation imparting device | Nippon Gakki | 1986-06-23 → 1987-11-10 | 1985-06-25 | Kakubo, Niimi, Ikegaya |
| [US4731835](https://patents.google.com/patent/US4731835A/en) | Reverberation tone generating apparatus | Nippon Gakki | 1987-03-10 → 1988-03-15 | 1984-11-19 | Futamase, Kato |
| [US4803731](https://patents.google.com/patent/US4803731A/en) | Reverberation imparting device | Yamaha | 1987-08-24 → 1989-02-07 | **1983-08-31** | Niimi, Kakubo, Ikegaya |
| [US5025472](https://patents.google.com/patent/US5025472A/en) | Reverberation imparting device | Yamaha | 1988-05-25 → 1991-06-18 | 1987-05-27 | Shimizu, Kawakami |
| [US5027687](https://patents.google.com/patent/US5027687A/en) | Sound field control device | Yamaha | 1989-10-05 → 1991-07-02 | 1987-01-27 | Iwamatsu |
| [US5195140](https://www.freepatentsonline.com/5195140.html) | Acoustic signal processing apparatus | Yamaha | 1990-12-28 → 1993-03-16 | 1990-01-05 | — |
| [US5272274](https://patents.google.com/patent/US5272274A/en) | Electronic musical instrument with reverberation effect | Yamaha | 1992-05-04 → 1993-12-21 | 1989-08-10 | Kimura |
| [US5553150](https://www.freepatentsonline.com/5553150.html) | Reverberation-imparting device capable of modulating an input signal by random numbers | Yamaha | 1994-08-10 → 1996-09-03 | — | — |

### The three that matter

**US4570523 / US4586417 / US4731835 — the Kato–Futamase family.** The closest thing to a canonical
Yamaha reverb block diagram in the public record:

```
in ─ bandpass (cascaded LPF + HPF, coeffs K11–K16)
   ├─ tapped delay line, 10 taps i₁…i₁₀, gains K₁…K₁₀, mults M1–M10 ─ sum ─ early reflections
   └─ 3 parallel comb filters (2A/2B/2C, delay times chosen to flatten composite magnitude)
      └─ 3 series allpass filters (3A/3B/3C) — densify echoes at flat magnitude
```

All of it on **shared digital memory**, not discrete delay lines: sections D0–D15 of 2048 words,
MD0–MD15 of 512 words, SD0–SD15 single-word, driven by one time-division-multiplexed address
generator. Read address is computed, never physically tapped:
`ADR(t−i) = ADR(t) ± ΔADR`, with ΔADR derived from a stored delay-time parameter DLDₘ(n). Decay is
by feedback coefficient only — **there is no damping filter inside the comb loops**; spectral
shaping lives in the input bandpass. That last point is a real, testable structural claim, and it
is the opposite of what a Dattorro/Griesinger-lineage model does.

**US4803731 — the shared-RAM addressing and grouped-tap patent (priority 1983-08-31, REV1 era).**
One delay memory, written circularly by a counter; every read address is `write_address −
delay_value` from a **subtractor**. Roughly forty taps, **grouped into clusters** (worked example:
40 taps in 3 groups); each group's weighted taps are summed by its own adder, and **each group sum
passes through a single first-order low-pass filter** (two coefficients A and B). That is
frequency-dependent decay whose HF loss grows with delay time — air and wall absorption — at
1/13th the cost of per-tap filtering. A **single room-size multiplier** scales all delay values
proportionally, expanding the whole echo pattern from one knob.

This is almost certainly the mechanism behind the SPX90's ROOM SIZE (1.0–20.0) and LIVENESS (0–10)
parameters, and it explains why the SPX90 has no separate diffusion control: liveness is the group
LPF coefficient, room size is the global delay multiplier. **Inference, confidence C** — the
patent does not name the SPX90 — but it is a strong, cheap, falsifiable hypothesis: if room size
is a pure proportional scaling, the Surefyre IR grid's ER tap positions should scale linearly
across the 20 room-size settings. That is a one-afternoon test against data that already exists.

**US4706291 — convolution, no recursion.** A "prememory" holds the freshest samples; a larger data
memory holds delayed copies. A **fixed-type convolution circuit** reads (τᵢ, gᵢ) pairs for i=1…k
from parameter memory against the prememory — the ER cluster, zero added latency. An
**adaptive-type convolution circuit** handles τ_{k+1}…τ_n against the main memory for the late
field. No comb or allpass loops at all; the impulse response is a sparse enumerated tap list.
Later embodiments add time-varying frequency shaping by **crossfading between two filter channels
A and B**, which is how they change response without zipper noise.

### Supporting three

* **US5027687** names the **YM3804** in its text and enumerates that generation's user parameters:
  room size 0.1–8.0, liveness, initial delay 5–150 ms, reverb time 0.3–8.6 s, reverb level,
  16 factory programs. Confirms the parameter vocabulary is a chip-family property.
* **US5195140** is the clearest late-era topology statement:
  `in → FIR initial reflections → allpass → 4 parallel IIR combs → weighted mix → stereo`.
  The invention proper is limit-cycle cancellation: the parallel combs generate *correlated*
  limit-cycle DC offsets, so the mixing coefficients are chosen to null the summed offset. Read
  this before implementing fixed-point comb loops.
* **US5272274** puts ER through ten multipliers K, splits into three parallel frequency branches
  (HP/BP/LP, input coefficients K11/K15/K23) so spectral tilt comes from branch balance, then
  three parallel comb reverberators followed by three series allpasses.

**US5553150** is outside the window (1994) but is the read-address modulation patent, and it
matters because it names the house topology of the period as "initial reflections + comb filters
with damping LPFs + allpass filters" and adds RNG modulation of read pointers with **two taps
cross-faded by complementary rising/falling envelope coefficients** to avoid clicks. If the
YM3807's read-address modulation in the SPX90 uses a crossfade rather than plain integer jumps,
this is the mechanism — and it would contradict the earlier note's guess that the SPX90 modulates
without interpolation. **Unresolved; audible on a measurement of the modulated programs.**

### Numbers checked and rejected

Do not chase these: **US4694724** (Roland, MIDI/SMPTE sync), **US4649783** (Stanford,
Karplus–Strong), **US4726276** (Nippon Gakki but portamento crossfade, not reverb), **US4998281**
(Casio, and it *is* a four-comb reverb patent — right structure, wrong company), **US5170369**
(E-mu/Rossum, dynamic IIR). Also rejected: US4653101, US4653096, US4628789, US4701956.

**Search-method gap, stated honestly:** the citation graph ("Cited By" / "Similar Documents") does
not survive Google Patents' markdown conversion, so expansion was done by assignee and keyword
search only. Additional mid-80s Nippon Gakki reverb filings probably exist that only the citation
network would surface.

---

## 4. Measurement data and the limits of IR matching

### 4.1 What exists (verified live, Aug 2026)

| Pack | Contents | Format | Price / terms |
|---|---|---|---|
| **[Surefyre SPX90ii](https://impulse.surefyre.com/impulse-packs/yamaha-spx90ii-impulse-pack/)** | **>1100 IRs, 2.2 GB.** Hall/Plate/Room/Vocal × damping {0.1, 0.5, 1.0} × 27 reverb lengths (0.3–99 s); ER1/ER2 × {Hall, Plate, Reverse, Random} × liveness {0,2,5,8,10} × 20 room sizes; **plus a noise profile for input correction** | recorded 24-bit/44.1 k, deconvolved to **32-bit float 44.1 kHz** | £10, PayPal, 24-hour download link, no refunds. **No licence text on the page.** |
| [Vintage Digital SPX90](https://www.vintagedigital.com.au/product/yamaha-spx90-reverb-impulse-responses/) | presets 1–6, 16, 20 | **Altiverb format only** | $11.99, terms not stated |
| [Sound Energy](https://soundenergy.gumroad.com/l/spx90) | 9 stereo IRs + PDF of factory parameters | 24-bit/48 kHz WAV | Gumroad, low single-figure €, terms not stated |
| [Millertone](http://www.millertone.com/free.html) | 9 IRs + Logic Space Designer presets, 4.1 MB zip | WAV, rate not stated | free, terms not stated |

No new packs since. The KVR 2008 free SPX90II pack link is dead. Samplicity/Voxengo have no SPX90
content. **No academic paper, blog measurement, published spectrogram or frequency-response plot
of the SPX90 exists** — searched specifically; this is a verified negative. The only numeric data
in the world are the manual's specs, the service manual's 12.7 kHz adjustment point, and
Surefyre's noise profile.

**Licence risk is real and unresolved.** Not one of the four packs states redistribution or
derived-work terms. Using Surefyre's grid to *fit coefficients* for an algorithmic model is a
different act from shipping their IRs, but it is not a settled one. Get written permission before
any of it touches a shipped product, or capture your own grid.

### 4.2 What a calibration workflow looks like

Surefyre's grid is not just a pile of IRs — it is a **pre-built parameter sweep**, which turns
coefficient fitting from guesswork into an optimisation with data already on disk:

1. **ER programs first.** They are FIR and LTI. Deconvolve each ER IR to recover literal tap
   positions and gains. With 20 room sizes × 5 liveness values you can then test the US4803731
   hypothesis directly: does room size scale all tap times by a single multiplier, and does
   liveness move one first-order LPF coefficient per tap group? If yes, you have the *parameter
   law*, not just 100 stored IRs, and the model interpolates correctly at settings Surefyre
   never captured.
2. **Tap count.** acreil says 13 taps per channel for the SPX90 ER algorithms (confidence C,
   single source). The deconvolution settles it in minutes. Do this early — it is a cheap,
   high-information check on the whole "64 ops/sample" story.
3. **REV programs.** Fit RT60 per octave band across the 27 reverb-time settings × 3 damping
   settings. Recover modal density from the tail's spectral flatness. Fit a comb/allpass set to
   the measured RT60(f) curve and echo density, constrained to the Kato–Futamase topology
   (3 parallel combs → 3 series allpasses, no in-loop damping) rather than to a Dattorro tank.
4. **Noise floor as a first-class target.** Surefyre shipped a noise profile. The published
   >75 dB (reverb) vs >81 dB (delay) delta is a direct measurement of what the recirculating loop
   adds; match that ~6 dB delta and the tail noise *spectrum shape*, not just its level.
5. **Analog I/O.** Sweep in bypass and in a null-effect program; the difference is the AA +
   reconstruction filter pair plus the input/output amps. Fit magnitude *and* phase.

### 4.3 Where IR matching stops working, and why it matters here

An IR is a complete description of an LTI system and nothing else. The SPX90 is not LTI in the
places that give it its character:

* **YM3807 read-address modulation.** Any tail modulation makes the system time-varying. An IR
  captures one frozen instant of LFO phase. This is why IR-based SPX90 "emulations" sound static.
* **PITCH CHANGE, MOD programs, PAN, VIB, FREEZE.** Inherently time-varying. IR is inapplicable,
  not merely inaccurate.
* **GATE, COMPRESSOR, REVERB & GATE.** Level-dependent by definition.
* **Fixed-point truncation in the recirculating loops.** Signal-dependent and *nonlinear*. A
  convolution reproduces the average behaviour of the tail and none of its granular texture —
  exactly the regime where a loud snare lives, and exactly what people mean when they say the
  SPX90 sounds the way it does. Note that the 24-bit memory word (§1.3) means this may be
  *less* granular than the earlier note assumed. Measure before modelling.
* **Converter nonlinearity.** The shared PCM54HP's DNL applies twice, on the way in and out.

So: IRs give you exact answers for ER1, ER2, GATE REVERB, REVERSE GATE, DELAY and ECHO at zero
feedback, and PARAMETRIC EQ. They give you good approximations for REV 1–4 at moderate level. They
give you nothing for roughly a third of the box.

### 4.4 Prior modelling work — updated

Still **no reverb** emulation of the SPX90 at any level. Two new Symphonic-only plugins appeared
since the last note, both attacking the modulation engine and neither the reverb:

* **Neyrinck V-90 Symphonic** (Jan 2024, $49.99) — Symphonic preset only, LFO scheme claimed
  "decoded bit-by-bit", runs natively at 31.25 kHz. <https://app.neyrinck.com/symphonic>,
  <https://www.macosaudio.com/2024/01/neyrinck-audio-conjures-v-90-symphonic-spx90-emulation/>
* **Rave Generation "Symphonic"** (~Apr 2026, €19.95) — claims an "SPX90II LDSP chip emulator"
  running 7 modulation algorithms at 31.25 kHz. <https://shop.re-style.nl/products/rave-generation-symphonic>;
  manual repo tagged `ym3804` at <https://github.com/RaveGeneration/Symphonic> (PDF only, no
  source). The chip-emulation claim is unverifiable marketing.
* **Witch Pig SPX Nevermore** (free, 2023) — Symphonic, inspired-by. Its page calls the SPX90
  "12 bit"; that is **wrong** — Yamaha's spec says linear 16-bit and the DAC is a 16-bit PCM54HP.

That two commercial products both chose Symphonic, both cite 31.25 kHz, and neither touched the
reverb is itself informative: the modulation engine is reachable by inspection, the reverb is not.
The reverb niche is still open, and §1.4 is the reason it need not stay closed.

---

## 5. Thrash-metal usage: re-verified, and the answer is no

This was searched again, independently and specifically, across Tape Op, Mix, Sound On Sound,
MusicRadar, Premier Guitar, Louder and the muzines 1985–1992 magazine archive.

**Zero documented instances of an SPX90 on any record by Metallica, Slayer, Megadeth, Exodus,
Testament or Anthrax.** Not one of the named engineers mentions the unit in any source reachable.

Sources checked that specifically do *not* contain an SPX90 mention:
[Perialas/Tape Op](https://tapeop.com/interviews/128/caught-mosh-alex-perialas),
[Wallace/Tape Op](https://tapeop.com/interviews/25/andy-wallace),
[Rasmussen/Tape Op](https://tapeop.com/interviews/172/flemming-rasmussen-riding-the-sweet-silence),
[Burns/Tape Op](https://tapeop.com/interviews/173/scott-burns),
[Mix, *Master of Puppets*](https://www.mixonline.com/recording/classic-track-master-of-puppets-metallica),
[MusicRadar, Rasmussen](https://www.musicradar.com/artists/producers-engineers/all-the-albums-i-did-with-metallica-were-recorded-on-24-track-analogue-tape-theres-not-a-computer-in-sight-how-flemming-rasmussen-produced-metallicas-classic-master-of-puppets),
[Premier Guitar, Wagener](https://www.premierguitar.com/gear/reviews/studio-legends-michael-wagener),
[AirGigs, Wagener](https://blog.airgigs.com/2019/01/interview-with-renowned-mixing-engineer-producer-michael-wagener/).
The two period SPX90 pieces in muzines are product reviews with no session content
([SOS Apr 1986](https://www.muzines.co.uk/articles/yamaha-spx90/1392),
[SOS Dec 1991 "Hands On"](https://www.muzines.co.uk/articles/hands-on-yamaha-spx90/7549)).

The single adjacent hit is **not thrash and not verified**: Judas Priest's *Painkiller* (1990),
where engineer Attie Bauw is reported to describe an SPX90 harmoniser on Halford's vocal (~6 cents
down left / 6 cents up right). <https://www.invisibleoranges.com/the-making-of-judas-priest-painkiller>
— **the page returned 403; this came from a search snippet only. Treat as unverified.**

### What IS documented for those records

| Record | Unit(s) named | By whom | Source |
|---|---|---|---|
| Metallica, *Master of Puppets* | **AMS delays**, **Lexicon delays** (no models given), UREI 530 EQ, Orban 622B EQ, LA-4A, Aphex EQ, UREI 1176 | Wagener (mix) and Rasmussen (tracking), quoted directly | [Mix](https://www.mixonline.com/recording/classic-track-master-of-puppets-metallica) |
| Metallica, *…And Justice for All* | Neve 1073-style desk, B&B Audio EQ in the amp loop. **No reverb unit documented anywhere.** | Rasmussen | [Tape Op](https://tapeop.com/interviews/172/flemming-rasmussen-riding-the-sweet-silence) |
| Slayer, *Reign in Blood* | Wallace confirms he engineered/mixed it and names **no hardware at all** | Wallace | [Tape Op](https://tapeop.com/interviews/25/andy-wallace); [Louder making-of](https://www.loudersound.com/features/slayer-the-making-of-reign-in-blood) names no gear either |
| Slayer, *South of Heaven*, *Seasons in the Abyss* | **Nothing.** Credits verifiable, gear not | — | — |
| Anthrax / Testament (Perialas) | TC Booster+ Line Driver, TC Stereo Chorus+ Flanger (used only as a splitter), Valley People 65K automation. **No studio reverb named.** | Perialas | [Tape Op](https://tapeop.com/interviews/128/caught-mosh-alex-perialas) |
| Megadeth, *Countdown to Extinction* | **No units documented.** Method only (click-track/tempo mapping with Menza) | Norman | — |
| Exodus, *Bonded by Blood* / *Fabulous Disaster* | **Nothing documented.** | — | — |
| Morrisound (Burns; death metal, adjacent) | **TC Electronic 2290** delay sampler — big drum sounds; on *World Downfall* he took the 2290s from both rooms, sampled the real kick, triggered live through a gate | Burns | [Tape Op](https://tapeop.com/interviews/173/scott-burns) |

**The one solid period reverb quote, and its scope limit.** Michael Wagener, answering directly on
Gearspace: long vocal reverbs mainly the **Lexicon 224** smooth-hall programs (~1.7–2.2 s, reduced
early reflections); short rooms the **Klark Teknik DN780** (<1 s, ER-emphasised); lead vocal and
snare pre-delay ~15–35 ms, backing vocals shorter or none.
<https://gearspace.com/board/high-end/27332-question-michael-wagener.html> —
**caveat: Gearspace 403'd direct fetch, so this is substance from a snippet, not verbatim.**
More important, Wagener frames it as his **mid-80s Amigo Studios** method and attaches it to
**Bonfire and White Lion — not to any thrash record.** Extending it to *Master of Puppets* is
inference, not documentation.

### Two factual corrections to the brief's premises

* **Wagener did not mix *Peace Sells*** (Paul Lani did the final mix) **or *Rust in Peace***. His
  Megadeth mix credit is *So Far, So Good… So What!*
  <https://en.wikipedia.org/wiki/Peace_Sells..._but_Who%27s_Buying%3F>
* **Perialas did not do *Among the Living*** — that was Anthrax with Eddie Kramer at Quadradial
  and Compass Point. Perialas's records are Testament's *The Legacy* and *The New Order*.
  <https://en.wikipedia.org/wiki/Among_the_Living>

### Explicitly not verified, at any confidence

SPX90 on any of those six bands' records. Any **AMS RMX16**, **AMS DMX 15-80S**, **EMT 140/250**,
**Yamaha REV7**, **Eventide H3000/SP2016**, **Publison**, **Roland SDE-3000**, **Lexicon 480L**,
**PCM70** or **PCM42** on any of them — zero hits across every source reached. Wallace's actual
reverb chain on any Slayer album. Rasmussen's reverbs on anything. No gear-naming interview found
for Rick Rubin, Mark Dodson or Randy Burns in this window.

### What to do with that

Build the SPX90 because it is a specific, characterful, genuinely undermodelled 1985 box whose
12 kHz-bandwidth ER and gate programs sit well in dense guitar music. Do not build it on the belief
that it made the thrash records — that premise has no evidentiary support and would be dishonest
in product copy. If the actual target is the *Puppets* / *Justice* drum sound, the documented path
is **room mics and ambience tracks**, and secondarily sample triggering — not a rack reverb.

Remaining leads if this matters enough to chase: Rasmussen's
[ToneGodz masterclass](https://blabmouth.invalid) (paywalled — the most likely place a chain is
actually stated; URL not verified, find it via blabbermouth.net), David Gehlke's book
*The Scott Burns Sessions*, the
[full-in-bloom Perialas podcast](https://fullinbloom.com/anthrax-producer-engineer-alex-perialas-interview-full-in-bloom-podcast-pyramid-ithaca/),
and Gearspace threads opened manually in a browser (they 403 automated fetches).

---

## 6. Open questions

**Blocking — answer before committing to an architecture**

1. **Can the eight TBP28L22N PROMs be dumped and decoded?** This is the project's fork in the road.
   Everything else is contingency planning. Needs: a unit, a programmer, and the LSI Data Table.
2. **Is the delay memory word really 24 bits of audio, or 16 + 8 of something else?** Determines
   whether tail granularity is a first-order tonal feature or a red herring. §1.3.
3. **How do 10 × 16K×4 DRAMs map onto a 24-bit bus?** Needs the board-layout page.
4. **Is the internal arithmetic wider than 16 bits, and does it truncate or round, saturate or
   wrap?** Undocumented. Drives the entire character of a driven reverb.

**Structural**

5. Does ROOM SIZE act as the single proportional delay multiplier of US4803731? Testable *today*
   against the Surefyre grid, no hardware needed. Highest information per unit of effort.
6. Does LIVENESS act as the first-order group-LPF coefficient of US4803731?
7. How many ER taps — 13 per channel (acreil, single source) or otherwise? Deconvolution settles it.
8. Do the REV programs really have **no damping filter inside the comb loops** (Kato–Futamase) with
   all shaping at the input bandpass? If so, the HIGH parameter (0.1–1.0) has to act somewhere
   else — where?
9. Does the YM3807 modulation crossfade two taps (US5553150 style) or jump integer addresses? Two
   very different sounds. Audible in a measurement of the MOD programs.

**Hardware / measurement**

10. Order, topology and phase response of the PFB-4 filter modules. Only the 12.7 kHz −6 dB/oct
    adjustment point is published. Must be swept.
11. Is the PCM54HP genuinely shared between the SAR ADC and the output DAC? Read the schematic
    image, not the OCR.
12. What is the divider chain from the 4 MHz resonator to 31.25 kHz, and to the 64 instruction
    slots per sample?
13. Transcribe the service manual's **Room Size Chart** — a factory calibration table nobody has
    used yet.
14. SPX90 vs SPX90II: is the *reverb* microcode identical, with only RAM addressing widened? The
    Surefyre pack is a **II**; if the engines differ, calibrating an SPX90 model against II data is
    a silent error.

**Legal / practical**

15. Redistribution and derived-work terms for the Surefyre pack. Unstated. Ask before relying on it.
16. Acquire a reference unit — or a **Yamaha FX900 / R100**, the cheaper YM3413-based cousins
    running essentially this reverb code, useful as a second reference and a decoding cross-check.
