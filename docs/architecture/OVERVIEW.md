# Architecture overview

This page describes SushiDSP's layering as built, verified against the tree under
`include/SushiDSP/`, `src/`, `modules/` and `apps/`. It is not the design spec: where a claim
comes from intent rather than code, it says so and cites
[`docs/design/sushidsp.md`](../design/sushidsp.md) by section instead of restating the intent
as fact.

## The four layers

```
apps/                 host, host_asio, host_gui — own IAudioDevice, drive the rig graph
  ↓
modules/<product>/     jcm800, tubescreamer, dd3, peavey5150, mesa_mark2c, diezel_vh4,
                        room, routing, capture, rig_preset, node_registry
  ↓
include+src: core/     IAudioNode, RigGraph, NodeDescriptor, BufferPool, IAudioDevice,
                        SpscRing — the portable graph plumbing
  ↓
include+src: math/     Biquad, Fft, Resampler, Oversampler, Wdf, dk/ — the DSP kernels
             circuits/ FmvToneStack, GraphicEqualizer, models/, stages/ — reusable circuit blocks
```

`device/`, `io/` and `audio_format/` sit beside `core/` at the same tier: `device/` (
`AsioAudioDevice`, `Sdl2AudioDevice`) implements `core/IAudioDevice.hpp`; `io/` (`Ir.hpp`,
`IrLoader.hpp`, `WavFile.hpp`, `WavWriter.hpp`) and `audio_format/`
(`AsioSampleConverter.hpp`, `ChannelSelection.hpp`) supply device- and cabinet-adjacent
utilities that products and hosts both use. None of the four sit above `core/` or `math/` in
the dependency order; a lower tier never includes a higher one.

## Core interfaces (`include/SushiDSP/core/`)

`IAudioNode.hpp` declares `prepare()`/`process()`/`ports()`/`reset()`, the one interface every
processing block in the tree implements — a product module, a routing node, or the graph
itself. `RigGraph.hpp` holds a `std::vector` of nodes and topologically sorts and replays them
per block; `NodeDescriptor.hpp` is the POD factory contract (id, name, parameters, ports,
`make()`) a product exposes so a host can instantiate it without knowing its concrete type.
`IAudioDevice.hpp` is the device seam `device/AsioAudioDevice.hpp` and
`device/Sdl2AudioDevice.hpp` implement. `BufferPool.hpp`, `RtGraphSlot.hpp`, `RtSwapSlot.hpp`
and `SpscRing.hpp` are the RT-safe plumbing underneath: block-sized buffer reuse, and the
control-thread-to-audio-thread handoff for a recompiled graph or a parameter change. This
matches the interfaces design spec §3 describes; the header contents above are what is
actually checked into `include/SushiDSP/core/` today.

## Math and circuit kernels (`include/SushiDSP/math/`, `include/SushiDSP/circuits/`)

`math/` holds the portable numerical kernels — `Biquad`, `Fft`, `Oversampler`,
`PartitionedConvolution`, `StreamingConvolution`, `Resampler`/`SincResampler`, `Wdf`,
`SmoothedParameter` — and `math/dk/` holds the DK-method nonlinear-circuit core:
`DenseLu`, `DampedNewtonSolver`, `DkNetlist`, `DkCircuit<S, N, I, O>` (see
[`docs/README.md`](../README.md) for the per-header description). `circuits/` holds
`FmvToneStack.hpp` and `GraphicEqualizer.hpp` alongside `circuits/models/` (the device laws:
`SiliconDiode`, `Triode12AX7`, `PentodeEL34`) and `circuits/stages/`. Everything in `math/` and
`circuits/` depends on the C++ standard library only, never on `core/` — a product module pulls
kernels down from here, never the reverse.

## Products (`modules/`)

Each of `jcm800`, `tubescreamer`, `dd3`, `peavey5150`, `mesa_mark2c` and `diezel_vh4` is a
library that composes `math`/`circuits` kernels behind one `IAudioNode` implementation and
exposes exactly one `NodeDescriptor`. `routing` supplies the rig graph's own
`SplitterNode`/`MixerNode`/`PanNode` as ordinary products rather than `RigGraph` special
cases; `capture` and `room` are DS/D-phase modules the same way. `node_registry` is the one
module allowed to depend on every product at once — `node_registry()` and `make_node()` are
the id-to-instance catalog every host in `apps/` builds a rig from today, so adding a product
means adding one entry there and touching nothing in `apps/`. A future plugin wrapper would
draw from the same catalog (see "What is spec-only, not yet built" below), but none exists in
the tree yet. `rig_preset` serializes
a rig (descriptor id plus parameter values) to and from JSON. This is the dependency rule
design spec §8 states for `modules/`; what is verified here is that `modules/catalog/node_registry/`
and `modules/session/rig_preset/` exist in the tree and match that description, alongside the six
product modules and `routing`/`capture`/`room`.

## Hosts (`apps/`)

`apps/host` is a minimal SDL2-backed standalone host; `apps/host_asio` is the ASIO-backed
counterpart; `apps/host_gui` is the full rig-editing GUI, with its own audio import, resampling
and MP3-decode support classes alongside `RigController` and the UI tree under
`apps/host_gui/ui/`. All three sit above `modules/` and `core/`: they own an `IAudioDevice`,
build a `RigGraph` from `node_registry()`, and drive `process()` per audio callback. A VST/CLAP
plugin wrapper is design intent only — design spec §3 ("Product contract") and §8's `apps/plugin`
layout entry describe it, but no `apps/plugin` directory exists in the tree yet.

## What is spec-only, not yet built

The following are described in `docs/design/sushidsp.md` as intent and were not found under
`include/`, `src/`, `modules/` or `apps/` during this review — read them from the spec, not as
current fact:

- The VST3/CLAP plugin wrapper and its `apps/plugin` directory (design spec §3, §8).
- Any packaging step beyond what `sd build`/`sd test` already exercise (design spec §8,
  "Targets & dependency rules").
- Roadmap phases beyond what the changelog records as shipped (design spec §9); check
  [`docs/reference/CHANGELOG.md`](../reference/CHANGELOG.md) for what has actually landed.
