# SushiDSP {#mainpage}

A portable, dependency-free C++17 amp/pedal DSP framework, a rig graph of `AudioNode`s, white-box
Wave Digital Filter circuit models, and the math library underneath them. No SushiRuntime, no SYCL,
no game engine: the `sushi_dsp` core links nothing beyond the C++17 standard library and SIMD
intrinsics, so a plugin built on it ships without a GPU toolchain.

This is the generated API reference. It documents what the headers say; the reasoning behind the
design lives in the prose docs, and those are the better place to start:

- `docs/README.md`, the manual index; `docs/getting_started/INTRODUCTION.md` says how to build.
- `docs/design/SUSHIDSP/README.md`, the design spec: architecture, math, and the product roadmap.
- `docs/CONTRIBUTING.md`, the build rules (`sd build`, never cmake directly) and coding conventions.

## Where to start reading

| If you want to | Read |
|---|---|
| Build a rig graph | `SushiDSP::RigGraph`, `SushiDSP::AudioNode`, `rig_graph.hpp`, `audio_node.hpp` |
| Register a product | `SushiDSP::NodeDescriptor`, `node_descriptor.hpp` |
| Do FFT / convolution / filtering | `SushiDSP::Fft`, `SushiDSP::PartitionedConvolution`, `SushiDSP::Biquad`, `math/` |
| Model a nonlinear circuit stage | `SushiDSP::WdfResistor`, `SushiDSP::WdfCapacitor`, `SushiDSP::NewtonRaphsonSolver`, `modules/foundation/math/include/SushiDSP/foundation/math/wdf.hpp`, `modules/foundation/math/include/SushiDSP/foundation/math/newton_raphson_solver.hpp` |
| Play audio out a real device | `SushiDSP::AudioDevice`, `SushiDSP::Sdl2AudioDevice`, `modules/foundation/core/include/SushiDSP/foundation/core/audio_device.hpp`, `modules/foundation/device/include/SushiDSP/foundation/device/sdl2_audio_device.hpp` |
