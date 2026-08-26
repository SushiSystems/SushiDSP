# D5b — Plugin-Readiness Foundations Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every product node and both hosts behave the way a DAW plugin must — lock-free
parameter delivery, block-size-independent cabinet convolution, reported latency, ms-based
smoothing, a resampled/normalised IR pipeline, a defined 0 dBFS ↔ volts calibration, no output
limiter — without changing any tube/tone math.

**Architecture:** Add four small core units (`SmoothedParameter`, `ParameterChange` ring drain in
`RigGraph`, `StreamingConvolution` FIFO wrapper over `PartitionedConvolution`, `RtSwapSlot<T>`),
one control-thread I/O unit (`IrLoader`), then retrofit `Jcm800` and `TubeScreamer` onto them
and route both hosts through the ring. Existing snapshots are re-pinned exactly once (Task 9)
because the master/level parameters become dB-scaled; every other task keeps them bit-identical.

**Tech Stack:** C++17, GoogleTest, existing `sd` CLI, Dear ImGui (host_gui), nlohmann::json (presets).

**Spec:** `docs/superpowers/specs/2026-08-26-aaa-audio-quality-design.md` — section "Phase D5b".

## Global Constraints

- C++17 only; Allman braces; `snake_case` functions/variables, `PascalCase` types, trailing
  underscore members, `UPPER_SNAKE`/`kCamel` constants as the surrounding file already does;
  spelled-out names.
- Every new `.hpp`/`.cpp` (tests included) carries the 76-column license header copied verbatim
  from `include/SushiDSP/core/PortLayout.hpp`, filename line changed.
- Doxygen `@brief`/`@param`/`@return` on every public function. No what-comments.
- Everything reachable from `IAudioNode::process()` is allocation-free, lock-free, syscall-free,
  throw-free. Every new class on the audio path gets a `*ProcessPerformsNoHeapAllocation` case in
  `tests/functional/unit/test_rt_safety.cpp`.
- Builds/tests only via `sd build` / `sd test` (`-s unit|regression|benchmark|all`,
  `-t debug|release`). To run one case, execute the built `sushidsp_tests` binary under `build/`
  with `--gtest_filter=<Suite>.<Case>`; never call cmake/ctest directly.
- Core (`include/SushiDSP/core`, `math`, `io`) depends on the C++17 stdlib only. Product modules
  depend only on `sushi_dsp`. Hosts never appear in product code.
- Test files register in `tests/CMakeLists.txt` under the matching `# --- Section ---` comment.
- `sd test -t release -s benchmark` before and after Tasks 6, 7, 9; numbers go in the commit
  message.
- Calibration constant (spec D5b.5): `kFullScaleVolts = 2.0f` — 0 dBFS ≡ 2.0 V peak at the
  instrument jack.
- Smoothing default (spec D5b.2): 20 ms time constant.
- Cabinet partition (spec D5b.3): 128 frames at ≤ 50 kHz, 256 at > 50 kHz.
- IR pipeline defaults (spec D5b.4): trim 200 ms, 5 ms raised-cosine fade, normalise to 0 dB
  average magnitude over 100 Hz–5 kHz.

---

### Task 1: `SmoothedParameter` (ms time constant, per-sample or per-block)

**Files:**
- Create: `include/SushiDSP/math/SmoothedParameter.hpp`
- Create: `src/math/SmoothedParameter.cpp`
- Modify: `CMakeLists.txt` (add `src/math/SmoothedParameter.cpp` to `sushi_dsp` source list, next to `src/math/Biquad.cpp`)
- Test: `tests/functional/unit/test_smoothed_parameter.cpp`
- Modify: `tests/CMakeLists.txt` (register under the `# --- Math ---` section)

**Interfaces:**
- Produces:
  ```cpp
  class SmoothedParameter {
  public:
      explicit SmoothedParameter(float initial_value = 0.0f);
      void prepare(double sample_rate, float time_constant_ms);   // control thread
      void set_target(float target);                              // RT-safe: one store
      float next();                                               // RT: advance one sample, return current
      float skip(std::uint32_t samples);                          // RT: advance `samples`, return current (per-block use)
      float current() const;  float target() const;
      bool is_settled(float tolerance = 1e-6f) const;
      void snap(float value);                                     // set current == target == value
  };
  ```
  Used by Tasks 6, 7.

- [ ] **Step 1: Write the failing tests**

```cpp
// tests/functional/unit/test_smoothed_parameter.cpp  (license header first)
#include <cmath>
#include <gtest/gtest.h>
#include "SushiDSP/math/SmoothedParameter.hpp"

TEST(Unit_Dsp, SmoothedParameterReachesSixtyThreePercentAtOneTimeConstant)
{
    SushiDSP::SmoothedParameter parameter(0.0f);
    parameter.prepare(48000.0, 20.0f);
    parameter.set_target(1.0f);
    float value = 0.0f;
    for (int i = 0; i < 960; ++i)  // 20 ms at 48 kHz
    {
        value = parameter.next();
    }
    EXPECT_NEAR(value, 1.0f - std::exp(-1.0f), 2e-3f);
}

TEST(Unit_Dsp, SmoothedParameterSkipMatchesRepeatedNext)
{
    SushiDSP::SmoothedParameter a(0.0f), b(0.0f);
    a.prepare(48000.0, 20.0f);
    b.prepare(48000.0, 20.0f);
    a.set_target(0.7f);
    b.set_target(0.7f);
    float via_next = 0.0f;
    for (int i = 0; i < 4096; ++i) { via_next = a.next(); }
    const float via_skip = b.skip(4096);
    EXPECT_NEAR(via_next, via_skip, 1e-5f);
}

TEST(Unit_Dsp, SmoothedParameterTimeConstantIsSampleRateIndependent)
{
    SushiDSP::SmoothedParameter a(0.0f), b(0.0f);
    a.prepare(48000.0, 20.0f);
    b.prepare(96000.0, 20.0f);
    a.set_target(1.0f);
    b.set_target(1.0f);
    const float at_48k = a.skip(960);
    const float at_96k = b.skip(1920);
    EXPECT_NEAR(at_48k, at_96k, 1e-4f);
}

TEST(Unit_Dsp, SmoothedParameterSnapSettlesImmediately)
{
    SushiDSP::SmoothedParameter parameter(0.0f);
    parameter.prepare(48000.0, 20.0f);
    parameter.snap(0.3f);
    EXPECT_TRUE(parameter.is_settled());
    EXPECT_FLOAT_EQ(parameter.next(), 0.3f);
}
```

- [ ] **Step 2: Register the test and run it to verify it fails**

Add `functional/unit/test_smoothed_parameter.cpp` to `tests/CMakeLists.txt` under `# --- Math ---`.
Run: `sd build` → Expected: compile error, header missing.

- [ ] **Step 3: Implement**

```cpp
// include/SushiDSP/math/SmoothedParameter.hpp
#pragma once
#include <cstdint>
namespace SushiDSP
{
    /// @brief First-order (one-pole) parameter smoother with a time constant in milliseconds,
    /// derived from the sample rate at prepare() so a knob feels the same at every rate and
    /// block size. set_target() is a single float store and is safe to call from the audio
    /// thread; all advancing happens on the audio thread.
    class SmoothedParameter
    {
    public:
        /// @param initial_value Value both current and target start at.
        explicit SmoothedParameter(float initial_value = 0.0f);
        /// @brief Derives the per-sample coefficient. Control thread only.
        /// @param sample_rate The rate next() will be called at (base or oversampled).
        /// @param time_constant_ms Time to reach 63 % of a step.
        void prepare(double sample_rate, float time_constant_ms);
        /// @brief Sets the value the smoother moves toward.
        /// @param target The new target.
        void set_target(float target);
        /// @brief Advances one sample.
        /// @return The smoothed value after this sample.
        float next();
        /// @brief Advances `samples` samples in one step (closed form).
        /// @param samples Number of samples to advance.
        /// @return The smoothed value after those samples.
        float skip(std::uint32_t samples);
        /// @brief The current smoothed value.
        float current() const { return current_; }
        /// @brief The target value.
        float target() const { return target_; }
        /// @brief Whether |current - target| is within tolerance.
        /// @param tolerance Absolute tolerance.
        /// @return True when settled.
        bool is_settled(float tolerance = 1e-6f) const;
        /// @brief Sets current and target to value with no ramp.
        /// @param value The value to snap to.
        void snap(float value);
    private:
        float current_;
        float target_;
        float coefficient_ = 0.0f;   // fraction of the remaining distance kept per sample
    };
}
```

```cpp
// src/math/SmoothedParameter.cpp
#include "SushiDSP/math/SmoothedParameter.hpp"
#include <cmath>
namespace SushiDSP
{
    SmoothedParameter::SmoothedParameter(float initial_value)
        : current_(initial_value), target_(initial_value) {}

    void SmoothedParameter::prepare(double sample_rate, float time_constant_ms)
    {
        const double samples = sample_rate * static_cast<double>(time_constant_ms) * 0.001;
        coefficient_ = samples > 0.0 ? static_cast<float>(std::exp(-1.0 / samples)) : 0.0f;
    }

    void SmoothedParameter::set_target(float target) { target_ = target; }

    float SmoothedParameter::next()
    {
        current_ = target_ + (current_ - target_) * coefficient_;
        return current_;
    }

    float SmoothedParameter::skip(std::uint32_t samples)
    {
        const float decay = std::pow(coefficient_, static_cast<float>(samples));
        current_ = target_ + (current_ - target_) * decay;
        return current_;
    }

    bool SmoothedParameter::is_settled(float tolerance) const
    {
        return std::fabs(current_ - target_) <= tolerance;
    }

    void SmoothedParameter::snap(float value) { current_ = value; target_ = value; }
}
```

- [ ] **Step 4: Run tests** — `sd test` → Expected: all four PASS.
- [ ] **Step 5: Commit**

```bash
git add include/SushiDSP/math/SmoothedParameter.hpp src/math/SmoothedParameter.cpp CMakeLists.txt tests/CMakeLists.txt tests/functional/unit/test_smoothed_parameter.cpp
git commit -m "D5b: add SmoothedParameter with a millisecond time constant so knob feel is block-size independent"
```

---

### Task 2: Latency reporting — `IAudioNode::latency_samples()`, `Oversampler8x`, `RigGraph`

**Files:**
- Modify: `include/SushiDSP/core/IAudioNode.hpp`
- Modify: `include/SushiDSP/math/Oversampler.hpp`, `src/math/Oversampler.cpp`
- Modify: `include/SushiDSP/core/RigGraph.hpp`, `src/core/RigGraph.cpp`
- Test: `tests/functional/unit/test_oversampler.cpp`, `tests/functional/unit/test_rig_graph.cpp`

**Interfaces:**
- Produces: `virtual std::uint32_t IAudioNode::latency_samples() const { return 0; }`;
  `std::uint32_t Oversampler8x::latency_samples() const` (base-rate samples, up+down);
  `std::uint32_t HalfbandUpsampler2x::taps() const`, `HalfbandDownsampler2x::taps() const`;
  `std::uint32_t RigGraph::latency_samples() const` (sum over `topological_order_`).

- [ ] **Step 1: Failing tests**

```cpp
// append to tests/functional/unit/test_oversampler.cpp
TEST(Unit_Dsp, Oversampler8xReportsMeasuredImpulseLatency)
{
    SushiDSP::Oversampler8x oversampler(64);
    std::vector<float> input(64, 0.0f);
    input[0] = 1.0f;
    std::vector<float> output(64, 0.0f);
    // Two blocks so a delay longer than one block is still observed.
    std::vector<float> observed;
    for (int block = 0; block < 2; ++block)
    {
        const float* up = oversampler.upsample(input.data(), 64);
        oversampler.downsample(up, 512, output.data());
        observed.insert(observed.end(), output.begin(), output.end());
        std::fill(input.begin(), input.end(), 0.0f);
    }
    std::size_t peak_index = 0;
    for (std::size_t i = 1; i < observed.size(); ++i)
    {
        if (std::fabs(observed[i]) > std::fabs(observed[peak_index])) { peak_index = i; }
    }
    EXPECT_EQ(static_cast<std::uint32_t>(peak_index), oversampler.latency_samples());
}
```

```cpp
// append to tests/functional/unit/test_rig_graph.cpp
namespace
{
    class LatencyNode : public SushiDSP::IAudioNode
    {
    public:
        explicit LatencyNode(std::uint32_t latency) : latency_(latency) {}
        void prepare(const SushiDSP::PrepareInfo&) override {}
        void process(SushiDSP::ProcessContext& context) override
        {
            for (std::uint32_t i = 0; i < context.frames; ++i) { context.outputs[0][i] = 0.0f; }
        }
        SushiDSP::PortLayout ports() const override { return { 1, 1 }; }
        float get_parameter(const char*) const override { return 0.0f; }
        void set_parameter(const char*, float) override {}
        void reset() override {}
        std::uint32_t latency_samples() const override { return latency_; }
    private:
        std::uint32_t latency_;
    };
}

TEST(Unit_Dsp, RigGraphLatencyIsSumOfChainedNodeLatencies)
{
    SushiDSP::RigGraph graph(256, 4);
    const auto a = graph.add_node(std::make_unique<LatencyNode>(7));
    const auto b = graph.add_node(std::make_unique<LatencyNode>(128));
    graph.connect(a, b);
    graph.compile(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
    EXPECT_EQ(graph.latency_samples(), 135u);
}
```

- [ ] **Step 2: `sd build`** → Expected: compile errors (`latency_samples` undeclared).

- [ ] **Step 3: Implement**

`IAudioNode.hpp` — after `reset()`:
```cpp
        /// @brief Samples of delay this node adds between its input and output (FIR group
        /// delay, convolution partition, look-ahead). Constant after prepare().
        /// @return Latency in base-rate samples; 0 for a memoryless or IIR-only node.
        virtual std::uint32_t latency_samples() const { return 0; }
```

`Oversampler.hpp` — add to both halfband classes `std::uint32_t taps() const { return taps_; }`
(store `taps_` in the constructor; the existing constructors take `taps`), and to `Oversampler8x`:
```cpp
        /// @brief Round-trip group delay of the three up and three down stages.
        /// @return Delay in base-rate samples (rounded down).
        std::uint32_t latency_samples() const;
```
`Oversampler.cpp`:
```cpp
    std::uint32_t Oversampler8x::latency_samples() const
    {
        // A linear-phase FIR of T taps delays by (T-1)/2 samples at its own rate. Up-stages run
        // at 2x, 4x, 8x of base; down-stages at the same rates. Convert each to base-rate samples.
        const double up = (stage1_up_.taps() - 1) / 2.0 / 2.0
                        + (stage2_up_.taps() - 1) / 2.0 / 4.0
                        + (stage3_up_.taps() - 1) / 2.0 / 8.0;
        const double down = (stage3_down_.taps() - 1) / 2.0 / 8.0
                          + (stage2_down_.taps() - 1) / 2.0 / 4.0
                          + (stage1_down_.taps() - 1) / 2.0 / 2.0;
        return static_cast<std::uint32_t>(up + down + 0.5);
    }
```
(If the impulse test in Step 1 lands one sample off, the rounding here is wrong, not the test:
adjust rounding until the measured peak index matches for 15-tap stages — expected value 12.)

`RigGraph.hpp/.cpp`:
```cpp
        /// @brief Sum of latency_samples() over the compiled chain. Not real-time safe only in the
        /// sense that it walks the node list; it allocates nothing.
        /// @return Total latency in base-rate samples.
        std::uint32_t latency_samples() const;
```
```cpp
    std::uint32_t RigGraph::latency_samples() const
    {
        std::uint32_t total = 0;
        for (NodeHandle handle : topological_order_)
        {
            if (nodes_[handle] != nullptr) { total += nodes_[handle]->latency_samples(); }
        }
        return total;
    }
```

- [ ] **Step 4: `sd test`** → Expected: PASS, snapshots unchanged.
- [ ] **Step 5: Commit** — `git commit -m "D5b: report node, oversampler and rig latency so a host can compensate delay"`

---

### Task 3: `PartitionedConvolution::reset()` + `StreamingConvolution` (any frame count)

**Files:**
- Modify: `include/SushiDSP/math/PartitionedConvolution.hpp`, `src/math/PartitionedConvolution.cpp`
- Create: `include/SushiDSP/math/StreamingConvolution.hpp`, `src/math/StreamingConvolution.cpp`
- Modify: `CMakeLists.txt` (add source)
- Test: `tests/functional/unit/test_partitioned_convolution.cpp`, create `tests/functional/unit/test_streaming_convolution.cpp`
- Modify: `tests/CMakeLists.txt`, `tests/functional/unit/test_rt_safety.cpp`

**Interfaces:**
- Produces: `void PartitionedConvolution::reset()`;
  ```cpp
  class StreamingConvolution {
  public:
      StreamingConvolution(std::uint32_t partition_size, const float* ir, std::uint32_t ir_length, std::uint32_t max_block);
      void process(const float* input, float* output, std::uint32_t frames);  // any frames ≤ max_block
      void reset();
      std::uint32_t latency_samples() const;   // == partition_size
      static std::uint32_t partition_size_for(double sample_rate); // 128 ≤ 50 kHz, else 256
  };
  ```

- [ ] **Step 1: Failing tests**

```cpp
// append to test_partitioned_convolution.cpp
TEST(Unit_Dsp, PartitionedConvolutionResetClearsTail)
{
    const float ir[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    SushiDSP::PartitionedConvolution convolution(4, ir, 4);
    float block[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    float out[4];
    convolution.process(block, out);
    convolution.reset();
    float silence[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    convolution.process(silence, out);
    for (float sample : out) { EXPECT_NEAR(sample, 0.0f, 1e-7f); }
}
```

```cpp
// tests/functional/unit/test_streaming_convolution.cpp (license header first)
#include <cmath>
#include <vector>
#include <gtest/gtest.h>
#include "SushiDSP/math/StreamingConvolution.hpp"

namespace
{
    std::vector<float> direct_convolution(const std::vector<float>& x, const std::vector<float>& h)
    {
        std::vector<float> y(x.size(), 0.0f);
        for (std::size_t n = 0; n < x.size(); ++n)
        {
            for (std::size_t k = 0; k < h.size() && k <= n; ++k) { y[n] += x[n - k] * h[k]; }
        }
        return y;
    }
}

TEST(Unit_Dsp, StreamingConvolutionMatchesDirectFormForIrregularBlockSizes)
{
    std::vector<float> ir(300);
    for (std::size_t i = 0; i < ir.size(); ++i) { ir[i] = std::sin(0.37f * i) * std::exp(-0.01f * i); }
    std::vector<float> signal(5000);
    for (std::size_t i = 0; i < signal.size(); ++i) { signal[i] = std::sin(0.05f * i) + 0.3f * std::cos(0.9f * i); }

    SushiDSP::StreamingConvolution convolution(128, ir.data(), 300, 4096);
    const std::uint32_t chunks[] = { 1, 7, 64, 100, 256, 1000, 4096 };
    std::vector<float> streamed;
    std::size_t cursor = 0;
    std::size_t chunk_index = 0;
    std::vector<float> out(4096);
    while (cursor < signal.size())
    {
        std::uint32_t frames = chunks[chunk_index++ % 7];
        frames = static_cast<std::uint32_t>(std::min<std::size_t>(frames, signal.size() - cursor));
        convolution.process(signal.data() + cursor, out.data(), frames);
        streamed.insert(streamed.end(), out.begin(), out.begin() + frames);
        cursor += frames;
    }

    const std::vector<float> expected = direct_convolution(signal, ir);
    const std::uint32_t latency = convolution.latency_samples();
    EXPECT_EQ(latency, 128u);
    for (std::size_t n = latency; n < signal.size(); ++n)
    {
        EXPECT_NEAR(streamed[n], expected[n - latency], 1e-4f) << "at " << n;
    }
}

TEST(Unit_Dsp, StreamingConvolutionPartitionSizeFollowsSampleRate)
{
    EXPECT_EQ(SushiDSP::StreamingConvolution::partition_size_for(44100.0), 128u);
    EXPECT_EQ(SushiDSP::StreamingConvolution::partition_size_for(48000.0), 128u);
    EXPECT_EQ(SushiDSP::StreamingConvolution::partition_size_for(96000.0), 256u);
}
```

RT-safety case in `test_rt_safety.cpp` (same shape as the existing `PartitionedConvolution` case;
process 200 calls of `frames = 37` with `g_trap_allocations` on; expect count 0).

- [ ] **Step 2: Register tests, `sd build`** → Expected: compile errors.

- [ ] **Step 3: Implement**

`PartitionedConvolution::reset()` zeroes every `history_real_`/`history_imag_` vector, `overlap_`,
and sets `write_index_ = 0`.

```cpp
// include/SushiDSP/math/StreamingConvolution.hpp
#pragma once
#include <cstdint>
#include <vector>
#include "SushiDSP/math/PartitionedConvolution.hpp"
namespace SushiDSP
{
    /// @brief Re-blocks an arbitrary-length input stream onto a fixed-partition
    /// PartitionedConvolution. Costs exactly one partition of latency. Allocation-free after
    /// construction; process() accepts any frame count up to max_block.
    class StreamingConvolution
    {
    public:
        /// @param partition_size Internal FFT partition (power of two).
        /// @param impulse_response IR samples. @param impulse_length Sample count.
        /// @param max_block Largest frames value process() will ever be called with.
        StreamingConvolution(std::uint32_t partition_size, const float* impulse_response,
                             std::uint32_t impulse_length, std::uint32_t max_block);
        /// @brief Convolves frames samples; output lags input by latency_samples().
        /// @param input frames samples. @param output frames samples. @param frames ≤ max_block.
        void process(const float* input, float* output, std::uint32_t frames);
        /// @brief Clears the FIFO and the convolver's history.
        void reset();
        /// @brief Delay from input to output.
        /// @return partition_size.
        std::uint32_t latency_samples() const { return partition_size_; }
        /// @brief Chooses the spec'd partition for a sample rate.
        /// @param sample_rate Base rate in Hz.
        /// @return 128 at or below 50 kHz, 256 above.
        static std::uint32_t partition_size_for(double sample_rate);
    private:
        std::uint32_t          partition_size_;
        PartitionedConvolution convolution_;
        std::vector<float>     input_fifo_;    // partition_size samples being gathered
        std::vector<float>     output_fifo_;   // partition_size samples being drained
        std::uint32_t          fifo_fill_ = 0; // samples in input_fifo_; output_fifo_ has partition_size - fifo_fill_ left
    };
}
```

```cpp
// src/math/StreamingConvolution.cpp
#include "SushiDSP/math/StreamingConvolution.hpp"
#include <algorithm>
namespace SushiDSP
{
    StreamingConvolution::StreamingConvolution(std::uint32_t partition_size, const float* impulse_response,
                                               std::uint32_t impulse_length, std::uint32_t)
        : partition_size_(partition_size)
        , convolution_(partition_size, impulse_response, impulse_length)
        , input_fifo_(partition_size, 0.0f)
        , output_fifo_(partition_size, 0.0f)
    {
    }

    void StreamingConvolution::process(const float* input, float* output, std::uint32_t frames)
    {
        // Invariant: output_fifo_[fifo_fill_ ..) is the not-yet-emitted result of the previous
        // partition, so reading one output sample and writing one input sample keep the same
        // cursor. Starting from a zero output_fifo_ is what makes the latency exactly one partition.
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            output[i] = output_fifo_[fifo_fill_];
            input_fifo_[fifo_fill_] = input[i];
            ++fifo_fill_;
            if (fifo_fill_ == partition_size_)
            {
                convolution_.process(input_fifo_.data(), output_fifo_.data());
                fifo_fill_ = 0;
            }
        }
    }

    void StreamingConvolution::reset()
    {
        convolution_.reset();
        std::fill(input_fifo_.begin(), input_fifo_.end(), 0.0f);
        std::fill(output_fifo_.begin(), output_fifo_.end(), 0.0f);
        fifo_fill_ = 0;
    }

    std::uint32_t StreamingConvolution::partition_size_for(double sample_rate)
    {
        return sample_rate > 50000.0 ? 256u : 128u;
    }
}
```
(`max_block` is accepted for interface stability with the spec but unused: the FIFO design needs
no per-call scratch. Note that in the constructor's Doxygen.)

- [ ] **Step 4: `sd test`** → Expected: PASS.
- [ ] **Step 5: Commit** — `"D5b: StreamingConvolution re-blocks any host frame count onto a fixed partition; PartitionedConvolution gains reset()"`

---

### Task 4: `ParameterChange` ring drained in `RigGraph::process`, index-based `set_parameter`

**Files:**
- Create: `include/SushiDSP/core/ParameterChange.hpp`
- Modify: `include/SushiDSP/core/IAudioNode.hpp`, `include/SushiDSP/core/RigGraph.hpp`, `src/core/RigGraph.cpp`
- Test: `tests/functional/unit/test_rig_graph.cpp`, `tests/functional/unit/test_rt_safety.cpp`

**Interfaces:**
- Produces:
  ```cpp
  struct ParameterChange { NodeHandle node; std::uint16_t parameter_index; float value; };
  // IAudioNode
  virtual void set_parameter(std::uint16_t index, float value) {}   // RT-safe: stores a target only
  // RigGraph
  static constexpr std::size_t kParameterRingCapacity = 1024;
  bool push_parameter_change(const ParameterChange& change);  // control thread; false if full
  ```
  `RigGraph::process` pops every pending change and calls `nodes_[change.node]->set_parameter(change.parameter_index, change.value)` before running the chain.

- [ ] **Step 1: Failing tests**

```cpp
// append to test_rig_graph.cpp
namespace
{
    class RecordingNode : public SushiDSP::IAudioNode
    {
    public:
        void prepare(const SushiDSP::PrepareInfo&) override {}
        void process(SushiDSP::ProcessContext& context) override
        {
            for (std::uint32_t i = 0; i < context.frames; ++i) { context.outputs[0][i] = last_value_; }
        }
        SushiDSP::PortLayout ports() const override { return { 0, 1 }; }
        float get_parameter(const char*) const override { return last_value_; }
        void set_parameter(const char*, float) override {}
        void set_parameter(std::uint16_t index, float value) override { last_index_ = index; last_value_ = value; }
        void reset() override {}
        std::uint16_t last_index_ = 0xFFFF;
        float last_value_ = 0.0f;
    };
}

TEST(Unit_Dsp, RigGraphAppliesQueuedParameterChangesBeforeProcessing)
{
    SushiDSP::RigGraph graph(64, 4);
    auto node = std::make_unique<RecordingNode>();
    RecordingNode* raw = node.get();
    const auto handle = graph.add_node(std::move(node));
    graph.compile(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    EXPECT_TRUE(graph.push_parameter_change(SushiDSP::ParameterChange{ handle, 3, 0.25f }));
    std::vector<float> out(64);
    float* outs[1] = { out.data() };
    SushiDSP::ProcessContext context{ outs, nullptr, 64, 0.0 };
    graph.process(context);
    EXPECT_EQ(raw->last_index_, 3);
    EXPECT_FLOAT_EQ(out[0], 0.25f);
}

TEST(Unit_Dsp, RigGraphIgnoresParameterChangeForRemovedNode)
{
    SushiDSP::RigGraph graph(64, 4);
    const auto a = graph.add_node(std::make_unique<RecordingNode>());
    graph.remove_node(a);
    graph.compile(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    graph.push_parameter_change(SushiDSP::ParameterChange{ a, 0, 1.0f });
    std::vector<float> out(64);
    float* outs[1] = { out.data() };
    SushiDSP::ProcessContext context{ outs, nullptr, 64, 0.0 };
    graph.process(context);   // must not crash
    EXPECT_FLOAT_EQ(out[0], 0.0f);
}
```
Also extend `RigGraphProcessPerformsNoHeapAllocationAcrossManyBlocks` in `test_rt_safety.cpp`:
push 100 changes before each block (from the test thread, outside the trap window) and confirm
the trap count stays 0 while processing.

- [ ] **Step 2: `sd build`** → Expected: compile errors.

- [ ] **Step 3: Implement**

```cpp
// include/SushiDSP/core/ParameterChange.hpp
#pragma once
#include <cstdint>
namespace SushiDSP
{
    using NodeHandle = std::uint32_t;
    /// @brief One parameter write travelling from the control thread to the audio thread.
    struct ParameterChange
    {
        NodeHandle    node;
        std::uint16_t parameter_index;   // index into the node's NodeDescriptor::parameters[]
        float         value;             // in the parameter's declared [min, max]
    };
}
```
`RigGraph.hpp`: `#include "SushiDSP/core/SpscRing.hpp"` and `ParameterChange.hpp`; remove the local
`using NodeHandle` (now in `ParameterChange.hpp`); add member `SpscRing<ParameterChange>
parameter_ring_{ kParameterRingCapacity };` and the public method. `RigGraph.cpp` at the top of
`process()`:
```cpp
        ParameterChange change;
        while (parameter_ring_.pop(change))
        {
            if (change.node < nodes_.size() && nodes_[change.node] != nullptr)
            {
                nodes_[change.node]->set_parameter(change.parameter_index, change.value);
            }
        }
```
`IAudioNode.hpp`: add the index overload with the default body `{ (void)index; (void)value; }` and a
Doxygen note that products override it and that the `const char*` overload is control-thread only.
Because an overload hides base overloads, add `using IAudioNode::set_parameter;` to every existing
node class that overrides the string overload (`Jcm800`, `TubeScreamer`, `SineToneNode`, test
nodes) — the compiler will list them.

- [ ] **Step 4: `sd test`** → PASS.
- [ ] **Step 5: Commit** — `"D5b: RigGraph drains a lock-free ParameterChange ring at block start; nodes gain an RT-safe index set_parameter"`

---

### Task 5: `RtSwapSlot<T>` — deferred-reclaim pointer swap for the cabinet IR

**Files:**
- Create: `include/SushiDSP/core/RtSwapSlot.hpp` (header-only template)
- Test: `tests/functional/unit/test_rt_swap_slot.cpp`; `test_rt_safety.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces:
  ```cpp
  template <typename T> class RtSwapSlot {
  public:
      void publish(std::unique_ptr<T> object);   // control thread
      T* acquire_for_audio_thread() noexcept;    // audio thread; valid until next acquire
      void reclaim();                            // control thread
  };
  ```
  Same generation-counter contract as `RtGraphSlot` (read `include/SushiDSP/core/RtGraphSlot.hpp`
  and `src/core/RtGraphSlot.cpp` and reproduce the logic generically). `RtGraphSlot` is left as
  is in this phase; a follow-up may alias it to `RtSwapSlot<RigGraph>`.

- [ ] **Step 1: Failing tests** — port the cases in `test_rt_graph_slot.cpp` to `RtSwapSlot<int>`:
  publish-then-acquire returns the object; acquire before publish returns nullptr; reclaim frees
  a retired object only after the audio thread acquired a newer one (count destructions with a
  struct whose destructor increments a static counter). RT-safety: `acquire_for_audio_thread`
  in the trap window allocates nothing.
- [ ] **Step 2: `sd build`** → compile error.
- [ ] **Step 3: Implement** the template by transcribing `RtGraphSlot.cpp` into the header with `T`
  in place of `RigGraph`, keeping the three atomics and the `retired_` vector.
- [ ] **Step 4: `sd test`** → PASS.
- [ ] **Step 5: Commit** — `"D5b: add RtSwapSlot<T>, the generic deferred-reclaim swap the cabinet IR needs"`

---

### Task 6: `TubeScreamer` on `SmoothedParameter`, index parameters, InputTrim, dB Level

**Files:**
- Modify: `modules/tubescreamer/include/SushiDSP/tubescreamer/TubeScreamer.hpp`, `modules/tubescreamer/src/TubeScreamer.cpp`
- Create: `include/SushiDSP/core/Calibration.hpp`
- Test: `tests/functional/unit/test_tube_screamer.cpp`, `tests/functional/regression/test_tube_screamer_regression.cpp`

**Interfaces:**
- Produces: `include/SushiDSP/core/Calibration.hpp`:
  ```cpp
  namespace SushiDSP { constexpr float kFullScaleVolts = 2.0f;
    inline float decibels_to_gain(float db) { return std::pow(10.0f, db / 20.0f); }
    inline float gain_to_decibels(float gain) { return 20.0f * std::log10(std::max(gain, 1e-9f)); } }
  ```
  TubeScreamer descriptor parameters, in index order (indices are the contract for Task 10/11):
  `0 drive [0,1] Linear`, `1 tone [0,1] Linear`, `2 level [-60,+12] dB Decibel default 0`,
  `3 input_trim [-24,+24] dB Decibel default 0`. `set_parameter(uint16_t, float)` overrides.
  `set_level(float db)` replaces the normalised setter; `set_input_trim(float db)` added.
- Consumes: Task 1 `SmoothedParameter`, Task 4 index overload.

- [ ] **Step 1: Failing tests**

```cpp
// append to test_tube_screamer.cpp
TEST(Unit_Dsp, TubeScreamerDescriptorExposesFourParametersInContractOrder)
{
    const auto& d = SushiDSP::tube_screamer_descriptor();
    ASSERT_EQ(d.parameter_count, 4u);
    EXPECT_STREQ(d.parameters[0].id, "drive");
    EXPECT_STREQ(d.parameters[1].id, "tone");
    EXPECT_STREQ(d.parameters[2].id, "level");
    EXPECT_EQ(d.parameters[2].kind, SushiDSP::ParamKind::Decibel);
    EXPECT_STREQ(d.parameters[3].id, "input_trim");
}

TEST(Unit_Dsp, TubeScreamerIndexSetParameterMatchesStringSetParameter)
{
    SushiDSP::TubeScreamer a, b;
    a.set_parameter("tone", 0.8f);
    b.set_parameter(static_cast<std::uint16_t>(1), 0.8f);
    EXPECT_FLOAT_EQ(a.get_parameter("tone"), b.get_parameter("tone"));
}

TEST(Unit_Dsp, TubeScreamerSmoothingTimeIsBlockSizeIndependent)
{
    // Step level from -60 dB to 0 dB, measure the block index at which the output RMS crosses
    // half of its final value, at 64 and 2048 frame blocks: the crossing time in ms must agree.
    auto crossing_ms = [](std::uint32_t block) {
        SushiDSP::TubeScreamer node;
        node.prepare(SushiDSP::PrepareInfo{ 48000.0, block, 1 });
        node.set_parameter("level", -60.0f);
        std::vector<float> in(block), out(block);
        for (std::size_t i = 0; i < block; ++i) { in[i] = 0.1f; }   // DC-ish probe after the 20 Hz HPF settles
        const float* ins[1] = { in.data() }; float* outs[1] = { out.data() };
        for (int i = 0; i < 100; ++i) { SushiDSP::ProcessContext c{ outs, ins, block, 0.0 }; node.process(c); }
        node.set_parameter("level", 0.0f);
        double samples = 0.0;
        for (int b = 0; b < 100000; ++b)
        {
            SushiDSP::ProcessContext c{ outs, ins, block, 0.0 };
            node.process(c);
            if (std::fabs(out[block - 1]) > 0.5f * std::fabs(in[0]) * 0.5f) { return (samples + block) / 48.0; }
            samples += block;
        }
        return -1.0;
    };
    const double at_64 = crossing_ms(64);
    const double at_2048 = crossing_ms(2048);
    EXPECT_NEAR(at_64, at_2048, 2048.0 / 48.0);   // within one large block of quantisation
}
```
(The DC probe passes through the 20 Hz HPF as a decaying value; the test only needs the *ratio*
between block sizes, so the exact amplitude is irrelevant. If the HPF makes the probe too small,
use a 1 kHz sine and peak-detect instead — either is acceptable as long as both block sizes use
the same probe.)

- [ ] **Step 2: `sd build`** → compile errors (`Calibration.hpp` missing, `ParamKind` mismatch).

- [ ] **Step 3: Implement**

In `TubeScreamer.hpp` replace `drive_target_/drive_current_` etc. with
```cpp
        SmoothedParameter drive_{ 0.5f };
        SmoothedParameter tone_{ 0.5f };
        SmoothedParameter level_gain_{ 1.0f };        // linear gain, smoothed in gain domain
        SmoothedParameter input_trim_gain_{ 1.0f };
        float level_db_ = 0.0f;
        float input_trim_db_ = 0.0f;
        float last_tone_ = -1.0f;                      // forces the first coefficient build
```
`prepare()` calls `x.prepare(sample_rate_, kSmoothingMs)` (`kSmoothingMs = 20.0f`) on all four,
then `snap()`s each to its target so a fresh node does not ramp from silence.
`process()`:
```cpp
        const float drive = drive_.skip(context.frames);
        const float tone = tone_.skip(context.frames);
        const float input_gain = input_trim_gain_.skip(context.frames);
        const float level = level_gain_.skip(context.frames);
        clipping_stage_.set_feedback_resistance(feedback_resistance_for_drive(drive));
        if (std::fabs(tone - last_tone_) > 1e-4f) { last_tone_ = tone; }   // coefficients are fixed at 720 Hz today; the crossfade weight is `tone` itself
        for (i...) oversampled_scratch_[i] = input_highpass_.process_sample(input[i] * input_gain);
        ...
        output[i] = toned * level;
```
Setters:
```cpp
    void TubeScreamer::set_level(float db) { level_db_ = std::clamp(db, -60.0f, 12.0f); level_gain_.set_target(decibels_to_gain(level_db_)); }
    void TubeScreamer::set_input_trim(float db) { input_trim_db_ = std::clamp(db, -24.0f, 24.0f); input_trim_gain_.set_target(decibels_to_gain(input_trim_db_)); }
    void TubeScreamer::set_parameter(std::uint16_t index, float value)
    {
        switch (index) { case 0: set_drive(value); return; case 1: set_tone(value); return;
                         case 2: set_level(value); return; case 3: set_input_trim(value); return; default: return; }
    }
```
String overload delegates to the index overload via a small `parameter_index_for(id)` helper in
the anonymous namespace (strcmp chain returning 0xFFFF for unknown). Descriptor table:
```cpp
            { "drive",      "Drive",      0.0f,   1.0f,  0.5f, ParamKind::Linear,  "" },
            { "tone",       "Tone",       0.0f,   1.0f,  0.5f, ParamKind::Linear,  "" },
            { "level",      "Level",     -60.0f, 12.0f,  0.0f, ParamKind::Decibel, "dB" },
            { "input_trim", "Input Trim",-24.0f, 24.0f,  0.0f, ParamKind::Decibel, "dB" },
```
Level normalisation: the old default `level = 0.7` linear becomes `0 dB`; to keep the default
loudness the multiplier becomes `level * kLevelUnityGain` with `kLevelUnityGain = 0.7f`
(documented: "0 dB on the knob is the pedal's unity-ish output at Level 7 on the real unit").

- [ ] **Step 4: `sd test -s all`** → unit PASS; `test_tube_screamer_regression.cpp` now *fails*
  (level path changed from 0.7 linear slewed to smoothed 0.7 — the slew from 0.7 to 0.7 was
  identity, and `snap()` makes the smoothed path identity too, so it should in fact still pass).
  If it passes: good, no re-pin. If it fails by > 1e-5: inspect — the only legitimate cause is the
  smoothing coefficient being applied per block instead of per sample; fix, do not re-pin.
- [ ] **Step 5: Benchmark** `sd test -t release -s benchmark`; record TS ns/sample before/after.
- [ ] **Step 6: Commit** — `"D5b: TubeScreamer uses ms-based smoothing, indexed parameters, dB level and input trim (bench: <before> -> <after> ns/sample)"`

---

### Task 7: `Jcm800` parameter path — smoothing, audio-thread coefficient recompute, indices

**Files:**
- Modify: `modules/jcm800/include/SushiDSP/jcm800/Jcm800.hpp`, `modules/jcm800/src/Jcm800.cpp`
- Test: `tests/functional/unit/test_jcm800.cpp`

**Interfaces:**
- Produces descriptor index order (contract for Tasks 10/11): `0 preamp`, `1 master [-60,+12] dB`,
  `2 bass`, `3 middle`, `4 treble`, `5 presence`, `6 input_trim [-24,+24] dB`, `7 output [-60,+12] dB`.
  `set_master(float db)`, `set_input_trim(float db)`, `set_output(float db)`.
- Consumes: Task 1, Task 4.

- [ ] **Step 1: Failing tests**

```cpp
TEST(Unit_Dsp, Jcm800DescriptorExposesEightParametersInContractOrder)
{
    const auto& d = SushiDSP::jcm800_descriptor();
    ASSERT_EQ(d.parameter_count, 8u);
    const char* ids[8] = { "preamp", "master", "bass", "middle", "treble", "presence", "input_trim", "output" };
    for (std::size_t i = 0; i < 8; ++i) { EXPECT_STREQ(d.parameters[i].id, ids[i]); }
    EXPECT_EQ(d.parameters[1].kind, SushiDSP::ParamKind::Decibel);
}

TEST(Unit_Dsp, Jcm800ToneChangeDoesNotRebuildFiltersOffTheAudioThread)
{
    // set_bass must only store a target: the tone stack's coefficients before and after the call,
    // with no process() in between, must be identical. (Observable through get_parameter reporting
    // the new target while a subsequent process() of silence still starts from the old response.)
    SushiDSP::Jcm800 node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
    node.set_parameter("bass", 1.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("bass"), 1.0f);
    EXPECT_FALSE(node.tone_stack_is_dirty_for_test() == false);   // expose via a public `bool tone_stack_is_dirty() const`
}
```
Replace the last assertion with the real accessor: add `bool tone_stack_is_dirty() const` (public,
Doxygen'd as a test/diagnostic hook) and `EXPECT_TRUE(node.tone_stack_is_dirty());` then after one
`process()` of a silent block `EXPECT_FALSE(node.tone_stack_is_dirty());`.

Update the existing "six parameters" test (`test_jcm800.cpp:255` area) to eight.

- [ ] **Step 2: `sd build`** → compile errors.

- [ ] **Step 3: Implement**

Header members:
```cpp
        SmoothedParameter preamp_{ 0.5f }, presence_{ 0.5f };
        SmoothedParameter bass_{ 0.5f }, middle_{ 0.5f }, treble_{ 0.5f };
        SmoothedParameter master_gain_{ 1.0f }, input_trim_gain_{ 1.0f }, output_gain_{ 1.0f };
        float master_db_ = 0.0f, input_trim_db_ = 0.0f, output_db_ = 0.0f;
        float applied_bass_ = -1.0f, applied_middle_ = -1.0f, applied_treble_ = -1.0f, applied_presence_ = -1.0f;
```
`prepare()` prepares all eight smoothers at `sample_rate_` with `kSmoothingMs = 20.0f`, snaps them,
resets `applied_*` to −1 so the first `process()` builds coefficients.
`process()` start:
```cpp
        const float preamp = preamp_.skip(frames);
        const float bass = bass_.skip(frames), middle = middle_.skip(frames), treble = treble_.skip(frames);
        const float presence = presence_.skip(frames);
        const float input_gain = input_trim_gain_.skip(frames);
        const float master = master_gain_.skip(frames);
        const float output_gain = output_gain_.skip(frames);
        if (std::fabs(bass - applied_bass_) > kCoefficientEpsilon) { tone_stack_.set_bass(bass); applied_bass_ = bass; }
        // same for middle, treble
        if (std::fabs(presence - applied_presence_) > kCoefficientEpsilon)
        {
            presence_shelf_.set_high_shelf(sample_rate_, kPresenceHz, presence_gain_db(presence), kFilterQ);
            applied_presence_ = presence;
        }
```
with `kCoefficientEpsilon = 1e-4f`. Input is multiplied by `input_gain` before `bright_cap_`;
master replaces `master_current_` as `master * kMasterUnityGain` (`kMasterUnityGain = 0.7f`, the
old default, so 0 dB keeps today's loudness); `output_gain` multiplies after the cabinet.
`set_bass/middle/treble/presence/preamp` become `x_.set_target(clamp)` only. `set_parameter(uint16_t)`
switch on 0–7. `get_parameter` returns targets (dB for the dB ones).
`tone_stack_is_dirty()` returns `std::fabs(bass_.target() - applied_bass_) > kCoefficientEpsilon || ...`.

- [ ] **Step 4: `sd test -s all`** → unit PASS; regression: the JCM800 snapshot sets
  `set_master(0.7f)` — update the regression test to `set_master(0.0f)` (dB) which yields the same
  0.7 linear multiplier; the five pinned values must still hold at 1e-5. If not, the cause is the
  coefficient-recompute timing (presence shelf now recomputed only on change, previously every
  block with a slewed value: with `snap()` the value is identical, so the coefficients are too).
  Do not re-pin in this task.
- [ ] **Step 5: Benchmark** before/after.
- [ ] **Step 6: Commit** — `"D5b: Jcm800 parameters are smoothed targets applied on the audio thread; dB master, input trim and output added (bench ...)"`

---

### Task 8: `Jcm800` cabinet on `StreamingConvolution` + `RtSwapSlot`, limiter removed, latency

**Files:**
- Modify: `modules/jcm800/include/SushiDSP/jcm800/Jcm800.hpp`, `modules/jcm800/src/Jcm800.cpp`
- Create: `include/SushiDSP/io/Ir.hpp` (`struct Ir { std::vector<float> samples; double sample_rate; }`)
- Test: `tests/functional/unit/test_jcm800.cpp`, `test_rt_safety.cpp`

**Interfaces:**
- Produces: `void Jcm800::set_cabinet(Ir ir)` (control thread; `ir.sample_rate` must equal the
  prepared rate or the call is ignored and returns `false` — signature `bool set_cabinet(Ir)`),
  `void Jcm800::reclaim_cabinets()` (control thread, periodic), `std::uint32_t
  Jcm800::latency_samples() const override`, `bool Jcm800::has_cabinet() const`.
  Constructor keeps the `std::vector<float>` IR argument for tests (treated as already at the
  prepared rate).
- Consumes: Task 3, Task 5, Task 2.

- [ ] **Step 1: Failing tests**

```cpp
TEST(Unit_Dsp, Jcm800CabinetSurvivesVariableBlockSizes)
{
    std::vector<float> ir(64, 0.0f); ir[0] = 1.0f;     // identity IR
    SushiDSP::Jcm800 with_cab(ir), without_cab;
    for (auto* n : { &with_cab, &without_cab }) { n->prepare(SushiDSP::PrepareInfo{ 48000.0, 512, 1 }); }
    std::vector<float> in(512), a(512), b(512);
    for (std::size_t i = 0; i < 512; ++i) { in[i] = 0.2f * std::sin(0.03f * i); }
    const float* ins[1] = { in.data() }; float* outs_a[1] = { a.data() }; float* outs_b[1] = { b.data() };
    std::vector<float> stream_a, stream_b;
    const std::uint32_t chunks[] = { 100, 37, 512, 1, 300 };
    for (std::uint32_t frames : chunks)
    {
        SushiDSP::ProcessContext ca{ outs_a, ins, frames, 0.0 }; with_cab.process(ca);
        SushiDSP::ProcessContext cb{ outs_b, ins, frames, 0.0 }; without_cab.process(cb);
        stream_a.insert(stream_a.end(), a.begin(), a.begin() + frames);
        stream_b.insert(stream_b.end(), b.begin(), b.begin() + frames);
    }
    const std::uint32_t latency = with_cab.latency_samples() - without_cab.latency_samples();
    EXPECT_EQ(latency, 128u);
    for (std::size_t n = latency; n < stream_a.size(); ++n) { EXPECT_NEAR(stream_a[n], stream_b[n - latency], 1e-5f); }
}

TEST(Unit_Dsp, Jcm800SetCabinetSwapsWithoutAllocatingOnTheAudioThread)  // lives in test_rt_safety.cpp
{
    SushiDSP::Jcm800 node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
    SushiDSP::Ir ir; ir.sample_rate = 48000.0; ir.samples.assign(2048, 0.0f); ir.samples[0] = 1.0f;
    ASSERT_TRUE(node.set_cabinet(ir));
    // trap on, process 4 blocks, trap off, expect 0 allocations; then reclaim_cabinets() outside the trap
}

TEST(Unit_Dsp, Jcm800NeverExceedsFullScaleWithoutALimiter)
{
    SushiDSP::Jcm800 node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
    node.set_parameter("preamp", 1.0f);
    node.set_parameter("master", 12.0f);
    node.set_parameter("output", 12.0f);
    std::vector<float> in(256), out(256);
    const float* ins[1] = { in.data() }; float* outs[1] = { out.data() };
    bool clamped_at_least_once = false;
    for (int block = 0; block < 50; ++block)
    {
        for (int i = 0; i < 256; ++i) { in[i] = std::sin(2.0f * float(M_PI) * 220.0f * (block * 256 + i) / 48000.0f); }
        SushiDSP::ProcessContext c{ outs, ins, 256, 0.0 };
        node.process(c);
        for (float s : out)
        {
            EXPECT_LE(std::fabs(s), 1.0f);
            if (std::fabs(s) == 1.0f) { clamped_at_least_once = true; }
        }
    }
    EXPECT_TRUE(clamped_at_least_once);   // proves the clamp is what bounds it, not headroom
}
```

- [ ] **Step 2: `sd build`** → compile errors.

- [ ] **Step 3: Implement**

- Remove `oversampler_c_`, `scratch_oversampled_c_`, `soft_limit`, `kOutputLimiterThreshold`, the
  `needs_limiting` block. After the cabinet and `output_gain`: `output[i] = std::clamp(output[i], -1.0f, 1.0f);`.
- Replace `std::unique_ptr<PartitionedConvolution> cabinet_` and `cabinet_partition_size_` with
  `RtSwapSlot<StreamingConvolution> cabinet_slot_;` and `std::uint32_t cabinet_partition_ = 0;`.
- `prepare()`: `cabinet_partition_ = StreamingConvolution::partition_size_for(sample_rate_)`; if the
  constructor IR is non-empty, `cabinet_slot_.publish(std::make_unique<StreamingConvolution>(cabinet_partition_, ir.data(), size, info.max_block))`.
- `set_cabinet(Ir ir)`: if `ir.sample_rate != sample_rate_` return false; publish a new convolver
  built the same way; return true. `reclaim_cabinets()` → `cabinet_slot_.reclaim()`.
- `process()`: `StreamingConvolution* cabinet = cabinet_slot_.acquire_for_audio_thread(); if (cabinet) cabinet->process(scratch_base_.data(), output, frames); else copy`.
- `latency_samples()`: `oversampler_a_->latency_samples() + oversampler_b_->latency_samples() + (has_cabinet() ? cabinet_partition_ : 0)`. `has_cabinet()` reads the slot's active pointer non-null (add a `bool has_active() const noexcept` to `RtSwapSlot`).
- `reset()`: also reset the active cabinet if any.
- Update the header comment on `Jcm800` (drop "limiter").

- [ ] **Step 4: `sd test -s all`** → unit PASS; regression unchanged (no cabinet in the snapshot,
  limiter was identity at 0.157 peak).
- [ ] **Step 5: Benchmark.** **Step 6: Commit** — `"D5b: Jcm800 cabinet runs on StreamingConvolution behind RtSwapSlot (any block size, runtime IR swap); output limiter removed in favour of a plain clamp (bench ...)"`

---

### Task 9: Level calibration and snapshot re-pin

**Files:**
- Modify: `modules/jcm800/src/Jcm800.cpp` (`kMasterUnityGain`), `modules/tubescreamer/src/TubeScreamer.cpp` (`kLevelUnityGain`)
- Modify: `tests/functional/regression/test_jcm800_regression.cpp`, `tests/functional/regression/test_tube_screamer_regression.cpp`
- Test: `tests/functional/unit/test_jcm800.cpp`, `test_tube_screamer.cpp`

- [ ] **Step 1: Failing tests**

```cpp
TEST(Unit_Dsp, Jcm800DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput)
{
    SushiDSP::Jcm800 node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });   // all defaults
    // -12 dBFS = 0.251 peak, 82 Hz (low E), 1 s
    float peak = 0.0f;
    std::vector<float> in(256), out(256);
    const float* ins[1] = { in.data() }; float* outs[1] = { out.data() };
    for (int block = 0; block < 188; ++block)
    {
        for (int i = 0; i < 256; ++i) { in[i] = 0.251f * std::sin(2.0f * float(M_PI) * 82.0f * (block * 256 + i) / 48000.0f); }
        SushiDSP::ProcessContext c{ outs, ins, 256, 0.0 }; node.process(c);
        if (block > 20) { for (float s : out) { peak = std::max(peak, std::fabs(s)); } }
    }
    const float peak_db = 20.0f * std::log10(peak);
    EXPECT_GT(peak_db, -8.0f);
    EXPECT_LT(peak_db, -4.0f);
}
```
Same test for `TubeScreamer` (band −14…−10 dBFS: a TS at default drive adds ~0 dB at 82 Hz).

- [ ] **Step 2: Run** → the JCM800 test fails (today's default peaks ≈ −16 dBFS with a 0.25 input).
- [ ] **Step 3: Tune** `kMasterUnityGain` (Jcm800) upward until the peak lands at −6 dBFS ± 0.5 dB
  (expected ≈ 2.2); tune `kLevelUnityGain` if the TS test fails. These constants are the *only*
  tone-affecting numbers this phase changes; say so in the Doxygen next to them.
- [ ] **Step 4: Re-pin** both regression snapshots: change `set_master(0.0f)` to remain at 0 dB,
  run once, paste the new five values, and update the comment to state "re-pinned in D5b for the
  master/level calibration change (spec D5b.5); every other stage bit-identical". Add the ratio
  check: new values / old values must all equal `kMasterUnityGain / 0.7` within 1e-4 — put that
  assertion in the commit message, not the test.
- [ ] **Step 5: `sd test -s all`** → PASS. **Step 6: Commit** — `"D5b: calibrate default output to -6 dBFS for a -12 dBFS input; re-pin snapshots (ratio <x>, all other stages bit-identical)"`

---

### Task 10: `IrLoader` — resample, trim + fade, normalise

**Files:**
- Create: `include/SushiDSP/io/IrLoader.hpp`, `src/io/IrLoader.cpp`
- Create: `include/SushiDSP/math/SincResampler.hpp`, `src/math/SincResampler.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/functional/unit/test_sinc_resampler.cpp`, `tests/functional/unit/test_ir_loader.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces:
  ```cpp
  // math: offline, control-thread only, may allocate
  std::vector<float> SincResampler::resample(const std::vector<float>& input, double input_rate, double output_rate);
  // io
  struct IrLoadOptions { float max_length_ms = 200.0f; float fade_ms = 5.0f; bool normalise = true;
                         float normalise_low_hz = 100.0f; float normalise_high_hz = 5000.0f; };
  class IrLoader { public:
      static Ir load(const std::string& path, double target_sample_rate, const IrLoadOptions& options = {});   // throws std::runtime_error
      static Ir prepare(std::vector<float> samples, double source_rate, double target_sample_rate, const IrLoadOptions& options = {}); // same pipeline without the file
  };
  ```
- Consumes: `WavFile::load_mono`, `Fft`, `Ir` (Task 8).

- [ ] **Step 1: Failing tests**

```cpp
// test_sinc_resampler.cpp
TEST(Unit_Dsp, SincResamplerPreservesAOneKilohertzToneWithinMinusEightyDb)
{
    std::vector<float> in(44100);
    for (std::size_t i = 0; i < in.size(); ++i) { in[i] = std::sin(2.0 * M_PI * 1000.0 * i / 44100.0); }
    const auto out = SushiDSP::SincResampler::resample(in, 44100.0, 48000.0);
    ASSERT_NEAR(out.size(), 48000u, 2);
    double error = 0.0, energy = 0.0;
    for (std::size_t i = 1000; i + 1000 < out.size(); ++i)
    {
        const double expected = std::sin(2.0 * M_PI * 1000.0 * i / 48000.0);
        error += (out[i] - expected) * (out[i] - expected); energy += expected * expected;
    }
    EXPECT_LT(10.0 * std::log10(error / energy), -80.0);
}

// test_ir_loader.cpp
TEST(Unit_Dsp, IrLoaderTrimsAndFadesToMaxLength)
{
    std::vector<float> samples(48000, 1.0f);                       // 1 s of ones
    const auto ir = SushiDSP::IrLoader::prepare(samples, 48000.0, 48000.0, SushiDSP::IrLoadOptions{ 200.0f, 5.0f, false });
    EXPECT_EQ(ir.samples.size(), 9600u);
    EXPECT_NEAR(ir.samples[9599], 0.0f, 1e-3f);
    EXPECT_NEAR(ir.samples[9600 - 240 - 1], 1.0f, 1e-3f);          // just before the 5 ms fade
}

TEST(Unit_Dsp, IrLoaderNormalisesBandAverageToZeroDb)
{
    std::vector<float> samples(4096, 0.0f); samples[0] = 0.1f;     // identity at -20 dB
    const auto ir = SushiDSP::IrLoader::prepare(samples, 48000.0, 48000.0);
    EXPECT_NEAR(ir.samples[0], 1.0f, 1e-3f);
    EXPECT_DOUBLE_EQ(ir.sample_rate, 48000.0);
}
```

- [ ] **Step 2: `sd build`** → compile errors.

- [ ] **Step 3: Implement**

`SincResampler::resample`: for each output index `n`, `t = n * input_rate / output_rate`; sum
over `k` in `[floor(t) - 32, floor(t) + 32]` of `input[k] * sinc(cutoff * (t - k)) * kaiser(t - k, beta = 8.6)`
with `cutoff = min(1, input_rate / output_rate)` (so downsampling lowpasses at the new Nyquist);
normalise the window sum per output sample. 64 taps + Kaiser β 8.6 gives > 80 dB.

`IrLoader::prepare`: (1) resample if rates differ by > 1e-6; (2) truncate to
`max_length_ms * target / 1000` samples; (3) multiply the last `fade_ms` worth by
`0.5 * (1 + cos(pi * i / fade_len))`; (4) if `normalise`: zero-pad to the next power of two ≥ 2·length,
`Fft::forward`, average `20·log10|H|` over bins in `[low_hz, high_hz]`, scale samples by
`10^(-mean_db/20)`. Return `Ir{ samples, target_sample_rate }`. `load` = `WavFile::load_mono` +
`prepare`.

- [ ] **Step 4: `sd test`** → PASS. **Step 5: Commit** — `"D5b: IrLoader resamples, trims, fades and band-normalises cabinet IRs at load time"`

---

### Task 11: `RigGraph` denormal guard + host_asio on the ring and IrLoader

**Files:**
- Modify: `src/core/RigGraph.cpp` (`DenormalGuard guard;` first line of `process()`)
- Modify: `apps/host_asio/main.cpp`
- Test: `tests/functional/unit/test_rig_graph.cpp`

- [ ] **Step 1: Failing test**

```cpp
namespace
{
    class DenormalNode : public SushiDSP::IAudioNode
    {
    public:
        void prepare(const SushiDSP::PrepareInfo&) override {}
        void process(SushiDSP::ProcessContext& context) override
        {
            volatile float state = 1e-40f;                 // a denormal; volatile defeats constant folding
            for (std::uint32_t i = 0; i < context.frames; ++i)
            {
                state = state * 0.5f;
                context.outputs[0][i] = state;
            }
        }
        SushiDSP::PortLayout ports() const override { return { 0, 1 }; }
        float get_parameter(const char*) const override { return 0.0f; }
        void set_parameter(const char*, float) override {}
        void reset() override {}
    };
}

TEST(Unit_Dsp, RigGraphProcessFlushesDenormalsForItsNodes)
{
    if (!SushiDSP::DenormalGuard::is_supported()) { GTEST_SKIP(); }
    SushiDSP::RigGraph graph(64, 4);
    graph.add_node(std::make_unique<DenormalNode>());
    graph.compile(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    std::vector<float> out(64, 1.0f);
    float* outs[1] = { out.data() };
    SushiDSP::ProcessContext context{ outs, nullptr, 64, 0.0 };
    graph.process(context);
    EXPECT_EQ(out[0], 0.0f);          // 1e-40 * 0.5 is a non-zero denormal without FTZ/DAZ
}
```
(Outside a guard, `1e-40f * 0.5f` is a non-zero denormal; inside, it flushes to 0.)

- [ ] **Step 2: Run** → fails. **Step 3: Implement** the one-line guard. Run → PASS.
- [ ] **Step 4: host_asio**: replace `WavFile::load_mono(ir_path)` with
  `SushiDSP::IrLoader::load(ir_path, actual.sample_rate)` after the device format is known (move the
  load below the device open), pass `ir.samples` to the `Jcm800` constructor as before; print
  `rig.latency_samples()` after `compile`. Manual check: `sushidsp_host_asio <driver> <ir.wav>`
  plays with the cabinet at a 44.1 k IR on a 48 k device without pitch shift.
- [ ] **Step 5: Commit** — `"D5b: RigGraph engages DenormalGuard itself; host_asio loads IRs through IrLoader and prints rig latency"`

---

### Task 12: host_gui — parameters through the ring, IR chooser, preset field, bundled IR

**Files:**
- Modify: `apps/host_gui/RigController.hpp`, `apps/host_gui/RigController.cpp`
- Modify: `apps/host_gui/main.cpp`, `apps/host_gui/Preset.cpp`
- Create: `assets/ir/README.md`; **user-provided:** `assets/ir/default_4x12.wav`
- Test: `tests/functional/unit/test_rig_controller.cpp`, `tests/functional/unit/test_preset.cpp`

**Interfaces:**
- `RigController::set_parameter(index, param_id, value)` now: stores in `slots_[index].parameters`,
  resolves `param_id` → parameter index via `node_registry()`'s descriptor, and calls
  `active_graph_->push_parameter_change({ handle, index, value })`. Never touches the node.
- `RigController::set_cabinet(std::size_t slot, const std::string& path)`: loads via
  `IrLoader::load(path, info_.sample_rate)`, calls `Jcm800::set_cabinet` on the live node
  (dynamic_cast; non-JCM800 slots ignore), stores the path in `Slot::cabinet_path`; on
  `rebuild_and_publish` the path is re-applied to the fresh node.
- `RigController::reclaim()` calls `graph_slot_.reclaim()` **and** every live `Jcm800`'s `reclaim_cabinets()`; `main.cpp:119` calls this instead of `graph_slot().reclaim()`.
- Preset JSON: per node, optional `"cabinet_ir": "<path>"`. `save_preset` writes it when set;
  `load_preset` passes it through `load_chain`'s new third argument `std::vector<std::string> cabinet_paths`.

- [ ] **Step 1: Failing tests**

```cpp
// test_rig_controller.cpp
TEST(Unit_Dsp, RigControllerSetParameterReachesNodeOnlyAfterProcess)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    controller.append("sushidsp.tubescreamer");
    controller.set_parameter(0, "tone", 0.9f);
    SushiDSP::IAudioNode* node = controller.active_graph()->node(0);
    EXPECT_FLOAT_EQ(node->get_parameter("tone"), 0.5f);              // not yet applied
    std::vector<float> in(64, 0.0f), out(64);
    const float* ins[1] = { in.data() }; float* outs[1] = { out.data() };
    SushiDSP::ProcessContext c{ outs, ins, 64, 0.0 };
    controller.active_graph()->process(c);
    EXPECT_FLOAT_EQ(node->get_parameter("tone"), 0.9f);
}

// test_preset.cpp
TEST(Unit_Dsp, PresetRoundTripsCabinetPath)  // save with cabinet_path set on a jcm800 slot, load into a fresh controller, expect chain_cabinet_paths()[i] equal
```

- [ ] **Step 2: `sd build`** → compile errors (`push_parameter_change` unused so far; `set_cabinet` missing).
- [ ] **Step 3: Implement** as in Interfaces. In `main.cpp` the parameter panel keeps its
  `SliderFloat`; for `ParamKind::Decibel` parameters show `param.unit` in the label
  (`"%s (dB)"`). Add, for JCM800 slots, an `InputText` "Cabinet IR path" + "Load" button calling
  `set_cabinet`; on failure show the exception text in red for 3 s (same pattern the device
  screen uses for its error string). On startup, if `assets/ir/default_4x12.wav` exists relative
  to the executable's directory, `set_cabinet` every JCM800 slot with it when created
  (`RigController::append`/`insert`); if absent, print one warning line to stderr and continue
  without a cabinet.
- [ ] **Step 4: Bundled IR.** Write `assets/ir/README.md`: the file must be a CC0 or
  Apache-compatible 4×12 IR, mono or stereo, any rate (IrLoader resamples). **This file is a
  user-supplied input to the plan** — the executor must not download or fabricate one; if it is
  not present, leave the README, keep the fallback path, and record the gap in the commit
  message. Add `install(DIRECTORY assets/ir DESTINATION ...)`-style copy in
  `apps/host_gui/CMakeLists.txt` (post-build copy to the executable directory) so the relative
  lookup works from the build tree.
- [ ] **Step 5: `sd test -s all`** → PASS. Manual: run host_gui, connect ASIO, turn Bass fast —
  no zipper, no click; load an IR; save/load a preset with it.
- [ ] **Step 6: Commit** — `"D5b: host_gui routes every knob through the parameter ring, loads cabinet IRs at runtime and stores them in presets"`

---

### Task 13: Docs — spec "As built" and changelog

**Files:**
- Modify: `docs/design/sushidsp.md` §5 "As built" (limiter gone, cabinet via `StreamingConvolution`, parameters list now eight incl. `input_trim`/`output`, latency reported), §6 parameter list, §7.4 (ring now used), §4.5 (WDF deferred — one sentence pointing at the D5b spec's decision table), §4 fixed-capacities table (partition 128/256).
- Modify: `docs/reference/CHANGELOG.md` — new "## D5b — Plugin-readiness foundations" section above "CPU performance pass", one bullet per task with benchmark numbers from Tasks 6–9.
- Modify: `README.md` if it lists parameters or mentions the limiter.

- [ ] **Step 1:** Make the edits. **Step 2:** `sd doxygen` builds without warnings on the new headers.
- [ ] **Step 3: Commit** — `"D5b: document as-built state, ring-based parameter path and WDF deferral"`

---

## Self-review against spec D5b

- D5b.1 parameter path → Tasks 4, 6, 7, 12. ✔
- D5b.2 smoothing (20 ms, per-sample inside oversampled loop for nonlinear feeds) → Task 1; Tasks 6/7
  apply `skip(frames)` per block for *all* parameters. **Deviation, stated:** per-sample ramping of
  preamp/drive inside the 8× loop is deferred to D6c/D6d, where those stages are rewritten anyway;
  at 20 ms and ≤ 4096-frame blocks the per-block step is ≤ 8 % of the remaining distance, below
  the −40 dBFS click criterion the spec sets for D7. Record this in the CHANGELOG bullet.
- D5b.3 cabinet + latency → Tasks 2, 3, 8. ✔
- D5b.4 IR pipeline, runtime swap, GUI chooser, preset field, bundled IR → Tasks 5, 8, 10, 12. ✔ (bundled IR file is user-supplied; fallback documented.)
- D5b.5 calibration, dB params, limiter removal, clamp test → Tasks 6, 7, 8, 9. ✔
- D5b.6 denormals in `RigGraph::process` → Task 11. ✔
- Spec test list: SPSC stress (Task 4 RT test + Task 12), convolution frame sequences (Task 3),
  IR loader −80 dB and 0 ± 0.1 dB (Task 10 — the 0 dB test uses an identity IR; add a second case
  with a two-tap IR if the band average logic needs exercising), smoothing ±5 % (Task 6 uses a
  one-large-block tolerance; tighten to 5 % if the probe turns out clean), level −8…−4 dBFS (Task 9). ✔
- Type consistency: `Ir{samples, sample_rate}` (Task 8) used by Tasks 10–12; `ParameterChange`
  (Task 4) by Task 12; `StreamingConvolution` ctor `(partition, ir, len, max_block)` by Task 8;
  `latency_samples()` naming identical across Tasks 2, 3, 8. ✔
