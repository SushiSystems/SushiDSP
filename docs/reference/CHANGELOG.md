# Changelog

Grouped by roadmap phase (spec §9). Each phase's implementation plan lives under
`docs/superpowers/plans/` with the full task-by-task record.

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
