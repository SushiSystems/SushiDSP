# `modules/nodes/dd3`, Boss DD-3 digital delay

One `AudioNode`, mono in and mono out, registered as `sushidsp.dd3` with four parameters:
D.TIME, F.BACK, E.LEVEL and MODE. The direct/wet mix stays inside the node, as the pedal's
E.LEVEL pot does.

Intent, the rulings behind the fitted constants and the acceptance table live in
[`docs/archive/superpowers/specs/2026-09-05-dd3-spx90-design.md`](../../../docs/archive/superpowers/specs/2026-09-05-dd3-spx90-design.md).
The schematic values come from
[`docs/archive/research/2026-08-26-boss-dd3-design-dossier.md`](../../../docs/archive/research/2026-08-26-boss-dd3-design-dossier.md).
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

- **`K_INPUT_PATH_GAIN_TO_CHECK_POINT_ONE = 4.70358`**: check point [1], 620 mVp-p at the pre-emphasis
  output for 1 kHz at 100 mVp-p in. The transcribed stage produces 131.8145 mVp-p, and
  620 / 131.8145 = 4.70358. It sits on the input buffer, upstream of the summing node, so the dry
  branch carries it and the feedback loop does not.
- **`K_EXPANDER_OUTPUT_STAGE_GAIN = 6.0`**: check points [3] 2.3 Vp-p and [4] 2.4 Vp-p. The expander
  returns 1.15² / 6.6125 = 0.2 V peak, so 0.4 Vp-p, and 2.4 / 0.4 = 6.0. Check point [4] is the
  reconstruction filter's output, upstream of the F.BACK take-off, so this gain recirculates.
- **`K_LEVEL_UNITY_GAIN = 0.0490307`**: check point [5], 140 mVp-p. In mode M at D.TIME 0.5 the wet
  leg measures 0.713838 full scale with E.LEVEL at unity, [5] is 0.035 full scale, and
  0.035 / 0.713838 = 0.0490307.

The other two are fitted to a published figure rather than read off any page. The spec's model
corrections carry their derivations.

- **`K_EMITTER_FOLLOWER_GAIN = 0.9548116`** in `dd3_analog_filters.hpp` damps every Sallen–Key section.
  It is not the follower's loaded voltage gain, which is about 0.987; it absorbs the follower's
  output impedance and its base-current loading of the shunt capacitor. The published +1/−3 dB
  window bounds it to 0.9431071–0.9665162, and this is the midpoint. The window is a knife edge in
  it: 0.98 would put the delay path +3.18 dB at 7 kHz.
- **`K_FEEDBACK_TAKEOFF_TRIM = 1 / 4.170136`** in `dd3_delay.hpp` makes `K_FEEDBACK_PATH_GAIN` equal to
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
  (`tests/nodes/dd3/test_dd3_delay/test_acceptance.cpp`) prints the alias floor that leaves, so the
  deferral carries a number.
- **The DD-3A and everything after it.** Deleting Q2, C1 0.027 → 0.047 µF, deleting C18 and
  dropping the top clock to 80 kHz would cover the DD-3A's deltas that change the sound. Its gate
  array's internal conversion timing is undocumented either way, which is the part that would not
  follow.

## Implementation notes

### The converter's rulings

Three rulings shape `Dd3Converter`. Ruling 13: the SBM1024 is a plain uniform mid-tread quantiser
here. The R-2R ladder's per-bit DNL has no measured target yet, so it is not modelled; a table
would replace the linear step in `quantise` if one is ever measured. Ruling 14: a MODE change
swaps the ring length and clears the ring, except that Long and Hold share one length, so
switching between them must not clear the loop HOLD is meant to freeze. Ruling 15: the converter
saturates at both rails and does not wrap on overflow.

### A mode set before `prepare`

`set_mode` only ever records the requested mode. It resizes or clears `ring_` itself only once
`prepare` has allocated it; before that `ring_` is empty whatever mode was requested, so a mode
selected before `prepare` cannot leave `length_` pointing past an empty ring for `process_sample`
to index into. `prepare()` resolves `length_` from the recorded mode. `process_sample` requires
`prepare` to have run first. On an unprepared object it asserts in debug and returns `0.0F` in
release without touching `ring_`. The empty-ring check that makes this safe is unconditional, not
compiled out, so the out-of-bounds read cannot happen in either build.

### The compander's rectifier

The compressor rectifies its own output (2:1, `sqrt(K_REFERENCE_VOLTS * volts)`); the expander
rectifies its input (1:2, `volts^2 / K_REFERENCE_VOLTS`). The compressor's gain therefore lags its
own rectifier by one sample. The loop is otherwise algebraic, and at a 6.8 ms time constant one
sample of lag is 0.3 % of one time constant at 48 kHz.

The rectifier and its pole run at `K_RECTIFIER_OVERSAMPLING` times the host rate; the gain cell
does not. Rectification is the one nonlinearity here whose aliasing is a discretisation artefact
and not modelled hardware.
`docs/archive/superpowers/specs/2026-09-05-dd3-spx90-design.md`, "Model corrections", carries the
ruling and the measurements.

### The rectifier's oversampling

`K_RECTIFIER_OVERSAMPLING` is 8. The board's rectifier is continuous time, so the harmonics `|x|`
carries at every `2k*f0` never fold. Sampling it at the host rate does fold them, and the fold
lands where the pole cannot reach: at 48 kHz an 8 kHz tone puts `6*f0` on 48001.5 Hz, which
aliases to 1.46 Hz and modulates the gain cell at full weight. Eight sub-steps move the surviving
fold from the 6th harmonic of `|x|` to the 48th, whose Fourier coefficient is 1/2303 against
1/35, and take the DD-3's alias floor from -35.86 dB to -55.32 dB at the service note's stimulus.

### The dry path's full-scale voltage

The dry path scales the input by the core's `SushiDSP::K_FULL_SCALE_VOLTS`, the 2.0 V calibration.
`dd3::Dd3Converter::K_FULL_SCALE_VOLTS` is a different constant: 2.5 V, the converter's own ADC/DAC
rail.

### The pending mode

The `mode` parameter row records the requested mode and does not apply it. Clearing 16384 samples
inside the host's parameter drain is not real-time safe, so the row stores the pending mode and
`apply_pending_mode` applies it.

Eight sub-steps of the rectifier's pole in `ne570_compander.cpp` compose to the host-rate pole's
coefficient exactly: `exp(-1/(8*fs*tau))^8` is `exp(-1/(fs*tau))`, so `tau` does not move with the
sub-stepping. Under a changing input the sub-stepped map differs from the host-rate one by about
0.4 samples of envelope lag, which is negligible against `tau`'s 326.4-sample time constant.