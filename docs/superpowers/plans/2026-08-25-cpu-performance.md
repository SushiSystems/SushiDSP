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
- [ ] **2. Optimized build flags** — `cmake/SushiDSPCompilerFlags.cmake`: `/O2 /arch:AVX2` or
  `-O3 -mavx2 -mfma` on Release/RelWithDebInfo, IPO/LTO when the toolchain supports it, both
  behind options that default on. Debug untouched.
- [ ] **3. Denormal guard** — `core/DenormalGuard.hpp` RAII FTZ/DAZ, engaged at the top of every
  device callback (SDL2 now; the ASIO backend must do the same when it lands) and in the bench.
- [ ] **4. Fast exp/tanh** — `math/FastMath.hpp`, bounded-range `fast_exp` with a golden test
  against `std::exp`; swapped into the Triode/Pentode/TubeScreamer residuals and the two tanh
  sites. Regression snapshots must stay within 1e-5.
- [ ] **5. Newton early exit** — stop iterating once the step is below a relative tolerance,
  instead of always running 4 iterations.
- [ ] **6. Polyphase halfband** — circular delay line, skip the zero-valued even taps. Same
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
