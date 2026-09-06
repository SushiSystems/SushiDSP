# Changelog

Grouped by roadmap phase (spec §9). Each phase's implementation plan lives under
`docs/superpowers/plans/` with the full task-by-task record.

## DA — High-gain amplifiers

The high-gain-amp track's first phase, plan `.superpowers/sdd/2026-09-06-high-gain-amps-part1/`.

- 2026-09-06 — Moved `TriodeStage`, `PentodeStage`, `OutputTransformerStage`, `PowerSupplySagStage` to `include/SushiDSP/circuits/stages/`; added `PowerTubePresets.hpp` (`kEl34`).

## DR2 — Boss DD-3 delay

The delay/reverb track's second phase, spec
`docs/superpowers/specs/2026-09-05-dd3-spx90-design.md`. The delay-time control moves the sample
clock rather than the buffer length, so pitch slur under a moving knob and darkening repeats fall
out of the model instead of being painted on. A header named below without a directory lives in
`modules/dd3/include/SushiDSP/dd3/`.

- 2026-09-05 — Added `modules/dd3/`, the Boss DD-2 / DD-3 v1 long-chip delay as one mono `IAudioNode`, six headers over five translation units, with `modules/dd3/README.md` beside them.
- 2026-09-05 — Registered the pedal as `sushidsp.dd3` under the `Delay` category in `modules/node_registry/src/NodeRegistry.cpp`.
- 2026-09-05 — Built the four linear networks in `modules/dd3/include/SushiDSP/dd3/Dd3AnalogFilters.hpp`: the emphasis shelf pair, the 6th-order anti-alias cascade and the 4th-order reconstruction cascade.
- 2026-09-05 — Damped every Sallen–Key section by a fitted emitter-follower gain K = 0.9548116, pulling the anti-alias cascade from +6.45 dB at 7 kHz to +0.19/−1.26 dB, inside the published +1/−3 dB window (`Dd3AnalogFilters.hpp`).
- 2026-09-05 — Anchored every section's bilinear prewarp at the 7 kHz band edge, holding the delay path's 7 kHz point to a 0.005 dB spread across 48, 96 and 192 kHz (`Dd3AnalogFilters.hpp`).
- 2026-09-05 — Added `Ne570Compander.hpp`, one class for both halves of the NE570 about a 6.6125 V reference derived from check points [2] and [3], tau 6.8 ms, rectifier ripple left in.
- 2026-09-05 — Added `Dd3Converter.hpp`: a saturating 12-bit mid-tread characteristic over a 16384-sample ring whose live length follows MODE, with HOLD as a write inhibit.
- 2026-09-05 — Added `Dd3Clock.hpp`, the VCO's 82.5 kHz to 20 kHz exponential law in the D.TIME knob, slewed by the R56/C52 0.594 ms pole sitting on log(fs).
- 2026-09-05 — Added `Dd3Delay.hpp`, closing the loop through two `Resampler`s re-clocked every 8 frames, with F.BACK taken from the wet leg's output.
- 2026-09-05 — Set the loop's forward gain at 6.318388, its peak at 3278 Hz and the same figure at 48, 96 and 192 kHz, so `kFeedbackPathGain` is its reciprocal (`Dd3Delay.hpp`).
- 2026-09-05 — Moved `kInputPathGainToCheckPointOne` onto the input buffer, outside the feedback branch, where the residual falls by exactly 4.703580 (`modules/dd3/src/Dd3Delay.cpp`).
- 2026-09-05 — Extracted `WetFifo.hpp`, which primes on 8 real samples instead of zeros written at reset.
- 2026-09-05 — Deferred a MODE change made by descriptor index to the next block start, moving the 16384-sample ring clear out of the host's parameter drain (`modules/dd3/src/Dd3Delay.cpp`).
- 2026-09-05 — Derived the wet delay at 13.995 ms in mode S at D.TIME 0 against the pedal's published 12.5 ms: the ring is 12.412 ms and the two conversions plus the FIFO prime add 1.583 ms (`Dd3Delay.hpp`).
- 2026-09-05 — Calibrated `kLevelUnityGain` to 0.0490307 from service-note check point [5], 140 mVp-p out for 1 kHz at 100 mVp-p in (`Dd3Delay.hpp`).
- 2026-09-05 — Added acceptance rows 1 and 4 at node level, the clock curve, pitch slur under a knob sweep, NaN battery, latency claim and alias floor, in `tests/functional/unit/test_dd3_acceptance.cpp`.
- 2026-09-05 — Added the 48 and 96 kHz golden pair in `tests/functional/regression/test_dd3_regression.cpp`, which fails until `SUSHIDSP_REGENERATE_GOLDEN=1 sd test -s regression` writes the two `.f32` files.
- 2026-09-05 — Corrected the wet-delay prediction's resampler round trip from 64 to 68 input samples, matching the committed prototype's half width, and derived it from a named constant (`tests/functional/unit/test_dd3_acceptance.cpp`).
- 2026-09-05 — Added three RT-safety traps for `Resampler::process`, `Resampler::set_rates` and `Dd3Delay::process` with a MODE change inside the trapped window (`tests/functional/unit/test_rt_safety.cpp`).
- 2026-09-05 — Added a DD-3 benchmark case against the spec's binding 0.15 ms per 256-frame block and a 48–20–48 kHz resampler round trip against an advisory 0.10 ms (`tests/benchmark/main.cpp`).
- 2026-09-06 — Gave `measure::alias_floor` an `amplitude` parameter, defaulted to the −12 dBFS it already swept at (`tests/measure/AliasFloor.hpp`).
- 2026-09-06 — Swept the DD-3's alias floor at the service note's 0.025 full scale, moving the reading from −20.39 to −35.86 dB (`test_dd3_acceptance.cpp`).
- 2026-09-06 — Recorded that the −35.86 dB residual is the NE570 rectifier folding 6·f₀ to 1.46 Hz at 8 kHz; Ruling 7 decides (spec `2026-09-05-dd3-spx90-design.md`, Model corrections).
- 2026-09-06 — Ran the NE570's rectifier and its pole at 8 sub-steps per host sample over a linear reconstruction, taking the DD-3's alias floor from −35.86 to −55.32 dB (`Ne570Compander.hpp`, `Ne570Compander.cpp`). Harness-derived; gtest confirmation deferred to the user-ordered run.
- 2026-09-06 — Held the compander's other numbers across that change: τ's 63.2 % crossing on sample 327, the 50 Hz ripple at −23.75 dB, check point [4] +0.018 % (`test_ne570_compander.cpp`).

Three readings contradict a published source, and the model carries each contradiction rather
than hiding it:

- 2026-09-05 — Recorded that the anti-alias cascade with an ideal buffer peaks at +6.45 dB at 7 kHz against the published +1/−3 dB delay path; K is fitted to the window, and a real-unit sweep is what settles it (dossier §4.7 item 1).
- 2026-09-05 — Recorded that N = 1024/4096/16384 and the published 12.5–800 ms delay ranges disagree by +2.40 % at the long end of each; the spec's N stands.
- 2026-09-05 — Recorded that check point [1] needs an unexplained ×4.70358 to reach its published 620 mVp-p from the transcribed 131.8145 mVp-p, carried as one named constant (`Dd3Delay.hpp`).

Not yet measured by gtest. The build was locked while DR2 was written, so the alias-floor and
NE570 sub-stepping entries above are harness-derived, and no number in the acceptance table or
benchmark has a gtest figure yet. `sd test -s all` runs the acceptance battery in
`tests/functional/unit/test_dd3_acceptance.cpp`; `sd test -t release -s benchmark` prints the
`DD-3 Delay` and `Resampler 48k-20k-48k round trip` rows against their budgets; and
`SUSHIDSP_REGENERATE_GOLDEN=1 sd test -s regression` writes `tests/golden/dd3_48k.f32` and
`dd3_96k.f32`, which do not exist yet and which the two regression cases fail without.

## DR1 — arbitrary-ratio resampler

The shared brick both delay/reverb products consume, and the first `math/` addition since D6a.
`math/Oversampler` is untouched: it converts by a fixed integer ratio through halfband cascades,
while this one takes a ratio that changes per block.

- 2026-09-05 — Added `include/SushiDSP/math/Resampler.hpp` and `src/math/Resampler.cpp`, an arbitrary-ratio polyphase resampler that walks one fixed prototype with a fractional phase accumulator.
- 2026-09-05 — Committed the prototype in `include/SushiDSP/math/ResamplerCoefficients.hpp`: half width 34 zero crossings (input samples; true crossings are fewer at fc 0.45), 512 phases per crossing, 17410 entries, Kaiser beta 10.0613.
- 2026-09-05 — Added the generator `tools/dsp/design_resampler.py`, which rebuilds that header and refuses to write one whose half width no longer clears the −100 dB stopband target.
- 2026-09-05 — Measured the prototype at −100.48 dB worst-case stopband over [0.5, 256.0] and 0.0001 dB passband ripple over [0, 0.4], at a half width of 34.
- 2026-09-05 — Fixed the group-delay contract: `Resampler::latency_seconds()` reports availability look-ahead, 34 / min(source, target), and the converter is zero phase, so output time t carries input time t (`Resampler.hpp`).
- 2026-09-05 — Held the phase floor across a widening rate change by clamping `position_` at the top of `process`, which took the sinusoid-identity residual from 0.18 to 6.77e-8 (`src/math/Resampler.cpp`).

## AmpliTube-style GUI

`apps/host_gui` stops being one 567-line `main.cpp` drawing sliders and a vertical list. No new
third-party dependency, no image file, no font file: every panel, knob and link is drawn with
`ImDrawList`.

- `apps/host_gui/ui/` — fourteen units, split so the half that computes is testable without an
  ImGui context. `KnobMath` maps a parameter value onto a 270-degree sweep and back, `PanelLibrary`
  holds the per-node table of where each control sits (plus a generic grid for a node type with no
  entry), `ChainLayout` turns slots and edges into a row-and-column grid, and `LevelProbe` carries a
  block peak from the audio thread. Those four carry unit tests; `Theme`, `Widgets`, `StagePanel`,
  `ChainStrip`, `GearBrowser`, `MeterBar`, `TopBar` and `DeviceSettings` draw, and are gated on the
  build plus one visual pass.
- Per-slot bypass. `RigController::set_bypassed` leaves the node out of the published graph and
  wires its upstream neighbours straight to its downstream ones, collapsing a run of bypassed slots
  in one step and refusing to exceed `RigGraph::kMaxFanInOut`. The slot keeps its parameters, its
  cabinet path and its edges. `publish()` and `save_to_json()` now share one `build_graph()`, called
  with bypass applied and without it, so a bypassed node still appears in a saved preset behind a
  `"bypassed"` field. A preset written before this change loads with every slot active.
- `SushiDSP::RigGraph::is_empty()` — the audio callback's O(1), allocation-free question. A rig with
  every slot bypassed publishes a graph with no node, and the host copies its input to its output
  rather than replaying the previous block. The same change corrects the stale comment above
  `latency_samples()`, which still claimed a sum over the chain after the DAG work made it a
  longest path.
- Parallel paths are buildable, not only drawable. A splitter with room for another branch offers
  a dashed slot on the row below; dropping gear on it calls `RigController::add_branch()`, which
  wires the splitter to the new node and the new node to the mixer, creating that mixer when the
  rig has none. The mixer is not a convenience: `RigGraph::compile()` allows one sink, so two
  branches without a rejoin do not compile.
- Cabinet impulse responses are chosen from a browser rather than typed. `DirectoryListing` reads
  and sorts a directory (subdirectories first, then `.wav` files, case-insensitive match) and
  `FileBrowser` draws it. The chosen folder is saved with the preset as a rig-level `"ir_folder"`
  key, so a tone reopens where its impulse responses live. The path is machine-local by design;
  a preset carried to another machine finds no folder and the browser falls back to the `assets/ir`
  directory beside the executable.
- Device selection moved out of a one-time startup screen into a window reachable at any time.
- `CMAKE_EXPORT_COMPILE_COMMANDS` is on, so a single translation unit can be type-checked without
  a build.

## D6b — DK-method circuit core

Infrastructure only: the solver and the device models exist, and **no product module changed**.
`Jcm800` and `TubeScreamer` still run their D6a stages; the rebuild onto DK is D6c/D6d.

- `include/SushiDSP/math/dk/` — `DenseLu.hpp` (fixed-size LU with partial pivoting and a
  size-aware overload), `DampedNewtonSolver.hpp` (damped multivariate Newton with a halving line
  search, convergence counters, and a best-iterate guarantee: when `solve()` returns false it
  restores the lowest-residual finite iterate it visited, so a failed solve never hands the caller
  a point worse than the one it started from, even though a non-reducing step is still taken
  inside the iteration to escape a flat region), `DkNetlist.hpp` (the control-thread netlist builder:
  resistors, capacitors, inductors, sources, nonlinear branches, probes, potentiometers) and
  `DkCircuit.hpp` (`DkCircuit<S, N, I, O>` — trapezoidal discretisation, per-sample solve,
  block-cadence potentiometer re-derivation, `nonlinear_currents()` for supply sag).
- `include/SushiDSP/circuits/models/` — `SiliconDiode.hpp`, `Triode12AX7.hpp`,
  `PentodeEL34.hpp`, and the shared `SoftTransition.hpp` softplus/logistic helpers. Every model
  evaluates `current` and `jacobian` in `double` against the exact constants of its `.sub` file,
  and is C¹ across every clamp and kink. `float` appears only at the `IAudioNode` boundary.
- Validation: RC and RLC blocks against their analytic pole (`+0.811320754717` at
  `R = 1 kΩ, C = 100 nF, 48 kHz`) and magnitude peak (4905.485 Hz at `R = 100 Ω, L = 10 mH,
  C = 100 nF`); a diode clipper against the true root of its own equation; the JCM800 `jcm_v1a`
  stage against its hand-solved operating point, `Vk = 2.310 V` / `Vp = 274.4 V`.
- The `jcm_v1a` netlist carries the 12AX7's interelectrode capacitances (Cgk 1.6 pF, Cgp 1.7 pF,
  Cpk 0.46 pF), which `docs/reference/spice/models/12ax7_koren.sub` declares inside the subcircuit
  and every SPICE run of the block therefore has. Omitting them made the DK build a different
  filter from the reference — Miller-multiplied, Cgp with the 68 kΩ grid stopper sets an input
  corner near 22 kHz — which would have floored the pending SPICE transient null near −27 dB
  instead of the −60 dB the plan asks for. The stage shape is therefore `DkCircuit<6, 2, 2, 1>`
  and the benchmark's tailed shape `DkCircuit<7, 2, 2, 1>`, not the `<3,…>`/`<4,…>` first built.
  The bias point is unchanged (`Vk = 2.310 V`, `Vp = 274.4 V`) — capacitors do not move DC.
- Newton convergence, by stimulus, both at 384 kHz, both with **zero** non-finite samples and at
  most **4** iterations:
  - *Continuous stimulus* (`test_dk_triode_stage.cpp`: a 2 s 20 Hz→20 kHz sweep plus 1 kHz bursts
    at 0.05 V, 0.5 V and 5 V, the last well past grid conduction) — 0 non-converged solves in
    1 056 000, **0.0000 %**, inside the 0.01 % target.
  - *Looped-block stimulus* (the benchmark: `measure()` replays one 256-frame buffer, so the tone
    steps back to sample 0 once per repetition, driving the grid into conduction at a
    discontinuity) — 0 non-converged in 450 560, **0.0000 %**. With the three-state netlist this
    case missed the target at 219 in 450 560 (**0.0486 %**, one per repetition); the interelectrode
    capacitances damp the buffer restart; on this one stimulus the count is now zero, so the miss
    previously handed to D6c is closed as measured — a zero on one stimulus is an upper bound, not a
    guarantee, and the counter stays printed so D6c can re-check on its own material.
- The LTspice null and AC-sweep cases against `jcm_v1a` were written to skip until the reference
  CSVs existed; LTspice 24 was installed on 2026-08-27 and every block's CSVs are now committed
  (`docs/reference/spice/<block>/`). Results: transient null **-97.8 / -94.6 / -88.7 dB** across the
  three PREAMP steps against the -60 dB target; AC sweep within 0.5 dB from 20 Hz to 10 kHz. The
  only failure on first run was in the measurement, not the model: `measure::frequency_response`
  started its sweep at 20 Hz and misread its own start bin by 1.7-3.4 dB; it now starts at 5 Hz.
- **Spec deviation:** the spec asked for diode-clipper agreement with `DiodeClipperCircuit`
  within 1e-6 V. That is unattainable, and the fault is the oracle's: `DiodeClipperCircuit` runs
  four fixed `float` Newton iterations with no convergence check, and is off the true root by up
  to 2.8e-3 V on about 1.3 % of samples. The D6b test therefore asserts the stronger thing — DK
  within **1e-11 V of the true root on every sample** — plus 1e-6 V agreement with the oracle
  wherever the oracle itself converged, and an oracle failure rate under 2 %. Whether
  `DiodeClipperCircuit` stays an oracle at all is a D6c decision.
- **Corrected for D6d (plan Ruling 6):** a nonlinear branch is a port of the device law, not a
  device. A 12AX7 is two branches, an EL34 three. D6d block 1 is `DkCircuit<6, 2, 2, 1>` (`S = 6`:
  three discrete capacitors plus the triode's three interelectrode ones) and block 5 needs
  `N = 10` — larger than the spec's D6d table implies.
- **Open question for D6d:** the `PentodeEL34` Koren fit draws 33.4 mA at `Vg1k = −37 V`, at the
  high end of a real 2203's idle. That may mean the spec's default fixed-bias point wants moving
  rather than the model wanting refitting; D6b did not decide it.
- `tests/circuits/JcmV1aNetlist.hpp` — the V1a block described once, with named component
  constants, and built by all three test-side consumers (the stage test, the RT-safety traps, the
  benchmark). Its `with_rc_tail` flag appends the series RC that makes the same circuit a
  seven-state block for the benchmark.
- RT-safety traps added for `DkCircuit::process_sample`, for `set_potentiometer` +
  `update_matrices_if_dirty` (the one audio-thread path that re-runs a derivation, which is what
  plan Ruling 3 rests on), and for `DampedNewtonSolver::solve`. All three allocate nothing.
- Bench (release, 48 kHz, 256-frame block), two new cases:

  | Case | ns/sample | ms/block | Budget |
  | --- | --- | --- | --- |
  | `DkCircuit 2-nonlin 7-state @8x` | 3762.5 | **0.963** | 0.12 |
  | `DkCircuit pot update (1x)` | 552.7 | 0.141 | — |
  | `DkCircuit pot re-derivation` | 1562.5 | — | — |

  One run quoted throughout. The plan's binding budget line is "2-nonlinearity / 4-state at 8×,
  0.12 ms"; the block is now seven states because the reference topology has the triode's
  interelectrode capacitances, and the row is renamed to match. The same case measured 0.873 ms at
  four states (spread 0.859–0.890 ms over eight release runs), so the three extra states cost about
  10 %. The original spec target is kept as the number of record and this is the deviation from it.

  **The 0.12 ms budget is missed by 8.0×** and the number is reported rather than the test
  loosened. Profile: one block is 2048 solver calls; a solve averages 470 ns, of which about
  90 % is `Triode12AX7::current` and `::jacobian` — 63.8 ns for the pair, called roughly 6.5
  times per solve by the Newton iteration and its line search. The levers, largest first, are a
  joint `current_and_jacobian` on the models (the two calls recompute the same exponentials), the
  redundant post-solve `currents()` call in `process_sample`, and a `DenseLu<2>` specialisation.
  Together they will not close an 8.0× gap on their own, so D6c/D6d also has to decide whether a
  two-nonlinearity block runs at 8× or lower. The third case backs Ruling 3 outright: its
  ns/sample column is nanoseconds per re-derivation, so a full re-derivation measures **1.56 µs**
  — 1.1 % of the block it happens in and 0.03 % of a 5.33 ms block period. The `@8x` case also
  prints the solver's own counters (solves, non-converged, worst-case iterations, NaN fallbacks),
  so the convergence figures above are reproducible from the harness.
- Test count: 312 (305 unit, 6 regression, 1 benchmark).

## D6a — Measurement infrastructure and oversampler alias floor

- `tests/measure/` — a test-side measurement library (`render`, `thd`, `alias_floor`,
  `frequency_response`, `null_test`) shared by every product's plausibility test. Measured
  alias floors at 48 kHz before this phase's DSP changes: TubeScreamer (`drive = 1`) −8.3 dB,
  Jcm800 (`preamp = 10`) −3.8 dB.
- Halfband redesign (`include/SushiDSP/math/HalfbandCoefficients.hpp`, regenerated by
  `tools/dsp/design_halfband.py` from a scipy `remez` design): stage 1 115 taps (−90.12 dB
  stopband), stage 2 27 taps (−101.60 dB), stage 3 15 taps (−110.69 dB), passband ripple
  ≤ 0.0003 dB throughout. `HalfbandUpsampler2x`/`HalfbandDownsampler2x` are now polyphase in
  both directions, touching only the non-zero taps. `Oversampler8x` round trip: 66.4 → 60.2
  ns/sample; round-trip latency rises from 12 to 65 base-rate samples (the wider filters behind
  the deeper stopbands cost more delay, not more per-sample work).
- `Jcm800` collapses to one 8×-oversampled region: sag and `JcmToneStack` move inside it
  alongside V1a/V1b/V2a/PI. **Host-visible behaviour change:** this raises the amp's reported
  latency rather than halving it — at D5b two regions cost 12 base-rate samples each, 24 in
  total; the single region built from the new halfbands costs 65, so the amp goes 24 → 65
  base-rate samples, about 0.85 ms more at 48 kHz.
  Three physically-derived 12AX7 Miller-effect one-pole lowpasses
  (`Biquad::set_one_pole_low_pass`, prewarped bilinear) go on the V1b, V2a and phase-inverter
  grid nodes (`C_in = C_gk` 1.6 pF `+ C_gp` 1.7 pF `· (1 + |A|) + 10 pF` stray, a stated
  assumption, through a 100 kΩ source): corners at 13 804 Hz (V1b), 13 804 Hz (V2a) and
  19 576 Hz (PI). Measured alias floor at `preamp = 10` goes −3.8 → −32.0 dB (two-region route)
  → −20.4 dB (collapsed to one region) → −43.4 dB (with the Miller poles), meeting the ≥ 30 dB
  improvement target. Audible cost, whole amp (preamp 0.6, knobs centred): −0.06 dB at 1 kHz,
  −1.33 dB at 5 kHz, −4.64 dB at 10 kHz, −8.76 dB at 15 kHz. The bias-mismatch asymmetry test
  re-measures at 2.17 % second harmonic active vs. 2.93 % zeroed, up from 1.77 % / 2.45 %
  before this phase: the absolute figures moved because the drive reaching the power stage
  changed, while the mismatch's own effect — removing about 0.76 pp of second harmonic — is
  unchanged.
- **Spec deviation:** TubeScreamer's alias floor moves −8.3 → −9.6 dB, short of the same ≥ 30 dB
  target. Its worst alias is the 21st harmonic of an 18 kHz tone folding around the 8×
  oversampled Nyquist — aliasing generated inside the oversampled domain itself, which no
  base-rate filter redesign can remove. Fixing it needs the diode clipper's C¹ curve rework and
  the stage rebuild planned for D6c; the target is deferred there, not silently dropped.
- Bench (release, 48 kHz/256-frame block): Jcm800 1457.8 → 1368.4 ns/sample (0.350 ms/block
  against a 1.5 ms budget); TubeScreamer 557.8 → 535.5 ns/sample (0.137 ms/block against a
  0.25 ms budget). `tests/benchmark/` now prints both the ms/256-frame-block figure and, where
  given, the budget it is measured against.
- Regression tests move off five-sample snapshots onto 1-second rendered golden `.f32` files at
  48 kHz and 96 kHz (`tests/golden/`), compared at −80 dB RMS residual; regenerate with
  `SUSHIDSP_REGENERATE_GOLDEN=1 sd test -s regression`.
- A SPICE reference corpus is staged (`docs/reference/spice/`, `tools/spice/export.py`,
  `tests/measure/SpiceCsv.hpp`) but not yet populated — LTspice is not installed here, so the
  golden CSVs are a user-side step (install LTspice, run `python tools/spice/export.py --all`,
  commit the output).
- Test count: 236 (229 unit, 6 regression, 1 benchmark).

## D5b — Plugin-readiness foundations

- `latency_samples()` on `IAudioNode`, `RigGraph` and `Oversampler8x` — every node (and the graph
  as a whole) reports the delay it adds from input to output, so a host can compensate.
- `RtSwapSlot<T>` — a generic deferred-reclaim swap for handing an owned object from the control
  thread to the audio thread without locking; `RigGraph` engages a `DenormalGuard` for the whole
  block inside `RigGraph::process`, and `host_asio` loads cabinet IRs through `IrLoader` and
  prints the rig's reported latency.
- `SmoothedParameter` — a millisecond-time-constant (20 ms) smoother so knob feel is independent
  of block size; nodes recompute derived coefficients only when the smoothed value actually moved
  that block.
- `IrLoader` / `Ir` — resamples a cabinet impulse response to the host rate (`SincResampler`,
  64-tap windowed sinc, Kaiser β 8.6), trims it to a bounded length (200 ms) with a 5 ms
  raised-cosine fade-out, and normalises the 100 Hz–5 kHz band average to 0 dB so swapping IRs
  does not change perceived loudness. Control-thread only.
- `RigGraph` drains a lock-free `SpscRing<ParameterChange>` at the top of every block, before any
  node runs, and dispatches each change to the target node's RT-safe indexed
  `set_parameter(std::uint16_t, float)`. `TubeScreamer` (bench 544.1 -> 543.0 ns/sample) and
  `Jcm800` (bench 1433.2 -> 1419.9 ns/sample) move onto this path with ms-based smoothing,
  indexed parameters, and dB-calibrated level/trim controls. **Deviation from the design spec:**
  per-sample ramping of preamp/drive inside the 8x-oversampled loop is deferred to D6c/D6d, where
  those nonlinear stages are rewritten anyway; today every parameter, including the ones fed into
  the oversampled loop, uses a per-block `skip(frames)` step. At 20 ms and blocks up to 4096
  frames that step is at most ~8 % of the remaining distance to target, below the −40 dBFS click
  criterion the spec sets for D7.
- `Jcm800`'s cabinet runs on `StreamingConvolution` (one partition of latency, any host block
  size; 128-frame partition at or below 50 kHz, 256 above) behind an `RtSwapSlot`, so
  `set_cabinet(Ir)` can swap the loaded impulse response at runtime and `reclaim_cabinets()`
  frees the previous one once the audio thread has moved past it. The output limiter added in
  the CPU performance pass is removed in favour of a plain clamp.
- Calibration: `Calibration::kFullScaleVolts = 2.0f` (0 dBFS is defined as a 2 V peak at the
  amp's input). `Jcm800` gains two new parameters, Input trim and Output (both dB), for eight
  total; `TubeScreamer` gains Input trim (dB) alongside a dB-calibrated Level, for four total.
  Defaults are re-pinned so a −12 dBFS input lands JCM800's output around −6 dBFS and
  TubeScreamer's around −12 dBFS (Task 9 numbers unchanged; the re-pin commit notes a
  5.117951 gain ratio with every other stage bit-identical).
- `host_gui` routes every knob through the parameter ring instead of calling setters directly,
  loads cabinet IRs at runtime through a device-selection/cabinet-chooser flow, and stores the
  chosen cabinet IR path in saved presets. The bundled cabinet IR at `assets/ir/default_4x12.wav`
  is user-supplied, not committed; the loader falls back with a printed warning when it is
  missing.
- WDF adaptor tree deferred indefinitely (§4.5 amendment) — see the decision table in
  `docs/superpowers/specs/2026-08-26-aaa-audio-quality-design.md`.

## CPU performance pass (after D4)

- `sd` detects its compiler instead of assuming MSVC: pinned `cxx`, then the SushiStack
  workspace's intel/llvm clang++, then `clang++` on PATH, then CMake's platform default.
  `sd config` names the choice; `sd env` shows the build environment (vcvars snapshot on
  Windows). FMA contraction is pinned off for every compiler so the regression snapshots are
  toolchain-independent. intel/llvm clang++: Jcm800 1780 -> 1430 ns/sample (-20 %).

- `tests/benchmark/` (`sushidsp_bench`, run via `sd test -s benchmark`) — the timing harness
  every optimization is measured with; reports ns per output sample and the share of one
  core at 48 kHz. Registered as a single ctest case under the `benchmark` label (no
  assertion, so `sd test -s benchmark` passes `-V` to ctest to show the table).
- Release/RelWithDebInfo now build with AVX2 + FMA and link-time optimization (per target,
  `SUSHIDSP_ENABLE_AVX2` / `SUSHIDSP_ENABLE_LTO`). No fast-math.
- `DenormalGuard` — RAII flush-to-zero for the audio thread, engaged in the SDL2 callback.
- `NewtonRaphsonSolver::solve_joint` — one callable returns residual and derivative so the
  shared `exp` is evaluated once; the solver stops at a float fixed point instead of always
  running `max_iterations`. Jcm800 2.8 -> 1.8 us per sample, output bit-identical.
- `HalfbandUpsampler2x` is polyphase over a ring buffer; `HalfbandDownsampler2x` drops the
  per-sample delay-line shift. Bit-identical to the direct form, pinned by tests.
- Tried and rejected with numbers in the plan: a polynomial fast `exp` (slower than MSVC's
  `expf` and moves the Jcm800 snapshot 20x past tolerance), a 1-ulp Newton exit, and a
  SIMD tap sum (bounded at ~5 % and not bit-identical).

## D2 — WDF/DK solver core + convolution + oversampling

- `PartitionedConvolution` — uniform-partitioned overlap-add FFT convolution (cabinet IRs).
- `HalfbandUpsampler2x` / `HalfbandDownsampler2x` / `Oversampler8x` — windowed-sinc halfband
  cascade for 8x oversampling.
- `WdfResistor` / `WdfCapacitor` — linear Wave Digital Filter one-ports.
- `NewtonRaphsonSolver` + `DiodeClipperCircuit` — the shared nonlinear-circuit solver core,
  proven end to end by a one-stage diode-clipper circuit (DC-stable, odd-symmetric, correctly
  compressive for large input).
- WDF series/parallel adaptor trees deliberately deferred to D3/D4 — no multi-element circuit
  exists yet to validate that combining logic against.

## D1 — Device seam + standalone host

- `IAudioDevice` (injected output backend) + `Sdl2AudioDevice` (SDL2 implementation, isolated to
  its own target; `SUSHIDSP_BUILD_SDL2` makes it optional).
- `SpscRing<T>` — lock-free single-producer/single-consumer ring for control-to-RT commands.
- `sushidsp_host` — the standalone rig host; plays a diagnostic sine tone through a real device
  as the phase's device-and-playback proof.
- Rig-editor UI deliberately not built — nothing worth editing a rig around exists yet.

## D0 — Skeleton + math kernels

- Repository/CMake skeleton, the `sd` developer CLI.
- `IAudioNode` / `RigGraph` / `NodeDescriptor` — the core rig-graph contracts.
- Allocation-free transient `BufferPool`.
- `Fft` (real FFT) and `Biquad` (RBJ biquad filter).
- The RT-safety `operator new` trap test harness every later real-time-path class is held to.

## Pre-D0 — Design

- The design pivots from the original game-engine-integrated spatializer/synthesis/aero-thermo
  scope to a standalone amp/pedal DSP framework: a freely-editable rig of `IAudioNode`s, starting
  with JCM800 and Tube Screamer.
- SushiStack membership: `sushidsp` registered as a stack module (`ss add`/`ss link`, alias
  `sd`), with the shared `sushicore` CLI presentation layer — CLI/tooling only, no code
  dependency on any other stack module (spec §11a).
