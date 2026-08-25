# CPU performance pass — implementation plan (2026-08-25)

Goal: make the DSP core cheap on the audio thread without changing what it sounds like. Every
step is measured with the benchmark harness added in Task 1, and every step must keep the
regression snapshots (`tests/functional/regression/*`, 1e-5 tolerance) green. Work proceeds
in leverage order; a step that measures as a wash is dropped, not kept for its own sake.

Deliberately out of scope, written down so it is not silently dropped:

- `-ffast-math` / `/fp:fast`: reassociation would move the pinned regression values and make
  the Newton residuals compiler-dependent. Contraction (FMA) is allowed only where the flag is
  explicit and the snapshots still pass.
- Cross-sample SIMD inside the tube stages: every stage is a warm-started serial recurrence,
  there is no independent lane to vectorize. The only loop shaped for SIMD is the FIR tap sum.

## Tasks

- [x] **1. Benchmark harness** — `bench/` target `sushidsp_bench` (chrono only, no external
  dependency), `sd bench` command building Release by default. Cases: JCM800 block,
  TubeScreamer block, Oversampler8x round-trip, TriodeStage, PartitionedConvolution. Reports
  ns/sample and the fraction of a 48 kHz real-time budget each case consumes.
- [x] **2. Optimized build flags** — `cmake/SushiDSPCompilerFlags.cmake`: `/O2 /arch:AVX2` or
  `-O3 -mavx2 -mfma` on Release/RelWithDebInfo, IPO/LTO when the toolchain supports it, both
  behind options that default on. Debug untouched.
- [x] **3. Denormal guard** — `core/DenormalGuard.hpp` RAII FTZ/DAZ, engaged at the top of every
  device callback (SDL2 now; the ASIO backend must do the same when it lands) and in the bench.
- [x] **4. Fast exp/tanh** — `math/FastMath.hpp`, bounded-range `fast_exp` with a golden test
  against `std::exp`; swapped into the Triode/Pentode/TubeScreamer residuals and the two tanh
  sites. Regression snapshots must stay within 1e-5.
- [x] **5. Newton early exit** — stop iterating once the step is below a relative tolerance,
  instead of always running 4 iterations.
- [x] **6. Polyphase halfband** — circular delay line, skip the zero-valued even taps. Same
  output to float precision; unit tests pin the equivalence.
- [ ] **7. SIMD FIR tap sum** — only if the profile after Task 6 still shows the FIR as
  significant and the compiler's autovectorization is not already doing the job.
- [ ] **8. Record results** — final before/after table in this file and a changelog entry.

## Results

Windows 10, MSVC (Visual Studio generator), Release, 48 kHz / 256-frame blocks, best of 200.
Measured with `sd bench`. "% core" is the share of one core needed to keep up in real time.

| Case                             | Baseline ns/sample | % core |
|----------------------------------|-------------------:|-------:|
| Jcm800 (no cabinet)              | 2819               | 13.53  |
| TubeScreamer                     | 646                | 3.10   |
| Oversampler8x round trip         | 143                | 0.68   |
| TriodeStage (base rate)          | 58                 | 0.28   |
| PartitionedConvolution (4096 IR) | 56                 | 0.27   |

Reading the baseline: the oversampler is already cheap, the cost is the Newton solves and
their `std::exp` calls (6 solves x 4 iterations x 2 exp x 8 oversampled samples per output
sample in Jcm800). Tasks 4 and 5 are therefore the ones that matter; Tasks 6 and 7 are
bounded at under 1 % of a core even if they were free.

### Task 2: build flags (AVX2 + LTO)

Jcm800 2933 ns/sample, TubeScreamer 668, Oversampler 139 — no change beyond run-to-run noise
(about +-5 %). Verified the flags reached the objects (`/arch:AVX2` and whole-program
optimization present in the generated projects). The result is expected: the hot loops are
serial warm-started recurrences with nothing to vectorize, and the per-sample stage calls were
already inlined within their translation units. Kept anyway, since Tasks 6-7 need the ISA and
the flags cost nothing; the regression snapshots stayed within 1e-5.

### Task 3: denormal guard

No change on the benchmark signal (a steady 220 Hz tone never reaches the denormal range in
256 frames), which is the expected result; the guard exists for the silence tail, where a
biquad or delay line decaying through 1e-38 would otherwise cost a microcode assist per
operation. Engaged in `Sdl2AudioDevice::sdl_callback` and in the bench. The ASIO backend
being written in parallel must engage one at the top of its buffer-switch callback too.

### Task 4: fast exp/tanh — rejected after measurement

A range-reduced polynomial `exp` was written, golden-tested (1.7e-7 relative error), and
swapped into the three residuals and two tanh sites. Two results killed it:

- It was slower. In a serial-dependency microbenchmark on this toolchain MSVC's `expf` costs
  8.3 ns; the best variant tried (Cephes degree-5 polynomial, Estrin evaluation, SSE round)
  costs 9.0 ns, a plain Horner form 14-16 ns. MSVC's scalar `expf` is already near the
  latency floor; Jcm800 measured 2930 -> 4260 ns/sample with the approximation in place.
- The Jcm800 snapshot moved by about 2e-4, twenty times the 1e-5 tolerance, from a 1.7e-7
  relative perturbation of `exp`. The preamp/PI/power cascade amplifies float-level
  differences at V1a to 1e-4 at the output, so that snapshot pins the exact float arithmetic
  path, not just the behaviour. Any future change to `exp` (a different libm, a SIMD exp for a
  vectorized stage) will need the tolerance policy the test itself states: relax, don't
  re-bake.

The approximation was removed rather than kept as dead code. A SIMD `exp` remains the right
tool if a stage ever gains independent lanes to fill; none does today.

Also in this task: `NewtonRaphsonSolver::solve_joint` evaluates residual and derivative in
one callable so the shared `exp` is computed once per iteration by construction. Measured as
no change — MSVC was already common-subexpression-eliminating the two identical `exp` calls
across the inlined lambdas — but the sharing is now explicit rather than a compiler favour,
and the two solver forms are pinned bit-identical by a unit test.

### Task 5: Newton fixed-point early exit

`solve_joint` stops when the clamped next iterate equals the current one exactly: a float
fixed point, after which every further iteration would recompute the same value. Output is
bit-identical by construction and the regression snapshots confirm it.

| Case                     | Before | After | Change |
|--------------------------|-------:|------:|-------:|
| Jcm800 (no cabinet)      | 2920   | 1872  | -36 %  |
| TubeScreamer             | 667    | 633   | -5 %   |
| TriodeStage (base rate)  | 62     | 30    | -52 %  |

The TubeScreamer's antiparallel-diode solve rarely lands on an exact fixed point (it
oscillates by an ulp), so it gains little from the exact criterion.

### Task 6: polyphase halfband over a ring buffer

Upsampler: the two output phases of a zero-stuffed input only touch the even- and odd-indexed
taps, so each phase is now a dot product over the base-rate history alone; the skipped
products were exact zeros and the surviving terms are summed in the original order, so the
output is bit-identical (pinned by a unit test against the old direct form, three tap
counts, split calls). Downsampler: ring buffer replaces the per-sample O(taps) shift; every
tap is still applied because the even-offset taps of a float-designed windowed sinc are near
zero, not zero, and dropping them would move the output.

| Case                     | Before | After | Change |
|--------------------------|-------:|------:|-------:|
| Oversampler8x round trip | 139    | 82    | -41 %  |
| Jcm800 (no cabinet)      | 1872   | 1780  | -5 %   |
| TubeScreamer             | 633    | 577   | -9 %   |
