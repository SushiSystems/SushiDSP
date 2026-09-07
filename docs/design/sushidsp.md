# SushiDSP — portable real-time amp/pedal DSP framework: block graph, JCM800, Tube Screamer

> **For the implementing agent.** This is the authoritative design specification for the **SushiDSP** repository — a standalone, dependency-free audio-DSP library. Build the repo from this doc. Conventions: **C++17** (no C++20/23), **Allman braces** (opening brace on its own line; trivial one-line accessors may stay compact), types `PascalCase`, functions/variables `snake_case`, members trailing underscore (`impl_`), constants `UPPER_SNAKE`, spelled-out names (no abbreviations), the **Apache 2.0 license header box** on every source file (line 2 carries the file's own name; the literal template is in §0a below — copy it, do not reconstruct it), a Doxygen `@brief`/`@param`/`@return` on every public function, and **no comments unless the *why* is non-obvious**. The developer CLI is **`sd`** (Python Typer, `cli/sushidsp/`, built on the shared `sushicore` presentation layer — see §11a). No comments in code beyond that rule.

SushiDSP is a **standalone amp/pedal simulation framework**, in the spirit of AmpliTube/Bias FX: a portable C++ DSP core (block graph of `IAudioNode`s), a math library (FFT, convolution, filters, WDF/DK-method circuit solving, oversampling), and a **rig** — a user-editable, freely-connectable chain of pedal/amp/cabinet nodes. It ships as a standalone host app and as a VST3/CLAP plugin. It depends on nothing but the C++ standard library and SIMD intrinsics; there is no game-engine integration, no GPU offload seam, and no spatializer/synthesis/aero-thermo content. The first two products are the **JCM800** amp model and the **Tube Screamer** overdrive pedal, both white-box (WDF/DK-method) circuit models sharing the same solver and oversampling infrastructure.

**Status:** Design only — nothing implemented. This supersedes the prior SushiDSP design (game-engine spatializer/aero-thermo/GPU-offload scope), which is abandoned in favor of a focused amp/pedal framework.

**Repository decision (owner, 2026-08-25):** SushiDSP drops the SushiEngine/SushiRuntime layering entirely. It is a fully independent repository and library with zero dependency on any other SushiStack project. Every product (amp, pedal, cabinet) is packaged as a `NodeDescriptor` (§3) so the rig, the standalone host, and the VST3/CLAP wrapper all consume products identically, with zero product-specific code in any of them.

---

## §0 Goals and hard acceptance criteria

1. **Portable, dependency-free CPU core.** The DSP graph, all `IAudioNode`s, and the math library compile and run with only a C++17 standard library and SIMD intrinsics. A headless unit test processes a graph to a float buffer on any toolchain.
2. **Real-time-safe processing.** `IAudioNode::process()` performs no allocation, no lock, no syscall, no exception. Verified by a test harness that traps `malloc`/lock calls on the processing thread across a 30-minute soak; the bar is zero violations.
3. **One core, N products, freely rigged.** The same block graph + math substrate hosts any number of amp/pedal/cabinet products; the **rig** is a user-editable graph of `NodeDescriptor`-backed nodes connected in any series/parallel topology the user chooses, recompiled off the RT thread and swapped in atomically. No product-specific code in the rig, host, or plugin wrapper.
4. **JCM800 fidelity.** White-box 2203 circuit (WDF / DK state-space), ≥ 8× oversampling, six front-panel controls, partitioned-convolution cabinet.
5. **Tube Screamer fidelity.** White-box TS808/TS9 circuit (WDF / DK state-space), same oversampling/solver infrastructure as JCM800, Drive/Tone/Level controls.
6. **VST/standalone wrapping.** A thin VST3/CLAP wrapper and a standalone host both drive the rig with no reference to any other engine or runtime.
7. **SOLID + conventions.** Apache headers, Allman braces, `I`-prefixed interfaces, Doxygen on every public function, trivially-copyable POD where it crosses a thread.

---

## §0a Reference header

Every `.hpp`/`.cpp` in this repo opens with this exact block, each line 76 columns wide, with `<FILENAME>` replaced by the file's own name left-aligned after `/* ` and padded out to the box edge. Copy it verbatim; `tests/common/test_main.cpp` is a live example.

```
/**************************************************************************/
/* <FILENAME>                                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/
```

---

## §1 Why SushiDSP is standalone

SushiDSP has no consumer beyond its own standalone host and its own VST3/CLAP plugin. It does not sit "between" anything, and it links nothing beyond the C++ standard library, SIMD intrinsics, and — for the wrapper/host targets only — SDL2 and the CLAP/VST3 SDK. This keeps the plugin free of any external runtime dependency and keeps the whole library buildable and testable headless.

```
         ┌───────────────────────┐        ┌───────────────────────┐
         │  Standalone host app  │        │  VST3 / CLAP plugin   │
         └───────────┬───────────┘        └───────────┬───────────┘
                      └────────────────┬────────────────┘
                                       ▼
                     ┌───────────────────────────────────┐
                     │              SushiDSP                │
                     │  rig graph · IAudioNode ·             │
                     │  NodeDescriptor · buffer pool ·       │
                     │  FFT/convolution/filters/WDF/DK ·     │
                     │  JCM800 · Tube Screamer · IAudioDevice │
                     │  PORTABLE C++/SIMD — zero external deps│
                     └───────────────────────────────────┘
```

---

## §2 Survey conclusions — what we adopt and why

| Source | What we take (math/structure only — no code, no dependency) |
|---|---|
| JUCE `dsp`, chowdsp | Oversampling (polyphase halfband cascade), partitioned-convolution structure, WDF adaptor class shape. |
| Zölzer *DAFX*; Yeh/Pakarinen/Werner (WDF/tube papers) | Triode models (Koren/Norman), DK-method state-space, op-amp clipping stage modeling (Tube Screamer). |
| pffft / KISS FFT (reference) | Split-radix real-FFT structure for the portable CPU FFT (§4.1) — reference for correctness, own implementation. |

**Skip list:** vendor DSP libraries as hard dependencies (own the math for portability); neural amp/cabinet (white-box chosen — §7/§8; a neural node stays an OCP seam, unbuilt); any GPU/accelerator dependency; spatializer, synthesis framework, and aero/thermo generators (out of scope — this is an amp/pedal framework, not a general audio-DSP or game-audio library).

---

## §3 Architecture — the rig graph

A directed acyclic graph of `IAudioNode`s, topologically sorted and **replayed per block**. Unlike a fixed per-product graph, the **rig** is a graph the user edits freely at runtime: any number of pedal/amp/cabinet nodes, connected in any series/parallel topology, via the VST host UI or the standalone host's UI. The graph is a plain C++ object walked on the RT thread — a deterministic single-thread topological walk, zero handoff, zero scheduler.

```
 IAudioNode rig graph (recompiled on edit, replayed per 256-frame block):

   [DI in]─▶[Tube Screamer]─▶[JCM800]─▶[Cabinet conv]─▶[Meter/Limiter]─▶ out
                  (any product, any order, any count — user-edited)

 process(context) contract per node:  fill exactly context.frames of output,
   no alloc / no lock / no syscall / no throw.  All memory acquired in prepare().
```

### Core interfaces (portable, `I`-prefixed)

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
    struct IAudioNode
    {
        virtual ~IAudioNode() = default;
        virtual void prepare(const PrepareInfo& info) = 0;   ///< all allocation happens here
        virtual void process(ProcessContext& context) = 0;   ///< RT-safe: no alloc/lock/syscall/throw
        virtual PortLayout ports() const = 0;
        virtual void reset() = 0;                             ///< clear state, click-free
    };

    /// A hardware/output device. Injected — SushiDSP never assumes one. SDL2/WASAPI/CoreAudio/VST-host.
    struct IAudioDevice
    {
        virtual ~IAudioDevice() = default;
        virtual DeviceFormat format() const = 0;
        virtual void start(DeviceCallback callback) = 0;     ///< callback runs on the RT thread
        virtual void stop() = 0;
    };
}
```

### Product contract — the `NodeDescriptor` (the rig-slot seam)

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
        std::unique_ptr<IAudioNode>     (*make)(const PrepareInfo&);  ///< the factory
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

### Graph mechanics

- **Transient buffer pool** — block-sized float channel buffers with liveness-based reuse; a node's output frees back to the pool once its last consumer reads it.
- **Recompile off the RT thread** — the user editing the rig (add/remove/reorder a pedal or amp) recompiles the graph on a control thread; the new graph is swapped in via an atomic pointer flip, the old retired after a grace block. This is the **primary** way users interact with SushiDSP, not a rare structural event.

### §3a The studio path — channels, takes and the transport

The DS phase (spec
[`docs/superpowers/specs/2026-09-07-stereo-tracks-design.md`](../superpowers/specs/2026-09-07-stereo-tracks-design.md),
plan [`docs/superpowers/plans/2026-09-07-stereo-tracks.md`](../superpowers/plans/2026-09-07-stereo-tracks.md))
made the graph channel-aware, added a capture module beside the products, and gave the standalone
host a transport, a track list and a re-amp path.

The amplifiers stay mono in and mono out. Spec §2 gives the reason: a guitar amplifier is a mono
circuit, and running its chain twice over the same samples costs twice the CPU for two identical
channels. Stereo begins where the channels differ — the pan law, a recorded track placed off
centre, two cabinets — so the graph's job is to carry whatever width its endpoints declare and
never collapse one by accident.

**The channel rule.** `RigGraph` acquires `PortLayout::output_channels` buffers per node instead
of one, and adapts at each edge under a table the graph applies and no node ever sees: one channel
into two is copied to both, two into one is summed and scaled by 0.5, n into n passes channel for
channel. `RigGraph::channels_adapt(from, to)` is that table as a predicate, and `compile()` refuses
any edge it rejects, naming the edge, so an unroutable rig fails at build time. `kMaxChannels` is
2. `kMaxFanInOut` stays 8, now counted in edges. `PortLayout::input_channels` is a slot count in
this tree — `MixerNode` declares `{8, 1}` and reads one edge per slot — so a node's width is
`min(input_channels, output_channels)` and its slot count is `input_channels / width`; edges past
the last slot fold into it, summed per channel. `live_buffer_count()` reports the pool size
`compile()` needed: every node's channels, plus `kMaxFanInOut * kMaxChannels` = 16 adaptation
buffers added once, because a gather's buffers go back to the pool before the next node runs.

**Pan and the device edge.** `routing::PanNode` is `PortLayout{1, 2}` with one `pan` parameter in
[-1, 1] on a constant-power law, smoothed by `SmoothedParameter`, registered under Routing with a
front panel. `ChannelSelection` carries the same table out to the device:
`rig_output_adapts_to_device(rig_channels, device_channels)` answers whether a pair is covered and
`write_rig_output_to_device()` performs the mapping, allocation-free. `apps/host_gui` asks its
device for two output channels and prepares the rig at
`min(device output channels, RigGraph::kMaxChannels)`, so a mono rig fills both speakers and a rig
ending in a `PanNode` moves between them. `Sdl2AudioDevice` writes any output width: its
constructor sizes one deinterleaved scratch block, and the callback interleaves into SDL2's buffer;
a one-channel device keeps the older path that writes SDL2's buffer directly.

**Takes, tracks and the transport.** `modules/capture` is product-independent — it models no
circuit, exposes no `NodeDescriptor` and depends only on `SushiDSP::sushi_dsp`. It holds `Take` (a
recording in memory with its sample rate), `TrackRecorder` (one destination: a `SpscRing<float>`
per channel, a writer thread, a dropped-block counter), `RecordingBus` (the processed/dry pair
armed in one call, which is what makes the two files sample-aligned), `Transport` and
`TrackPlayer` (up to `kMaxTracks` = 8 tracks, each with a take, gain, pan, mute and solo).
`SushiDSP::WavWriter`, in the core's `io/`, is the streaming write half of `WavFile`: 16-bit PCM or
32-bit float, the float path carrying a `fact` chunk and an 18-byte `fmt `. `TrackRecorder::write`
and `TrackPlayer::process` are the module's two audio-thread entry points and both are trapped in
`tests/functional/unit/test_rt_safety.cpp`. `Transport` packs the loop range into one 64-bit word,
begin in the high half and length in the low, so a block reads both ends in one load and cannot see
a half-applied range; the cost is that a loop must lie inside the first 2^32 frames, 24.8 hours at
48 kHz. `modules/capture/README.md` carries the rest.

**Re-amping** needs no node. The host chooses what feeds the rig — `RigInputSource::LiveInput`, a
track, or both summed — and a dry take played into a different amplifier is then an ordinary
playback with the switch thrown. The dry leg records the buffer that actually drove the rig rather
than the raw device input, which departs from spec §5's letter so that the dry file matches the
processed file beside it whatever the input source is.
`RigControllerReampingADryTakeReproducesTheRigsOwnOutput`
(`tests/functional/unit/test_rig_controller.cpp`) renders a Tube Screamer rig, writes a float WAV,
re-amps it through an identical rig in the host's own callback order, and asserts every sample
within 1e-6. The preset carries the track list and the transport's loop range.


---

## §4 DSP math library (portable, SIMD)

All numeric building blocks the amp/pedal circuit models need.

- **§4.1 FFT/DFT** — split-radix real FFT (forward/inverse), power-of-two sizes, in-place, cache-blocked. Foundation for partitioned convolution. Own implementation.
- **§4.2 Partitioned convolution** — uniform-partitioned (256-frame) overlap-add FFT convolution for the cabinet IR. CPU only.
- **§4.3 Filters** — TPT/zero-delay-feedback state-variable filter (SVF), biquad cascade (RBJ cookbook coefficients), one-pole/one-zero, all-pass; per-block-smoothed parameters, click-free. Used inside the WDF/DK circuit models and for cabinet mic-model pre-filtering.
- **§4.4 Resamplers** — two of them, solving different problems and sharing no code. `math/Oversampler` converts by a **fixed integer ratio** through a polyphase FIR halfband cascade, which is what the amp/pedal oversampler wants (§7/§8): the ratio is a build-time constant, so a per-stage halfband is both the cheapest and the sharpest answer. `math/Resampler` converts by an **arbitrary ratio that may move every block**, over one fixed prototype filter walked by a fractional phase accumulator; a halfband cascade cannot do that at all, and rebuilding a bank per ratio would allocate on the audio thread. The DD-3's sample clock, which the delay-time knob moves between 20 and 82.5 kHz, is the first consumer. **As built (DR1):** see "As built in DR1" below.
- **§4.5 WDF/DK circuit-solving core** — the shared nonlinear-circuit infrastructure both JCM800 and Tube Screamer build on: Wave Digital Filter adaptor primitives (series/parallel/resistive/reactive), a DK-method (discrete Kirchhoff state-space) formulation for tightly-coupled blocks, and a Newton–Raphson nonlinear-root solver (2–4 iterations, warm-started from the previous sample) shared by every nonlinear stage (triode, op-amp clipper, diode pair). **Amendment (D5b, user-confirmed 2026-08-26):** the WDF adaptor tree is deferred indefinitely — DK covers everything the roadmap needs and `Wdf.hpp` stays as the two linear one-ports it already has; see the decision table in `docs/superpowers/specs/2026-08-26-aaa-audio-quality-design.md`. **As built (D6b):** see the "As built in D6b" section below for the headers that exist, the corrected nonlinear-branch counts D6d must size against, and the measured solver cost.
- **§4.6 Complex + windowing** — a POD complex type, Hann/Blackman-Harris/Kaiser windows.
- **§4.7 Portability** — a thin SIMD abstraction (SSE/AVX/NEON with a scalar fallback) so the core builds on any target; no intrinsics leak into public headers.

### As built in D6a

The three halfband stages (`include/SushiDSP/math/HalfbandCoefficients.hpp`, generated by
`tools/dsp/design_halfband.py` from a scipy `remez` design, not hand-tuned) are, each running at
its own stage's higher rate with passband 0.45·f_base / stopband 0.55·f_base:

| Stage | Taps | Measured stopband | Passband ripple |
|---|---|---|---|
| 1 (base ↔ 2×) | 115 | −90.12 dB | ≤ 0.00027 dB |
| 2 (2× ↔ 4×) | 27 | −101.60 dB | ≤ 0.0003 dB |
| 3 (4× ↔ 8×) | 15 | −110.69 dB | ≤ 0.0003 dB |

Both `HalfbandUpsampler2x` and `HalfbandDownsampler2x` are polyphase in both directions — the
even-indexed zero taps that a symmetric halfband design produces are never multiplied, only the
non-zero half of each stage's taps is touched per output sample. `Oversampler8x`'s round trip
(upsample + downsample of a 256-frame block) measures 66.4 → 60.2 ns/sample after this change,
even though the wider filters needed for the deeper stopbands above raise the round-trip latency
`Oversampler8x` reports from 12 to 65 base-rate samples — the per-sample cost still falls because
the polyphase evaluation touches only the non-zero taps, regardless of how many taps a stage
carries.

### As built in D6b — the DK circuit core

D6b built §4.5's circuit solver and nothing else: no product module changed, and the JCM800 and
Tube Screamer still run their D6a stages. What exists now is the layer the D6c/D6d rebuilds sit
on.

**Layering.** `include/SushiDSP/math/dk/` holds four headers, each header-only and each depending
only on the standard library:

- `DenseLu.hpp` — fixed-size LU with partial pivoting, `DenseLu<N>`, with a size-aware overload so
  a circuit derived at less than its maximum node count factorises only the live block.
- `DampedNewtonSolver.hpp` — `DampedNewtonSolver<N>`, damped multivariate Newton with a halving
  line search, warm-started from the previous sample, carrying call/non-convergence/worst-case
  iteration counters.
- `DkNetlist.hpp` — the control-thread description: `resistor`, `capacitor`, `inductor`,
  `voltage_source`, `nonlinear`, `output`, `potentiometer`, over fixed-size arrays
  (`kMaximumNodes = 16`).
- `DkCircuit.hpp` — `DkCircuit<S, N, I, O>`: `prepare(netlist, sample_rate)` discretises by the
  trapezoidal rule and derives the DK matrices; `process_sample` is the per-sample solve;
  `set_potentiometer` / `update_matrices_if_dirty` re-derive at block cadence;
  `nonlinear_currents()` is the hand-off a supply-sag model reads.

Above it, `include/SushiDSP/circuits/models/` holds the device laws the solver calls back into —
`SiliconDiode.hpp`, `Triode12AX7.hpp`, `PentodeEL34.hpp` and the `SoftTransition.hpp` softplus and
logistic helpers shared by them. Everything evaluates in `double`; `float` appears only at the
`IAudioNode` boundary. The dependency runs one way: `circuits/` includes `math/`, and nothing in
`math/` names a circuit model — the models reach `DkCircuit` as a template parameter, never as an
`#include`.

**WDF.** The adaptor tree stays deferred, as the D5b amendment above says. `Wdf.hpp` remains the
two linear one-ports it has always been. DK is the circuit solver.

**Branch counts for D6d (Ruling 6 of the D6b plan).** The spec's D6d block table is wrong about
this and the numbers below supersede it. A nonlinear branch is a *port* of the device law, not a
device: a 12AX7 is **two** branches (grid-to-cathode and plate-to-cathode) and an EL34 is
**three** (grid, screen, plate). So D6d block 1, one 12AX7 gain stage, is `DkCircuit<3, 2, 2, 1>`,
and block 5, the push-pull EL34 output stage, needs `N = 10`, not the smaller counts the spec
implies. Anyone sizing a D6d template from the spec alone will size it too small. `S` grows the
same way: a 12AX7 carries three interelectrode capacitances of its own (see *Measured* below), so
block 1's three discrete capacitors make `S = 6`, and the shape is `DkCircuit<6, 2, 2, 1>`.

**Open question carried into D6d.** The Koren fit in `PentodeEL34.hpp` draws 33.4 mA at
`Vg1k = −37 V`, `Vg2k = Vpk = 400 V`. That is at the high end of a real 2203's idle, which may
mean the default fixed-bias point in spec §D6d wants moving rather than the model wanting
refitting. D6b did not decide it; D6d has to.

**Measured.** The `jcm_v1a` stage carries **six** reactive states, not three: the three discrete
capacitors of the SPICE block plus the 12AX7's own Cgk/Cgp/Cpk, which
`docs/reference/spice/models/12ax7_koren.sub` declares inside the subcircuit and every SPICE run
therefore has. Miller-multiplied by the stage gain, Cgp with the 68 kΩ grid stopper puts the input
corner near 22 kHz; a DK build without it is a different filter from the reference and would floor
the pending SPICE transient null around −27 dB instead of the −60 dB the plan asks for. The stage
(six capacitors, two triode branches, one preamp pot) settles to `Vk = 2.310 V`, `Vp = 274.4 V` —
the same bias point, since capacitors do not move DC. Newton convergence depends on the stimulus,
and both figures matter:

- *Continuous* — a 2 s 20 Hz→20 kHz sweep plus 1 kHz bursts at 0.05 V, 0.5 V and 5 V (past grid
  conduction), 1 056 000 samples at 384 kHz: **0 non-converged solves, 0.0000 %**, inside the
  plan's 0.01 % target.
- *Looped block* — the benchmark harness replays one 256-frame buffer, so the tone steps back to
  sample 0 once per repetition: **0 in 450 560, 0.0000 %**. Before the interelectrode capacitances
  were added this case missed the target at 219 in 450 560 (0.0486 %) — one per repetition, the
  harness's own discontinuity. The extra states damp that step; on this stimulus the count is zero, so the miss handed to D6c
  is closed as measured (one stimulus — an upper bound, not a guarantee; the counter stays printed).

Both cases produced zero non-finite samples and never exceeded 4 iterations.

Cost, release build, 256-frame block at 48 kHz: the seven-state two-nonlinearity block at 8×
oversampling costs **0.963 ms/block against the plan's 0.12 ms budget** — 8.0× over. The plan's
binding number was written for a *four-state* block (0.873 ms as first measured); the shape grew to
seven states for reference-topology parity, and the ~10 % it costs is recorded here rather than
hidden by keeping the smaller netlist. About 90 % of the time is `Triode12AX7::current` and
`::jacobian`, called separately by the solver at 63.8 ns per pair; the levers, in order of size,
are a joint `current_and_jacobian` on the models, `process_sample`'s redundant post-solve
`currents()` call, and a `DenseLu<2>` specialisation. D6c decides. A potentiometer re-derivation,
timed on its own, costs 1.56 µs and confirms plan Ruling 3 (pot moves re-derive on the audio
thread at block cadence, allocation-free).

### As built in DR1 — `math/Resampler`

`include/SushiDSP/math/Resampler.hpp` and `src/math/Resampler.cpp`. One prototype filter, a
Kaiser-windowed sinc at cutoff 0.45, lives in `include/SushiDSP/math/ResamplerCoefficients.hpp`
and is regenerated by `tools/dsp/design_resampler.py`; the header is generated output and is not
edited by hand.

| Property | Value |
|---|---|
| Half width | 34 zero crossings each side |
| Phases per zero crossing | 512 |
| Table entries | 17 410 |
| Kaiser beta | 10.0613 |
| Measured stopband | −100.48 dB worst case over [0.5, 256.0] |
| Measured passband ripple | 0.0001 dB over [0, 0.4] |

"Zero crossing" in those two rows is the generator's unit for the kernel's argument, and one of
them is one **input sample**: a half width of 34 spans 34 input samples either side of the output
instant, and the table holds 512 entries per input sample. At `CUTOFF = 0.45` the sinc's true
zeros sit 1/0.9 input samples apart, so fewer than 34 of them fall inside that span. Every span
figure elsewhere — the trap and benchmark comments, the wet-delay arithmetic — counts input
samples for this reason.

The half width grew from the planned 32 because 32 stopped at −98.45 dB, short of the −100 dB the
spec binds; 33 clears it by 0.08 dB at the true worst frequency and 34 by about 0.45 dB, so 34 is
what is committed. The cost of the two extra samples is latency: `latency_seconds()` returns
`34 / min(source_rate, target_rate)`, which is 0.708 ms per stage at 48 kHz. Decimating widens the
span by 1/ratio, so at the DD-3's slowest clock the half span is 34 / 0.416667 = 81.6 input
samples, `taps_per_side_` ceilings to 82, and `interpolate()` touches 164 taps per output.

That number is **availability look-ahead, not an alignment offset**. An output at time *t* reads
input a half kernel span either side of *t*, so it can only be emitted once the input half a span
past it has arrived; the converter itself is zero phase, so output time *t* still carries input
time *t* and a null against a dry path aligns at lag 0. Report it for plugin delay compensation
and size buffers with it; subtracting it as an offset misaligns the signal.

Every buffer is sized in `prepare(source_rate, target_rate, max_input_frames, minimum_ratio)`, and
`minimum_ratio` is what fixes the history length: the kernel's span in input samples grows as
1/ratio when decimating. `set_rates` moves the accumulator's step and the kernel's time scale and
allocates nothing, which is what lets the DD-3 re-clock 32 times per 256-frame block on the audio
thread. `tests/functional/unit/test_rt_safety.cpp` traps both `process` and `set_rates`.

### Fixed capacities

| Cap | Value | Where |
|---|---|---|
| Sample rate (base) | 48 000 Hz | `DeviceFormat`, configurable 44.1/48/96 |
| Block size | 256 frames (5.33 ms) | configurable 64–1024 |
| Convolution partition | 128 frames at or below 50 kHz, 256 above | `StreamingConvolution::partition_size_for`, cabinet IR |
| Amp/pedal oversample factor | 8× (→ 384 kHz) | `Jcm800`/`TubeScreamer` nodes |

---

## §5 JCM800 amp simulation (white-box) — product module `jcm800`

One composite `IAudioNode` (`Jcm800`) modeling the Marshall JCM800 2203 signal path with **Wave Digital Filters** for modular reactive sub-circuits and the **DK-method** for tightly-coupled blocks (the tone stack), at **8× oversampling**. Exposes `jcm800_descriptor()`.

```
DI in ─▶ input/bright cap ─▶ [V1a 12AX7] ─▶ coupling cap (tightness HPF) ─▶ [V1b 12AX7]
      ─▶ Marshall tone stack (Bass/Mid/Treble) ─▶ [V2a] ─▶ [phase inverter LTP]
      ─▶ [EL34 push-pull class-AB: grid conduction + crossover] ─▶ [output transformer:
         primary inductance + leakage + core saturation] ─▶ [power-supply sag: rectifier+RC → dynamic B+]
      (global NFB: Presence/Resonance) ─▶ downsample ─▶ [cabinet IR: partitioned convolution] ─▶ out
```

- **Triode stages (12AX7):** WDF sub-tree terminated by a nonlinear triode root; plate/grid currents from **Koren/Norman** equations, solved per oversampled sample by the shared Newton–Raphson solver (§4.5). Cathode bypass + grid-leak set level/frequency dependence; bright cap adds low-gain sparkle.
- **Tone stack:** the passive Marshall (FMV) network via DK-method (or 3-port WDF) so Bass/Mid/Treble interact exactly as the real passive network.
- **Phase inverter:** 12AX7 long-tailed pair.
- **Power section (EL34 push-pull, class AB):** pentode model with screen dependence; grid conduction clamps positive grid swing; crossover emerges from the class-AB bias point.
- **Output transformer:** primary inductance (low-end rolloff), leakage (top shaping), Jiles-Atherton-lite core saturation.
- **Power-supply sag:** rectifier + reservoir RC → dynamic B+.
- **Cabinet:** measured 4×12 IR via §4.2 partitioned convolution, mic-model pre-filter, swappable IR.

### As built in D4

The section above records the intended design. What D4 actually shipped is simpler in five
places, each a deliberate scope decision rather than an omission:

- **Triode stages** are not a WDF sub-tree with Koren/Norman plate and grid equations. `TriodeStage`
  solves one grid-node equation — source resistance, grid leak, and a Shockley grid-conduction
  diode — with the shared Newton–Raphson solver, and takes the plate output as an inverting linear
  gain on the grid's excursion from its bias point. The distortion character comes from grid
  conduction, not from a plate-current law.
- **Tone stack** is three cascaded RBJ biquads (low shelf, peaking, high shelf), not the passive
  FMV network via DK-method or a 3-port WDF, so the three controls do not interact the way the real
  passive network's do.
- **Phase inverter** is a single `TriodeStage` plus a sign flip, not a long-tailed pair.
- **Power section** is `PentodeStage`: the same grid-conduction model as the triode, followed by an
  explicit `tanh` plate saturation. There is no screen-grid dependence. Its node equation is
  bias-invariant by construction, so the class-AB asymmetry between the two tubes is applied as an
  input-referred grid offset on one half rather than as a difference in their bias points — a
  cathode-bias difference cancels out of the equation entirely and produces no asymmetry at all.
- **Output transformer** core saturation is a plain `tanh`, not Jiles-Atherton-lite.

The signal chain otherwise follows the diagram, including where the decimator sits: the output
transformer and the sag stage run inside the oversampled region, and only the presence shelf and
the master volume — both linear, so neither generates anything to alias — run after it. Resonance
and a true global NFB loop are not implemented; presence is a fixed high shelf. Of the advanced
parameters listed below, only the cabinet IR is settable (a constructor argument); bias, sag
amount and oversample factor are compile-time constants.

**Parameters (`NodeDescriptor`):** Preamp, Master, Bass, Middle, Treble, Presence; advanced: bias, sag_amount, cabinet_ir, oversample_factor. All smoothed, click-free. **Cost target:** Newton–Raphson × 8× × ~6 nonlinear stages, **< 1.0 ms per 256-block on one core**.

### As built in D5b

- **Parameters** are eight, not six: Preamp, Master (dB), Bass, Middle, Treble, Presence,
  Input trim (dB), Output (dB). Master, Input trim and Output are calibrated in decibels via
  `Calibration::decibels_to_gain`; the rest stay normalized `[0, 1]`. Every parameter is a
  `SmoothedParameter` target set by `Jcm800::set_parameter(std::uint16_t, float)` — index-based,
  RT-safe — and applied with a per-block `skip(frames)` step rather than per-sample ramping.
  Defaults are calibrated so a −12 dBFS input lands the amp's output at roughly −6 dBFS.
- **Cabinet** runs on `StreamingConvolution` (§4.2/§4.4's partition, chosen by
  `StreamingConvolution::partition_size_for` — 128 frames at or below 50 kHz, 256 above) behind
  an `RtSwapSlot<StreamingConvolution>`, so `set_cabinet(Ir)` can install a new impulse response
  from the control thread at any time, at any host block size, with `reclaim_cabinets()` freeing
  the previous engine once the audio thread has moved past it. `latency_samples()` reports the
  oversampler's latency plus the active cabinet's partition size, and moves after `set_cabinet()`
  — a host should re-read it whenever the cabinet changes.
- **Output limiter removed.** The soft-knee limiter added after D4 is gone; the output stage is
  a plain clamp to full scale. Calibration (input trim + the −6 dBFS default operating point)
  is what keeps normal playing levels off the clamp, not a limiter.

### As built in D6a

- **One 8× region, not two.** The sag stage and `JcmToneStack` now run inside the same
  oversampled region as V1a/V1b/V2a/PI, instead of the chain being split into two oversampled
  regions with a base-rate gap between them. **This raises the reported latency, it does not
  halve it:** at D5b each `Oversampler8x` round trip cost 12 base-rate samples and the amp ran
  two regions, for 24 in total; one region built from the new 115/27/15-tap halfbands costs 65,
  so the amp goes 24 → 65 base-rate samples — about 0.85 ms more latency at 48 kHz, and a
  host-visible behaviour change.
- **Where the alias floor actually came from.** The D5b baseline is −3.8 dB at `preamp = 10`
  (two regions, 15-tap stages). The new halfbands alone, still with two regions, take it to
  −32.0 dB. Collapsing to a single region then *regresses* it to −20.4 dB: the intermediate
  decimate/interpolate pair that the collapse removes was itself filtering some of what the
  measurement sees, so the single-region route on its own is worse than the two-region one.
  The Miller poles below are what recover it and go past the target, to −43.4 dB.
- **Three Miller-effect grid poles.** V1b, V2a and the phase inverter each gain a physically
  derived one-pole lowpass (`Biquad::set_one_pole_low_pass`, prewarped bilinear) on their grid
  node, modeling the 12AX7's Miller capacitance seen through a 100 kΩ source: `C_in = C_gk`
  (1.6 pF) `+ C_gp` (1.7 pF) `· (1 + |A|) + 10 pF` stray (a stated assumption — the stray term is
  not derived from a real JCM800 layout, and is the knob to turn if the amp should sound
  brighter). At the stage gains involved this puts the corner at 13 804 Hz (V1b, |A| = 60),
  13 804 Hz (V2a, |A| = 60) and 19 576 Hz (PI, |A| = 40). Adding these three poles is what takes
  the measured alias floor the rest of the way, from −20.4 dB to −43.4 dB at `preamp = 10` — past
  the D6a acceptance target of ≥ 30 dB improvement over the D5b baseline of −3.8 dB. Their audible
  cost is a whole-amp treble rolloff (preamp 0.6, knobs centred): −0.06 dB at 1 kHz, −1.33 dB at
  5 kHz, −4.64 dB at 10 kHz, −8.76 dB at 15 kHz. The bias-mismatch asymmetry re-measures at
  2.17 % second harmonic active vs. 2.93 % zeroed, up from 1.77 % / 2.45 % before D6a: the
  absolute figures moved because the drive reaching the power stage changed, while the
  mismatch's own effect — it removes about 0.76 pp of second harmonic — is unchanged.
- Since DA0 the four stage classes live in `include/SushiDSP/circuits/stages/`, owned by the
  core, so any amplifier module can use them; `modules/jcm800` keeps only `JcmToneStack` and
  `Jcm800`.

### As built in DA4

`TriodeStage` gained a seven-argument constructor that gives its plate a two-sided `tanh` ceiling
set by how far the real plate can swing toward the supply and toward the bottom of its load line
(a `PlateSwing`), and on that path the grid-conduction diode references the cathode rather than
the bias point, so a stage's clean swing matches its bias voltage the way the real grid does. The
six-argument constructor is unchanged and stays bit-identical. With a physical ceiling in place,
all four amplifier modules dropped the level-staging devices (JCM800's five scalars, the 5150's
seven `k*LevelStaging` constants and its derived power-grid lift, the VH4's four staging
constants, the IIC+'s per-stage normalisation) that used to keep every following grid off its
solver bound; each dossier's own dividers now run at face value, in volts, from the first grid to
the power tubes. Two circuit facts came with that change, applied by every module: the phase
inverter is a long-tailed pair, so its grid-cathode window is the input divided by the tail
degeneration `1 + (R_tail ∥ Z_shunt) / Z_k` of the spec's section 4
(`docs/superpowers/specs/2026-09-07-triode-plate-swing-design.md`), not the full swing a
common-cathode stage would see; and `PentodeStage`'s grid is not volt-true, so the link from a preamp stage's volts into its
grid carries a derived `kPowerTubeGridScale` bridge until `PentodeStage` is rebuilt in DA5. Every
coupling capacitor moved to `Biquad::set_first_order_high_pass`, the numerically stable one-pole
shape for a physical RC link, in place of the second-order `set_high_pass`, which is unsafe below
a `cutoff_hz / sample_rate` of `1e-4`. Because the dossiers' own dividers run this hot, "no grid on
its bound" is not achievable at these gain structures; the acceptance criterion each module's
README documents instead is a clean end at minimum gain, non-decreasing THD across the knob, and
a −6 dBFS ± 1 dB default peak. The JCM800's alias floor regressed to −6.7 dB against the −33.8 dB
bound as a result — every stage now clips at the level its own circuit puts it at, feeding more
harmonics into the 8× fold — and is carried as a known issue (`docs/reference/KNOWN_ISSUES.md`)
until DA5's antialiasing work.

### §5a Amplifier family

The DA phase (plans `docs/superpowers/plans/2026-09-06-high-gain-amps-part1.md` and
`docs/superpowers/plans/2026-09-06-high-gain-amps-part2.md`) grew the
single JCM800 into four sibling amplifier modules sharing the stage classes above.

| Module | Channel | Dossier | Module path | Status |
| --- | --- | --- | --- | --- |
| JCM800 | Marshall JCM800 2203, one channel | — | `modules/jcm800/` | White-box, see "As built" above. DA4: plate-swing chain. |
| Peavey 5150 | Block-letter 1992 5150, lead channel | `docs/reference/research/2026-09-06-peavey-5150-design-dossier.md` | `modules/peavey5150/` | White-box against the dossier's schematic set. DA4: plate-swing chain. |
| Mesa/Boogie Mark IIC+ | Lead mode | `docs/reference/research/2026-09-06-mesa-mark-iic-plus-design-dossier.md` | `modules/mesa_mark2c/` | White-box against the dossier's schematic set. DA4: plate-swing chain. |
| Diezel VH4 | Channel 3 ("Mega") | `docs/reference/research/2026-09-06-diezel-vh4-design-dossier.md` | `modules/diezel_vh4/` | Documented approximation: the gain chain is a reverse-engineering drawing, the tone stack's interconnection is a reconstruction, and the power section is built from prose alone. DA4: plate-swing chain. |

Each module's own README lists exactly where its model departs from its dossier.

---

## §6 Tube Screamer overdrive (white-box) — product module `tubescreamer`

One composite `IAudioNode` (`TubeScreamer`) modeling the TS808/TS9 signal path: input buffer stage, an op-amp soft-clipping stage with a diode feedback pair, and a passive tone/output stage, all via WDF/DK-method on the same solver as JCM800 (§4.5), at the same 8× oversampling. Exposes `tube_screamer_descriptor()`.

```
DI in ─▶ input buffer (JFET/op-amp, unity gain) ─▶ high-pass (input cap)
      ─▶ [op-amp clipping stage: gain network + diode-pair feedback loop, Drive-controlled]
      ─▶ [passive tone stack: single Tone pot RC network]
      ─▶ output level (Level control) ─▶ out
```

- **Op-amp clipping stage:** the core TS nonlinearity — an inverting op-amp with a parallel diode pair (2× silicon diodes, antiparallel) in the feedback path; modeled as a WDF sub-tree with the diode pair as the nonlinear root, solved by the shared Newton–Raphson solver (§4.5). `drive` sets the feedback resistance (a potentiometer), directly setting the clipping threshold and asymmetry.
- **Tone stack:** single-pot passive RC tone network (the characteristic TS "tone" shaping, a fixed low-pass/high-pass blend) via DK-method, matching the real circuit's frequency-dependent interaction with the pot position.
- **Input/output buffering:** unity-gain buffer stages modeled as linear WDF elements (no nonlinear cost).

**Parameters (`NodeDescriptor`):** Drive, Tone, Level. All smoothed, click-free. **Cost target:** one Newton–Raphson nonlinear stage × 8×, **< 0.2 ms per 256-block on one core** (far cheaper than JCM800 — one nonlinear stage vs. ~6).

### As built in D5b

**Parameters** are four, not three: Drive, Tone, Level (dB), Input trim (dB) — indexed via
`TubeScreamer::set_parameter(std::uint16_t, float)`, each a `SmoothedParameter` stepped once per
block. Level and Input trim are calibrated in decibels the same way as the JCM800's Master/Input
trim/Output; defaults land the pedal's output around −12 dBFS for a −12 dBFS input. `drive` still
sets the clipper's feedback resistance directly. No cabinet stage; `latency_samples()` reports
only the oversampler's latency. Level is the one knob applied per sample rather than once per
block, so its smoothing time does not depend on the host's block size.

---

## §7 Products, device backend & wrappers

A **product module** is a self-contained library (its own DSP) that exposes one `NodeDescriptor` (§3) and nothing else. It never references a device or a host.

Three products exist: `jcm800` (§5), `tubescreamer` (§6), and `dd3`, the Boss DD-3 delay built in DR2 against `docs/superpowers/specs/2026-09-05-dd3-spx90-design.md`. The DD-3 has no section of its own here; `modules/dd3/README.md` carries its block table, the three constants that are calibrations rather than component values, and what it leaves unmodelled, and the spec carries the rulings behind them. The SPX90 joins them in DR3 behind the same seam.

From these modules, two consumers are built by generic (non-product-specific) code:

- **§7.1 `IAudioDevice`** — injected output backend. First implementation SDL2; WASAPI-exclusive/ASIO/CoreAudio/ALSA/PipeWire later, each behind the same seam.
- **§7.2 Standalone host** — a generic app that opens an `IAudioDevice`, lets the user build/edit a rig from the registered `NodeDescriptor`s, and runs it with a simple parameter UI. Rig presets save/load as portable `{descriptor.id, {param_id → value}}` lists.
- **§7.3 VST3 / CLAP wrapper** — one generic adapter: maps host process-block → rig graph `process()`; the rig itself (not a single product) is what the plugin hosts, so the plugin's own UI is the rig editor. Built with the CLAP/VST3 SDK (or iPlug2). Depends on **SushiDSP only** — zero external runtime dependency.
- **§7.4 Command/param plumbing** — a lock-free SPSC ring carries parameter changes, rig-edit commands, and events from the control/UI thread to the RT thread; SushiDSP provides its own generic `SpscRing<T>`. **As built in D5b:** `RigGraph` owns an `SpscRing<ParameterChange>`; `RigGraph::push_parameter_change` is the control-thread-facing producer (used by `RigController` in `apps/host_gui`), and `RigGraph::process` drains the ring at the top of every block before running any node, dispatching each change to the target node's `IAudioNode::set_parameter(std::uint16_t, float)`. Nodes recompute derived coefficients on the audio thread only when their `SmoothedParameter` actually moved that block, so an idle knob costs nothing.

---

## §8 Repository, modules & packaging

SushiDSP is a standalone repository with a developer CLI **`sd`** (`sd configure` / `sd build` / `sd test`, growing `sd run` / `sd plugin` as those targets land). It is a registered SushiStack member (`hub add sushidsp` / `hub link sushidsp <path>`, alias `sd`) — this buys the shared CLI presentation layer (`sushicore`, §11a) and dependency-fragment aggregation (`cli/sushistack.deps.toml`), not a code dependency: the `sushi_dsp` core target itself still links nothing beyond the C++17 standard library and SIMD intrinsics (§0/§11), and building/testing this repo standalone (outside any SushiStack workspace) works unchanged.

### Layout

```
sushidsp/
  include/SushiDSP/
    core/     rig graph, IAudioNode, NodeDescriptor, buffer pool, SpscRing, IAudioDevice
    math/     fft, convolution, filters, resamplers, wdf, complex, windows, simd
  src/                       core/math impls (portable C++/SIMD)
  modules/                   ← the separable PRODUCTS, each a library exposing a NodeDescriptor
    jcm800/                    §5 amp: WDF/DK stages, tone stack, cabinet — depends only on core+math
    peavey5150/                §5a amp: lead channel, shared stage classes (its own README)
    mesa_mark2c/               §5a amp: lead mode, five-band graphic EQ (its own README)
    diezel_vh4/                §5a amp: channel 3, documented approximation (its own README)
    tubescreamer/              §6 pedal: WDF/DK clipping stage, tone stack — depends only on core+math
    dd3/                       DR2 delay: variable-rate loop, 12-bit ring, NE570 compander (its own README)
  apps/
    host/                      the standalone rig host (SDL2, IAudioDevice, rig editor UI)
    plugin/                    the VST3/CLAP rig wrapper
  examples/  rig_demo
  tests/     Unit_Dsp golden-vector + RT-safety
  cli/       the `sd` CLI
```

### Targets & dependency rules

- **`SushiDSP::sushi_dsp`** — the portable core (`core`+`math`). C++17 stdlib + SIMD only, **zero** external deps.
- **`SushiDSP::jcm800`, `SushiDSP::peavey5150`, `SushiDSP::mesa_mark2c`, `SushiDSP::diezel_vh4`, `SushiDSP::tubescreamer`, `SushiDSP::dd3`** — product libs, each depending only on `sushi_dsp`. Fully separable; a product can graduate to its own repo with no code change, because it couples to the core only through headers and to consumers only through its `NodeDescriptor`.
- **Exported & namespaced** — SushiDSP ships install/export rules producing `SushiDSP::*` targets and a `SushiDSPConfig.cmake`.

Tests: `TEST(Unit_Dsp, …)` for FFT/convolution/filter/WDF-solver numerical correctness (golden-vector), rig compile/recompile, RT-safety (no-alloc trap); per-product physical-plausibility tests (DC stability, frequency response, THD sweep) in their module. Perceptual A/B validation against real hardware captures is deferred — no reference captures available yet. **As built in D6a:** `tests/measure/` is a test-side-only measurement library (`render`, `thd`, `alias_floor`, `frequency_response`, `null_test`) that every product's plausibility and alias-floor test builds on, so those numbers come from one audited implementation instead of each test computing its own FFT/THD by hand. `tests/golden/` replaces the old five-sample-snapshot regression tests with 1-second rendered `.f32` files at 48 kHz and 96 kHz, compared at −80 dB RMS residual; regenerate them with `SUSHIDSP_REGENERATE_GOLDEN=1 sd test -s regression` and add a row to `tests/golden/README.md` recording why. A SPICE reference corpus is staged under `docs/reference/spice/` (netlist-directive `.asc` files, Koren/Norman `.sub` models, `tools/spice/export.py`, `tests/measure/SpiceCsv.hpp`) but nothing has been simulated yet — LTspice is not installed in this environment, so producing the golden CSVs (`python tools/spice/export.py --all`, then commit) is a user-side step, not something this repo's tests currently check against.

---

## §9 Roadmap (prefix **D** = Dsp)

- **D0 — Skeleton + math kernels.** Repo, `sd` CLI, `sushi_dsp` target, `IAudioNode`/rig graph/`NodeDescriptor`, transient pool, FFT (§4.1) + filters (§4.3) with golden-vector tests. *Ships: headless graph processes a verified tone/filter sweep.*
- **D1 — Device seam + standalone host.** `IAudioDevice` (SDL2), RT-safe callback, `SpscRing`, generic `apps/host` with a minimal rig editor. *Ships: sound out the speakers, zero XRuns.*
- **D2 — WDF/DK solver core.** §4.5 shared nonlinear-circuit infrastructure (WDF adaptors, DK-method state-space, Newton–Raphson solver), partitioned convolution (§4.2), oversampling (§4.4), golden-vector tests against known WDF reference circuits. *Ships: a minimal one-stage WDF test circuit processes correctly.*
- **D3 — Tube Screamer (`modules/tubescreamer`).** §6 op-amp clipping + tone stack; `tube_screamer_descriptor()`. *Ships: TS standalone in the rig host, plausibility-tested (DC stability, THD sweep).* Chosen before JCM800 as the simpler single-nonlinear-stage circuit to validate the solver core.
- **D4 — JCM800 (`modules/jcm800`).** §5 white-box chain + cabinet convolution; `jcm800_descriptor()`. *Ships: JCM800 in the rig host, plausibility-tested.*
- **D5 — Rig recompile + presets.** Full free-graph rig editing (add/remove/reorder any product, series/parallel), atomic-swap recompile, portable rig-preset save/load. *Ships: a rig with TS → JCM800 chained, edited live with no dropout.*
- **D6 — VST3/CLAP wrapper.** `apps/plugin` hosting the rig graph as a DAW plugin with its own rig-editor UI. *Ships: SushiDSP.vst3/.clap loads in a DAW, hosts TS + JCM800.*
- **D7 — Hardening & packaging.** Namespaced export/Config finalized, perceptual A/B validation once reference captures are available.
- **DA — High-gain amplifiers.** §5a: three sibling amp modules (`peavey5150`, `mesa_mark2c`, `diezel_vh4`) on the shared stage classes, registered in the node registry with their own front-panel layouts. *Ships: all four amps selectable in the rig host, RT-safety-trapped, panel- and registry-tested.*
- **DA4 — Plate-swing `TriodeStage`.** Spec `docs/superpowers/specs/2026-09-07-triode-plate-swing-design.md`. A physical plate ceiling replaces the level-staging devices in all four amplifiers; the phase-inverter tail degeneration and the `PentodeStage` grid bridge become explicit, derived constants. *Ships: all four amps on the seven-argument `TriodeStage`, a clean end and non-decreasing THD on every gain knob, two known issues carried to DA5 (the JCM800's alias floor, the IIC+'s undocumented loop return).*
- **DA5 — Antialiasing and the volt-true power stage.** Antiderivative antialiasing in `TriodeStage`, or 16× oversampling, to recover the alias floor DA4 cost; `PentodeStage` rebuilt volt-true; the six-argument `TriodeStage` path and `PlateSwing::apply`'s duplication deleted once `PentodeStage` shares it; the shelf-corner rule and the transformer's DC pass (final review F2, F5, F7).

**DS — stereo, tracks and playback, a parallel track.** Spec
`docs/superpowers/specs/2026-09-07-stereo-tracks-design.md`, plan
`docs/superpowers/plans/2026-09-07-stereo-tracks.md`. It runs beside the D phases because nothing
in it touches the circuit core: it is the graph's channel routing, a capture module and the host.
§3a describes what it built.

| Phase | What it builds | Status |
| --- | --- | --- |
| DS0 | `RigGraph` routes as many channels per edge as its two nodes declare; `channels_adapt()`, `live_buffer_count()`, `kMaxChannels = 2` | Landed 2026-09-07, commit 394a605 |
| DS1 | `routing::PanNode` on a constant-power law, the same table in `ChannelSelection`, two output channels from both hosts' devices | Landed 2026-09-07, commits 99796e6, bc774fa, 02a9a1a |
| DS2 | `modules/capture`'s recording half — `Take`, `TrackRecorder`, `RecordingBus` — and `io::WavWriter` | Landed 2026-09-07, commits 39c1a02, 18eb040 |
| DS3 | `Transport` and `TrackPlayer`: eight tracks, gain, pan, mute, solo, and a loop range packed into one 64-bit word | Landed 2026-09-07, commit 394a605 (attribution, see `docs/reference/KNOWN_ISSUES.md`) |
| DS4 | The host: a stereo output, record and play in the callback, a transport bar, a track panel, the re-amp input switch, the preset's track list and loop range | Landed 2026-09-07, commits af6a9ed, da342fc, afa0627 |
| DS5 | OGG export through `libvorbis`/`libogg`, inside `apps/host_gui` only | Deferred; written up once a user asks for it |

**DR — delay and reverb, a parallel track.** Spec: `docs/superpowers/specs/2026-09-05-dd3-spx90-design.md`. It runs beside the D phases instead of inside them, because neither unit needs the DK core: no block in either is a nonlinear network wanting a Newton solve. **DR1** built `math/Resampler` (§4.4, "As built in DR1"). **DR2** built the DD-3, `modules/dd3/`. **DR3** builds the SPX90 engine and the eight programs that calibrate against the Surefyre IR pack; **DR4** adds the remaining 22 and the graph integration; DS0 built the per-node multi-channel routing it waited on.

---

## §10 SOLID

- **SRP:** every `IAudioNode` does one DSP job; math kernels, device, and each product are separate.
- **OCP:** new products (amps, pedals, cabinets) and the deferred neural-amp path plug in behind existing interfaces; the rig never names concrete node types; new products register a `NodeDescriptor` without touching the rig, host, or plugin wrapper.
- **LSP:** every node honors "fill exactly `frames`, allocation-free"; any node is substitutable in any rig slot; any `NodeDescriptor` is hostable by the generic wrapper.
- **ISP:** narrow interfaces — `IAudioNode`, `IAudioDevice` — no god-interface.
- **DIP:** the core depends on abstractions; the platform device is injected.

---

## §11 Dependencies

- **Platform:** SDL2 for the first `IAudioDevice` (PRIVATE, host target only); the CLAP/VST3 SDK for the wrapper (PRIVATE, plugin target only).
- **Everything else:** C++17 standard library + SIMD intrinsics only. No other SushiStack repository's *code*, no GPU runtime, no neural-inference dependency — see §11a for the CLI-only exception.

## §11a SushiStack CLI membership (developer tooling only, not a code dependency)

SushiDSP is a registered SushiStack module (`sushistack/cli/sushistack/services/modules.py`), linked into a developer's workspace with `hub link sushidsp <path>` or cloned with `hub add sushidsp`, exactly like sushiruntime/sushiengine/sushiai/sushiblas. This buys two things, both developer-tooling-only:

1. **`cli/sushistack.deps.toml`** — SushiDSP's dependency fragment (today: SDL2), aggregated by `hub install` alongside the other modules' fragments into the workspace's shared `dependencies/` tree. SushiDSP has no `[module] depends_on` entry — it needs no other stack module's build output.
2. **`sushicore`** — the shared CLI presentation layer (themed console output, layered `config.toml`/`config.local.toml` loading, workspace-root discovery) that `sr`/`se`/`sa`/`sb` already build on. `cli/sushidsp/console.py` and `cli/sushidsp/config.py` are thin wrappers over `sushicore.build_console`/`sushicore.config_base.ToolConfig`, matching those CLIs' shape exactly (see any of them for the pattern). `sushicore` is not a resolvable pip dependency (unpublished), so `cli/pyproject.toml` does not list it — `hub install-cli sushidsp` injects it the same way it does for every other module.

None of this touches `sushi_dsp`'s C++ link graph. A checkout of this repo with no SushiStack workspace around it still configures and builds with plain `cmake`/`ctest` — `sd`'s commands (`configure`/`build`/`test`) are POSIX/Windows subprocess wrappers around those, and `sushicore`'s console degrades to an unthemed default (not a crash) when no `cli/config.toml` is reachable (see `console.py`'s lazy-build docstring).

---

## §12 Open items

Known work with no phase attached. An item leaves this list when it lands, not when it is planned.

- **No mechanical comment-shape check.** `tools/documentation/check_source_comments.py` does not exist in this repo. `docs/CONTRIBUTING.md` §3 states the shape and every reviewer applies it by reading, so the rule holds by attention rather than by a script.
- **DR2 shipped without a single measured number.** The build was locked while the DD-3 was written, so the 0.15 ms benchmark budget is unjudged, every acceptance row is unrun, and `tests/golden/dd3_48k.f32` and `dd3_96k.f32` do not exist, which fails the two regression cases. The changelog's DR2 "Not yet measured" paragraph names the three commands that close this.
