# D6c — Tube Screamer Rebuilt on the DK Core Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The shipping `TubeScreamer` node stops being a memoryless inverting clipper between two
Butterworth biquads and becomes two `DkCircuit` blocks built node-for-node from
`docs/reference/spice/ts_clip/` and `docs/reference/spice/ts_tone/`, nulling against the committed
LTspice CSVs, inside one 8× region, at or near the spec's 0.25 ms budget — which requires making
the D6b solver roughly twice as fast first.

**Architecture:** Three layers, bottom up. (1) The D6b core gains one new element — an ideal
op-amp, stamped as a nullor — and one performance change: the device models expose a joint
`current_and_jacobian()`, the `NonlinearSystem` glue collapses to one method, and
`DkCircuit::process_sample` stops re-evaluating the currents after a converged solve. (2) The
product module gains `TubeScreamerCircuits.hpp`, a header-only description of the two circuits with
every component value named once, plus the diode-pair glue. (3) `TubeScreamer` itself keeps its four
parameters, its ports, its descriptor and its latency contract, and replaces its internals: input
biquad and `TubeScreamerClippingStage` out, clip block and tone block in, one oversampled region
around both.

**Tech Stack:** C++17 header-only DK core (`std::array`, `<cmath>`), GoogleTest (`sushidsp_tests`),
`tests/measure/` and `tests/measure/SpiceCsv.hpp` from D6a, `tests/benchmark/` harness,
`tests/golden/` regression files.

**Spec:** `docs/superpowers/specs/2026-08-26-aaa-audio-quality-design.md` — Phase D6c, with §D6a.4
(cost budget), §D6b (what exists), the "Acceptance summary" and the "Risks" rows binding. The D6b
plan `docs/superpowers/plans/2026-08-26-d6b-dk-circuit-core.md` and the design doc's "As built in
D6b" section describe the core this phase extends.

## Global Constraints

- Build/test only through the `sd` CLI: `sd build`, `sd test` (`-s unit|regression|benchmark|all`,
  `-t release`), `sd doxygen`. Never invoke cmake/ctest directly. `sd test` has **no** filter flag
  (`cli/sushidsp/cli.py`): run the whole suite.
- No git worktrees; work directly in the `main` checkout (user ruling, D5b). Commit only the files
  your task names; never `git add -A`.
- C++17 only. Allman braces. Types `PascalCase`, functions/variables `snake_case`, members trailing
  `_`, constants `kPascalCase`. No abbreviations — `feedback_capacitance_farads`, not `cf`.
- Every `.hpp`/`.cpp` (tests included) carries the 76-column boxed license header: copy it
  **verbatim** from `include/SushiDSP/core/PortLayout.hpp` and change only the filename line, padded
  so the closing `*/` stays in column 76. Below, `/* LICENSE HEADER — copy from PortLayout.hpp,
  filename line changed */` stands for that block.
- Doxygen `@brief`/`@param`/`@return`/`@tparam` on every public function, struct and template
  parameter, test-side helpers included.
- Comments only where the *why* is non-obvious. Every equation transcribed from this plan carries a
  one-line comment naming it.
- RT safety: `TubeScreamer::process` and everything it reaches — both `DkCircuit::process_sample`
  calls, both `update_matrices_if_dirty()` calls, the oversampler — must be allocation-free,
  lock-free, syscall-free and throw-free. Task 9 arms the existing trap over a *knob-moving* block
  so the re-derivation path is inside the trap window.
- **Double inside, float at the boundary.** Both DK blocks, both glue structs and every component
  constant are `double`. `float` appears where `TubeScreamer::process` reads and writes the host's
  buffers and where it drives `Oversampler8x`, which is a `float` API.
- Tests: `TEST(Unit_Dsp, …)` for unit cases; `TEST(Regression_Dsp, …)` for anything that renders
  seconds of audio (the golden files and the alias floor already follow this). Expectations are
  golden-vector: a closed form written out in this plan, a value computed here at double precision,
  or the committed LTspice CSV. Never the implementation's own output.
- **The CSVs exist.** `docs/reference/spice/ts_clip/{ts_clip_ac.csv, ts_clip_tran.csv}` and
  `docs/reference/spice/ts_tone/{ts_tone_ac.csv, ts_tone_tran.csv}` are committed: three stepped
  settings each, 200 AC points per setting, 3840 transient samples per setting at 192 kHz. No test
  in this plan may `GTEST_SKIP()`. A skip here is a failure.
- `_USE_MATH_DEFINES` before any include in a `.cpp` that uses `M_PI`, with the `#ifndef M_PI`
  fallback, as every existing file does.
- Numbers that bind this phase: transient null ≤ **−60 dB** band-limited to 10 kHz at each of the
  three stepped settings, for both blocks (the brief's number; §D6c's own table says −55 dB and is
  the documented floor if the tighter gate is missed — see Ruling 3); tone AC response within
  **±0.5 dB** of `ts_tone_ac.csv` from 20 Hz to 10 kHz at three settings; Newton non-convergence
  ≤ **0.01 %** with **zero** NaN fallbacks; alias floor ≥ **30 dB** better than the D5b −8.3 dB
  baseline (≤ −38.3 dB), the D6a target this phase inherits; cost ≤ **0.25 ms** per 256-frame block
  at 48 kHz; golden files null at ≤ −80 dB RMS after re-pinning.
- Component values are the ones in `docs/reference/spice/ts_clip/README.md` and
  `docs/reference/spice/ts_tone/README.md`, **exactly**, plus `docs/reference/spice/models/d1n914.sub`
  (`Is = 2.52 nA`, `n = 1.752`, `Rs = 0.568 Ω`, `Cjo = 4 pF`). A value that drifts from its
  `README.md` makes every null meaningless.
- Never touch `apps/host_gui`, `src/core/RigGraph*`, `modules/rig_preset`. `TubeScreamer`'s public
  API — the four parameters, their ids, order, kinds and ranges, `ports()`, `type_id()`,
  `latency_samples()`, `tube_screamer_descriptor()` — does not change.

### Rulings taken while writing this plan (spec deviations and choices the spec left open)

1. **The ideal op-amp enters `DkNetlist` as a nullor, not as a high-gain VCVS.** Both blocks need an
   op-amp and `DkNetlist` has no element that can express one: an op-amp is an *asymmetric* two-port
   constraint, sensing across one node pair and driving current into another, while every existing
   element (`resistor`, `capacitor`, `voltage_source`) stamps symmetrically. The nullor is
   the exact circuit-theory object: a nullator across the input pair (`V(p) − V(q) = 0`, no current)
   and a norator across the output pair (arbitrary voltage and current). It costs one augmented row
   and one augmented column, no state, no Newton branch, and it is the limit the reference's
   `Eop … 1e6` approaches to within about 1e-6 relative — 60 dB below the −60 dB null floor.
   *Rejected:* stamping a finite-gain VCVS. It needs exactly the same new asymmetric machinery, and
   then multiplies the augmented matrix's condition number by the gain. Verified while writing this
   plan, with the stamp Task 4 specifies: an inverting amplifier derives `H = −10` exactly, a
   non-inverting amplifier `H = 11.851063829787234` exactly, a follower `H = 1`, and an inverting
   one-pole matches its analytic bilinear transform to 9.77e-14 over 4096 samples.
2. **The drive control is a rheostat, wired the way the pedal wires it.** `Rf = 51 kΩ + Drive·500 kΩ`
   is a variable *resistance*, not a divider, and `DkNetlist::potentiometer` builds a three-terminal
   divider. It needs no new element: a pot becomes a rheostat by tying its wiper to one end, which
   is what the TS808 does. The netlist therefore splits `Rf` at an internal node `rfm`
   (`resistor(out, rfm, 51 kΩ)`) and adds `potentiometer(rfm, inv, inv, 500 kΩ)`. The dead
   `inv`–`inv` section stamps nothing: `DkCircuit::stamp_conductance` adds `+g` twice to the
   diagonal and subtracts it twice when both terminals are the same node, so the contribution
   cancels exactly. At `Drive = 0` the live section floors at `kMinimumPotOhms = 1 Ω`, 20 ppm of the
   51 kΩ in series with it — and the reference never steps below `DRIVE = 0.1` anyway.
   *Rejected:* adding a `rheostat()` element and generalising `PotentiometerEntry` into a
   `(minimum, span)` pair. It is the tidier abstraction, but it changes a D6b public struct that
   `tests/circuits/JcmV1aNetlist.hpp` and every D6d block build against, to buy something the
   hardware already expresses.
3. **The diodes' junction capacitance is part of the reference and goes into the netlist.**
   `d1n914.sub` declares `Cjo=4p`, so every LTspice run of `ts_clip` has 4 pF across each diode —
   8 pF in parallel with the 51 pF feedback cap, a 16 % increase. This is the same class of trap as
   D6b's 12AX7 interelectrode capacitances. Measured while writing this plan against
   `ts_clip_ac.csv`, over all three `DRIVE` steps and the whole 10 Hz–20 kHz sweep: with
   `Cf = 51 pF` the worst error is **1.15 dB at 20 kHz**; with `Cf = 51 pF + 2 × 4 pF = 59 pF` it is
   **0.004 dB**. The netlist therefore carries one capacitor of
   `kFeedbackFarads + 2 × kDiodeJunctionFarads` between `out` and `inv`, both constants named and
   both commented with the file they come from.
   *Deviation, stated:* SPICE's junction capacitance is voltage-dependent
   (`Cj = Cjo / sqrt(1 − v/VJ)`, `VJ = 1 V` by default) and ours is fixed at its zero-bias value. At
   the clipping excursion (|v| ≈ 0.39 V) the two junctions sum to about 8.9 pF rather than 8.0 — an
   11 % error on 16 % of the feedback capacitance, so the feedback pole sits about 1.6 % low. The
   transient nulls computed while writing this plan already carry that error and still land at
   −80.3 / −73.7 / −76.0 dB, so it costs headroom, not the gate. A nonlinear capacitor is out of
   scope for D6c. `Rs = 0.568 Ω` is also dropped: at the clip stage's largest diode current
   (≈ 21 µA) it contributes 12 µV.
4. **The `NonlinearSystem` glue exposes exactly one method, `currents_and_jacobian`.** D6b gave it
   two, `currents()` and `jacobian()`, and `DkCircuit` calls them back to back on every residual
   evaluation. For `Triode12AX7` that recomputes one `sqrt`, two `exp`, one `log1p` and one `pow`
   per evaluation; for `SiliconDiode` it doubles the `exp` count. The client always wants both, so
   two methods are not a narrower interface — they are one interface with a consistency hazard. One
   method, always both.
   *Rejected:* keeping `currents()` as an optional fast path. Nothing on the audio thread wants the
   currents alone once Ruling 6 lands.
5. **The models keep `current()` and `jacobian()`, reimplemented as wrappers over the joint form.**
   The physics is written once, in `current_and_jacobian()`; the two D6b entry points forward to it
   and discard the half they do not need. Every existing model test keeps compiling and keeps
   asserting the same numbers, the two forms cannot drift apart, and the only cost is paid by
   callers that are not on the audio thread.
   *Rejected:* deleting `current()`/`jacobian()`. It would churn three green test files for nothing,
   and `jacobian()` alone is genuinely the right call in a centred-difference test.
6. **The post-solve `currents()` call goes, on a contract the solver now states and a test now
   pins.** `DkCircuit::process_sample` today calls `nonlinear.currents(nonlinear_voltage_, …)` after
   `solver_.solve()` returns, re-evaluating the models at a point the solver has already evaluated.
   Reading `DampedNewtonSolver::solve` line by line: it returns `true` either at the
   absolute-tolerance check (the residual belongs to the current `v`) or after a step that sets
   `v = trial_v_` (the trial was the last point handed to `residual_and_jacobian`, whether the line
   search accepted at full damping, at a halving, or at the re-evaluated smallest finite damping).
   It returns `false` only through `fail()`, which restores `best_v_` — an *earlier* iterate. So:
   **on success the last model evaluation was at the returned iterate; on failure it was not.**
   Task 3 writes that sentence into the solver's Doxygen, adds a test that pins it, has the glue
   adapter write straight into `nonlinear_current_`, and re-evaluates only on the failure path
   (under 0.01 % of samples).
7. **The block shapes are `DkCircuit<3, 2, 1, 1>` (clip) and `DkCircuit<3, 0, 1, 1>` (tone).** The
   clip shape is the spec's. The tone shape is not: §D6c writes `DkCircuit<2,0,1,1>` and
   `ts_tone.asc` has **three** capacitors — `C5` 220 nF, `C6` 220 nF and `Cout` 1 µF. Anyone sizing
   a template from the spec alone sizes it one state short and `prepare()` returns `false`. Same
   correction class as D6b's Ruling 6; this plan states it so the surprise lands here.
8. **The two blocks cascade with no loading error, and that is why they are two blocks.** The clip
   stage's probe node is `out`, which is the op-amp's norator terminal: an ideal voltage source of
   zero output impedance. No current the tone block draws through `C5` can change it. Feeding the
   clip block's `y[n]` straight into the tone block's `u[n]` within the same sample is therefore
   *exact*, not an approximation, and no unit delay is needed between them.
9. **The pots stay linear this phase, and the tone knob keeps the sense it ships with.** §D6c says
   "audio-taper laws are applied to the normalised parameter before the pot enters"; this plan does
   not add one. A taper is a feel decision, it is invertible at any time, and adding it in the same
   commit that replaces the circuit would (a) break the direct correspondence with the reference's
   `DRIVE`/`TONE` steps and (b) make a regression impossible to attribute. `drive` maps to the drive
   pot's position unchanged, exactly as `feedback_resistance_for_drive` maps it today. `tone` is
   **inverted** on the way in: `ts_tone.asc` puts the wiper at the resonant node (bright) at
   `TONE = 0`, while the shipping knob is bright at `tone = 1`, so the position is `1 − tone` and a
   saved preset keeps meaning what it meant. Both mappings are one named function each in
   `TubeScreamerCircuits.hpp`, with the arithmetic in their Doxygen.
10. **`TubeScreamerClippingStage` is deleted; `DiodeClipperCircuit`'s retirement is proposed and
    gated.** The clipping stage is a private class of this module with no consumer once Task 7
    lands, so it and its five tests go in Task 7 with no ceremony. `DiodeClipperCircuit` is
    different: it is a public core header, and the D6b changelog left "whether it stays an oracle at
    all" as a D6c decision. This plan's recommendation is **retire it**, because its whole remaining
    job is being a second opinion in `test_dk_circuit.cpp`, it is off the true root by up to 2.8 mV
    on 1.3 % of samples, and that test already carries a stronger oracle (agreement with the true
    root of the same equation to 1e-11, by bisection inside the test). Task 10 does exactly that and
    nothing else, so the controller can drop the task whole and lose nothing else in the plan. **It
    must not run without an explicit yes** — deleting a public header is a boundary decision, not a
    plan decision.
11. **The SPICE transient pre-roll is exactly one pass, not eight.** `tools/spice/export.py` runs
    `tran 0 40m 20m 1u`: LTspice solves the DC operating point, runs 40 ms and saves the last 20 ms.
    Both TS blocks sit at zero at DC (there is no supply rail), so a DK block starting from zero
    state has exactly LTspice's initial condition, and the saved window is 20 whole cycles of the
    1 kHz stimulus. Replaying the window **once** therefore reproduces LTspice's own first 20 ms and
    the second pass is the comparison window. Replaying it more converges the DK block to the
    *periodic steady state* while the reference is still on its turn-on transient (`Cout` into
    `Rload` is a 1.59 Hz corner, τ = 100 ms), and the null gets worse. Measured while writing this
    plan, band-limited to 10 kHz, through `null_test`'s gain and lag alignment:

    | pre-roll passes | ts_clip (0.1 / 0.5 / 0.9) | ts_tone (0.1 / 0.5 / 0.9) |
    |---|---|---|
    | 0 | −30.4 / −30.8 / −30.9 | −22.4 / −22.3 / −22.5 |
    | **1** | **−80.3 / −73.7 / −76.0** | **−85.4 / −82.3 / −74.7** |
    | 2 | −80.3 / −73.7 / −76.0 | −49.5 / −59.7 / −74.6 |
    | 8 | −80.1 / −73.7 / −76.0 | −46.2 / −60.0 / −74.4 |

    D6b's `jcm_v1a` test uses eight passes and is right to: that block has a 360 V rail and a bias
    point the DK model must settle into from zero, which LTspice's `.op` handed it for free. Neither
    TS block does. Both facts belong in the test comments.
12. **Oversampling stays 8× for the whole node unless Task 5's measured gate says otherwise.** The
    estimate below puts 8× within about 10 % of the 0.25 ms budget *after* the Wave A/B performance
    work, and roughly 65 % over it without. That is close enough that the plan measures rather than
    assumes: Task 5 prices the two blocks alone and applies a written decision rule, and Contingency
    C1 (below) is what runs if the rule says the budget is missed.
13. **`ts_tone.asc`'s topology is taken as given and flagged, not fixed.** The reference is the
    binding acceptance and the DK block reproduces it. But the block that netlist describes is not a
    tone control: analysed in closed form below and confirmed against `ts_tone_ac.csv` to 0.004 dB,
    moving `TONE` from 0.1 to 0.9 drops the plateau by **19.08 dB** while the treble corner moves
    only from **807.2 Hz to 760.0 Hz**, a volume control with a fixed corner, and at noon the
    stage's insertion loss is **−30.4 dB**, which is why Task 8's calibration constant comes out
    near 23 (a +26 dB makeup the D6a chain did not need). `ts_tone/README.md` flags the topology as
    unverified against a traced TS808; these three numbers are the evidence that it is wrong.
    Changing `R7` from 10 kΩ to 1 kΩ does not fix it (the swing stays 19 dB and the corner stays
    static), so the fix would be a different network, a re-export of both `ts_tone` CSVs, and new
    values in one constant table. See Self-review, open question 1: **this is a decision the
    controller owes before Wave E.**
14. **The Tone knob spans the reference's own stepped range, 0.1 to 0.9, not the pot's mechanical
    ends.** Both ends of this network are degenerate. At `TONE = 1` the pot shorts the wiper to the
    op-amp's output, and since a nullor's output node has zero impedance the summing-junction
    equation collapses to `V(tsout)·(1/R7 + s·C6) = 0`: the stage outputs **nothing**. Working the
    closed form at `kMinimumPotOhms` gives a plateau of **−146 dB**, so the 1 Ω floor is all that
    stands between the shipping pedal and a knob position that mutes it. At `TONE = 0` the other
    section shorts, the feedback path from the output back to the wiper disappears, and the stage
    is a bare integrator held finite only by the same 1 Ω floor (plateau −21.2 dB, low pole
    3.16 Hz). Neither is a place to let a user's knob go. `tone_to_position` therefore maps the
    knob onto `[0.1, 0.9]`, precisely the span the committed CSVs step and validate, as
    `position = 0.9 − 0.8·tone`, with both endpoints named constants. The block tests set wiper
    positions directly and are unaffected.
    *This is a symptom, not a fix.* A real TS tone control does not mute at one end and does not
    swing the level 19 dB across its travel; both belong to open question 1, and if the topology is
    corrected this mapping should go back to the identity.

### The cost budget, and why the performance work comes first

D6b measured its 2-nonlinearity / 7-state block at **3762.5 ns per base-rate sample** at 8× —
0.963 ms per 256-frame block against a 0.12 ms budget, 8.0× over — and profiled about 90 % of it
into `Triode12AX7::current` and `::jacobian`, 63.8 ns per pair, called roughly 6.5 times per solve.
That is the number D6c inherits, and it is why Wave A comes before any Tube Screamer code.

The TS clip block is cheaper than that block in every dimension, so the budget question is a
calculation, not a guess. Instrumenting the clip block's Newton iteration at 384 kHz on a 220 Hz,
0.5 V stimulus while writing this plan gives **2.7 iterations per solve on average, 4 worst case,
and 5.7 residual-plus-Jacobian evaluations per solve** at every drive setting — near the triode
block's 6.5, so the iteration count is not where the saving is. The saving is per evaluation:

| Term | Today's shape | After Wave A/B |
|---|---|---|
| Model evaluations per solve | 5.7 joint-equivalent + 1 currents-only | 5.7 joint |
| Cost of one evaluation (2 diode branches) | 4 `exp` ≈ 24 ns | 2 `exp` ≈ 14 ns |
| Model per solve | ≈ 149 ns | ≈ 80 ns |
| Solver + `DkCircuit` matrix work (`S=3, N=2, I=1, O=1`) | ≈ 35 ns | ≈ 35 ns |
| **Per solve** | **≈ 184 ns** | **≈ 115 ns** |
| Clip block, 2048 solves per 256-frame block at 8× | 0.377 ms | **0.236 ms** |
| Tone block (`N = 0`, matrix work only, ≈ 10 ns × 2048) | 0.020 ms | 0.020 ms |
| `Oversampler8x` round trip (60.2 ns/sample, D6a) | 0.015 ms | 0.015 ms |
| Scaling, smoothers, level | 0.001 ms | 0.001 ms |
| **Node total against a 0.25 ms budget** | **≈ 0.414 ms** | **≈ 0.273 ms** |

The `exp` cost is calibrated from D6b's own measurement: 63.8 ns for a triode pair holding two
`sqrt`, four `exp`, two `log1p` and two `pow` implies roughly 5 ns per `exp`. The estimate is
therefore an extrapolation with an error bar near ±40 %, which straddles the budget. Task 5 measures
it. If the measurement lands over 0.25 ms, the reserve levers, largest first, are: the `DenseLu<2>`
specialisation (Task 2, worth roughly 5 ns per solve); collapsing the diode pair to a single
antiparallel branch, `DkCircuit<3,1,1,1>` with `i(w) = 2·Is·sinh(w / (n·Vt))` — the same two `exp`
but a scalar solve instead of a 2×2 one, worth roughly 15–20 ns per solve, at the cost of the
asymmetric-pair option §D6c explicitly wants kept open; and then Contingency C1.

**Contingency C1 — run the node at 4×.** Only if Task 5's gate fails and the two reserve levers
above do not close the gap. `Oversampler4x` is a sibling of `Oversampler8x` built from halfband
designs 1 and 2 of the existing cascade — one class in `include/SushiDSP/math/Oversampler.hpp` and
`src/math/Oversampler.cpp`, mirroring `Oversampler8x` method for method, plus a round-trip case
beside the existing `Oversampler8x` ones in `tests/functional/unit/test_oversampler.cpp`. The
acceptance then becomes the alias floor: **4× must still reach ≤ −38.3 dB** (the D6a target of
30 dB over the D5b −8.3 dB baseline). If it cannot, the node stays at 8× and the CPU overrun is
reported with its measured figure, because a pedal that aliases is a worse product than a pedal that
costs 0.3 ms — Priority 2 loses to the spec's own reason for existing. Do not loosen the budget in
`tests/benchmark/main.cpp` either way; report the number.

**What the alias floor should do, and why.** D6a left TubeScreamer at **−9.6 dB** against a
−38.3 dB target and diagnosed it precisely: the worst alias is the 21st harmonic of an 18 kHz tone
(378 kHz) folding about the 8× domain's own 192 kHz Nyquist, generated *inside* the oversampled
region where no halfband can reach it. The D6a clipper is memoryless — a scalar Newton solve of a
diode pair with no capacitor anywhere — so its loop bandwidth is infinite and it generates that
harmonic at full strength. The rebuilt stage cannot: the physical feedback network is `Rf ∥ 59 pF`,
a real pole at **26 708 Hz / 8 962 Hz / 5 384 Hz** for `DRIVE = 0.1 / 0.5 / 0.9`, so by 378 kHz the
loop gain is 23 to 37 dB down and still falling at 6 dB/octave. That is the physical mechanism the
D6a note called "the diode clipper's C¹ curve rework and the stage rebuild planned for D6c", and
Task 9 is where it gets measured. §D6c also asks for ≤ −85 dB; nothing in this analysis predicts
−85 dB, and Self-review open question 3 carries that gap to the controller rather than pretending it
away.

### Parallelism map (for the controller)

- **Task 0** (controller, inline): registers the three new test files in `tests/CMakeLists.txt` with
  stubs, so no task in Waves A–F touches CMake. The only other CMake edits in the plan are Task 7's
  (deleting the clipping stage from two lists) and Task 10's (deleting `DiodeClipperCircuit`), and
  neither runs beside another CMake-touching task.
- **Wave A**, independent, run concurrently: **Task 1** (joint `current_and_jacobian` on the three
  device models) and **Task 2** (`DenseLu<2>` plus the `DampedNewtonSolver` last-evaluation
  contract). Disjoint files, disjoint tests.
- **Wave B**, serial: **Task 3** (`DkCircuit` consumes the joint form; the redundant post-solve
  `currents()` goes; re-benchmark). Needs Tasks 1 and 2.
- **Wave C**, serial: **Task 4** (the nullor element). It depends on Task 3 only because both edit
  `DkCircuit.hpp`; there is no logical dependency, so the controller may run Task 4 first and Task 3
  second — but never together.
- **Wave D**, serial: **Task 5** (`TubeScreamerCircuits.hpp`, `tests/circuits/DkBlockNode.hpp`, the
  clip-block test, the two benchmark cases, **the oversampling gate**). Needs Task 4.
- **Wave E**, run concurrently once the controller has answered open question 1: **Task 6** (the
  tone-block test) and **Task 7** (the module rewrite). Disjoint files.
- **Wave F**, run concurrently: **Task 8** (level calibration, the module's unit tests, golden
  re-pin) and **Task 9** (alias floor, RT traps, benchmark against the budget). Disjoint files;
  Task 9 changes no behaviour, so Task 8's re-pinned goldens stay valid.
- **Wave G**, serial: **Task 10** (`DiodeClipperCircuit` retirement — *gated on the controller*),
  then **Task 11** (documentation).

Model per dispatch: Tasks 3, 4 and 7 fix interfaces or data ownership and run on `opus`; Task 5
derives a circuit and runs on `opus`; Tasks 1, 2, 6, 8, 9, 10, 11 are implementation or mechanical
and run on `sonnet`.

---

## The two circuits being implemented

Every task below transcribes from this section. Read it once before touching code. Node numbering,
component values and probe nodes come from the two SPICE `README.md` files node for node; the closed
forms were derived and checked against the committed CSVs while writing this plan, and the check
figures are quoted so an implementer can tell a transcription error from a solver error.

### Clip block — `docs/reference/spice/ts_clip/`

| Node | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|
| name | `src` | `in` | `out` | `inv` | `gnd1` | `rfm` |

`rfm` is not in the reference netlist: it is the internal node that splits `Rf` into its fixed
51 kΩ and its 500 kΩ rheostat section (Ruling 2). Two resistors in series through a node that
nothing else touches is the same circuit.

| Element | Value | Nodes | Netlist call |
|---|---|---|---|
| `V1` | input | `src`–gnd | `voltage_source(1, 0)` |
| `Cin` | 1 µF | `src`–`in` | `capacitor(1, 2, kInputFarads)` |
| `Rin` | 510 kΩ | `in`–gnd | `resistor(2, 0, kInputLeakOhms)` |
| `Rf` fixed | 51 kΩ | `out`–`rfm` | `resistor(3, 6, kFeedbackFixedOhms)` |
| `Rf` variable | 0…500 kΩ | `rfm`–`inv` | `potentiometer(6, 4, 4, kDrivePotOhms)` |
| `Cf` + 2·`Cjo` | 51 pF + 8 pF | `out`–`inv` | `capacitor(3, 4, kFeedbackFarads + 2.0 * kDiodeJunctionFarads)` |
| `D1` | 1N914 | anode `out`, cathode `inv` | `nonlinear(3, 4)` |
| `D2` | 1N914 | anode `inv`, cathode `out` | `nonlinear(4, 3)` |
| `Rg` | 4.7 kΩ | `inv`–`gnd1` | `resistor(4, 5, kGroundLegOhms)` |
| `Cg` | 47 nF | `gnd1`–gnd | `capacitor(5, 0, kGroundLegFarads)` |
| `Eop` | ideal | senses `in`–`inv`, drives `out`–gnd | `nullor(2, 4, 3, 0)` |
| probe | | `out`–gnd | `output(3, 0)` |

`node_count = 6`, `S = 3`, `N = 2`, `I = 1`, `O = 1`, one nullor, one potentiometer, five resistors
(`Rin`, the fixed `Rf`, both pot sections, `Rg`). Shape: **`DkCircuit<3, 2, 1, 1>`**.

**Closed-form small-signal response** (the diodes linearised at `v = 0`, which is what an LTspice
`.ac` does), with `g_d = Is / (n·Vt)` per diode:

```
n·Vt   = 1.752 × 0.025864          = 0.045313728 V
g_d    = 2.52e-9 / 0.045313728     = 5.5612285972145123e-8 S      (2·g_d = 8.99 MΩ)
Yf(s)  = 1/Rf + s·Cf_total + 2·g_d                                 Cf_total = 59 pF
Hin(s) = s·Cin / (s·Cin + 1/Rin)                                   corner 0.312069 Hz
H(s)   = Hin(s) · [ 1 + s·Cg / ((1 + s·Cg·Rg) · Yf(s)) ]
```

Derivation: the nullator forces `V(inv) = V(in)`, and `in` is unloaded (a nullator draws no
current), so `V(in)` is the `Cin`/`Rin` high-pass alone. The `Rg`+`Cg` leg then draws
`V(inv)·s·Cg/(1 + s·Cg·Rg)` out of `inv`, and every bit of it arrives through the feedback
admittance `Yf` from `out`.

Corners and gains, computed at double precision here:

| `DRIVE` | `Rf` | midband `1 + Rf/Rg` | feedback pole `1/(2π·Rf·Cf_total)` |
|---|---|---|---|
| 0.1 | 101 kΩ | 22.489362 (27.039543 dB) | 26 708.33 Hz |
| 0.5 | 301 kΩ | 65.042553 (36.263952 dB) | 8 961.93 Hz |
| 0.9 | 501 kΩ | 107.595745 (40.635902 dB) | 5 384.31 Hz |

`Rg·Cg` corner: `1/(2π × 4700 × 47e-9) = 720.4841 Hz` — the TS's famous 720 Hz.

Spot values of `20·log10|H|`, which Task 5 asserts against:

| Hz | `DRIVE` 0.1 | 0.5 | 0.9 |
|---|---|---|---|
| 100 | 10.1534 | 18.8134 | 22.9649 |
| 1000 | 25.1395 | 34.1273 | 38.2321 |
| 10000 | 26.3776 | 32.6157 | 34.0365 |

Checked against `ts_clip_ac.csv` over all 200 points × 3 settings: worst deviation **0.004 dB**.

**Diode goldens** (`SiliconDiode` defaults, `1/(n·Vt) = 22.068367449`). The antiparallel pair sees
`v[0] = w` and `v[1] = −w` for `w = V(out) − V(inv)`, so its total current is
`i(w) = Is(e^{w/nVt} − 1) − Is(e^{−w/nVt} − 1) = 2·Is·sinh(w/nVt)` and its conductance is
`jacobian(w) + jacobian(−w)`:

| `w` | `i(w)` | `di/dw` | `1 / (di/dw)` |
|---|---|---|---|
| 0.0 V | 0 | 1.112245719e-7 S | 8.990819e6 Ω |
| 0.2 V | 2.080515143e-7 A | 4.592704262e-6 S | 2.177366e5 Ω |
| 0.3 V | 1.890822805e-6 A | 4.172752069e-5 S | 2.396500e4 Ω |
| 0.38 V | 1.105058316e-5 A | 2.438683551e-4 S | 4.100573e3 Ω |
| 0.4 V | 1.718179822e-5 A | 3.791742530e-4 S | 2.637310e3 Ω |
| 0.5 V | 1.561297429e-4 A | 3.445528538e-3 S | 2.902312e2 Ω |

**Clipping threshold**, the diode-pair voltage at which the pair carries as much current as `Rf`
(`w / Rf = 2·Is·sinh(w / nVt)`, bisected to machine precision here):

| `DRIVE` | `Rf` | `w*` | pair current at `w*` |
|---|---|---|---|
| 0.0 | 51 kΩ | 0.359645534 V | 7.051873e-6 A |
| 0.1 | 101 kΩ | 0.323945681 V | 3.207383e-6 A |
| 0.5 | 301 kΩ | 0.265437577 V | 8.818524e-7 A |
| 0.9 | 501 kΩ | 0.237267523 V | 4.735879e-7 A |
| 1.0 | 551 kΩ | 0.231925300 V | 4.209171e-7 A |

Sanity check against the reference transient, `DRIVE = 0.1`, 100 mV at 1 kHz: at the input peak
`V(inv) = V(in) ≈ 0.1 V` drives `0.1 / |4700 − j3386.28| = 17.26 µA` into the ground leg, and the
`w` that carries it through `Rf ∥ diodes` is 0.3886 V, so `V(out) = 0.1 + 0.3886 = 0.4886 V`.
`ts_clip_tran.csv` peaks at **0.481683 V**. The 1.4 % gap is the `Cf` current and phase this
back-of-envelope drops; the DK block, integrated properly, peaks at **0.481659 V**.

### Tone block — `docs/reference/spice/ts_tone/`

| Node | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| name | `src` | `tp1` | `tw` | `inv` | `tsout` | `lw` | `out` |

| Element | Value | Nodes | Netlist call |
|---|---|---|---|
| `V1` | input | `src`–gnd | `voltage_source(1, 0)` |
| `C5` | 220 nF | `src`–`tp1` | `capacitor(1, 2, kResonantFarads)` |
| `R6` | 1 kΩ | `tp1`–gnd | `resistor(2, 0, kResonantOhms)` |
| tone pot | 20 kΩ | `tp1`–`tw`–`tsout` | `potentiometer(2, 3, 5, kTonePotOhms)` |
| `R7` | 10 kΩ | `tw`–`inv` | `resistor(3, 4, kLowPassLegOhms)` |
| `C6` | 220 nF | `inv`–`tsout` | `capacitor(4, 5, kLowPassLegFarads)` |
| `Eop` | ideal | senses gnd–`inv`, drives `tsout`–gnd | `nullor(0, 4, 5, 0)` |
| level pot | 100 kΩ | `tsout`–`lw`–gnd | `potentiometer(5, 6, 0, kLevelPotOhms)` |
| `Cout` | 1 µF | `lw`–`out` | `capacitor(6, 7, kOutputFarads)` |
| `Rload` | 100 kΩ | `out`–gnd | `resistor(7, 0, kLoadOhms)` |
| probe | | `out`–gnd | `output(7, 0)` |

`node_count = 7`, `S = 3`, `N = 0`, `I = 1`, `O = 1`, one nullor, two potentiometers, seven
resistors. Shape: **`DkCircuit<3, 0, 1, 1>`** (Ruling 7).

The level pot is present so the block is the reference node for node, and is held at the
reference's `LEVEL = 1`. `DkNetlist::potentiometer` makes the `a`-to-wiper section `ohms × position`
while the reference writes `Rla = 100k·(1 − LEVEL)`, so the position is `1 − LEVEL` — **0.0** here.
`kMinimumPotOhms` then floors `Rla` at 1 Ω where the reference uses 1 mΩ; both are in series with
`Cout` into 100 kΩ, so the difference is 10 ppm. The shipping `Level` knob stays the calibrated dB
gain D5b defined and does not drive this pot (Global Constraints: the public API does not change).

**Closed-form response.** With `Ga = 1/Rta`, `Gb = 1/Rtb`, `GR = 1/R7`, `G6 = 1/R6`,
`Gsum = Ga + Gb + GR`, `D1(s) = s·C5 + G6 + Ga`:

```
Htone(s) = −Ga·GR·s·C5 / [ s·C6·(D1(s)·Gsum − Ga²) + D1(s)·Gb·GR ]
H(s)     = Htone(s) · s·τ / (1 + s·τ)        τ = Rload·Cout = 0.1 s, corner 1.591549 Hz
```

Derivation: the nullator holds `inv` at ground, so `V(tw)/R7 + s·C6·V(tsout) = 0`; substituting into
KCL at `tw` and then at `tp1` (which the tone pot loads — the reason a bare `C5`/`R6` high-pass
gives the wrong answer) leaves the biquadratic denominator above. As a polynomial in `s` the
denominator is `a2 s² + a1 s + a0` with

```
a2 = C5·C6·Gsum
a1 = C6·((G6 + Ga)·Gsum − Ga²) + C5·Gb·GR
a0 = (G6 + Ga)·Gb·GR
b1 = −Ga·GR·C5                                (the numerator's only term; the zero is at s = 0)
```

so the plateau between the two poles is exactly `b1 / a1`. Values, at double precision here:

| `TONE` | `Rta` / `Rtb` | plateau `b1/a1` | lower pole | upper pole |
|---|---|---|---|---|
| 0.1 | 2 kΩ / 18 kΩ | −0.067669173 (−23.392183 dB) | 8.242319 Hz | 807.150879 Hz |
| 0.5 | 10 kΩ / 10 kΩ | −0.030303030 (−30.370279 dB) | 24.893080 Hz | 770.881636 Hz |
| 0.9 | 18 kΩ / 2 kΩ | −0.007518797 (−42.477033 dB) | 55.443640 Hz | 759.949559 Hz |

Spot values of `20·log10|H|`, which Task 6 asserts against:

| Hz | `TONE` 0.1 | 0.5 | 0.9 |
|---|---|---|---|
| 100 | −23.4006 | −30.4289 | −43.1051 |
| 1000 | −27.3439 | −34.3828 | −46.2428 |
| 10000 | −45.1930 | −52.3802 | −64.2748 |

Checked against `ts_tone_ac.csv` over all 200 points × 3 settings: worst deviation **0.004 dB**.
Note what the table says about the control — see Ruling 13 and Self-review open question 1. The
`C5`/`R6` high-pass on its own would sit at `1/(2π × 1000 × 220e-9) = 723.4316 Hz`; the loaded
upper pole lands 36 to 47 Hz above it.

### The node the two blocks live in

```
in (float) ─▶ × input_trim × kFullScaleVolts ─▶ upsample 8× ─▶ ┌──────────────────┐
                                                               │ clip DkCircuit   │
                                                               │ <3,2,1,1>        │
                                                               └────────┬─────────┘
                                                                        │ volts, same sample
                                                               ┌────────▼─────────┐
                                                               │ tone DkCircuit   │
                                                               │ <3,0,1,1>        │
                                                               └────────┬─────────┘
 out (float) ◀─ × level × kLevelUnityGain ÷ kFullScaleVolts ◀─ downsample ◀───┘
```

One `Oversampler8x`, one loop over the oversampled samples doing both blocks (Ruling 8). Knob
handling is once per block, before the loop: `set_potentiometer` then `update_matrices_if_dirty` on
each circuit, which costs nothing while the knob is still and 1.56 µs when it moves (D6b's measured
re-derivation, on a bigger circuit than either of these).

---

### Task 0: Scaffolding (controller does this inline before dispatching Wave A)

**Files:**
- Modify: `tests/CMakeLists.txt`
- Create: `tests/functional/unit/test_dk_op_amp.cpp`, `test_ts_clip_stage.cpp`,
  `test_ts_tone_stage.cpp` (stubs)

- [ ] **Step 1: Register the three test files.** In `add_executable(sushidsp_tests …)`, under
`# --- Circuits ---`, after `functional/unit/test_dk_triode_stage.cpp`:

```cmake
    functional/unit/test_dk_op_amp.cpp
    functional/unit/test_ts_clip_stage.cpp
    functional/unit/test_ts_tone_stage.cpp
```

Nothing else in this file changes: `SUSHIDSP_SPICE_DIR`, `SUSHIDSP_GOLDEN_DIR` and the `tests/`
include root all exist already.

- [ ] **Step 2: Create the three stubs**, each with the license header and a single
`#include <gtest/gtest.h>` so the binary links.

- [ ] **Step 3: `sd build` succeeds; `sd test -s unit` green; commit**

```bash
git add tests/CMakeLists.txt tests/functional/unit/test_dk_op_amp.cpp tests/functional/unit/test_ts_clip_stage.cpp tests/functional/unit/test_ts_tone_stage.cpp
git commit -m "D6c: scaffold the op-amp and Tube Screamer block test units so no later task shares a CMake edit"
```

---

### Task 1: Joint `current_and_jacobian()` on the three device models

**Files:**
- Modify: `include/SushiDSP/circuits/models/SoftTransition.hpp`
- Modify: `include/SushiDSP/circuits/models/SiliconDiode.hpp`
- Modify: `include/SushiDSP/circuits/models/Triode12AX7.hpp`
- Modify: `include/SushiDSP/circuits/models/PentodeEL34.hpp`
- Modify: `tests/functional/unit/test_silicon_diode.cpp`, `test_triode_12ax7.cpp`,
  `test_pentode_el34.cpp`

**Interfaces (consumed by Task 3 and by every D6c/D6d glue struct):**

```cpp
void SiliconDiode::current_and_jacobian(double voltage, double& current_amps,
                                        double& conductance_siemens) const;
void Triode12AX7::current_and_jacobian(const std::array<double, 2>& v, std::array<double, 2>& i,
                                       std::array<std::array<double, 2>, 2>& j) const;
void PentodeEL34::current_and_jacobian(const std::array<double, 3>& v, std::array<double, 3>& i,
                                       std::array<std::array<double, 3>, 3>& j) const;
```

`current()` and `jacobian()` stay, reimplemented as one-line wrappers (Ruling 5). The physics is
written exactly once, in the joint form.

- [ ] **Step 1: Write the failing tests.** One case per model, appended to its existing file. They
assert the joint form against the *same* hand-computed operating points the D6b cases already carry,
so a joint form that drifts from the physics fails on the physics, not on self-consistency; and they
assert exact agreement with the two wrappers, so the three entry points can never disagree.

```cpp
TEST(Unit_Dsp, SiliconDiodeJointFormAgreesWithTheSeparateEntryPoints)
{
    const SushiDSP::SiliconDiode diode;
    for (const double voltage : { -60.0, -0.5, 0.0, 0.4, 0.6, 0.7, 1.81254912, 60.0 })
    {
        double current = 0.0;
        double conductance = 0.0;
        diode.current_and_jacobian(voltage, current, conductance);
        EXPECT_DOUBLE_EQ(current, diode.current(voltage)) << voltage;
        EXPECT_DOUBLE_EQ(conductance, diode.jacobian(voltage)) << voltage;
    }
    // The same three Shockley points the D6b case pins, read out of the joint form.
    double current = 0.0;
    double conductance = 0.0;
    diode.current_and_jacobian(0.4, current, conductance);
    EXPECT_NEAR(current, 1.71792785943e-5, 1e-16);
    EXPECT_NEAR(conductance, 3.79174244818e-4, 1e-15);
    diode.current_and_jacobian(0.6, current, conductance);
    EXPECT_NEAR(current, 1.41873699671e-3, 1e-14);
    EXPECT_NEAR(conductance, 3.13092649696e-2, 1e-13);
    diode.current_and_jacobian(0.7, current, conductance);
    EXPECT_NEAR(current, 1.2891979355e-2, 1e-13);
    EXPECT_NEAR(conductance, 2.84504993167e-1, 1e-12);
}

TEST(Unit_Dsp, SiliconDiodeAntiparallelPairMatchesTwoTimesSaturationCurrentTimesSinh)
{
    // The D6c clip stage's nonlinear port is v[0] = w, v[1] = -w across one pair, so the pair's
    // total current is Is(e^(w/nVt) - 1) - Is(e^(-w/nVt) - 1) = 2 Is sinh(w / nVt) and its
    // conductance is jacobian(w) + jacobian(-w). n*Vt = 1.752 * 0.025864 = 0.045313728 V, so
    // 1/(n*Vt) = 22.068367449. Values computed at double precision in the D6c plan:
    //   w = 0.2  : i = 2.080515143e-7 A,  di/dw = 4.592704262e-6 S
    //   w = 0.3  : i = 1.890822805e-6 A,  di/dw = 4.172752069e-5 S
    //   w = 0.38 : i = 1.105058316e-5 A,  di/dw = 2.438683551e-4 S
    //   w = 0.4  : i = 1.718179822e-5 A,  di/dw = 3.791742530e-4 S
    //   w = 0.5  : i = 1.561297429e-4 A,  di/dw = 3.445528538e-3 S
    const SushiDSP::SiliconDiode diode;
    struct Point { double w, current, conductance; };
    const Point points[5] = {
        { 0.2,  2.080515143e-7, 4.592704262e-6 },
        { 0.3,  1.890822805e-6, 4.172752069e-5 },
        { 0.38, 1.105058316e-5, 2.438683551e-4 },
        { 0.4,  1.718179822e-5, 3.791742530e-4 },
        { 0.5,  1.561297429e-4, 3.445528538e-3 },
    };
    for (const Point& point : points)
    {
        double forward_current = 0.0, forward_conductance = 0.0;
        double reverse_current = 0.0, reverse_conductance = 0.0;
        diode.current_and_jacobian(point.w, forward_current, forward_conductance);
        diode.current_and_jacobian(-point.w, reverse_current, reverse_conductance);
        EXPECT_NEAR(forward_current - reverse_current, point.current, 1e-9 * point.current)
            << point.w;
        EXPECT_NEAR(forward_conductance + reverse_conductance, point.conductance,
                    1e-9 * point.conductance) << point.w;
    }
}
```

and, in `test_triode_12ax7.cpp` and `test_pentode_el34.cpp`, the same shape:

```cpp
TEST(Unit_Dsp, Triode12AX7JointFormAgreesWithTheSeparateEntryPointsToWithinOneUlpScale)
{
    // The joint form computes E1^kx as E1^(kx-1) * E1 rather than calling pow twice, and takes
    // softplus and logistic from one exp. Both are the same value to a couple of ulp, not
    // bit-for-bit, so this compares at 1e-13 relative rather than with EXPECT_DOUBLE_EQ; the
    // exact zeros (the whole grid row's Vpk entry, and plate cutoff) are still exact.
    const SushiDSP::Triode12AX7 triode;
    for (const double vgk : { -8.0, -3.0, -1.5, 0.0, 1.0 })
    {
        for (const double vpk : { -10.0, 5.0, 100.0, 250.0, 400.0 })
        {
            const std::array<double, 2> v{ vgk, vpk };
            std::array<double, 2> separate_currents{};
            std::array<std::array<double, 2>, 2> separate_jacobian{};
            triode.current(v, separate_currents);
            triode.jacobian(v, separate_jacobian);
            std::array<double, 2> joint_currents{};
            std::array<std::array<double, 2>, 2> joint_jacobian{};
            triode.current_and_jacobian(v, joint_currents, joint_jacobian);
            for (std::size_t branch = 0; branch < 2; ++branch)
            {
                EXPECT_NEAR(joint_currents[branch], separate_currents[branch],
                            1e-13 * std::abs(separate_currents[branch]) + 1e-300)
                    << vgk << " " << vpk << " " << branch;
                for (std::size_t input = 0; input < 2; ++input)
                {
                    EXPECT_NEAR(joint_jacobian[branch][input], separate_jacobian[branch][input],
                                1e-13 * std::abs(separate_jacobian[branch][input]) + 1e-300)
                        << vgk << " " << vpk << " " << branch << " " << input;
                }
            }
            EXPECT_DOUBLE_EQ(joint_jacobian[0][1], 0.0);
        }
    }
    // The classic quiescent point, straight out of the joint form (D6b Task 3's golden vector).
    std::array<double, 2> currents{};
    std::array<std::array<double, 2>, 2> jacobian{};
    triode.current_and_jacobian({ -1.5, 250.0 }, currents, jacobian);
    EXPECT_NEAR(currents[1], 1.99187388685e-3, 1e-11);
    EXPECT_NEAR(jacobian[1][0], 2.45547036171e-3, 1e-11);
    EXPECT_NEAR(jacobian[1][1], 2.58169362129e-5, 1e-13);
}

TEST(Unit_Dsp, PentodeEL34JointFormAgreesWithTheSeparateEntryPointsToWithinOneUlpScale)
{
    // Same shape as the triode case, over Vg1k in {-45, -37, -20, -5, 0},
    // Vg2k in {1e-4, 50, 300, 400} (the first below kScreenFloorVolts, so the clamped column is
    // exercised) and Vpk in {20, 100, 250, 400}; then the -37 V / 400 V / 400 V idle point out of
    // the joint form against D6b Task 4's golden vector: Ip = 3.34149261301e-2 A,
    // dIp/dVg1k = 4.98316907313e-3 S, Ig2 = 0.
}
```

- [ ] **Step 2: `sd build` — expected: FAIL to compile (no `current_and_jacobian`).**

- [ ] **Step 3: Add the joint softplus/logistic to `SoftTransition.hpp`** and make the two existing
helpers forward to it, so there is one implementation of each curve rather than two that can drift:

```cpp
        /// @brief Evaluates softplus and its derivative together from one exponential.
        ///
        /// softplus() and logistic() each cost one exp(); a caller that needs both — which every
        /// device model's joint current-and-Jacobian entry point does — would otherwise pay for
        /// two. The branches are the same overflow guards the two single-value helpers carried.
        /// @param t Argument in the units of the caller's transition scale.
        /// @param softplus_value Receives ln(1 + exp(t)).
        /// @param logistic_value Receives 1 / (1 + exp(-t)).
        inline void softplus_and_logistic(double t, double& softplus_value, double& logistic_value)
        {
            // ln(1 + exp(t)) - t is 9.24e-14 at t = 30 and falls from there, so the linear branch
            // is exact to double precision and keeps exp(t) away from its ~709 overflow.
            if (t >= 30.0)
            {
                softplus_value = t;
                logistic_value = 1.0 / (1.0 + std::exp(-t));
                return;
            }
            // exp(t) underflows gracefully to zero for very negative t, giving softplus 0 and
            // logistic 0 rather than the inf that exp(-t) would produce.
            const double exponential = std::exp(t);
            softplus_value = std::log1p(exponential);
            logistic_value = exponential / (1.0 + exponential);
        }

        inline double softplus(double t)
        {
            double softplus_value = 0.0;
            double logistic_value = 0.0;
            softplus_and_logistic(t, softplus_value, logistic_value);
            return softplus_value;
        }

        inline double logistic(double t)
        {
            double softplus_value = 0.0;
            double logistic_value = 0.0;
            softplus_and_logistic(t, softplus_value, logistic_value);
            return logistic_value;
        }
```

Keep each helper's existing Doxygen block; only the bodies move.

- [ ] **Step 4: `SiliconDiode::current_and_jacobian`.** One `exp` serves both outputs; the two
extrapolation branches are the ones already in the file.

```cpp
        /// @brief Evaluates the diode current and its derivative from a single exponential.
        ///
        /// The DK solver always needs both, and current()/jacobian() each cost one exp(); this
        /// entry point costs one for the pair. current() and jacobian() forward here, so the
        /// Shockley law and its linear extrapolation exist in exactly one place.
        /// @param voltage The voltage across the diode (anode minus cathode), in volts.
        /// @param current_amps Receives the diode current in amperes. Always finite.
        /// @param conductance_siemens Receives di/dv in siemens. Always finite.
        void current_and_jacobian(double voltage, double& current_amps,
                                  double& conductance_siemens) const
        {
            if (voltage > knee_voltage_)
            {
                current_amps = knee_current_ + knee_jacobian_ * (voltage - knee_voltage_);
                conductance_siemens = knee_jacobian_;
                return;
            }
            if (voltage < -knee_voltage_)
            {
                current_amps = -knee_current_ + knee_jacobian_ * (voltage + knee_voltage_);
                conductance_siemens = knee_jacobian_;
                return;
            }
            const double exponential = std::exp(voltage * inverse_scaled_thermal_voltage_);
            current_amps = saturation_current_ * (exponential - 1.0);
            conductance_siemens = saturation_current_ * inverse_scaled_thermal_voltage_ * exponential;
        }

        double current(double voltage) const
        {
            double current_amps = 0.0;
            double conductance_siemens = 0.0;
            current_and_jacobian(voltage, current_amps, conductance_siemens);
            return current_amps;
        }

        double jacobian(double voltage) const
        {
            double current_amps = 0.0;
            double conductance_siemens = 0.0;
            current_and_jacobian(voltage, current_amps, conductance_siemens);
            return conductance_siemens;
        }
```

Keep the existing Doxygen on `current()` and `jacobian()` verbatim.

- [ ] **Step 5: `Triode12AX7::current_and_jacobian`.** One `sqrt`, two `exp` (via two
`softplus_and_logistic` calls) and one `pow`, against the two-call pair's two `sqrt`, four `exp`,
two `log1p` and two `pow`:

```cpp
        /// @brief Evaluates both branch currents and the 2x2 Jacobian at one operating point.
        ///
        /// The plate law's root, softplus, logistic and E1^(kx-1) are shared between the current
        /// and its derivative, and E1^kx is formed as E1^(kx-1) * E1 rather than by a second pow.
        /// current() and jacobian() forward here.
        /// @param v Branch voltages: v[0] = Vgk, v[1] = Vpk, in volts.
        /// @param i Receives the branch currents: i[0] = Ig, i[1] = Ip, in amperes.
        /// @param j Receives the 2x2 Jacobian j[a][b] = d i_a / d v_b in siemens.
        void current_and_jacobian(const std::array<double, 2>& v, std::array<double, 2>& i,
                                  std::array<std::array<double, 2>, 2>& j) const
        {
            const double vgk = v[0];
            const double vpk = v[1];

            const double root = std::sqrt(kKvb + vpk * vpk);                        // root
            const double t = kKp * (1.0 / kMu + vgk / root);                        // t
            double       s = 0.0;                                                   // s
            double       sigma = 0.0;                                               // sigma
            models::softplus_and_logistic(t, s, sigma);
            const double e1 = vpk * s / kKp;                                        // E1

            double grid_softplus = 0.0;
            double grid_logistic = 0.0;
            models::softplus_and_logistic((vgk - kGridThresholdVolts) / kGridTransitionVolts,
                                          grid_softplus, grid_logistic);

            const double de1_dvgk = vpk * sigma / root;                             // dE1/dVgk
            const double de1_dvpk = s / kKp
                                    - vpk * vpk * vgk * sigma / (root * root * root);  // dE1/dVpk

            // kx > 1, so E1^(kx-1) and with it both Ip and its derivatives vanish continuously as
            // E1 reaches zero; the single guard here covers the whole plate branch.
            const double e1_power = e1 <= 0.0 ? 0.0 : std::pow(e1, kKx - 1.0);      // E1^(kx-1)
            const double dip_de1 = 2.0 * kKx * e1_power / kKg1;

            i[0] = kGridCurrentScale * grid_softplus;                               // Ig
            i[1] = 2.0 * e1_power * e1 / kKg1;                                      // Ip

            j[0][0] = (kGridCurrentScale / kGridTransitionVolts) * grid_logistic;
            j[0][1] = 0.0;
            j[1][0] = dip_de1 * de1_dvgk;                                           // dIp/dVgk
            j[1][1] = dip_de1 * de1_dvpk;                                           // dIp/dVpk
        }
```

`current()` and `jacobian()` become wrappers that call this into local scratch and copy out the half
they were asked for; keep their Doxygen unchanged.

- [ ] **Step 6: `PentodeEL34::current_and_jacobian`,** the same treatment. Shared between the two
halves: the `kScreenFloorVolts` clamp and its `screen_follows_input` flag, `u`, the
`softplus_and_logistic` pair, `E1`, `knee`, `z`, the grid-law `softplus_and_logistic` pair,
`E1^(kx-1)` and `z^(kx-1)`. `Ip` is `2 · e1_power · e1 / kKg1 · knee`, `Ig2` is
`z_power · z / kKg2`, and `j[2][2]` reuses the same `e1_power * e1` product instead of calling `pow`
a third time. Every line keeps the comment naming the plan symbol it transcribes.

- [ ] **Step 7: `sd build && sd test -s unit`** — every existing model case and the three new ones
green. The D6b golden vectors must pass **unchanged**: if one moves, the joint form is not the same
physics and the fault is in this task, not in the tolerance.

- [ ] **Step 8: Commit**

```bash
git add include/SushiDSP/circuits/models tests/functional/unit/test_silicon_diode.cpp tests/functional/unit/test_triode_12ax7.cpp tests/functional/unit/test_pentode_el34.cpp
git commit -m "D6c: joint current_and_jacobian on every device model, sharing the transcendentals the DK solver was paying for twice"
```

---

### Task 2: `DenseLu<2>` and the solver's last-evaluation contract

**Files:**
- Modify: `include/SushiDSP/math/dk/DenseLu.hpp`
- Modify: `include/SushiDSP/math/dk/DampedNewtonSolver.hpp` (documentation only)
- Modify: `tests/functional/unit/test_damped_newton_solver.cpp`

**Interfaces:** unchanged. `DenseLu<2>` is an explicit specialisation with the same four member
functions as the primary template — `factorise(matrix)`, `factorise(matrix, order)`,
`solve(rhs, x)`, `solve(rhs, x, order)` — so nothing that uses `DenseLu` can tell the difference
except by timing it. `DampedNewtonSolver` gains no member; it gains a documented guarantee.

- [ ] **Step 1: Write the failing tests** in `tests/functional/unit/test_damped_newton_solver.cpp`.

```cpp
TEST(Unit_Dsp, DenseLuTwoByTwoSolvesTheSameSystemsAsTheGeneralTemplate)
{
    // [[4, 3], [6, 3]] x = [10, 12] has the exact answer x = [1, 2]:
    //   4*1 + 3*2 = 10,  6*1 + 3*2 = 12.   det = 4*3 - 3*6 = -6.
    std::array<std::array<double, 2>, 2> matrix{ { { 4.0, 3.0 }, { 6.0, 3.0 } } };
    SushiDSP::dk::DenseLu<2> lu;
    ASSERT_TRUE(lu.factorise(matrix));
    std::array<double, 2> solution{};
    lu.solve({ 10.0, 12.0 }, solution);
    EXPECT_NEAR(solution[0], 1.0, 1e-14);
    EXPECT_NEAR(solution[1], 2.0, 1e-14);

    // The pivoting case the general template is tested on: [[0, 1], [1, 0]] x = [3, 5] -> [5, 3].
    std::array<std::array<double, 2>, 2> pivoting{ { { 0.0, 1.0 }, { 1.0, 0.0 } } };
    ASSERT_TRUE(lu.factorise(pivoting));
    lu.solve({ 3.0, 5.0 }, solution);
    EXPECT_NEAR(solution[0], 5.0, 1e-14);
    EXPECT_NEAR(solution[1], 3.0, 1e-14);

    // Singular, and the order-aware overload restricted to the leading 1x1 block.
    std::array<std::array<double, 2>, 2> singular{ { { 1.0, 2.0 }, { 2.0, 4.0 } } };
    EXPECT_FALSE(lu.factorise(singular));
    std::array<std::array<double, 2>, 2> leading{ { { 5.0, 99.0 }, { 99.0, 99.0 } } };
    ASSERT_TRUE(lu.factorise(leading, 1));
    lu.solve({ 10.0, 0.0 }, solution, 1);
    EXPECT_NEAR(solution[0], 2.0, 1e-14);
}

namespace
{
    /// @brief Records the point of the last residual_and_jacobian() call, to pin the contract
    /// DkCircuit::process_sample relies on: on success the solver's last model evaluation was at
    /// the iterate it returns, so the caller may reuse whatever that evaluation cached.
    struct RecordingSystem
    {
        mutable std::array<double, 1> last_evaluated{ 0.0 };

        void residual_and_jacobian(const std::array<double, 1>& v, std::array<double, 1>& r,
                                   std::array<std::array<double, 1>, 1>& jacobian) const
        {
            last_evaluated = v;
            r[0] = std::exp(v[0]) - std::exp(0.25);
            jacobian[0][0] = std::exp(v[0]);
        }
    };
}

TEST(Unit_Dsp, DampedNewtonLeavesItsLastEvaluationAtTheIterateItReturns)
{
    SushiDSP::dk::DampedNewtonSolver<1> solver;
    for (const double start : { 0.0, 5.0, -5.0, 12.0, 0.2499999 })
    {
        RecordingSystem system;
        std::array<double, 1> v{ start };
        ASSERT_TRUE(solver.solve(system, v)) << start;
        EXPECT_DOUBLE_EQ(system.last_evaluated[0], v[0]) << start;
        EXPECT_NEAR(v[0], 0.25, 1e-9) << start;
    }
}
```

- [ ] **Step 2: `sd build && sd test -s unit` — expected: `DenseLuTwoByTwo…` fails to compile or
fails, `DampedNewtonLeavesItsLastEvaluation…` PASSES.** The contract test passing before any change
is the point: it pins behaviour the solver already has, so Task 3 may rely on it. If it *fails*,
stop and report — Ruling 6 is then wrong and Task 3's Step 3 must not be written.

- [ ] **Step 3: Write the `DenseLu<2>` specialisation,** mirroring `DenseLu<1>`'s shape in the same
file. Store the four factored entries and the pivot flag; `factorise` picks the larger of
`|matrix[0][0]|` and `|matrix[1][0]|` as the pivot, rejects a pivot below `1e-300`, computes the
multiplier and the single Schur entry, and rejects that too when it underflows; `solve` is the
two-step substitution written out. The `order`-taking overloads forward to the 1×1 path when
`order == 1` and to the 2×2 path otherwise, matching what the primary template does. Doxygen says
why the specialisation exists: `N = 2` is the shape of every triode block and of the D6c clip
stage's diode pair, so it is the hot Jacobian size in the tree.

- [ ] **Step 4: Document the contract in `DampedNewtonSolver::solve`'s Doxygen,** as a new
paragraph, since a caller is about to depend on it:

> On success the last call this function made to `System::residual_and_jacobian` was made at the
> iterate it leaves in `v`: the tolerance return happens with the residual belonging to the current
> `v`, and every step return sets `v` to the trial point the line search last evaluated. A system
> that caches per-evaluation quantities may therefore reuse them after a `true` return. **On
> failure it may not** — `fail()` restores the lowest-residual iterate seen, which is generally not
> the last one evaluated. `test_damped_newton_solver.cpp` pins this.

- [ ] **Step 5: `sd build && sd test -s unit` green.**

- [ ] **Step 6: `sd test -t release -s benchmark`, and record the `DkCircuit 2-nonlin 7-state @8x`
row.** `DenseLu<2>` is the only change reaching it. Report the before and after ns/sample even if
the difference is inside the run-to-run spread; a lever that turns out to be worth nothing is worth
knowing about.

- [ ] **Step 7: Commit**

```bash
git add include/SushiDSP/math/dk/DenseLu.hpp include/SushiDSP/math/dk/DampedNewtonSolver.hpp tests/functional/unit/test_damped_newton_solver.cpp
git commit -m "D6c: DenseLu<2> closed form for the hot Jacobian size, and the solver's last-evaluation contract written down and pinned (<a> -> <b> ns/sample)"
```

---

### Task 3: `DkCircuit` consumes the joint form and stops re-evaluating after a converged solve

**Files:**
- Modify: `include/SushiDSP/math/dk/DkCircuit.hpp`
- Modify: `tests/circuits/JcmV1aNetlist.hpp`
- Modify: `tests/functional/unit/test_dk_circuit.cpp`

**Depends on:** Tasks 1 and 2.

**Interface change (breaking, and the only one in this plan):** a `NonlinearSystem` now exposes one
method instead of two.

```cpp
void currents_and_jacobian(const std::array<double, N>& v, std::array<double, N>& i,
                           std::array<std::array<double, N>, N>& j) const;
```

Every consumer in the tree is a test-side or module-side glue struct and they are all listed in this
task's Files plus Task 5's.

- [ ] **Step 1: Update the class documentation** of `DkCircuit` where it describes the
`NonlinearSystem` contract: one method, both outputs, the Jacobian buffer still zeroed before every
call so a glue struct for independent devices leaves the off-diagonal blocks alone.

- [ ] **Step 2: Rewrite the `NewtonAdapter`** inside `process_sample` to make one call, and to write
the currents into the member the caller reads rather than into a separate scratch:

```cpp
                struct NewtonAdapter
                {
                    NonlinearSystem&                             system;
                    const std::array<double, N>&                 open_circuit;
                    const std::array<std::array<double, N>, N>&  f_matrix;
                    std::array<double, N>&                       branch_current;
                    std::array<std::array<double, N>, N>&        model_jacobian;

                    void residual_and_jacobian(const std::array<double, N>&          v,
                                               std::array<double, N>&                residual,
                                               std::array<std::array<double, N>, N>& jacobian)
                    {
                        for (std::size_t row = 0; row < N; ++row)
                        {
                            for (std::size_t column = 0; column < N; ++column)
                            {
                                model_jacobian[row][column] = 0.0;
                            }
                        }
                        system.currents_and_jacobian(v, branch_current, model_jacobian);

                        // r(v) = v - p - F f(v)  (D6b plan, Newton on the nonlinear port).
                        for (std::size_t row = 0; row < N; ++row)
                        {
                            double accumulator = v[row] - open_circuit[row];
                            for (std::size_t column = 0; column < N; ++column)
                            {
                                accumulator -= f_matrix[row][column] * branch_current[column];
                            }
                            residual[row] = accumulator;
                        }

                        // J(v) = Identity - F Jf(v).
                        for (std::size_t row = 0; row < N; ++row)
                        {
                            for (std::size_t column = 0; column < N; ++column)
                            {
                                double accumulator = row == column ? 1.0 : 0.0;
                                for (std::size_t inner = 0; inner < N; ++inner)
                                {
                                    accumulator -= f_matrix[row][inner] * model_jacobian[inner][column];
                                }
                                jacobian[row][column] = accumulator;
                            }
                        }
                    }
                };
```

- [ ] **Step 3: Drop the unconditional post-solve evaluation.** Replace

```cpp
                if (!solver_.solve(adapter, nonlinear_voltage_))
                {
                    ++non_converged_samples_;
                }
                nonlinear.currents(nonlinear_voltage_, nonlinear_current_);
```

with

```cpp
                NewtonAdapter adapter{ nonlinear, open_circuit_voltage_, matrix_f_,
                                       nonlinear_current_, model_jacobian_ };
                if (!solver_.solve(adapter, nonlinear_voltage_))
                {
                    // A failed solve restores the best iterate seen, which is generally not the
                    // point the models were last evaluated at, so nonlinear_current_ has to be
                    // brought back into step with nonlinear_voltage_. On a converged solve the
                    // solver's own contract says they already are (DampedNewtonSolver::solve).
                    ++non_converged_samples_;
                    adapter.residual_and_jacobian(nonlinear_voltage_, residual_scratch_,
                                                  jacobian_scratch_);
                }
```

Add the two scratch members (`std::array<double, N> residual_scratch_{};` and
`std::array<std::array<double, N>, N> jacobian_scratch_{};`) beside the existing ones and delete
`model_current_`, which nothing writes any more. Note in a comment that the fallback re-evaluation
runs on under 0.01 % of samples by the phase's own convergence gate, so its cost is not on the
critical path.

- [ ] **Step 4: Update `tests/circuits/JcmV1aNetlist.hpp`'s `TriodeStagePort`** to the one-method
form, forwarding to `Triode12AX7::current_and_jacobian`. Its Doxygen keeps saying which branch is
which; only the method count changes.

```cpp
    struct TriodeStagePort
    {
        SushiDSP::Triode12AX7 triode{};

        /// @brief Evaluates both branch currents and the 2x2 Jacobian at one port voltage.
        /// @param v Port voltages: v[0] = Vgk, v[1] = Vpk.
        /// @param i Receives the branch currents in amperes.
        /// @param j Receives the 2x2 Jacobian in siemens.
        void currents_and_jacobian(const std::array<double, 2>& v, std::array<double, 2>& i,
                                   std::array<std::array<double, 2>, 2>& j) const
        {
            triode.current_and_jacobian(v, i, j);
        }
    };
```

- [ ] **Step 5: Update every glue struct in `tests/functional/unit/test_dk_circuit.cpp`** —
`NoNonlinearity`, the diode-pair glue, the deliberately-infinite glue in the NaN case — to the
one-method form. Nothing else in the file changes, and every expectation in it must still hold
**exactly**: this task is meant to be pure cost.

- [ ] **Step 6: `sd build && sd test -s unit`.** Expected: everything green, with the D6b
`DkCircuit`, `jcm_v1a` bias, convergence and SPICE-null numbers **unchanged** — the `jcm_v1a`
transient nulls are −97.8 / −94.6 / −88.7 dB and must stay there. A moved null means the joint form
is not the same physics.

- [ ] **Step 7: `sd test -t release -s benchmark`. This is the gate.** Record
`DkCircuit 2-nonlin 7-state @8x` and `DkCircuit pot re-derivation`. The plan's estimate is that the
first drops from 3762.5 ns/sample toward roughly 2000 ns/sample (0.963 → ≈ 0.52 ms/block). Report
the measured figure whatever it is; it is the multiplier the whole D6c cost estimate rests on, and
Task 5's gate reads it. If the improvement is under 20 %, stop and report before Task 4: the cost
model in this plan is then wrong and the oversampling decision has to be retaken with the real
number.

- [ ] **Step 8: Commit**

```bash
git add include/SushiDSP/math/dk/DkCircuit.hpp tests/circuits/JcmV1aNetlist.hpp tests/functional/unit/test_dk_circuit.cpp
git commit -m "D6c: DkCircuit takes one joint model call per residual and stops re-evaluating a converged solve (<a> -> <b> ns/sample on the 2-nonlin 7-state case)"
```

---

### Task 4: The ideal op-amp as a nullor element

**Files:**
- Modify: `include/SushiDSP/math/dk/DkNetlist.hpp`
- Modify: `include/SushiDSP/math/dk/DkCircuit.hpp`
- Modify: `tests/functional/unit/test_dk_op_amp.cpp` (replace stub)

**Depends on:** Task 3 (same file, not the same concern).

**Interfaces (consumed by Task 5 and by every op-amp circuit after it):**

```cpp
namespace SushiDSP::dk
{
    inline constexpr std::size_t kMaximumNullors = 4;

    /// @brief One nullor: an ideal op-amp's input constraint and its output current.
    struct NullorEntry
    {
        Branch nullator;   ///< The sensed pair; V(positive) - V(negative) is forced to zero.
        Branch norator;    ///< The driven pair; an unknown current flows positive to negative.
    };

    // On DkNetlist:
    std::size_t nullor(std::size_t sense_positive, std::size_t sense_negative,
                       std::size_t output_positive, std::size_t output_negative);
}
```

- [ ] **Step 1: Write the failing tests** in `tests/functional/unit/test_dk_op_amp.cpp`. Every
expected value below is a closed form, and every one was checked against this exact stamp while
writing the plan.

```cpp
/* LICENSE HEADER — copy from PortLayout.hpp, filename line changed */

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI.
#define _USE_MATH_DEFINES
#include <array>
#include <cmath>
#include <cstddef>

#include <gtest/gtest.h>

#include "SushiDSP/math/dk/DkCircuit.hpp"
#include "SushiDSP/math/dk/DkNetlist.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    /// @brief The empty nonlinear port every purely linear DK block is stepped with.
    struct NoNonlinearity
    {
        void currents_and_jacobian(const std::array<double, 0>&, std::array<double, 0>&,
                                   std::array<std::array<double, 0>, 0>&) const {}
    };
}

TEST(Unit_Dsp, DkCircuitIdealOpAmpInvertingAmplifierHasGainMinusRfOverRin)
{
    // Node 1 = source, 2 = the summing junction, 3 = the output. The nullator senses ground
    // against node 2, so V(2) = 0; the norator drives node 3. KCL at node 2 is then
    // -V(1)/Rin - V(3)/Rf = 0, so V(3) = -(Rf/Rin) V(1) = -10 V(1) at every frequency.
    SushiDSP::dk::DkNetlist<0, 0, 1, 1> netlist;
    netlist.node_count = 3;
    netlist.voltage_source(1, 0);
    netlist.resistor(1, 2, 10.0e3);
    netlist.resistor(2, 3, 100.0e3);
    netlist.nullor(0, 2, 3, 0);
    netlist.output(3, 0);

    SushiDSP::dk::DkCircuit<0, 0, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, 48000.0));

    NoNonlinearity nonlinear;
    std::array<double, 1> outputs{};
    for (const double input : { -3.0, -0.25, 0.0, 0.25, 3.0 })
    {
        circuit.process_sample(nonlinear, { input }, outputs);
        EXPECT_NEAR(outputs[0], -10.0 * input, 1e-12) << input;
    }
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
}

TEST(Unit_Dsp, DkCircuitIdealOpAmpNonInvertingAmplifierHasGainOnePlusRfOverRg)
{
    // The Tube Screamer clip stage with its capacitors removed and the drive pot at zero:
    // 1 + 51000/4700 = 11.851063829787234 exactly.
    SushiDSP::dk::DkNetlist<0, 0, 1, 1> netlist;
    netlist.node_count = 3;
    netlist.voltage_source(1, 0);
    netlist.resistor(3, 2, 51.0e3);
    netlist.resistor(2, 0, 4700.0);
    netlist.nullor(1, 2, 3, 0);
    netlist.output(3, 0);

    SushiDSP::dk::DkCircuit<0, 0, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, 48000.0));

    NoNonlinearity nonlinear;
    std::array<double, 1> outputs{};
    circuit.process_sample(nonlinear, { 1.0 }, outputs);
    EXPECT_NEAR(outputs[0], 11.851063829787234, 1e-11);
}

TEST(Unit_Dsp, DkCircuitIdealOpAmpVoltageFollowerPassesItsInputThrough)
{
    // The degenerate case: the sensed node and the driven node are the same one.
    SushiDSP::dk::DkNetlist<0, 0, 1, 1> netlist;
    netlist.node_count = 2;
    netlist.voltage_source(1, 0);
    netlist.resistor(2, 0, 10.0e3);
    netlist.nullor(1, 2, 2, 0);
    netlist.output(2, 0);

    SushiDSP::dk::DkCircuit<0, 0, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, 48000.0));

    NoNonlinearity nonlinear;
    std::array<double, 1> outputs{};
    circuit.process_sample(nonlinear, { 0.7 }, outputs);
    EXPECT_NEAR(outputs[0], 0.7, 1e-12);
}

TEST(Unit_Dsp, DkCircuitIdealOpAmpLowPassMatchesTheBilinearTransformExactly)
{
    // Inverting stage with Rf in parallel with Cf: H(s) = -(Rf/Rin) / (1 + s Rf Cf).
    // Trapezoidal DK must reproduce its bilinear transform exactly. With Rin = 10 kOhm,
    // Rf = 100 kOhm, Cf = 10 nF, fs = 48000: a = 2 fs Rf Cf = 2*48000*1e-3 = 96, so
    //   H(z) = -(Rf/Rin) (1 + z^-1) / ((1 + a) + (1 - a) z^-1)
    // with pole (a - 1)/(a + 1) = 95/97 = 0.979381443298969 and DC gain -10 exactly.
    // The analogue corner is 1/(2 pi Rf Cf) = 159.154943 Hz.
    constexpr double kInputOhms = 10.0e3;
    constexpr double kFeedbackOhms = 100.0e3;
    constexpr double kFeedbackFarads = 10.0e-9;
    constexpr double kSampleRate = 48000.0;

    SushiDSP::dk::DkNetlist<1, 0, 1, 1> netlist;
    netlist.node_count = 3;
    netlist.voltage_source(1, 0);
    netlist.resistor(1, 2, kInputOhms);
    netlist.resistor(2, 3, kFeedbackOhms);
    netlist.capacitor(2, 3, kFeedbackFarads);
    netlist.nullor(0, 2, 3, 0);
    netlist.output(3, 0);

    SushiDSP::dk::DkCircuit<1, 0, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, kSampleRate));

    const double alpha = 2.0 * kSampleRate * kFeedbackOhms * kFeedbackFarads;
    const double gain = kFeedbackOhms / kInputOhms;
    double reference_input = 0.0;
    double reference_output = 0.0;

    NoNonlinearity nonlinear;
    for (int sample = 0; sample < 4096; ++sample)
    {
        const double input = std::sin(2.0 * M_PI * 137.0 * sample / kSampleRate)
                             + 0.3 * std::sin(2.0 * M_PI * 6000.0 * sample / kSampleRate);
        std::array<double, 1> outputs{};
        circuit.process_sample(nonlinear, { input }, outputs);
        reference_output = (-gain * (input + reference_input) - (1.0 - alpha) * reference_output)
                           / (1.0 + alpha);
        reference_input = input;
        ASSERT_NEAR(outputs[0], reference_output, 1e-11) << sample;
    }
}

TEST(Unit_Dsp, DkCircuitRejectsAnOpAmpWithNoFeedbackPath)
{
    // An op-amp whose output does not reach its own input leaves the augmented system rank
    // deficient. prepare() must say so rather than derive nonsense.
    SushiDSP::dk::DkNetlist<0, 0, 1, 1> netlist;
    netlist.node_count = 3;
    netlist.voltage_source(1, 0);
    netlist.resistor(1, 2, 10.0e3);
    netlist.resistor(3, 0, 10.0e3);
    netlist.nullor(0, 2, 3, 0);
    netlist.output(3, 0);

    SushiDSP::dk::DkCircuit<0, 0, 1, 1> circuit;
    EXPECT_FALSE(circuit.prepare(netlist, 48000.0));
}
```

- [ ] **Step 2: `sd build` — expected: FAIL to compile (`nullor` does not exist).**

- [ ] **Step 3: Add the element to `DkNetlist.hpp`.** `kMaximumNullors = 4` beside the other
capacity constants; the `NullorEntry` struct after `PotentiometerEntry`; the storage
(`std::array<NullorEntry, kMaximumNullors> nullors_{}; std::size_t nullor_count_ = 0;`) beside the
potentiometer storage; and:

```cpp
        /// @brief Adds an ideal op-amp, stamped as a nullor.
        ///
        /// The sensed pair is a nullator — the derivation forces
        /// V(sense_positive) - V(sense_negative) to zero and draws no current through it — and the
        /// driven pair is a norator, carrying whatever current that constraint demands. Together
        /// they are the infinite-gain limit of a voltage-controlled voltage source, which is what
        /// the LTspice reference blocks model with an E source of gain 1e6.
        ///
        /// A nullor is only well posed inside a negative-feedback loop: if the driven pair cannot
        /// reach the sensed pair through the rest of the circuit, the augmented nodal system is
        /// singular and DkCircuit::prepare returns false.
        /// @param sense_positive The non-inverting input node; 0 is ground.
        /// @param sense_negative The inverting input node; 0 is ground.
        /// @param output_positive The node the output current flows out of; 0 is ground.
        /// @param output_negative The node it returns to; 0 is ground.
        /// @return The nullor's index, in the order they were added.
        std::size_t nullor(std::size_t sense_positive, std::size_t sense_negative,
                           std::size_t output_positive, std::size_t output_negative)
        {
            assert(nullor_count_ < kMaximumNullors);
            assert(sense_positive <= kMaximumNodes && sense_negative <= kMaximumNodes);
            assert(output_positive <= kMaximumNodes && output_negative <= kMaximumNodes);
            nullors_[nullor_count_]
                = NullorEntry{ Branch{ sense_positive, sense_negative, 0.0 },
                               Branch{ output_positive, output_negative, 0.0 } };
            const std::size_t index = nullor_count_;
            ++nullor_count_;
            return index;
        }
```

- [ ] **Step 4: Widen the augmented system in `DkCircuit.hpp`.**

  1. `static constexpr std::size_t kMaximumSystemOrder = kMaximumNodes + I + kMaximumNullors;`
     (was `kMaximumNodes + I`). At the worst shape this is 16 + 2 + 4 = 22, so `system_` and
     `inverse_` grow to 22 × 22 doubles each — under 8 KB per instance, still nothing beside a
     partitioned convolution.
  2. In `prepare()`, beside the other count checks, reject `netlist.nullor_count_ >
     kMaximumNullors`, and validate both branches of every nullor with the existing `in_range`
     helper.
  3. In `derive()`, the order becomes `nodes + I + netlist_.nullor_count_`, and after the source
     stamping loop:

```cpp
            // A nullor's row is its nullator (V(p) - V(q) = 0, right-hand side zero) and its
            // column is its norator (an unknown current into the driven pair). Unlike a source,
            // the two are different node pairs, which is exactly why an op-amp cannot be spelled
            // with voltage_source() (D6c plan, ruling 1).
            for (std::size_t index = 0; index < netlist_.nullor_count_; ++index)
            {
                const std::size_t row = nodes + I + index;
                const Branch&     sense = netlist_.nullors_[index].nullator;
                const Branch&     drive = netlist_.nullors_[index].norator;
                if (sense.positive_node != 0)
                {
                    system_[row][sense.positive_node - 1] += 1.0;
                }
                if (sense.negative_node != 0)
                {
                    system_[row][sense.negative_node - 1] -= 1.0;
                }
                if (drive.positive_node != 0)
                {
                    system_[drive.positive_node - 1][row] += 1.0;
                }
                if (drive.negative_node != 0)
                {
                    system_[drive.negative_node - 1][row] -= 1.0;
                }
            }
```

  4. Nothing else changes. `Pleft` is still `inverse_[0..nodes)[0..nodes)` and `Pright` still
     `inverse_[0..nodes)[nodes..nodes+I)`: the nullor rows and columns sit *after* the sources, and
     their right-hand side is zero, so they contribute to the inverse without ever being read out
     of it. Put that sentence in a comment above `build_left_and_right`'s call site — it is the one
     non-obvious thing about the change.

- [ ] **Step 5: `sd build && sd test -s unit`** — the five new cases pass and every D6b case is
untouched (no existing netlist has a nullor, so `nullor_count_` is zero and `order` is what it was).

- [ ] **Step 6: Commit**

```bash
git add include/SushiDSP/math/dk/DkNetlist.hpp include/SushiDSP/math/dk/DkCircuit.hpp tests/functional/unit/test_dk_op_amp.cpp
git commit -m "D6c: ideal op-amps in DkNetlist as nullors, validated against inverting, non-inverting, follower and one-pole closed forms"
```

---

### Task 5: The two circuits, the clip block's test surface, and the oversampling gate

**Files:**
- Create: `modules/tubescreamer/include/SushiDSP/tubescreamer/TubeScreamerCircuits.hpp`
- Create: `tests/circuits/DkBlockNode.hpp`
- Create: `tests/circuits/SpiceBlockCompare.hpp`
- Modify: `tests/functional/unit/test_dk_triode_stage.cpp` (use the extracted helpers)
- Modify: `tests/functional/unit/test_ts_clip_stage.cpp` (replace stub)
- Modify: `tests/benchmark/main.cpp`

**Depends on:** Task 4. Header-only, so `modules/tubescreamer/CMakeLists.txt` does not change.

**Why the netlists live in the module, not in `tests/circuits/`.** `tests/circuits/JcmV1aNetlist.hpp`
exists because D6b had no product to put the JCM800's V1a stage in. D6c does: the Tube Screamer's
circuits *are* the Tube Screamer, the module already exposes public headers, and the tests already
link `SushiDSP::tubescreamer`. One description, owned by the thing it describes.

- [ ] **Step 1: Write `TubeScreamerCircuits.hpp`.** Every component value named once, each commented
with the reference file it comes from. Transcribe the two tables in "The two circuits being
implemented" line for line.

```cpp
/* LICENSE HEADER — copy from PortLayout.hpp, filename line changed */

#pragma once

#include <array>
#include <cstddef>

#include "SushiDSP/circuits/models/SiliconDiode.hpp"
#include "SushiDSP/math/dk/DkCircuit.hpp"
#include "SushiDSP/math/dk/DkNetlist.hpp"

/// @file
/// @brief The TS808's two DK blocks, described once: component values, node numbering, the
/// nonlinear port glue and the knob-to-wiper mappings.
///
/// Both netlists follow docs/reference/spice/ts_clip/README.md and
/// docs/reference/spice/ts_tone/README.md node for node, because the committed CSVs from those
/// schematics are what tests/functional/unit/test_ts_clip_stage.cpp and test_ts_tone_stage.cpp
/// null against. A value that drifts from its README makes those nulls meaningless.
namespace SushiDSP::tube_screamer_circuits
{
    /// @brief Node numbering of docs/reference/spice/ts_clip, node for node; 0 is ground.
    ///
    /// kClipNodeFeedbackMiddle is the one node the reference does not have: it splits Rf into its
    /// fixed 51 kOhm part and its 500 kOhm rheostat part, which is how a three-terminal
    /// DkNetlist::potentiometer expresses a two-terminal variable resistor (D6c plan, ruling 2).
    enum ClipNodeIndex : std::size_t
    {
        kClipNodeSource = 1,
        kClipNodeInput = 2,
        kClipNodeOutput = 3,
        kClipNodeInverting = 4,
        kClipNodeGroundLeg = 5,
        kClipNodeFeedbackMiddle = 6
    };

    inline constexpr double kInputFarads = 1.0e-6;            ///< Cin (ts_clip README).
    inline constexpr double kInputLeakOhms = 510.0e3;         ///< Rin (ts_clip README).
    inline constexpr double kFeedbackFixedOhms = 51.0e3;      ///< Rf's fixed part (ts_clip README).
    inline constexpr double kDrivePotOhms = 500.0e3;          ///< Rf's Drive part (ts_clip README).
    inline constexpr double kFeedbackFarads = 51.0e-12;       ///< Cf (ts_clip README).
    inline constexpr double kGroundLegOhms = 4700.0;          ///< Rg (ts_clip README).
    inline constexpr double kGroundLegFarads = 47.0e-9;       ///< Cg (ts_clip README).

    /// @brief One diode's zero-bias junction capacitance, from d1n914.sub's Cjo.
    ///
    /// The SPICE model carries it, so every LTspice run of ts_clip has 4 pF across each diode and
    /// 8 pF in parallel with Cf - a 16 % increase the AC sweep sees plainly: with Cf alone the DK
    /// block is 1.15 dB off the reference at 20 kHz, and with the two junctions folded in it is
    /// 0.004 dB off across the whole sweep. SPICE's junction capacitance varies with the diode
    /// voltage and this one does not; at the clipping excursion the pair is about 8.9 pF rather
    /// than 8.0, which moves the feedback pole 1.6 % (D6c plan, ruling 3).
    inline constexpr double kDiodeJunctionFarads = 4.0e-12;

    /// @brief Node numbering of docs/reference/spice/ts_tone, node for node; 0 is ground.
    enum ToneNodeIndex : std::size_t
    {
        kToneNodeSource = 1,
        kToneNodeResonant = 2,
        kToneNodeWiper = 3,
        kToneNodeInverting = 4,
        kToneNodeStageOutput = 5,
        kToneNodeLevelWiper = 6,
        kToneNodeOutput = 7
    };

    inline constexpr double kResonantFarads = 220.0e-9;       ///< C5 (ts_tone README).
    inline constexpr double kResonantOhms = 1.0e3;            ///< R6 (ts_tone README).
    inline constexpr double kTonePotOhms = 20.0e3;            ///< Tone pot (ts_tone README).
    inline constexpr double kLowPassLegOhms = 10.0e3;         ///< R7 (ts_tone README).
    inline constexpr double kLowPassLegFarads = 220.0e-9;     ///< C6 (ts_tone README).
    inline constexpr double kLevelPotOhms = 100.0e3;          ///< Level pot (ts_tone README).
    inline constexpr double kOutputFarads = 1.0e-6;           ///< Cout (ts_tone README).
    inline constexpr double kLoadOhms = 100.0e3;              ///< Rload (ts_tone README).

    /// @brief Wiper position of the level pot, held where the reference holds it.
    ///
    /// ts_tone.asc writes the upper section as 100k*(1 - LEVEL) while DkNetlist::potentiometer
    /// makes it ohms * position, so position is 1 - LEVEL and the reference's LEVEL = 1 is
    /// position 0. The shipping Level control stays the calibrated dB output gain D5b defined and
    /// does not drive this pot; the pot is in the netlist so the block is the reference node for
    /// node, and so a later phase can expose it without re-deriving the circuit.
    inline constexpr double kLevelPotPosition = 0.0;

    using ClipNetlist = dk::DkNetlist<3, 2, 1, 1>;
    using ClipCircuit = dk::DkCircuit<3, 2, 1, 1>;
    using ToneNetlist = dk::DkNetlist<3, 0, 1, 1>;
    using ToneCircuit = dk::DkCircuit<3, 0, 1, 1>;

    /// @brief The clip stage's nonlinear port: an antiparallel 1N914 pair across the feedback.
    ///
    /// Branch 0 is D1 (anode at the op-amp output, cathode at the summing junction) and branch 1
    /// is D2, wired the other way, so v[1] is always -v[0] and the two branches are independent
    /// one-ports. The Jacobian is therefore diagonal, and DkCircuit zeroes the buffer before every
    /// call, so the off-diagonal entries are simply left alone. The pair is two branches rather
    /// than one antiparallel branch so an asymmetric pair stays a value change (spec D6c).
    struct DiodePairPort
    {
        SiliconDiode diode{};

        /// @brief Evaluates both diode currents and the diagonal Jacobian at one port voltage.
        /// @param v Branch voltages in volts: v[0] across D1, v[1] across D2.
        /// @param i Receives the branch currents in amperes.
        /// @param j Receives the 2x2 Jacobian in siemens; only the diagonal is written.
        void currents_and_jacobian(const std::array<double, 2>& v, std::array<double, 2>& i,
                                   std::array<std::array<double, 2>, 2>& j) const
        {
            diode.current_and_jacobian(v[0], i[0], j[0][0]);
            diode.current_and_jacobian(v[1], i[1], j[1][1]);
        }
    };

    /// @brief The empty nonlinear port a purely linear DK block is stepped with.
    struct LinearPort
    {
        /// @brief Does nothing; a block with no nonlinear branches has no currents to evaluate.
        void currents_and_jacobian(const std::array<double, 0>&, std::array<double, 0>&,
                                   std::array<std::array<double, 0>, 0>&) const {}
    };

    /// @brief Builds the ts_clip netlist, node for node against the block's component table.
    /// @param netlist Receives the description; any previous contents are overwritten.
    /// @return The drive rheostat's index, for DkCircuit::set_potentiometer.
    inline std::size_t build_clip_block(ClipNetlist& netlist)
    {
        netlist = ClipNetlist{};
        netlist.node_count = kClipNodeFeedbackMiddle;

        netlist.voltage_source(kClipNodeSource, 0);
        netlist.capacitor(kClipNodeSource, kClipNodeInput, kInputFarads);
        netlist.resistor(kClipNodeInput, 0, kInputLeakOhms);

        netlist.resistor(kClipNodeOutput, kClipNodeFeedbackMiddle, kFeedbackFixedOhms);
        netlist.capacitor(kClipNodeOutput, kClipNodeInverting,
                          kFeedbackFarads + 2.0 * kDiodeJunctionFarads);
        netlist.nonlinear(kClipNodeOutput, kClipNodeInverting);
        netlist.nonlinear(kClipNodeInverting, kClipNodeOutput);

        netlist.resistor(kClipNodeInverting, kClipNodeGroundLeg, kGroundLegOhms);
        netlist.capacitor(kClipNodeGroundLeg, 0, kGroundLegFarads);

        netlist.nullor(kClipNodeInput, kClipNodeInverting, kClipNodeOutput, 0);
        netlist.output(kClipNodeOutput, 0);

        // Wiper tied to the summing junction: the pot is wired as the rheostat the pedal wires it
        // as, and the dead wiper-to-b section stamps nothing because both its terminals are the
        // same node (D6c plan, ruling 2).
        return netlist.potentiometer(kClipNodeFeedbackMiddle, kClipNodeInverting,
                                     kClipNodeInverting, kDrivePotOhms);
    }

    /// @brief The two wiper indices build_tone_block() hands back.
    struct TonePotentiometers
    {
        std::size_t tone = 0;   ///< The 20 kOhm tone pot.
        std::size_t level = 0;  ///< The 100 kOhm level pot, held at kLevelPotPosition.
    };

    /// @brief Builds the ts_tone netlist, node for node against the block's component table.
    /// @param netlist Receives the description; any previous contents are overwritten.
    /// @return Both wiper indices, for DkCircuit::set_potentiometer.
    inline TonePotentiometers build_tone_block(ToneNetlist& netlist)
    {
        netlist = ToneNetlist{};
        netlist.node_count = kToneNodeOutput;

        netlist.voltage_source(kToneNodeSource, 0);
        netlist.capacitor(kToneNodeSource, kToneNodeResonant, kResonantFarads);
        netlist.resistor(kToneNodeResonant, 0, kResonantOhms);

        TonePotentiometers pots;
        pots.tone = netlist.potentiometer(kToneNodeResonant, kToneNodeWiper,
                                          kToneNodeStageOutput, kTonePotOhms);

        netlist.resistor(kToneNodeWiper, kToneNodeInverting, kLowPassLegOhms);
        netlist.capacitor(kToneNodeInverting, kToneNodeStageOutput, kLowPassLegFarads);
        netlist.nullor(0, kToneNodeInverting, kToneNodeStageOutput, 0);

        pots.level = netlist.potentiometer(kToneNodeStageOutput, kToneNodeLevelWiper, 0,
                                           kLevelPotOhms);
        netlist.capacitor(kToneNodeLevelWiper, kToneNodeOutput, kOutputFarads);
        netlist.resistor(kToneNodeOutput, 0, kLoadOhms);
        netlist.output(kToneNodeOutput, 0);

        return pots;
    }

    /// @brief Converts the Drive knob to the rheostat's wiper position.
    ///
    /// ts_clip.asc writes Rf as 51k + DRIVE*500k and the rheostat's live section is
    /// ohms * position, so the mapping is the identity and the reference's DRIVE steps compare
    /// directly against knob values. No audio taper this phase (D6c plan, ruling 9).
    /// @param drive Normalised Drive knob, 0 to 1.
    /// @return The wiper position.
    inline double drive_to_position(double drive) { return drive; }

    /// @brief Lowest wiper position the Tone knob reaches; the reference's brightest step.
    inline constexpr double kToneMinimumPosition = 0.1;

    /// @brief Highest wiper position the Tone knob reaches; the reference's darkest step.
    inline constexpr double kToneMaximumPosition = 0.9;

    /// @brief Converts the Tone knob to the tone pot's wiper position.
    ///
    /// Two things happen here. The sense is inverted: ts_tone.asc puts the wiper at the resonant
    /// node - the bright end - at TONE = 0, while the shipping knob is bright at tone = 1, so a
    /// saved preset keeps meaning what it meant. And the travel is limited to the reference's own
    /// stepped range: at TONE = 1 the pot shorts the wiper to the op-amp output and the stage
    /// outputs nothing at all, while at TONE = 0 it shorts away the feedback path and leaves a
    /// bare integrator, so neither mechanical end is a place to let a knob go (D6c plan,
    /// rulings 9 and 14).
    /// @param tone Normalised Tone knob, 0 (dark) to 1 (bright).
    /// @return The wiper position, in [kToneMinimumPosition, kToneMaximumPosition].
    inline double tone_to_position(double tone)
    {
        return kToneMaximumPosition - tone * (kToneMaximumPosition - kToneMinimumPosition);
    }
}
```

- [ ] **Step 2: Write `tests/circuits/DkBlockNode.hpp`,** the `IAudioNode` adapter both block tests
drive `tests/measure/` through. It is the pattern `test_dk_triode_stage.cpp` wrote inline for
`jcm_v1a`, generalised over the block, because two blocks now need it and Task 7's module node is a
third instance of the same float-at-the-boundary rule.

```cpp
/* LICENSE HEADER — copy from PortLayout.hpp, filename line changed */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/math/dk/DkNetlist.hpp"

/// @file
/// @brief A one-in, one-out DkCircuit wrapped as an IAudioNode, for tests/measure/.
///
/// measure::render, frequency_response and alias_floor all drive an IAudioNode. A DK block is not
/// one, so every block-level measurement needs this adapter: the block solves in double and float
/// appears only at the buffer boundary, which is the same rule every product node follows.
/// Test-side only; no product includes it.
namespace SushiDSP::test_circuits
{
    /// @brief Wraps one single-input, single-output DK block as a node.
    /// @tparam Netlist The DkNetlist instantiation describing the block.
    /// @tparam Circuit The matching DkCircuit instantiation.
    /// @tparam Port The block's nonlinear-port glue.
    template <typename Netlist, typename Circuit, typename Port>
    class DkBlockNode final : public IAudioNode
    {
    public:
        /// @brief The block's description; fill it before prepare().
        Netlist netlist{};

        /// @brief Wiper positions applied after prepare()'s derivation.
        std::array<double, dk::kMaximumPotentiometers> pot_positions{ { 0.5, 0.5, 0.5, 0.5 } };

        /// @brief Volts per unit sample at this node's boundary; 1.0 means the buffers are volts.
        double input_scale_volts = 1.0;

        /// @brief Derives the block's matrices at the run rate and applies the wiper positions.
        /// @param info Sample rate, block size and channel count.
        void prepare(const PrepareInfo& info) override
        {
            prepared_ = circuit_.prepare(netlist, info.sample_rate);
            for (std::size_t index = 0; index < dk::kMaximumPotentiometers; ++index)
            {
                circuit_.set_potentiometer(index, pot_positions[index]);
            }
            circuit_.update_matrices_if_dirty();
            circuit_.reset_statistics();
        }

        /// @brief Solves one sample per frame, scaling into volts and back.
        /// @param context The per-block buffers and frame count.
        void process(ProcessContext& context) override
        {
            const float* input = context.inputs == nullptr ? nullptr : context.inputs[0];
            float*       output = context.outputs[0];
            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
            {
                const double drive
                    = input == nullptr ? 0.0
                                       : static_cast<double>(input[frame]) * input_scale_volts;
                std::array<double, 1> outputs{};
                circuit_.process_sample(port_, { drive }, outputs);
                output[frame] = static_cast<float>(outputs[0] / input_scale_volts);
            }
        }

        /// @brief Reports the adapter's fixed channel counts.
        /// @return One input channel and one output channel.
        PortLayout ports() const override { return PortLayout{ 1u, 1u }; }

        /// @brief Clears the block's state.
        void reset() override { circuit_.reset(); }

        /// @brief Reads a parameter; this fixture exposes none.
        /// @param id Ignored.
        /// @return Always zero.
        float get_parameter(const char* id) const override
        {
            static_cast<void>(id);
            return 0.0f;
        }

        /// @brief Writes a parameter; this fixture exposes none.
        /// @param id Ignored.
        /// @param value Ignored.
        void set_parameter(const char* id, float value) override
        {
            static_cast<void>(id);
            static_cast<void>(value);
        }

        /// @brief Whether prepare() derived a usable circuit.
        /// @return True when the derivation succeeded.
        bool prepared() const { return prepared_; }

        /// @brief The wrapped circuit, for reading its convergence counters.
        /// @return The circuit.
        const Circuit& circuit() const { return circuit_; }

    private:
        Circuit circuit_{};
        Port    port_{};
        bool    prepared_{ false };
    };
}
```

- [ ] **Step 3: Extract the SPICE comparison helpers into `tests/circuits/SpiceBlockCompare.hpp`.**
Move `spice_path`, `band_limit` and `interpolate_magnitude` out of
`test_dk_triode_stage.cpp`'s anonymous namespace into `namespace SushiDSP::test_circuits` in the new
header, verbatim and comments included, and have `test_dk_triode_stage.cpp` include it and use the
qualified names. Three files will otherwise carry three copies of a 4th-order Butterworth. This is a
pure refactor: `sd test -s unit` must print the same `jcm_v1a` residuals afterwards
(−97.8 / −94.6 / −88.7 dB).

- [ ] **Step 4: Write the clip block's tests** in `tests/functional/unit/test_ts_clip_stage.cpp`.

```cpp
namespace
{
    using namespace SushiDSP::tube_screamer_circuits;
    using namespace SushiDSP::test_circuits;

    using ClipNode = DkBlockNode<ClipNetlist, ClipCircuit, DiodePairPort>;

    /// @brief The three DRIVE values ts_clip.asc steps, in setting-index order.
    inline constexpr std::array<double, 3> kSteppedDrive{ 0.1, 0.5, 0.9 };

    /// @brief The closed-form small-signal response of the clip stage (D6c plan).
    ///
    /// The nullator forces V(inv) = V(in) and draws no current, so V(in) is the Cin/Rin high-pass
    /// alone; the Rg + Cg leg then draws s*Cg/(1 + s*Cg*Rg) times that out of the summing
    /// junction, and all of it arrives through the feedback admittance Yf from the output.
    /// @param frequency_hz Frequency in hertz.
    /// @param drive The drive pot's position.
    /// @return Magnitude in decibels.
    double clip_closed_form_db(double frequency_hz, double drive)
    {
        const double thermal = 1.752 * 0.025864;                       // n*Vt = 0.045313728 V
        const double diode_conductance = 2.52e-9 / thermal;            // g_d = 5.5612285972e-8 S
        const double feedback_ohms = kFeedbackFixedOhms + drive * kDrivePotOhms;
        const double feedback_farads = kFeedbackFarads + 2.0 * kDiodeJunctionFarads;
        const std::complex<double> s(0.0, 2.0 * M_PI * frequency_hz);
        const std::complex<double> admittance
            = 1.0 / feedback_ohms + s * feedback_farads + 2.0 * diode_conductance;
        const std::complex<double> high_pass
            = (s * kInputFarads) / (s * kInputFarads + 1.0 / kInputLeakOhms);
        const std::complex<double> response
            = high_pass * (1.0 + s * kGroundLegFarads
                                     / ((1.0 + s * kGroundLegFarads * kGroundLegOhms) * admittance));
        return 20.0 * std::log10(std::abs(response));
    }
}

TEST(Unit_Dsp, DkTsClipBlockPreparesAtEveryDrivePositionIncludingBothEndStops)
{
    ClipNetlist       netlist;
    const std::size_t drive = build_clip_block(netlist);
    EXPECT_EQ(netlist.node_count, 6u);

    ClipCircuit circuit;
    ASSERT_TRUE(circuit.prepare(netlist, 384000.0));
    for (const double position : { 0.0, 0.1, 0.5, 0.9, 1.0 })
    {
        circuit.set_potentiometer(drive, position);
        circuit.update_matrices_if_dirty();
        DiodePairPort         port;
        std::array<double, 1> outputs{};
        for (int sample = 0; sample < 64; ++sample)
        {
            circuit.process_sample(port, { 0.1 }, outputs);
            ASSERT_TRUE(std::isfinite(outputs[0])) << position;
        }
    }
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
}

TEST(Unit_Dsp, DkTsClipBlockSmallSignalResponseMatchesItsClosedForm)
{
    // Driven far below the clipping threshold so the diodes stay on their zero-bias conductance,
    // which is the regime the closed form (and an LTspice .ac) describes. The pair's conductance
    // grows as cosh(w / nVt); holding the diode voltage w under 14 mV keeps it within 5 % of the
    // zero-bias value, and at the highest gain here (107.6) that means an input under
    // 0.0143 / 106.6 = 1.34e-4 V. 1e-4 V is used for all three settings.
    //
    // Closed-form values from the D6c plan, in dB:
    //          100 Hz    1000 Hz
    //   0.1   10.1534   25.1395
    //   0.5   18.8134   34.1273
    //   0.9   22.9649   38.2321
    constexpr double kRunRate = 384000.0;
    constexpr double kAmplitudeVolts = 1.0e-4;

    for (const double drive : kSteppedDrive)
    {
        ClipNetlist       netlist;
        const std::size_t pot = build_clip_block(netlist);
        ClipCircuit       circuit;
        DiodePairPort     port;
        ASSERT_TRUE(circuit.prepare(netlist, kRunRate));
        circuit.set_potentiometer(pot, drive_to_position(drive));
        circuit.update_matrices_if_dirty();

        for (const double frequency : { 100.0, 1000.0 })
        {
            circuit.reset();
            std::array<double, 1> outputs{};
            double                peak = 0.0;
            const std::size_t     samples = static_cast<std::size_t>(0.5 * kRunRate);
            for (std::size_t index = 0; index < samples; ++index)
            {
                const double phase = 2.0 * M_PI * frequency * static_cast<double>(index) / kRunRate;
                circuit.process_sample(port, { kAmplitudeVolts * std::sin(phase) }, outputs);
                if (index > samples * 4 / 5)
                {
                    peak = std::max(peak, std::abs(outputs[0]));
                }
            }
            const double measured_db = 20.0 * std::log10(peak / kAmplitudeVolts);
            EXPECT_NEAR(measured_db, clip_closed_form_db(frequency, drive), 0.02)
                << drive << " " << frequency;
        }
    }
}

TEST(Unit_Dsp, DkTsClipBlockClipsAtTheReferencePeakVoltage)
{
    // The reference transient drives 100 mV at 1 kHz and peaks at 0.481683 / 0.490169 / 0.491758 V
    // for DRIVE = 0.1 / 0.5 / 0.9 - almost independent of drive, because the diode pair, not the
    // feedback resistor, is what sets the output above threshold. Back of envelope at DRIVE = 0.1:
    // the peak drives 0.1 / |4700 - j3386.28| = 17.26 uA into the ground leg, and the diode-pair
    // voltage carrying that through Rf in parallel with the pair is 0.3886 V, so the output is
    // 0.1 + 0.3886 = 0.4886 V. This case is the legible failure the null test is not: a stamp sign
    // error moves this number by volts.
    constexpr double kRunRate = 192000.0;
    const std::array<double, 3> expected_peak{ 0.481683, 0.490169, 0.491758 };

    for (std::size_t setting = 0; setting < kSteppedDrive.size(); ++setting)
    {
        ClipNetlist       netlist;
        const std::size_t pot = build_clip_block(netlist);
        ClipCircuit       circuit;
        DiodePairPort     port;
        ASSERT_TRUE(circuit.prepare(netlist, kRunRate));
        circuit.set_potentiometer(pot, drive_to_position(kSteppedDrive[setting]));
        circuit.update_matrices_if_dirty();

        std::array<double, 1> outputs{};
        double                peak = 0.0;
        const std::size_t     samples = static_cast<std::size_t>(0.1 * kRunRate);
        for (std::size_t index = 0; index < samples; ++index)
        {
            const double phase = 2.0 * M_PI * 1000.0 * static_cast<double>(index) / kRunRate;
            circuit.process_sample(port, { 0.1 * std::sin(phase) }, outputs);
            if (index > samples / 2)
            {
                peak = std::max(peak, std::abs(outputs[0]));
            }
        }
        EXPECT_NEAR(peak, expected_peak[setting], 0.01 * expected_peak[setting]) << setting;
    }
}

TEST(Unit_Dsp, DkTsClipBlockNewtonConvergesOnNinetyNinePointNineNinePercentOfSamples)
{
    // Spec's Newton acceptance, on this block's own material: a 2 s sweep 20 Hz -> 20 kHz at
    // -12 dBFS in volts (0.251 * kFullScaleVolts = 0.502 V), then 1 kHz bursts at 0.05 V, 0.5 V
    // and 5 V, all at 384 kHz with the drive pot wide open, which is the hardest setting.
    // Instrumented while writing the D6c plan: 2.7 Newton iterations per solve on average, 4 worst
    // case, at every drive position.
    ...
    std::printf("[ measure ] ts_clip DK: %.4f %% non-converged (%llu of %llu), max %u iterations, "
                "%llu NaN fallbacks\n", ratio, ...);
    EXPECT_LE(ratio, 0.01);
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
}

TEST(Unit_Dsp, DkTsClipBlockNullsAgainstTheLtspiceTransientReference)
{
    // tools/spice/export.py runs `tran 0 40m 20m 1u`: LTspice solves the DC operating point, runs
    // 40 ms and saves the last 20 ms. This block sits at zero at DC - there is no supply rail - so
    // a DK block starting from zero state has LTspice's initial condition exactly, and the saved
    // window is 20 whole cycles of the 1 kHz stimulus. Replaying it ONCE therefore reproduces
    // LTspice's own first 20 ms, and the second pass is the comparison window. Replaying it more
    // converges this block to the periodic steady state while the reference is still on its
    // turn-on transient, and the null gets worse - see the pre-roll table in the D6c plan. This is
    // why jcm_v1a's eight passes are wrong here: that block has a 360 V rail and a bias point.
    //
    // Both signals are low-passed at 10 kHz by the same 4th-order Butterworth before comparison,
    // so the band-limiting cannot flatter either one; the top octave is where LTspice's
    // variable-step integrator and our fixed-step trapezoidal rule are known to disagree (spec
    // Risks). Computed while writing the D6c plan with exactly this procedure:
    // -80.3 / -73.7 / -76.0 dB.
    constexpr std::size_t kPreRollPasses = 1;
    ...
    std::printf("[ measure ] ts_clip tran setting %zu: %.2f dB residual\n", setting, residual);
    EXPECT_LE(residual, -60.0) << "setting " << setting;
}
```

The two elided bodies follow `test_dk_triode_stage.cpp`'s corresponding cases line for line, with
`kSupplyVolts` and the second source dropped (this block has one input) and
`SushiDSP::test_circuits::band_limit` / `spice_path` from Step 3. `null_test(reference, circuit, 256)`
as there.

- [ ] **Step 5: `sd build && sd test -s unit`.** Expected: all five cases green, the three residuals
printed and at or below −80 / −73 / −75 dB. **If a residual lands between −60 and −55 dB**, the
first thing to check is the junction capacitance (Ruling 3): print the response at 5 and 10 kHz
against `clip_closed_form_db` and see whether the feedback pole is where it should be. Report a
residual worse than −55 dB rather than loosening the gate.

- [ ] **Step 6: Add the two benchmark cases** to `tests/benchmark/main.cpp`, beside the `DkCircuit`
group, following `measure_dk_circuit()`'s shape: `measure_ts_clip_block()` prepares the clip block
at `48000 × 8` Hz, settles nothing (there is no bias point), and runs 256 frames × 8 oversampled
steps of the existing 220 Hz test tone scaled to volts by `kFullScaleVolts`;
`measure_ts_tone_block()` does the same for the tone block with `LinearPort`. Both print their
solver counters the way `measure_dk_circuit()` does. Register them under a new
`selected("TubeScreamerBlock", …)` group and print them with no budget — the node's 0.25 ms budget
belongs to the existing `TubeScreamer` case.

- [ ] **Step 7: `sd test -t release -s benchmark`. THE GATE.** Read the two new ns/sample figures
and apply this rule, whose terms are the cost table in "The cost budget" above:

```
budget_ns_per_sample   = 0.25 ms / 256 frames                      = 976.6
predicted_node_cost    = clip_ns + tone_ns + 60.2 + 5.0
                         (60.2 = Oversampler8x round trip, D6a; 5.0 = scaling and smoothers)
```

- `predicted_node_cost ≤ 976.6` → **8× stands.** Record both numbers and go to Wave E.
- `976.6 < predicted_node_cost ≤ 1170` (within 20 %) → pull the reserve levers named in the cost
  section, in order, re-measure, and only then decide.
- `predicted_node_cost > 1170` → **report and stop.** Contingency C1 is a new class and a new
  acceptance argument; it is the controller's call, not a task's.

Put the measured figures and the branch taken in the commit message.

- [ ] **Step 8: Commit**

```bash
git add modules/tubescreamer/include/SushiDSP/tubescreamer/TubeScreamerCircuits.hpp tests/circuits/DkBlockNode.hpp tests/circuits/SpiceBlockCompare.hpp tests/functional/unit/test_dk_triode_stage.cpp tests/functional/unit/test_ts_clip_stage.cpp tests/benchmark/main.cpp
git commit -m "D6c: the TS808 clip and tone circuits as DkNetlists, and the clip block nulling against ts_clip at <a>/<b>/<c> dB (block cost <d> ns/sample)"
```

---

### Task 6: The tone block against its reference

**Files:**
- Modify: `tests/functional/unit/test_ts_tone_stage.cpp` (replace stub)

**Depends on:** Task 5, and on the controller's answer to Self-review open question 1.

- [ ] **Step 1: Write the closed form and the golden-vector response case.** The derivation is in
"The two circuits being implemented"; transcribe it, with the coefficient names it uses.

```cpp
namespace
{
    using namespace SushiDSP::tube_screamer_circuits;
    using namespace SushiDSP::test_circuits;

    using ToneNode = DkBlockNode<ToneNetlist, ToneCircuit, LinearPort>;

    /// @brief The three TONE values ts_tone.asc steps, in setting-index order.
    inline constexpr std::array<double, 3> kSteppedTone{ 0.1, 0.5, 0.9 };

    /// @brief The closed-form response of the tone stage (D6c plan).
    ///
    /// The nullator holds the summing junction at ground, so V(tsout) = -V(tw)/(s R7 C6);
    /// substituting that into KCL at the wiper and then at the resonant node - which the tone
    /// pot loads, so a bare C5/R6 high-pass is the wrong answer - gives a one-zero, two-pole
    /// response, and Cout into Rload adds the second zero and the third pole.
    /// @param frequency_hz Frequency in hertz.
    /// @param tone The tone pot's position, which is the reference's TONE.
    /// @return Magnitude in decibels.
    double tone_closed_form_db(double frequency_hz, double tone)
    {
        const double upper_ohms = kTonePotOhms * tone;
        const double lower_ohms = kTonePotOhms * (1.0 - tone);
        const double upper = 1.0 / upper_ohms;                     // Ga
        const double lower = 1.0 / lower_ohms;                     // Gb
        const double leg = 1.0 / kLowPassLegOhms;                  // GR
        const double resonant = 1.0 / kResonantOhms;               // G6
        const double sum = upper + lower + leg;                    // Gsum
        const std::complex<double> s(0.0, 2.0 * M_PI * frequency_hz);
        const std::complex<double> shunt = s * kResonantFarads + resonant + upper;   // D1(s)
        const std::complex<double> stage
            = (-upper * leg * s * kResonantFarads)
              / (s * kLowPassLegFarads * (shunt * sum - upper * upper) + shunt * lower * leg);
        const double               tau = kLoadOhms * kOutputFarads;                  // 0.1 s
        const std::complex<double> output = s * tau / (1.0 + s * tau);
        return 20.0 * std::log10(std::abs(stage * output));
    }
}

TEST(Unit_Dsp, DkTsToneBlockResponseMatchesItsClosedForm)
{
    // Plateau and poles, at double precision from the D6c plan:
    //   TONE 0.1: plateau -23.392183 dB, poles   8.242319 Hz and 807.150879 Hz
    //   TONE 0.5: plateau -30.370279 dB, poles  24.893080 Hz and 770.881636 Hz
    //   TONE 0.9: plateau -42.477033 dB, poles  55.443640 Hz and 759.949559 Hz
    // and the output network adds a 1.591549 Hz high-pass. Spot magnitudes in dB:
    //          100 Hz    1000 Hz   10000 Hz
    //   0.1  -23.4006   -27.3439   -45.1930
    //   0.5  -30.4289   -34.3828   -52.3802
    //   0.9  -43.1051   -46.2428   -64.2748
    // The block is linear, so any drive level measures the same response; -40 dBFS is what
    // measure::frequency_response uses.
    constexpr double kRunRate = 384000.0;
    const std::vector<double> frequencies{ 20.0,   50.0,   100.0,  200.0,  500.0,
                                           1000.0, 2000.0, 5000.0, 10000.0 };

    for (const double tone : kSteppedTone)
    {
        ToneNode node;
        const TonePotentiometers pots = build_tone_block(node.netlist);
        node.pot_positions[pots.tone] = tone;
        node.pot_positions[pots.level] = kLevelPotPosition;

        const auto measured = SushiDSP::measure::frequency_response(node, kRunRate, frequencies);
        ASSERT_TRUE(node.prepared());
        ASSERT_EQ(measured.size(), frequencies.size());
        for (const auto& point : measured)
        {
            EXPECT_NEAR(point.magnitude_db, tone_closed_form_db(point.frequency_hz, tone), 0.25)
                << tone << " at " << point.frequency_hz << " Hz";
        }
    }
}
```

The 0.25 dB tolerance is the sweep measurement's own accuracy (D6a's note puts the worst error above
20 Hz at 0.17 dB), not the block's: the block reproduces the closed form to under 0.01 dB, which the
transient null in Step 3 is what actually pins.

- [ ] **Step 2: The AC null against `ts_tone_ac.csv`,** the spec's own tone acceptance: for each of
the three settings, at 20, 50, 100, 200, 500, 1000, 2000, 5000 and 10000 Hz, compare
`measure::frequency_response` at 192 kHz against `SushiDSP::test_circuits::interpolate_magnitude` of
the loaded sweep, `EXPECT_NEAR(…, 0.5)`. Same shape as
`DkTriodeStageMatchesTheLtspiceAcSweepWithinHalfADecibel`, with `ToneNode` in place of
`JcmV1aStageNode`.

- [ ] **Step 3: The transient null against `ts_tone_tran.csv`,** exactly the clip block's procedure
and the same one-pass pre-roll, with the same comment. Computed while writing this plan:
**−85.4 / −82.3 / −74.7 dB**; the gate is −60 dB. The tone block is linear, so a residual anywhere
near the gate means a component value or a node index is wrong, not that a solver drifted.

- [ ] **Step 4: The "no notch" acceptance.** §D6c asks that the mid-tone response be monotonic
between 300 Hz and 2 kHz — the D6a crossfade had a full null at 720 Hz at noon and this is the case
that pins its absence.

```cpp
TEST(Unit_Dsp, DkTsToneBlockIsMonotonicBetweenThreeHundredHertzAndTwoKilohertzAtNoon)
{
    // The D5b/D6a tone control crossfaded a Butterworth low-pass against a Butterworth high-pass,
    // both at 720 Hz, so at noon the two cancelled and left a notch there. The real network cannot
    // notch: it is one zero at DC and two real poles, so above the upper pole it falls
    // monotonically. Measured at 24 points, each within 0.01 dB of the closed form.
    ...
    for (std::size_t index = 1; index < magnitudes.size(); ++index)
    {
        EXPECT_LE(magnitudes[index], magnitudes[index - 1] + 0.01) << frequencies[index];
    }
}
```

- [ ] **Step 5: Pin the knob mapping and the degenerate end stops** (Ruling 14), because this is
where a reader will otherwise assume the mapping is the identity.

```cpp
TEST(Unit_Dsp, DkTsToneBlockKnobMapsOntoTheReferencesOwnSteppedRange)
{
    // tone 1 is bright, which is the reference's TONE = 0.1, and tone 0 is dark, TONE = 0.9.
    EXPECT_DOUBLE_EQ(tone_to_position(1.0), 0.1);
    EXPECT_DOUBLE_EQ(tone_to_position(0.5), 0.5);
    EXPECT_DOUBLE_EQ(tone_to_position(0.0), 0.9);
}

TEST(Unit_Dsp, DkTsToneBlockMutesAtTheWiperPositionTheKnobIsKeptAwayFrom)
{
    // At wiper position 1 the pot shorts the wiper to the op-amp output. The op-amp's output node
    // is a norator - zero impedance - so the summing junction's equation collapses to
    // V(tsout) * (1/R7 + s C6) = 0 and the stage outputs nothing; only kMinimumPotOhms keeps it
    // finite, at about -146 dB. This case documents why tone_to_position stops at 0.9, and it is
    // the test that fails first if someone "simplifies" that mapping back to 1 - tone.
    ToneNode node;
    const TonePotentiometers pots = build_tone_block(node.netlist);
    node.pot_positions[pots.tone] = 1.0;
    node.pot_positions[pots.level] = kLevelPotPosition;
    node.prepare(SushiDSP::PrepareInfo{ 384000.0, 256, 1 });
    ASSERT_TRUE(node.prepared());

    const std::vector<double> frequencies{ 100.0, 1000.0 };
    const auto measured = SushiDSP::measure::frequency_response(node, 384000.0, frequencies);
    for (const auto& point : measured)
    {
        EXPECT_LT(point.magnitude_db, -100.0) << point.frequency_hz;
    }
}
```

- [ ] **Step 6: `sd build && sd test -s unit` green; the three residuals printed. Commit**

```bash
git add tests/functional/unit/test_ts_tone_stage.cpp
git commit -m "D6c: the TS808 tone block against its closed form, its LTspice AC sweep within 0.5 dB and its transient at <a>/<b>/<c> dB"
```

---

### Task 7: The module rewrite

**Files:**
- Modify: `modules/tubescreamer/include/SushiDSP/tubescreamer/TubeScreamer.hpp`
- Modify: `modules/tubescreamer/src/TubeScreamer.cpp`
- Delete: `modules/tubescreamer/include/SushiDSP/tubescreamer/TubeScreamerClippingStage.hpp`
- Delete: `modules/tubescreamer/src/TubeScreamerClippingStage.cpp`
- Delete: `tests/functional/unit/test_tube_screamer_clipping_stage.cpp`
- Modify: `modules/tubescreamer/CMakeLists.txt`, `tests/CMakeLists.txt`

**Depends on:** Task 5, and on the controller's answer to Self-review open question 1.

**What does not change:** the four parameters and their ids, order, kinds, ranges and defaults;
`ports()`; `type_id()`; `latency_samples()` (still the oversampler alone); `tube_screamer_descriptor()`;
`set_parameter`/`get_parameter` in both overloads; `kMinFeedbackResistance`/`kMaxFeedbackResistance`
as the published endpoints of the drive law. `kLevelUnityGain` changes value in Task 8, not here.

- [ ] **Step 1: Rewrite the header's private section.** Out: `Biquad input_highpass_`,
`tone_lowpass_`, `tone_highpass_`, `TubeScreamerClippingStage clipping_stage_`, and the
`TubeScreamerClippingStage.hpp` include. In: the two circuits, their ports and their wiper indices.

```cpp
        SmoothedParameter drive_{ 0.5f };
        SmoothedParameter tone_{ 0.5f };
        SmoothedParameter level_gain_{ 1.0f };
        SmoothedParameter input_trim_gain_{ 1.0f };
        float level_db_ = 0.0f;
        float input_trim_db_ = 0.0f;

        tube_screamer_circuits::ClipCircuit   clip_circuit_;
        tube_screamer_circuits::DiodePairPort clip_port_;
        tube_screamer_circuits::ToneCircuit   tone_circuit_;
        tube_screamer_circuits::LinearPort    tone_port_;
        std::size_t                           drive_pot_ = 0;
        std::size_t                           tone_pot_ = 0;
        bool                                  circuits_ready_ = false;

        std::unique_ptr<Oversampler8x> oversampler_;
        std::vector<float> oversampled_scratch_;
        std::vector<float> clipped_scratch_;
```

`kMinFeedbackResistance`/`kMaxFeedbackResistance` stay as public constants and gain a Doxygen line
saying they are now the endpoints of the drive rheostat in `TubeScreamerCircuits.hpp`, kept public
because the descriptor's documented drive law is written in terms of them.

- [ ] **Step 2: Rewrite `prepare()`.** Both circuits derive at the **run rate**, `sample_rate ×
kOversamplingFactor`, not at the host rate — the single most likely mistake in this task.

```cpp
    void TubeScreamer::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;

        tube_screamer_circuits::ClipNetlist clip_netlist;
        drive_pot_ = tube_screamer_circuits::build_clip_block(clip_netlist);
        tube_screamer_circuits::ToneNetlist tone_netlist;
        const tube_screamer_circuits::TonePotentiometers tone_pots
            = tube_screamer_circuits::build_tone_block(tone_netlist);
        tone_pot_ = tone_pots.tone;

        // The DK blocks are stepped once per oversampled sample, so they are discretised at the
        // run rate, not at the host rate.
        const double run_rate = sample_rate_ * static_cast<double>(kOversamplingFactor);
        circuits_ready_ = clip_circuit_.prepare(clip_netlist, run_rate)
                          && tone_circuit_.prepare(tone_netlist, run_rate);

        clip_circuit_.set_potentiometer(
            drive_pot_, tube_screamer_circuits::drive_to_position(drive_.target()));
        tone_circuit_.set_potentiometer(
            tone_pot_, tube_screamer_circuits::tone_to_position(tone_.target()));
        tone_circuit_.set_potentiometer(tone_pots.level,
                                        tube_screamer_circuits::kLevelPotPosition);
        clip_circuit_.update_matrices_if_dirty();
        tone_circuit_.update_matrices_if_dirty();

        drive_.prepare(sample_rate_, kSmoothingMs);
        tone_.prepare(sample_rate_, kSmoothingMs);
        level_gain_.prepare(sample_rate_, kSmoothingMs);
        input_trim_gain_.prepare(sample_rate_, kSmoothingMs);
        drive_.snap(drive_.target());
        tone_.snap(tone_.target());
        level_gain_.snap(level_gain_.target());
        input_trim_gain_.snap(input_trim_gain_.target());

        oversampler_ = std::make_unique<Oversampler8x>(info.max_block);
        oversampled_scratch_.assign(info.max_block, 0.0f);
        clipped_scratch_.assign(static_cast<std::size_t>(info.max_block) * kOversamplingFactor, 0.0f);
        reset();
    }
```

- [ ] **Step 3: Rewrite `process()`.** Knobs at block cadence, both blocks in one oversampled loop.

```cpp
    void TubeScreamer::process(ProcessContext& context)
    {
        const float drive = drive_.skip(context.frames);
        const float tone = tone_.skip(context.frames);
        const float input_gain = input_trim_gain_.skip(context.frames);

        // Wiper positions are a control-rate concern: set_potentiometer marks nothing dirty when
        // the value has not moved, so a still knob costs nothing and a moved one costs one
        // re-derivation per block (D6b plan, ruling 3).
        clip_circuit_.set_potentiometer(
            drive_pot_, tube_screamer_circuits::drive_to_position(static_cast<double>(drive)));
        tone_circuit_.set_potentiometer(
            tone_pot_, tube_screamer_circuits::tone_to_position(static_cast<double>(tone)));
        clip_circuit_.update_matrices_if_dirty();
        tone_circuit_.update_matrices_if_dirty();

        const float* input = context.inputs[0];
        float*       output = context.outputs[0];

        // Volts in, volts out: the DK blocks are the real circuit and kFullScaleVolts is what
        // 0 dBFS means at the instrument jack (spec D5b.5). The D6a 20 Hz input biquad is gone -
        // the clip block's own 1 uF into 510 kOhm is the pedal's real input high-pass, at 0.31 Hz.
        for (std::uint32_t frame = 0; frame < context.frames; ++frame)
        {
            oversampled_scratch_[frame] = input[frame] * input_gain * kFullScaleVolts;
        }

        const float* upsampled = oversampler_->upsample(oversampled_scratch_.data(), context.frames);

        const std::uint32_t frames_oversampled = context.frames * kOversamplingFactor;
        for (std::uint32_t index = 0; index < frames_oversampled; ++index)
        {
            std::array<double, 1> clipped{};
            std::array<double, 1> toned{};
            clip_circuit_.process_sample(clip_port_, { static_cast<double>(upsampled[index]) },
                                         clipped);
            // The clip stage's probe is its op-amp's output node, which has zero output
            // impedance, so the tone block cannot load it: cascading the two within one sample is
            // exact, not an approximation (D6c plan, ruling 8).
            tone_circuit_.process_sample(tone_port_, clipped, toned);
            clipped_scratch_[index] = static_cast<float>(toned[0]);
        }

        oversampler_->downsample(clipped_scratch_.data(), frames_oversampled,
                                 oversampled_scratch_.data());

        for (std::uint32_t frame = 0; frame < context.frames; ++frame)
        {
            // Level alone ramps per sample: it is a single multiply outside the oversampled loop,
            // and stepping it once per block would make the audible smoothing time a function of
            // the host's block size.
            output[frame] = oversampled_scratch_[frame] / kFullScaleVolts * level_gain_.next()
                            * kLevelUnityGain;
        }
    }
```

- [ ] **Step 4: Rewrite `reset()`** as `clip_circuit_.reset(); tone_circuit_.reset();` plus the
oversampler reset. Unlike `JcmV1aStageNode::reset()` there is no bias point to settle, so this stays
cheap and stays RT-safe. Say so in a comment: it is the difference between a pedal and an amp block,
and a reader coming from `test_dk_triode_stage.cpp` will expect a settle.

- [ ] **Step 5: Delete `TubeScreamerClippingStage`** — both files, its test file, its line in
`modules/tubescreamer/CMakeLists.txt` and its line in `tests/CMakeLists.txt`. Nothing else
references it (`modules/jcm800/src/TriodeStage.cpp` mentions it only in a comment about where the
exponent clamp came from; leave that comment alone, it is a historical citation and still true).

- [ ] **Step 6: `sd build && sd test -s unit`.** Expected at this point: the module compiles and
runs, `TubeScreamerProcessesABlock…`, `PortsAreMonoInMonoOut`, `Descriptor…`,
`IndexSetParameterMatches…`, `LevelAndInputTrimClamp…`, `GenericParameterRoundTrip`,
`UnknownParameterIsSafeNoOp`, `ReportsMeasuredImpulseLatency` and `IsDcStableAcrossManyBlocks…`
green; **`DefaultSettingsPeakNearMinusTwelveDbfs…` red at roughly −38 dBFS**, and
`TubeScreamerToneControlsHighFrequencyBalance`, `TubeScreamerInputTrimScalesTheSignalIntoTheClipper`
and `TubeScreamerThdIncreasesWithDrive` possibly red at the new levels. That is expected and is
Task 8's whole job. The two regression golden cases are red for the same reason. Do **not** touch
them here; report which ones are red and with what numbers.

- [ ] **Step 7: Commit**

```bash
git add modules/tubescreamer tests/CMakeLists.txt
git rm modules/tubescreamer/include/SushiDSP/tubescreamer/TubeScreamerClippingStage.hpp modules/tubescreamer/src/TubeScreamerClippingStage.cpp tests/functional/unit/test_tube_screamer_clipping_stage.cpp
git commit -m "D6c: TubeScreamer runs the ts_clip and ts_tone DK blocks in one 8x region; the memoryless clipper and both tone biquads are gone (level calibration follows)"
```

---

### Task 8: Level calibration, the module's own tests, and the golden re-pin

**Files:**
- Modify: `modules/tubescreamer/include/SushiDSP/tubescreamer/TubeScreamer.hpp` (one constant)
- Modify: `tests/functional/unit/test_tube_screamer.cpp`
- Modify: `tests/golden/tube_screamer_48k.f32`, `tests/golden/tube_screamer_96k.f32`,
  `tests/golden/README.md`

**Depends on:** Task 7.

- [ ] **Step 1: Measure the chain's gain and re-pin `kLevelUnityGain`.** The calibration contract is
D5b's and does not change: *at default settings a −12 dBFS 82 Hz sine comes out between −14 and
−10 dBFS*, which is what `TubeScreamerDefaultSettingsPeakNearMinusTwelveDbfsForAMinusTwelveDbfsInput`
asserts. Run that case, read the measured peak from its failure message, and set

```
kLevelUnityGain_new = kLevelUnityGain_old × 10^((−12 − measured_dBFS) / 20)
```

Simulated while writing this plan (both DK blocks at 384 kHz, drive 0.5, tone 0.5, 0.502 V peak in):
the clip block outputs 0.81137 V peak, the tone block 0.0217043 V, so the chain lands at
**−38.40 dBFS** with today's `kLevelUnityGain = 1.1078783` and the new constant should come out near
**23.13**. Use the measured number, not this one — but if the measurement is more than a couple of
dB from −38.4, something in Task 7's wiring is wrong and the calibration is not the place to hide
it.

Update the constant's Doxygen: it is now a **+26 dB makeup**, and it is that large because the tone
stage as netlisted has 30.4 dB of insertion loss at noon (Ruling 13). Say so, and cite this plan, so
the next reader knows the number is a symptom and not a taste decision.

- [ ] **Step 2: Bring the four behavioural cases back to green,** each on its own terms:
  - `TubeScreamerDefaultSettingsPeakNearMinusTwelveDbfsForAMinusTwelveDbfsInput` — green by Step 1,
    with the band unchanged at −14…−10 dBFS.
  - `TubeScreamerToneControlsHighFrequencyBalance` — the assertion (bright above 2 kHz is more than
    twice dark) is a property of any real tone control and must still hold; with the mapping of
    Ruling 14 the span is 19 dB, so it should pass with room. If it does not, stop: the tone
    mapping's sense is inverted.
  - `TubeScreamerThdIncreasesWithDrive` — the clip stage's gain rises from 22.5 to 118 across the
    knob, so this must still hold. Keep the case as written.
  - `TubeScreamerInputTrimScalesTheSignalIntoTheClipper` — read it and adjust only its *levels* if
    the trim's effect has moved; its claim (more input into a compressive stage gives less than
    proportionally more output) is unchanged by the rebuild.
  Any case whose expectation moves gets a comment saying what changed and why, in the same commit.

- [ ] **Step 3: Add two cases the rebuild makes newly meaningful.**

```cpp
TEST(Unit_Dsp, TubeScreamerPassesLowFrequencyContentTheOldTwentyHertzBiquadRemoved)
{
    // The D6a chain high-passed at 20 Hz with a biquad. The real pedal's input network is 1 uF
    // into 510 kOhm - a 0.312069 Hz corner - so a 30 Hz sine now arrives essentially intact where
    // it used to be shelved. Measured against a 300 Hz reference at the same amplitude, small
    // signal so the clipper does not compress the comparison.
    ...
    EXPECT_GT(rms_at_30_hz / rms_at_300_hz, 0.9);
}

TEST(Unit_Dsp, TubeScreamerDriveKnobMovesTheClipThresholdWithoutClicking)
{
    // The drive pot re-derives the clip block's matrices at block cadence (D6b ruling 3). Sweeping
    // it across its whole travel over a second must not produce a sample-to-sample step: the
    // largest first difference stays under the largest one the same render produces with the knob
    // still, times two.
    ...
}
```

- [ ] **Step 4: Re-pin the golden files.** `SUSHIDSP_REGENERATE_GOLDEN=1 sd test -s regression`,
then confirm a second `sd test -s regression` is green, then commit the two `.f32` files and add a
row to `tests/golden/README.md`:

```
| tube_screamer_48k/96k | D6c Task 8, 2026-09-05 | TubeScreamer rebuilt on two DkCircuit blocks from the ts_clip/ts_tone SPICE references (spec D6c); the memoryless clipper, the 20 Hz input biquad and the LP/HP tone crossfade are gone, and kLevelUnityGain is re-calibrated 1.1078783 -> <new> for the tone stage's insertion loss |
```

The JCM800 goldens must **not** move: nothing in D6c touches `modules/jcm800`. If they do, a change
leaked out of this phase and that is the bug.

- [ ] **Step 5: `sd test -s all` green. Commit**

```bash
git add modules/tubescreamer/include/SushiDSP/tubescreamer/TubeScreamer.hpp tests/functional/unit/test_tube_screamer.cpp tests/golden
git commit -m "D6c: re-calibrate the Tube Screamer's output for the DK chain (kLevelUnityGain 1.1078783 -> <new>) and re-pin its golden files"
```

---

### Task 9: Alias floor, RT-safety traps, and the node's budget

**Files:**
- Modify: `tests/functional/unit/test_measure.cpp`
- Modify: `tests/functional/unit/test_rt_safety.cpp`

**Depends on:** Task 7. Changes no behaviour, so it may run beside Task 8.

- [ ] **Step 1: Measure the alias floor and re-state its acceptance.** `Regression_Dsp,
TubeScreamerAliasFloorDoesNotRegress` currently asserts `≤ −9.0 dB` and carries a long comment
saying the target is "deferred to D6c". D6c is now. Run it, read the printed figure, and:
  - **≤ −38.3 dB** (30 dB better than the D5b −8.3 dB baseline, the spec's D6a target): change the
    assertion to `EXPECT_LE(floor, -38.3)`, rename the case to
    `TubeScreamerAliasFloorMeetsTheThirtyDecibelImprovementTarget`, and replace the deferral comment
    with the measured number and the mechanism — the feedback pole at 26.7 / 9.0 / 5.4 kHz is what
    stops the high-order harmonics being generated at the level that folded (see "What the alias
    floor should do" in this plan).
  - **between −9 and −38.3 dB**: keep the case as a regression floor at the measured value rounded
    toward zero by 1 dB, keep its name, and rewrite the comment with the new number, the mechanism,
    and what is left. Do not delete the D6a diagnosis; it is still the correct account of where the
    aliasing comes from.
  - **worse than −9 dB**: stop and report. The rebuild was supposed to make this better by
    construction; if it did not, something is generating harmonics that the physical circuit cannot.
  Either way the spec's own −85 dB is not reachable by this route and stays open (Self-review,
  open question 3).

- [ ] **Step 2: Arm the RT trap over a knob-moving block.** `TubeScreamerProcessPerformsNoHeapAllocation`
already exists and now covers both `process_sample` paths for free. What it does not cover is the
one audio-thread path that re-runs a matrix derivation. Extend the case, inside the trap window,
to call `node.set_drive(...)` and `node.set_tone(...)` with a different value before each of its
blocks, so `update_matrices_if_dirty()` actually derives, and assert `g_trapped_allocation_count`
is still zero. Add a comment: this is the D6c instance of the trap D6b's
`DkCircuitPotentiometerUpdatePerformsNoHeapAllocation` established, now on a shipping node.

- [ ] **Step 3: `sd build && sd test -s all` green.**

- [ ] **Step 4: `sd test -t release -s benchmark`. The node's budget.** Read the `TubeScreamer` row,
which already prints against a 0.25 ms budget. Record it beside D6a's 535.5 ns/sample (0.137 ms) and
beside Task 5's two block figures, so the sum is checkable:
`TubeScreamer ≈ clip + tone + oversampler + scaling`. **Do not change the budget number in
`tests/benchmark/main.cpp`.** If the row is over, report DONE_WITH_CONCERNS with the measured
figure, the three block figures it decomposes into, and the levers from the cost section; the
oversampling decision was already taken with data at Task 5's gate and is not re-taken here.

- [ ] **Step 5: Commit**

```bash
git add tests/functional/unit/test_measure.cpp tests/functional/unit/test_rt_safety.cpp
git commit -m "D6c: TubeScreamer alias floor -9.6 -> <a> dB on the rebuilt stage, RT trap armed over a knob-moving block, node cost <b> ms/256 against 0.25"
```

---

### Task 10: Retire `DiodeClipperCircuit` — **gated: do not dispatch without the controller's yes**

**Files:**
- Delete: `include/SushiDSP/circuits/DiodeClipperCircuit.hpp`, `src/circuits/DiodeClipperCircuit.cpp`,
  `tests/functional/unit/test_diode_clipper_circuit.cpp`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/functional/unit/test_rt_safety.cpp`,
  `tests/functional/unit/test_dk_circuit.cpp`, `README.md`

**Depends on:** Task 9 (shares `test_rt_safety.cpp`). Droppable whole: nothing else in this plan
references the class (Ruling 10).

**The test surface that replaces it.** `test_dk_circuit.cpp`'s diode-clipper case already carries
two assertions: DK within **1e-11 V of the true root** of `v = Vs − Rs·Is·(exp(v/Vt) − exp(−v/Vt))`,
found by bisection inside the test, and agreement with `DiodeClipperCircuit` to 1e-6 V wherever the
oracle itself converged (with an oracle failure-rate assertion under 2 %, because it does not always
converge). The first is strictly stronger than the second — it compares against the equation rather
than against another approximation of it — and it does not need the class. What is genuinely lost is
an *independently written* implementation; what replaces it is an independently written *root*.

- [ ] **Step 1: Cut the conditional oracle out of `test_dk_circuit.cpp`.** Remove the
`DiodeClipperCircuit` include, its instance, the 1e-6 agreement assertion and the failure-rate
assertion. Keep the bisection root, the 1e-11 assertion, the convergence and NaN assertions, and the
whole stimulus. Rewrite the case's comment to say what it now claims and why that is the stronger
claim, citing this plan.

- [ ] **Step 2: Delete the class and its test file**, and remove
`src/circuits/DiodeClipperCircuit.cpp` from the root `CMakeLists.txt` and
`functional/unit/test_diode_clipper_circuit.cpp` from `tests/CMakeLists.txt`.

- [ ] **Step 3: Remove `DiodeClipperCircuitProcessPerformsNoHeapAllocation`** from
`test_rt_safety.cpp` and drop `DiodeClipperCircuit` from the list in `README.md`'s
"Determinism and safety" paragraph, replacing it with `DkCircuit` — which is what actually carries
that duty now and already has three trap cases.

- [ ] **Step 4: `sd build && sd test -s all` green; `sd doxygen` produces no dangling reference to
the deleted header.** `grep -r DiodeClipperCircuit` should return only historical mentions in
`docs/reference/CHANGELOG.md`, `docs/superpowers/plans/2026-08-26-d6b-dk-circuit-core.md` and the
comment in `modules/jcm800/src/TriodeStage.cpp` that cites where its exponent clamp came from. None
of the three is edited: a changelog and a landed plan record what happened, and the citation is
still true.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt tests/CMakeLists.txt tests/functional/unit/test_dk_circuit.cpp tests/functional/unit/test_rt_safety.cpp README.md
git rm include/SushiDSP/circuits/DiodeClipperCircuit.hpp src/circuits/DiodeClipperCircuit.cpp tests/functional/unit/test_diode_clipper_circuit.cpp
git commit -m "D6c: retire DiodeClipperCircuit; the DK diode-clipper case keeps the stronger oracle, agreement with the true root of the equation itself"
```

---

### Task 11: Documentation

**Files:**
- Modify: `docs/design/sushidsp.md` (§4.5 and §6), `docs/reference/CHANGELOG.md`, `docs/README.md`

**Depends on:** everything.

- [ ] **Step 1: `docs/design/sushidsp.md` §4.5, "As built in D6b" section.** Append an **As built in
D6c** paragraph covering the core changes only: the nullor element and what it is for; the joint
`current_and_jacobian` contract and the one-method `NonlinearSystem`; the removal of the post-solve
`currents()` call and the solver contract it rests on; `DenseLu<2>`; and the re-measured
`DkCircuit 2-nonlin 7-state @8x` figure against D6b's 3762.5 ns/sample, which is the number D6d
inherits.

- [ ] **Step 2: `docs/design/sushidsp.md` §6, after "As built in D5b".** Add **As built in D6c**:
the two DK blocks with their shapes and node counts; one 8× region; the pot mappings including
Ruling 14's restricted tone travel and why; the removal of the input biquad, the tone crossfade and
`TubeScreamerClippingStage`; the measured nulls, alias floor, convergence figures and cost; the
re-calibrated `kLevelUnityGain` and the insertion loss behind it. Correct §6's opening prose too —
it still says "WDF/DK-method" and "an inverting op-amp with a parallel diode pair", both of which
are now wrong (the real stage is non-inverting and there is no WDF anywhere).

- [ ] **Step 3: `docs/reference/CHANGELOG.md`.** A `## D6c — Tube Screamer rebuilt on DK` section
above D6b, following D6b's shape: the new element and the model change, the two blocks, the measured
nulls per setting, the AC agreement, convergence, the alias floor before and after, the benchmark
table with the budget column, the golden re-pin, the deletions, and the deviations — Ruling 3's
fixed junction capacitance, Ruling 9's deferred taper, Ruling 14's restricted tone travel, the −85 dB
alias target still open, and the `ts_tone` topology question with its three numbers. Every deviation
gets its number, the way D6b's entry does.

- [ ] **Step 4: `docs/README.md`.** Add index entries for
`modules/tubescreamer/include/SushiDSP/tubescreamer/TubeScreamerCircuits.hpp` (where the pedal's
component values live) and for `tests/circuits/`'s two new headers, beside the existing
`tests/circuits/` entry.

- [ ] **Step 5: `sd test -s all` green; `sd doxygen` produces no new warnings. Check every path this
phase's documents cite resolves. Commit**

```bash
git add docs
git commit -m "D6c: design doc, changelog and manual index for the DK Tube Screamer and the core changes it needed"
```

---

## No Placeholders

Every code block above is either complete and compilable as written, or is explicitly marked as a
shape whose body the surrounding step describes in prose and whose sibling already exists in the
tree to copy from: Task 5 Step 4's two elided test bodies (the convergence case and the null case,
both line-for-line from `test_dk_triode_stage.cpp`), Task 5 Step 6's two benchmark functions (from
`measure_dk_circuit()`), Task 6 Steps 2 and 3 (from the corresponding `jcm_v1a` cases), Task 8
Step 3's two cases, and Task 2 Step 3's `DenseLu<2>` body (from `DenseLu<1>`). Nothing is stubbed
"for later".

`<a>`, `<b>`, `<c>`, `<d>` and `<new>` in commit messages are values produced by the step
immediately above them.

Every number this plan asserts was computed here at double precision, and every one that could be
cross-checked against the committed references was: the clip stage's closed form against
`ts_clip_ac.csv` (worst 0.004 dB over 600 points), the tone stage's against `ts_tone_ac.csv` (worst
0.004 dB over 600 points), the nullor stamp against four analytic op-amp circuits, and both blocks'
full DK derivations against the two transient CSVs (−80.3 / −73.7 / −76.0 dB and
−85.4 / −82.3 / −74.7 dB, band-limited to 10 kHz through `null_test`'s own gain and lag alignment).
The Newton iteration counts and the pre-roll table are measurements of the same model. The cost
table is an extrapolation from D6b's benchmark and is labelled as one; Task 5's gate is what turns
it into a number.

## Self-review

- **Spec coverage.** §D6c's schematic: the 1 µF into 510 kΩ replacing the 20 Hz biquad → Task 5's
  netlist and Task 8 Step 3's low-frequency case; the non-inverting op-amp stage → Ruling 1 and
  Task 4; `Rf = 51 kΩ + Drive·500 kΩ ∥ 51 pF` → Ruling 2 and Ruling 3; the 4.7 kΩ + 47 nF ground leg
  → the 720.4841 Hz corner in Task 5's closed form; the antiparallel `SiliconDiode` pair as two
  branches → `DiodePairPort`; the tone section as a second DK block with pot handling through the
  same path → Task 5's `build_tone_block` and Task 6; one 8× region wrapping both → Task 7 Step 3
  and Ruling 12. §D6c's acceptance: AC vs `ts_tone.csv` within ±0.5 dB at three settings → Task 6
  Step 2; clip transient null vs `ts_clip.csv` → Task 5 Step 4; no notch at mid tone → Task 6
  Step 4; alias floor → Task 9 Step 1; regression golden re-pinned → Task 8 Step 4; cost → Task 5's
  gate and Task 9 Step 4. §D6a.4's budget and §D6b.4's Newton criteria → the cost section, Task 5
  Step 7 and the convergence cases in Tasks 5 and 6.
- **What this plan changes in the D6b core, and why that is proportionate.** Three things:
  a new element (nothing could express an op-amp), a changed glue contract (the measured 8.0× budget
  miss D6b handed forward), and one deleted call (the same). Each is one task, each keeps every D6b
  number where it was, and Task 3's Step 6 makes "the `jcm_v1a` nulls do not move" an explicit gate
  rather than an assumption. D6d inherits all three and is better off for them.
- **SOLID shape.** One responsibility per new unit: `TubeScreamerCircuits.hpp` describes circuits and
  knows nothing about nodes or blocks; `DkBlockNode` adapts one block to `IAudioNode` and knows
  nothing about which block; `SpiceBlockCompare.hpp` compares against SPICE and knows nothing about
  circuits; `DiodePairPort` and `LinearPort` are the same shape as `TriodeStagePort` and sit beside
  the models they call. The two block tests are siblings and are written the same way, deliberately.
  Nothing in `math/` learns anything about a circuit model: the nullor is a topology element, and
  the models still reach `DkCircuit` only as a template parameter.
- **Where the risk is.** Task 7 is the only task that can silently produce a plausible-but-wrong
  pedal, because it is the only one with no reference to null against — the blocks are validated in
  Tasks 5 and 6 and the *node* is validated only by level, alias floor and golden files. The
  mitigation is that Task 7 changes no numbers of its own: every constant it uses comes from Task 5's
  header, and the one place it could go wrong quietly is preparing the blocks at the host rate
  instead of the run rate, which is called out in its Step 2. Second-largest risk is Task 5's gate
  landing in the 976–1170 ns band, where the decision needs judgement rather than a rule.
- **Honest scope narrowing, stated rather than dropped:** no audio taper (Ruling 9); a fixed rather
  than voltage-dependent diode junction capacitance (Ruling 3); `Rs = 0.568 Ω` dropped (Ruling 3);
  the tone knob's travel restricted to the reference's validated span (Ruling 14); the clip stage
  nulled at three DRIVE settings rather than §D6c's "three drive × three level combinations",
  because the level pot is in the *tone* block and is not a clip-stage variable at all — the spec
  sentence contradicts the block split the same paragraph defines, and the committed reference steps
  DRIVE only.

### Open questions for the controller

1. **`ts_tone.asc`'s topology — blocking before Wave E.** The netlist's own README flags it as
   unverified against a traced TS808. Three measurements say it is wrong: the knob moves the level
   19.08 dB while the treble corner moves only 807.2 → 760.0 Hz; the stage loses 30.4 dB at noon,
   which forces a +26 dB makeup constant into the shipping module; and one mechanical end of the pot
   mutes the pedal outright. Changing `R7` from 10 kΩ to 1 kΩ does not fix any of it. If the
   topology is to be corrected, the order is: fix `ts_tone.asc`, re-export both its CSVs
   (`python tools/spice/export.py --block ts_tone`, LTspice 24 is installed), then run Tasks 6 and 7
   against the new values — which are one constant table in `TubeScreamerCircuits.hpp`. Doing it
   after Task 8 means calibrating and re-pinning the goldens twice. **Waves A–D are unaffected
   either way and can start now.**
2. **Deleting `DiodeClipperCircuit` (Task 10).** A public core header goes away. The recommendation
   and the replacement test surface are in Ruling 10 and Task 10; the decision is not mine to take.
   Yes runs Task 10, no drops it whole and costs the plan nothing else.
3. **The −85 dB alias target.** §D6c's acceptance table asks for it; nothing in the physical analysis
   predicts it. The feedback pole should buy 30 to 50 dB over D6a's −9.6 dB, which meets the D6a
   target this phase inherits (−38.3 dB) and does not come near −85 dB. Reaching −85 dB would need
   16× for the clip block or ADAA, and the spec's own decision table rules ADAA out. Should the −85
   dB row be revised to the measured figure, or is closing that gap a phase of its own?
4. **−60 dB or −55 dB for the clip null.** The brief says −60; §D6c's own text says −55. The plan
   gates at −60 and the simulation says both blocks clear it with 13 dB to spare, so the question is
   only what happens if the C++ implementation gives back more than 13 dB to the fixed junction
   capacitance. Confirm −55 dB is the documented floor in that case.
5. **Contingency C1 adds a public core class.** If Task 5's gate fails badly enough to need
   `Oversampler4x`, that is a new class in `include/SushiDSP/math/` and a change to the pedal's
   latency — a boundary decision, so the plan stops and asks rather than proceeding. Confirm that is
   the behaviour you want.
6. **The audio taper (Ruling 9).** Deferred, with reasons. It is a one-function change in
   `TubeScreamerCircuits.hpp` whenever you want it; confirm the deferral rather than discovering it
   in the changelog.
