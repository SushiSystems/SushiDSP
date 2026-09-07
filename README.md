# SushiDSP

A portable C++17 amp/pedal DSP framework — the shared engineering behind a standalone rig of
white-box amp and pedal models.

Most amp-sim libraries either ship a neural approximation trained on a reference capture, or
wire their DSP straight into a specific plugin SDK. This one goes the other way: every nonlinear
stage is a physically derived circuit model — Wave Digital Filters and Newton–Raphson-solved
diode/triode equations, not a trained network — and the DSP core has **zero dependency on any
plugin format, game engine, or GPU runtime**. It depends on nothing but the C++17 standard
library and SIMD intrinsics, so a VST built on it ships without dragging in a SYCL toolchain or
an engine. A standalone `IAudioNode` graph — the **rig** — lets a user chain any number of
pedal/amp/cabinet models in any order, the way a real pedalboard works:

```
SushiDSP  ──consumed by──▶  standalone host / VST3 / CLAP plugin
```

## What is built

Everything below is in the tree today.

**Rig graph.** A directed-acyclic `IAudioNode` graph (`RigGraph`), topologically sorted and
replayed per block from an allocation-free transient buffer pool. Compiled/recompiled off the
real-time thread; the audio thread only ever replays the current compiled graph.

**Math library.** A from-scratch DSP kernel set: a real FFT, uniform-partitioned overlap-add
convolution (for cabinet IRs), an RBJ biquad, a windowed-sinc halfband 2x cascade (for 8x
oversampling), and the shared nonlinear-circuit core every product's amp/pedal stage builds on —
Wave Digital Filter one-ports (`WdfResistor`, `WdfCapacitor`) and a generic Newton–Raphson
solver, proven end to end by a one-stage diode-clipper circuit.

**Device seam + standalone host.** `IAudioDevice` is injected (SDL2 today; WASAPI/CoreAudio/ALSA
behind the same seam later), so the DSP core never assumes a device exists. `sushidsp_host`
opens it, builds a rig, and plays audio out real hardware. A lock-free `SpscRing<T>` carries
parameter/rig-edit commands from the control thread to the audio callback.

**Determinism and safety.** `IAudioNode::process()` — and everything called from it — must be
allocation-free, lock-free, syscall-free, and throw-free. That bar is enforced by an
`operator new`-trapping regression test, not left to code review — today that covers `RigGraph`,
`Fft`, `Oversampler8x`, `PartitionedConvolution`, and `DkCircuit`
(`tests/functional/unit/test_rt_safety.cpp`); every class destined for the real-time path gets a
case there before it's trusted (see `docs/CONTRIBUTING.md`).

## Where it is today

This is a DSP library you drive from C++, not a finished pedal you can plug in this afternoon.
No product ships yet: Tube Screamer (the simpler, single-nonlinear-stage circuit, chosen first
to validate the solver core) and JCM800 are both designed in
[the spec](docs/design/sushidsp.md) and not yet built. There is no VST3/CLAP wrapper. The Wave
Digital Filter series/parallel adaptor tree — needed to combine more than one nonlinear element
into an arbitrary circuit topology — is deliberately not built either: nothing in the tree yet
needs it, and inventing one without a real circuit to validate it against risks getting the
math wrong with no way to catch it.

None of that is hidden — the two products are the next two roadmap phases (D3, D4), and the
plugin wrapper is D6, all in [the design spec's §9](docs/design/sushidsp.md).

## Getting started

The toolchain and every shared dependency live in the **SushiStack** workspace repository.
There is nothing to install system-wide beyond the CLI itself.

```powershell
irm https://sushisystems.io/install.ps1 | iex          # Windows (PowerShell)
```

```bash
curl -fsSL https://sushisystems.io/install.sh | bash   # Linux
```

```bash
hub add sushidsp             # clone this repository into the workspace
hub install-cli sushidsp     # install the `sd` command line tool
```

Then build and run the tests:

```bash
sd build
sd test
```

If the suite passes, the tree is healthy. SushiDSP builds and tests standalone too — clone it
outside any SushiStack workspace and `sd build`/`sd test` still work unchanged; SushiStack
membership buys shared tooling (§11a of the spec), not a code dependency.

Every build action goes through the `sd` command line tool, never `cmake`/`ctest` directly.

## Requirements

- **CMake** and a C++17 compiler (MSVC via the Visual Studio generator on Windows, or any
  C++17-capable compiler elsewhere) — resolved from PATH, or pinned in `cli/config.local.toml`.
- **GoogleTest** and **SDL2** are both fetched automatically via CMake `FetchContent` — nothing
  to install for them. Set `-DSUSHIDSP_BUILD_SDL2=OFF` to configure/test without ever fetching
  SDL2 (the unit suite links nothing from it).
- **Doxygen** + Graphviz, optional, only for `sd doxygen` (the generated API reference).

## Build options

| Option | Default | Gates |
| --- | --- | --- |
| `SUSHIDSP_BUILD_SDL2` | ON | the SDL2 FetchContent fetch, the `sushi_dsp_sdl2` target, and `apps/host` |

## Repository layout

```
include/SushiDSP/
  core/       rig graph, IAudioNode, NodeDescriptor, buffer pool, SpscRing, IAudioDevice
  math/       fft, convolution, filters, resamplers, wdf, complex, windows, simd
  device/     the SDL2 IAudioDevice implementation
  circuits/   nonlinear-circuit building blocks proven ahead of a product needing them
src/          core/math/device/circuits implementations (portable C++/SIMD, no SYCL)
modules/      ← the separable PRODUCTS (not yet built), each a library exposing a NodeDescriptor
apps/
  host/       the standalone rig host (SDL2, IAudioDevice)
tests/
  common/     the GTest entry point
  functional/unit/   golden-vector + RT-safety tests (Unit_Dsp.*)
cli/          the `sd` command line tool
docs/         the design spec, contribution rules, and reference docs
```

## Documentation

[`docs/README.md`](docs/README.md) is the manual's index. The entries worth knowing by name:

- [Design](docs/design/sushidsp.md) — the architecture, the math, and the product roadmap. A
  record of intent, so it may describe work that does not exist yet.
- [Contributing](docs/CONTRIBUTING.md) — the conventions a change is held to.
- [Changelog](docs/reference/CHANGELOG.md) — what changed and when.
- [Code of Conduct](docs/CODE_OF_CONDUCT.md) · [Security Policy](docs/SECURITY.md).

`docs/superpowers/plans/` holds the implementation plan for each roadmap phase, a historical
record once that phase merges — see `docs/README.md` for how to read one.

## License

Apache License, Version 2.0. See [`LICENSE`](LICENSE).
