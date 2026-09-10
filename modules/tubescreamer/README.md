# `modules/tubescreamer` — Ibanez Tube Screamer (TS808/TS9)

One `IAudioNode`, mono in and mono out, registered as `sushidsp.tubescreamer` with four
parameters: drive, tone, level, input trim.
[`docs/design/sushidsp.md`](../../docs/design/sushidsp.md) §6 describes it as an input buffer
stage, an op-amp soft-clipping stage with a diode feedback pair, and a passive tone/output
stage, all via WDF/DK-method on the same solver as JCM800 (§4.5); as built, the pedal runs at
4x oversampling, not the 8x the spec names, and this file records that discrepancy rather than
repeating the spec's figure as fact. This file records what the code in this directory does.

## The two DK blocks

`TubeScreamerCircuits.hpp` describes the netlists node for node against
[`docs/reference/spice/ts_clip/README.md`](../../docs/reference/spice/ts_clip/README.md) and
`ts_tone/README.md`, because the committed reference CSVs from those schematics are what the
block tests null against — a value drifting from its README source makes those nulls
meaningless.

| Block | Class | What it does |
| --- | --- | --- |
| Clip stage | `tube_screamer_circuits::ClipCircuit` + `DiodePairPort` | Op-amp clipper with a diode feedback pair; `kClipNodeFeedbackMiddle` splits the feedback resistor into its fixed 51 kΩ part and the 500 kΩ Drive-pot part, since `DkNetlist::potentiometer` is three-terminal and the real feedback element is a two-terminal variable resistor |
| Tone stage | `tube_screamer_circuits::ToneCircuit` + `LinearPort` | Passive tone/output network |

Both blocks run inside one 4x-oversampled region (`Oversampler4x`), built once in `prepare()`
and cleared, along with both DK blocks, in `reset()`.

## Level staging and calibration

- `kLevelUnityGain = 0.6942987f` — the Level knob's 0 dB gain, measured against the 4x DK chain
  directly: a −12 dBFS 82 Hz sine measures −7.94 dBFS through the chain at this setting.
- `kMinFeedbackResistance = 51000.0f` / `kMaxFeedbackResistance = 551000.0f` — the Drive
  rheostat's endpoints in ohms, exposed publicly so a host or test can read what the knob's ends
  mean without opening the netlist.

## Latency

`latency_samples()` reports only the 4x oversampler's round-trip group delay; the DK blocks'
own group delay is frequency- and tone-dependent, so it is left out deliberately — tone, not
latency.

## Known deviation: `DiodePairPort` not wired in

`clip_port_` is declared as `DiodePairPort`, but the node still wires up the plain
`LinearPort`-style port. At the shipping 4x rate, `AdaaDiodePairPort` nulls further from the
reference circuit than the plain port does, so the smaller alias floor it would buy costs more
total error than it saves — see
[`docs/superpowers/specs/2026-08-26-aaa-audio-quality-design.md`](../../docs/superpowers/specs/2026-08-26-aaa-audio-quality-design.md),
"Model corrections D6c C2, The verdict". Wiring it in would need one more line in `reset()`
(`clip_port_.reset()`) beyond the declaration already present.

## Dependencies

Links only `SushiDSP::sushi_dsp` (`modules/tubescreamer/CMakeLists.txt`) — this is the D3
product the design spec uses to validate the shared DK solver core before JCM800 (§9, D3), as
the simpler single-nonlinear-stage circuit.
