# SushiDSP — portable real-time DSP core: block graph, spatializer, synthesis, JCM800, aero/thermo

> **For the implementing agent.** This is the authoritative design specification for the **SushiDSP** repository — a standalone SushiStack sibling of `sushiruntime` and `sushiengine`. Build the repo from this doc. Conventions to follow throughout: **C++17** (no C++20/23), **Allman braces** (opening brace on its own line; trivial one-line accessors may stay compact), types `PascalCase`, functions/variables `snake_case`, members trailing underscore (`impl_`), constants `UPPER_SNAKE`, spelled-out names (no abbreviations), the **Apache 2.0 license header box** on every source file (see any `sushiruntime`/`sushiengine` source for the exact box; line 2 carries the file's own name), a Doxygen `@brief`/`@param`/`@return` on every public function, and **no comments unless the *why* is non-obvious**. The developer CLI is **`sd`** (Python Typer, mirroring `sr`/`se`). Cross-references below to `docs/design/audio_system.md` and `docs/slop/REALTIME_EXECUTION_PLAN.md` live in the sibling `sushiengine` and `sushiruntime` repos respectively; they describe the *consumers* and the *optional GPU host* and are not required to build SushiDSP.

SushiDSP is the shared audio-DSP layer that sits **between** SushiRuntime and its consumers. It is the reusable core that both the game engine (`SushiEngine::Audio`) and standalone plugins (JCM800.vst3/.clap, an aero/thermo instrument) build on, so the expensive engineering — the block DSP graph, the FFT/convolution/filter math, the ambisonic spatializer, the procedural synthesis framework, the white-box JCM800 amp model, and the aeroacoustic generators — is written **once** and consumed by both. Its hard design commitment: the DSP core is **portable C++ (SIMD, no SYCL)** and depends on nothing but the standard library, so a VST built on it ships without the SYCL/hwloc toolchain; GPU offload is an **optional** capability injected behind the `IDspAccelerator` seam, and only that seam links SushiRuntime. This keeps the layering clean and one-way: `consumer → SushiDSP`, with `SushiDSP → SushiRuntime` optional and confined to one target.

**Status:** Design only — nothing implemented. Roadmap phases **D0–D12** below are all pending. The portable CPU core (D0–D5) is the critical path and has no dependency outside its own phases. The GPU-offload phase (D8) depends on SushiRuntime work packages WP-RT-1/2/3 (`sushiruntime/docs/slop/REALTIME_EXECUTION_PLAN.md`) and substrate WP-6/WP-7, but the CPU core needs none of them.

**Repository decision (owner, 2026-07-24):** SushiDSP is a **separate SushiStack repository**, the owner's long-intended standalone audio library. **JCM800 and the aero/thermo generators are separate products** — each its own library module inside this repo, with its own standalone app and VST3/CLAP plugin target — but structured so **porting one into SushiEngine is a drop-in, not a rewrite**. The mechanism that guarantees the easy port is the **`NodeDescriptor` contract** (§3): every product is packaged as an `IAudioNode` factory + a parameter schema, and *both* the VST wrapper and the game's audio layer consume the identical descriptor — so porting a product into the game is "register its descriptor as a SoundBank source", with zero product-specific glue on either side. Repository, module, and product-target layout is §11.

---

## §0 Goals and hard acceptance criteria

1. **Portable, dependency-free CPU core.** The DSP graph, all `IAudioNode`s, and the math library compile and run with only a C++17 standard library and SIMD intrinsics — no SYCL, no SushiRuntime, no platform audio API. A headless unit test processes a graph to a float buffer on any toolchain. GPU and device I/O are injected, never assumed.
2. **Real-time-safe processing.** `IAudioNode::process()` performs no allocation, no lock, no syscall, no exception. Verified by a test harness that traps `malloc`/lock calls on the processing thread across a 30-minute soak; the bar is zero violations.
3. **One core, three products.** The same block graph + math + synthesis substrate hosts (a) a game spatial mixer, (b) the JCM800 amp chain, (c) the aero/thermo generators, with no product-specific forks in the core.
4. **Cinema-grade spatializer math.** Third-order ambisonic encode/decode, SOFA HRTF binaural decode, ISO 9613-1 air absorption, and **physically-exact delay-line Doppler** (fractional-delay, zipper-free, propagation-delay-correct — §5.2b) are implemented as pure `IAudioNode`s independent of any world model.
4a. **Fast, controllable 3D sound.** Per-voice spatialization is SIMD-batched and LOD-tiered (§5.5): a near/important voice gets full ambisonic + HRTF + sinc-Doppler; a distant/low-priority voice gets a cheap pan + Hermite-Doppler. Doppler is exact yet costs one fractional read per sample; a fast flyby (jet/bullet) produces a clean, zipper-free, alias-free pitch bend at any speed, with a hard max-pitch clamp so teleports never chirp.
5. **JCM800 fidelity.** White-box 2203 circuit (WDF / DK state-space), ≥ 8× oversampling, six front-panel controls, partitioned-convolution cabinet, A/B-validated against a reference capture within perceptual tolerance.
6. **Physically-derived aero/thermo.** Wind, fire, and blast synthesized from a small physical parameter vector, running real-time on the CPU core.
7. **Optional GPU offload, invisible when absent.** With an `IDspAccelerator` injected, convolution/HRTF/batch work moves to SushiRuntime with k-block lookahead; with none injected, the identical graph runs fully on CPU. No `#ifdef` in consumer code — the seam is a runtime injection.
8. **VST/standalone wrapping.** A thin VST3/CLAP wrapper hosts any SushiDSP product in a DAW with no reference to a game engine.
9. **SOLID + conventions.** Apache headers, Allman braces, `I`-prefixed interfaces, Doxygen on every public function, trivially-copyable POD where it crosses a thread.

---

## §1 Why SushiDSP is a separate layer

1. **The VST cannot depend on a game engine.** A DAW plugin has no ECS, no game loop, no snapshot seam, and must not link them. The products' shared value (amp model, generators, spatializer, graph) therefore cannot live in an engine. It lives one layer down, reachable by both an engine and a plugin.
2. **The VST should not require the SYCL toolchain.** SushiRuntime links `IntelSYCL::SYCL_CXX` (PUBLIC) + hwloc. A DAW plugin that drags a SYCL runtime and hwloc into a host process is a distribution and compatibility liability. So the DSP core must be buildable with **zero** SushiRuntime dependency, and GPU acceleration must be optional and isolated.
3. **The real-time path is CPU anyway.** Audio's low-latency mix runs on the device-callback thread; the GPU is only for batch/tail/lookahead work (§8/§9). A portable CPU core loses nothing on the interactive path and gains portability everywhere.
4. **SushiRuntime is a pure orchestrator.** DSP math does **not** belong in the runtime. SushiDSP is exactly the kind of domain compute layer that lives above it.

> **Clarification (do not misread the "no SYCL" rule).** "No SYCL in the core" does **not** mean SushiDSP avoids SushiRuntime. SushiDSP **does** link SushiRuntime and **does** use SYCL/GPU — but only through the isolated `accel-sushiruntime` target that implements `IDspAccelerator` (§9/§11). The core, the product modules, and the plugin/standalone targets stay SYCL-free; the game build additionally links the accel target for GPU offload. The hard invariant driving this split: the real-time per-block mix runs on the device-callback thread under a ~5 ms deadline, so it **cannot** run on the GPU regardless — GPU serves only lookahead-tolerant batch work (convolution tails, HRTF, aeroacoustic fields). Therefore every DSP unit needs a CPU implementation no matter what, and SYCL is an *additive* acceleration path, never a replacement for the CPU core.

```
         ┌──────────────────────┐        ┌──────────────────────┐
         │  SushiEngine::Audio  │        │  VST3 / CLAP / app    │
         │  (game integration)  │        │  (no engine)          │
         └──────────┬───────────┘        └───────────┬──────────┘
                    └───────────────┬────────────────┘
                                    ▼
                  ┌───────────────────────────────────────┐
                  │             SushiDSP                    │
                  │  block DSP graph · IAudioNode ·          │
                  │  NodeDescriptor · buffer pool ·          │
                  │  FFT/convolution/filters/resamplers ·    │
                  │  ambisonics+HRTF · synthesis · JCM800 ·   │
                  │  aero/thermo · IAudioDevice · IDsp-      │
                  │  Accelerator (seam)                     │
                  │  PORTABLE C++/SIMD — no SYCL, no engine  │
                  └─────────────────┬─────────────────────┘
                                    ┆ optional, one target only
                                    ▼
                  ┌───────────────────────────────────────┐
                  │  SushiRuntime (GPU offload: convolution,│
                  │  HRTF batch, aeroacoustic field eval)   │
                  │  reached ONLY via IDspAccelerator impl  │
                  └───────────────────────────────────────┘
```

---

## §2 Survey conclusions — what we adopt and why

| Source | What we take (math/structure only — no code, no dependency) |
|---|---|
| JUCE `dsp`, chowdsp | Oversampling (polyphase halfband cascade), partitioned-convolution structure, WDF adaptor class shape. |
| Google Resonance / IEM Plug-in Suite | ACN/SN3D ambisonic encode; AllRAD / mode-matching decoders; dual-band ambisonic-to-binaural. |
| SOFA (AES69) / libmysofa | HRTF asset format; barycentric HRIR interpolation on the measurement sphere. |
| Zölzer *DAFX*; Yeh/Pakarinen/Werner (WDF/tube papers); Smith *PASP* | Triode models (Koren/Norman), DK-method state-space, digital waveguides, modal synthesis, Karplus-Strong. |
| Lighthill analogy; FW-H; von Kármán spectrum; Strouhal shedding; Friedlander blast | The physics the aero/thermo generators derive from (§8). |
| pffft / KISS FFT (reference) | Split-radix real-FFT structure for the portable CPU FFT (§4.1) — reference for correctness, own implementation. |

**Skip list:** vendor DSP libraries as hard dependencies (own the math for portability); neural amp/cabinet (white-box chosen — §7; a neural node stays an OCP seam, unbuilt); full CFD (parametric physical models — §8); MPEG-H/Atmos encoder licensing (internal object+bed, ADM-BWF export deferred).

---

## §3 Architecture — the block DSP graph

A directed acyclic graph of `IAudioNode`s, topologically sorted once and **replayed per block**. The graph is a plain C++ object walked on whatever thread drives it (the device-callback thread in a game/VST, a test thread in unit tests). It does **not** use SushiRuntime's scheduler on the real-time path — RT audio wants a deterministic single-thread topological walk with zero handoff, not a work-stealing fan-out. SushiRuntime enters only for optional batch offload (§9).

```
 IAudioNode graph (compiled once, replayed per 256-frame block):

   [Source]─┐                                   transient buffer pool
   [Source]─┼─▶[Effect]─▶[Send]──┐              (block-sized float channels,
   [Source]─┘           └▶[Bus]──┼─▶[Spatializer]─▶[Bus master]─▶[Meter/Limiter]─▶ out
   [Synth ]──────────────────────┘                        │
   [JCM800]──▶[Cabinet conv]─────────────────────────────┘
   [Aero  ]──▶ ...

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
        double              time_seconds; ///< monotonic block time for LFOs/scheduling
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

    /// Optional GPU/heterogeneous offload. The ONLY seam that may touch SushiRuntime.
    /// With no implementation injected, every node runs its CPU path.
    struct IDspAccelerator
    {
        virtual ~IDspAccelerator() = default;
        virtual BufferId  alloc(std::size_t floats, Residency r) = 0;
        virtual void      upload(BufferId dst, const float* src, ElementRange range) = 0;
        virtual JobId     submit_convolution(BufferId in, BufferId ir, BufferId out, /*...*/) = 0;
        virtual bool      ready(JobId job) const = 0;        ///< non-blocking; RT thread never waits
        virtual void      download(BufferId src, float* dst, ElementRange range) = 0;
    };
}
```

### Product contract — the `NodeDescriptor` (the easy-port seam)

A **product** (JCM800, a wind generator, a reverb, any shippable DSP unit) is packaged as a self-describing factory. This single POD contract is what makes a product simultaneously (a) a standalone app, (b) a VST3/CLAP plugin, and (c) a drop-in game-engine SoundBank source — with no product-specific code in any consumer.

```cpp
namespace SushiDSP
{
    enum class ParamKind { Linear, Logarithmic, Decibel, Boolean, Choice };
    struct ParameterDescriptor
    {
        const char* id;          ///< stable, e.g. "preamp", "master", "wind_speed"
        const char* label;       ///< UI name
        float       min, max, default_value;
        ParamKind   kind;
        const char* unit;        ///< "dB", "Hz", "m/s", ""
    };

    /// Everything a host needs to instantiate and drive a product, with no knowledge of its internals.
    struct NodeDescriptor
    {
        const char*                       id;         ///< "sushidsp.jcm800", "sushidsp.wind"
        const char*                       name;       ///< "JCM800", "Aeolian Wind"
        span<const ParameterDescriptor>   parameters; ///< the full automation/RTPC surface
        PortLayout                        ports;      ///< in/out channel counts
        std::unique_ptr<IAudioNode>     (*make)(const PrepareInfo&);  ///< the factory
    };

    /// Each product exposes exactly one of these. Registration is a free function.
    const NodeDescriptor& jcm800_descriptor();       // from the jcm800 module
    const NodeDescriptor& wind_descriptor();          // from the aerothermo module
    // ...
}
```

- **The VST/CLAP wrapper** reads `parameters` → declares host automation params; calls `make()` → owns the node; forwards `process()`. One generic wrapper serves every product.
- **A game engine** reads the *same* `parameters` → registers each as an RTPC on a SoundBank source; `make()` builds the node on a voice. Porting a product = adding its `NodeDescriptor` to the bank registry. Nothing else.
- **Presets are portable:** a preset is `{descriptor.id, {param_id → value}}`, so a patch tuned in the VST loads unchanged in the game and vice versa.

This contract is the load-bearing guarantee behind "products are separate but the port is trivial."

### Graph mechanics

- **Transient buffer pool** — block-sized float channel buffers with liveness-based reuse; a node's output frees back to the pool once its last consumer reads it. A 500-node graph holds only a handful of live buffers.
- **Recompile off the RT thread** — structural changes (add/remove node, reroute) recompile on a control thread; the new graph is swapped in via an atomic pointer flip, the old retired after a grace block. Mirrors SushiRuntime's compile-once/replay discipline without using its scheduler.
- **Dynamic voices/effects** — add/remove without full recompile follows a region-keyed dynamic-graph idiom, reimplemented here as CPU graph regions so it works with no runtime present.

---

## §4 DSP math library (portable, SIMD)

All numeric building blocks. SushiRuntime ships **none** of these, so SushiDSP owns them. Each is a plain C++/SIMD implementation with a scalar fallback; the same source optionally compiles to a SYCL kernel for the GPU path (§9).

- **§4.1 FFT/DFT** — split-radix real FFT (forward/inverse), power-of-two sizes, in-place, cache-blocked. Foundation for partitioned convolution and spectral analysis. Own implementation (portability > a vendor bind); a SYCL/oneMKL bind is an optional accelerator path.
- **§4.2 Partitioned convolution** — uniform-partitioned (256-frame) overlap-add FFT convolution for reverb IRs and the cabinet IR; non-uniform partitioning for low-latency long tails. CPU by default; offloads to `IDspAccelerator::submit_convolution` when injected.
- **§4.3 Filters** — TPT/zero-delay-feedback state-variable filter (SVF), biquad cascade (RBJ cookbook coefficients), nonlinear Moog ladder, one-pole/one-zero, all-pass; per-block-smoothed parameters, click-free.
- **§4.4 Resamplers & fractional delay** — cubic **Hermite (Catmull-Rom, 4-point)** and **windowed-sinc (8-point)** fractional-delay/resampling for the delay-line Doppler (§5.2), pitch, and sample-rate conversion; Lagrange optional; polyphase FIR halfband cascade for the amp oversampler (§7). **Linear interpolation is disallowed on the Doppler/pitch path** (aliasing + HF loss). All fractional readers are SIMD-batched across voices.
- **§4.5 Complex + windowing** — a POD complex type, Hann/Blackman-Harris/Kaiser windows, Hilbert transform.
- **§4.6 Analysis** — RMS/peak, true-peak (4× oversampled), ITU-R BS.1770 K-weighted gated loudness integrator (`double` accumulator), FFT spectrum for editor scopes.
- **§4.7 Portability** — a thin SIMD abstraction (SSE/AVX/NEON with a scalar fallback) so the core builds on any target; no intrinsics leak into public headers.

### Fixed capacities

| Cap | Value | Where |
|---|---|---|
| Sample rate (base) | 48 000 Hz | `DeviceFormat`, configurable 44.1/48/96 |
| Block size | 256 frames (5.33 ms) | configurable 64–1024 |
| Ambisonic order | 3 (16 channels, ACN/SN3D) | scene bus |
| FDN size | 16 delay lines | late reverb |
| Convolution partition | 256 frames | uniform-partitioned FFT |
| Amp oversample factor | 8× (→ 384 kHz) | `Jcm800` node |
| GPU lookahead | 2–4 blocks | `IDspAccelerator` |

---

## §5 Spatializer (ambisonic + HRTF)

The *math* of spatialization lives here as pure nodes; the *world-driven parameters* (which emitter is where, occlusion rays) are computed by the consumer and pushed in. A VST can drive the same nodes from panner UI instead.

- **§5.1 Ambisonic encode** — evaluate real spherical harmonics `Y_l^m(direction)` to order 3 (16 ACN-ordered, SN3D-normalized coefficients); scatter a mono source into the 16-channel B-format bus weighted by those coefficients. Source **spread/size** attenuates higher orders; near-field compensation (NFC) filters for close sources.
- **§5.2 Distance / air absorption.** Distance attenuation curves; ISO 9613-1 frequency-dependent atmospheric absorption as a distance-driven one-pole low-pass.
- **§5.2b Doppler — time-varying fractional delay-line read (the load-bearing choice).** Doppler is **not** an explicit pitch-ratio resampler driven by `(c−v)/(c−v)`; it is an emergent property of a per-voice **delay line** whose read pointer sits `distance/c · sample_rate` samples behind the write. As source–listener distance changes, the read pointer advances at a rate ≠ 1, and **that rate *is* the Doppler shift** — physically exact, and the finite speed of sound (propagation delay — you hear the jet where it *was* — plus the flyby pitch bend) falls out for free, with **no per-block discontinuity**. This is simultaneously the **cheapest** (a delay line + one fractional read per sample, no transcendentals, no FFT) and the **highest-quality** model. Quality is entirely: **(a)** the interpolator — Hermite default, windowed-sinc ultra (§4.4), never linear; **(b) per-sample smoothing of the read delay** — slew/interpolate the control-rate distance across the block so fast flybys never zipper; **(c)** an **anti-alias tracking low-pass** engaged only when the pitch-up ratio is extreme (supersonic approach), driven by the instantaneous read rate. **Controls (for "fast & controllable"):** `doppler_scale` (0 = off, 1 = physical, > 1 = exaggerated for drama), a **max-pitch clamp** (teleport/glitch safety — a position jump must never chirp), and a **propagation-delay toggle** (decouple pitch from distance-delay when a cue must stay latency-tight). Delay buffers come from a pool; reads are SIMD-batched across voices. The consumer supplies only per-voice distance + radial velocity; SushiDSP owns the delay-line read. Cost is O(interpolation) per sample and stays on the RT CPU path (never GPU — it is per-sample and latency-critical).
- **§5.3 Ambisonic decode** — selectable: **binaural** (dual-band ambisonic-to-binaural over a SOFA HRIR set, per-order HRTF matrices via §4.2 convolution, scene-rotation for head-tracking), **7.1.4** (AllRAD/mode-matching), **stereo** fold-down.
- **§5.4 Reverb** — FDN (16-line) late reverb parameterized by RT60 per band; optional convolution reverb (§4.2) with a baked IR; early-reflection taps encoded to ambisonics at arrival direction (the *geometry* of reflections — image-source / ray tracing — is a consumer concern; SushiDSP renders the taps it is given).

---

## §6 Procedural synthesis framework

The shared substrate for all non-sample sound, reused by §7 and §8. A **synth patch** is an `IAudioNode` sub-graph, authored as data (`.sushipatch`) or built in code. Primitives (each an `IAudioNode`): band-limited oscillators (PolyBLEP/wavetable, mip-mapped), noise (white/pink/velvet/band-limited), filters (§4.3), envelopes/LFOs (ADSR/DAHDSR, tempo-sync), a **modal resonator bank** (parallel biquad resonators — impacts/ring/metal/glass and resonant aero tones), **digital waveguides** (Karplus-Strong strings/tubes/membranes — whistles, edge tones), a **granular** scheduler (textures, crackle, debris), and oversampled **waveshapers** (shared with §7). A **modulation matrix** routes any source (envelope, LFO, external parameter, physical parameter) to any destination parameter, smoothed per block — this is what lets gameplay physics drive synthesis in §8.

---

## §7 JCM800 amp simulation (white-box) — product module `jcm800`

One composite `IAudioNode` (`Jcm800`) modeling the Marshall JCM800 2203 signal path with **Wave Digital Filters** for modular reactive sub-circuits and the **DK-method (discrete Kirchhoff state-space)** for tightly-coupled blocks (the tone stack), at **8× oversampling**. Exposes `jcm800_descriptor()`.

```
DI in ─▶ input/bright cap ─▶ [V1a 12AX7] ─▶ coupling cap (tightness HPF) ─▶ [V1b 12AX7]
      ─▶ Marshall tone stack (Bass/Mid/Treble) ─▶ [V2a] ─▶ [phase inverter LTP]
      ─▶ [EL34 push-pull class-AB: grid conduction + crossover] ─▶ [output transformer:
         primary inductance + leakage + core saturation] ─▶ [power-supply sag: rectifier+RC → dynamic B+]
      (global NFB: Presence/Resonance) ─▶ downsample ─▶ [cabinet IR: partitioned convolution] ─▶ out
```

- **Triode stages (12AX7):** WDF sub-tree terminated by a nonlinear triode root; plate/grid currents from **Koren/Norman** equations, solved per oversampled sample by **Newton–Raphson** (2–4 iterations, warm-started from the previous sample). Cathode bypass + grid-leak set level/frequency dependence; bright cap adds low-gain sparkle.
- **Tone stack:** the passive Marshall (FMV) network via DK-method (or 3-port WDF) so Bass/Mid/Treble interact exactly as the real passive network (mid scoop, treble/bass coupling).
- **Phase inverter:** 12AX7 long-tailed pair; models the asymmetry that seeds even-order content.
- **Power section (EL34 push-pull, class AB):** pentode model with screen dependence; **grid conduction** clamps positive grid swing (aggressive compression when pushed); crossover emerges from the class-AB bias point.
- **Output transformer:** primary inductance (low-end rolloff, load interaction), leakage (top shaping), Jiles-Atherton-lite core saturation (brown compression at high output).
- **Power-supply sag:** rectifier + reservoir RC → **dynamic B+**; transients pull the rail down, adding touch sensitivity and bloom.
- **Cabinet:** measured 4×12 IR via §4.2 partitioned convolution (GPU-offloadable), mic-model pre-filter, swappable IR.

**Parameters (`NodeDescriptor`):** Preamp, Master, Bass, Middle, Treble, Presence; advanced: bias, sag_amount, tube_type, cabinet_ir, oversample_factor. All smoothed, click-free. **Cost:** Newton–Raphson × 8× × ~6 nonlinear stages dominates; target **< 1.0 ms per 256-block on one core**, cabinet convolution optionally on GPU. **Validation:** A/B null-test the linear regime and perceptually match the driven regime against a reference DI-through-2203 capture. A neural tube/cabinet node behind the same `IAudioNode` seam is an OCP extension (unbuilt).

---

## §8 Thermodynamic & aerodynamic generators — product module `aerothermo`

Each generator is a synth patch (§6) driven by a small **physical parameter vector** supplied by the consumer (gameplay/physics), no runtime CFD. Each exposes its own `NodeDescriptor` (`wind_descriptor()`, `fire_descriptor()`, `blast_descriptor()`, `jet_descriptor()`).

- **§8.1 Wind (aerodynamic).** Broadband body: filtered noise shaped by the **von Kármán turbulence spectrum**, wind speed `U` sets level + spectral tilt; multiple placed "wind voices" with gust modulation from a supplied wind field. **Aeolian tones:** vortex shedding gives `f = St·U/d` (`St ≈ 0.2`), a bank of narrow resonators (§6) tracks `f`/amplitude (`~U^3`) — wires, fences, gaps, swung objects (FW-H surface-velocity modulation for fast movers).
- **§8.2 Fire/combustion (thermodynamic).** Roar: low-frequency monopole driven by unsteady heat-release rate (`~ d(Q̇)/dt`), low-passed noise tracking intensity. Crackle: **Poisson-timed** filtered-impulse grains, rate ∝ fuel/moisture. Hiss: HF filtered noise for small flames.
- **§8.3 Explosions/blast.** **Friedlander waveform** `p(t)=P·(1−t/t*)·e^(−t/t*)` for overpressure (`P`, `t*` from energy/distance); shock **N-wave** close in; layered roar + granular debris + large-spread diffuse tail. Flash-to-arrival delay from speed-of-sound propagation.
- **§8.4 Jet/exhaust/rocket.** Turbulent mixing noise with **Lighthill `U^8`** velocity scaling; shock-cell/screech tones for supersonic.
- **§8.5 GPU option.** When many aero emitters are active, per-emitter control-parameter evaluation (sampling `U`/`f`/level from a field) batches onto `IDspAccelerator`; the synthesis stays on CPU (cheap, latency-sensitive). Offline CFD→bake remains an OCP seam (unbuilt).

---

## §9 Optional GPU offload (`IDspAccelerator`)

The single seam that may link SushiRuntime. With none injected, every node runs its CPU path and SushiDSP has **zero** SushiRuntime/SYCL dependency. When injected (the game build), the implementation wraps a borrowed `Runtime&` and maps to:

- **Allocation** — `runtime.buffer<float>(n)` (device-resident IR/HRTF tables via `Residency::Device`; per-callback host-filled blocks via `Residency::Shared` + `write_range`), `runtime.state<float>(n)` for feedback lines. Handles fixed after recording (SushiRuntime `buffer.hpp` move caveat).
- **Dispatch** — `runtime.graph().add(NdRange<1>, fn)` for partitioned FFT convolution; `add(Reads,Writes,n,fn)` for element-wise; `add(State<float>,fn)` for delay evolution. Runs on a **dedicated GPU-worker thread**, not the RT thread.
- **Lookahead** — the worker runs k blocks ahead (k = 2–4 ≈ 10–20 ms); the RT thread reads results for block `N` submitted at `N−k` and **never** blocks on a job (`ready()` is non-blocking). Load-bearing decision: latency-critical work on CPU, tails/beds/convolution/HRTF-batch/aeroacoustic-field on GPU lookahead.

The runtime-side changes that make this coexist with the RT thread without jitter (core reservation, RT thread class, realtime profile, cheap async stepping) are specified in `sushiruntime/docs/slop/REALTIME_EXECUTION_PLAN.md` (WP-RT-1/2/3) and build on substrate WP-6/WP-7. **None are required for the CPU-only build.**

---

## §10 Products, device backend & wrappers

A **product module** is a self-contained library (`core` + its own DSP) that exposes one or more `NodeDescriptor`s (§3) and nothing else. It never references a device, a host, or a game engine. From that one library, three consumers are built by generic (non-product-specific) code:

- **§10.1 `IAudioDevice`** — injected output backend. First implementation SDL2; WASAPI-exclusive/ASIO/CoreAudio/ALSA/PipeWire later, each behind the same seam.
- **§10.2 Standalone host** — a generic app that opens an `IAudioDevice`, instantiates a chosen `NodeDescriptor`, and runs it with a simple parameter UI. One host binary can audition any product; per-product convenience apps (`jcm800_standalone`, `aerothermo_standalone`) are one-line wrappers naming the descriptor.
- **§10.3 VST3 / CLAP wrapper** — one generic adapter templated on a `NodeDescriptor`: maps host process-block → `IAudioNode::process()`, host params → `parameters`, built with the CLAP/VST3 SDK (or iPlug2). Each product's plugin (`JCM800.vst3/.clap`, `AeroThermo.vst3/.clap`) is a thin target that hands the wrapper its descriptor. Depends on **SushiDSP only** — no engine, no SYCL (CPU build).
- **§10.4 Game-engine port** — the game's audio layer registers the *same* `NodeDescriptor` as a SoundBank source. This is the "easy port": a product proven in its VST is dropped into the game by adding its descriptor to the bank registry — identical factory, identical parameters, portable presets.
- **§10.5 Command/param plumbing** — a lock-free SPSC ring carries parameter changes and events from the control/UI thread to the RT thread. SushiRuntime exposes no reusable ring, so SushiDSP provides its own generic `SpscRing<T>`.

---

## §11 Repository, modules & packaging (SushiStack)

SushiDSP is its own SushiStack repository, sibling to `sushiruntime`/`sushiengine`. It declares its dependencies in `cli/sushistack.deps.toml` (aggregated by `ss install`) and ships a developer CLI **`sd`** (`sd build` / `sd test` / `sd run <product>` / `sd plugin <product>`), following the `sr`/`se` convention. It is linked into the workspace with `ss link` and provisions SDL2 + the CLAP/VST3 SDK; SushiRuntime is an **optional** provision, pulled only for the GPU-accel target.

### Layout

```
sushidsp/
  include/SushiDSP/
    core/     graph, IAudioNode, NodeDescriptor, buffer pool, SpscRing, IAudioDevice, IDspAccelerator
    math/     fft, convolution, filters, resamplers, complex, windows, simd
    spatial/  ambisonics, hrtf, air-absorption, doppler, fdn/convolution reverb
    synth/    oscillators, noise, filters, envelopes, modal, waveguide, granular, modulation, patch
  src/                       core/math/spatial/synth impls (portable C++/SIMD, no SYCL)
  modules/                   ← the separable PRODUCTS, each a library exposing NodeDescriptor(s)
    jcm800/                    §7 amp: WDF/DK stages, tone stack, cabinet — depends only on core+math
    aerothermo/                §8 generators: wind, fire, blast, jet — depends only on core+math+synth
  apps/
    host/                      one generic standalone host (picks a descriptor)
    plugin/                    one generic VST3/CLAP wrapper (templated on a descriptor)
    jcm800_standalone/  jcm800_plugin/       ← thin targets naming jcm800_descriptor()
    aerothermo_standalone/  aerothermo_plugin/
  accel-sushiruntime/        the OPTIONAL IDspAccelerator impl; the ONLY target that links sushiruntime
  examples/  synth_demo, spatializer_demo
  tests/     Unit_Dsp golden-vector + RT-safety
  cli/       sushistack.deps.toml + the `sd` CLI
```

### Targets & dependency rules

- **`SushiDSP::sushi_dsp`** — the portable core (`core`+`math`+`spatial`+`synth`). C++17 stdlib + SIMD only, **zero** external deps.
- **`SushiDSP::jcm800`, `SushiDSP::aerothermo`** — product libs, each depending only on `sushi_dsp`. Fully separable; a product can later graduate to its **own** SushiStack repo with no code change, because it couples to the core only through headers and to consumers only through its `NodeDescriptor`.
- **`SushiDSP::sushi_dsp_accel`** — optional; links `sushiruntime` and implements `IDspAccelerator`. The game build links it; VST/standalone builds do not (so a plugin never pulls SYCL/hwloc).
- **Exported & namespaced** — SushiDSP ships install/export rules producing `SushiDSP::*` targets and a `SushiDSPConfig.cmake`, so a game engine and each plugin `find_package(SushiDSP)` cleanly.
- **Consumer consumption** — a game engine links `SushiDSP::sushi_dsp` + whichever product libs it ships (`jcm800`, `aerothermo`) + `sushi_dsp_accel` in the GPU build, and registers their descriptors as SoundBank sources.

Tests: `TEST(Unit_Dsp, …)` for FFT/convolution/filter numerical correctness (golden-vector), graph compile/replay, RT-safety (no-alloc trap); per-product A/B and physical-plausibility tests in their module.

---

## §12 Roadmap (prefix **D** = Dsp)

**Core track (critical path):**
- **D0 — Skeleton + math kernels.** Repo, `sd` CLI, `sushi_dsp` target, `IAudioNode`/graph/`NodeDescriptor`, transient pool, FFT (§4.1) + filters (§4.3) with golden-vector tests. *Ships: headless graph processes a verified tone/filter sweep.*
- **D1 — Device seam + standalone host.** `IAudioDevice` (SDL2), RT-safe callback, `SpscRing`, generic `apps/host`. *Ships: sound out the speakers, zero XRuns.*
- **D2 — Convolution + resamplers.** Partitioned convolution (§4.2), windowed-sinc resampler (§4.4). *Ships: convolution reverb + pitch/Doppler on CPU.*
- **D3 — Spatializer.** Ambisonic encode/decode, SOFA HRTF, air absorption (§5). *Ships: binaural + 7.1.4 + stereo from one bus.*
- **D4 — Reverb.** FDN + convolution reverb, early-reflection tap rendering (§5.4).
- **D5 — Synthesis framework.** §6 primitives + patch graph + modulation matrix. *Ships: `synth_demo`.*

**Product tracks (parallel once D5 lands; each ends in a shippable standalone + plugin):**
- **D6 — JCM800 (`modules/jcm800`).** §7 white-box chain + cabinet convolution; `jcm800_standalone` + `JCM800.vst3/.clap`. *Ships: A/B validated amp + plugin.*
- **D7 — Aero/thermo (`modules/aerothermo`).** §8 generators; `aerothermo_standalone` + `AeroThermo.vst3/.clap`. *Ships: physically-driven generators + plugin.*

**Cross-cutting:**
- **D8 — GPU offload.** `accel-sushiruntime` implements `IDspAccelerator`, lookahead convolution/HRTF (§9). *Depends on `REALTIME_EXECUTION_PLAN.md` WP-RT-1/2/3.* *Ships: GPU path with RT thread never waiting; CPU path unchanged when absent.*
- **D9 — Mastering.** BS.1770 meter + true-peak limiter (§4.6).
- **D10 — Generic plugin wrapper.** `apps/plugin` hardened for any descriptor; preset format.
- **D11 — Head-tracking + high-tier acoustics.** Scene rotation binaural, non-uniform partitioned convolution.
- **D12 — Packaging.** Namespaced export/Config finalized; per-product repo split available (§11).

---

## §13 SOLID

- **SRP:** every `IAudioNode` does one DSP job; math kernels, device, accelerator, and each product are separate.
- **OCP:** new nodes/products/decoders/devices and the deferred neural-amp / CFD-bake paths plug in behind existing interfaces; the graph never names concrete node types; new products register a `NodeDescriptor` without touching any consumer.
- **LSP:** every node honors "fill exactly `frames`, allocation-free"; any node is substitutable; any `NodeDescriptor` is hostable by the generic wrapper.
- **ISP:** narrow interfaces — `IAudioNode`, `IAudioDevice`, `IDspAccelerator` — no god-interface.
- **DIP:** the core depends on abstractions; the platform device and the SushiRuntime accelerator are injected. SushiRuntime is reached only through `IDspAccelerator`, so the CPU core has no downward dependency at all.

---

## §14 Dependencies

- **Runtime (optional, D8 only):** `IDspAccelerator` maps to SushiRuntime's fluent API and benefits from `REALTIME_EXECUTION_PLAN.md` WP-RT-1 (async/armed step), WP-RT-2 (core reservation), WP-RT-3 (realtime profile), and substrate WP-6 (device-resident state) / WP-7 (native CPU path). The CPU core depends on **none** of these.
- **Consumers:** a game engine's audio layer supplies world-driven spatial parameters, voice management, and its snapshot extract, and registers product `NodeDescriptor`s; a VST wrapper supplies host param/callback plumbing. Both depend on SushiDSP; neither reaches around it into SushiRuntime for DSP.
- **Platform:** SDL2 for the first `IAudioDevice` (PRIVATE); the CLAP/VST3 SDK for the wrapper (PRIVATE, plugin targets only).
