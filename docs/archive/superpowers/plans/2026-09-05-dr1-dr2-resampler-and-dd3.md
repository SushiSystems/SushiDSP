# DR1 + DR2 — `math/Resampler` and the Boss DD-3 Delay Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Two deliverables. **DR1** is one shared brick — `Resampler`, an arbitrary-ratio polyphase
interpolator over a single committed prototype filter, allocation-free after `prepare()`, whose
ratio may change per block. **DR2** is the Boss DD-3 (long-chip revision) as one `IAudioNode`,
built as a variable-rate loop around that resampler, meeting acceptance rows 1–4 of the spec and
the binding benchmark budget of 0.15 ms per 256-frame block at 48 kHz.

**Architecture:** DR1 follows the tree's existing filter-design pattern exactly: a Python script
under `tools/dsp/` designs the prototype and emits a committed coefficient header, and a small
C++ class walks it. `tools/dsp/design_resampler.py` →
`include/SushiDSP/math/ResamplerCoefficients.hpp` (the kernel's right wing, sampled 512× per zero
crossing), consumed by `include/SushiDSP/math/Resampler.hpp` + `src/math/Resampler.cpp`. One
prototype, one table, one fractional phase accumulator; a ratio change is a change to the
accumulator's step and to a kernel time-scale, never a rebuilt filter bank. `math/Oversampler`
is untouched — it solves the fixed-integer-ratio problem and keeps its halfbands.

DR2 is a new product module, `modules/dd3/`, shaped exactly like `modules/tubescreamer/` and
`modules/jcm800/`: a static library, its own `include/SushiDSP/dd3/` tree, one class per block
of the schematic, and one `IAudioNode` on top that owns them.

```
Dd3AnalogFilters   pre-emphasis and de-emphasis shelves, the 6th-order anti-alias
                   cascade and the 4th-order reconstruction cascade, all as Biquads
                   with coefficients derived from the schematic's component values
Ne570Compander     one envelope-driven gain block, instantiated twice (compressor,
                   expander); tau = 6.8 ms; the two instances never share state
Dd3Converter       the 12-bit SAR/DAC characteristic and the fixed-length ring
                   (N = 1024 / 4096 / 16384), with HOLD as a write inhibit
Dd3Clock           TIME knob -> 10 kA log taper -> R56/C52 slew -> VCO law -> fs(t)
Dd3Delay           the IAudioNode: owns the four blocks and two Resamplers, closes
                   the feedback loop around the whole chain, mixes dry and wet
```

Each block is testable on its own and knows nothing of its neighbours; `Dd3Delay` is the only
class that knows the pedal's topology. The D6b DK core is **not** used: no block here is a
nonlinear network needing a Newton solve, and the dossier's evidence binds behaviour, not
netlists.

**Tech Stack:** C++17 (`std::vector` sized in `prepare`, `<cmath>`), the existing `Biquad`,
`SmoothedParameter`, `NodeDescriptor`/`IAudioNode` core, GoogleTest in the existing
`sushidsp_tests` binary, the `tests/measure/` library, the `tests/golden/` snapshot convention,
and the `tests/benchmark/` harness. Python + numpy/scipy for the design-time script only.

**Spec:** `docs/superpowers/specs/2026-09-05-dd3-spx90-design.md` — "New shared brick:
`math/Resampler`" (DR1), "DD-3 — variable-rate loop (approach A1)" (DR2), the acceptance table
rows 1–4, the 0.15 ms/256 benchmark budget, and the Phasing section's DR1→DR2 order.
**Research source:** `docs/reference/research/2026-08-26-boss-dd3-design-dossier.md` §1 (component
values), §3 (measurable targets) and §4 (topology). Every component value in this plan is
transcribed from §1; none is rounded or invented.

## Global Constraints

- Build and test only through the `sd` CLI: `sd build`, `sd test` (`-s unit|regression|benchmark|all`,
  `-t release`), `sd doxygen`. Never invoke cmake, ninja or ctest directly.
- No git worktrees; work directly in the `main` checkout (user ruling, D5b). Commit only the files
  your task names; never `git add -A`.
- **Do not touch** `apps/host_gui/**`, `src/core/RigGraph*`, or `modules/rig_preset/**`. The DD-3
  reaches the GUI through `modules/node_registry` alone (Task 2.7), which is a one-line addition.
- C++17 only. Allman braces. Types `PascalCase`, functions and variables `snake_case`, members with
  a trailing underscore, constants `kPascalCase`. Spelled-out names, no abbreviations —
  `saturation_current`, not `is`; `reconstruction_filter_`, not `recon_`.
- Every `.hpp`/`.cpp`, tests included, carries the 76-column boxed license header: copy it
  **verbatim** from `include/SushiDSP/core/PortLayout.hpp` and change only the filename line, padded
  so the closing `*/` stays in column 76. In the code blocks below,
  `/* LICENSE HEADER — copy from PortLayout.hpp, filename line changed */` stands for that block.
- Doxygen `@file`/`@brief` at the top of every file and `@brief`/`@param`/`@return` on every public
  function, struct and template parameter, test-side helpers included. A Doxygen block is at most
  eight lines; a file header at most six.
- Comments only where the *why* is non-obvious. Every equation transcribed from the "Numbers"
  section below carries a one-line comment naming it, e.g.
  `// f0 = 1/(2*pi*R*sqrt(C1*C2))  (DR1+DR2 plan, Sallen-Key section table)`.
- **RT safety.** `Resampler::process`, `Resampler::set_rates`, `Dd3Delay::process` and
  everything reachable from them must be allocation-free, lock-free, syscall-free and throw-free.
  Every buffer is sized in `prepare()`. Task 2.7 adds the `*ProcessPerformsNoHeapAllocation`
  traps to `tests/functional/unit/test_rt_safety.cpp`; the existing traps stay green after every
  task.
- **Double where precision matters, float at the buffers.** Phase accumulators, sample rates,
  envelope states, filter coefficient derivations and the VCO law are `double`. The kernel table,
  the ring, the scratch buffers and the `IAudioNode` boundary are `float`.
- Tests: `TEST(Unit_Dsp, …)` for unit cases, `TEST(Regression_Dsp, …)` for anything that renders
  seconds of audio (the acceptance and alias-floor cases, and the golden files), per
  `docs/CONTRIBUTING.md` §4. Expectations are golden vectors — an analytic closed form or a value
  hand-computed in this plan — never the implementation's own output.
- `_USE_MATH_DEFINES` before any include in a `.cpp` that uses `M_PI`, with the `#ifndef M_PI`
  fallback, exactly as every existing file does.
- Numbers that bind these two phases: resampler prototype passband edge `0.400 × f_low`, stopband
  edge `0.500 × f_low`, design stopband **−100 dB**, group delay **32 samples at `f_low`**; DD-3
  fs range **20.0–82.5 kHz**; N = **1024 / 4096 / 16384**; compander τ = **6.8 ms**; VCO slew pole
  **R56 22 kΩ / C52 0.027 µF**; benchmark **≤ 0.15 ms per 256-frame block at 48 kHz** (binding).

### Rulings taken while writing this plan (spec deviations and choices the spec left open)

1. **The prototype is a Kaiser-windowed sinc, half-width 32 zero crossings, tabulated at 512
   points per zero crossing, stored as the right wing only.** The kernel is even, so the left wing
   is the same table read backwards; committing one wing halves the header. Table length is
   `32 × 512 + 2 = 16386` entries: 16385 to reach the far zero crossing inclusive, plus one guard
   entry so the linear interpolation between adjacent table points can always read `h[i]` and
   `h[i + 1]`. Both trailing entries are exactly `0.0f`. The alternative (a per-phase polyphase
   bank, 512 banks of 64 taps) stores the same 32 768 numbers with no interpolation but cannot be
   time-scaled for decimation, which Ruling 2 needs.
2. **Decimation scales the kernel in time; interpolation does not.** With `ratio = target/source`,
   the kernel is walked with `scale = min(1.0, ratio)` and its output multiplied by `scale`, so the
   filter's cutoff always sits at `0.45 × min(source, target)` and its DC gain stays exactly 1.
   This is the one-table form of Julius Smith's resampler and is what makes "one fixed prototype
   filter with a fractional phase accumulator" (spec) work for both directions. Consequence: the
   number of taps touched per output is `64 / scale`, so `prepare()` must be told the lowest target
   rate the caller will ever ask for, and sizes its history buffer for that worst case.
3. **`prepare()` takes a worst-case ratio; `set_rates()` is the per-block control.**
   The spec fixes the ratio at `prepare(source_rate, target_rate, max_block)` and then says the
   DD-3's ratio changes per block. Both are honoured by keeping `prepare()` the only allocating call
   and adding one RT-safe setter:
   `prepare(source_rate, target_rate, max_input_frames, minimum_ratio)` allocates for the worst case,
   and `set_rates(source_rate, target_rate)` changes the phase step and the kernel scale and
   allocates nothing. **Both** rates are settable, not just the target, because the DD-3's second
   conversion runs `fs(t) → 48 kHz`, where it is the *source* that moves. A ratio outside
   `[minimum_ratio, kMaximumRatio]` is clamped, not asserted, because `set_rates` is reached from
   the audio thread.
4. **`Resampler` is push-form and returns a variable output count.** `process(input, input_frames,
   output, output_capacity)` returns how many samples it produced. A pull form ("give me exactly n
   outputs") would have to own an input FIFO, which is the caller's problem and different for every
   caller. `maximum_output_frames(input_frames)` gives the capacity a caller must provide.
5. **The DD-3 reports `latency_samples() == 0`.** Its dry path is sample-aligned (the LEVEL pot
   mixes dry and wet inside the node, as the pedal does), so there is nothing for a host to
   compensate. The wet path's delay *is* the effect, and the resampler's group delay adds to the
   delay time, not to the node's latency. The wet-path total is stated in the "Numbers" section and
   asserted in Task 2.6, not reported through `latency_samples()`.
6. **The resampler's group delay is not compensated out of the ring.** The spec says "the spec
   accepts the floor and the plan states the number once the prototype is designed". The floor is
   **+1.500 ms** on the wet path (1.3333 ms of resampler round trip plus 0.1667 ms of wet FIFO
   prime), which at the shortest setting takes the delay from the pedal's 12.412 ms to 13.912 ms,
   +11.30 %, and at the longest setting from 819.2 ms to 822.5 ms, +0.41 %. Reading the ring at
   `N − d` instead of `N` would cancel it exactly (arithmetic in the "Numbers" section); it is not
   done here because the spec accepts the floor, and it is carried to Self-review as an open
   question rather than decided.
7. **No oversampling of the analog chain in DR2.** The dossier §4.4 asks for ×4 to ×8 across
   everything between pre-emphasis and the reconstruction filter; this plan runs those blocks at
   the host rate. Reasons, in order: (a) the spec's own block chain does not call for it; (b) a 2×
   halfband pair would add `2 × (115 − 1)/4 = 57` base-rate samples — 1.1875 ms at 48 kHz — to the
   wet path, nearly doubling the Ruling 6 floor the spec already accepts under protest; (c) the
   only high-Q section near Nyquist is the reconstruction filter's 15.758 kHz / Q 3.367, and RBJ
   prewarping places its peak exactly where the component values put it; (d) the compander's
   nonlinearity is an envelope-driven multiply whose control signal is band-limited to a 23.4 Hz
   rectifier corner, not a hard clipper. This is a **deliberate scope narrowing, written down per
   `docs/CONTRIBUTING.md` §2**: Task 2.6 measures the node's alias floor with
   `tests::measure::alias_floor` and **prints** it, so the decision is revisited against a number
   rather than against this paragraph.
8. **The block order is the schematic's, not the spec's list.** The spec writes
   `AA LPF → compressor → pre-emphasis → Resampler`; the dossier's §1.0 signal chain and its §1.6
   feedback finding both put pre-emphasis first: "F.BACK injects at the pre-emphasis summing node,
   i.e. *upstream of the anti-alias filter*. It has to — the anti-alias filter must see the
   recirculated signal." The spec's own prose agrees ("the repeat signal re-enters before the
   anti-alias filter"). Implemented order:
   `pre-emphasis → anti-alias → compressor → resample down`, and mirrored on the way out.
9. **The de-emphasis shelf sits in the wet branch's input leg, not in the mixer's feedback.**
   Read literally, dossier §1.0 puts the shelved network (`R16 47K ∥ (R15 10K + C10 0.0047 µ)`) in
   IC1a's feedback, where it would shelve the **dry** branch (`R13 33K`) by the same −15.12 dB above
   3.4 kHz. That contradicts everything the DD-3 is known for; its dry path is transparent. Modelled
   instead as: wet → de-emphasis shelf → `47/33`, dry → `47/33`, summed. Electrically this is the
   shelf moved into the wet input leg; audibly it is the only reading consistent with the pedal.
   Dossier §5 open question 2 already flags the emphasis network's leg as confidence C, and §4.7
   item 8 (photograph the board) is the resolution. Carried to Self-review.
10. **MODE is not a `SmoothedParameter`.** The spec lists all four parameters as smoothed; a mode
    index is discrete, and ramping it would sweep `N` through values that are neither 1024, 4096 nor
    16384. MODE is a plain `int` with a change detector; the spec's own sentence — "MODE changes swap
    N and clear the ring" — describes a discrete event, not a ramp. TIME, FEEDBACK and LEVEL are
    `SmoothedParameter` as specified.
11. **The clock and the resampler ratio update every 8 frames, not once per block.** The R56/C52
    pole's time constant is 0.594 ms; a 256-frame block at 48 kHz is 5.333 ms, nine time constants,
    so a once-per-block ratio update renders every TIME change as a single jump and acceptance row 4
    ("shows the R56/C52 glide, not a jump") cannot pass. `Dd3Delay::process` therefore walks the
    block in sub-blocks of at most `kClockUpdateFrames = 8` samples (0.1667 ms, 0.281 τ, so the pole
    advances 24.47 % per sub-block) and calls `set_rates` once per sub-block. The resampler
    still sees one constant ratio per call, which is what the spec's "ratio changes per block" asks
    of it.
12. **`Biquad` gains one new coefficient setter, `set_first_order_shelf`.** The emphasis networks
    are first-order shelves with independent zero and pole (594.07 Hz and 3386.28 Hz), which RBJ's
    symmetric `set_high_shelf` cannot express. The addition follows the precedent and the reasoning
    already in `set_one_pole_low_pass`'s Doxygen — "the right shape for a physical RC pole". Both
    corners are prewarped independently so each lands exactly, which makes the pre/de-emphasis pair
    cancel to the last bit (proof in the "Numbers" section). This is a **public interface change to
    a core brick** and is carried to Self-review for the controller to confirm.
13. **The R-2R ladder's differential nonlinearity is deferred.** Dossier §4.3 item 2 asks for a
    per-code offset table "calibrated from a measured low-level sine staircase". No such measurement
    exists — dossier §3 records that no independent bench measurement of a DD-2/DD-3 has ever been
    published. DR2 implements the ideal 12-bit mid-tread characteristic with saturation on overflow;
    the per-code table is a later addition behind the same interface, and the header says so.
14. **The ring clears when `N` changes, not on every mode change.** The spec says "MODE changes swap
    N and clear the ring", and for S/M/L that is exactly right. Applied literally to the fourth
    position it makes HOLD useless: L and HOLD share `N = 16384` and the same clock range (dossier
    1.8's truth table), the LSI's HOLD pin only inhibits writes, and clearing the DRAM on the way
    into HOLD would hold silence — the one thing the control exists not to do. So `set_mode` clears
    the ring **iff** the new mode's `N` differs from the current one, which reproduces the spec's
    sentence everywhere it was aimed and keeps HOLD working.
15. **The converter saturates on overflow; it does not wrap.** Dossier §5 open question 3 says the
    behaviour is undocumented and §4.3 item 3 suggests a switch. DR2 ships saturation only — the
    quieter, non-destructive choice — and records the switch as an open question rather than adding
    a public knob nobody can yet calibrate.

### Parallelism map (for the controller)

Files are disjoint within every wave; no two tasks in a wave touch one file.

- **Task 0** (controller, inline, before Wave 1): every CMake edit both phases need, and every test
  stub, so no later task touches a build file.
  Files: `CMakeLists.txt`, `tests/CMakeLists.txt`, `modules/dd3/CMakeLists.txt`,
  `modules/node_registry/CMakeLists.txt`, and eight test stubs.

- **Wave 1** — two fully independent tasks, run concurrently:
  - **Task 1.1** the prototype filter: `tools/dsp/design_resampler.py`,
    `include/SushiDSP/math/ResamplerCoefficients.hpp`, `tools/dsp/README.md`,
    `tests/functional/unit/test_resampler_coefficients.cpp`.
  - **Task 1.2** `Biquad::set_first_order_shelf`: `include/SushiDSP/math/Biquad.hpp`,
    `src/math/Biquad.cpp`, `tests/functional/unit/test_biquad.cpp`.

- **Wave 2** — **Task 1.3** the resampler itself (needs Task 1.1):
  `include/SushiDSP/math/Resampler.hpp`, `src/math/Resampler.cpp`,
  `tests/functional/unit/test_resampler.cpp`.

- **Wave 3** — four fully independent DD-3 blocks, run concurrently (Task 2.1 needs Task 1.2; the
  other three need nothing from Waves 1–2):
  - **Task 2.1** `modules/dd3/include/SushiDSP/dd3/Dd3AnalogFilters.hpp`,
    `modules/dd3/src/Dd3AnalogFilters.cpp`, `tests/functional/unit/test_dd3_analog_filters.cpp`.
  - **Task 2.2** `…/dd3/Ne570Compander.hpp`, `modules/dd3/src/Ne570Compander.cpp`,
    `tests/functional/unit/test_ne570_compander.cpp`.
  - **Task 2.3** `…/dd3/Dd3Converter.hpp`, `modules/dd3/src/Dd3Converter.cpp`,
    `tests/functional/unit/test_dd3_converter.cpp`.
  - **Task 2.4** `…/dd3/Dd3Clock.hpp`, `modules/dd3/src/Dd3Clock.cpp`,
    `tests/functional/unit/test_dd3_clock.cpp`.

- **Wave 4** — **Task 2.5** the node (needs Tasks 1.3, 2.1, 2.2, 2.3, 2.4):
  `…/dd3/Dd3Delay.hpp`, `modules/dd3/src/Dd3Delay.cpp`,
  `tests/functional/unit/test_dd3_delay.cpp`.

- **Wave 5** — two independent tasks, run concurrently once Task 2.5 lands:
  - **Task 2.6** acceptance and snapshots: `tests/functional/unit/test_dd3_acceptance.cpp`,
    `tests/functional/regression/test_dd3_regression.cpp`, `tests/golden/dd3_48k.f32`,
    `tests/golden/dd3_96k.f32`, `tests/golden/README.md`.
  - **Task 2.7** traps, benchmark, registry, documentation: `tests/functional/unit/test_rt_safety.cpp`,
    `tests/benchmark/main.cpp`, `modules/node_registry/src/NodeRegistry.cpp`,
    `modules/dd3/README.md`, `docs/README.md`, `docs/reference/CHANGELOG.md`,
    `docs/design/sushidsp.md`.

A plan-level note on width: Waves 1 and 3 are the wide ones (two and four tasks). Waves 2 and 4 are
single tasks because both are the integration points their wave exists to reach — the resampler
cannot be split without splitting its phase accumulator from its kernel walk, and the node cannot be
split without inventing a second owner for the feedback loop.

---

## The numbers this plan asserts

Every task below transcribes from this section. Read it once before touching code. Nothing here is
rounded from a source: where the dossier prints a rounded value (`4.47 kHz`, `Q 0.66`), the exact
figure derived from the component values is given beside it, and the exact figure is what the code
carries.

### N1. The resampler prototype

Design: `h(t) = sinc(2 * fc * t) * kaiser(t / half_width, beta)`, with `t` in input samples and
`fc` normalised to the **lower** of the two sample rates.

| Quantity | Value | Where it comes from |
|---|---|---|
| Half-width | 32 zero crossings | 64 taps at unit scale (Ruling 1) |
| Table density | 512 entries per zero crossing | Ruling 1 |
| Table length | 16386 entries | `32 * 512 + 2`; last two exactly `0.0f` |
| Sinc cutoff `fc` | 0.45 * `f_low` | midpoint of the transition band, below |
| Kaiser beta | 10.0613 | `0.1102 * (A - 8.7)` with `A = 100` dB |
| Passband edge | 0.400 * `f_low` = 0.800 * Nyquist_low | `0.5 - transition width` |
| Stopband edge | 0.500 * `f_low` | Nyquist of the lower rate |
| Design stopband | -100 dB | the `A` the beta was solved for |

**Transition width.** Kaiser's estimator, `N = (A - 8) / (2.285 * dw)` with `N = 64` taps and
`A = 100` dB, gives

```
dw = (100 - 8) / (2.285 * 64) = 92 / 146.24 = 0.629103 rad/sample
df = dw / (2*pi)              = 0.629103 / 6.283185 = 0.100125 cycles/sample
```

so with the stopband edge pinned at `0.5` the passband edge is `0.5 - 0.100125 = 0.399875`, and the
sinc cutoff sits at the midpoint, `0.5 - 0.100125/2 = 0.449937`, which this plan rounds to
`fc = 0.45`. **Kaiser beta**, for `A > 50`: `beta = 0.1102 * (A - 8.7) = 0.1102 * 91.3 = 10.0613`.

**What that buys each consumer.** Passband flat to `0.400 * f_low`:

| Consumer | `f_low` | Flat to | Note |
|---|---|---|---|
| DD-3, TIME at the fast end | 48 000 Hz | 19 200 Hz | the pedal's own wall is at 7 kHz |
| DD-3, TIME at the slow end | 20 000 Hz | 8 000 Hz | above the 7 kHz delay-path spec |
| SPX90 (future, DR3) | 31 250 Hz | 12 500 Hz | the SPX90's published band is 20 Hz-12 kHz |

**Linear interpolation between table entries.** With 512 entries per zero crossing the step is
`d = 1/512` input samples. The kernel's curvature is bounded by `(2*pi*fc)^2 = (2*pi*0.45)^2 =
7.9948` per input-sample-squared, so the worst interpolation error is
`|h''| * d^2 / 8 = 7.9948 * (1/512)^2 / 8 = 7.9948 * 3.8147e-6 / 8 = 3.8127e-6`, i.e. **-108.4 dB**
relative to the kernel's peak of 1. That sits a comfortable 8 dB below the -100 dB stopband, which
is why the table is 512 dense and not 256 (256 would give -96.4 dB and would set the floor instead
of the filter).

**Group delay.** The kernel is symmetric with half-span 32 zero crossings, and Ruling 2 scales it by
`s = min(1, ratio)`, so its half-span in **input samples** is `32 / s` and in seconds it is always

```
group delay = 32 / min(source_rate, target_rate)   seconds
```

| Conversion | Group delay (s) | In 48 kHz samples | In ms |
|---|---|---|---|
| 48 000 -> 48 000 | 32 / 48 000 | 32.0 | 0.666667 |
| 48 000 -> 82 500 (interpolate) | 32 / 48 000 | 32.0 | 0.666667 |
| 82 500 -> 48 000 (decimate) | 32 / 48 000 | 32.0 | 0.666667 |
| 48 000 -> 20 000 (decimate) | 32 / 20 000 | 76.8 | 1.600000 |
| 20 000 -> 48 000 (interpolate) | 32 / 20 000 | 76.8 | 1.600000 |
| 48 000 -> 31 250 (SPX90) | 32 / 31 250 | 49.152 | 1.024000 |

So a **round trip** `48 k -> fs -> 48 k` costs `64 / min(fs, 48000)` seconds: **1.333333 ms**
whenever `fs >= 48 kHz`, and `64 / fs` when `fs < 48 kHz` (3.2 ms at the 20 kHz end).

### N2. The DD-3's wet-path delay, and the minimum-time floor

`fs(TIME)` is exponential between the two published endpoints (N5). Ring delay is `N / fs`. The
model's wet path adds the resampler round trip (N1) and the 8-sample wet FIFO prime that keeps the
two conversions' sample counts reconcilable (Ruling 11's sub-blocks emit 7, 8 or 9 samples; the
prime is `kWetPrimeFrames = 8` = 0.166667 ms at 48 kHz).

```
T_wet(mode, TIME) = N / fs  +  64 / min(fs, 48000)  +  8 / 48000       seconds
```

| Setting | Ring | Resampler | Prime | Model total | Published | Error |
|---|---|---|---|---|---|---|
| S, TIME = 0 (fs = 82 500) | 12.412121 ms | 1.333333 | 0.166667 | **13.912121 ms** | 12.5 ms | +11.30 % |
| S, TIME = 1 (fs = 20 000) | 51.2 ms | 3.2 | 0.166667 | **54.566667 ms** | 50 ms | +9.13 % |
| M, TIME = 0 | 49.648485 ms | 1.333333 | 0.166667 | **51.148485 ms** | 50 ms | +2.30 % |
| M, TIME = 1 | 204.8 ms | 3.2 | 0.166667 | **208.166667 ms** | 200 ms | +4.08 % |
| L, TIME = 0 | 198.593939 ms | 1.333333 | 0.166667 | **200.093939 ms** | 200 ms | +0.05 % |
| L, TIME = 1 | 819.2 ms | 3.2 | 0.166667 | **822.566667 ms** | 800 ms | +2.82 % |

Arithmetic for the ring column: `1024/82500 = 0.0124121212 s`, `1024/20000 = 0.0512 s`,
`4096/82500 = 0.0496484848 s`, `4096/20000 = 0.2048 s`, `16384/82500 = 0.1985939394 s`,
`16384/20000 = 0.8192 s`.

**The floor is +1.500 ms at the fast end** (`1.333333 + 0.166667`), which is Ruling 6's number. It
is not the largest error in the table: the `N = 1024/4096/16384` choice contributes
`819.2/800 - 1 = +2.40 %` at the long end on its own (N5), and the slow-clock resampler delay
contributes 3.2 ms.

**What compensation would cost, if the controller ever wants it (Ruling 6, open question).** Read
the ring at `N - d` instead of `N`, with `d` the round trip expressed in fs-rate samples:
`d = 64 * fs / min(fs, 48000)`, i.e. `d = 64` samples for `fs <= 48 000` and
`d = 64 * 82500/48000 = 110` samples at the fast end. Then
`T_wet = (N - d)/fs + 64/min(fs, 48000) = N/fs` exactly, and only the 0.166667 ms prime remains.
At mode S / TIME = 0 the read offset would be `1024 - 110 = 914` samples, comfortably positive, so
the compensation is implementable; it is not implemented, because the spec accepts the floor.

### N3. The Sallen-Key cascades, from the schematic's component values

Every section is a unity-gain equal-R Sallen-Key lowpass buffered by a bare NPN emitter follower
(dossier 1.3, 1.4), so

```
f0 = 1 / (2*pi*R*sqrt(C1*C2))        Q = 0.5 * sqrt(C1/C2)
```

with `C1` the cap bridging to the buffer's output and `C2` the cap shunting the base to ground.

**Anti-alias, 6th order (dossier 1.3 - `LPF Q3-Q6: 2SC732TM-GR fc=7KHz`):**

| Section | R | C1 (bridge) | C2 (shunt) | `sqrt(C1*C2)` | f0 (exact) | Q (exact) | Dossier |
|---|---|---|---|---|---|---|---|
| 1 (Q3) | 10 kOhm | 4700 pF (C9) | 2700 pF (C8) | 3.562303e-9 | **4467.73 Hz** | **0.6596857** | 4.47 k / 0.66 |
| 2 (Q4) | 10 kOhm | 6800 pF (C15) | 470 pF (C14) | 1.787736e-9 | **8902.32 Hz** | **1.9018468** | 8.90 k / 1.90 |
| 3 (Q6) | 10 kOhm | 0.018 uF (C17) | 220 pF (C16) | 1.989975e-9 | **7997.84 Hz** | **4.5226702** | 8.00 k / 4.52 |

Arithmetic, section by section:

```
1: C1*C2 = 4.7e-9 * 2.7e-9 = 1.269e-17;  sqrt = 3.562303e-9
   f0 = 1/(2*pi*1e4*3.562303e-9) = 1/2.238267e-4 = 4467.73 Hz
   Q  = 0.5*sqrt(4700/2700) = 0.5*sqrt(1.7407407) = 0.5*1.3193714 = 0.6596857
2: C1*C2 = 6.8e-9 * 4.7e-10 = 3.196e-18; sqrt = 1.787736e-9
   f0 = 1/(2*pi*1e4*1.787736e-9) = 1/1.123250e-4 = 8902.32 Hz
   Q  = 0.5*sqrt(6800/470) = 0.5*sqrt(14.468085) = 0.5*3.8036936 = 1.9018468
3: C1*C2 = 1.8e-8 * 2.2e-10 = 3.96e-18;  sqrt = 1.989975e-9
   f0 = 1/(2*pi*1e4*1.989975e-9) = 1/1.250337e-4 = 7997.84 Hz
   Q  = 0.5*sqrt(1.8e-8/2.2e-10) = 0.5*sqrt(81.818182) = 0.5*9.0453404 = 4.5226702
```

**Reconstruction, 4th order (dossier 1.4 - `LPF Q9-Q10: 2SC732TM-GR fc=7KHz`):**

| Section | R | C1 (bridge) | C2 (shunt) | `sqrt(C1*C2)` | f0 (exact) | Q (exact) | Dossier |
|---|---|---|---|---|---|---|---|
| 1 (Q10) | 10 kOhm | 2700 pF (C38) | 1200 pF (C37) | 1.8e-9 exactly | **8842.18 Hz** | **0.75** exactly | 8.84 k / 0.75 |
| 2 (Q9) | 10 kOhm | 6800 pF (C35) | 150 pF (C36) | 1.0099505e-9 | **15758.51 Hz** | **3.3665016** | 15.8 k / 3.37 |

```
1: C1*C2 = 2.7e-9*1.2e-9 = 3.24e-18; sqrt = 1.8e-9 exactly
   f0 = 1/(2*pi*1e4*1.8e-9) = 1/1.1309734e-4 = 8842.18 Hz
   Q  = 0.5*sqrt(2.7/1.2) = 0.5*sqrt(2.25) = 0.5*1.5 = 0.75 exactly
2: C1*C2 = 6.8e-9*1.5e-10 = 1.02e-18; sqrt = 1.0099505e-9
   f0 = 1/(2*pi*1e4*1.0099505e-9) = 1/6.345710e-5 = 15758.51 Hz
   Q  = 0.5*sqrt(6800/150) = 0.5*sqrt(45.333333) = 0.5*6.7330033 = 3.3665016
```

**Analytic cascade magnitudes** (the goldens acceptance row 2 asserts against), using
`|H_k(f)| = 1 / sqrt((1 - x^2)^2 + (x/Q)^2)` with `x = f/f0`:

| f | AA section 1 | AA section 2 | AA section 3 | AA cascade | dB |
|---|---|---|---|---|---|
| 1 000 Hz | 0.991395 | 1.010971 | 1.015481 | 1.017788 | **+0.1531** |
| 7 000 Hz | 0.359028 | 1.777115 | 3.293367 | 2.101083 | **+6.4487** |
| 10 000 Hz | 0.190382 | 1.547827 | 1.593564 | 0.469527 | **-6.5631** |
| 20 000 Hz | 0.049474 | 0.237185 | 0.189308 | 0.0022213 | **-53.0667** |

| f | Recon section 1 | Recon section 2 | Recon cascade | dB |
|---|---|---|---|---|
| 1 000 Hz | 1.001342 | 1.003864 | 1.005211 | **+0.0451** |
| 7 000 Hz | 0.893173 | 1.229320 | 1.097942 | **+0.8117** |
| 20 000 Hz | 0.195969 | 1.393226 | 0.273029 | **-11.2802** |

Worked example, AA section 3 at 7 000 Hz: `x = 7000/7997.84 = 0.875237`, `x^2 = 0.766040`,
`(1 - x^2)^2 = 0.233960^2 = 0.054737`, `(x/Q)^2 = (0.875237/4.5226702)^2 = 0.193521^2 = 0.037450`,
sum `0.092187`, `sqrt = 0.303623`, `|H| = 3.293367`.

**A contradiction this plan does not hide.** The anti-alias cascade as derived **peaks +6.45 dB at
7 kHz** and is only -6.56 dB at 10 kHz. That contradicts the published delay-path figure the dossier
quotes at confidence A - 40 Hz-7 kHz, +1/-3 dB - and it contradicts the dossier's own structural
argument that "at the slowest clock Nyquist is 10 kHz against a 6th-order near-elliptic 7 kHz wall,
so there is essentially no in-band aliasing even at 800 ms". A cascade 6.6 dB down at Nyquist is not
a wall. Either one of the three C-value readings is wrong, or the emitter follower's finite gain and
output impedance damp the high-Q sections substantially - `Q` is very sensitive to buffer gain in a
Sallen-Key, and the dossier reads these values off a scan at confidence B. This plan implements
exactly what the component values give and asserts the analytic goldens above, which is what
acceptance row 2 asks for ("analytic goldens from component values"), and carries the contradiction
to Self-review with the dossier's own resolution path, 4.7 item 1: a swept sine on a real unit
through the DIRECT/MONO wet-only trick.

### N4. The emphasis pair

**Pre-emphasis (IC1b, dossier 1.2)**, taking the dossier's own ruling that `R8 + C4` sits in the
input leg - "companding theory requires pre-emphasis to *boost* treble before the converter". The
inverting stage's input impedance is `R7 || (R8 + 1/(s*C4))`, its feedback `R12 || 1/(s*C5)`:

```
Zi(s) = R7 * (1 + s*R8*C4) / (1 + s*(R7 + R8)*C4)
H(s)  = -(R12/R7) * (1 + s*(R7+R8)*C4) / ((1 + s*R8*C4) * (1 + s*R12*C5))
```

With `R7 = 47 kOhm`, `R8 = 10 kOhm`, `R12 = 33 kOhm`, `C4 = 0.0047 uF`, `C5 = 33 pF`:

```
zero  f_z  = 1/(2*pi*(R7+R8)*C4) = 1/(2*pi*57000*4.7e-9) = 1/1.683308e-3 = 594.07 Hz
pole  f_p  = 1/(2*pi*R8*C4)      = 1/(2*pi*10000*4.7e-9) = 1/2.953097e-4 = 3386.28 Hz
pole  f_p2 = 1/(2*pi*R12*C5)     = 1/(2*pi*33000*33e-12) = 1/6.842362e-6 = 146149 Hz
low-frequency gain  = R12/R7     = 33/47 = 0.7021277   (-3.0745 dB)
shelf ratio         = (R7+R8)/R8 = 57/10 = 5.7         (+15.1175 dB)
high-frequency gain = 0.7021277 * 5.7 = 4.0021277      (+12.0430 dB)
```

The dossier's "approx +10.4 dB shelf above ~3.4 kHz" is the `33k/10k = 3.3` shorthand, which ignores
that `R7` stays in parallel at high frequency; the exact network gives
`33k/(47k || 10k) = 33/8.245614 = 4.0021277`. The corner it quotes, ~3.4 kHz, is `f_p = 3386.28 Hz`
exactly. **The code carries the exact values.**

**De-emphasis (IC1a)** is the mirror: `R16 = 47 kOhm` feedback shunted by
`R15 = 10 kOhm + C10 = 0.0047 uF`, so its zero is at `1/(2*pi*R15*C10) = 3386.28 Hz` and its pole at
`1/(2*pi*(R16+R15)*C10) = 594.07 Hz` - the same two corners, swapped - and its low-frequency gain
into the wet leg is `R16/R14 = 47/33 = 1.4242424` (+3.0745 dB), with `C12 = 33 pF` across `R16`
adding a pole at `1/(2*pi*47000*33e-12) = 102 598 Hz`.

**The pair is exactly unity.** `0.7021277 * 1.4242424 = 1.0` and `5.7 * (1/5.7) = 1.0`, and the two
corner frequencies are the same pair swapped, so every zero of one is a pole of the other. The only
residue in the analog domain is the two 33 pF poles, at 146.149 kHz and 102.598 kHz:

```
at 10 kHz: -20*log10(sqrt(1+(10/146.149)^2)) = -0.0203 dB
           -20*log10(sqrt(1+(10/102.598)^2)) = -0.0411 dB   total -0.0614 dB
at  7 kHz: -0.0100 dB and -0.0202 dB                         total -0.0301 dB
```

Both poles sit **above Nyquist at every rate this node runs at** (24 kHz at 48 kHz base, 48 kHz at
96 kHz base), so `C5` and `C12` are dropped from the model, stated in `Dd3AnalogFilters`'s header
with these two frequencies. With them dropped, and with Ruling 12's independent prewarping of the
zero and the pole, the discretised pre/de-emphasis pair cancels **exactly**: the numerator
polynomial of one is the denominator polynomial of the other, and the scalars multiply to 1. Task
2.1 asserts that cascade nulls below -120 dB, a far tighter golden than any tolerance-based
frequency check.

**Discretised pre-emphasis coefficients at 48 kHz**, from Ruling 12's form
(`t = tan(pi*f/fs)`, `b0 = g*(tp/tz)*(tz+1)/(tp+1)`, `b1 = g*(tp/tz)*(tz-1)/(tp+1)`,
`a1 = (tp-1)/(tp+1)`):

```
tz = tan(pi*594.07/48000)  = tan(0.03887919) = 0.0388988
tp = tan(pi*3386.28/48000) = tan(0.22162865) = 0.2253301
tp/tz = 5.792728        g = 0.7021277        g*(tp/tz) = 4.067235
b0 =  4.067235 * ( 1.0388988/1.2253301) = 4.067235 *  0.8478529 =  3.4484165
b1 =  4.067235 * (-0.9611012/1.2253301) = 4.067235 * -0.7843612 = -3.1901795
a1 = (0.2253301 - 1)/1.2253301 = -0.7746699/1.2253301 = -0.6322130
check DC:      (b0+b1)/(1+a1) = 0.2582370/0.3677870 = 0.702141   (target 0.7021277)
check Nyquist: (b0-b1)/(1-a1) = 6.6385960/1.6322130 = 4.067239   (analog asymptote 4.0021277;
               the 1.6 % excess is the prewarped shelf's asymptote and cancels against the
               de-emphasis stage, which is why the pair still nulls exactly)
```

### N5. `fs(TIME)`, the VCO curve, and the mode boundaries

The published endpoints (dossier 1.7, confidence A, read verbatim from the factory adjustment
procedure) are `T = 12.12 us -> fs = 82.5 kHz` at one end of D.TIME and `T = 50 us -> fs = 20.0 kHz`
at the other. The scope point is the sample-and-hold, so `T` is genuinely the sample period. The
10 kA log taper against an exponential-ish VCO law is modelled as a single exponential in the knob:

```
fs(TIME) = kFastRateHz * (kSlowRateHz / kFastRateHz)^TIME
         = 82500 * (20000/82500)^TIME = 82500 * 4.125^(-TIME)
ln(4.125) = 1.4170660        log2(4.125) = 2.0443847
```

with `TIME = 0` the fastest clock (shortest delay) and `TIME = 1` the slowest.

| TIME | fs (Hz) | Mode S delay (N = 1024) | Mode M (N = 4096) | Mode L / HOLD (N = 16384) |
|---|---|---|---|---|
| 0.00 | 82 500.0 | 12.412121 ms | 49.648485 ms | 198.593939 ms |
| 0.25 | 57 889.5 | 17.689 ms | 70.755 ms | 283.021 ms |
| 0.50 | 40 620.4 | 25.209 ms | 100.836 ms | 403.345 ms |
| 0.75 | 28 502.5 | 35.927 ms | 143.707 ms | 574.828 ms |
| 1.00 | 20 000.0 | 51.2 ms | 204.8 ms | 819.2 ms |

`4.125^0.5 = 2.0310096`, so `fs(0.5) = 82500/2.0310096 = 40620.4 Hz`; `4.125^0.25 = 1.4251349`, so
`fs(0.25) = 82500/1.4251349 = 57889.5 Hz`. Use the exponential form in code, not these intermediate
roots - the table is for the reader; the tests evaluate `82500 * 4.125^(-TIME)` directly.

**Acceptance row 1 is a check on `fs`, and it passes.** The published *delay* ranges are
12.5-50 / 50-200 / 200-800 ms. The clock those correspond to, given this plan's `N`:

| Published time | Required fs | Model fs at that setting | Error |
|---|---|---|---|
| 12.5 ms, mode S | `1024/0.0125 = 81 920 Hz` | 82 500 Hz | **+0.708 %** |
| 50 ms, mode S | `1024/0.050 = 20 480 Hz` | 20 000 Hz | **-2.344 %** |
| 50 ms, mode M | `4096/0.050 = 81 920 Hz` | 82 500 Hz | **+0.708 %** |
| 200 ms, mode M | `4096/0.200 = 20 480 Hz` | 20 000 Hz | **-2.344 %** |
| 200 ms, mode L | `16384/0.200 = 81 920 Hz` | 82 500 Hz | **+0.708 %** |
| 800 ms, mode L | `16384/0.800 = 20 480 Hz` | 20 000 Hz | **-2.344 %** |

The fast end is inside the spec's 2 % band; **the slow end misses it by 0.344 percentage points**,
and it misses for one reason: `N = 1024/4096/16384` (spec-bound, and the value the DRAM arithmetic
supports - `3 * 64 Kbit = 196 608 = 16 384 * 12`) against published delay times computed with
`N = 1000/4000/16000` (`50 ms * 20 kHz = 1000` exactly, dossier 1.8's own check column). The two
cannot both be right and the dossier flags it as open question 4. **Task 2.6 asserts the 2 % band
against the fs endpoints as published** - `fs(0) = 82500 +/- 2 %` and `fs(1) = 20000 +/- 2 %`, which
pass exactly - and **prints** the derived delay-time errors above so the `N` question is visible in
the test output. Carried to Self-review.

**The VCO slew.** `R56 = 22 kOhm`, `C52 = 0.027 uF` (dossier 1.7, effect board, control node):

```
tau = 22000 * 2.7e-8 = 5.94e-4 s = 0.594 ms
f_c = 1/(2*pi*tau) = 1/3.732124e-3 = 267.945 Hz
one-pole coefficient at 48 kHz: exp(-1/(48000*5.94e-4)) = exp(-1/28.512)
                              = exp(-0.03507300) = 0.96553486
per 8-frame sub-block (Ruling 11): exp(-8/28.512) = exp(-0.28058305) = 0.7553430
                              -> the pole closes 24.4657 % of the remaining distance per sub-block
```

The pole is applied to the **control voltage**, i.e. to `log(fs)`, because that is the node the R/C
actually sits on; a constant-rate glide in `log fs` is a constant-rate glide in pitch, which is what
the pedal does. `SmoothedParameter::prepare(sample_rate, 0.594f)` is exactly this pole - its
documented contract is "time to reach 63 % of a step" - so no new smoother is written.

**Pitch-slur golden (acceptance row 4).** A TIME step from `TIME = 0` to `TIME = 1` moves
`log2(fs)` by `log2(4.125) = 2.0443847` octaves. After one time constant the control has covered
63.212 % of it, `0.63212 * 2.0443847 = 1.2922934` octaves, so the pitch ratio of the recirculating
material at `t = 0.594 ms` is `2^-1.2922934 = 0.408259` of its starting value. Task 2.6 traces the
frequency and asserts the trace is monotone with no single step larger than the 24.4657 % sub-block
quantum computed above - i.e. a glide, not a jump.

### N6. The NE570 compander

The datasheet gives one time constant for both halves (dossier 1.5, quoted verbatim):
`G(t) = (G_initial - G_final)*e^(-t/tau) + G_final`, `tau = 10k * C_RECT`, and the DD-2/DD-3 use
`C32 = C33 = 0.68 uF` for both:

```
tau = 10000 * 6.8e-7 = 6.8e-3 s = 6.8 ms                (symmetric, both halves)
rectifier ripple corner = 1/(2*pi*10000*6.8e-7) = 1/0.042725660 = 23.4051 Hz
one-pole coefficient at 48 kHz: exp(-1/(48000*6.8e-3)) = exp(-1/326.4)
                              = exp(-0.00306373) = 0.99694096
```

**The two laws.** The compressor's rectifier hangs on the gain cell's **output** (feedback), the
expander's on its **input** (feedforward); that is the whole reason the pair is 2:1 and 1:2. With
`V_ref` the gain cell's unity point and `env` a full-wave rectifier calibrated to read the peak of a
sine (mean-|x| * pi/2 = peak):

```
compressor: gain = V_ref / max(env_of_output, V_ref * kEnvelopeFloor)
            steady state  V_out = (V_ref/V_out) * V_in  =>  V_out = sqrt(V_ref * V_in)
expander:   gain = max(env_of_input, V_ref * kEnvelopeFloor) / V_ref
            steady state  V_out = V_in^2 / V_ref
```

`kEnvelopeFloor = 1e-4`, so the maximum compressor gain is 80 dB and the law is linear below
`-80 dB` relative to `V_ref`; the header says so, and Task 2.2 asserts the knee.

**Static curve goldens** (`V_ref = 1` for the table; the code's `V_ref` is N7's calibrated value, and
the law depends only on `V_in/V_ref`):

| `V_in / V_ref` | dB in | compressor out | dB out | expander out | dB out |
|---|---|---|---|---|---|
| 0.001 | -60 | 0.0316227766 | -30 | 1e-6 | -120 |
| 0.01 | -40 | 0.1 exactly | -20 | 1e-4 | -80 |
| 0.1 | -20 | 0.3162277660 | -10 | 0.01 | -40 |
| 1.0 | 0 | 1.0 exactly | 0 | 1.0 exactly | 0 |
| 4.0 | +12.0412 | 2.0 exactly | +6.0206 | 16.0 exactly | +24.0824 |
| 10.0 | +20 | 3.1622776602 | +10 | 100.0 | +40 |

`sqrt(0.1) = 0.31622776601683794`, `sqrt(10) = 3.1622776601683795`. **The round trip is the
identity**: `expander(compressor(x)) = (sqrt(V_ref*x))^2/V_ref = x` for every `x`, exactly, in the
static limit - which is Task 2.2's strongest golden. That the two do **not** cancel dynamically -
the expander is open-loop and sees the reconstructed, quantised, band-limited signal, never what the
compressor did (dossier 1.9 item 2, 4.3 item 1) - is the pedal's voice, and Task 2.2 asserts the
mismatch is present by driving the pair with a step and showing the residual.

**Envelope step golden.** A step from 0 to A at the rectifier: the envelope reaches
`A * (1 - e^-1) = 0.6321206 * A` after exactly 6.8 ms = `0.0068 * 48000 = 326.4` samples at 48 kHz,
and `0.8646647 * A` after two time constants (652.8 samples). Task 2.2 asserts the 63.21 % crossing
lands within +/- 1 % of 6.8 ms.

### N7. The check-point ladder, and what it does and does not calibrate

Dossier 1.0 prints the service note's ladder for a 1 kHz, 100 mVp-p input: `[1] 620 mVp-p`
(pre-emphasis out), `[2] 400 mVp-p` (anti-alias out), `[3] 2.3 Vp-p` (sample-and-hold),
`[4] 2.4 Vp-p` (reconstruction out), `[5] 140 mVp-p` (delay signal).

**What it calibrates.** The `[2] -> [3]` step fixes the compander's reference. `[2]` is 0.2 V peak
in, `[3]` is 1.15 V peak out, and the compressor's law is `V_out = sqrt(V_ref * V_in)`:

```
V_ref = V_out^2 / V_in = 1.15^2 / 0.2 = 1.3225 / 0.2 = 6.6125 V
```

so `kCompanderReferenceVolts = 6.6125`. The `[3] -> [4]` step then fixes the expander's output
network: the expander returns `1.15^2 / 6.6125 = 0.2` V peak = 0.4 Vp-p, and `[4]` reads 2.4 Vp-p,
so `kExpanderOutputStageGain = 2.4 / 0.4 = 6.0`, standing for the fixed gain of `R42 47K` with
`C20 0.0027 || R41 22K` that the dossier gives values for but never resolves into a number.

**What it does not calibrate, and this plan says so plainly.** The transcribed component values do
**not** reproduce `[1]`. The pre-emphasis stage's own gain at 1 kHz is

```
|H(1k)| = 0.7021277 * sqrt((1 + (1000/594.07)^2) / (1 + (1000/3386.28)^2))
        = 0.7021277 * sqrt(3.8335160 / 1.0872080)
        = 0.7021277 * sqrt(3.5259940) = 0.7021277 * 1.8777630 = 1.3183700
```

so 100 mVp-p in gives 131.837 mVp-p at `[1]`, against the published 620 mVp-p - a factor of
`620 / 131.837 = 4.70358` unaccounted for. The input buffer (a JFET source follower into a BJT
emitter follower, dossier 1.1) has a gain near 0.9, not 4.7. The model carries the residual as one
honestly-named constant, `kInputPathGainToCheckPointOne = 4.70358`, whose Doxygen says it is a
**calibration residual against published check point [1], not a value derived from any component**,
and the discrepancy goes to Self-review. It matters rather than being cosmetic: the whole chain is
level-dependent through the compander, so getting `[2]` and `[3]` right requires the level arriving
at `[1]` to be right.

Levels enter the model in volts through `SushiDSP::kFullScaleVolts = 2.0` (a full-scale +/-1.0
sample is 2.0 V peak at the node's input), so the ladder's 100 mVp-p test signal is 0.05 V peak =
**0.025 full scale = -32.04 dBFS**, and Task 2.6's ladder test drives exactly that.

### N8. The 12-bit converter and the ring

One R-2R ladder, time-shared between the SAR and the DAC, 12 bits (dossier 1.9, confidence A).
Mid-tread, saturating (Ruling 15):

```
kConverterFullScaleVolts = 2.5            (the +5 V rail's half-swing; [3] 2.3 Vp-p = 1.15 V peak
                                           sits at 46.0 % of full scale, i.e. -6.74 dBFS)
kConverterLevels = 4096                   (12 bits)
step  = 2 * 2.5 / 4096 = 5.0/4096 = 1.220703125e-3 V   (1.2207 mV per code)
code(v)  = clamp(lround(v / step), -2048, 2047)
value(c) = c * step
ideal SNR of a full-scale sine = 6.0206 * 12 + 1.7609 = 72.2472 + 1.7609 = 74.008 dB
```

Task 2.3 asserts the code boundaries exactly (`0.5 * step` rounds to code 1, `0.49 * step` to code
0), the saturation points (`+10 V -> code 2047 -> 2.4988 V`, `-10 V -> code -2048 -> -2.5 V`), and
measures the quantisation SNR of a full-scale sine at **74.0 dB +/- 1.0 dB**.

The ring is `float`, allocated once at `kMaximumRingLength = 16384` in `prepare()` and used `N`-deep.
The read index is the write index taken **before** the write, so the delay is exactly `N` samples at
whatever rate the ring is clocked. HOLD skips the write and nothing else. A MODE change sets `N` and
zeroes the whole 16 384-sample buffer - 64 KB of `std::fill`, on the order of 2 us, paid once per
mode change and never in steady state; the header states that cost, and the RT trap in Task 2.7
covers it because `std::fill` on an already-sized vector allocates nothing.

---

### Task 0: Scaffolding (the controller does this inline, before dispatching Wave 1)

**Files:**
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`, `modules/node_registry/CMakeLists.txt`
- Create: `modules/dd3/CMakeLists.txt`
- Create (stubs): `tests/functional/unit/test_resampler_coefficients.cpp`, `test_resampler.cpp`,
  `test_dd3_analog_filters.cpp`, `test_ne570_compander.cpp`, `test_dd3_converter.cpp`,
  `test_dd3_clock.cpp`, `test_dd3_delay.cpp`, `test_dd3_acceptance.cpp`,
  `tests/functional/regression/test_dd3_regression.cpp`

- [ ] **Step 1: Add the resampler and the DD-3 module to the root `CMakeLists.txt`.** In
`add_library(sushi_dsp STATIC …)`, after `src/math/PartitionedConvolution.cpp`:

```cmake
    src/math/Resampler.cpp
```

and after `add_subdirectory(modules/routing)`:

```cmake
add_subdirectory(modules/dd3)
```

- [ ] **Step 2: Create `modules/dd3/CMakeLists.txt`,** copying `modules/tubescreamer/CMakeLists.txt`
line for line and changing only the names and the source list:

```cmake
add_library(dd3 STATIC
    src/Dd3AnalogFilters.cpp
    src/Ne570Compander.cpp
    src/Dd3Converter.cpp
    src/Dd3Clock.cpp
    src/Dd3Delay.cpp
)
add_library(SushiDSP::dd3 ALIAS dd3)
target_link_libraries(dd3 PUBLIC SushiDSP::sushi_dsp)
sushidsp_apply_optimization_flags(dd3)
target_include_directories(dd3 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
```

- [ ] **Step 3: Link `dd3` into the node registry.** In `modules/node_registry/CMakeLists.txt`, add
`SushiDSP::dd3` to the `target_link_libraries` list. Nothing else in that file changes; the registry
entry itself is Task 2.7.

- [ ] **Step 4: Register the nine test files in `tests/CMakeLists.txt`.** Under `# --- Math ---`,
after `functional/unit/test_sinc_resampler.cpp`:

```cmake
    functional/unit/test_resampler_coefficients.cpp
    functional/unit/test_resampler.cpp
```

Add a new group after `# --- Products ---`'s last entry:

```cmake
    # --- DD-3 ---
    functional/unit/test_dd3_analog_filters.cpp
    functional/unit/test_ne570_compander.cpp
    functional/unit/test_dd3_converter.cpp
    functional/unit/test_dd3_clock.cpp
    functional/unit/test_dd3_delay.cpp
    functional/unit/test_dd3_acceptance.cpp
```

and under `# --- Regression ---`:

```cmake
    functional/regression/test_dd3_regression.cpp
```

Add `SushiDSP::dd3` to `target_link_libraries(sushidsp_tests PRIVATE …)`, after
`SushiDSP::jcm800`. `SUSHIDSP_GOLDEN_DIR` and the `tests/` include root already exist; nothing else
in this file changes.

- [ ] **Step 5: Create the nine stubs**, each with the license header, the `@file`/`@brief`/`@author`
Doxygen block, and a single `#include <gtest/gtest.h>` so the binary links.

- [ ] **Step 6: `sd build` succeeds, `sd test -s unit` green. Commit**

```bash
git add CMakeLists.txt tests/CMakeLists.txt modules/dd3/CMakeLists.txt modules/node_registry/CMakeLists.txt tests/functional/unit/test_resampler_coefficients.cpp tests/functional/unit/test_resampler.cpp tests/functional/unit/test_dd3_analog_filters.cpp tests/functional/unit/test_ne570_compander.cpp tests/functional/unit/test_dd3_converter.cpp tests/functional/unit/test_dd3_clock.cpp tests/functional/unit/test_dd3_delay.cpp tests/functional/unit/test_dd3_acceptance.cpp tests/functional/regression/test_dd3_regression.cpp
git commit -m "DR1/DR2: scaffold the resampler and dd3 targets and their test units so no parallel task shares a CMake edit"
```

---

### Task 1.1: The resampler prototype filter and its generator

**Files:**
- Create: `tools/dsp/design_resampler.py`
- Create: `include/SushiDSP/math/ResamplerCoefficients.hpp` (generated, committed)
- Modify: `tools/dsp/README.md`
- Modify: `tests/functional/unit/test_resampler_coefficients.cpp` (replace stub)

**Interfaces (consumed by Task 1.3):**

```cpp
namespace SushiDSP
{
    inline constexpr std::uint32_t kResamplerHalfWidthZeroCrossings = 32u;
    inline constexpr std::uint32_t kResamplerPhasesPerZeroCrossing  = 512u;
    inline constexpr std::uint32_t kResamplerKernelLength           = 16386u;
    inline constexpr float         kResamplerCutoff                 = 0.45f;
    inline constexpr float         kResamplerPassbandEdge           = 0.400f;
    inline constexpr float         kResamplerStopbandDecibels       = /* measured, from the script */;
    inline constexpr float         kResamplerKernel[kResamplerKernelLength] = { … };
}
```

`kResamplerKernel[i]` is `h(i / 512)` in input samples, i.e. the **right wing** of the even kernel;
`kResamplerKernel[0] == 1.0f` exactly (the peak), and entries `16384` and `16385` are exactly `0.0f`.

- [ ] **Step 1: Write `tools/dsp/design_resampler.py`,** modelled on `design_halfband.py` — same
licence-header lift from `PortLayout.hpp`, same "GENERATED by … do not edit by hand" banner with the
scipy/numpy versions, same `emit(sys.stdout)` shape, same four-values-per-line formatting. The design
itself:

```python
HALF_WIDTH = 32          # zero crossings each side
PHASES     = 512         # table entries per zero crossing
CUTOFF     = 0.45        # normalised to the LOWER of the two sample rates
BETA       = 10.0613     # 0.1102 * (100 - 8.7), for a 100 dB stopband

def design():
    n = HALF_WIDTH * PHASES
    t = np.arange(n + 1) / PHASES                 # input samples, 0 .. HALF_WIDTH
    kernel = 2.0 * CUTOFF * np.sinc(2.0 * CUTOFF * t)
    kernel *= np.kaiser(2 * n + 1, BETA)[n:]      # right half of the window
    kernel /= mean_phase_sum(kernel)              # every polyphase branch has DC gain ~= 1
    return np.concatenate([kernel, [0.0]])        # the guard entry
```

Note `np.sinc(x)` is `sin(pi x)/(pi x)`, so `2*fc*np.sinc(2*fc*t)` is the ideal lowpass impulse
response with cutoff `fc` cycles per input sample. **Normalise on the polyphase DC sums, not on the
peak.** The quantity that must be 1 is every branch's DC gain, `sum_k h(phase/512 + k) +
sum_k h(1 - phase/512 + k)` over `k = 0 .. 31` — a branch whose sum is 0.999 shifts the level of a
constant by 0.009 dB and, worse, differs from its neighbour, which is amplitude modulation at the
phase-wrap rate. Poisson summation says the ideal infinite kernel gives exactly 1 for every phase
whenever the cutoff is below 0.5; truncation and windowing perturb that in the fifth decimal, so
`mean_phase_sum` divides by the mean over all 512 branches and the residual spread is *measured*, not
assumed. `kernel[0]` then comes out near 1.0 but is not exactly 1.0, and the test below allows that.

**Verification the script performs and prints to stderr** (the measured numbers go into the header's
comment, exactly as `design_halfband.py` prints its stopband and ripple):
1. Build the full symmetric kernel by mirroring, decimate it to the `phase = 0` polyphase branch
   (every 512th entry, i.e. the 64-tap unit-scale filter), `freqz` it on a 65536-point grid, and
   report the worst passband deviation over `[0, 0.400]` and the worst stopband level over
   `[0.500, 0.5]`… note the stopband band of the *interpolating* filter is measured on the
   **oversampled** kernel, not on one branch: `freqz(full_kernel, worN=65536, fs=PHASES)` and take
   the maximum over `f >= 0.5` in cycles per input sample. Print both.
2. Report the worst linear-interpolation error between adjacent table entries, as
   `max |h[i+1] - 2*h[i] + h[i-1]| / 8` — the second-difference bound from N1 — and check it is
   below `4.0e-6` (N1's `3.8127e-6` prediction plus headroom).

- [ ] **Step 2: Run the script and commit its output.**

```bash
python tools/dsp/design_resampler.py > include/SushiDSP/math/ResamplerCoefficients.hpp
```

(write the file without a BOM, exactly as `tools/dsp/README.md` already instructs for the halfband).
Paste the stderr line into the commit message.

- [ ] **Step 3: Write the tests** in `tests/functional/unit/test_resampler_coefficients.cpp`. These
check the **committed table**, not the script, so a hand-edited or truncated header fails here:

```cpp
/* LICENSE HEADER — copy from PortLayout.hpp, filename line changed */

#define _USE_MATH_DEFINES

#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/math/ResamplerCoefficients.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(Unit_Dsp, ResamplerKernelHasTheCommittedShape)
{
    // 32 zero crossings * 512 entries + 1 endpoint + 1 guard = 16386  (DR1+DR2 plan, N1).
    EXPECT_EQ(SushiDSP::kResamplerKernelLength, 16386u);
    EXPECT_EQ(SushiDSP::kResamplerHalfWidthZeroCrossings * SushiDSP::kResamplerPhasesPerZeroCrossing,
              16384u);
    // The peak is near 1 but not exactly 1: the generator normalises on the polyphase DC sums,
    // not on the peak  (DR1+DR2 plan, Task 1.1 Step 1).
    EXPECT_NEAR(SushiDSP::kResamplerKernel[0], 1.0f, 1e-3f);
    EXPECT_FLOAT_EQ(SushiDSP::kResamplerKernel[16384], 0.0f);
    EXPECT_FLOAT_EQ(SushiDSP::kResamplerKernel[16385], 0.0f);
    // A windowed sinc alternates in sign between zero crossings; the first sidelobe is negative.
    EXPECT_LT(SushiDSP::kResamplerKernel[768], 0.0f);   // 1.5 input samples out
    EXPECT_GT(SushiDSP::kResamplerKernel[1280], 0.0f);  // 2.5 input samples out
}

TEST(Unit_Dsp, ResamplerKernelSumsToOnePerPhase)
{
    // Every polyphase branch must have unity DC gain, or a rate conversion would change the
    // level of a constant. Branch p is h(p/512 + k) for integer k, over both wings.
    for (std::uint32_t phase = 0; phase < SushiDSP::kResamplerPhasesPerZeroCrossing; ++phase)
    {
        double sum = 0.0;
        for (std::uint32_t tap = 0; tap < SushiDSP::kResamplerHalfWidthZeroCrossings; ++tap)
        {
            const std::uint32_t right = tap * SushiDSP::kResamplerPhasesPerZeroCrossing + phase;
            const std::uint32_t left  = (tap + 1) * SushiDSP::kResamplerPhasesPerZeroCrossing - phase;
            sum += SushiDSP::kResamplerKernel[right];
            if (left < SushiDSP::kResamplerKernelLength)
            {
                sum += SushiDSP::kResamplerKernel[left];
            }
        }
        EXPECT_NEAR(sum, 1.0, 2.0e-4) << phase;
    }
}

TEST(Unit_Dsp, ResamplerKernelMeetsItsPassbandAndStopbandTargets)
{
    // The full symmetric kernel, evaluated as a filter running at 512x the input rate. Its
    // passband is |f| <= 0.400 input-rate cycles/sample and its stopband |f| >= 0.500.
    // DFT directly at the frequencies of interest - 32769 taps is too many for a naive FFT here,
    // and only a handful of frequencies matter.
    auto magnitude_db = [](double frequency_cycles_per_input_sample)
    {
        std::complex<double> sum(SushiDSP::kResamplerKernel[0], 0.0);
        for (std::uint32_t i = 1; i < 16385u; ++i)
        {
            const double t = static_cast<double>(i) / SushiDSP::kResamplerPhasesPerZeroCrossing;
            const double phase = 2.0 * M_PI * frequency_cycles_per_input_sample * t;
            sum += 2.0 * SushiDSP::kResamplerKernel[i] * std::complex<double>(std::cos(phase), 0.0);
        }
        // The kernel is even and real, so the transform is real; the 1/512 factor turns the sum
        // into the continuous-time integral the interpolating filter actually applies.
        const double gain = std::abs(sum) / SushiDSP::kResamplerPhasesPerZeroCrossing;
        return 20.0 * std::log10(std::max(gain, 1e-300));
    };

    for (const double f : { 0.0, 0.05, 0.10, 0.20, 0.30, 0.35, 0.399 })
    {
        EXPECT_NEAR(magnitude_db(f), 0.0, 0.10) << f;
    }
    for (const double f : { 0.50, 0.60, 0.80, 1.00, 1.50, 2.00, 4.00 })
    {
        EXPECT_LE(magnitude_db(f), -100.0) << f;
    }
}

TEST(Unit_Dsp, ResamplerKernelIsSmoothEnoughForLinearInterpolation)
{
    // N1: the linear-interpolation error is bounded by |h''| * step^2 / 8, and the committed
    // table's second difference already carries the step^2 factor. The predicted bound is
    // 3.8127e-6 (-108.4 dB); assert it against 4.0e-6 so a table designed at half the density
    // fails here rather than quietly setting the noise floor.
    double worst = 0.0;
    for (std::uint32_t i = 1; i + 1 < 16385u; ++i)
    {
        const double second_difference = static_cast<double>(SushiDSP::kResamplerKernel[i + 1])
                                       - 2.0 * SushiDSP::kResamplerKernel[i]
                                       + SushiDSP::kResamplerKernel[i - 1];
        worst = std::max(worst, std::abs(second_difference) / 8.0);
    }
    std::printf("[ measure ] resampler kernel linear-interpolation bound %.4e (%.2f dB)\n",
                worst, 20.0 * std::log10(worst));
    EXPECT_LE(worst, 4.0e-6);
}
```

- [ ] **Step 4: Update `tools/dsp/README.md`** with a `design_resampler.py` bullet in the same shape
as the halfband one: what it regenerates, what it needs, the exact command, and where the design
targets live (this plan's N1, and the spec's "New shared brick" section).

- [ ] **Step 5: `sd build && sd test -s unit`** — expected: all four cases PASS. **Commit**

```bash
git add tools/dsp/design_resampler.py tools/dsp/README.md include/SushiDSP/math/ResamplerCoefficients.hpp tests/functional/unit/test_resampler_coefficients.cpp
git commit -m "DR1: committed windowed-sinc resampler prototype (32 zero crossings, 512 phases, Kaiser beta 10.0613, <measured> dB stopband) and its generator"
```

---

### Task 1.2: `Biquad::set_first_order_shelf`

**Files:**
- Modify: `include/SushiDSP/math/Biquad.hpp`
- Modify: `src/math/Biquad.cpp`
- Modify: `tests/functional/unit/test_biquad.cpp`

**Interface (consumed by Task 2.1):**

```cpp
/// @brief Configures this filter as a first-order shelf with independent zero and pole.
///
/// The shape of an op-amp stage whose input or feedback leg carries a series R+C: the gain is
/// low_frequency_gain below the zero, low_frequency_gain * pole_hz / zero_hz above the pole, and
/// the transition is first order. Both corners are prewarped separately, so each lands exactly
/// where the component values put it and a mirrored pair (zero and pole swapped, gain inverted)
/// cancels to the last bit. See DR1+DR2 plan N4.
/// @param sample_rate Sample rate in Hz.
/// @param zero_hz Frequency of the transfer function's zero, in Hz.
/// @param pole_hz Frequency of its pole, in Hz.
/// @param low_frequency_gain Gain below both corners; 1.0 is unity.
void set_first_order_shelf(double sample_rate, double zero_hz, double pole_hz,
                           double low_frequency_gain);
```

- [ ] **Step 1: Write the failing tests** in `tests/functional/unit/test_biquad.cpp`, appended after
the existing cases. The expected values are N4's, computed at double precision while writing this
plan:

```cpp
namespace
{
    /// @brief Magnitude response of a configured Biquad at one frequency, by driving it with a
    /// long sine and reading the settled peak.
    /// @param filter The filter, already configured; reset before use.
    /// @param frequency_hz Probe frequency.
    /// @param sample_rate Sample rate the filter was configured at.
    /// @return Peak output amplitude for a unit-amplitude input.
    double biquad_magnitude(SushiDSP::Biquad& filter, double frequency_hz, double sample_rate)
    {
        filter.reset();
        const std::size_t settle = 200000;
        const std::size_t measure = static_cast<std::size_t>(std::round(20.0 * sample_rate / frequency_hz));
        double peak = 0.0;
        for (std::size_t n = 0; n < settle + measure; ++n)
        {
            const float y = filter.process_sample(
                static_cast<float>(std::sin(2.0 * M_PI * frequency_hz * static_cast<double>(n) / sample_rate)));
            if (n >= settle)
            {
                peak = std::max(peak, std::abs(static_cast<double>(y)));
            }
        }
        return peak;
    }
}

TEST(Unit_Dsp, BiquadFirstOrderShelfPlacesItsCornersAndAsymptotesExactly)
{
    // DD-3 pre-emphasis (DR1+DR2 plan N4): zero 594.07 Hz, pole 3386.28 Hz, LF gain 33/47.
    //   LF gain  = 0.7021277                     (-3.0745 dB)
    //   HF gain  = 0.7021277 * (3386.28/594.07)  = 0.7021277 * 5.700063 = 4.0021712
    //   at 1 kHz = 0.7021277 * sqrt((1+(1000/594.07)^2)/(1+(1000/3386.28)^2))
    //            = 0.7021277 * 1.8777630 = 1.3183700
    // The discretised HF asymptote is tan-warped and is NOT 4.0021712 at Nyquist; 12 kHz is far
    // enough below Nyquist that the warping is under 1 %, which is what the tolerance allows.
    SushiDSP::Biquad shelf;
    shelf.set_first_order_shelf(48000.0, 594.07, 3386.28, 33.0 / 47.0);
    EXPECT_NEAR(biquad_magnitude(shelf, 20.0, 48000.0), 0.7021277, 2e-3);
    EXPECT_NEAR(biquad_magnitude(shelf, 1000.0, 48000.0), 1.3183700, 3e-3);
    EXPECT_NEAR(biquad_magnitude(shelf, 12000.0, 48000.0), 4.0021712, 4.0e-2);
}

TEST(Unit_Dsp, BiquadFirstOrderShelfPairCancelsExactly)
{
    // N4: pre-emphasis (zero 594.07, pole 3386.28, gain 33/47) followed by de-emphasis (the same
    // two corners swapped, gain 47/33) is the identity, because the numerator polynomial of one
    // is the denominator polynomial of the other and the scalars multiply to 1. Assert a null
    // below -120 dB on a broadband signal, which no tolerance-based response check would catch.
    SushiDSP::Biquad pre, de;
    pre.set_first_order_shelf(48000.0, 594.07, 3386.28, 33.0 / 47.0);
    de.set_first_order_shelf(48000.0, 3386.28, 594.07, 47.0 / 33.0);
    double signal_power = 0.0;
    double error_power = 0.0;
    std::uint32_t state = 22695477u;
    for (std::size_t n = 0; n < 200000; ++n)
    {
        state = state * 1103515245u + 12345u;
        const float input = 0.5f * (static_cast<float>(state >> 16) / 32768.0f - 1.0f);
        const float output = de.process_sample(pre.process_sample(input));
        if (n >= 4096)
        {
            signal_power += static_cast<double>(input) * input;
            error_power += static_cast<double>(output - input) * (output - input);
        }
    }
    EXPECT_LE(10.0 * std::log10(error_power / signal_power), -120.0);
}

TEST(Unit_Dsp, BiquadFirstOrderShelfWithEqualCornersIsAFlatGain)
{
    // A degenerate but reachable configuration: zero == pole cancels, leaving a plain gain.
    SushiDSP::Biquad shelf;
    shelf.set_first_order_shelf(48000.0, 1000.0, 1000.0, 2.0);
    for (const double frequency : { 30.0, 300.0, 3000.0, 15000.0 })
    {
        EXPECT_NEAR(biquad_magnitude(shelf, frequency, 48000.0), 2.0, 1e-3) << frequency;
    }
}
```

- [ ] **Step 2: `sd build` — expected FAIL (no such member).**

- [ ] **Step 3: Implement `set_first_order_shelf` in `src/math/Biquad.cpp`,** transcribing N4's
derivation line for line, with `b2_` and `a2_` set to exactly zero:

```cpp
void Biquad::set_first_order_shelf(double sample_rate, double zero_hz, double pole_hz,
                                   double low_frequency_gain)
{
    // t = tan(pi*f/fs) prewarps each corner separately, so both land exactly where the component
    // values put them  (DR1+DR2 plan, N4).
    const double zero_tangent = std::tan(M_PI * zero_hz / sample_rate);
    const double pole_tangent = std::tan(M_PI * pole_hz / sample_rate);
    const double scale = low_frequency_gain * pole_tangent / zero_tangent;
    const double denominator = pole_tangent + 1.0;
    b0_ = static_cast<float>(scale * (zero_tangent + 1.0) / denominator);
    b1_ = static_cast<float>(scale * (zero_tangent - 1.0) / denominator);
    b2_ = 0.0f;
    a1_ = static_cast<float>((pole_tangent - 1.0) / denominator);
    a2_ = 0.0f;
}
```

`_USE_MATH_DEFINES` and the `M_PI` fallback are already at the top of `Biquad.cpp`; confirm rather
than re-adding them.

- [ ] **Step 4: `sd test -s unit` green — the three new cases and every existing `Biquad` case.
Commit**

```bash
git add include/SushiDSP/math/Biquad.hpp src/math/Biquad.cpp tests/functional/unit/test_biquad.cpp
git commit -m "DR2: Biquad first-order shelf with independently prewarped zero and pole, for the DD-3 emphasis pair"
```

---

### Task 1.3: `math/Resampler`

**Files:**
- Create: `include/SushiDSP/math/Resampler.hpp`
- Create: `src/math/Resampler.cpp`
- Modify: `tests/functional/unit/test_resampler.cpp` (replace stub)

**Depends on:** Task 1.1.

**Not in this task:** the RT-safety trap and the benchmark case. Both edit files that Task 2.7 also
edits (`tests/functional/unit/test_rt_safety.cpp`, `tests/benchmark/main.cpp`), and no two tasks in
this plan share a file, so they land in Task 2.7 alongside the DD-3's.

**Interface (consumed by Task 2.5 and by DR3):**

```cpp
namespace SushiDSP
{
    /// @brief Arbitrary-ratio polyphase resampler over one committed prototype filter.
    ///
    /// A single fractional phase accumulator walks the kernel of ResamplerCoefficients.hpp; a
    /// ratio change moves the accumulator's step and the kernel's time scale, and never rebuilds
    /// a filter bank. Every buffer is sized in prepare(); process() and set_rates() are
    /// allocation-, lock-, syscall- and throw-free.
    class Resampler
    {
    public:
        /// @brief Largest target/source ratio set_rates() will honour.
        static constexpr double kMaximumRatio = 16.0;

        /// @brief Sizes every buffer for the worst case this instance will be asked for.
        /// @param source_rate Initial input rate in Hz.
        /// @param target_rate Initial output rate in Hz.
        /// @param max_input_frames Largest input_frames a single process() call will carry.
        /// @param minimum_ratio Smallest target/source this instance will ever be set to; the
        /// kernel's span grows as 1/ratio when decimating, so this sizes the history.
        void prepare(double source_rate, double target_rate, std::uint32_t max_input_frames,
                     double minimum_ratio);

        /// @brief Changes the conversion rates. Real-time safe; allocates nothing.
        /// @param source_rate Input rate in Hz.
        /// @param target_rate Output rate in Hz.
        /// @pre prepare() has been called; the ratio is clamped to
        /// [minimum_ratio, kMaximumRatio].
        void set_rates(double source_rate, double target_rate);

        /// @brief Converts one push of input, producing however many outputs are now available.
        /// @param input input_frames source-rate samples.
        /// @param input_frames Number of input samples; must not exceed max_input_frames.
        /// @param output Destination, at least output_capacity samples; must not alias input.
        /// @param output_capacity Room available; use maximum_output_frames() to size it.
        /// @return Number of output samples written.
        std::uint32_t process(const float* input, std::uint32_t input_frames,
                              float* output, std::uint32_t output_capacity);

        /// @brief Upper bound on what process() can return for that many input frames.
        /// @param input_frames The push size.
        /// @return Output samples to make room for, at the current rates.
        std::uint32_t maximum_output_frames(std::uint32_t input_frames) const;

        /// @brief Algorithmic delay: an output can only be formed once the input half a kernel
        /// span past it has arrived. Equals 32 / min(source_rate, target_rate) seconds at every
        /// ratio (DR1+DR2 plan, N1).
        /// @return Delay in seconds.
        double latency_seconds() const;

        /// @brief The same delay counted in source-rate samples, rounded to nearest.
        /// @return Delay in input samples.
        std::uint32_t latency_samples() const;

        /// @brief Zeroes the history and the phase, so the next process() starts from silence.
        void reset();
    };
}
```

**The algorithm, which the implementer transcribes rather than invents.** All of it in `double`
except the history and the kernel.

```
scale_ = min(1.0, ratio)             ratio = target_rate / source_rate
step_  = 1.0 / ratio                 input samples advanced per output sample
half_span_    = kResamplerHalfWidthZeroCrossings / scale_        (input samples)
taps_per_side_= ceil(half_span_)

kernel_at(u):                        u in [0, kResamplerHalfWidthZeroCrossings)
    x = u * kResamplerPhasesPerZeroCrossing
    i = min((uint32)x, 16384u)
    f = x - (double)i
    return kernel[i] + f * (kernel[i + 1] - kernel[i])

interpolate(t):                      t = output time, in this call's input-sample coordinates
    left = floor(t);  frac = t - left
    sum = 0
    for k in [0, taps_per_side_):    // taps at and below t
        distance = (frac + k) * scale_
        if distance >= kResamplerHalfWidthZeroCrossings: break
        sum += history(left - k) * kernel_at(distance)
    for k in [0, taps_per_side_):    // taps above t
        distance = ((1 - frac) + k) * scale_
        if distance >= kResamplerHalfWidthZeroCrossings: break
        sum += history(left + 1 + k) * kernel_at(distance)
    return sum * scale_              // the scale keeps the DC gain at 1 when decimating

process(input, input_frames, output, capacity):
    append input to the history ring        // write_index_ advances by input_frames
    produced = 0
    while position_ + half_span_ <= (double)input_frames - 1.0 and produced < capacity:
        output[produced++] = (float)interpolate(position_)
        position_ += step_
    position_ -= (double)input_frames        // rebase into the next call's coordinates
    return produced
```

`position_` is the time of the next output in **this call's** coordinates, where input sample `j` of
this call sits at time `j`. Rebasing it by `input_frames` at the end keeps it bounded in roughly
`[-half_span_ - step_, 0]` forever, so no accumulator drifts and no `double` loses precision on a
long stream. `history(m)` for negative `m` reaches into the previous calls' samples: with
`write_index_` already advanced, the ring index is `(write_index_ - input_frames + m) & history_mask_`.

**Sizing in `prepare()`:** `history_length_` is the next power of two at or above
`2 * ceil(kResamplerHalfWidthZeroCrossings / minimum_ratio) + max_input_frames + 4`. For the DD-3's
first conversion (`48 kHz → 20…82.5 kHz`, `minimum_ratio = 20000/48000 = 0.4166667`) that is
`2 * 77 + 256 + 4 = 414 → 512` samples. `maximum_output_frames(n)` is
`(uint32)ceil(n * ratio_) + 2`.

`set_rates` clamps the ratio into `[minimum_ratio_, kMaximumRatio]` and recomputes `scale_`, `step_`,
`half_span_` and `taps_per_side_`. It must **not** touch `position_` or the history: a mid-stream
rate change is exactly the DD-3's pitch glide, and resetting the phase there would be the click the
whole design exists to avoid.

- [ ] **Step 1: Write the failing tests** in `tests/functional/unit/test_resampler.cpp`.

```cpp
/* LICENSE HEADER — copy from PortLayout.hpp, filename line changed */

#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/math/Resampler.hpp"
#include "measure/NullTest.hpp"
#include "measure/Spectrum.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    /// @brief Streams a signal through a resampler in fixed-size pushes.
    /// @param resampler A prepared resampler.
    /// @param input The signal at the resampler's source rate.
    /// @param push Frames per process() call.
    /// @return Everything the resampler produced, in order.
    std::vector<float> stream(SushiDSP::Resampler& resampler, const std::vector<float>& input,
                              std::uint32_t push)
    {
        std::vector<float> output;
        std::vector<float> scratch(resampler.maximum_output_frames(push) + 8u, 0.0f);
        for (std::size_t position = 0; position < input.size(); position += push)
        {
            const std::uint32_t frames =
                static_cast<std::uint32_t>(std::min<std::size_t>(push, input.size() - position));
            const std::uint32_t produced = resampler.process(
                input.data() + position, frames, scratch.data(),
                static_cast<std::uint32_t>(scratch.size()));
            output.insert(output.end(), scratch.begin(), scratch.begin() + produced);
        }
        return output;
    }

    /// @brief Sum of unit-amplitude sines, scaled so the peak stays inside +/-1.
    /// @param frequencies_hz The partials.
    /// @param sample_rate Sample rate.
    /// @param length Samples to generate.
    /// @return The signal.
    std::vector<float> multitone(const std::vector<double>& frequencies_hz, double sample_rate,
                                 std::size_t length)
    {
        std::vector<float> signal(length, 0.0f);
        const double amplitude = 0.8 / static_cast<double>(frequencies_hz.size());
        for (std::size_t n = 0; n < length; ++n)
        {
            double value = 0.0;
            for (const double frequency : frequencies_hz)
            {
                value += amplitude * std::sin(2.0 * M_PI * frequency * static_cast<double>(n) / sample_rate);
            }
            signal[n] = static_cast<float>(value);
        }
        return signal;
    }
}

TEST(Unit_Dsp, ResamplerHoldsADcLevelAtEveryRatio)
{
    // Every polyphase branch has unity DC gain and the decimating scale factor restores it, so a
    // constant comes back as the same constant whichever way the rate moves.
    for (const double target : { 20000.0, 31250.0, 48000.0, 82500.0, 96000.0 })
    {
        SushiDSP::Resampler resampler;
        resampler.prepare(48000.0, target, 256u, 20000.0 / 48000.0);
        const std::vector<float> ones(48000u, 1.0f);
        const std::vector<float> output = stream(resampler, ones, 256u);
        ASSERT_GT(output.size(), 4000u) << target;
        for (std::size_t n = 2000; n < output.size() - 200; ++n)
        {
            ASSERT_NEAR(output[n], 1.0f, 1e-3f) << target << " " << n;
        }
    }
}

TEST(Unit_Dsp, ResamplerRoundTripNullsAtTheDd3SlowClock)
{
    // 48 kHz -> 20 kHz -> 48 kHz. The content stops at 7 kHz, which is the pedal's own delay-path
    // limit and is inside the prototype's passband (0.400 * 20000 = 8000 Hz, N1), so the round
    // trip is limited by the kernel's own error: -100 dB stopband, -108 dB interpolation floor.
    // -80 dB is asserted with margin for the float buffers.
    const std::vector<float> input = multitone({ 100.0, 1000.0, 3000.0, 7000.0 }, 48000.0, 96000u);
    SushiDSP::Resampler down, up;
    down.prepare(48000.0, 20000.0, 256u, 20000.0 / 48000.0);
    up.prepare(20000.0, 48000.0, 256u, 20000.0 / 82500.0);
    const std::vector<float> narrow = stream(down, input, 256u);
    const std::vector<float> wide = stream(up, narrow, 256u);
    EXPECT_LE(SushiDSP::measure::null_test(input, wide, 512u), -80.0);
}

TEST(Unit_Dsp, ResamplerRoundTripNullsAtTheDd3FastClockAndTheSpx90Ratio)
{
    // 48 kHz -> 82.5 kHz -> 48 kHz, and 48 kHz -> 31.25 kHz -> 48 kHz (the SPX90's 1.536).
    // At the SPX90 ratio the passband reaches 0.400 * 31250 = 12500 Hz, which is exactly the
    // unit's published 20 Hz - 12 kHz band, so 12 kHz content must survive.
    struct Case { double middle_rate; std::vector<double> partials; };
    const Case cases[2] = {
        { 82500.0, { 100.0, 1000.0, 7000.0, 15000.0 } },
        { 31250.0, { 100.0, 1000.0, 7000.0, 12000.0 } },
    };
    for (const Case& item : cases)
    {
        const std::vector<float> input = multitone(item.partials, 48000.0, 96000u);
        SushiDSP::Resampler down, up;
        down.prepare(48000.0, item.middle_rate, 256u, 31250.0 / 48000.0);
        up.prepare(item.middle_rate, 48000.0, 512u, 48000.0 / 82500.0);
        const std::vector<float> middle = stream(down, input, 256u);
        const std::vector<float> back = stream(up, middle, 256u);
        EXPECT_LE(SushiDSP::measure::null_test(input, back, 512u), -80.0) << item.middle_rate;
    }
}

TEST(Unit_Dsp, ResamplerRejectsAToneAboveTheOutputNyquist)
{
    // 15 kHz into a 20 kHz output rate would fold to |15000 - 20000| = 5000 Hz. Normalised to
    // the lower rate, 15 kHz is 0.75 cycles/sample, deep in the prototype's stopband, so the
    // image must land below -90 dB relative to the input tone.
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 20000.0, 256u, 20000.0 / 48000.0);
    std::vector<float> input(200000u, 0.0f);
    for (std::size_t n = 0; n < input.size(); ++n)
    {
        input[n] = 0.8f * static_cast<float>(std::sin(2.0 * M_PI * 15000.0 * static_cast<double>(n) / 48000.0));
    }
    const std::vector<float> output = stream(resampler, input, 256u);
    ASSERT_GT(output.size(), 2u * SushiDSP::measure::kFftSize);
    const auto spectrum = SushiDSP::measure::magnitude_spectrum_db(
        output, output.size() - SushiDSP::measure::kFftSize);
    const double image = SushiDSP::measure::peak_near(spectrum, SushiDSP::measure::bin_of(5000.0, 20000.0));
    // 0 dB reference: what the same tone would read if it had passed unattenuated. Use the input
    // itself, analysed at its own rate, so the two spectra share a window and a length.
    const auto reference_spectrum = SushiDSP::measure::magnitude_spectrum_db(
        input, input.size() - SushiDSP::measure::kFftSize);
    const double reference = SushiDSP::measure::peak_near(
        reference_spectrum, SushiDSP::measure::bin_of(15000.0, 48000.0));
    std::printf("[ measure ] resampler 15 kHz -> 20 kHz image at 5 kHz: %.2f dB\n", image - reference);
    EXPECT_LE(image - reference, -90.0);
}

TEST(Unit_Dsp, ResamplerOutputCountFollowsTheRatioWithoutDrift)
{
    // Over a million frames the accumulated output count must stay within a sample or two of
    // input_frames * ratio; a phase accumulator that rebases badly drifts here and nowhere else.
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 20000.0, 256u, 20000.0 / 48000.0);
    const std::vector<float> silence(256u, 0.0f);
    std::vector<float> scratch(resampler.maximum_output_frames(256u) + 8u, 0.0f);
    std::uint64_t produced = 0;
    for (std::uint32_t block = 0; block < 4000u; ++block)
    {
        produced += resampler.process(silence.data(), 256u, scratch.data(),
                                      static_cast<std::uint32_t>(scratch.size()));
    }
    // 4000 * 256 = 1 024 000 input frames; 1 024 000 * 20000/48000 = 426 666.67 outputs.
    EXPECT_NEAR(static_cast<double>(produced), 426666.67, 80.0);
}

TEST(Unit_Dsp, ResamplerLatencyIsThirtyTwoSamplesOfTheLowerRate)
{
    // DR1+DR2 plan N1: the delay is 32 / min(source, target) seconds at every ratio.
    struct Case { double source; double target; double expected_seconds; std::uint32_t expected_samples; };
    const Case cases[6] = {
        { 48000.0, 48000.0, 32.0 / 48000.0, 32u },
        { 48000.0, 82500.0, 32.0 / 48000.0, 32u },
        { 82500.0, 48000.0, 32.0 / 48000.0, 55u },   // 32 / (48000/82500) = 55.0 source samples
        { 48000.0, 20000.0, 32.0 / 20000.0, 77u },   // 32 / (20000/48000) = 76.8 -> 77
        { 20000.0, 48000.0, 32.0 / 20000.0, 32u },
        { 48000.0, 31250.0, 32.0 / 31250.0, 49u },   // 32 / (31250/48000) = 49.152 -> 49
    };
    for (const Case& item : cases)
    {
        SushiDSP::Resampler resampler;
        resampler.prepare(item.source, item.target, 256u, 20000.0 / 48000.0);
        EXPECT_NEAR(resampler.latency_seconds(), item.expected_seconds, 1e-12) << item.target;
        EXPECT_EQ(resampler.latency_samples(), item.expected_samples) << item.target;
    }
}

TEST(Unit_Dsp, ResamplerKeepsAToneAtItsFrequency)
{
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 20000.0, 256u, 20000.0 / 48000.0);
    const double frequency = SushiDSP::measure::bin_exact_frequency(1000.0, 20000.0, SushiDSP::measure::kFftSize);
    std::vector<float> input(200000u, 0.0f);
    for (std::size_t n = 0; n < input.size(); ++n)
    {
        input[n] = 0.5f * static_cast<float>(std::sin(2.0 * M_PI * frequency * static_cast<double>(n) / 48000.0));
    }
    const std::vector<float> output = stream(resampler, input, 256u);
    const auto spectrum = SushiDSP::measure::magnitude_spectrum_db(
        output, output.size() - SushiDSP::measure::kFftSize);
    const std::uint32_t expected_bin = SushiDSP::measure::bin_of(frequency, 20000.0);
    double best_bin = 0.0;
    double best_level = -1e300;
    for (std::uint32_t bin = 1; bin + 1 < spectrum.size(); ++bin)
    {
        if (spectrum[bin] > best_level)
        {
            best_level = spectrum[bin];
            best_bin = bin;
        }
    }
    EXPECT_NEAR(best_bin, static_cast<double>(expected_bin), 1.0);
}

TEST(Unit_Dsp, ResamplerRateChangeMidStreamIsContinuous)
{
    // The DD-3 usage: set_rates() every 8 frames while the target sweeps 82.5 kHz -> 20 kHz.
    // A phase reset or a bank rebuild shows up here as a step; a correct accumulator does not.
    // Reference: a 100 Hz unit sine sampled at 20 kHz moves at most 2*pi*100/20000 = 0.0314 per
    // sample, so 0.05 leaves room for the sweep's own rate change and none for a click.
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 82500.0, 8u, 20000.0 / 48000.0);
    std::vector<float> scratch(resampler.maximum_output_frames(8u) + 8u, 0.0f);
    std::vector<float> output;
    float previous = 0.0f;
    for (std::uint32_t sub_block = 0; sub_block < 2000u; ++sub_block)
    {
        const double position = static_cast<double>(sub_block) / 2000.0;
        resampler.set_rates(48000.0, 82500.0 * std::pow(4.125, -position));
        float input[8];
        for (std::uint32_t j = 0; j < 8u; ++j)
        {
            const std::size_t n = sub_block * 8u + j;
            input[j] = static_cast<float>(std::sin(2.0 * M_PI * 100.0 * static_cast<double>(n) / 48000.0));
        }
        const std::uint32_t produced =
            resampler.process(input, 8u, scratch.data(), static_cast<std::uint32_t>(scratch.size()));
        for (std::uint32_t j = 0; j < produced; ++j)
        {
            if (!output.empty())
            {
                ASSERT_LE(std::abs(scratch[j] - previous), 0.05f) << sub_block << " " << j;
            }
            previous = scratch[j];
            output.push_back(scratch[j]);
        }
    }
    EXPECT_GT(output.size(), 10000u);
}

TEST(Unit_Dsp, ResamplerResetReturnsItToSilence)
{
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 20000.0, 256u, 20000.0 / 48000.0);
    const std::vector<float> loud(4096u, 0.9f);
    stream(resampler, loud, 256u);
    resampler.reset();
    const std::vector<float> silence(4096u, 0.0f);
    for (const float sample : stream(resampler, silence, 256u))
    {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}
```

- [ ] **Step 2: `sd build` — expected: FAIL to compile (`Resampler.hpp` missing).**

- [ ] **Step 3: Write `include/SushiDSP/math/Resampler.hpp`** — the interface above, verbatim, with
private members `history_` (`std::vector<float>`), `history_mask_`, `write_index_`, `position_`,
`step_`, `scale_`, `half_span_`, `taps_per_side_`, `ratio_`, `minimum_ratio_`, `source_rate_`,
`target_rate_`, `max_input_frames_`. Nothing is `static`; two instances of the class share nothing.

- [ ] **Step 4: Write `src/math/Resampler.cpp`,** transcribing the algorithm block above line for
line, each line carrying the plan's symbol name in a comment. Three details the implementer must not
improvise:
  - The kernel index is clamped to `16384` before the `+1` read, which is what the table's guard
    entry exists for; state that in a one-line comment.
  - The two tap loops break on `distance >= kResamplerHalfWidthZeroCrossings` rather than relying on
    `taps_per_side_` alone, because `taps_per_side_` is a ceiling and the last tap of a loop can sit
    exactly on the kernel's far zero.
  - `process()` returns as soon as `produced == output_capacity`, leaving `position_` where it is;
    the caller has under-sized its buffer and the next call resumes correctly rather than dropping
    audio silently.

- [ ] **Step 5: `sd build && sd test -s unit`** — expected: all nine cases PASS. Copy the two
`[ measure ]` lines into the commit message.

- [ ] **Step 6: Commit**

```bash
git add include/SushiDSP/math/Resampler.hpp src/math/Resampler.cpp tests/functional/unit/test_resampler.cpp
git commit -m "DR1: arbitrary-ratio polyphase Resampler over the committed prototype; round trip nulls <x> dB at 20 k/31.25 k/82.5 k, group delay 32 samples of the lower rate"
```

---

### Task 2.1: `Dd3AnalogFilters` — the four linear networks

**Files:**
- Create: `modules/dd3/include/SushiDSP/dd3/Dd3AnalogFilters.hpp`
- Create: `modules/dd3/src/Dd3AnalogFilters.cpp`
- Modify: `tests/functional/unit/test_dd3_analog_filters.cpp` (replace stub)

**Depends on:** Task 1.2 (`Biquad::set_first_order_shelf`).

**Four sibling classes in one header**, because they share the component-value constants and nothing
else; each has one job and the same three-method shape, so any of them can be replaced without
touching its neighbours:

```cpp
namespace SushiDSP::dd3
{
    // Component values, verbatim from the DD-2/DD-3 service notes as transcribed in
    // docs/reference/research/2026-08-26-boss-dd3-design-dossier.md 1.2-1.4.
    inline constexpr double kSallenKeyResistanceOhms = 10000.0;

    inline constexpr double kAntiAliasSectionFrequencies[3] = { 4467.73, 8902.32, 7997.84 };
    inline constexpr double kAntiAliasSectionQ[3]           = { 0.6596857, 1.9018468, 4.5226702 };
    inline constexpr double kReconstructionSectionFrequencies[2] = { 8842.18, 15758.51 };
    inline constexpr double kReconstructionSectionQ[2]           = { 0.75, 3.3665016 };

    inline constexpr double kEmphasisZeroHz        = 594.07;    // 1/(2*pi*(R7+R8)*C4)
    inline constexpr double kEmphasisPoleHz        = 3386.28;   // 1/(2*pi*R8*C4)
    inline constexpr double kPreEmphasisLowGain    = 33.0 / 47.0;   // R12/R7
    inline constexpr double kDeEmphasisLowGain     = 47.0 / 33.0;   // R16/R14

    class Dd3PreEmphasis  { public: void prepare(double sample_rate); void reset();
                                   float process_sample(float input); /* one Biquad */ };
    class Dd3DeEmphasis   { /* the same, corners swapped, gain inverted */ };
    class Dd3AntiAliasFilter      { /* three Biquad low-passes */ };
    class Dd3ReconstructionFilter { /* two Biquad low-passes */ };
}
```

Every `f0`/`Q` above is N3's exact figure, not the dossier's rounded one. Both classes' Doxygen names
the section table it came from, and `Dd3PreEmphasis`'s says why `C5 33 pF` and `C12 33 pF` are absent
— their poles are at 146.149 kHz and 102.598 kHz, above Nyquist at every rate this node runs at (N4).

- [ ] **Step 1: Write the failing tests.** The magnitudes are N3's, computed at double precision
while writing this plan.

```cpp
namespace
{
    /// @brief Peak output of a chain driven by a unit sine, after it has settled.
    /// @param process Callable taking one float and returning one float.
    /// @param frequency_hz Probe frequency.
    /// @param sample_rate Sample rate the chain was prepared at.
    /// @return Peak amplitude, i.e. |H| for a unit input.
    template <typename Process>
    double chain_magnitude(Process&& process, double frequency_hz, double sample_rate)
    {
        const std::size_t settle = static_cast<std::size_t>(0.5 * sample_rate);
        const std::size_t measure = static_cast<std::size_t>(std::round(20.0 * sample_rate / frequency_hz));
        double peak = 0.0;
        for (std::size_t n = 0; n < settle + measure; ++n)
        {
            const float y = process(static_cast<float>(
                std::sin(2.0 * M_PI * frequency_hz * static_cast<double>(n) / sample_rate)));
            if (n >= settle) { peak = std::max(peak, std::abs(static_cast<double>(y))); }
        }
        return peak;
    }
}

TEST(Unit_Dsp, Dd3SectionConstantsMatchTheComponentValues)
{
    // f0 = 1/(2*pi*R*sqrt(C1*C2)), Q = 0.5*sqrt(C1/C2)  (DR1+DR2 plan, N3).
    // AA 1: C9 4700p / C8 2700p -> sqrt(C1*C2) = 3.562303e-9 -> f0 4467.73, Q 0.6596857
    // AA 2: C15 6800p / C14 470p -> 1.787736e-9 -> 8902.32, Q 1.9018468
    // AA 3: C17 0.018u / C16 220p -> 1.989975e-9 -> 7997.84, Q 4.5226702
    // RC 1: C38 2700p / C37 1200p -> 1.8e-9 exactly -> 8842.18, Q 0.75 exactly
    // RC 2: C35 6800p / C36 150p -> 1.0099505e-9 -> 15758.51, Q 3.3665016
    struct Section { double c1; double c2; double f0; double q; };
    const Section sections[5] = {
        { 4.7e-9,  2.7e-9,  4467.73,  0.6596857 },
        { 6.8e-9,  4.7e-10, 8902.32,  1.9018468 },
        { 1.8e-8,  2.2e-10, 7997.84,  4.5226702 },
        { 2.7e-9,  1.2e-9,  8842.18,  0.75      },
        { 6.8e-9,  1.5e-10, 15758.51, 3.3665016 },
    };
    for (const Section& section : sections)
    {
        const double f0 = 1.0 / (2.0 * M_PI * SushiDSP::dd3::kSallenKeyResistanceOhms
                                 * std::sqrt(section.c1 * section.c2));
        const double q = 0.5 * std::sqrt(section.c1 / section.c2);
        EXPECT_NEAR(f0, section.f0, 0.01) << section.c1;
        EXPECT_NEAR(q, section.q, 1e-6) << section.c1;
    }
    for (std::size_t i = 0; i < 3; ++i)
    {
        EXPECT_NEAR(SushiDSP::dd3::kAntiAliasSectionFrequencies[i], sections[i].f0, 0.01);
        EXPECT_NEAR(SushiDSP::dd3::kAntiAliasSectionQ[i], sections[i].q, 1e-6);
    }
    for (std::size_t i = 0; i < 2; ++i)
    {
        EXPECT_NEAR(SushiDSP::dd3::kReconstructionSectionFrequencies[i], sections[3 + i].f0, 0.01);
        EXPECT_NEAR(SushiDSP::dd3::kReconstructionSectionQ[i], sections[3 + i].q, 1e-6);
    }
}

TEST(Unit_Dsp, Dd3AntiAliasCascadeMatchesItsAnalyticResponse)
{
    // N3's analytic cascade, measured at 192 kHz so bilinear warping is negligible: the highest
    // section sits at 8.9 kHz, i.e. f/fs = 0.046, where prewarping moves the shape by well under
    // 0.05 dB. Asserting this at 48 kHz instead would fail for a legitimate reason and someone
    // would then loosen the tolerance.
    //   1 kHz  -> 1.017788  (+0.1531 dB)
    //   7 kHz  -> 2.101083  (+6.4487 dB)  <- the cascade PEAKS here; see the plan's N3 note
    //  10 kHz  -> 0.469527  (-6.5631 dB)
    //  20 kHz  -> 0.0022213 (-53.0667 dB)
    SushiDSP::dd3::Dd3AntiAliasFilter filter;
    filter.prepare(192000.0);
    struct Point { double frequency; double magnitude; };
    const Point points[4] = { { 1000.0, 1.017788 }, { 7000.0, 2.101083 },
                              { 10000.0, 0.469527 }, { 20000.0, 0.0022213 } };
    for (const Point& point : points)
    {
        filter.reset();
        const double measured = chain_magnitude(
            [&](float x) { return filter.process_sample(x); }, point.frequency, 192000.0);
        EXPECT_NEAR(measured, point.magnitude, 0.02 * point.magnitude) << point.frequency;
    }
}

TEST(Unit_Dsp, Dd3ReconstructionCascadeMatchesItsAnalyticResponse)
{
    //   1 kHz  -> 1.005211  (+0.0451 dB)
    //   7 kHz  -> 1.097942  (+0.8117 dB)
    //  20 kHz  -> 0.273029  (-11.2802 dB)
    SushiDSP::dd3::Dd3ReconstructionFilter filter;
    filter.prepare(192000.0);
    struct Point { double frequency; double magnitude; };
    const Point points[3] = { { 1000.0, 1.005211 }, { 7000.0, 1.097942 }, { 20000.0, 0.273029 } };
    for (const Point& point : points)
    {
        filter.reset();
        const double measured = chain_magnitude(
            [&](float x) { return filter.process_sample(x); }, point.frequency, 192000.0);
        EXPECT_NEAR(measured, point.magnitude, 0.02 * point.magnitude) << point.frequency;
    }
}

TEST(Unit_Dsp, Dd3AntiAliasCascadeStaysUsableAtFortyEightKilohertz)
{
    // The rate the node actually runs at. The 1 kHz point is unaffected by warping; the 20 kHz
    // point is more attenuated than the analytic -53.07 dB, because the RBJ low-pass carries a
    // zero at Nyquist that the analogue prototype does not. Both are asserted so a future change
    // of discretisation is caught, and the 7 kHz peak is printed rather than pinned.
    SushiDSP::dd3::Dd3AntiAliasFilter filter;
    filter.prepare(48000.0);
    filter.reset();
    EXPECT_NEAR(chain_magnitude([&](float x) { return filter.process_sample(x); }, 1000.0, 48000.0),
                1.017788, 0.01);
    filter.reset();
    const double at_seven = chain_magnitude([&](float x) { return filter.process_sample(x); }, 7000.0, 48000.0);
    std::printf("[ measure ] DD-3 anti-alias cascade at 7 kHz, 48 kHz rate: %.4f (%.3f dB)\n",
                at_seven, 20.0 * std::log10(at_seven));
    filter.reset();
    EXPECT_LE(chain_magnitude([&](float x) { return filter.process_sample(x); }, 20000.0, 48000.0),
              0.0022213);
}

TEST(Unit_Dsp, Dd3EmphasisPairIsTheIdentity)
{
    // N4: the pre-emphasis zero is the de-emphasis pole and vice versa, and 33/47 * 47/33 = 1,
    // so the cascade is exactly unity. Null below -120 dB on broadband noise.
    for (const double sample_rate : { 48000.0, 96000.0 })
    {
        SushiDSP::dd3::Dd3PreEmphasis pre;
        SushiDSP::dd3::Dd3DeEmphasis de;
        pre.prepare(sample_rate);
        de.prepare(sample_rate);
        double signal_power = 0.0;
        double error_power = 0.0;
        std::uint32_t state = 22695477u;
        for (std::size_t n = 0; n < 200000; ++n)
        {
            state = state * 1103515245u + 12345u;
            const float input = 0.5f * (static_cast<float>(state >> 16) / 32768.0f - 1.0f);
            const float output = de.process_sample(pre.process_sample(input));
            if (n >= 4096)
            {
                signal_power += static_cast<double>(input) * input;
                error_power += static_cast<double>(output - input) * (output - input);
            }
        }
        EXPECT_LE(10.0 * std::log10(error_power / signal_power), -120.0) << sample_rate;
    }
}

TEST(Unit_Dsp, Dd3PreEmphasisHitsItsThreeDerivedGains)
{
    // N4: 0.7021277 below the zero, 1.3183700 at 1 kHz, 4.0021712 above the pole.
    SushiDSP::dd3::Dd3PreEmphasis pre;
    pre.prepare(96000.0);
    pre.reset();
    EXPECT_NEAR(chain_magnitude([&](float x) { return pre.process_sample(x); }, 20.0, 96000.0),
                0.7021277, 2e-3);
    pre.reset();
    EXPECT_NEAR(chain_magnitude([&](float x) { return pre.process_sample(x); }, 1000.0, 96000.0),
                1.3183700, 3e-3);
    pre.reset();
    EXPECT_NEAR(chain_magnitude([&](float x) { return pre.process_sample(x); }, 24000.0, 96000.0),
                4.0021712, 4e-2);
}
```

- [ ] **Step 2: `sd build` — expected FAIL. Step 3: write the header and the source.** Each class
owns its `Biquad`s by value and configures them in `prepare()` with
`set_low_pass(sample_rate, kAntiAliasSectionFrequencies[i], kAntiAliasSectionQ[i])` and
`set_first_order_shelf(sample_rate, kEmphasisZeroHz, kEmphasisPoleHz, kPreEmphasisLowGain)`
respectively (corners swapped and `kDeEmphasisLowGain` for the de-emphasis). `process_sample` is the
cascade, `reset()` resets every section.

- [ ] **Step 4: `sd test -s unit` green. Commit**

```bash
git add modules/dd3/include/SushiDSP/dd3/Dd3AnalogFilters.hpp modules/dd3/src/Dd3AnalogFilters.cpp tests/functional/unit/test_dd3_analog_filters.cpp
git commit -m "DR2: the DD-3's four linear networks from the schematic's component values; anti-alias cascade peaks +6.45 dB at 7 kHz, which the plan's N3 records as a contradiction with the published +1/-3 dB figure"
```

---

### Task 2.2: `Ne570Compander`

**Files:**
- Create: `modules/dd3/include/SushiDSP/dd3/Ne570Compander.hpp`
- Create: `modules/dd3/src/Ne570Compander.cpp`
- Modify: `tests/functional/unit/test_ne570_compander.cpp` (replace stub)

**Depends on:** nothing.

**Interface:**

```cpp
namespace SushiDSP::dd3
{
    /// @brief Which half of the NE570 an instance is.
    enum class CompanderHalf { Compressor, Expander };

    class Ne570Compander
    {
    public:
        /// @param half Compressor (rectifier on the output, 2:1) or expander (on the input, 1:2).
        explicit Ne570Compander(CompanderHalf half);

        /// @brief Derives the rectifier's one-pole coefficient. Control thread only.
        /// @param sample_rate Rate process_sample() will be called at.
        void prepare(double sample_rate);

        /// @brief Runs one sample through the rectifier and the gain cell.
        /// @param volts Input in volts.
        /// @return Output in volts.
        float process_sample(float volts);

        /// @brief Zeroes the envelope.
        void reset();

        /// @brief The rectifier's current output, in volts. For tests and for diagnostics.
        double envelope_volts() const;

        static constexpr double kRectifierResistanceOhms   = 10000.0;   // NE570 internal R1
        static constexpr double kRectifierCapacitanceFarads = 6.8e-7;   // C32 = C33 = 0.68 uF
        static constexpr double kTimeConstantSeconds        = 6.8e-3;   // 10k * 0.68u
        static constexpr double kRippleCornerHz             = 23.4051;  // 1/(2*pi*10k*0.68u)
        static constexpr double kReferenceVolts             = 6.6125;   // from check points [2],[3]
        static constexpr double kEnvelopeFloor              = 1e-4;     // 80 dB maximum gain
    };
}
```

`process_sample`, per N6, with `rectified = |volts| * (pi/2)` (a full-wave rectifier calibrated to
read a sine's peak) and `coefficient = 1 - exp(-1/(sample_rate * kTimeConstantSeconds))`:

```
compressor: gain = kReferenceVolts / max(envelope_, kReferenceVolts * kEnvelopeFloor)
            output = volts * gain
            envelope_ += coefficient * (|output| * pi/2 - envelope_)     // rectifier on the OUTPUT
expander:   envelope_ += coefficient * (|volts| * pi/2 - envelope_)      // rectifier on the INPUT
            gain = max(envelope_, kReferenceVolts * kEnvelopeFloor) / kReferenceVolts
            output = volts * gain
```

The compressor's one-sample envelope lag is deliberate and is stated in the header: the loop is
otherwise algebraic, and at a 6.8 ms time constant one sample of lag is 0.3 % of one time constant at
48 kHz.

- [ ] **Step 1: Write the failing tests.**

```cpp
namespace
{
    /// @brief Settled peak of a compander driven by a sine of the given peak amplitude.
    /// @param half Which half to build.
    /// @param input_peak_volts Peak of the drive signal.
    /// @param frequency_hz Drive frequency.
    /// @return Peak output in volts, measured over the last 20 cycles of a 500 ms run.
    double settled_peak(SushiDSP::dd3::CompanderHalf half, double input_peak_volts, double frequency_hz)
    {
        SushiDSP::dd3::Ne570Compander compander(half);
        compander.prepare(48000.0);
        const std::size_t settle = 24000;                        // 500 ms, 73 time constants
        const std::size_t measure = static_cast<std::size_t>(std::round(20.0 * 48000.0 / frequency_hz));
        double peak = 0.0;
        for (std::size_t n = 0; n < settle + measure; ++n)
        {
            const float y = compander.process_sample(static_cast<float>(
                input_peak_volts * std::sin(2.0 * M_PI * frequency_hz * static_cast<double>(n) / 48000.0)));
            if (n >= settle) { peak = std::max(peak, std::abs(static_cast<double>(y))); }
        }
        return peak;
    }
}

TEST(Unit_Dsp, Ne570CompressorFollowsTheTwoToOneLaw)
{
    // N6: V_out = sqrt(V_ref * V_in) with V_ref = 6.6125 V.
    //   V_in = 0.066125 (0.01 V_ref) -> sqrt(6.6125*0.066125) = 0.66125     (exactly 0.1 V_ref)
    //   V_in = 0.66125  (0.1 V_ref)  -> sqrt(6.6125*0.66125)  = 2.0910639   (0.3162278 V_ref)
    //   V_in = 6.6125   (1.0 V_ref)  -> 6.6125                              (exactly V_ref)
    //   V_in = 26.45    (4.0 V_ref)  -> sqrt(6.6125*26.45)    = 13.225      (exactly 2 V_ref)
    // The 2 % tolerance is the rectifier's own ripple at 1 kHz: the ripple sits at 2 kHz and the
    // 23.4051 Hz pole leaves 23.4051/2000 = 1.17 % of it on the control voltage.
    struct Point { double input; double output; };
    const Point points[4] = { { 0.066125, 0.66125 }, { 0.66125, 2.0910639 },
                              { 6.6125, 6.6125 }, { 26.45, 13.225 } };
    for (const Point& point : points)
    {
        EXPECT_NEAR(settled_peak(SushiDSP::dd3::CompanderHalf::Compressor, point.input, 1000.0),
                    point.output, 0.02 * point.output) << point.input;
    }
}

TEST(Unit_Dsp, Ne570ExpanderFollowsTheOneToTwoLaw)
{
    // N6: V_out = V_in^2 / V_ref.
    //   0.66125    -> 0.66125^2/6.6125    = 0.066125
    //   2.0910639  -> 2.0910639^2/6.6125  = 0.66125
    //   6.6125     -> 6.6125
    //   13.225     -> 13.225^2/6.6125     = 26.45
    struct Point { double input; double output; };
    const Point points[4] = { { 0.66125, 0.066125 }, { 2.0910639, 0.66125 },
                              { 6.6125, 6.6125 }, { 13.225, 26.45 } };
    for (const Point& point : points)
    {
        EXPECT_NEAR(settled_peak(SushiDSP::dd3::CompanderHalf::Expander, point.input, 1000.0),
                    point.output, 0.02 * point.output) << point.input;
    }
}

TEST(Unit_Dsp, Ne570RoundTripIsTheIdentityInTheStaticLimit)
{
    // The two halves are exact inverses on a steady tone: expander(compressor(x)) = x.
    for (const double input : { 0.066125, 0.66125, 2.0910639, 6.6125, 13.225 })
    {
        SushiDSP::dd3::Ne570Compander compressor(SushiDSP::dd3::CompanderHalf::Compressor);
        SushiDSP::dd3::Ne570Compander expander(SushiDSP::dd3::CompanderHalf::Expander);
        compressor.prepare(48000.0);
        expander.prepare(48000.0);
        double peak = 0.0;
        for (std::size_t n = 0; n < 24960; ++n)
        {
            const float y = expander.process_sample(compressor.process_sample(static_cast<float>(
                input * std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(n) / 48000.0))));
            if (n >= 24000) { peak = std::max(peak, std::abs(static_cast<double>(y))); }
        }
        EXPECT_NEAR(peak, input, 0.03 * input) << input;
    }
}

TEST(Unit_Dsp, Ne570EnvelopeReachesSixtyThreePercentAtSixPointEightMilliseconds)
{
    // G(t) = (G_i - G_f)*e^(-t/tau) + G_f with tau = 10k * 0.68u = 6.8 ms exactly.
    // 1 - e^-1 = 0.6321206 of a step, at 0.0068 * 48000 = 326.4 samples.
    SushiDSP::dd3::Ne570Compander expander(SushiDSP::dd3::CompanderHalf::Expander);
    expander.prepare(48000.0);
    const double target = 1.0;                       // a 1 V DC step reads 1 V * pi/2 rectified...
    std::size_t crossing = 0;
    for (std::size_t n = 0; n < 4800; ++n)
    {
        expander.process_sample(static_cast<float>(target));
        // The rectifier reads |x| * pi/2, so its final value is pi/2; the 63.21 % point is
        // 0.6321206 * pi/2 = 0.9929407.
        if (crossing == 0 && expander.envelope_volts() >= 0.6321206 * M_PI / 2.0)
        {
            crossing = n + 1;
        }
    }
    EXPECT_NEAR(static_cast<double>(crossing), 326.4, 3.3);   // +/- 1 %
    EXPECT_NEAR(SushiDSP::dd3::Ne570Compander::kTimeConstantSeconds, 6.8e-3, 1e-12);
    EXPECT_NEAR(SushiDSP::dd3::Ne570Compander::kRippleCornerHz, 23.4051, 1e-3);
}

TEST(Unit_Dsp, Ne570CompressorGainSaturatesAtTheEnvelopeFloor)
{
    // Below V_ref * kEnvelopeFloor = 6.6125e-4 V the gain stops at 1/kEnvelopeFloor = 1e4 and the
    // law goes linear, which is the modelled end of the gain cell's range (N6).
    const double tiny = 1e-9;
    const double output = settled_peak(SushiDSP::dd3::CompanderHalf::Compressor, tiny, 1000.0);
    EXPECT_NEAR(output / tiny, 1.0e4, 1.0e2);
}

TEST(Unit_Dsp, Ne570RectifierRippleReachesTheSignalAtLowFrequencies)
{
    // Dossier 3, "Compander behaviour": the 23.4051 Hz rectifier corner is deliberately fast, and
    // the datasheet's stated penalty - ripple modulating the gain cell into third-harmonic
    // distortion on low-frequency material - is a feature to reproduce, not an artefact to filter
    // out. At 50 Hz the ripple sits at 100 Hz and the pole leaves 23.4051/100 = 23.4 % of it, so
    // the third harmonic must be clearly present. This asserts a floor, not a ceiling: the test
    // fails if a future "improvement" smooths the envelope.
    SushiDSP::dd3::Ne570Compander compressor(SushiDSP::dd3::CompanderHalf::Compressor);
    compressor.prepare(48000.0);
    const double f0 = SushiDSP::measure::bin_exact_frequency(50.0, 48000.0, SushiDSP::measure::kFftSize);
    std::vector<float> output(48000u + SushiDSP::measure::kFftSize, 0.0f);
    for (std::size_t n = 0; n < output.size(); ++n)
    {
        output[n] = compressor.process_sample(static_cast<float>(
            6.6125 * std::sin(2.0 * M_PI * f0 * static_cast<double>(n) / 48000.0)));
    }
    const auto result = SushiDSP::measure::thd(output, f0, 48000.0, 5u);
    std::printf("[ measure ] NE570 compressor at 50 Hz / V_ref: THD %.3f %%, H3 %.2f dB\n",
                result.thd_percent, result.per_harmonic_db[2]);
    EXPECT_GT(result.per_harmonic_db[2], -40.0);
}

TEST(Unit_Dsp, Ne570HalvesDoNotShareState)
{
    // The expander is open-loop: it tracks the reconstructed signal, never what the compressor
    // did (dossier 1.9 item 2). Two instances driven differently must hold different envelopes.
    SushiDSP::dd3::Ne570Compander first(SushiDSP::dd3::CompanderHalf::Expander);
    SushiDSP::dd3::Ne570Compander second(SushiDSP::dd3::CompanderHalf::Expander);
    first.prepare(48000.0);
    second.prepare(48000.0);
    for (std::size_t n = 0; n < 1000; ++n)
    {
        first.process_sample(1.0f);
        second.process_sample(0.0f);
    }
    EXPECT_GT(first.envelope_volts(), 1.0);
    EXPECT_NEAR(second.envelope_volts(), 0.0, 1e-9);
}
```

- [ ] **Step 2: `sd build` — expected FAIL. Step 3: write the header and the source,** transcribing
the two branches above. The `half_` member is `const` and set in the constructor; there is no
runtime mode switch, because a compressor never becomes an expander.

- [ ] **Step 4: `sd test -s unit` green. Copy the two `[ measure ]` lines into the commit. Commit**

```bash
git add modules/dd3/include/SushiDSP/dd3/Ne570Compander.hpp modules/dd3/src/Ne570Compander.cpp tests/functional/unit/test_ne570_compander.cpp
git commit -m "DR2: NE570 compander halves, 2:1 and 1:2 about a 6.6125 V reference derived from check points [2] and [3], tau 6.8 ms, rectifier ripple left in"
```

---

### Task 2.3: `Dd3Converter` — the 12-bit characteristic and the ring

**Files:**
- Create: `modules/dd3/include/SushiDSP/dd3/Dd3Converter.hpp`
- Create: `modules/dd3/src/Dd3Converter.cpp`
- Modify: `tests/functional/unit/test_dd3_converter.cpp` (replace stub)

**Depends on:** nothing.

**Interface:**

```cpp
namespace SushiDSP::dd3
{
    /// @brief The mode switch's four positions (dossier 1.8, S1 SBM1024 truth table).
    enum class Dd3Mode { Short, Middle, Long, Hold };

    class Dd3Converter
    {
    public:
        /// @brief Allocates the ring at its maximum length. Control thread only.
        void prepare();

        /// @brief Selects the buffer length, clearing the ring only when N changes (Ruling 14).
        /// Audio-thread safe; the clear is 16384 floats, about 2 us, and never happens in steady
        /// state or on the Long <-> Hold transition.
        /// @param mode The new mode; Long and Hold share N = 16384, so switching between them
        /// keeps whatever the loop holds, which is what HOLD is for.
        void set_mode(Dd3Mode mode);

        /// @brief Quantises, writes (unless HOLD) and reads one sample at the ring's clock.
        /// @param volts The sample to store, in volts at the converter's own scale.
        /// @return What the ring held N samples ago, dequantised.
        float process_sample(float volts);

        /// @brief Zeroes the ring without changing the mode.
        void reset();

        /// @brief The active buffer length in samples.
        /// @return 1024, 4096 or 16384.
        std::uint32_t ring_length() const;

        /// @brief The 12-bit mid-tread characteristic, saturating at both rails.
        /// @param volts Input voltage.
        /// @return The dequantised voltage.
        static double quantise(double volts);

        static constexpr double        kFullScaleVolts    = 2.5;
        static constexpr double        kStepVolts         = 5.0 / 4096.0;   // 1.220703125e-3 V
        static constexpr std::int32_t  kMaximumCode       = 2047;
        static constexpr std::int32_t  kMinimumCode       = -2048;
        static constexpr std::uint32_t kShortLength       = 1024u;
        static constexpr std::uint32_t kMiddleLength      = 4096u;
        static constexpr std::uint32_t kLongLength        = 16384u;
        static constexpr std::uint32_t kMaximumRingLength = 16384u;
    };
}
```

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, Dd3ConverterQuantisesOnItsExactCodeBoundaries)
{
    // step = 2 * 2.5 / 4096 = 5/4096 = 1.220703125e-3 V exactly  (DR1+DR2 plan, N8).
    using Converter = SushiDSP::dd3::Dd3Converter;
    EXPECT_DOUBLE_EQ(Converter::kStepVolts, 1.220703125e-3);
    EXPECT_DOUBLE_EQ(Converter::quantise(0.0), 0.0);
    EXPECT_DOUBLE_EQ(Converter::quantise(0.49 * Converter::kStepVolts), 0.0);
    EXPECT_DOUBLE_EQ(Converter::quantise(0.51 * Converter::kStepVolts), Converter::kStepVolts);
    EXPECT_DOUBLE_EQ(Converter::quantise(-0.51 * Converter::kStepVolts), -Converter::kStepVolts);
    EXPECT_DOUBLE_EQ(Converter::quantise(1.0 * Converter::kStepVolts), Converter::kStepVolts);
}

TEST(Unit_Dsp, Dd3ConverterSaturatesRatherThanWrapping)
{
    // Ruling 15. Code 2047 is 2047 * 1.220703125e-3 = 2.4987793 V; code -2048 is exactly -2.5 V.
    using Converter = SushiDSP::dd3::Dd3Converter;
    EXPECT_DOUBLE_EQ(Converter::quantise(10.0), 2047.0 * Converter::kStepVolts);
    EXPECT_DOUBLE_EQ(Converter::quantise(1e6), 2047.0 * Converter::kStepVolts);
    EXPECT_DOUBLE_EQ(Converter::quantise(-10.0), -2048.0 * Converter::kStepVolts);
    EXPECT_NEAR(2047.0 * Converter::kStepVolts, 2.4987793, 1e-6);
}

TEST(Unit_Dsp, Dd3ConverterQuantisationNoiseMatchesTwelveBits)
{
    // An ideal N-bit converter gives 6.0206*N + 1.7609 dB of SNR for a full-scale sine:
    // 6.0206*12 + 1.7609 = 74.008 dB. Measure it directly as the ratio of signal power to
    // error power, which needs no window and no FFT.
    using Converter = SushiDSP::dd3::Dd3Converter;
    double signal_power = 0.0;
    double error_power = 0.0;
    for (std::size_t n = 0; n < 480000; ++n)
    {
        const double input = Converter::kFullScaleVolts
                           * std::sin(2.0 * M_PI * 997.0 * static_cast<double>(n) / 48000.0);
        const double clamped = std::max(-Converter::kFullScaleVolts,
                                        std::min(input, 2047.0 * Converter::kStepVolts));
        const double output = Converter::quantise(clamped);
        signal_power += clamped * clamped;
        error_power += (output - clamped) * (output - clamped);
    }
    const double snr_db = 10.0 * std::log10(signal_power / error_power);
    std::printf("[ measure ] DD-3 converter SNR: %.3f dB (ideal 12-bit 74.008)\n", snr_db);
    EXPECT_NEAR(snr_db, 74.008, 1.0);
}

TEST(Unit_Dsp, Dd3ConverterRingDelaysByExactlyNSamples)
{
    using SushiDSP::dd3::Dd3Mode;
    struct Case { Dd3Mode mode; std::uint32_t length; };
    const Case cases[3] = { { Dd3Mode::Short, 1024u }, { Dd3Mode::Middle, 4096u },
                            { Dd3Mode::Long, 16384u } };
    for (const Case& item : cases)
    {
        SushiDSP::dd3::Dd3Converter converter;
        converter.prepare();
        converter.set_mode(item.mode);
        EXPECT_EQ(converter.ring_length(), item.length);
        std::uint32_t impulse_at = 0;
        for (std::uint32_t n = 0; n < 3u * item.length; ++n)
        {
            const float out = converter.process_sample(n == 0 ? 1.0f : 0.0f);
            if (out != 0.0f && impulse_at == 0) { impulse_at = n; }
        }
        EXPECT_EQ(impulse_at, item.length) << item.length;
    }
}

TEST(Unit_Dsp, Dd3ConverterHoldKeepsTheLoopAndInhibitsWrites)
{
    // Dossier 1.8: HOLD is a mode bit into the LSI, sharing L's clock range with writes inhibited.
    // Ruling 14: Long and Hold share N, so the transition between them must not clear the ring -
    // holding silence is the one thing HOLD must not do.
    SushiDSP::dd3::Dd3Converter converter;
    converter.prepare();
    converter.set_mode(SushiDSP::dd3::Dd3Mode::Long);
    for (std::uint32_t n = 0; n < 16384u; ++n)
    {
        converter.process_sample(n == 100u ? 1.0f : 0.0f);
    }
    converter.set_mode(SushiDSP::dd3::Dd3Mode::Hold);
    // The stored impulse comes back on schedule and keeps coming back, lap after lap, while
    // everything pushed in is discarded.
    for (std::uint32_t lap = 0; lap < 3u; ++lap)
    {
        for (std::uint32_t n = 0; n < 16384u; ++n)
        {
            const float out = converter.process_sample(0.5f);
            if (n == 100u) { EXPECT_NEAR(out, 1.0f, SushiDSP::dd3::Dd3Converter::kStepVolts); }
            else           { EXPECT_FLOAT_EQ(out, 0.0f) << lap << " " << n; }
        }
    }
    EXPECT_EQ(converter.ring_length(), 16384u);
}

TEST(Unit_Dsp, Dd3ConverterModeChangeClearsTheRingWhenNChanges)
{
    // Spec: "MODE changes swap N and clear the ring" - which is exactly the S/M/L case.
    SushiDSP::dd3::Dd3Converter converter;
    converter.prepare();
    converter.set_mode(SushiDSP::dd3::Dd3Mode::Short);
    for (std::uint32_t n = 0; n < 512u; ++n) { converter.process_sample(0.9f); }
    converter.set_mode(SushiDSP::dd3::Dd3Mode::Middle);
    EXPECT_EQ(converter.ring_length(), 4096u);
    for (std::uint32_t n = 0; n < 4096u; ++n)
    {
        EXPECT_FLOAT_EQ(converter.process_sample(0.0f), 0.0f) << n;
    }
}
```

- [ ] **Step 2: `sd build` — expected FAIL. Step 3: write the header and the source.** `prepare()`
does `ring_.assign(kMaximumRingLength, 0.0f)`; `set_mode` sets `length_`, and **only when `length_` actually changed** does
`std::fill(ring_.begin(), ring_.end(), 0.0f)` and reset `index_` to 0 (Ruling 14); it allocates
nothing either way;
`process_sample` reads `ring_[index_]`, writes `quantise(volts)` there unless the mode is `Hold`,
advances `index_` modulo `length_`, and returns the value read. `quantise` is
`clamp(lround(volts / kStepVolts), kMinimumCode, kMaximumCode) * kStepVolts` in `double`. The header
carries Ruling 13 (no R-2R DNL table yet, and where it would go), Ruling 14 (the ring clears only
when N changes) and Ruling 15 (saturate, not wrap).

- [ ] **Step 4: `sd test -s unit` green. Commit**

```bash
git add modules/dd3/include/SushiDSP/dd3/Dd3Converter.hpp modules/dd3/src/Dd3Converter.cpp tests/functional/unit/test_dd3_converter.cpp
git commit -m "DR2: 12-bit saturating converter characteristic (SNR <x> dB) and the fixed-length ring with HOLD as a write inhibit"
```

---

### Task 2.4: `Dd3Clock` — the log taper, the R56/C52 slew and the VCO law

**Files:**
- Create: `modules/dd3/include/SushiDSP/dd3/Dd3Clock.hpp`
- Create: `modules/dd3/src/Dd3Clock.cpp`
- Modify: `tests/functional/unit/test_dd3_clock.cpp` (replace stub)

**Depends on:** nothing (uses the existing `SmoothedParameter`).

**Interface:**

```cpp
namespace SushiDSP::dd3
{
    class Dd3Clock
    {
    public:
        /// @brief Prepares the R56/C52 pole at this rate. Control thread only.
        /// @param sample_rate Rate advance() is counted in.
        void prepare(double sample_rate);

        /// @brief Sets the D.TIME knob position. Real-time safe.
        /// @param normalized 0 = fastest clock (shortest delay), 1 = slowest.
        void set_time(float normalized);

        /// @brief Advances the slew pole and reports the clock.
        /// @param frames Samples to advance; the DD-3 calls this once per 8-frame sub-block.
        /// @return The sample clock in Hz, inside [20000, 82500].
        double advance(std::uint32_t frames);

        /// @brief The clock without advancing.
        /// @return Hz.
        double sample_rate_hz() const;

        /// @brief Snaps the pole to the current knob position, for a click-free restart.
        void reset();

        /// @brief The VCO law with the log taper already folded in.
        /// @param normalized Knob position in [0, 1].
        /// @return 82500 * 4.125^(-normalized) Hz.
        static double clock_for_time(double normalized);

        static constexpr double kFastRateHz              = 82500.0;
        static constexpr double kSlowRateHz              = 20000.0;
        static constexpr double kSlewResistanceOhms      = 22000.0;    // R56
        static constexpr double kSlewCapacitanceFarads   = 2.7e-8;     // C52 0.027 uF
        static constexpr double kSlewTimeConstantSeconds = 5.94e-4;    // 22k * 0.027u
        static constexpr float  kSlewTimeConstantMs      = 0.594f;
    };
}
```

Internally one `SmoothedParameter control_` holding `log(fs)` and prepared with
`kSlewTimeConstantMs`; `set_time` sets its target to `log(clock_for_time(normalized))`; `advance`
calls `control_.skip(frames)` and returns `exp(control_.current())`. Smoothing the **logarithm** is
what the physical R/C does — it sits on the VCO's control node, and a constant-rate glide in
`log fs` is a constant-rate glide in pitch (N5).

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, Dd3ClockHitsThePublishedEndpointsAndTheDerivedMidpoint)
{
    // N5: fs(TIME) = 82500 * 4.125^(-TIME); 4.125^0.5 = 2.0310096, so fs(0.5) = 40620.4 Hz.
    using Clock = SushiDSP::dd3::Dd3Clock;
    EXPECT_NEAR(Clock::clock_for_time(0.0), 82500.0, 1e-6);
    EXPECT_NEAR(Clock::clock_for_time(1.0), 20000.0, 1e-6);
    EXPECT_NEAR(Clock::clock_for_time(0.5), 40620.4, 0.5);
    EXPECT_NEAR(Clock::clock_for_time(0.25), 57889.5, 0.5);
    // Acceptance row 1: within 2 % of the published 20-82.5 kHz curve at both endpoints.
    EXPECT_LE(std::abs(Clock::clock_for_time(0.0) - 82500.0) / 82500.0, 0.02);
    EXPECT_LE(std::abs(Clock::clock_for_time(1.0) - 20000.0) / 20000.0, 0.02);
}

TEST(Unit_Dsp, Dd3ClockIsMonotoneAndStaysInsideItsRange)
{
    using Clock = SushiDSP::dd3::Dd3Clock;
    double previous = 1e9;
    for (int step = 0; step <= 1000; ++step)
    {
        const double rate = Clock::clock_for_time(static_cast<double>(step) / 1000.0);
        EXPECT_LT(rate, previous) << step;
        EXPECT_GE(rate, 20000.0 - 1e-6);
        EXPECT_LE(rate, 82500.0 + 1e-6);
        previous = rate;
    }
}

TEST(Unit_Dsp, Dd3ClockDelayTimesLandOnThePublishedRanges)
{
    // N5's mode table, and the reason it is not exact: N = 1024/4096/16384 against published
    // times computed with N = 1000/4000/16000. The delay-time errors are printed, not asserted;
    // acceptance row 1 is a check on fs, which the case above makes.
    using Clock = SushiDSP::dd3::Dd3Clock;
    struct Mode { const char* name; double n; double fast_ms; double slow_ms; };
    const Mode modes[3] = { { "S", 1024.0, 12.5, 50.0 }, { "M", 4096.0, 50.0, 200.0 },
                            { "L", 16384.0, 200.0, 800.0 } };
    for (const Mode& mode : modes)
    {
        const double fast = 1000.0 * mode.n / Clock::clock_for_time(0.0);
        const double slow = 1000.0 * mode.n / Clock::clock_for_time(1.0);
        std::printf("[ measure ] DD-3 mode %s: %.6f - %.6f ms (published %.1f - %.1f, "
                    "error %+.2f %% / %+.2f %%)\n",
                    mode.name, fast, slow, mode.fast_ms, mode.slow_ms,
                    100.0 * (fast / mode.fast_ms - 1.0), 100.0 * (slow / mode.slow_ms - 1.0));
        EXPECT_NEAR(fast, mode.fast_ms, 0.12 * mode.fast_ms);
        EXPECT_NEAR(slow, mode.slow_ms, 0.12 * mode.slow_ms);
    }
    // The exact values this plan computed, so a change to the VCO law shows up here.
    EXPECT_NEAR(1000.0 * 1024.0 / Clock::clock_for_time(0.0), 12.412121, 1e-5);
    EXPECT_NEAR(1000.0 * 16384.0 / Clock::clock_for_time(1.0), 819.2, 1e-4);
}

TEST(Unit_Dsp, Dd3ClockSlewMatchesTheR56C52Pole)
{
    // tau = 22000 * 2.7e-8 = 5.94e-4 s = 0.594 ms = 28.512 samples at 48 kHz.
    // After one tau the control has covered 1 - e^-1 = 0.6321206 of the log distance, and
    // log2(4.125) = 2.0443847 octaves, so the pitch has moved 1.2922934 octaves:
    //   fs = 82500 * 2^-1.2922934 = 82500 * 0.408259 = 33681.4 Hz
    using Clock = SushiDSP::dd3::Dd3Clock;
    EXPECT_NEAR(Clock::kSlewTimeConstantSeconds, 5.94e-4, 1e-12);
    SushiDSP::dd3::Dd3Clock clock;
    clock.prepare(48000.0);
    clock.set_time(0.0f);
    clock.reset();
    ASSERT_NEAR(clock.sample_rate_hz(), 82500.0, 1.0);
    clock.set_time(1.0f);
    const double after_one_tau = clock.advance(28u);      // 28.512 samples, rounded down
    std::printf("[ measure ] DD-3 clock after one R56/C52 time constant: %.1f Hz\n", after_one_tau);
    EXPECT_NEAR(after_one_tau, 33681.4, 400.0);
}

TEST(Unit_Dsp, Dd3ClockAdvancesOneQuarterOfTheRemainingDistancePerSubBlock)
{
    // Ruling 11: 8 frames at 48 kHz is 8/28.512 = 0.28058305 time constants, so the pole closes
    // 1 - exp(-0.28058305) = 0.2446570 of what is left, every sub-block. That figure is what
    // makes the pitch trace a glide rather than a jump, so it is pinned here.
    using Clock = SushiDSP::dd3::Dd3Clock;
    SushiDSP::dd3::Dd3Clock clock;
    clock.prepare(48000.0);
    clock.set_time(0.0f);
    clock.reset();
    clock.set_time(1.0f);
    const double start_log = std::log(82500.0);
    const double end_log = std::log(20000.0);
    const double after = std::log(clock.advance(8u));
    EXPECT_NEAR((after - start_log) / (end_log - start_log), 0.2446570, 5e-3);
}

TEST(Unit_Dsp, Dd3ClockResetSnapsWithoutGliding)
{
    SushiDSP::dd3::Dd3Clock clock;
    clock.prepare(48000.0);
    clock.set_time(0.75f);
    clock.reset();
    EXPECT_NEAR(clock.sample_rate_hz(), SushiDSP::dd3::Dd3Clock::clock_for_time(0.75), 1.0);
}
```

- [ ] **Step 2: `sd build` — expected FAIL. Step 3: write the header and the source,** transcribing
N5's law and the `SmoothedParameter` wiring. `clock_for_time` is
`kFastRateHz * std::pow(kSlowRateHz / kFastRateHz, normalized)` with `normalized` clamped to
`[0, 1]`.

- [ ] **Step 4: `sd test -s unit` green. Copy the three `[ measure ]` lines into the commit. Commit**

```bash
git add modules/dd3/include/SushiDSP/dd3/Dd3Clock.hpp modules/dd3/src/Dd3Clock.cpp tests/functional/unit/test_dd3_clock.cpp
git commit -m "DR2: DD-3 VCO clock, 82.5 kHz to 20 kHz exponential in the knob, slewed by the R56/C52 0.594 ms pole on log(fs)"
```

---

### Task 2.5: `Dd3Delay` — the node, and the loop that closes around everything

**Files:**
- Create: `modules/dd3/include/SushiDSP/dd3/Dd3Delay.hpp`
- Create: `modules/dd3/src/Dd3Delay.cpp`
- Modify: `tests/functional/unit/test_dd3_delay.cpp` (replace stub)

**Depends on:** Tasks 1.3, 2.1, 2.2, 2.3, 2.4.

**Interface:**

```cpp
namespace SushiDSP
{
    class Dd3Delay : public IAudioNode
    {
    public:
        Dd3Delay();
        void prepare(const PrepareInfo& info) override;
        void process(ProcessContext& context) override;
        PortLayout ports() const override;                  // { 1, 1 }
        void reset() override;
        std::uint32_t latency_samples() const override;     // 0, always: Ruling 5
        const char* type_id() const override;               // "sushidsp.dd3"

        void set_time(float normalized);                    // 0 = shortest
        void set_feedback(float normalized);
        void set_level(float normalized);
        void set_mode(dd3::Dd3Mode mode);

        using IAudioNode::set_parameter;
        float get_parameter(const char* id) const override;
        void set_parameter(const char* id, float value) override;
        void set_parameter(std::uint16_t index, float value) override;

        /// @brief The wet path's current delay: the ring, plus the two conversions' group delay,
        /// plus the wet FIFO's prime (DR1+DR2 plan, N2). Not reported as node latency, because
        /// the dry path is sample-aligned and the wet delay is the effect.
        /// @return Seconds.
        double wet_delay_seconds() const;

        static constexpr std::uint32_t kClockUpdateFrames = 8u;    // Ruling 11
        static constexpr std::uint32_t kWetPrimeFrames    = 8u;    // N2
        static constexpr double kInputPathGainToCheckPointOne = 4.70358;   // N7, a residual
        static constexpr double kExpanderOutputStageGain      = 6.0;       // N7
        static constexpr double kMaximumFeedback              = 1.1;
        static constexpr float  kLevelUnityGain = /* Step 5 measures it */;
    };

    /// @brief The DD-3's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.dd3".
    const NodeDescriptor& dd3_descriptor();
}
```

Parameters, in descriptor order: `time`, `feedback`, `level` (all `Linear`, `[0, 1]`) and `mode`
(`Choice`, `[0, 3]`, 0 = S, 1 = M, 2 = L, 3 = HOLD). Defaults: `time` 0.3, `feedback` 0.3, `level`
0.5, `mode` 1 (Middle — the range with the most use, and the only genuinely free choice in this
plan; changing it is a one-line edit and it is listed in Self-review as such).

**The per-sample loop, which the implementer transcribes.** One base-rate sample at a time, because
the feedback has to be sample-accurate: holding it across a sub-block would put a 166 us staircase on
a signal band-limited to 7 kHz, which is a much larger artefact than the one-sample hand-off it would
be avoiding.

```
every kClockUpdateFrames samples:
    fs = clock_.advance(kClockUpdateFrames)          // R56/C52 pole, Ruling 11
    down_.set_rates(sample_rate_, fs)
    up_.set_rates(fs, sample_rate_)

per sample j:
    dry   = input[j] * kFullScaleVolts
    drive = (dry + feedback_.next() * kMaximumFeedback * wet_previous_)
            * kInputPathGainToCheckPointOne
    drive = pre_emphasis_.process_sample(drive)          // Ruling 8: emphasis first
    drive = anti_alias_.process_sample(drive)            // the feedback sees this filter, as on the board
    drive = compressor_.process_sample(drive)

    k = down_.process(&drive, 1, fs_scratch_, kFsScratchCapacity)
    for i in [0, k):
        fs_scratch_[i] = converter_.process_sample(fs_scratch_[i])   // quantise, write unless HOLD, read
    m = up_.process(fs_scratch_, k, wet_scratch_, kWetScratchCapacity)
    push m samples of wet_scratch_ into wet_fifo_
    wet = pop one sample from wet_fifo_                  // primed with kWetPrimeFrames zeros

    wet = expander_.process_sample(wet) * kExpanderOutputStageGain
    wet = reconstruction_.process_sample(wet)
    wet_previous_ = wet                                  // dossier 1.6 reading (a): the F.BACK and
                                                         // E.LEVEL takeoff is AFTER the filter
    wet = de_emphasis_.process_sample(wet)               // Ruling 9: the shelf is in the wet leg
    output[j] = (float)((wet * level_.next() * kLevelUnityGain + dry) / kFullScaleVolts)
```

The dry term carries no shelf and no trim, so **`level = 0` makes the node a wire** — which is what
Task 2.5's second test asserts and what Ruling 9 exists to give.

`kFsScratchCapacity` and `kWetScratchCapacity` are `16` each: one base-rate sample can produce at
most `ceil(82500/48000) + 2 = 4` fs-rate samples and one fs-rate sample at most
`ceil(48000/20000) + 2 = 5` base-rate ones, and 16 leaves the bound obviously safe without a
`std::vector` on the audio path. `wet_fifo_` is a 64-sample `std::array<float, 64>` ring with a read
and a write index, primed to `kWetPrimeFrames` zeros by `reset()`.

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, Dd3DelayDeclaresItselfCorrectly)
{
    SushiDSP::Dd3Delay node;
    node.prepare({ 48000.0, 256u, 1u });
    EXPECT_EQ(node.ports().input_channels, 1u);
    EXPECT_EQ(node.ports().output_channels, 1u);
    EXPECT_STREQ(node.type_id(), "sushidsp.dd3");
    // Ruling 5: the dry path is sample-aligned, so there is nothing for a host to compensate.
    EXPECT_EQ(node.latency_samples(), 0u);
}

TEST(Unit_Dsp, Dd3DelayAtZeroLevelIsAWire)
{
    // Ruling 9: the dry branch carries no shelf and no trim. With E.LEVEL down, in == out.
    SushiDSP::Dd3Delay node;
    node.set_level(0.0f);
    node.set_feedback(0.0f);
    const auto input = SushiDSP::measure::sine_stimulus(440.0, 48000.0, 48000u, 0.5f);
    const auto output = SushiDSP::measure::render(node, input, 48000.0, 256u);
    for (std::size_t n = 0; n < input.size(); ++n)
    {
        ASSERT_NEAR(output[n], input[n], 1e-5f) << n;
    }
}

TEST(Unit_Dsp, Dd3DelayWetDelayMatchesThePlansArithmetic)
{
    // N2. Mode L at TIME = 0: 16384/82500 + 64/48000 + 8/48000
    //   = 0.198593939 + 0.001333333 + 0.000166667 = 0.200093939 s = 9604.51 samples at 48 kHz.
    // Mode S at TIME = 0: 1024/82500 + 64/48000 + 8/48000 = 0.013912121 s = 667.78 samples.
    struct Case { SushiDSP::dd3::Dd3Mode mode; double expected_seconds; };
    const Case cases[2] = {
        { SushiDSP::dd3::Dd3Mode::Short, 0.013912121 },
        { SushiDSP::dd3::Dd3Mode::Long,  0.200093939 },
    };
    for (const Case& item : cases)
    {
        SushiDSP::Dd3Delay node;
        node.set_mode(item.mode);
        node.set_time(0.0f);
        node.set_feedback(0.0f);
        node.set_level(1.0f);
        node.prepare({ 48000.0, 256u, 1u });
        EXPECT_NEAR(node.wet_delay_seconds(), item.expected_seconds, 1e-6);

        // And the measured delay agrees: an impulse comes back once, near that time.
        std::vector<float> input(static_cast<std::size_t>(0.5 * 48000.0), 0.0f);
        input[0] = 1.0f;
        const auto output = SushiDSP::measure::render(node, input, 48000.0, 256u);
        std::size_t peak_at = 0;
        float peak = 0.0f;
        for (std::size_t n = 8; n < output.size(); ++n)
        {
            if (std::abs(output[n]) > peak) { peak = std::abs(output[n]); peak_at = n; }
        }
        const double expected_samples = item.expected_seconds * 48000.0;
        std::printf("[ measure ] DD-3 wet echo at %zu samples, predicted %.2f\n", peak_at, expected_samples);
        // The echo has been through two 7 kHz cascades, so its peak spreads over a few samples;
        // 32 samples is tight enough to catch a wrong N, a wrong fs or a missed group delay.
        EXPECT_NEAR(static_cast<double>(peak_at), expected_samples, 32.0);
    }
}

TEST(Unit_Dsp, Dd3DelayFillsEveryBlockSizeAndStaysFinite)
{
    for (const std::uint32_t block : { 1u, 7u, 8u, 9u, 64u, 255u, 256u })
    {
        SushiDSP::Dd3Delay node;
        node.set_level(0.7f);
        node.set_feedback(0.6f);
        node.set_time(0.4f);
        const auto input = SushiDSP::measure::sine_stimulus(220.0, 48000.0, 48000u, 0.8f);
        const auto output = SushiDSP::measure::render(node, input, 48000.0, block);
        ASSERT_EQ(output.size(), input.size()) << block;
        for (const float sample : output)
        {
            ASSERT_TRUE(std::isfinite(sample)) << block;
        }
    }
}

TEST(Unit_Dsp, Dd3DelayResetReturnsItToSilence)
{
    SushiDSP::Dd3Delay node;
    node.set_level(1.0f);
    node.set_feedback(0.8f);
    node.prepare({ 48000.0, 256u, 1u });
    std::vector<float> loud(24000u, 0.0f);
    for (std::size_t n = 0; n < loud.size(); ++n)
    {
        loud[n] = 0.9f * static_cast<float>(std::sin(2.0 * M_PI * 300.0 * static_cast<double>(n) / 48000.0));
    }
    std::vector<float> scratch(24000u, 0.0f);
    const float* inputs[1] = { loud.data() };
    float* outputs[1] = { scratch.data() };
    for (std::uint32_t offset = 0; offset < 24000u; offset += 256u)
    {
        const float* block_in[1] = { loud.data() + offset };
        float* block_out[1] = { scratch.data() + offset };
        SushiDSP::ProcessContext context{ block_out, block_in, 256u, 0.0 };
        node.process(context);
    }
    (void)inputs; (void)outputs;
    node.reset();
    std::vector<float> silence(48000u, 0.0f);
    std::vector<float> after(48000u, 0.0f);
    for (std::uint32_t offset = 0; offset < 48000u; offset += 256u)
    {
        const float* block_in[1] = { silence.data() + offset };
        float* block_out[1] = { after.data() + offset };
        SushiDSP::ProcessContext context{ block_out, block_in, 256u, 0.0 };
        node.process(context);
    }
    for (const float sample : after)
    {
        EXPECT_NEAR(sample, 0.0f, 1e-6f);
    }
}

TEST(Unit_Dsp, Dd3DelayRepeatsDecayAndDarkenWithFeedback)
{
    // Dossier 3, "Self-oscillation": every repeat is re-band-limited by both 7 kHz cascades and
    // re-quantised, so repeats darken cumulatively. Assert the mechanism is present: the second
    // echo's high-frequency energy is a smaller fraction of its total than the first echo's.
    SushiDSP::Dd3Delay node;
    node.set_mode(SushiDSP::dd3::Dd3Mode::Short);
    node.set_time(0.0f);
    node.set_feedback(0.7f);
    node.set_level(1.0f);
    std::vector<float> input(static_cast<std::size_t>(1.0 * 48000.0), 0.0f);
    for (std::size_t n = 0; n < 480; ++n)      // a 10 ms burst of 3 kHz, well inside the band
    {
        input[n] = 0.6f * static_cast<float>(std::sin(2.0 * M_PI * 3000.0 * static_cast<double>(n) / 48000.0));
    }
    const auto output = SushiDSP::measure::render(node, input, 48000.0, 256u);
    auto window_energy = [&](std::size_t begin, std::size_t length)
    {
        double energy = 0.0;
        for (std::size_t n = begin; n < begin + length && n < output.size(); ++n)
        {
            energy += static_cast<double>(output[n]) * output[n];
        }
        return energy;
    };
    // Echoes land near 668 and 1336 samples (N2's 13.912 ms, twice).
    const double first = window_energy(600u, 600u);
    const double second = window_energy(1268u, 600u);
    EXPECT_GT(first, 0.0);
    EXPECT_LT(second, first);
    std::printf("[ measure ] DD-3 echo energies: first %.6e, second %.6e (ratio %.3f)\n",
                first, second, second / first);
}

TEST(Unit_Dsp, Dd3DelayHoldKeepsRepeatingWithNoInput)
{
    SushiDSP::Dd3Delay node;
    node.set_mode(SushiDSP::dd3::Dd3Mode::Long);
    node.set_time(0.0f);
    node.set_feedback(0.0f);
    node.set_level(1.0f);
    node.prepare({ 48000.0, 256u, 1u });
    // Fill the loop, then hold.
    std::vector<float> tone(static_cast<std::size_t>(0.25 * 48000.0), 0.0f);
    for (std::size_t n = 0; n < tone.size(); ++n)
    {
        tone[n] = 0.5f * static_cast<float>(std::sin(2.0 * M_PI * 500.0 * static_cast<double>(n) / 48000.0));
    }
    std::vector<float> scratch(tone.size(), 0.0f);
    for (std::uint32_t offset = 0; offset + 256u <= tone.size(); offset += 256u)
    {
        const float* block_in[1] = { tone.data() + offset };
        float* block_out[1] = { scratch.data() + offset };
        SushiDSP::ProcessContext context{ block_out, block_in, 256u, 0.0 };
        node.process(context);
    }
    node.set_mode(SushiDSP::dd3::Dd3Mode::Hold);   // Ruling 14: same N, so the loop survives
    std::vector<float> silence(48000u, 0.0f);
    std::vector<float> held(48000u, 0.0f);
    for (std::uint32_t offset = 0; offset + 256u <= 48000u; offset += 256u)
    {
        const float* block_in[1] = { silence.data() + offset };
        float* block_out[1] = { held.data() + offset };
        SushiDSP::ProcessContext context{ block_out, block_in, 256u, 0.0 };
        node.process(context);
    }
    double energy = 0.0;
    for (std::size_t n = 24000; n < held.size(); ++n)
    {
        energy += static_cast<double>(held[n]) * held[n];
    }
    EXPECT_GT(energy, 1e-4);
}

TEST(Unit_Dsp, Dd3DelayParametersRoundTripByNameAndByIndex)
{
    SushiDSP::Dd3Delay node;
    node.set_parameter("time", 0.8f);
    node.set_parameter("feedback", 0.25f);
    node.set_parameter("level", 0.6f);
    node.set_parameter("mode", 3.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("time"), 0.8f);
    EXPECT_FLOAT_EQ(node.get_parameter("feedback"), 0.25f);
    EXPECT_FLOAT_EQ(node.get_parameter("level"), 0.6f);
    EXPECT_FLOAT_EQ(node.get_parameter("mode"), 3.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("nonsense"), 0.0f);
    node.set_parameter(static_cast<std::uint16_t>(0), 0.1f);
    EXPECT_FLOAT_EQ(node.get_parameter("time"), 0.1f);
    node.set_parameter(static_cast<std::uint16_t>(99), 0.1f);   // out of range: a no-op
    EXPECT_FLOAT_EQ(node.get_parameter("time"), 0.1f);
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::dd3_descriptor();
    EXPECT_STREQ(descriptor.id, "sushidsp.dd3");
    EXPECT_EQ(descriptor.parameter_count, 4u);
    EXPECT_EQ(descriptor.ports.input_channels, 1u);
    EXPECT_EQ(descriptor.ports.output_channels, 1u);
}
```

- [ ] **Step 2: `sd build` — expected FAIL. Step 3: write the header and the source,** transcribing
the per-sample loop above. Every buffer — `fs_scratch_`, `wet_scratch_`, `wet_fifo_` — is a
`std::array` of the fixed capacities named above, so nothing on this path can allocate. `prepare()`
prepares the four filter objects, both companders, the converter, the clock and both `Resampler`s:

```cpp
down_.prepare(sample_rate_, dd3::Dd3Clock::kFastRateHz, 1u,
              dd3::Dd3Clock::kSlowRateHz / sample_rate_);
up_.prepare(dd3::Dd3Clock::kFastRateHz, sample_rate_, 8u,
            sample_rate_ / dd3::Dd3Clock::kFastRateHz);
```

then `reset()`. `set_mode` forwards to the converter (which decides whether to clear) and stores the
mode for `get_parameter`.

- [ ] **Step 4: `sd test -s unit`** — every case above except the ones that depend on
`kLevelUnityGain` is green at this point. Build with `kLevelUnityGain = 1.0f` first.

- [ ] **Step 5: Calibrate `kLevelUnityGain` against check point [5] and pin it.** Drive the node
with the service note's own stimulus — a 1 kHz sine at 100 mVp-p, i.e. 0.05 V peak, i.e.
**0.025 full scale** (N7) — with `level = 1.0`, `feedback = 0`, mode M, `time = 0.5`, and measure the
settled wet peak with `level` at 1.0 and the dry subtracted (run once at `level = 0` and once at
`level = 1` and difference the two renders). Check point [5] is 140 mVp-p, i.e. 0.07 V peak, i.e.
`0.07 / 2.0 = 0.035` full scale. Set

```
kLevelUnityGain = 0.035f / <measured wet peak in full-scale units at kLevelUnityGain = 1.0f>
```

and put the measured number, and this arithmetic, in the constant's Doxygen and in the commit
message — the same shape `TubeScreamer::kLevelUnityGain` already uses. Then add the case:

```cpp
TEST(Unit_Dsp, Dd3DelayHitsCheckPointFive)
{
    // Service note ladder, 1 kHz at 100 mVp-p in -> [5] 140 mVp-p of delay signal (dossier 1.0).
    // In this tree's units: 0.025 full scale in, 0.035 full scale of wet out.
    SushiDSP::Dd3Delay wet_node, dry_node;
    for (SushiDSP::Dd3Delay* node : { &wet_node, &dry_node })
    {
        node->set_mode(SushiDSP::dd3::Dd3Mode::Middle);
        node->set_time(0.5f);
        node->set_feedback(0.0f);
    }
    wet_node.set_level(1.0f);
    dry_node.set_level(0.0f);
    const auto input = SushiDSP::measure::sine_stimulus(1000.0, 48000.0, 96000u, 0.025f);
    const auto with_wet = SushiDSP::measure::render(wet_node, input, 48000.0, 256u);
    const auto without = SushiDSP::measure::render(dry_node, input, 48000.0, 256u);
    double peak = 0.0;
    for (std::size_t n = 72000; n < with_wet.size(); ++n)
    {
        peak = std::max(peak, std::abs(static_cast<double>(with_wet[n] - without[n])));
    }
    std::printf("[ measure ] DD-3 check point [5]: %.5f full scale (target 0.035)\n", peak);
    EXPECT_NEAR(peak, 0.035, 0.05 * 0.035);
}
```

- [ ] **Step 6: `sd test -s unit` fully green. Commit**

```bash
git add modules/dd3/include/SushiDSP/dd3/Dd3Delay.hpp modules/dd3/src/Dd3Delay.cpp tests/functional/unit/test_dd3_delay.cpp
git commit -m "DR2: the DD-3 node - variable-rate loop through two Resamplers, feedback around the whole chain, wet delay <x> ms at mode S / TIME 0 against the pedal's 12.5; kLevelUnityGain <y> from check point [5]"
```

---

### Task 2.6: Acceptance rows 1–4, the alias floor, and the golden snapshots

**Files:**
- Modify: `tests/functional/unit/test_dd3_acceptance.cpp` (replace stub)
- Modify: `tests/functional/regression/test_dd3_regression.cpp` (replace stub)
- Create: `tests/golden/dd3_48k.f32`, `tests/golden/dd3_96k.f32`
- Modify: `tests/golden/README.md`

**Depends on:** Task 2.5. Runs concurrently with Task 2.7; the two share no file.

**Where each acceptance row is met.** Rows 2 and 3 are already met by Tasks 2.1 and 2.2 — the
analytic filter goldens and the NE570 static curve and tau are unit-level facts about single blocks,
and testing them again through the whole node would only make the failure harder to read. This task
carries rows 1 and 4, the node-level "0 NaN" and "latency reported and asserted" row, and the
alias-floor number Ruling 7 promised. Every case here uses the `Regression_Dsp` prefix: each renders
seconds of audio, and `docs/CONTRIBUTING.md` §4 keeps `sd test -s unit` a fast edit loop.

- [ ] **Step 1: Row 1 — fs(t) against the published curve, in all three modes.**

```cpp
TEST(Regression_Dsp, Dd3AcceptanceClockCurveMatchesThePublishedRange)
{
    // Acceptance row 1: within 2 % of the published 20-82.5 kHz curve. The check is on fs, which
    // is what the service note actually publishes (a scope on the sample-and-hold); the derived
    // delay times are printed beside it, because N = 1024/4096/16384 and the published times were
    // computed with N = 1000/4000/16000, which is dossier open question 4.
    using Clock = SushiDSP::dd3::Dd3Clock;
    EXPECT_LE(std::abs(Clock::clock_for_time(0.0) - 82500.0) / 82500.0, 0.02);
    EXPECT_LE(std::abs(Clock::clock_for_time(1.0) - 20000.0) / 20000.0, 0.02);

    struct Mode { const char* name; SushiDSP::dd3::Dd3Mode mode; double n; };
    const Mode modes[3] = { { "S", SushiDSP::dd3::Dd3Mode::Short, 1024.0 },
                            { "M", SushiDSP::dd3::Dd3Mode::Middle, 4096.0 },
                            { "L", SushiDSP::dd3::Dd3Mode::Long, 16384.0 } };
    for (const Mode& mode : modes)
    {
        for (const float time : { 0.0f, 0.5f, 1.0f })
        {
            SushiDSP::Dd3Delay node;
            node.set_mode(mode.mode);
            node.set_time(time);
            node.set_feedback(0.0f);
            node.set_level(1.0f);
            node.prepare({ 48000.0, 256u, 1u });
            // The clock the node settled on must be the one the law predicts, to 0.1 %.
            const double predicted_clock = Clock::clock_for_time(time);
            const double predicted_wet = mode.n / predicted_clock
                                       + 64.0 / std::min(predicted_clock, 48000.0)
                                       + 8.0 / 48000.0;
            EXPECT_NEAR(node.wet_delay_seconds(), predicted_wet, 1e-3 * predicted_wet)
                << mode.name << " " << time;
            std::printf("[ measure ] DD-3 mode %s, TIME %.2f: fs %.1f Hz, ring %.3f ms, "
                        "wet %.3f ms\n",
                        mode.name, static_cast<double>(time), predicted_clock,
                        1000.0 * mode.n / predicted_clock, 1000.0 * node.wet_delay_seconds());
        }
    }
}
```

- [ ] **Step 2: Row 4 — the pitch slur.** Trace the wet signal's instantaneous frequency while TIME
sweeps, and assert it glides.

```cpp
TEST(Regression_Dsp, Dd3AcceptancePitchSlursRatherThanJumping)
{
    // Acceptance row 4: "frequency trace under a time-knob sweep shows the R56/C52 glide, not a
    // jump". Drive a steady 1 kHz tone into a HELD loop (so the only thing that can move the
    // pitch is the clock), step TIME from 0 to 1 in one call, and trace zero crossings.
    //
    // N5: a full-scale TIME step moves log2(fs) by log2(4.125) = 2.0443847 octaves with a 0.594 ms
    // time constant, and the clock is advanced once per 8-frame sub-block, so the largest single
    // step in the trace is 24.4657 % of what is left. A jump would move it 100 % in one step.
    SushiDSP::Dd3Delay node;
    node.set_mode(SushiDSP::dd3::Dd3Mode::Long);
    node.set_time(0.0f);
    node.set_feedback(0.0f);
    node.set_level(1.0f);
    node.prepare({ 48000.0, 8u, 1u });

    std::vector<float> tone(static_cast<std::size_t>(0.6 * 48000.0), 0.0f);
    for (std::size_t n = 0; n < tone.size(); ++n)
    {
        tone[n] = 0.5f * static_cast<float>(std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(n) / 48000.0));
    }
    std::vector<float> filled(tone.size(), 0.0f);
    for (std::size_t offset = 0; offset + 8u <= tone.size(); offset += 8u)
    {
        const float* in[1] = { tone.data() + offset };
        float* out[1] = { filled.data() + offset };
        SushiDSP::ProcessContext context{ out, in, 8u, 0.0 };
        node.process(context);
    }
    node.set_mode(SushiDSP::dd3::Dd3Mode::Hold);
    node.set_time(1.0f);                                   // the step the pole has to glide

    std::vector<float> swept(static_cast<std::size_t>(0.05 * 48000.0), 0.0f);
    const std::vector<float> silence(swept.size(), 0.0f);
    for (std::size_t offset = 0; offset + 8u <= swept.size(); offset += 8u)
    {
        const float* in[1] = { silence.data() + offset };
        float* out[1] = { swept.data() + offset };
        SushiDSP::ProcessContext context{ out, in, 8u, 0.0 };
        node.process(context);
    }

    // Instantaneous frequency from successive upward zero crossings.
    std::vector<double> periods;
    std::size_t previous_crossing = 0;
    for (std::size_t n = 1; n < swept.size(); ++n)
    {
        if (swept[n - 1] <= 0.0f && swept[n] > 0.0f)
        {
            if (previous_crossing != 0)
            {
                periods.push_back(static_cast<double>(n - previous_crossing));
            }
            previous_crossing = n;
        }
    }
    ASSERT_GT(periods.size(), 8u);
    // Monotone downward in pitch (the period grows) and no step bigger than the sub-block quantum.
    for (std::size_t i = 1; i < periods.size(); ++i)
    {
        EXPECT_GE(periods[i], periods[i - 1] * 0.98) << i;                 // monotone, with a
                                                                            // half-sample allowance
        EXPECT_LE(periods[i] / periods[i - 1], 1.35) << i;                  // a jump would be 4.125x
    }
    std::printf("[ measure ] DD-3 pitch slur: period %.2f -> %.2f samples over %zu crossings\n",
                periods.front(), periods.back(), periods.size());
    // The glide really did travel: the final pitch is well below the initial one.
    EXPECT_GT(periods.back(), 1.5 * periods.front());
}
```

- [ ] **Step 3: NaN battery, latency assertion, and the alias floor Ruling 7 promised.**

```cpp
TEST(Regression_Dsp, Dd3AcceptanceProducesNoNonFiniteSampleUnderTheStimulusBattery)
{
    // The D6b battery: a 2 s 20 Hz -> 20 kHz sweep at -12 dBFS, plus 1 kHz bursts at 0.05, 0.5
    // and 1.0 full scale, plus a DC step, plus digital silence - each at feedback 0.0, 0.5 and
    // 1.0 (past self-oscillation) in every mode.
    for (const SushiDSP::dd3::Dd3Mode mode : { SushiDSP::dd3::Dd3Mode::Short,
                                              SushiDSP::dd3::Dd3Mode::Middle,
                                              SushiDSP::dd3::Dd3Mode::Long,
                                              SushiDSP::dd3::Dd3Mode::Hold })
    {
        for (const float feedback : { 0.0f, 0.5f, 1.0f })
        {
            SushiDSP::Dd3Delay node;
            node.set_mode(mode);
            node.set_feedback(feedback);
            node.set_level(1.0f);
            node.set_time(0.35f);
            const auto stimulus = /* build the battery, 6 s at 48 kHz */;
            const auto output = SushiDSP::measure::render(node, stimulus, 48000.0, 256u);
            for (std::size_t n = 0; n < output.size(); ++n)
            {
                ASSERT_TRUE(std::isfinite(output[n])) << static_cast<int>(mode) << " "
                                                      << feedback << " " << n;
                ASSERT_LT(std::abs(output[n]), 32.0f) << n;   // a diverging loop fails here first
            }
        }
    }
}

TEST(Regression_Dsp, Dd3AcceptanceReportsZeroLatencyAndDelaysOnlyItsWetPath)
{
    // Ruling 5. The node reports no latency, and the assertion that backs the claim is that the
    // dry path is genuinely sample-aligned: at level 0 the node is bit-close to a wire, which
    // test_dd3_delay.cpp already pins, and the wet delay is reported separately.
    SushiDSP::Dd3Delay node;
    node.prepare({ 48000.0, 256u, 1u });
    EXPECT_EQ(node.latency_samples(), 0u);
    EXPECT_GT(node.wet_delay_seconds(), 0.0);
}

TEST(Regression_Dsp, Dd3AcceptanceAliasFloorIsMeasuredNotAssumed)
{
    // Ruling 7 deferred oversampling of the analog chain and promised a number instead of a
    // paragraph. alias_floor() sweeps 1-20 kHz tones and reports the worst non-harmonic line.
    // The assertion is deliberately loose (-40 dB): the DD-3 is a 12-bit converter behind a
    // 7 kHz wall and is not supposed to be clean. The PRINTED figure is the point - if it comes
    // back worse than -60 dB, Ruling 7 is revisited with data.
    SushiDSP::Dd3Delay node;
    node.set_mode(SushiDSP::dd3::Dd3Mode::Middle);
    node.set_time(0.5f);
    node.set_feedback(0.0f);
    node.set_level(1.0f);
    const double floor_db = SushiDSP::measure::alias_floor(node, 48000.0);
    std::printf("[ measure ] DD-3 alias floor: %.2f dB (Ruling 7 revisit threshold -60 dB)\n", floor_db);
    EXPECT_LE(floor_db, -40.0);
}
```

The `/* build the battery */` comment is the one deliberate gap in this plan's code, and it is a gap
only because the same battery is written out three times in the tree already: copy the construction
from `tests/functional/unit/test_dk_triode_stage.cpp`'s convergence case and scale it to full-scale
sample units instead of volts.

- [ ] **Step 4: The golden snapshots.** In `tests/functional/regression/test_dd3_regression.cpp`,
copy `test_tube_screamer_regression.cpp`'s shape exactly — a `render_dd3(sample_rate)` helper that
sets the knobs **before** `prepare()` so the smoothers snap, then `compare_or_regenerate` at −80 dB:

```cpp
TEST(Regression_Dsp, Dd3GoldenFile48k)
{
    EXPECT_LE(SushiDSP::golden::compare_or_regenerate("dd3_48k", render_dd3(48000.0)), -80.0);
}

TEST(Regression_Dsp, Dd3GoldenFile96k)
{
    EXPECT_LE(SushiDSP::golden::compare_or_regenerate("dd3_96k", render_dd3(96000.0)), -80.0);
}
```

Knobs for the snapshot: `mode = Middle`, `time = 0.4`, `feedback = 0.45`, `level = 0.5` — inside the
loop's stable region, with enough feedback that a change to the compander, the converter or either
resampler moves the file. Generate with `SUSHIDSP_REGENERATE_GOLDEN=1 sd test -s regression`, commit
both `.f32` files, and add a row to `tests/golden/README.md` naming DR2 and these knob positions.

- [ ] **Step 5: `sd test -s all` green. Copy every `[ measure ]` line into the commit. Commit**

```bash
git add tests/functional/unit/test_dd3_acceptance.cpp tests/functional/regression/test_dd3_regression.cpp tests/golden/dd3_48k.f32 tests/golden/dd3_96k.f32 tests/golden/README.md
git commit -m "DR2: acceptance rows 1 and 4 at node level, NaN battery over four modes x three feedback settings, alias floor <x> dB, and the 48k/96k golden snapshots"
```

---

### Task 2.7: RT-safety traps, benchmark, registry, documentation

**Files:**
- Modify: `tests/functional/unit/test_rt_safety.cpp`
- Modify: `tests/benchmark/main.cpp`
- Modify: `modules/node_registry/src/NodeRegistry.cpp`
- Create: `modules/dd3/README.md`
- Modify: `docs/README.md`, `docs/reference/CHANGELOG.md`, `docs/design/sushidsp.md`

**Depends on:** everything. Runs concurrently with Task 2.6.

- [ ] **Step 1: RT traps.** Following the existing `operator new` trap pattern in the file, add three
cases. Each constructs and `prepare`s **outside** the trap window, then arms the trap:

```cpp
TEST(Unit_Dsp, ResamplerProcessPerformsNoHeapAllocation)
TEST(Unit_Dsp, ResamplerRateChangePerformsNoHeapAllocation)
TEST(Unit_Dsp, Dd3DelayProcessPerformsNoHeapAllocation)
```

The second one matters most: `set_rates` is the call the DD-3 makes 32 times per 256-frame block, and
Ruling 3 stands or falls on it allocating nothing. The third arms the trap, runs 256 frames, then
calls `set_mode` with a **different N** (which does the 16 384-sample `std::fill`) and runs another
256 frames, so Ruling 14's clear is inside the trapped window rather than outside it. Assert
`g_trapped_allocation_count == 0` in all three.

- [ ] **Step 2: Benchmark cases.** In `tests/benchmark/main.cpp`, add two cases beside the existing
ones:

```cpp
if (selected("Resampler", argc, argv))
{
    // 48 kHz -> 20 kHz -> 48 kHz, the DD-3's worst case: the decimating stage's kernel spans
    // 64/0.4167 = 154 taps per output.
    print(measure_resampler_round_trip(), 0.10);
}
if (selected("Dd3", argc, argv))
{
    print(measure_node<SushiDSP::Dd3Delay>("DD-3 Delay", [](SushiDSP::Dd3Delay& node)
    {
        node.set_mode(SushiDSP::dd3::Dd3Mode::Middle);
        node.set_time(0.4f);
        node.set_feedback(0.45f);
        node.set_level(0.5f);
    }), 0.15);
}
```

`measure_resampler_round_trip()` follows `measure_dk_circuit()`'s shape: prepare both resamplers
outside the timed body, and time one 256-frame block pushed through `down` then `up`. The 0.10 ms
sub-budget is advisory — it exists so the split between the resamplers and everything else is
visible; **0.15 ms for the node is the binding number** (spec, acceptance table).

Run `sd test -t release -s benchmark`, and put both numbers in the commit message. **If the 0.15 ms
budget is missed**, do not loosen it: report DONE_WITH_CONCERNS with the measured figure and the
profile, and name the levers this plan has already identified — the prototype's half-width (32 zero
crossings, the dominant per-sample cost, and every step down costs passband; N1 has the arithmetic),
`kClockUpdateFrames` (8 sub-blocks' worth of `set_rates` per 8 samples), and the per-sample
`Resampler::process` call in Task 2.5's loop, which could be hoisted to a sub-block push at the cost
of Ruling 11's sample-accurate feedback. Let the controller decide with data.

- [ ] **Step 3: Registry.** In `modules/node_registry/src/NodeRegistry.cpp`, add the include and one
entry:

```cpp
#include "SushiDSP/dd3/Dd3Delay.hpp"
…
            { &dd3_descriptor(), NodeCategory{ "Delay" } },
```

`test_node_registry.cpp` asserts `entries.size() >= 2` and looks its two products up by id, so it
keeps passing unchanged; do not edit it.

- [ ] **Step 4: `modules/dd3/README.md`** — the module's own facts, beside the code: which revision
is modelled (DD-2 / DD-3 v1, long chip, 1986–89) and why (dossier §2); the block list and which class
owns each; the three constants that are calibrations rather than component values
(`kInputPathGainToCheckPointOne`, `kExpanderOutputStageGain`, `kLevelUnityGain`) with the check point
each comes from; the wet-delay formula from N2 and the +1.500 ms floor; and a short "what is not
modelled" list — R-2R differential nonlinearity (Ruling 13), converter overflow wrapping (Ruling 15),
no oversampling of the analog chain (Ruling 7), the DD-3A variant.

- [ ] **Step 5: Manual and changelog.**
  - `docs/README.md`: two bullets in "Where to go" — `include/SushiDSP/math/Resampler.hpp` (what it
    is, that its coefficients are generated by `tools/dsp/design_resampler.py`, and its group-delay
    contract) and `modules/dd3/` (pointing at the module README and at the spec).
  - `docs/reference/CHANGELOG.md`: a DR1 section and a DR2 section, in the existing style. The DR1
    section lists the new header, the generator, the measured stopband and the group-delay contract.
    The DR2 section lists the module, the acceptance results, the benchmark number, the wet-delay
    floor, and — plainly — the three findings this plan turned up that contradict a source: the
    anti-alias cascade's +6.45 dB peak at 7 kHz against the published +1/−3 dB (N3), the `N`
    versus published-delay-time inconsistency (N5), and the unexplained factor of 4.70358 at check
    point [1] (N7).
  - `docs/design/sushidsp.md`: add `math/Resampler` beside `math/Oversampler` in the math-layer
    section, stating that the two solve different problems — fixed integer ratio with halfbands
    versus arbitrary, per-block-variable ratio with one prototype — and add the DD-3 to the product
    list with its spec reference.

- [ ] **Step 6: `sd test -s all` green; `sd doxygen` produces no new warnings for the new headers.
Commit**

```bash
git add tests/functional/unit/test_rt_safety.cpp tests/benchmark/main.cpp modules/node_registry/src/NodeRegistry.cpp modules/dd3/README.md docs
git commit -m "DR1/DR2: RT traps for the resampler and the DD-3, benchmark <a> ms/256 against the 0.15 budget, registry entry, module README, design doc and changelog"
```

---

## No Placeholders

Every code block above is either complete and compilable as written, or is explicitly marked as a
shape whose body the surrounding step describes in prose: Task 1.3 Step 4 (`Resampler.cpp`'s loops,
fully specified in the algorithm block above them), Task 2.1 Step 3 and Task 2.2 Step 3 (the
`Biquad` wiring and the two compander branches, both written out in the interface blocks), Task 2.5
Step 3 (the node's per-sample loop, written out line for line), and Task 2.6 Step 3's
`/* build the battery */`, which the step names an existing file to copy from. Nothing is stubbed
"for later".

`<x>`, `<y>` and `<a>` in commit messages are values produced by the step immediately above them.
`kLevelUnityGain` is the one constant this plan cannot compute on paper — it depends on the settled
gain of a compander inside a feedback loop — and Task 2.5 Step 5 is a numbered procedure that
produces it, calibrates it against a published check point, and pins it with a test, rather than
leaving a magic number to be guessed.

Every expected value in every test is either an analytic closed form, a value hand-computed in the
"Numbers" section with its arithmetic shown, or a published figure quoted from the dossier with its
confidence level. No test asserts the implementation's own output.

## Self-review

- **Spec coverage.** DR1: "arbitrary-ratio polyphase resampler" → Tasks 1.1 and 1.3; "fixed ratio set
  at prepare, all buffers allocated there, process allocation/lock/syscall/throw-free" → Task 1.3's
  interface and Task 2.7 Step 1's traps; "polyphase interpolator over a fixed prototype filter with a
  fractional phase accumulator, not a bank rebuilt per ratio" → Rulings 1–3 and Task 1.3's algorithm;
  "group delay reported through `latency_samples()`" → Task 1.3's `latency_seconds`/`latency_samples`
  pair and its six-row test; "the plan states the number once the prototype is designed" → N1 and N2,
  **+1.500 ms**, Ruling 6. "`math/Oversampler` untouched" → no task names that file. DR2: the block
  chain → Tasks 2.1–2.5 with Ruling 8's order correction; feedback around the whole chain in the
  analog domain, re-companded and re-quantised every pass → Task 2.5's loop and Task 2.5's
  darkening-repeats test; `fs(t)` from the VCO with the R56/C52 slew → Task 2.4 and N5; N = 1024/4096/
  16384 by mode switch, HOLD as a write inhibit, MODE clearing the ring → Task 2.3 with Ruling 14's
  correction; TIME/FEEDBACK/LEVEL through `SmoothedParameter` → Task 2.5, with MODE excepted by
  Ruling 10; "the DK core is not used" → no task includes a `dk/` header. Acceptance rows 1–4 →
  Tasks 2.6 (rows 1, 4), 2.1 (row 2), 2.2 (row 3); "RT traps, 0 NaN, latency reported and asserted" →
  Tasks 2.6 and 2.7; benchmark 0.15 ms/256 → Task 2.7 Step 2, binding.
- **SOLID shape.** Five DD-3 classes, one job each, none of which names another: the filters know
  component values, the compander knows an envelope, the converter knows codes and a ring, the clock
  knows a knob and a pole, and only `Dd3Delay` knows the topology. The four sibling filter classes
  have the same three-method shape, as do the two compander halves (one class, one constructor
  argument, no runtime mode switch). `Resampler` names no consumer and no product; it is in
  `math/`, depends only on the standard library and its own generated table, and DR3 consumes it
  unchanged.
- **Wave discipline.** Every file in this plan is written by exactly one task. The three files that
  more than one phase would naturally touch — `tests/CMakeLists.txt`, `tests/functional/unit/
  test_rt_safety.cpp`, `tests/benchmark/main.cpp` — are handled by Task 0 (CMake, before any wave)
  and Task 2.7 (traps and benchmark, after every wave), which is why DR1 has no trap task of its own.
- **Where the risk is.** Task 2.5 is the largest single task and the only one whose correctness
  depends on five other tasks at once. Its most legible failure is the wet-delay test: a single
  number, predicted to six decimals in N2, that moves if any of `N`, `fs`, either resampler's group
  delay, or the FIFO prime is wrong. Run that case before the others. The second risk is the
  benchmark: the decimating resampler touches `64/0.4167 = 154` taps per output at the slow clock,
  and the budget is 586 ns per output sample (`0.15 ms / 256`), which is roomy but was not measured
  while writing this plan.

### Open questions for the controller

1. **The anti-alias cascade contradicts the published bandwidth (N3).** The three component-derived
   sections cascade to **+6.45 dB at 7 kHz** and **−6.56 dB at 10 kHz**, against a published delay
   path of 40 Hz–7 kHz, +1/−3 dB. The plan implements the component values as written, because that
   is what acceptance row 2 asks for, and prints the result. Either a capacitor reading is wrong
   (dossier confidence B) or the emitter follower's finite gain damps the high-Q sections. Resolution
   is dossier §4.7 item 1, a swept sine on a real unit. **Should DR2 ship the peaking cascade, or
   should the Q values be damped to meet the published figure?**
2. **`N = 1024/4096/16384` versus the published delay times (N5).** The two are inconsistent by
   +2.40 % at the long end of every range. The spec binds `N`, and the DRAM arithmetic
   (`16384 × 12 = 196 608 bits`) supports it; the published times are exact for `N = 1000/4000/16000`.
   The plan keeps the spec's `N` and asserts acceptance row 1 on `fs`, where it passes. **Confirm, or
   switch to 1000/4000/16000 and accept that the memory no longer divides evenly.**
3. **The unexplained 4.70358 at check point [1] (N7).** The transcribed component values give a
   1 kHz pre-emphasis output of 131.8 mVp-p where the service note publishes 620 mVp-p. The plan
   carries the residual as one named constant with a Doxygen block saying exactly that. **Accept the
   named residual, or hold DR2 until the input stage's gain is resolved from the scan?**
4. **Ruling 9, the de-emphasis leg.** Read literally, the dossier's schematic shelves the dry branch
   by −15.12 dB above 3.4 kHz. The plan moves the shelf into the wet leg so the dry path stays
   transparent. **Confirm** — it is an inference from how the pedal is known to sound, not something
   read off the page, and dossier §5 open question 2 flags the same network.
5. **Ruling 12 adds a public method to a core brick.** `Biquad::set_first_order_shelf` is a new
   public interface on `math/Biquad`, which is shared by every product. Global CLAUDE.md says to ask
   before deciding public interface shape. **Confirm the signature**
   `set_first_order_shelf(double sample_rate, double zero_hz, double pole_hz, double low_frequency_gain)`,
   or say where else it should live.
6. **Ruling 6, the resampler floor.** +1.500 ms at the fast end, taking the shortest delay from
   12.412 ms to 13.912 ms (+11.30 %). The spec accepts it. Reading the ring at `N − d` would cancel
   it exactly, and N2 gives the numbers (`d = 110` samples at the fast end, leaving a 914-sample
   read offset). **Ship the floor, or compensate?**
7. **Ruling 7, no oversampling.** Deferred with a reason and a measurement (Task 2.6's alias floor)
   rather than a promise. **If the printed floor is worse than −60 dB, does DR2 add a 2× stage — and
   accept another 1.19 ms on the wet path and a larger share of the 0.15 ms budget — or does the
   number stand as the pedal's character?**
8. **Two defaults that are free choices.** The DD-3's default MODE (this plan picks Middle) and the
   `kMaximumFeedback = 1.1` mapping that puts the pot's top just past self-oscillation. Both are
   one-line edits; neither is derived from anything.
9. **The dossier's own blocking questions are not resolved by this plan** and are carried forward as
   they stand: whether the F.BACK/E.LEVEL takeoff is before or after the reconstruction filter
   (§5 Q1 — the plan takes reading (a), post-filter, which the dossier calls near-certain on
   electrical grounds), and whether the SAR wraps or saturates on overflow (§5 Q3 — Ruling 15 takes
   saturation). Both are one-line changes in `Dd3Delay` and `Dd3Converter` respectively if a
   measurement ever settles them.
