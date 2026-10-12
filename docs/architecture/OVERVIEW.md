# Architecture overview

This page describes SushiDSP's layering as built, verified against the tree under
`include/SushiDSP/`, `src/`, `modules/` and `applications/`. It is not the design spec: where a claim
comes from intent rather than code, it says so and cites
[`docs/design/SUSHIDSP.md`](../design/SUSHIDSP.md) by section instead of restating the intent
as fact.

## The four layers

```
applications/                 host, host_asio, host_gui, own AudioDevice, drive the rig graph
  ↓
modules/session/       rig_preset
  ↓
modules/catalog/       node_registry
  ↓
modules/nodes/         jcm800, tubescreamer, dd3, peavey5150, mesa_mark2c, diezel_vh4,
                        cabinet, room, routing, capture
  ↓
include+src: core/     AudioNode, RigGraph, NodeDescriptor, BufferPool, AudioDevice,
                        SpscRing, the portable graph plumbing
  ↓
include+src: math/     Biquad, Fft, Resampler, Oversampler, Wdf, dk/, the DSP kernels
             circuits/ FmvToneStack, GraphicEqualizer, models/, stages/, reusable circuit blocks
```

`device/`, `io/` and `audio_format/` sit beside `core/` at the same tier: `device/` (
`AsioAudioDevice`, `Sdl2AudioDevice`) implements `modules/foundation/core/include/SushiDSP/core/audio_device.hpp`; `io/` (`ir.hpp`,
`ir_loader.hpp`, `wav_file.hpp`, `wav_writer.hpp`) and `audio_format/`
(`asio_sample_converter.hpp`, `channel_selection.hpp`) supply device- and cabinet-adjacent
utilities that products and hosts both use. None of the four sit above `core/` or `math/` in
the dependency order; a lower tier never includes a higher one.

`log/` (`logger.hpp`, `log.hpp`, the sinks) is the library's logger. It depends on the C++
standard library only, and every module and host may include it. The audio thread never calls
it; [`modules/foundation/README.md`](../../modules/foundation/README.md) says what it does instead.

## Core interfaces (`modules/foundation/core/include/SushiDSP/core/`)

`audio_node.hpp` declares `prepare()`/`process()`/`ports()`/`reset()`, the one interface every
processing block in the tree implements, a product module, a routing node, or the graph
itself. `rig_graph.hpp` holds a `std::vector` of nodes and topologically sorts and replays them
per block; `node_descriptor.hpp` is the POD factory contract (id, name, parameters, ports,
`make()`) a product exposes so a host can instantiate it without knowing its concrete type.
`audio_device.hpp` is the device seam `modules/foundation/device/include/SushiDSP/device/asio_audio_device.hpp` and
`modules/foundation/device/include/SushiDSP/device/sdl2_audio_device.hpp` implement. `buffer_pool.hpp`, `rt_graph_slot.hpp`, `rt_swap_slot.hpp`
and `spsc_ring.hpp` are the RT-safe plumbing underneath: block-sized buffer reuse, and the
control-thread-to-audio-thread handoff for a recompiled graph or a parameter change. This
matches the interfaces design spec §3 describes; the header contents above are what is
actually checked into `modules/foundation/core/include/SushiDSP/core/` today.

## Math and circuit kernels (`modules/foundation/math/include/SushiDSP/math/`, `modules/foundation/circuits/include/SushiDSP/circuits/`)

`math/` holds the portable numerical kernels, `Biquad`, `Fft`, `Oversampler`,
`PartitionedConvolution`, `StreamingConvolution`, `Resampler`/`SincResampler`, `Wdf`,
`SmoothedParameter`, and `math/dk/` holds the DK-method nonlinear-circuit core:
`DenseLu`, `DampedNewtonSolver`, `DkNetlist`, `DkCircuit<S, N, I, O>`. `circuits/` holds
`fmv_tone_stack.hpp` and `graphic_equalizer.hpp` alongside `circuits/models/` (the device laws:
`SiliconDiode`, `Triode12AX7`, `PentodeEL34`) and `circuits/stages/`. Everything in `math/` and
`circuits/` depends on the C++ standard library only, never on `core/`, a product module pulls
kernels down from here, never the reverse.

## Products (`modules/`)

Each of `jcm800`, `tubescreamer`, `dd3`, `peavey5150`, `mesa_mark2c` and `diezel_vh4` is a
library that composes `math`/`circuits` kernels behind one `AudioNode` implementation and
exposes exactly one `NodeDescriptor`. `cabinet` is the impulse-response convolution the
amplifiers used to carry themselves, now a node of its own. `routing` supplies the rig graph's own
`SplitterNode`/`MixerNode`/`PanNode` as ordinary products rather than `RigGraph` special
cases; `capture` and `room` are DS/D-phase modules the same way. `node_registry` is the one
module allowed to depend on every product at once, `node_registry()` and `make_node()` are
the id-to-instance catalog every host in `applications/` builds a rig from today. Adding a product
adds one entry there; the host GUI still names products by id in
`applications/host_gui/ui/panel_library.cpp`, `ui/gear_filter.cpp` and `topology_applier.cpp`, which
[`KNOWN_ISSUES.md`](../reference/KNOWN_ISSUES.md) records. A future plugin wrapper would
draw from the same catalog (see "What is spec-only, not yet built" below), but none exists in
the tree yet. `rig_preset` serializes
a rig (descriptor id plus parameter values) to and from JSON. This is the dependency rule
design spec §8 states for `modules/`; what is verified here is that `modules/catalog/node_registry/`
and `modules/session/rig_preset/` exist in the tree and match that description, alongside the six
product modules and `cabinet`/`routing`/`capture`/`room` under `modules/nodes/`.

## Hosts (`applications/`)

`applications/host` is a minimal SDL2-backed standalone host; `applications/host_asio` is the ASIO-backed
counterpart; `applications/host_gui` is the full rig-editing GUI, with its own audio import, resampling
and MP3-decode support classes alongside `RigController` and the UI tree under
`applications/host_gui/ui/`. All three sit above `modules/` and `core/`: they own an `AudioDevice`,
build a `RigGraph` from `node_registry()`, and drive `process()` per audio callback. A VST/CLAP
plugin wrapper is design intent only, design spec §3 ("Product contract") and §8's `applications/plugin`
layout entry describe it, but no `applications/plugin` directory exists in the tree yet.

## What is spec-only, not yet built

The following are described in `docs/design/SUSHIDSP.md` as intent and were not found under
`include/`, `src/`, `modules/` or `applications/` during this review, read them from the spec, not as
current fact:

- The VST3/CLAP plugin wrapper and its `applications/plugin` directory (design spec §3, §8).
- Any packaging step beyond what `sd build`/`sd test` already exercise (design spec §8,
  "Targets & dependency rules").
- Roadmap phases beyond what the changelog records as shipped (design spec §9); check
  [`docs/reference/CHANGELOG.md`](../reference/CHANGELOG.md) for what has actually landed.
