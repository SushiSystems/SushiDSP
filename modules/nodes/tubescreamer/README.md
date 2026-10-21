# `modules/nodes/tubescreamer`, Ibanez Tube Screamer (TS808/TS9)

One `AudioNode`, mono in and mono out, registered as `sushidsp.tubescreamer` with four
parameters: drive, tone, level, input trim.
[`docs/design/SUSHIDSP/TUBE_SCREAMER.md`](../../../docs/design/SUSHIDSP/TUBE_SCREAMER.md) describes it as an input buffer
stage, an op-amp soft-clipping stage with a diode feedback pair, and a passive tone/output
stage, all via WDF/DK-method on the same solver as JCM800 (§4.5); as built, the pedal runs at
4x oversampling, not the 8x the spec names, and this file records that discrepancy rather than
repeating the spec's figure as fact. This file records what the code in this directory does.

## The two DK blocks

`tube_screamer_circuits.hpp` describes the netlists node for node against
[`docs/reference/spice/ts_clip/README.md`](../../../docs/reference/spice/ts_clip/README.md) and
`docs/reference/spice/ts_tone/README.md`, because the committed reference CSVs from those schematics are what the
block tests null against, a value drifting from its README source makes those nulls
meaningless.

| Block | Class | What it does |
| --- | --- | --- |
| Clip stage | `tube_screamer_circuits::ClipCircuit` + `DiodePairPort` | Op-amp clipper with a diode feedback pair; `kClipNodeFeedbackMiddle` splits the feedback resistor into its fixed 51 kΩ part and the 500 kΩ Drive-pot part, since `DkNetlist::potentiometer` is three-terminal and the real feedback element is a two-terminal variable resistor |
| Tone stage | `tube_screamer_circuits::ToneCircuit` + `LinearPort` | Passive tone/output network |

Both blocks run inside one 4x-oversampled region (`Oversampler4x`), built once in `prepare()`
and cleared, along with both DK blocks, in `reset()`.

## Level staging and calibration

- `K_LEVEL_UNITY_GAIN = 0.6942987f`, the Level knob's 0 dB gain, measured against the 4x DK chain
  directly: a −12 dBFS 82 Hz sine measures −7.94 dBFS through the chain at this setting.
- `K_MIN_FEEDBACK_RESISTANCE = 51000.0f` / `K_MAX_FEEDBACK_RESISTANCE = 551000.0f`, the Drive
  rheostat's endpoints in ohms, exposed publicly so a host or test can read what the knob's ends
  mean without opening the netlist.

## Latency

`latency_samples()` reports only the 4x oversampler's round-trip group delay; the DK blocks'
own group delay is frequency- and tone-dependent, so it is left out deliberately, tone, not
latency.

## Known deviation: `DiodePairPort` not wired in

`clip_port_` is declared as `DiodePairPort`, but the node still wires up the plain
`LinearPort`-style port. At the shipping 4x rate, `AdaaDiodePairPort` nulls further from the
reference circuit than the plain port does, so the smaller alias floor it would buy costs more
total error than it saves, see
the aaa audio quality design spec in the version history,
"Model corrections D6c C2, The verdict". Wiring it in would need one more line in `reset()`
(`clip_port_.reset()`) beyond the declaration already present. The measured gap is 14 dB. The
reset line is needed because the ADAA port keeps a sample interval of history, and `DkCircuit`
is handed the port per sample and does not own it, so `DkCircuit::reset()` cannot clear it.

## Dependencies

Links only `SushiDSP::sushi_dsp` (`modules/nodes/tubescreamer/CMakeLists.txt`), this is the D3
product the design spec uses to validate the shared DK solver core before JCM800 (§9, D3), as
the simpler single-nonlinear-stage circuit.

## Implementation notes

### The ADAA diode pair port

`AdaaDiodePairPort` models the diode pair as two branches that are each other's reverse, so the
port's second row is the negative of its first throughout: `v[1] = -v[0]`, `p[1] = -p[0]` and `F`
is antisymmetric with `F[0][1] = F[1][0] = -F[0][0]`. The two-branch port is therefore one scalar
port carrying the pair current `h(v) = i(v) - i(-v)`, and `DkCircuit`'s residual row reads
`v = p + F[0][0] h(v)`: a memoryless map from the open-circuit voltage `p` to the current the
linear part is stamped with. That is exactly the shape Holters' stateful ADAA needs, and the
substitution `dp = (1 - F[0][0] h'(v)) dv` integrates it in closed form, so no lookup table is
required. The derivation, the deliberate omission of Holters' delay compensation and what that
costs are in spec `2026-08-26-aaa-audio-quality-design.md`, Model corrections (D6c C2).

The port has a cadence contract: the owning node calls `reset()` whenever it resets its circuit,
because the history in the port is the circuit's state as much as `DkCircuit`'s own.

`currents_for_update` replaces the stamped pair current with its average over the sample
interval. The average is the divided difference of the composed map's antiderivative, and near a
stationary open-circuit voltage the midpoint of the two port voltages stands in for it. A moved
potentiometer changes `F[0][0]` between samples and so changes which curve the stored point sits
on. Re-anchoring the previous port voltage onto the new curve puts it back on one exactly, which
is why the history keeps `v` and not `p` alone. Its `currents` argument receives the branch
currents that the state update and the output stamp use.

`pair_current_and_antiderivative` computes `h(v) = i(v) - i(-v)` and, with `H` the sum of the two
diode antiderivatives, `Q(v) = H(v) - F[0][0] h(v)^2 / 2`, whose derivative with respect to `p`
is `h` by the substitution above.

### The oversampling factor and the smoothing time

`K_OVERSAMPLING_FACTOR` is 4. Each knob's smoother uses the 20 ms `K_SMOOTHING_MS`: slow enough that
a jump cannot step the clipper's operating point audibly, fast enough to feel immediate under a
hand. Four times is where the diode pair's aliasing products fall below the
noise floor of the committed halfbands, and the DK solve costs too much to spend eight (approved
C1).

### A failed derivation

When a DK derivation fails in `prepare()` it never recovers, so `process()` degrades to what the
pedal's own bypass would give: the dry signal, copied. No solver runs and the input is already
bounded, so that path cannot alias or blow up. The guard sits above every `skip()` so all four
smoothers advance together or not at all.

### Potentiometer updates

Wiper positions are a control-rate concern. `set_potentiometer` marks nothing dirty when the
value has not moved, so a still knob costs nothing and a moved one costs one re-derivation per
block (D6b plan, ruling 3).

### Volts inside the oversampled loop

The oversampled loop works in volts in and volts out: the DK blocks are the real circuit, and
`K_FULL_SCALE_VOLTS` is what 0 dBFS means at the instrument jack (spec D5b.5). The D6a 20 Hz input
biquad is gone. The clip block's own 1 uF into 510 kOhm is the pedal's real input high-pass, at
0.31 Hz.

### Cascading the clip and tone blocks

The clip stage's probe is its op-amp's output node, which has zero output impedance, so the tone
block cannot load it. Cascading the two within one sample is exact, not an approximation (D6c
plan, ruling 8).

### The Level ramp

Level alone ramps per sample. It is a single multiply outside the oversampled loop, and stepping
it once per block would make the audible smoothing time a function of the host's block size.

### Reset

Zeroing the state vectors is the whole of `reset()`. A pedal has no bias point to settle, so
unlike `JcmV1aStageNode::reset()` there is no warm-up loop to run.

## Designs

<!-- designs:begin -->
- [SUSHIDSP](../../../docs/design/SUSHIDSP/README.md): The standalone amp and pedal simulation framework: the rig graph, the DSP math library, the amplifiers and the studio, timeline and room paths.
<!-- designs:end -->
