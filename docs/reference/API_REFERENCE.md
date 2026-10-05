# SushiDSP {#mainpage}

A portable, dependency-free C++17 amp/pedal DSP framework — a rig graph of `IAudioNode`s, white-box
Wave Digital Filter circuit models, and the math library underneath them. No SushiRuntime, no SYCL,
no game engine: the `sushi_dsp` core links nothing beyond the C++17 standard library and SIMD
intrinsics, so a plugin built on it ships without a GPU toolchain.

This is the generated API reference. It documents what the headers say; the reasoning behind the
design lives in the prose docs, and those are the better place to start:

- `docs/README.md` — the manual index; `docs/getting_started/INTRODUCTION.md` says how to build.
- `docs/design/SUSHIDSP.md` — the design spec: architecture, math, and the product roadmap.
- `docs/CONTRIBUTING.md` — the build rules (`sd build`, never cmake directly) and coding conventions.

## Where to start reading

| If you want to | Read |
|---|---|
| Build a rig graph | `SushiDSP::RigGraph`, `SushiDSP::IAudioNode` — `RigGraph.hpp`, `IAudioNode.hpp` |
| Register a product | `SushiDSP::NodeDescriptor` — `NodeDescriptor.hpp` |
| Do FFT / convolution / filtering | `SushiDSP::Fft`, `SushiDSP::PartitionedConvolution`, `SushiDSP::Biquad` — `math/` |
| Model a nonlinear circuit stage | `SushiDSP::WdfResistor`, `SushiDSP::WdfCapacitor`, `SushiDSP::NewtonRaphsonSolver` — `math/Wdf.hpp`, `math/NewtonRaphsonSolver.hpp` |
| Play audio out a real device | `SushiDSP::IAudioDevice`, `SushiDSP::Sdl2AudioDevice` — `core/IAudioDevice.hpp`, `device/Sdl2AudioDevice.hpp` |
