# AAA Audio Quality & Plugin Readiness — Design Spec

**Status:** Draft for user review. Supersedes the D5–D7 entries of `docs/design/sushidsp.md` §9
once approved; §5/§6 "As built" sections are rewritten at the end of each phase below.

## Goal

Bring SushiDSP from a well-engineered DSP skeleton to a shippable, commercial-fidelity guitar
amp/pedal plugin: clipping, treble and bass behaviour that survives an A/B against Neural DSP /
STL / Helix class products, loaded in a DAW as VST3/CLAP/AU. "Accurate" is defined measurably,
not by ear: every nonlinear stage null-tests against a SPICE reference of the same schematic,
and the whole chain meets numeric aliasing, THD and level-calibration criteria (§Acceptance).

## Context — what the 2026-08-26 audit found

Three independent audits (amp model, math core, host/plugin readiness) agree on the picture.
Infrastructure is sound: RT-safety is trap-tested, partitioned convolution and the Newton solver
are correct, the `NodeDescriptor`/`RigGraph`/`RtGraphSlot` seams are the right shape for a
plugin. The *circuit math* is not commercial-grade, and several host-level gaps would make a
plugin misbehave in a DAW regardless of tone. Concretely:

**Tone-limiting (why it does not sound AAA):**

- `TriodeStage` has no plate-current law. It solves one grid-node diode and applies a linear
  gain; cutoff is a Newton clamp at `vk − 4.14 V` — a hard clip with a slope discontinuity,
  the worst possible aliasing source. Grid conduction uses Vt = 25.8 mV (diode-hard knee).
- No capacitor state inside any nonlinear stage → no bias shift, no blocking distortion, no
  V1b cold-clipper asymmetry, no cathode-network frequency-dependent gain. Bass tightness
  cannot emerge; a 120 Hz biquad HPF is standing in for it.
- Tone stack is three RBJ biquads, not the FMV network (no mid scoop at noon, no interaction).
- No cathode follower, no long-tailed-pair PI, no global NFB (presence is a post shelf), master
  volume sits after the power section instead of before the PI, push-pull is two independent
  `tanh` then A−B, sag is an output gain reduction instead of B+ modulation.
- Oversampling is 8× but each halfband is a 15-tap Hamming sinc: ≈ −16 dB at 0.3·f_stage,
  ≤ 54 dB best-case stopband, −1.6 dB passband droop at 19 kHz per pass. JCM800 makes three
  8× round trips per block (tone stack sits at base rate between two regions); TS one more.
- Tube Screamer clipping stage is inverting (real: non-inverting, clean pass-through + clipped
  hump), lacks the 4.7 kΩ + 47 nF ground leg (no drive-dependent 720 Hz corner) and the 51 pF
  feedback cap; diode ideality n fixed at 1. Tone control is LP+HP Butterworth crossfade with a
  full null at 720 Hz at mid position.
- Output soft limiter (dcbb28c) is a static `tanh` after the cabinet; its oversampler runs only
  when engaged, so stale FIR history clicks on every engagement.

**Plugin-blocking:**

- GUI writes node parameters directly on the live node while the audio thread processes
  (`RigController::set_parameter` → `Jcm800::set_bass` → biquad coefficient rewrite). `SpscRing`
  exists and is unused.
- Cabinet convolution partition = `max_block` and silently bypasses when `frames != partition`.
  DAW variable block sizes drop the cabinet in and out.
- No latency reporting. No IR resampling/normalisation. No 0 dBFS ↔ volts calibration; no
  input trim. Smoothing is per-block (block-size-dependent). No plugin target, no CMake export.
- Tests: regression snapshots pin five samples; THD is a proxy; no alias/response measurement of
  the full chain; no SPICE null-test; everything at 48 kHz.

## Decisions taken (user-confirmed 2026-08-26)

| Decision | Choice | Rationale |
|---|---|---|
| Scope | One umbrella spec, seven sequential phases, one implementation plan per phase | Keeps the whole picture coherent while each plan stays executable |
| Circuit solver | **DK-method** (nodal state-space, implicit Newton on the nonlinear currents) | Handles multi-nonlinearity blocks (LTP, push-pull + NFB) that a WDF tree cannot without R-type adaptors; the industry-standard approach for this class of product |
| WDF | **Deferred indefinitely** — `Wdf.hpp` stays as the two elements it is; spec §4.5 amended to say so | Everything the roadmap needs is covered by DK; keeping two solver families doubles maintenance |
| Plugin format | **CLAP first, VST3/AU/Standalone via clap-wrapper** | One API, no Steinberg agreement, ImGui editor embeds; wrapper is Apache-compatible |
| Validation reference | **LTspice schematics + exported CSV responses committed to the repo** | Reproducible without hardware; each circuit block ships with its `.asc` and its golden data |
| ADAA | Not used. Tried on the TS clip block in D6c C2 and measured worse; see Model corrections (D6c C2), *The verdict* | First-order ADAA at 4× removes a −19 dB aliasing error and introduces a −5 dB band-limiting one. It is built and kept as `AdaaDiodePairPort`, unwired, with its numbers; revisit only at 8× or higher |

## Architecture overview

```
                     ┌───────────── product modules (DK circuits) ─────────────┐
                     │  tubescreamer: [input buf][clip stage][tone][level]      │
 host/plugin ─▶ RigGraph ─▶ jcm800: [V1a][V1b][CF][FMV][master][LTP+PP+OT+NFB][sag][cab] ─▶ out
   │  ▲              │                       all inside ONE 8× region per node │
   │  │              └───────────────────────────────────────────────────────┘
   │  └── latency_samples() summed per node
   └── SpscRing<ParamChange> drained at top of RigGraph::process
        core/math: DkCircuit<N_states,N_nonlin,N_in,N_out>, TubeModels, Oversampler (≥90 dB),
                   PartitionedConvolution (fixed partition + ring), SmoothedParameter (ms),
                   IrLoader (resample/normalise/trim), Measurement (tests only)
```

Layering is unchanged: `sushi_dsp` core stays C++17 + SIMD only; product modules depend only on
core; the plugin depends only on core + modules. New code lands in existing directories
(`include/SushiDSP/math/dk/`, `include/SushiDSP/circuits/`), not new top-level layers.

---

## Phase D5b — Plugin-readiness foundations

Purpose: make the standalone behave like a plugin will, so every later measurement is taken on
the real signal path. No tone changes except removing the limiter.

### D5b.1 Parameter path

- `ParameterChange { NodeId node; std::uint16_t param_index; float value; }` carried by the
  existing `SpscRing<T>`; `RigGraph::process` drains it at block start before any node runs.
- `IAudioNode` gains `set_parameter(std::uint16_t index, float value)` (RT-safe: writes a
  target only). The `const char* id` overload becomes a control-thread helper that resolves the
  index through the descriptor once; it is never called from a callback.
- Coefficient recomputation moves to the audio thread and happens only when the smoothed
  value has moved (per-block dirty flag), never from the GUI thread.
- `apps/host_gui` and `apps/host_asio` route every knob through the ring. `RigController`
  keeps no pointer to the live node for parameter purposes.

### D5b.2 Smoothing

- `SmoothedParameter`: one-pole toward target with a time constant in **milliseconds**
  (default 20 ms), coefficient derived from sample rate at `prepare()`. Per-sample update
  inside the oversampled loop for anything that feeds a nonlinear stage (preamp, master,
  drive); per-block for linear filter targets, followed by coefficient recompute.
- Removes `kSlewPerBlock` everywhere.

### D5b.3 Cabinet block independence + latency

- `PartitionedConvolution` takes a fixed partition (default 128, chosen at `prepare` from
  sample rate: 128 @ 44.1/48 k, 256 @ 88.2/96 k) and an input FIFO; `process(frames)` accepts
  any `frames`, producing exactly `frames` outputs. Adds `reset()` and `latency_samples()`
  (= partition size).
- `IAudioNode::latency_samples()` (default 0). `RigGraph::latency_samples()` sums the active
  chain. Oversampler reports its FIR group delay in base-rate samples; nodes sum their
  internal contributions.
- Non-power-of-two host `max_block` no longer disables anything.

### D5b.4 IR pipeline

- `IrLoader::load(path, target_sample_rate, options) → Ir { std::vector<float> samples; }`
  (control thread only): WAV → mono mix → **windowed-sinc resample** to target rate → trim to
  `max_length_ms` (default 200 ms) with a 5 ms raised-cosine fade-out → **normalise** so the
  IR's magnitude response averaged over 100 Hz–5 kHz is 0 dB (broadband gain preserved,
  level made IR-independent). Min-phase conversion is *out of scope* (linear-phase IRs
  measured by a mic are already near-min-phase; latency is reported anyway).
- `Jcm800` accepts an `Ir` via a control-thread setter that swaps through `RtGraphSlot`-style
  deferred reclaim (the node owns two convolution engines; swap is one pointer store).
- `host_gui` gets an IR chooser; presets store the IR path. `cabinet_ir` becomes a real
  descriptor parameter of kind `ParamKind::Path`. One CC0/Apache-compatible 4×12 IR is
  committed under `assets/ir/` so the amp never runs without a cabinet by default. (DeCuir
  packs are commercial: user-loaded only, never redistributed.)

### D5b.5 Level calibration

- Project-wide constant `kFullScaleVolts = 2.0f` (0 dBFS ≡ 2.0 V peak at the instrument
  jack). A passive humbucker hitting −12 dBFS then maps to 0.5 V peak, which is the level the
  tube-stage bias points are designed around in D6d.
- `InputTrim` node parameter on both products (−24…+24 dB, default 0). `Master`/`Level` become
  dB-scaled (`ParamKind::Decibel`, −60…+12). Output of each product normalised at design time so
  default settings peak at ≈ −6 dBFS with a −12 dBFS input.
- The soft limiter and its oversampler are **removed**; a plain `std::clamp` to ±1.0 stays as
  the last line of defence, after the cab, with a test asserting it never engages in the
  regression corpus.

### D5b.6 Denormals

`DenormalGuard` is engaged inside `RigGraph::process` (RAII, restores on exit) in addition to
the device callbacks, so offline rendering, tests and the plugin are all covered.

### Tests

- SPSC path: GUI-thread writer + audio-thread reader stress test, no torn coefficients (assert
  biquad stability every block).
- Convolution: `frames` ∈ {1, 7, 64, 100, 256, 1000, 4096} sequences equal the direct
  convolution within 1e-6; `latency_samples()` matches the measured impulse delay.
- IR loader: 44.1 k IR resampled to 48 k matches an offline reference within −80 dB; normalised
  response averages 0 dB ± 0.1 dB over the band.
- Smoothing: time constant measured at 64 and 4096 frames agrees within 5 %.
- Level: −12 dBFS 82 Hz sine at defaults → output peak −8…−4 dBFS for both products.

---

## Phase D6a — Measurement infrastructure and oversampling

Purpose: give every later phase a numeric acceptance test, then fix the anti-aliasing so the
measurements mean something.

### D6a.1 SPICE reference corpus

- `docs/reference/spice/<block>/<block>.asc` (LTspice schematic) + `<block>.csv` golden data +
  `README.md` naming the component values and the LTspice version used. Blocks: `ts_clip`,
  `ts_tone`, `jcm_v1a`, `jcm_v1b_cold`, `jcm_cf`, `jcm_fmv`, `jcm_ltp`, `jcm_pp_ot_nfb`.
- Two CSV kinds per block: **AC sweep** (magnitude/phase, 10 Hz–20 kHz, log, 200 points, at
  three control settings) and **transient** (a 1 kHz sine at three input levels, 20 ms, steady
  state, 192 kHz). Transient CSVs are the null-test inputs.
- A `tools/spice/export.py` script regenerates CSVs from `.asc` for whoever has LTspice; CI
  never needs LTspice — it only reads the committed CSVs.
- Tube models used in SPICE are the same Koren parameter sets D6b implements (see D6b.3), so a
  null-test measures the *solver*, not model disagreement.

### D6a.2 Measurement library (`tests/measure/`)

Header-only, test-side only, never linked into products:

- `render(node, stimulus, sample_rate, block_size) → std::vector<float>`
- `thd(signal, f0, sample_rate, harmonics=20) → { thd_percent, per_harmonic_dB[] }` (FFT with
  Blackman-Harris, bin-exact f0).
- `alias_floor(node, sample_rate)`: sweep 1–20 kHz sines at −12 dBFS, for each measure the
  highest non-harmonic spectral line in 20 Hz–20 kHz relative to the fundamental. Returns the
  worst case in dB.
- `frequency_response(node, sample_rate) → magnitude[]` via exponential sine sweep at −40 dBFS
  (small-signal) + deconvolution.
- `null_test(a, b) → residual_dB` after gain/delay alignment.

### D6a.3 Oversampler redesign

- Keep the 3-stage 2× halfband cascade (cheapest for 8×) but redesign every stage with
  Parks–McClellan halfband constraints, coefficients generated by `tools/dsp/design_halfband.py`
  and committed as `static constexpr` arrays with the design parameters in the header:
  - Stage 1 (base ↔ 2×): passband to 0.45·f_base, stopband from 0.55·f_base, ≥ 90 dB;
    ≈ 63 taps at 48 k. Passband ripple ≤ 0.05 dB.
  - Stage 2 (2× ↔ 4×) and stage 3 (4× ↔ 8×): the audio band is a narrow fraction of these
    rates, so shorter filters suffice: ≥ 80 dB, ≈ 23 and 15 taps.
- Polyphase both directions (today decimation is direct-form). Group delay reported via
  `latency_samples()`.
- **One oversampled region per node.** JCM800 moves the tone stack inside it (it becomes a DK
  block in D6d anyway); the three round trips become one.

### D6a.4 Cost budget

Node budgets are measured at 48 kHz, 256-frame blocks, one core, `sd test -t release -s
benchmark`: TubeScreamer ≤ 0.25 ms, JCM800 ≤ 1.5 ms (raised from 1.0 ms for the DK power
section; still ≈ 28 % of the block). Numbers go in every phase's commit message.

### Tests / acceptance

- Oversampler alone: alias rejection ≥ 90 dB at 0.3·f_base and beyond (was ≈ 16 dB); passband
  droop ≤ 0.1 dB at 20 kHz round trip.
- Existing chain (pre-DK): `alias_floor` improves by ≥ 30 dB vs. the D5b baseline; that baseline
  number is recorded in the plan.
- Regression snapshots are re-pinned (this phase is a deliberate behaviour change) and widened
  to **full-block golden files** (`tests/golden/*.f32`, 1 s at 48 k and 96 k) with a −80 dB
  null tolerance instead of five samples at 1e-5.

---

## Phase D6b — DK-method circuit core

Purpose: one generic, allocation-free, compile-time-sized nonlinear circuit solver that every
stage in D6c/D6d instantiates.

### D6b.1 Formulation

Discretised nodal analysis (Yeh 2008 / Holters & Zölzer 2015), trapezoidal rule. For a circuit
with `S` capacitor/inductor states, `N` nonlinear branch currents, `I` inputs, `O` outputs:

```
x[n]   = A x[n-1] + B u[n] + C i[n]
v[n]   = D x[n-1] + E u[n] + F i[n]      // voltages across nonlinear elements
y[n]   = G x[n-1] + H u[n] + K i[n]
i[n]   = f(v[n])                         // tube / diode currents
```

`v = p + F·f(v)` with `p = D x + E u` is solved per sample by Newton–Raphson on the `N`-vector
(Jacobian `I − F·J_f`). Matrices are precomputed at `prepare()` from component values and the
sample rate (base × oversample factor); potentiometers that change at runtime are handled by the
**variable-resistor extension**: a pot enters as an extra "nonlinear" branch with a linear
current law, so its matrices are precomputed once and the knob only changes a scalar — no
matrix re-derivation on the audio thread.

### D6b.2 Implementation

- `template <int S, int N, int I, int O> class DkCircuit` with fixed-size `std::array` storage;
  matrices are plain `float` (double during `prepare()` derivation, cast once). State `x` is
  kept in **double** — the audit's float-DF1-at-low-cutoff drift applies directly here.
- `DkNetlist` builder (control thread): `resistor(a,b,R)`, `capacitor(a,b,C)`,
  `inductor(a,b,L)`, `voltage_source(a,b)`, `potentiometer(a,b,c,R)`, `nonlinear(a,b,Model&)`,
  `output(a,b)`; `build(sample_rate) → DkCircuit`. This is where nodal-matrix assembly and the
  `(R⁻¹ + 2C/T)` inversions live; it is **not** RT code and may allocate.
- `NewtonRaphsonSolver` gains the `N`-dimensional variant: LU on a stack `N×N`, residual
  tolerance (1e-9 relative on `v`), max 8 iterations, **fallback** to a damped step (halving up
  to 4 times) when the residual grows, and a per-block counter of non-converged samples the
  tests can read. The scalar path stays for TS diodes (N=1 fast path).
- The `DiodeClipperCircuit` header comment claiming "WDF" is corrected; the class is
  re-expressed as `DkCircuit<0,1,1,1>` to prove the core on a known answer.

### D6b.3 Device models (`include/SushiDSP/circuits/models/`)

Each exposes `current(v) `and `jacobian(v)`, both C¹-continuous, in double:

- `Triode12AX7` — Koren plate current
  `Ip = (E1^kx / kg1)(1 + sgn(E1))`, `E1 = (Vpk/kp)·ln(1 + exp(kp(1/μ + Vgk/√(kvb + Vpk²))))`,
  with the standard 12AX7 set (μ = 100, kp = 600, kvb = 300, kg1 = 1060, kx = 1.4).
  Grid current: Norman's soft law `Ig = Ig0·ln(1 + exp((Vgk − Vγ)/Vt_g))`, `Vγ ≈ 0.5 V`,
  `Vt_g ≈ 0.15 V` (removes the 25.8 mV diode-hard knee).
- `PentodeEL34` — Koren pentode with screen dependence (`Vg2` term, `kg2`), same grid-current
  law; screen current modelled so the screen resistor's drop (and thus compression) is real.
- `SiliconDiode` — Shockley with ideality `n` (1N914: Is = 2.52 nA, n = 1.752).
- Every model ships with an **LTspice `.subckt`** using identical equations and parameters
  (`docs/reference/spice/models/`), which is what D6a's reference schematics include.

### D6b.4 Validation

- Golden tests against closed-form circuits: RC/RLC (compare to analytic), a diode clipper
  (compare to D2's existing solver bit-for-bit within 1e-6), a single common-cathode 12AX7
  stage vs its LTspice transient CSV (null ≤ −60 dB at three drive levels).
- Newton convergence: ≤ 0.01 % non-converged samples over the full stimulus set; zero NaN/inf.
- RT-safety trap test for `DkCircuit::process` and the N-dim solver.
- Benchmark: a 2-nonlinearity, 4-state circuit at 8× ≤ 0.12 ms per 256-block.

---

## Phase D6c — Tube Screamer rebuilt on DK

Schematic (TS808): input buffer (unity, modelled linear), 1 µF input cap to 510 kΩ
(≈ 0.3 Hz — replaces the 20 Hz biquad), **non-inverting op-amp stage** (ideal op-amp,
`Vout = Vin + i_f·Zf`) with feedback `Rf = 51 kΩ + Drive·500 kΩ ∥ 51 pF` and `4.7 kΩ + 47 nF`
to ground; antiparallel `SiliconDiode` pair across `Rf`; tone section: op-amp with
`220 nF / 10 kΩ` low-pass leg and `1 kΩ / 220 nF` resonant leg blended by the 20 kΩ tone pot
(second DK block, 2 states, 0 nonlinearities → pure linear state-space, still via `DkCircuit`
so pot handling is uniform); level pot; output cap. Drive and Tone pots use the variable-resistor
extension; audio-taper laws are applied to the normalised parameter before the pot enters.

One 8× region wraps both blocks. Two DK instances: `DkCircuit<3,2,1,1>` (clip: 2 diodes as
separate branches so asymmetric pairs are possible later) and `DkCircuit<2,0,1,1>` (tone).

Acceptance: AC response vs `ts_tone.csv` within ±0.5 dB at three tone settings; clip stage
transient null vs `ts_clip.csv` ≤ −55 dB at three drive × three level combinations; mid-tone
shows **no notch** (response monotonic between 300 Hz and 2 kHz); alias floor ≤ −85 dB;
regression golden re-pinned; cost ≤ 0.25 ms.

### Model corrections (D6c C2, 2026-09-07)

Task 9 measured the rebuilt Tube Screamer's alias floor at −18.6 dB against C1's −38.3 dB, and
named the mechanism: at the 4× run rate of 192 kHz, the 13th harmonic of a 15 kHz tone lands on
195 kHz and folds to 3 kHz, in band before any filter can act. A filter cannot reach it and the
oversampling factor is already spent, so the remaining lever is an antialiased nonlinearity.
This subsection records what was built, what it measures, and why the shipping node does not yet
use it.

**Status: built, measured, and deliberately not wired in.** `AdaaDiodePairPort` exists beside
`DiodePairPort` in `modules/tubescreamer/include/SushiDSP/tubescreamer/TubeScreamerCircuits.hpp`
and switching `TubeScreamer::clip_port_` onto it is a one-declaration change. Do not make that
change: measured against the analogue circuit at the rate the pedal actually runs, the
antialiased port is **14 dB further from it** than the plain one. The measurement that decides
this, and the reason a smaller alias floor does not mean a better model, are under *The verdict*
below.

#### The diode pair is one scalar port, and that is what makes the method apply

Antiderivative antialiasing is textbook for a memoryless waveshaper and the diode pair is not
one: it sits inside `DkCircuit<3,2,1,1>`, solved implicitly by Newton against the feedback
capacitor and the ground leg. Holters (DAFx-19, *Antiderivative Antialiasing for Stateful
Systems*, extended in Applied Sciences 10(1):20) covers exactly this case, and his condition is
that the nonlinearity take scalar input. The clip block meets it.

`build_clip_block()` wires branch 1 as branch 0 reversed, so every row of the port is the
negative of its partner: `v[1] = -v[0]`, `p[1] = -p[0]`, and `F` is antisymmetric with
`F[0][1] = F[1][0] = -F[0][0]`. `Unit_Dsp.DkTsClipBlockDiodePairPortIsOneScalarPortInDisguise`
measures those to 0, 0 and 1e-15 respectively rather than assuming them. The two-branch port is
therefore one scalar port carrying the pair current

```
h(v) = i(v) - i(-v) = 2*Is*sinh(v / (n*Vt))
```

and `DkCircuit`'s residual row reads `v = p + F[0][0]*h(v)`. Because `F[0][0]` is negative and
`h` is increasing, `p(v)` is strictly increasing, so `v` is a single-valued function of `p`:
the whole nonlinear port is a memoryless map from the open-circuit voltage to the current the
linear part is stamped with. Everything else in the block is a linear state update.

#### The antiderivative is closed form, so no lookup table is needed

Holters precomputes his antiderivative into a 1024-point table with cubic interpolation, because
in general the composed map has no closed solution. Here it does. Substituting
`dp = (1 - F00*h'(v)) dv`:

```
Q(p) = integral h dp = integral h(v) * (1 - F00*h'(v)) dv = H(v) - F00*h(v)^2 / 2
```

with `H(v) = I(v) + I(-v)` the sum of the two diode antiderivatives, which on the Shockley
branch is `2*Is*n*Vt*cosh(v / (n*Vt))`. `SiliconDiode::current_and_antiderivative()` carries
`I`, piecewise with the current so the tangent-line extrapolation past either knee integrates to
the quadratic matched to it. The port then costs one exponential per side per sample and no
memory. `Unit_Dsp.AdaaDiodePairPortAntiderivativeDividesBackToThePairCurrent` checks
`dQ/dp = h(v)` across the port's whole operating range at the block's own `F[0][0]`; the worst
relative error is 2.5e-10.

The stamped current becomes the divided difference `(Q(p[n]) - Q(p[n-1])) / (p[n] - p[n-1])`,
with the midpoint of the two port voltages standing in below a step of 1 µV. Newton is untouched
— the residual and its Jacobian stay the block's own physics — so the insertion point is a hook
after the solve, `DkCircuit`'s `HasCurrentsForUpdate`, which lets a port restate the currents the
state update and the output stamp without `DkCircuit` learning what a diode is.

A moved Drive pot changes `F[0][0]` between samples and so changes which curve the stored point
sits on. The port re-anchors the previous port voltage onto the new curve rather than carrying a
stale one, which costs two exponentials on the sample after a knob moves and nothing otherwise.

#### What it buys

At 192 kHz, Drive 1, a 15 kHz tone at 0.5 V, the folded 13th harmonic at 3 kHz drops from
−32.35 dB to −55.92 dB — 23.6 dB, measured by
`Unit_Dsp.AdaaDiodePairPortSinksTheFoldedHarmonicThatSetsTheAliasFloor`. Whole-node
`alias_floor()` improves by less, because the sweep reports the worst tone and a different one
becomes worst:

| tone | worst alias, plain | worst alias, ADAA | fold |
| --- | --- | --- | --- |
| 9 kHz | −29.5 dB | −59.4 dB | 21st to 3 kHz |
| 10 kHz | −23.6 dB | −35.9 dB | 19th to 2 kHz |
| 13 kHz | −20.7 dB | −38.4 dB | 15th to 3 kHz |
| 15 kHz | −18.6 dB | −42.2 dB | 13th to 3 kHz |
| 17 kHz | not printed | −31.2 dB | 11th to 5 kHz |
| 18 kHz | not printed | −39.0 dB | 11th to 6 kHz |

The floor moves from **−18.6 dB to −31.2 dB**, set now by 17 kHz. First-order antialiasing
suppresses a fold more the higher the harmonic order that reaches it, so halving the order from
13 to 11 costs most of the remaining gap to −38.3 dB.

#### What it costs, and why the node still holds a `DiodePairPort`

Antiderivative antialiasing replaces the endpoint value of the pair current with its average over
the sample interval. That is a half-sample delay and a mild high-frequency attenuation, both
inside the feedback loop, and both are real changes to the modelled circuit rather than artefacts
of the measurement.

Against ts_clip's LTspice transient the clip block's null falls from −80.3 / −73.7 / −76.0 dB to
**−36.5 / −32.8 / −31.8 dB**, past the spec's −55 dB and past the case's own −60 dB. Three
measurements say what that is and what it is not:

- Nulling the antialiased block against the *plain* block through the same pipeline gives
  −36.2 / −31.9 / −30.2 dB. The gap to LTspice is entirely the difference between the two ports.
- The departure is first order in the timestep: plain against ADAA on a 1 kHz, 0.5 V stimulus at
  Drive 0.5 reads −23.6 dB at 192 kHz, −48.6 dB at 1 MHz and −64.6 dB at 4 MHz.
- Offering the reference delayed by half a sample does not recover it (−34.6 / −35.2 / −35.5 dB),
  so the error is not a pure delay; the attenuation of the clipped corner is the larger part.

The reference grid is 1 MHz, where a clipped corner has next to no aliasing left to remove. At
that rate antialiasing can only spend its cost, which is why the null reads as pure loss. Whether
it earns that cost back at the rate the pedal runs is a different question, and *The verdict*
below answers it.

Newton's miss rate rises from 0.0054 % to 0.0105 % of samples, past the spec's 0.01 % gate, with
zero NaN fallbacks and the same worst-case iteration count.

Cost is not the obstacle. Stepping the clip block four million times at 192 kHz reads 247 ns per
sample through the plain port and 269 ns through the antialiased one, **+9 %**, which would take
the node from 0.150 ms per 256-block to roughly 0.162 ms against a 0.25 ms budget. The closed-form
antiderivative is what keeps it there: two exponentials per sample and no table.

#### Recovering the null needs a decision that is not this task's to make

Holters compensates the half sample by discretising the system at two thirds of the run rate and
operating it at the full rate, so that a 1.5-sample loop delay reads as one sample. His two
examples are circuits whose behaviour is the loop; the clip block is not. Retuning its
discretisation would move every corner of the linear part by 1.5×, including the 0.31 Hz input
high-pass and the small-signal response that
`Unit_Dsp.DkTsClipBlockSmallSignalResponseMatchesItsClosedForm` pins to a closed form. That is
why the compensation was not applied, and why the null is reported rather than accommodated. It
would not have saved the port in any case: the half sample turns out to be the smaller half of
its error, as the aligned figures below show.

#### The verdict (2026-09-07): the plain port models the circuit better at 192 kHz

The reference-rate null charges antialiasing for its cost without crediting it for its benefit,
so it cannot decide on its own. Two measurements at the shipping rate do, and they agree.

**Against LTspice decimated to 192 kHz.** `Resampler` (DR1) converts the 1 MHz transient to the
run rate — 24/125, which no halfband cascade reaches, and zero phase, so nothing has to be
shifted back. It touches the reference and the stimulus only, never a circuit output, so whatever
it leaves below 10 kHz is common to both ports and cancels in the comparison
(`Unit_Dsp.DkTsClipBlockBothPortsNullAgainstTheReferenceDecimatedToTheShippingRunRate`):

| Drive | plain at 192 kHz | ADAA at 192 kHz |
| --- | --- | --- |
| 0.1 | **−79.32 dB** | −36.44 dB |
| 0.5 | **−73.47 dB** | −32.79 dB |
| 0.9 | **−75.63 dB** | −31.78 dB |

The plain port loses almost nothing going from the 1 MHz grid to 192 kHz (−80.3 → −79.3 dB),
which is itself the finding: on this stimulus, at this rate, aliasing is not a measurable error.
ts_clip's stimulus is a 1 kHz sine and 192000/1000 is an integer, so every folded harmonic lands
exactly on a harmonic of the tone. The reference is blind to the thing antialiasing removes.

**Against the converged model, on a tone that does alias.** The same block's equations at
1.92 MHz — where the 13th harmonic of 15 kHz is nowhere near Nyquist — decimated to 192 kHz by
the same converter, is a converged solution of the model, which is the right reference for asking
which discretisation is closer to the answer it is trying to compute
(`Unit_Dsp.DkTsClipBlockBothPortsNullAgainstTheConvergedModelOnTheToneThatAliases`):

```
15 kHz, 0.5 V, Drive 1:   plain -19.18 dB    ADAA -5.35 dB    ADAA half-sample aligned -5.58 dB
```

Plain's −19.18 dB *is* its aliasing: the alias floor at this tone was −18.55 dB relative to the
fundamental, so essentially the whole of its error against the converged model is the fold. ADAA
removes that error and puts back one of −5.35 dB — **14 dB worse**. Offering it the reference
delayed by half a sample makes it slightly worse still, so almost none of that is the removable
delay; it is the attenuation of the clipped corner, which at 4× is simply too coarse a grid for
a first-order interval average to sit on.

**So: the plain port stays, and the alias floor stays at −18.6 dB.** A smaller alias floor bought
by a larger total error is not a better model, and the whole-node figure of −31.2 dB, while real,
measures only the part of the error that lands on non-harmonic bins. Antialiasing at this
oversampling factor trades a measured error for an unmeasured one.

#### The Newton miss rate, diagnosed

The antialiased port's 0.0105 % is past the 0.01 % gate, and instrumenting the port says what it
is not. Across the same 1 056 000-sample stimulus: the solve never once leaves the Shockley range
(worst |v| = 0.579 V, zero samples past the 1.8125 V knee), the divided difference never loses
its conditioning (tightest relative difference between the two antiderivative evaluations
2.1e-07, nine digits intact, zero severely cancelled samples), and the ill-conditioning guard
fires on 1111 samples, 0.105 %, doing its job. Both ports put every miss in the 20 Hz–20 kHz
sweep and none in the 0.05 / 0.5 / 5 V bursts.

So neither the guard's 1 µV threshold nor the cancellation the guard exists to prevent explains
it. The residual Newton solves is character for character the plain port's; what differs is the
state the antialiased current writes, and the extra 54 misses are that trajectory arriving at
harder open-circuit voltages. Nothing here would be fixed by re-tuning the guard, and since the
port is not wired in, the shipping node's rate stays 0.0054 %.

#### What is left, with what is known about each

1. ~~**Build the null the antialiasing deserves.**~~ Done, above. It decided against the port.
2. **Second-order antialiasing.** The second antiderivative of the composed map is also closed
   form — every term integrates, `∫h²h' dv = h³/3` included. It would suppress the fold further,
   but it costs a full sample of loop delay instead of half and attenuates the corner harder, and
   the corner attenuation is already what loses the comparison above. On this evidence it would
   lose by more, not less, at 4×. Worth revisiting only at 8× or higher.
3. **8× with the plain port**, C1's decision reverted. The honest remaining route to the alias
   floor, and the one the evidence now points at: it costs no modelling accuracy at all. Task 9
   measured the clip block at 1071 ns/sample at 8× against 586.7 ns/sample for the whole node at
   4×, so the node would land near 0.28 ms per 256-block against a 0.25 ms budget — over, and the
   budget or the block would have to give.
4. **Antialiasing at 8×**, where the interval average sits on a grid fine enough for its error to
   fall below the fold it removes. The error is first order in the timestep, so halving the step
   halves it while the fold it suppresses drops much faster. Untested, and it inherits route 3's
   cost problem before adding its own 9 %.

#### A defect found on the way

`SiliconDiode`'s reverse extrapolation mirrors its forward tangent line rather than continuing
the Shockley law, so at `v = -1.8125 V` the reported current steps from −2.5 nA to −593 MA. The
class documentation claims C¹ everywhere, and that claim is false at the negative knee. Nothing
in the tree drives a converged solution there — the pair sits near ±0.7 V — and the antiderivative
added here integrates the current the model actually reports, so it is consistent with whatever
the model does. Changing the reverse branch would change the Newton guard behaviour of every DK
block that uses the part, so it is left alone and recorded here.

---

## Phase D6d — JCM800 2203 rebuilt on DK

Blocks, each a `DkCircuit`, all inside one 8× region in this order:

1. **Input + V1a** — 1 MΩ grid leak, 68 kΩ grid stopper, 470 pF bright cap across the
   1 MΩ preamp pot (bright becomes level-dependent as in the amp), 100 kΩ plate,
   2.7 kΩ ∥ 0.68 µF cathode, 0.022 µF coupling cap into the pot. States: 3, nonlin: 1.
2. **V1b cold clipper** — 470 kΩ / 470 pF treble peaker into the grid, 100 kΩ plate,
   **10 kΩ unbypassed cathode**, 0.022 µF coupling. States: 3, nonlin: 1. This is the stage
   that produces the 2203's asymmetric clipping and bias shift.
3. **V2b cathode follower** — 100 kΩ cathode load, direct-coupled from V2a's plate (V2a is the
   third gain stage: 100 kΩ plate, 820 Ω ∥ 0.68 µF cathode); modelled as one block of 2
   triodes (grid-conduction compression of the CF driving the tone stack is a key 2203
   behaviour and needs the coupling to be implicit). States: 3, nonlin: 2.
4. **FMV tone stack** — 33 kΩ, 470 pF, 22 nF, 22 nF, treble 220 kΩ, bass 1 MΩ, mid 25 kΩ pots;
   `DkCircuit<3,0,1,1>` with three variable-resistor pots. Insertion loss and interaction come
   for free. **Master volume** (1 MΩ pot) sits at its output, before the PI, as in the amp.
5. **Phase inverter + power + OT + NFB** — one block: LTP (12AX7 pair: 82 kΩ/100 kΩ plates,
   470 Ω + 10 kΩ tail, 1 MΩ grid references), coupling to the EL34 pair (220 kΩ grid leaks,
   5.6 kΩ grid stoppers, fixed bias −38 V default, 1 kΩ screen stoppers), push-pull output
   transformer as primary magnetising inductance + leakage inductance + winding resistance
   reflected to a nominal 16 Ω resistive load (reactive speaker model is out of scope; the cab
   IR carries the speaker's response), **global NFB** from the secondary through 100 kΩ into the
   LTP tail with the **Presence** pot (5 kΩ + 0.1 µF) as the shunt. States: 8, nonlin: 4. The
   NFB loop closes *inside* the implicit solve, so presence, damping and the "resonance" that
   appears at low presence are physical, not shelves. Class-AB crossover emerges from the
   pentode cutoff; the B-side grid-offset hack is removed.
6. **Power-supply sag** — rectifier + 50 µF reservoir + 10 kΩ/32 µF dropping stages as a
   1-state linear block driven by the block-5 total cathode current; its output voltage is the
   `Vplate`/`Vscreen` supply seen by block 5 next sample (one-sample delay is acceptable here:
   the sag time constant is milliseconds). Sag now shifts headroom and bias instead of scaling
   output.
7. **Decimate → cabinet convolution → output trim → clamp.**

Removed: `bright_cap_` biquad, coupling-cap biquad, `JcmToneStack`, `PentodeStage`'s B-side
offset, `OutputTransformerStage`'s HPF/LPF/tanh, `PowerSupplySagStage`'s gain law, the
presence shelf. `TriodeStage`/`PentodeStage` are deleted once no test references them.

Parameters: Preamp, Master, Bass, Middle, Treble, Presence, InputTrim, Output (dB), Cabinet
IR; advanced (descriptor-exposed, smoothed): Bias (−30…−45 V). `oversample_factor` stays a
compile-time constant; `sag_amount` disappears (the circuit values define it).

Acceptance (all measured by `tests/measure/`):

- Block-level nulls vs `jcm_*.csv`: V1a, V1b, CF ≤ −55 dB at three levels; FMV AC response
  ±0.5 dB at five knob combinations (incl. all-noon showing the ≈ −10 dB mid scoop); PI/PP/OT
  block ≤ −45 dB (four nonlinearities; larger tolerance, documented).
- Whole-chain alias floor ≤ −80 dB at 48 k with Preamp = 10, −12 dBFS input.
- THD sweep monotonic in Preamp and in Master (master now changes distortion).
- Palm-mute "tightness" proxy: 82 Hz burst at −6 dBFS, Preamp = 8: the ratio of 2nd-harmonic
  to fundamental and the envelope recovery time are recorded as golden numbers from the SPICE
  full-chain transient, and the DSP matches within 1 dB / 10 %.
- 44.1 / 48 / 96 kHz golden files null against each other after resampling ≤ −70 dB (rate
  independence).
- Cost ≤ 1.5 ms / 256-block. Newton non-convergence ≤ 0.01 %.
- `docs/design/sushidsp.md` §5 "As built" rewritten to match.

---

## Phase D7 — CLAP plugin (`apps/plugin`)

- `SushiDspClap`: CLAP entry, one plugin id `io.sushisystems.sushidsp.rig`, mono-in /
  stereo-out and stereo-in / stereo-out (dual-mono: two `RigGraph` instances sharing a preset)
  bus configurations, `clap.latency` from `RigGraph::latency_samples()`, `clap.params` built
  from every registered `NodeDescriptor` (parameter id = FNV-1a of `"<node-instance>/<param>"`,
  stable across sessions), sample-accurate param events converted to `ParameterChange` pushed
  into the ring at the event's frame offset (the ring drain in `RigGraph::process` gains a
  frame-offset field; sub-block splitting at those offsets), `clap.state` = the existing preset
  JSON plus IR path (IR bytes are *not* embedded; missing IR falls back to the bundled one and
  reports via `clap.log`), `clap.gui` hosting the ImGui rig editor in the host-provided window
  (Win32 HWND first; Cocoa/X11 in D8).
- **clap-wrapper** produces `SushiDSP.vst3`, `SushiDSP.component` and a standalone from the
  same binary. `sd plugin` builds and installs into the user's plugin folders; `sd plugin
  validate` runs `clap-validator` and the VST3 validator.
- CMake `install()`/`export()` producing `SushiDSP::*` + `SushiDSPConfig.cmake` (spec §8).
- `sample_rate`/`max_block` changes from the host call `prepare()` off the audio thread with a
  graph swap, never in `process`.

Acceptance: clap-validator and VST3 validator pass with zero errors; loads in Reaper, Ableton
Live and Pro Tools (variable block sizes, PDC correct: a dry/wet null with the plugin's
reported latency ≤ −80 dB on a linear preset); automation of every parameter produces no
clicks (measure: no sample-to-sample step > −40 dBFS during a 1 ms full-range sweep at −60 dBFS
input); RT-safety trap test for the plugin `process` callback.

---

## Phase D8 — Hardening and packaging

- macOS (Cocoa GUI, AU via wrapper, universal binary) and Linux (X11) targets; code signing and
  notarisation scripted in `sd plugin package`.
- Multi-rate CI matrix (44.1/48/88.2/96/192 k) running the golden nulls.
- Perceptual A/B: once a real 2203 capture exists, add it under `docs/reference/captures/` and
  a documented listening protocol; not a gate for release.
- CHANGELOG brought current; stray files (`image.png`, `imgui.ini`, `rust_in_peace.wav`)
  removed from the root.

---

## Acceptance summary (the "AAA" definition)

| Criterion | Target | Measured by |
|---|---|---|
| Stage accuracy | null vs SPICE ≤ −55 dB (single-tube), ≤ −45 dB (PP block) | `null_test` |
| FMV / TS tone response | ± 0.5 dB vs SPICE AC | `frequency_response` |
| Aliasing, whole chain | ≤ −80 dB worst non-harmonic line, Preamp 10 | `alias_floor` |
| Oversampler | ≥ 90 dB stopband, ≤ 0.1 dB droop @ 20 k | unit test |
| Rate independence | 44.1/48/96 k nulls ≤ −70 dB | golden files |
| Automation | no step > −40 dBFS during sweeps | plugin test |
| Latency | reported = measured, PDC null ≤ −80 dB | DAW test |
| CPU | JCM800 ≤ 1.5 ms, TS ≤ 0.25 ms per 256 @ 48 k | benchmark |
| Newton | ≤ 0.01 % non-converged, 0 NaN | counters |

## Explicitly out of scope

Reactive speaker impedance model, min-phase IR conversion, ADAA, a WDF adaptor tree, tube
noise/microphonics, hum, multiple amp channels, stereo cab/mic blending, AAX. Each can be a
later spec; none blocks the acceptance table above.

## Risks

- **DK power block size** (8 states, 4 nonlinearities at 384 kHz) is the CPU risk. Mitigation:
  measure early in D6d with the block alone; fall back to splitting NFB with a one-sample delay
  (documented behaviour change) only if the 1.5 ms budget fails after SIMD-ising the LU.
- **SPICE model agreement**: tube parameter sets are shared by construction (D6b.3), but
  LTspice's trapezoidal integrator at 192 k vs. ours at 384 k will disagree at the top octave;
  nulls are evaluated band-limited to 10 kHz.
- **CLAP GUI embedding of ImGui** on Win32 is well-trodden (the standalone already owns a
  window); macOS Cocoa in D8 is where surprises live.
- **Regression churn**: D6a, D6c and D6d each intentionally re-pin golden files; each re-pin is
  its own commit with the measurement deltas in the message.
