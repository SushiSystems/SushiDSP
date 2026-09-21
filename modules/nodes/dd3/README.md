# `modules/dd3` — Boss DD-3 digital delay

One `IAudioNode`, mono in and mono out, registered as `sushidsp.dd3` with four parameters:
D.TIME, F.BACK, E.LEVEL and MODE. The direct/wet mix stays inside the node, as the pedal's
E.LEVEL pot does.

Intent, the rulings behind the fitted constants and the acceptance table live in
[`docs/superpowers/specs/2026-09-05-dd3-spx90-design.md`](../../docs/archive/superpowers/specs/2026-09-05-dd3-spx90-design.md).
The schematic values come from
[`docs/reference/research/2026-08-26-boss-dd3-design-dossier.md`](../../docs/archive/research/2026-08-26-boss-dd3-design-dossier.md).
This file records what the code in this directory does.

## Which pedal

The DD-2 / DD-3 v1, long chip: the RDD63H101 gate array with three 4164 DRAMs. The DD-2 arrived in
1983 and the DD-3 v1 in 1986, and the long chip ran until the DD-3A replaced it with the
MN51010RBA in December 1990. Boss's own parts list says the DD-2 and the DD-3 v1 differ only in
the treadle part number, so one model is both pedals.

Dossier §2 picks that revision on documentation, not on reputation. It is the only version whose
schematic is legible at scan magnification and whose LSI has published pin functions, conversion
sequence and memory organisation, through the SDE-3000, DE-200 and RDD-10/20 service notes. The
DD-3A's MN51010RBA has none of that.

## The blocks and who owns each

| Block | Class |
| --- | --- |
| IC1b pre-emphasis shelf and IC1a de-emphasis shelf | `dd3::Dd3PreEmphasis`, `dd3::Dd3DeEmphasis` |
| 6th-order Sallen–Key anti-alias filter (Q3, Q4, Q6) | `dd3::Dd3AntiAliasFilter` |
| 4th-order Sallen–Key reconstruction filter (Q10, Q9) | `dd3::Dd3ReconstructionFilter` |
| NE570 compressor and expander | `dd3::Ne570Compander`, one instance per half |
| 12-bit converter, delay ring, HOLD write inhibit | `dd3::Dd3Converter` |
| VCO law and its R56/C52 slew | `dd3::Dd3Clock` |
| Elastic wet buffer between the two conversions | `dd3::WetFifo` |
| Host rate to fs(t) and back | `SushiDSP::Resampler`, in the core's `math/` |
| The chain's order, the feedback path, the mix | `SushiDSP::Dd3Delay` |

No block names another. `Dd3Delay` is the only class here that knows the topology, which is why
a change to the anti-alias cascade touches one file.

## The five constants that are not component values

Most numbers in the module come straight from a component value. Five do not, and they split into
two kinds. These three close a gap between the transcribed network and a service-note check point,
and a bench measurement would replace each one.

- **`kInputPathGainToCheckPointOne = 4.70358`** — check point [1], 620 mVp-p at the pre-emphasis
  output for 1 kHz at 100 mVp-p in. The transcribed stage produces 131.8145 mVp-p, and
  620 / 131.8145 = 4.70358. It sits on the input buffer, upstream of the summing node, so the dry
  branch carries it and the feedback loop does not.
- **`kExpanderOutputStageGain = 6.0`** — check points [3] 2.3 Vp-p and [4] 2.4 Vp-p. The expander
  returns 1.15² / 6.6125 = 0.2 V peak, so 0.4 Vp-p, and 2.4 / 0.4 = 6.0. Check point [4] is the
  reconstruction filter's output, upstream of the F.BACK take-off, so this gain recirculates.
- **`kLevelUnityGain = 0.0490307`** — check point [5], 140 mVp-p. In mode M at D.TIME 0.5 the wet
  leg measures 0.713838 full scale with E.LEVEL at unity, [5] is 0.035 full scale, and
  0.035 / 0.713838 = 0.0490307.

The other two are fitted to a published figure rather than read off any page. The spec's model
corrections carry their derivations.

- **`kEmitterFollowerGain = 0.9548116`** in `Dd3AnalogFilters.hpp` damps every Sallen–Key section.
  It is not the follower's loaded voltage gain, which is about 0.987; it absorbs the follower's
  output impedance and its base-current loading of the shunt capacitor. The published +1/−3 dB
  window bounds it to 0.9431071–0.9665162, and this is the midpoint. The window is a knife edge in
  it: 0.98 would put the delay path +3.18 dB at 7 kHz.
- **`kFeedbackTakeoffTrim = 1 / 4.170136`** in `Dd3Delay.hpp` makes `kFeedbackPathGain` equal to
  1 / 6.318388, the reciprocal of the loop's forward gain at its 3278 Hz peak. A bench measurement
  of the loop gain, dossier §4.7 item 5, is what would replace it.

## Wet delay

`Dd3Delay::wet_delay_seconds()` is the ring, plus what each rate conversion waits for, plus the
FIFO's prime:

```
N / fs(t)  +  down.latency_seconds()  +  up.latency_seconds()  +  8 / host_rate
```

At the fast end of mode S, `N` is 1024 and `fs` is 82.5 kHz, so the ring alone is 12.412 ms. Each
conversion waits `34 / min(source, target)` = 0.708 ms for the prototype's half kernel span, and
the FIFO holds back 8 host-rate samples, 0.167 ms. The shortest delay the model delivers is
13.995 ms, against the pedal's published 12.5 ms. That is 1.495 ms long, and plan Ruling 6 ships
the floor: reading the ring at `N − d` would cancel it exactly, and the spec's risk table records
the choice.

`latency_samples()` returns 0. The dry path is sample-aligned and the wet delay is the effect, not
something a host should compensate.

## What is not modelled

- **R-2R differential nonlinearity.** The R73 ladder's per-bit DNL has no measured target, so
  `Dd3Converter::quantise` is a uniform mid-tread step (plan Ruling 13). A measured table would
  replace the step and touch nothing else.
- **Converter overflow wrapping.** The SAR saturates at both rails here (plan Ruling 15). Whether
  the real gate array wraps instead is dossier §5 question 3, and it is a one-line change in
  `Dd3Converter`.
- **Oversampling of the analog chain.** The Sallen–Key cascades, the companders and the emphasis
  shelves all run at the host rate (plan Ruling 7). DR2's acceptance battery
  (`tests/functional/unit/test_dd3_acceptance.cpp`) prints the alias floor that leaves, so the
  deferral carries a number.
- **The DD-3A and everything after it.** Deleting Q2, C1 0.027 → 0.047 µF, deleting C18 and
  dropping the top clock to 80 kHz would cover the DD-3A's deltas that change the sound. Its gate
  array's internal conversion timing is undocumented either way, which is the part that would not
  follow.
