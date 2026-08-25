# Changelog

Grouped by roadmap phase (spec §9). Each phase's implementation plan lives under
`docs/superpowers/plans/` with the full task-by-task record.

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
