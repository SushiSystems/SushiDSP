# SushiDSP design

**Status:** Open — the core, four amplifiers and the studio, timeline and room paths are built; D6, D7, DR3 and DR4 are open
**About:** The standalone amp and pedal simulation framework: the rig graph, the DSP math library, the amplifiers and the studio, timeline and room paths.
**Modules:** modules/foundation/core, modules/foundation/math, modules/foundation/circuits, modules/foundation/circuit_blocks, modules/foundation/io, modules/foundation/device, modules/catalog/node_registry, modules/nodes/routing, modules/nodes/jcm800, modules/nodes/tubescreamer, modules/nodes/dd3, modules/nodes/diezel_vh4, modules/nodes/mesa_mark2c, modules/nodes/peavey5150, modules/nodes/cabinet, modules/nodes/capture, modules/nodes/microphone, modules/session/rig_preset

## Problem

SushiDSP is a **standalone amp/pedal simulation framework**, in the spirit of AmpliTube/Bias FX: a portable C++ DSP core (block graph of `AudioNode`s), a math library (FFT, convolution, filters, WDF/DK-method circuit solving, oversampling), and a **rig**: a user-editable, freely-connectable chain of pedal/amp/cabinet nodes. It ships as a standalone host app and as a VST3/CLAP plugin. It depends on nothing but the C++ standard library and SIMD intrinsics; there is no game-engine integration, no GPU offload seam, and no spatializer/synthesis/aero-thermo content. The first two products are the **JCM800** amp model and the **Tube Screamer** overdrive pedal, both white-box (WDF/DK-method) circuit models sharing the same solver and oversampling infrastructure.

### Why SushiDSP is standalone

SushiDSP has no consumer beyond its own standalone host and its own VST3/CLAP plugin. It does not sit "between" anything, and it links nothing beyond the C++ standard library, SIMD intrinsics, and, for the wrapper/host targets only, SDL2 and the CLAP/VST3 SDK. This keeps the plugin free of any external runtime dependency and keeps the whole library buildable and testable headless.

```
         ┌───────────────────────┐        ┌───────────────────────┐
         │  Standalone host app  │        │  VST3 / CLAP plugin   │
         └───────────┬───────────┘        └───────────┬───────────┘
                      └────────────────┬────────────────┘
                                       ▼
                     ┌───────────────────────────────────┐
                     │              SushiDSP                │
                     │  rig graph · AudioNode ·             │
                     │  NodeDescriptor · buffer pool ·       │
                     │  FFT/convolution/filters/WDF/DK ·     │
                     │  JCM800 · Tube Screamer · AudioDevice │
                     │  PORTABLE C++/SIMD, zero external deps│
                     └───────────────────────────────────┘
```

## Decision

**Repository decision (owner):** SushiDSP drops the SushiEngine/SushiRuntime layering entirely. It is a fully independent repository and library with zero dependency on any other SushiStack project. Every product (amp, pedal, cabinet) is packaged as a `NodeDescriptor` (see the rig graph) so the rig, the standalone host, and the VST3/CLAP wrapper all consume products identically, with zero product-specific code in any of them.

### Goals and hard acceptance criteria

1. **Portable, dependency-free CPU core.** The DSP graph, all `AudioNode`s, and the math library compile and run with only a C++17 standard library and SIMD intrinsics. A headless unit test processes a graph to a float buffer on any toolchain.
2. **Real-time-safe processing.** `AudioNode::process()` performs no allocation, no lock, no syscall, no exception. Verified by a test harness that traps `malloc`/lock calls on the processing thread across a 30-minute soak; the bar is zero violations.
3. **One core, N products, freely rigged.** The same block graph + math substrate hosts any number of amp/pedal/cabinet products; the **rig** is a user-editable graph of `NodeDescriptor`-backed nodes connected in any series/parallel topology the user chooses, recompiled off the RT thread and swapped in atomically. No product-specific code in the rig, host, or plugin wrapper.
4. **JCM800 fidelity.** White-box 2203 circuit (WDF / DK state-space), ≥ 8× oversampling, six front-panel controls, partitioned-convolution cabinet.
5. **Tube Screamer fidelity.** White-box TS808/TS9 circuit (WDF / DK state-space), same oversampling/solver infrastructure as JCM800, Drive/Tone/Level controls.
6. **VST/standalone wrapping.** A thin VST3/CLAP wrapper and a standalone host both drive the rig with no reference to any other engine or runtime.
7. **SOLID + conventions.** the license block of the `source-comments` skill, Allman braces, `I`-prefixed interfaces, Doxygen on every public function, trivially-copyable POD where it crosses a thread.

### Conventions

C++17 (no C++20/23), Allman braces (opening brace on its own line; trivial one-line accessors may stay compact), types `PascalCase`, functions and variables `snake_case`, members with a trailing underscore (`impl_`), constants `UPPER_SNAKE`, spelled-out names, the license block of the `source-comments` skill on every source file, a Doxygen `@brief`/`@param`/`@return` on every public function, and no comments unless the why is non-obvious. The developer CLI is `sd` (Python Typer, `cli/sushidsp/`, built on the shared `sushicore` presentation layer, see "CLI membership").

Every source file opens with the license block of the `source-comments` skill. `tools/licensing/write_license_block.py` in SushiSkills writes it with the project line `SushiDSP - https://github.com/SushiSystems/SushiDSP`; nobody types it by hand. `tests/main.cpp` is a live example.

### Survey conclusions, what we adopt and why

| Source | What we take (math/structure only, no code, no dependency) |
|---|---|
| JUCE `dsp`, chowdsp | Oversampling (polyphase halfband cascade), partitioned-convolution structure, WDF adaptor class shape. |
| Zölzer *DAFX*; Yeh/Pakarinen/Werner (WDF/tube papers) | Triode models (Koren/Norman), DK-method state-space, op-amp clipping stage modeling (Tube Screamer). |
| pffft / KISS FFT (reference) | Split-radix real-FFT structure for the portable CPU FFT (the FFT), reference for correctness, own implementation. |

**Skip list:** vendor DSP libraries as hard dependencies (own the math for portability); neural amp/cabinet (white-box chosen, products and wrappers/repository and packaging; a neural node stays an OCP seam, unbuilt); any GPU/accelerator dependency; spatializer, synthesis framework, and aero/thermo generators (out of scope, this is an amp/pedal framework, not a general audio-DSP or game-audio library).

### SOLID

- **SRP:** every `AudioNode` does one DSP job; math kernels, device, and each product are separate.
- **OCP:** new products (amps, pedals, cabinets) and the deferred neural-amp path plug in behind existing interfaces; the rig never names concrete node types; new products register a `NodeDescriptor` without touching the rig, host, or plugin wrapper.
- **LSP:** every node honors "fill exactly `frames`, allocation-free"; any node is substitutable in any rig slot; any `NodeDescriptor` is hostable by the generic wrapper.
- **ISP:** narrow interfaces, `AudioNode`, `AudioDevice`, no god-interface.
- **DIP:** the core depends on abstractions; the platform device is injected.

### Dependencies

- **Platform:** SDL2 for the first `AudioDevice` (PRIVATE, host target only); the CLAP/VST3 SDK for the wrapper (PRIVATE, plugin target only).
- **Everything else:** C++17 standard library + SIMD intrinsics only. No other SushiStack repository's *code*, no GPU runtime, no neural-inference dependency, see CLI membership for the CLI-only exception.

### CLI membership (developer tooling only, not a code dependency)

SushiDSP is in SushiHub's catalog (`cli/sushihub/catalog.toml` in the SushiStack repository), cloned with `hub add sushidsp` or linked into a developer's workspace with `hub link sushidsp <path>`, like sushiruntime, sushiengine, sushiai and sushiblas. A checkout works the same without SushiHub. The membership buys two things, both developer-tooling-only:

1. **`cli/sushistack.deps.toml`**: SushiDSP's dependency fragment (today: SDL2), provisioned by `sd setup` into the shared root, `~/.sushisystems` (or `SUSHISYSTEMS_HOME`). SushiDSP has no `[module] depends_on` entry, it needs no other stack module's build output.
2. **`sushicore`**: the shared CLI presentation layer (themed console output, layered `config.toml`/`config.local.toml` loading, workspace-root discovery) that `sr`/`se`/`sa`/`sb` already build on. `cli/sushidsp/console.py` and `cli/sushidsp/config.py` are thin wrappers over `sushicore.build_console`/`sushicore.config_base.ToolConfig`, matching those CLIs' shape exactly (see any of them for the pattern). `cli/pyproject.toml` lists `sushicore` as an ordinary pip dependency, which `python -m pip install -e ./cli` resolves from PyPI.

None of this touches `sushi_dsp`'s C++ link graph. A checkout of this repo with no SushiStack workspace around it still configures and builds with plain `cmake`/`ctest`, `sd`'s commands (`configure`/`build`/`test`) are POSIX/Windows subprocess wrappers around those, and `sushicore`'s console degrades to an unthemed default (not a crash) when no `cli/config.toml` is reachable (see `console.py`'s lazy-build docstring).

## Design

The pages below hold the detail: [the studio path](STUDIO_PATH.md),
[the timeline path](TIMELINE_PATH.md), [the room path](../ROOM/PATH.md),
[the DSP math library](DSP_MATH_LIBRARY.md), [the amplifiers](AMPLIFIERS.md),
[the Tube Screamer](TUBE_SCREAMER.md) and [the parallel tracks](PARALLEL_TRACKS.md).

### The rig graph

A directed acyclic graph of `AudioNode`s, topologically sorted and **replayed per block**. Unlike a fixed per-product graph, the **rig** is a graph the user edits freely at runtime: any number of pedal/amp/cabinet nodes, connected in any series/parallel topology, via the VST host UI or the standalone host's UI. The graph is a plain C++ object walked on the RT thread, a deterministic single-thread topological walk, zero handoff, zero scheduler.

```
 AudioNode rig graph (recompiled on edit, replayed per 256-frame block):

   [DI in]─▶[Tube Screamer]─▶[JCM800]─▶[Cabinet conv]─▶[Meter/Limiter]─▶ out
                  (any product, any order, any count, user-edited)

 process(context) contract per node:  fill exactly context.frames of output,
   no alloc / no lock / no syscall / no throw.  All memory acquired in prepare().
```

#### Core interfaces (portable, `I`-prefixed)

```cpp
namespace SushiDSP
{
    struct PrepareInfo { double sample_rate; std::uint32_t max_block; std::uint32_t channels; };
    struct ProcessContext
    {
        float* const*       outputs;      ///< [channel][frame], node fills [0, frames)
        const float* const* inputs;
        std::uint32_t       frames;
        double              time_seconds; ///< monotonic block time for parameter smoothing
    };

    /// One DSP graph node. The whole library is compositions of these.
    struct AudioNode
    {
        virtual ~AudioNode() = default;
        virtual void prepare(const PrepareInfo& info) = 0;   ///< all allocation happens here
        virtual void process(ProcessContext& context) = 0;   ///< RT-safe: no alloc/lock/syscall/throw
        virtual PortLayout ports() const = 0;
        virtual void reset() = 0;                             ///< clear state, click-free
    };

    /// A hardware/output device. Injected, SushiDSP never assumes one. SDL2/WASAPI/CoreAudio/VST-host.
    struct AudioDevice
    {
        virtual ~AudioDevice() = default;
        virtual DeviceFormat format() const = 0;
        virtual void start(DeviceCallback callback) = 0;     ///< callback runs on the RT thread
        virtual void stop() = 0;
    };
}
```

#### Product contract, the `NodeDescriptor` (the rig-slot seam)

A **product** (JCM800, Tube Screamer, a cabinet IR loader, any future amp/pedal) is packaged as a self-describing factory. This POD contract is what lets the rig, the standalone host, and the VST3/CLAP wrapper all instantiate and drive any product identically.

```cpp
namespace SushiDSP
{
    enum class ParamKind { Linear, Logarithmic, Decibel, Boolean, Choice };
    struct ParameterDescriptor
    {
        const char* id;          ///< stable, e.g. "drive", "tone", "master"
        const char* label;       ///< UI name
        float       min, max, default_value;
        ParamKind   kind;
        const char* unit;        ///< "dB", "Hz", ""
    };

    /// Everything a host needs to instantiate and drive a product, with no knowledge of its internals.
    struct NodeDescriptor
    {
        const char*                       id;         ///< "sushidsp.jcm800", "sushidsp.tubescreamer"
        const char*                       name;       ///< "JCM800", "Tube Screamer"
        span<const ParameterDescriptor>   parameters; ///< the full automation surface
        PortLayout                        ports;      ///< in/out channel counts
        std::unique_ptr<AudioNode>     (*make)(const PrepareInfo&);  ///< the factory
    };

    /// Each product exposes exactly one of these. Registration is a free function.
    const NodeDescriptor& jcm800_descriptor();         // from the jcm800 module
    const NodeDescriptor& tube_screamer_descriptor();  // from the tubescreamer module
    // ...
}
```

- **The VST/CLAP wrapper** reads `parameters` → declares host automation params; calls `make()` per rig slot → owns the node; forwards `process()` in rig order.
- **The standalone host** builds/edits the rig from a registry of known `NodeDescriptor`s and renders a simple UI from `parameters`.
- **Rigs are portable presets:** a rig preset is an ordered list of `{descriptor.id, {param_id → value}}`, so a rig built in the VST loads unchanged in the standalone host and vice versa.

#### Graph mechanics

- **Transient buffer pool**: block-sized float channel buffers with liveness-based reuse; a node's output frees back to the pool once its last consumer reads it.
- **Recompile off the RT thread**: the user editing the rig (add/remove/reorder a pedal or amp) recompiles the graph on a control thread; the new graph is swapped in via an atomic pointer flip, the old retired after a grace block. This is the **primary** way users interact with SushiDSP, not a rare structural event.

### Products, device backend and wrappers

A **product module** is a self-contained library (its own DSP) that exposes one `NodeDescriptor` (the rig graph) and nothing else. It never references a device or a host.

Three products exist: `jcm800` (the amplifiers), `tubescreamer` (the Tube Screamer), and `dd3`, the Boss DD-3 delay, specified in the dd3 spx90 design spec in the version history. The DD-3 has no section of its own here; `modules/nodes/dd3/README.md` carries its block table, the three constants that are calibrations rather than component values, and what it leaves unmodelled, and the spec carries the rulings behind them. The SPX90 joins them behind the same seam.

From these modules, two consumers are built by generic (non-product-specific) code:

- **`AudioDevice`**: injected output backend. First implementation SDL2; WASAPI-exclusive/ASIO/CoreAudio/ALSA/PipeWire later, each behind the same seam.
- **Standalone host**: a generic app that opens an `AudioDevice`, lets the user build/edit a rig from the registered `NodeDescriptor`s, and runs it with a simple parameter UI. Rig presets save/load as portable `{descriptor.id, {param_id → value}}` lists.
- **VST3 / CLAP wrapper**: one generic adapter: maps host process-block → rig graph `process()`; the rig itself (not a single product) is what the plugin hosts, so the plugin's own UI is the rig editor. Built with the CLAP/VST3 SDK (or iPlug2). Depends on **SushiDSP only**: zero external runtime dependency.
- **Command/param plumbing**: a lock-free SPSC ring carries parameter changes, rig-edit commands, and events from the control/UI thread to the RT thread; SushiDSP provides its own generic `SpscRing<T>`. **As built in D5b:** `RigGraph` owns an `SpscRing<ParameterChange>`; `RigGraph::push_parameter_change` is the control-thread-facing producer (used by `RigController` in `applications/host_gui`), and `RigGraph::process` drains the ring at the top of every block before running any node, dispatching each change to the target node's `AudioNode::set_parameter(std::uint16_t, float)`. Nodes recompute derived coefficients on the audio thread only when their `SmoothedParameter` actually moved that block, so an idle knob costs nothing.

### Repository, modules and packaging

SushiDSP is a standalone repository with a developer CLI **`sd`** (`sd configure` / `sd build` / `sd test`, growing `sd run` / `sd plugin` as those targets land). It is in SushiHub's catalog (`hub add sushidsp` / `hub link sushidsp <path>`, alias `sd`), which buys the shared CLI presentation layer (`sushicore`, CLI membership) and dependency-fragment aggregation (`cli/sushistack.deps.toml`), not a code dependency: the `sushi_dsp` core target itself still links nothing beyond the C++17 standard library and SIMD intrinsics (the goals/dependencies), and building/testing this repo standalone (outside any SushiStack workspace) works unchanged.

#### Layout

```
sushidsp/
  include/SushiDSP/
    core/     rig graph, AudioNode, NodeDescriptor, buffer pool, SpscRing, AudioDevice
    math/     fft, convolution, filters, resamplers, wdf, complex, windows, simd
  src/                       core/math impls (portable C++/SIMD)
  modules/                   ← the separable PRODUCTS, each a library exposing a NodeDescriptor
    jcm800/                    the amplifiers amp: WDF/DK stages, tone stack, cabinet, depends only on core+math
    peavey5150/                the amplifier family amp: lead channel, shared stage classes (its own README)
    mesa_mark2c/               the amplifier family amp: lead mode, five-band graphic EQ (its own README)
    diezel_vh4/                the amplifier family amp: channel 3, documented approximation (its own README)
    tubescreamer/              the Tube Screamer pedal: WDF/DK clipping stage, tone stack, depends only on core+math
    dd3/                       DR2 delay: variable-rate loop, 12-bit ring, NE570 compander (its own README)
  applications/
    host/                      the standalone rig host (SDL2, AudioDevice, rig editor UI)
    plugin/                    the VST3/CLAP rig wrapper
  examples/  rig_demo
  tests/     unit suites: golden-vector + RT-safety
  cli/       the `sd` CLI
```

#### Targets and dependency rules

- **`SushiDSP::sushi_dsp`**: the portable core (`core`+`math`). C++17 stdlib + SIMD only, **zero** external deps.
- **`SushiDSP::jcm800`, `SushiDSP::peavey5150`, `SushiDSP::mesa_mark2c`, `SushiDSP::diezel_vh4`, `SushiDSP::tubescreamer`, `SushiDSP::dd3`**: product libs, each depending only on `sushi_dsp`. Fully separable; a product can graduate to its own repo with no code change, because it couples to the core only through headers and to consumers only through its `NodeDescriptor`.
- **Exported & namespaced**: SushiDSP ships install/export rules producing `SushiDSP::*` targets and a `SushiDSPConfig.cmake`.

Tests: `TEST(<unit>, …)` for FFT/convolution/filter/WDF-solver numerical correctness (golden-vector), rig compile/recompile, RT-safety (no-alloc trap); per-product physical-plausibility tests (DC stability, frequency response, THD sweep) in their module. Perceptual A/B validation against real hardware captures is deferred, no reference captures available yet. **As built in D6a:** `modules/foundation/signal_measure/` is a test-side-only measurement library (`render`, `thd`, `alias_floor`, `frequency_response`, `null_test`) that every product's plausibility and alias-floor test builds on, so those numbers come from one audited implementation instead of each test computing its own FFT/THD by hand. The golden files, one folder per unit under `tests/<tier>/<module>/goldens/`, replace the old five-sample-snapshot regression tests with 1-second rendered `.f32` files at 48 kHz and 96 kHz, compared at −80 dB RMS residual; regenerate them with `SUSHIDSP_REGENERATE_GOLDEN=1 sd test --label medium` and add a row to `modules/foundation/golden_compare/README.md` recording why. A SPICE reference corpus is staged under `docs/reference/spice/` (netlist-directive `.asc` files, Koren/Norman `.sub` models, `tools/spice/export.py`, `modules/foundation/spice_reference/include/SushiDSP/foundation/spice_reference/spice_csv.hpp`) but nothing has been simulated yet, LTspice is not installed in this environment, so producing the golden CSVs (`python tools/spice/export.py --all`, then commit) is a user-side step, not something this repo's tests currently check against.

## Phases

| Id | Phase | State |
| --- | --- | --- |
| D0 | Skeleton and math kernels: repo, `sd` CLI, `sushi_dsp` target, `AudioNode`, rig graph, `NodeDescriptor`, transient pool, FFT and filters with golden-vector tests | built |
| D1 | Device seam and standalone host: `AudioDevice` (SDL2), RT-safe callback, `SpscRing`, a generic host with a minimal rig editor | built |
| D2 | WDF/DK solver core: shared nonlinear-circuit infrastructure, partitioned convolution, oversampling | built |
| D3 | Tube Screamer (`modules/nodes/tubescreamer`), chosen before the JCM800 as the simpler single-nonlinear-stage circuit | built |
| D4 | JCM800 (`modules/nodes/jcm800`): white-box chain and cabinet convolution | built |
| D5 | Rig recompile and presets: free-graph editing, atomic-swap recompile, portable rig-preset save and load | built |
| D6 | VST3/CLAP wrapper: `applications/plugin` hosts the rig graph as a DAW plugin with its own rig-editor UI | planned |
| D7 | Hardening and packaging: namespaced export and Config finalized, perceptual A/B validation once reference captures exist | planned |
| DA | High-gain amplifiers: three sibling amp modules (`peavey5150`, `mesa_mark2c`, `diezel_vh4`) on the shared stage classes | built |
| DA4 | Plate-swing `TriodeStage`, spec the triode plate swing design spec in the version history | built |
| DA5 | The grid knee, antialiasing and the volt-true power stage, spec the grid knee antialiasing design spec in the version history; the -33.8 dB alias bound stays a known issue | building |
| DS | Stereo, tracks and playback; see [the parallel tracks](PARALLEL_TRACKS.md) | built |
| DT | The arrangement view | built |
| DE | Room acoustics; DE7 and DE8 are not built | building |
| DR | Delay and reverb; DR3 and DR4 are not built | building |
| DP | The stack profiler and the logging gaps: the `profile` front, zones and counters in the block path, a benchmark on a timeline with checks, a counter and a record for every place the audio thread fails silently, and the status enums of a refused cabinet or HRTF set | building |

The dates and commits of the landed phases are in
the record. The states above are read
from the status text of each phase, not verified against the tree; the owner confirms them.

### Work

<!-- work:begin -->
- [2026_10_13_TRACK_RIGS_AND_PLUGIN](../../agent/2026_10_13_TRACK_RIGS_AND_PLUGIN/SPEC.md): D6
- [2026_10_26_PROFILER_AND_LOG_ADOPTION](../../agent/2026_10_26_PROFILER_AND_LOG_ADOPTION/SPEC.md): DP
<!-- work:end -->

## Open questions

- **The comment-shape check is outside the gate.** `tools/documentation/check_source_comments.py` exists and `sd check` does not run it; its `--report` finding count is listed by rule in `docs/backlog/README.md`.
- **DR2 shipped without a single measured number.** The build was locked while the DD-3 was written, so the 0.15 ms benchmark budget is unjudged, every acceptance row is unrun, and the pinned `dd3_48k.f32` and `dd3_96k.f32` were not yet committed, which failed the two regression cases. The changelog's DR2 "Not yet measured" paragraph names the three commands that close this.
