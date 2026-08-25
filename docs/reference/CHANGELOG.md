# Changelog

Grouped by roadmap phase (spec §9). Each phase's implementation plan lives under
`docs/superpowers/plans/` with the full task-by-task record.

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
