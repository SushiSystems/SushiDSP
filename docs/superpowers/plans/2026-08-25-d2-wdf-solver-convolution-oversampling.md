# SushiDSP D2 — WDF/DK Solver Core + Convolution + Oversampling Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the shared nonlinear-circuit math both JCM800 and Tube Screamer will build on — partitioned FFT convolution, an 8x oversampling cascade, Wave Digital Filter one-port primitives, and a generic Newton–Raphson solver — culminating in a minimal one-stage nonlinear WDF circuit that proves the whole pipeline works end to end. This is the D2 phase from `docs/design/sushidsp.md` §9.

**Architecture:** Four independent math-kernel additions to `include/SushiDSP/math/`, each with its own `.cpp` and golden-vector/plausibility tests, following the exact pattern established in D0 (`Fft`, `Biquad`). `PartitionedConvolution` reuses D0's `Fft` class. The WDF work is deliberately scoped down from the spec's full series/parallel adaptor tree: only `WdfResistor` and `WdfCapacitor` (standalone one-port primitives, algebraically validated) plus a generic `NewtonRaphsonSolver` and a minimal one-stage `DiodeClipperCircuit` land in D2 — full adaptor trees (needed to combine multiple elements into an arbitrary circuit topology) are deferred to D3/D4, where JCM800's and Tube Screamer's real topologies will drive the right abstraction rather than an invented synthetic one. This satisfies D2's roadmap "Ships" line ("a minimal one-stage WDF test circuit processes correctly") without building infrastructure nothing yet exercises.

**Tech Stack:** C++17, CMake, GoogleTest — no new external dependencies.

**Spec:** `docs/design/sushidsp.md` (§4.2 partitioned convolution, §4.4 resamplers, §4.5 WDF/DK circuit-solving core, §9 D2, §0a reference header)

## Global Constraints

- C++17 only — no C++20/23 features.
- Allman braces (opening brace on its own line); trivial one-line accessors may stay compact.
- Naming: types `PascalCase`, functions/variables `snake_case`, class members trailing underscore (`impl_`), constants `UPPER_SNAKE`, no abbreviations.
- Apache 2.0 license header: the exact 76-column boxed template pinned in `docs/design/sushidsp.md` §0a — copy an existing file (e.g. `include/SushiDSP/math/Fft.hpp`) verbatim as the literal starting point; only the filename line changes.
- Doxygen `@brief`/`@param`/`@return` on every public function.
- No comments unless the *why* is non-obvious.
- `sushi_dsp` core target: zero external dependencies beyond the C++17 standard library and SIMD intrinsics.
- All new math kernels used from a real-time context must be allocation-free after construction, per D0's precedent (`Fft::forward`/`inverse` are; `PartitionedConvolution::process`, `Oversampler8x::upsample`/`downsample`, and the WDF/Newton-Raphson types in this plan must be too — size all scratch storage in the constructor).
- Test files live under `tests/functional/unit/test_<snake_case_name>.cpp`, GTest suite name `Unit_Dsp`, added to `tests/CMakeLists.txt`'s `add_executable(sushidsp_tests ...)` source list.
- Block size default 256 frames, sample rate default 48000 Hz (established in D0). Amp/pedal oversample factor is 8x (spec §4, "Fixed capacities" table) — `Oversampler8x` is named for this.

---

## File Structure

```
sushidsp/
  include/SushiDSP/math/
    PartitionedConvolution.hpp
    Oversampler.hpp              # HalfbandUpsampler2x, HalfbandDownsampler2x, Oversampler8x
    Wdf.hpp                      # WdfPort, WdfResistor, WdfCapacitor
    NewtonRaphsonSolver.hpp
  include/SushiDSP/circuits/
    DiodeClipperCircuit.hpp      # the D2 "ships" proof circuit — not a product (no NodeDescriptor)
  src/math/
    PartitionedConvolution.cpp
    Oversampler.cpp
    Wdf.cpp
  src/circuits/
    DiodeClipperCircuit.cpp
  tests/functional/unit/
    test_partitioned_convolution.cpp
    test_oversampler.cpp
    test_wdf.cpp
    test_newton_raphson_solver.cpp
    test_diode_clipper_circuit.cpp
```

`include/SushiDSP/circuits/` is a new top-level namespace for circuit-topology building blocks that sit above the raw math library but below a product module (`modules/jcm800`, `modules/tubescreamer`, both still D3/D4). `DiodeClipperCircuit` is the first thing to live there; it is explicitly a proof-of-pipeline circuit, not a product — it has no `NodeDescriptor`.

---

## Task 1: `PartitionedConvolution` — uniform-partitioned overlap-add FFT convolution

**Files:**
- Create: `include/SushiDSP/math/PartitionedConvolution.hpp`
- Create: `src/math/PartitionedConvolution.cpp`
- Test: `tests/functional/unit/test_partitioned_convolution.cpp`
- Modify: `CMakeLists.txt` (add `src/math/PartitionedConvolution.cpp` to `sushi_dsp`'s source list)
- Modify: `tests/CMakeLists.txt` (add the test file to `add_executable(sushidsp_tests ...)`)

**Interfaces:**
- Consumes: `SushiDSP::Fft` (`forward`/`inverse`/`size()`, from D0 Task 5, `include/SushiDSP/math/Fft.hpp`).
- Produces: `SushiDSP::PartitionedConvolution` with `PartitionedConvolution(std::uint32_t partition_size, const float* impulse_response, std::uint32_t impulse_length)` and `void process(const float* input, float* output)` (both buffers exactly `partition_size` frames). No other task in this plan consumes this class — it stands alone as the cabinet-IR building block D3 (JCM800) will use.

**Algorithm (uniform-partitioned overlap-add convolution):** split the impulse response into `K = ceil(impulse_length / partition_size)` partitions of `N = partition_size` samples each (zero-padding the last), and FFT each zero-padded-to-`2N` partition once at construction. Per `process()` call (one block of `N` new input samples): zero-pad the input block to `2N` and FFT it into slot `write_index` of a length-`K` circular history of past input spectra; compute `Y = Σ_{k=0}^{K-1} X[(write_index - k) mod K] * H[k]` (complex multiply-accumulate); inverse-FFT `Y` to get a `2N`-sample time-domain block `y`; output = `y[0:N] + overlap` (the tail saved from the *previous* call), then save `y[N:2N]` as the new `overlap` for the *next* call. This is standard overlap-add partitioned convolution (see e.g. Gardner, "Efficient Convolution Without Input-Output Delay") — the key invariant a reviewer should check is that partition `k` combined with input block `i` contributes to *output* blocks `i+k` and `i+k+1`, which is exactly what indexing history by `(write_index - k)` and doing the two-block overlap-add achieves.

- [ ] **Step 1: Write the failing golden-vector test `tests/functional/unit/test_partitioned_convolution.cpp`**

```cpp
#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/math/PartitionedConvolution.hpp"

namespace
{
    std::vector<float> direct_convolution(const std::vector<float>& x, const std::vector<float>& h)
    {
        std::vector<float> y(x.size() + h.size() - 1, 0.0f);
        for (std::size_t n = 0; n < x.size(); ++n)
        {
            for (std::size_t k = 0; k < h.size(); ++k)
            {
                y[n + k] += x[n] * h[k];
            }
        }
        return y;
    }
}

TEST(Unit_Dsp, PartitionedConvolutionOfImpulseReproducesImpulseResponse)
{
    constexpr std::uint32_t partition_size = 64;
    std::vector<float> impulse_response(100);
    for (std::size_t i = 0; i < impulse_response.size(); ++i)
    {
        impulse_response[i] = std::sin(0.1f * static_cast<float>(i)) * 0.5f;
    }

    SushiDSP::PartitionedConvolution convolution(partition_size, impulse_response.data(),
        static_cast<std::uint32_t>(impulse_response.size()));

    std::vector<float> input(partition_size, 0.0f);
    input[0] = 1.0f;

    std::vector<float> collected;
    for (int block = 0; block < 4; ++block)
    {
        std::vector<float> output(partition_size, 0.0f);
        convolution.process(input.data(), output.data());
        collected.insert(collected.end(), output.begin(), output.end());
        std::fill(input.begin(), input.end(), 0.0f);
    }

    for (std::size_t i = 0; i < impulse_response.size(); ++i)
    {
        EXPECT_NEAR(collected[i], impulse_response[i], 1e-3f) << "sample " << i;
    }
    for (std::size_t i = impulse_response.size(); i < collected.size(); ++i)
    {
        EXPECT_NEAR(collected[i], 0.0f, 1e-3f) << "sample " << i;
    }
}

TEST(Unit_Dsp, PartitionedConvolutionMatchesDirectConvolutionForWhiteNoise)
{
    constexpr std::uint32_t partition_size = 64;
    constexpr std::uint32_t impulse_length = 150;  // spans 3 partitions, last one padded
    constexpr int num_blocks = 6;

    std::vector<float> impulse_response(impulse_length);
    unsigned seed = 12345u;
    auto next_random = [&seed]() -> float
    {
        seed = seed * 1664525u + 1013904223u;
        return (static_cast<float>(seed >> 8) / static_cast<float>(1u << 24)) * 2.0f - 1.0f;
    };
    for (float& h : impulse_response)
    {
        h = next_random() * 0.3f;
    }

    std::vector<float> input(partition_size * num_blocks);
    for (float& x : input)
    {
        x = next_random();
    }

    SushiDSP::PartitionedConvolution convolution(partition_size, impulse_response.data(), impulse_length);
    std::vector<float> streamed;
    for (int block = 0; block < num_blocks; ++block)
    {
        std::vector<float> output(partition_size, 0.0f);
        convolution.process(input.data() + block * partition_size, output.data());
        streamed.insert(streamed.end(), output.begin(), output.end());
    }

    std::vector<float> full_input(input.begin(), input.end());
    std::vector<float> reference = direct_convolution(full_input, impulse_response);

    for (std::size_t i = 0; i < streamed.size(); ++i)
    {
        EXPECT_NEAR(streamed[i], reference[i], 1e-2f) << "sample " << i;
    }
}
```

- [ ] **Step 2: Add the test file to CMake and run to verify it fails**

Edit `tests/CMakeLists.txt`'s `add_executable(sushidsp_tests ...)` source list to add `functional/unit/test_partitioned_convolution.cpp`.

Run: `sd test`
Expected: FAIL — `SushiDSP/math/PartitionedConvolution.hpp` does not exist.

- [ ] **Step 3: Write `include/SushiDSP/math/PartitionedConvolution.hpp`**

```cpp
#pragma once

#include <cstdint>
#include <vector>

#include "SushiDSP/math/Fft.hpp"

namespace SushiDSP
{
    /// @brief Uniform-partitioned overlap-add FFT convolution against a fixed impulse response.
    /// Allocation-free after construction.
    class PartitionedConvolution
    {
    public:
        /// @brief Precomputes the FFT of every impulse-response partition.
        /// @param partition_size Block size in frames; also the FFT half-size (fft_size = 2*partition_size).
        /// @param impulse_response The impulse response samples.
        /// @param impulse_length Number of samples in impulse_response.
        PartitionedConvolution(std::uint32_t partition_size, const float* impulse_response, std::uint32_t impulse_length);

        /// @brief Processes one block, convolving it against the impulse response.
        /// @param input partition_size input samples.
        /// @param output partition_size output samples (overlap-added).
        /// @note input and output must not alias each other.
        void process(const float* input, float* output);

    private:
        std::uint32_t partition_size_;
        std::uint32_t fft_size_;
        std::uint32_t partition_count_;
        Fft            fft_;

        std::vector<std::vector<float>> ir_real_;
        std::vector<std::vector<float>> ir_imag_;
        std::vector<std::vector<float>> history_real_;
        std::vector<std::vector<float>> history_imag_;
        std::uint32_t write_index_ = 0;

        std::vector<float> overlap_;
        std::vector<float> zero_padded_scratch_;
        std::vector<float> freq_real_accum_;
        std::vector<float> freq_imag_accum_;
        std::vector<float> time_domain_scratch_;
    };
}
```

- [ ] **Step 4: Write `src/math/PartitionedConvolution.cpp`**

```cpp
#include "SushiDSP/math/PartitionedConvolution.hpp"

#include <algorithm>

namespace SushiDSP
{
    PartitionedConvolution::PartitionedConvolution(
        std::uint32_t partition_size, const float* impulse_response, std::uint32_t impulse_length)
        : partition_size_(partition_size)
        , fft_size_(partition_size * 2)
        , partition_count_((impulse_length + partition_size - 1) / partition_size)
        , fft_(fft_size_)
        , ir_real_(partition_count_, std::vector<float>(fft_size_, 0.0f))
        , ir_imag_(partition_count_, std::vector<float>(fft_size_, 0.0f))
        , history_real_(partition_count_, std::vector<float>(fft_size_, 0.0f))
        , history_imag_(partition_count_, std::vector<float>(fft_size_, 0.0f))
        , overlap_(partition_size_, 0.0f)
        , zero_padded_scratch_(fft_size_, 0.0f)
        , freq_real_accum_(fft_size_, 0.0f)
        , freq_imag_accum_(fft_size_, 0.0f)
        , time_domain_scratch_(fft_size_, 0.0f)
    {
        for (std::uint32_t partition = 0; partition < partition_count_; ++partition)
        {
            std::vector<float> padded(fft_size_, 0.0f);
            const std::uint32_t start = partition * partition_size_;
            for (std::uint32_t i = 0; i < partition_size_; ++i)
            {
                const std::uint32_t source_index = start + i;
                padded[i] = (source_index < impulse_length) ? impulse_response[source_index] : 0.0f;
            }
            fft_.forward(padded.data(), ir_real_[partition].data(), ir_imag_[partition].data());
        }
    }

    void PartitionedConvolution::process(const float* input, float* output)
    {
        std::fill(zero_padded_scratch_.begin(), zero_padded_scratch_.end(), 0.0f);
        std::copy(input, input + partition_size_, zero_padded_scratch_.begin());

        fft_.forward(zero_padded_scratch_.data(),
            history_real_[write_index_].data(), history_imag_[write_index_].data());

        std::fill(freq_real_accum_.begin(), freq_real_accum_.end(), 0.0f);
        std::fill(freq_imag_accum_.begin(), freq_imag_accum_.end(), 0.0f);

        for (std::uint32_t partition = 0; partition < partition_count_; ++partition)
        {
            const std::uint32_t history_slot =
                (write_index_ + partition_count_ - partition) % partition_count_;
            const std::vector<float>& x_real = history_real_[history_slot];
            const std::vector<float>& x_imag = history_imag_[history_slot];
            const std::vector<float>& h_real = ir_real_[partition];
            const std::vector<float>& h_imag = ir_imag_[partition];

            for (std::uint32_t bin = 0; bin < fft_size_; ++bin)
            {
                freq_real_accum_[bin] += x_real[bin] * h_real[bin] - x_imag[bin] * h_imag[bin];
                freq_imag_accum_[bin] += x_real[bin] * h_imag[bin] + x_imag[bin] * h_real[bin];
            }
        }

        fft_.inverse(freq_real_accum_.data(), freq_imag_accum_.data(), time_domain_scratch_.data());

        for (std::uint32_t i = 0; i < partition_size_; ++i)
        {
            output[i] = time_domain_scratch_[i] + overlap_[i];
        }
        for (std::uint32_t i = 0; i < partition_size_; ++i)
        {
            overlap_[i] = time_domain_scratch_[partition_size_ + i];
        }

        write_index_ = (write_index_ + 1) % partition_count_;
    }
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `sd test`
Expected: PASS — both `PartitionedConvolution*` tests.

- [ ] **Step 6: Add the license header to both new files (copy `include/SushiDSP/math/Fft.hpp`'s and `src/math/Fft.cpp`'s headers verbatim, only the filename line differs) before committing.**

- [ ] **Step 7: Add `src/math/PartitionedConvolution.cpp` to the root `CMakeLists.txt`'s `sushi_dsp` source list, then commit**

```bash
git add include/SushiDSP/math/PartitionedConvolution.hpp src/math/PartitionedConvolution.cpp tests/functional/unit/test_partitioned_convolution.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -m "Add uniform-partitioned overlap-add FFT convolution"
```

---

## Task 2: `Oversampler` — halfband 2x stages cascaded into `Oversampler8x`

**Files:**
- Create: `include/SushiDSP/math/Oversampler.hpp`
- Create: `src/math/Oversampler.cpp`
- Test: `tests/functional/unit/test_oversampler.cpp`
- Modify: `CMakeLists.txt` (add `src/math/Oversampler.cpp`)
- Modify: `tests/CMakeLists.txt` (add the test file)

**Interfaces:**
- Produces: `SushiDSP::HalfbandUpsampler2x` (`process(const float* input, std::uint32_t frames, float* output)`, output has `2*frames` samples), `SushiDSP::HalfbandDownsampler2x` (`process(const float* input, std::uint32_t frames_in, float* output)`, `frames_in` even, output has `frames_in/2` samples), both with streaming state (a delay line member) and `reset()`. `SushiDSP::Oversampler8x` cascades three of each: `const float* upsample(const float* input, std::uint32_t frames)` returns an internally-owned buffer of `frames*8` samples; `void downsample(const float* oversampled, std::uint32_t frames_oversampled, float* output)` writes `frames_oversampled/8` samples. No other task in this plan consumes this — it is the building block D3/D4's amp/pedal nonlinear stages (8x oversample factor, spec §4 "Fixed capacities") will wrap their Newton-Raphson solve in.

**Design note on correctness-first scope:** the halfband FIR coefficients are generated by a windowed-sinc design in the constructor (Hamming window, cutoff at the new Nyquist/2) rather than a hand-tuned equiripple design, and applied via direct-form FIR (not an optimized polyphase structure that skips the halfband's near-zero even-index taps) — matching D0's own precedent of "correctness first, `Fft`'s class doc says 'split-radix' while the implementation is radix-2." A future optimization pass can convert to a true polyphase structure without changing this class's public interface.

- [ ] **Step 1: Write the failing test `tests/functional/unit/test_oversampler.cpp`**

```cpp
#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/math/Oversampler.hpp"

TEST(Unit_Dsp, HalfbandUpsamplerDoublesFrameCount)
{
    SushiDSP::HalfbandUpsampler2x upsampler;
    std::vector<float> input(64, 0.0f);
    std::vector<float> output(128, 0.0f);
    upsampler.process(input.data(), 64, output.data());
    // No crash / correct sizing is the assertion here; DC content checked below.
    SUCCEED();
}

TEST(Unit_Dsp, HalfbandUpsamplerPassesDcAtUnityGainAfterSettling)
{
    SushiDSP::HalfbandUpsampler2x upsampler;
    std::vector<float> input(64, 1.0f);
    std::vector<float> output(128, 0.0f);

    // Run several blocks to flush the FIR's group delay through the pipeline.
    for (int block = 0; block < 5; ++block)
    {
        upsampler.process(input.data(), 64, output.data());
    }

    for (std::size_t i = output.size() / 2; i < output.size(); ++i)
    {
        EXPECT_NEAR(output[i], 1.0f, 5e-2f) << "sample " << i;
    }
}

TEST(Unit_Dsp, HalfbandDownsamplerHalvesFrameCountAndPassesDc)
{
    SushiDSP::HalfbandDownsampler2x downsampler;
    std::vector<float> input(128, 1.0f);
    std::vector<float> output(64, 0.0f);

    for (int block = 0; block < 5; ++block)
    {
        downsampler.process(input.data(), 128, output.data());
    }

    for (std::size_t i = output.size() / 2; i < output.size(); ++i)
    {
        EXPECT_NEAR(output[i], 1.0f, 5e-2f) << "sample " << i;
    }
}

TEST(Unit_Dsp, Oversampler8xRoundTripPreservesDcAfterSettling)
{
    constexpr std::uint32_t frames = 32;
    SushiDSP::Oversampler8x oversampler(frames);
    std::vector<float> input(frames, 1.0f);
    std::vector<float> output(frames, 0.0f);

    for (int block = 0; block < 10; ++block)
    {
        const float* up = oversampler.upsample(input.data(), frames);
        oversampler.downsample(up, frames * 8, output.data());
    }

    for (std::size_t i = output.size() / 2; i < output.size(); ++i)
    {
        EXPECT_NEAR(output[i], 1.0f, 1e-1f) << "sample " << i;
    }
}

TEST(Unit_Dsp, HalfbandUpsamplerResetClearsState)
{
    SushiDSP::HalfbandUpsampler2x upsampler;
    std::vector<float> input(64, 1.0f);
    std::vector<float> output_a(128, 0.0f);
    std::vector<float> output_b(128, 0.0f);

    upsampler.process(input.data(), 64, output_a.data());
    upsampler.reset();
    upsampler.process(input.data(), 64, output_b.data());

    for (std::size_t i = 0; i < output_a.size(); ++i)
    {
        EXPECT_NEAR(output_a[i], output_b[i], 1e-6f) << "sample " << i;
    }
}
```

- [ ] **Step 2: Add the test file to CMake and run to verify it fails**

Edit `tests/CMakeLists.txt`'s `add_executable(sushidsp_tests ...)` source list to add `functional/unit/test_oversampler.cpp`.

Run: `sd test`
Expected: FAIL — `SushiDSP/math/Oversampler.hpp` does not exist.

- [ ] **Step 3: Write `include/SushiDSP/math/Oversampler.hpp`**

```cpp
#pragma once

#include <cstdint>
#include <vector>

namespace SushiDSP
{
    /// @brief Interpolates by 2x using a windowed-sinc halfband FIR lowpass. Allocation-free
    /// after construction; carries streaming state across process() calls.
    class HalfbandUpsampler2x
    {
    public:
        /// @param taps Odd FIR length (default 15); larger is a sharper anti-imaging filter.
        explicit HalfbandUpsampler2x(std::uint32_t taps = 15);

        /// @brief Zero-stuffs input to 2x rate and lowpass-filters it.
        /// @param input frames input samples.
        /// @param frames Number of input frames.
        /// @param output 2*frames output samples.
        void process(const float* input, std::uint32_t frames, float* output);

        void reset();

    private:
        std::vector<float> coefficients_;
        std::vector<float> delay_line_;
    };

    /// @brief Decimates by 2x using a windowed-sinc halfband FIR lowpass. Allocation-free
    /// after construction; carries streaming state across process() calls.
    class HalfbandDownsampler2x
    {
    public:
        /// @param taps Odd FIR length (default 15).
        explicit HalfbandDownsampler2x(std::uint32_t taps = 15);

        /// @brief Lowpass-filters input then drops every other sample.
        /// @param input frames_in input samples.
        /// @param frames_in Number of input frames; must be even.
        /// @param output frames_in/2 output samples.
        void process(const float* input, std::uint32_t frames_in, float* output);

        void reset();

    private:
        std::vector<float> coefficients_;
        std::vector<float> delay_line_;
    };

    /// @brief Cascades three halfband stages each way for 8x oversampling. Allocation-free
    /// after construction.
    class Oversampler8x
    {
    public:
        /// @param block_size The base-rate block size this instance will be called with.
        explicit Oversampler8x(std::uint32_t block_size);

        /// @brief Upsamples one base-rate block to 8x. Returns an internally-owned buffer.
        /// @param input block_size base-rate input samples.
        /// @param frames Must equal the block_size passed to the constructor.
        /// @return Pointer to frames*8 oversampled output samples, valid until the next call.
        const float* upsample(const float* input, std::uint32_t frames);

        /// @brief Downsamples an 8x-rate buffer back to the base rate.
        /// @param oversampled frames_oversampled samples at 8x rate.
        /// @param frames_oversampled Must equal 8 * the block_size passed to the constructor.
        /// @param output frames_oversampled/8 base-rate output samples.
        void downsample(const float* oversampled, std::uint32_t frames_oversampled, float* output);

    private:
        std::uint32_t block_size_;

        HalfbandUpsampler2x stage1_up_, stage2_up_, stage3_up_;
        HalfbandDownsampler2x stage1_down_, stage2_down_, stage3_down_;

        std::vector<float> buffer_2x_;
        std::vector<float> buffer_4x_;
        std::vector<float> buffer_8x_;
        std::vector<float> scratch_4x_;
        std::vector<float> scratch_2x_;
    };
}
```

- [ ] **Step 4: Write `src/math/Oversampler.cpp`**

```cpp
#include "SushiDSP/math/Oversampler.hpp"

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SushiDSP
{
    namespace
    {
        std::vector<float> design_halfband(std::uint32_t taps)
        {
            std::vector<float> coefficients(taps, 0.0f);
            const int center = static_cast<int>(taps - 1) / 2;
            for (int n = 0; n < static_cast<int>(taps); ++n)
            {
                const int k = n - center;
                float sinc_value;
                if (k == 0)
                {
                    sinc_value = 0.5f;
                }
                else
                {
                    sinc_value = std::sin(static_cast<float>(M_PI) * k / 2.0f) / (static_cast<float>(M_PI) * k);
                }
                const float window = 0.54f - 0.46f * std::cos(2.0f * static_cast<float>(M_PI) * n / (taps - 1));
                coefficients[n] = sinc_value * window;
            }
            return coefficients;
        }
    }

    HalfbandUpsampler2x::HalfbandUpsampler2x(std::uint32_t taps)
        : coefficients_(design_halfband(taps))
        , delay_line_(taps, 0.0f)
    {
    }

    void HalfbandUpsampler2x::process(const float* input, std::uint32_t frames, float* output)
    {
        const std::uint32_t taps = static_cast<std::uint32_t>(coefficients_.size());
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            for (std::uint32_t sub = 0; sub < 2; ++sub)
            {
                for (std::uint32_t i = taps - 1; i > 0; --i)
                {
                    delay_line_[i] = delay_line_[i - 1];
                }
                delay_line_[0] = (sub == 0) ? input[frame] * 2.0f : 0.0f;

                float accumulator = 0.0f;
                for (std::uint32_t i = 0; i < taps; ++i)
                {
                    accumulator += coefficients_[i] * delay_line_[i];
                }
                output[frame * 2 + sub] = accumulator;
            }
        }
    }

    void HalfbandUpsampler2x::reset()
    {
        std::fill(delay_line_.begin(), delay_line_.end(), 0.0f);
    }

    HalfbandDownsampler2x::HalfbandDownsampler2x(std::uint32_t taps)
        : coefficients_(design_halfband(taps))
        , delay_line_(taps, 0.0f)
    {
    }

    void HalfbandDownsampler2x::process(const float* input, std::uint32_t frames_in, float* output)
    {
        const std::uint32_t taps = static_cast<std::uint32_t>(coefficients_.size());
        std::uint32_t output_index = 0;
        for (std::uint32_t frame = 0; frame < frames_in; ++frame)
        {
            for (std::uint32_t i = taps - 1; i > 0; --i)
            {
                delay_line_[i] = delay_line_[i - 1];
            }
            delay_line_[0] = input[frame];

            if (frame % 2 == 0)
            {
                float accumulator = 0.0f;
                for (std::uint32_t i = 0; i < taps; ++i)
                {
                    accumulator += coefficients_[i] * delay_line_[i];
                }
                output[output_index++] = accumulator;
            }
        }
    }

    void HalfbandDownsampler2x::reset()
    {
        std::fill(delay_line_.begin(), delay_line_.end(), 0.0f);
    }

    Oversampler8x::Oversampler8x(std::uint32_t block_size)
        : block_size_(block_size)
        , buffer_2x_(block_size * 2, 0.0f)
        , buffer_4x_(block_size * 4, 0.0f)
        , buffer_8x_(block_size * 8, 0.0f)
        , scratch_4x_(block_size * 4, 0.0f)
        , scratch_2x_(block_size * 2, 0.0f)
    {
    }

    const float* Oversampler8x::upsample(const float* input, std::uint32_t frames)
    {
        stage1_up_.process(input, frames, buffer_2x_.data());
        stage2_up_.process(buffer_2x_.data(), frames * 2, buffer_4x_.data());
        stage3_up_.process(buffer_4x_.data(), frames * 4, buffer_8x_.data());
        return buffer_8x_.data();
    }

    void Oversampler8x::downsample(const float* oversampled, std::uint32_t frames_oversampled, float* output)
    {
        stage3_down_.process(oversampled, frames_oversampled, scratch_4x_.data());
        stage2_down_.process(scratch_4x_.data(), frames_oversampled / 2, scratch_2x_.data());
        stage1_down_.process(scratch_2x_.data(), frames_oversampled / 4, output);
    }
}
```

Include `<algorithm>` for `std::fill` if not transitively available.

- [ ] **Step 5: Run tests to verify they pass. If the DC-settling tolerance is too tight given the actual filter's group delay and passband ripple, it is acceptable to widen the tolerance or run more settling blocks — do not weaken the test to something that would pass a broken filter (e.g. do not remove the check that the *second half* of a late block is near 1.0; that specifically catches a filter that never converges).**

Run: `sd test`
Expected: PASS — all `Halfband*`/`Oversampler8x*` tests.

- [ ] **Step 6: Add the license header to both new files, add `src/math/Oversampler.cpp` to the root `CMakeLists.txt`'s `sushi_dsp` source list, then commit**

```bash
git add include/SushiDSP/math/Oversampler.hpp src/math/Oversampler.cpp tests/functional/unit/test_oversampler.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -m "Add halfband 2x stages cascaded into Oversampler8x"
```

---

## Task 3: `WdfResistor` + `WdfCapacitor` — linear WDF one-port primitives

**Files:**
- Create: `include/SushiDSP/math/Wdf.hpp`
- Create: `src/math/Wdf.cpp`
- Test: `tests/functional/unit/test_wdf.cpp`
- Modify: `CMakeLists.txt` (add `src/math/Wdf.cpp`)
- Modify: `tests/CMakeLists.txt` (add the test file)

**Interfaces:**
- Produces: `SushiDSP::WdfPort { float port_resistance; float incident_wave; float reflected_wave; }` (a plain POD struct — `a`/`b`/`R` in WDF notation), `SushiDSP::WdfResistor` (`explicit WdfResistor(float resistance_ohms)`, `float port_resistance() const`, `float reflect(float incident_wave)` — a matched resistor always reflects 0), `SushiDSP::WdfCapacitor` (`WdfCapacitor(double sample_rate, float capacitance_farads)`, `float port_resistance() const`, `float reflect(float incident_wave)` — bilinear-transform discretization, `b[n] = a[n-1]`, `void reset()`). Task 4 (the diode clipper) uses the same `port_resistance()`-plus-Ohm's-law pattern these classes establish, though it does not directly instantiate `WdfResistor`/`WdfCapacitor` (the clipper's linear side is a Thevenin source, simpler than a general resistor one-port — see Task 4).

**Scope note:** per this plan's architecture section, `WdfSeriesAdaptor`/`WdfParallelAdaptor` are explicitly NOT built in this task — they combine multiple one-ports into a tree, and D2 has no real multi-element circuit to validate that combining logic against. `WdfResistor` and `WdfCapacitor` are validated here as standalone one-ports via their defining port equations (algebraic/golden-vector tests), which is the right scope for kernels with no consumer yet.

- [ ] **Step 1: Write the failing test `tests/functional/unit/test_wdf.cpp`**

```cpp
#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/math/Wdf.hpp"

TEST(Unit_Dsp, WdfResistorReportsConfiguredPortResistance)
{
    SushiDSP::WdfResistor resistor(1000.0f);
    EXPECT_FLOAT_EQ(resistor.port_resistance(), 1000.0f);
}

TEST(Unit_Dsp, WdfResistorAlwaysReflectsZero)
{
    SushiDSP::WdfResistor resistor(1000.0f);
    EXPECT_FLOAT_EQ(resistor.reflect(5.0f), 0.0f);
    EXPECT_FLOAT_EQ(resistor.reflect(-3.0f), 0.0f);
    EXPECT_FLOAT_EQ(resistor.reflect(0.0f), 0.0f);
}

TEST(Unit_Dsp, WdfCapacitorPortResistanceMatchesBilinearFormula)
{
    constexpr double sample_rate = 48000.0;
    constexpr float capacitance = 1e-6f;  // 1 microfarad
    SushiDSP::WdfCapacitor capacitor(sample_rate, capacitance);

    const float expected_resistance = static_cast<float>(1.0 / sample_rate) / (2.0f * capacitance);
    EXPECT_NEAR(capacitor.port_resistance(), expected_resistance, expected_resistance * 1e-4f);
}

TEST(Unit_Dsp, WdfCapacitorReflectsPreviousIncidentWave)
{
    SushiDSP::WdfCapacitor capacitor(48000.0, 1e-6f);

    EXPECT_FLOAT_EQ(capacitor.reflect(0.0f), 0.0f);  // initial state is zero
    EXPECT_FLOAT_EQ(capacitor.reflect(2.0f), 0.0f);  // b[1] = a[0] = 0
    EXPECT_FLOAT_EQ(capacitor.reflect(3.0f), 2.0f);  // b[2] = a[1] = 2
    EXPECT_FLOAT_EQ(capacitor.reflect(-1.0f), 3.0f); // b[3] = a[2] = 3
}

TEST(Unit_Dsp, WdfCapacitorResetClearsState)
{
    SushiDSP::WdfCapacitor capacitor(48000.0, 1e-6f);
    capacitor.reflect(5.0f);
    capacitor.reset();
    EXPECT_FLOAT_EQ(capacitor.reflect(1.0f), 0.0f);
}
```

- [ ] **Step 2: Add the test file to CMake and run to verify it fails**

Edit `tests/CMakeLists.txt`'s `add_executable(sushidsp_tests ...)` source list to add `functional/unit/test_wdf.cpp`.

Run: `sd test`
Expected: FAIL — `SushiDSP/math/Wdf.hpp` does not exist.

- [ ] **Step 3: Write `include/SushiDSP/math/Wdf.hpp`**

```cpp
#pragma once

namespace SushiDSP
{
    /// @brief A Wave Digital Filter one-port's wave-domain state: incident wave a, reflected
    /// wave b, and the port resistance R that relates them to voltage/current.
    struct WdfPort
    {
        float port_resistance = 1.0f;
        float incident_wave = 0.0f;
        float reflected_wave = 0.0f;
    };

    /// @brief An adapted (reflection-free) linear resistor one-port.
    class WdfResistor
    {
    public:
        /// @param resistance_ohms The resistor's value; also its WDF port resistance.
        explicit WdfResistor(float resistance_ohms);

        /// @brief The port resistance this element presents (equals its resistance).
        /// @return The port resistance in ohms.
        float port_resistance() const;

        /// @brief Computes the reflected wave for a given incident wave.
        /// @param incident_wave The incident wave a.
        /// @return The reflected wave b, always 0 for a matched resistor.
        float reflect(float incident_wave);

    private:
        float resistance_;
    };

    /// @brief A linear capacitor one-port, discretized via the bilinear transform.
    class WdfCapacitor
    {
    public:
        /// @param sample_rate The sample rate in Hz.
        /// @param capacitance_farads The capacitance in farads.
        WdfCapacitor(double sample_rate, float capacitance_farads);

        /// @brief The port resistance R = T / (2C), where T is the sample period.
        /// @return The port resistance in ohms.
        float port_resistance() const;

        /// @brief Computes the reflected wave: b[n] = a[n-1] (the bilinear-transform result).
        /// @param incident_wave The incident wave a[n].
        /// @return The reflected wave b[n] = the previous call's incident wave.
        float reflect(float incident_wave);

        /// @brief Clears the stored previous incident wave.
        void reset();

    private:
        float port_resistance_;
        float previous_incident_wave_ = 0.0f;
    };
}
```

- [ ] **Step 4: Write `src/math/Wdf.cpp`**

```cpp
#include "SushiDSP/math/Wdf.hpp"

namespace SushiDSP
{
    WdfResistor::WdfResistor(float resistance_ohms)
        : resistance_(resistance_ohms)
    {
    }

    float WdfResistor::port_resistance() const
    {
        return resistance_;
    }

    float WdfResistor::reflect(float)
    {
        return 0.0f;
    }

    WdfCapacitor::WdfCapacitor(double sample_rate, float capacitance_farads)
        : port_resistance_(static_cast<float>(1.0 / sample_rate) / (2.0f * capacitance_farads))
    {
    }

    float WdfCapacitor::port_resistance() const
    {
        return port_resistance_;
    }

    float WdfCapacitor::reflect(float incident_wave)
    {
        const float reflected = previous_incident_wave_;
        previous_incident_wave_ = incident_wave;
        return reflected;
    }

    void WdfCapacitor::reset()
    {
        previous_incident_wave_ = 0.0f;
    }
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `sd test`
Expected: PASS — all `Wdf*` tests.

- [ ] **Step 6: Add the license header to both new files, add `src/math/Wdf.cpp` to the root `CMakeLists.txt`'s `sushi_dsp` source list, then commit**

```bash
git add include/SushiDSP/math/Wdf.hpp src/math/Wdf.cpp tests/functional/unit/test_wdf.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -m "Add WdfResistor and WdfCapacitor one-port primitives"
```

---

## Task 4: `NewtonRaphsonSolver` + `DiodeClipperCircuit` — the D2 "ships" proof circuit

**Files:**
- Create: `include/SushiDSP/math/NewtonRaphsonSolver.hpp`
- Create: `src/math/NewtonRaphsonSolver.cpp`
- Create: `include/SushiDSP/circuits/DiodeClipperCircuit.hpp`
- Create: `src/circuits/DiodeClipperCircuit.cpp`
- Test: `tests/functional/unit/test_newton_raphson_solver.cpp`
- Test: `tests/functional/unit/test_diode_clipper_circuit.cpp`
- Modify: `CMakeLists.txt` (add `src/math/NewtonRaphsonSolver.cpp` and `src/circuits/DiodeClipperCircuit.cpp`)
- Modify: `tests/CMakeLists.txt` (add both test files)

**Interfaces:**
- Produces: `SushiDSP::NewtonRaphsonSolver::solve(function, derivative, initial_guess, max_iterations = 4)` (static method, `function`/`derivative` are `std::function<float(float)>`), and `SushiDSP::DiodeClipperCircuit` (`DiodeClipperCircuit(float source_resistance_ohms, float saturation_current_amps, float thermal_voltage_volts)`, `float process_sample(float source_voltage)`, `void reset()`).

**Circuit derivation (a single WDF root, no adaptor needed — this is why D2 does not need `WdfSeriesAdaptor`):** the circuit is `Vs -> Rs -> [antiparallel diode pair] -> ground`. Because `Vs` and `Rs` are in series with nothing else, the linear side presents a Thevenin one-port directly to the nonlinear diode-pair root: port resistance `R = Rs`, incident wave `a = Vs` each sample (no reflection to compute — the source's own reflection is folded into treating `a` as the open-circuit Thevenin voltage). At the nonlinear root, KCL gives the diode-pair current `I = Is*(exp(V/Vt) - exp(-V/Vt))` (Shockley model, antiparallel pair) must equal the port current `I = (a - V) / R` (derived from `V = (a+b)/2`, `I = (a-b)/(2R)`, eliminating `b = 2V - a`). Newton-Raphson solves `f(V) = R*Is*(exp(V/Vt) - exp(-V/Vt)) + V - a = 0` for `V`, warm-started from the previous sample's solution. The output tap is `V` itself (the voltage across the diode pair — the clipped signal).

- [ ] **Step 1: Write the failing test `tests/functional/unit/test_newton_raphson_solver.cpp`**

```cpp
#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/math/NewtonRaphsonSolver.hpp"

TEST(Unit_Dsp, NewtonRaphsonSolverFindsSquareRootOfTwo)
{
    auto function = [](float x) { return x * x - 2.0f; };
    auto derivative = [](float x) { return 2.0f * x; };

    const float root = SushiDSP::NewtonRaphsonSolver::solve(function, derivative, 1.0f, 6);

    EXPECT_NEAR(root, std::sqrt(2.0f), 1e-4f);
}

TEST(Unit_Dsp, NewtonRaphsonSolverConvergesFromNegativeGuess)
{
    auto function = [](float x) { return x * x - 2.0f; };
    auto derivative = [](float x) { return 2.0f * x; };

    const float root = SushiDSP::NewtonRaphsonSolver::solve(function, derivative, -1.0f, 6);

    EXPECT_NEAR(root, -std::sqrt(2.0f), 1e-4f);
}

TEST(Unit_Dsp, NewtonRaphsonSolverHandlesNearZeroDerivativeWithoutDivergingToNanOrInf)
{
    // f(x) = x^3, root at 0, derivative 3x^2 vanishes exactly at the root.
    auto function = [](float x) { return x * x * x; };
    auto derivative = [](float x) { return 3.0f * x * x; };

    const float result = SushiDSP::NewtonRaphsonSolver::solve(function, derivative, 0.5f, 8);

    EXPECT_TRUE(std::isfinite(result));
}

TEST(Unit_Dsp, NewtonRaphsonSolverWarmStartConvergesInFewerIterationsThanColdStart)
{
    auto function = [](float x) { return x * x - 2.0f; };
    auto derivative = [](float x) { return 2.0f * x; };

    const float warm = SushiDSP::NewtonRaphsonSolver::solve(function, derivative, 1.4f, 2);
    const float cold = SushiDSP::NewtonRaphsonSolver::solve(function, derivative, 0.01f, 2);

    const float target = std::sqrt(2.0f);
    EXPECT_LT(std::abs(warm - target), std::abs(cold - target));
}
```

- [ ] **Step 2: Add `test_newton_raphson_solver.cpp` to CMake and run to verify it fails**

Edit `tests/CMakeLists.txt`'s `add_executable(sushidsp_tests ...)` source list.

Run: `sd test`
Expected: FAIL — `SushiDSP/math/NewtonRaphsonSolver.hpp` does not exist.

- [ ] **Step 3: Write `include/SushiDSP/math/NewtonRaphsonSolver.hpp`**

```cpp
#pragma once

#include <cstdint>
#include <functional>

namespace SushiDSP
{
    /// @brief Generic Newton-Raphson root finder for a scalar nonlinear equation f(x) = 0.
    class NewtonRaphsonSolver
    {
    public:
        /// @brief Finds x such that function(x) is approximately zero, starting from initial_guess.
        /// @param function The residual function f(x).
        /// @param derivative The derivative f'(x).
        /// @param initial_guess Starting point; warm-starting from the previous sample's
        /// solution is what makes 2-4 iterations sufficient at audio rates.
        /// @param max_iterations Maximum Newton iterations.
        /// @return The converged root estimate.
        static float solve(
            const std::function<float(float)>& function,
            const std::function<float(float)>& derivative,
            float initial_guess,
            std::uint32_t max_iterations = 4);
    };
}
```

- [ ] **Step 4: Write `src/math/NewtonRaphsonSolver.cpp`**

```cpp
#include "SushiDSP/math/NewtonRaphsonSolver.hpp"

#include <cmath>

namespace SushiDSP
{
    float NewtonRaphsonSolver::solve(
        const std::function<float(float)>& function,
        const std::function<float(float)>& derivative,
        float initial_guess,
        std::uint32_t max_iterations)
    {
        float x = initial_guess;
        constexpr float minimum_derivative_magnitude = 1e-6f;

        for (std::uint32_t iteration = 0; iteration < max_iterations; ++iteration)
        {
            const float f_value = function(x);
            float f_prime_value = derivative(x);

            if (std::abs(f_prime_value) < minimum_derivative_magnitude)
            {
                f_prime_value = (f_prime_value < 0.0f) ? -minimum_derivative_magnitude : minimum_derivative_magnitude;
            }

            x = x - f_value / f_prime_value;
        }

        return x;
    }
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `sd test`
Expected: PASS — all `NewtonRaphsonSolver*` tests.

- [ ] **Step 6: Write the failing test `tests/functional/unit/test_diode_clipper_circuit.cpp`**

```cpp
#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/DiodeClipperCircuit.hpp"

namespace
{
    // Typical silicon small-signal diode values.
    constexpr float kSourceResistance = 1000.0f;
    constexpr float kSaturationCurrent = 1e-12f;
    constexpr float kThermalVoltage = 0.02585f;
}

TEST(Unit_Dsp, DiodeClipperCircuitIsDcStableForConstantInput)
{
    SushiDSP::DiodeClipperCircuit circuit(kSourceResistance, kSaturationCurrent, kThermalVoltage);

    float output = 0.0f;
    for (int i = 0; i < 1000; ++i)
    {
        output = circuit.process_sample(0.1f);
    }

    EXPECT_TRUE(std::isfinite(output));
}

TEST(Unit_Dsp, DiodeClipperCircuitIsApproximatelyOddSymmetric)
{
    SushiDSP::DiodeClipperCircuit positive_circuit(kSourceResistance, kSaturationCurrent, kThermalVoltage);
    SushiDSP::DiodeClipperCircuit negative_circuit(kSourceResistance, kSaturationCurrent, kThermalVoltage);

    float positive_output = 0.0f;
    float negative_output = 0.0f;
    for (int i = 0; i < 50; ++i)
    {
        positive_output = positive_circuit.process_sample(0.3f);
        negative_output = negative_circuit.process_sample(-0.3f);
    }

    EXPECT_NEAR(positive_output, -negative_output, 1e-3f);
}

TEST(Unit_Dsp, DiodeClipperCircuitIsNearlyLinearForTinyInput)
{
    SushiDSP::DiodeClipperCircuit circuit(kSourceResistance, kSaturationCurrent, kThermalVoltage);

    float output = 0.0f;
    for (int i = 0; i < 10; ++i)
    {
        output = circuit.process_sample(0.001f);
    }

    EXPECT_NEAR(output, 0.001f, 2e-4f);
}

TEST(Unit_Dsp, DiodeClipperCircuitCompressesLargeInput)
{
    SushiDSP::DiodeClipperCircuit circuit(kSourceResistance, kSaturationCurrent, kThermalVoltage);

    float output = 0.0f;
    for (int i = 0; i < 50; ++i)
    {
        output = circuit.process_sample(5.0f);
    }

    EXPECT_LT(std::abs(output), 1.0f);
    EXPECT_GT(output, 0.0f);
}

TEST(Unit_Dsp, DiodeClipperCircuitResetClearsWarmStartState)
{
    SushiDSP::DiodeClipperCircuit circuit_a(kSourceResistance, kSaturationCurrent, kThermalVoltage);
    SushiDSP::DiodeClipperCircuit circuit_b(kSourceResistance, kSaturationCurrent, kThermalVoltage);

    circuit_a.process_sample(2.0f);
    circuit_a.reset();

    const float from_reset = circuit_a.process_sample(0.05f);
    const float from_fresh = circuit_b.process_sample(0.05f);

    EXPECT_NEAR(from_reset, from_fresh, 1e-5f);
}
```

- [ ] **Step 7: Add both new test files to CMake and run to verify `test_diode_clipper_circuit.cpp` fails**

Edit `tests/CMakeLists.txt`'s `add_executable(sushidsp_tests ...)` source list to add `functional/unit/test_diode_clipper_circuit.cpp`.

Run: `sd test`
Expected: FAIL — `SushiDSP/circuits/DiodeClipperCircuit.hpp` does not exist.

- [ ] **Step 8: Write `include/SushiDSP/circuits/DiodeClipperCircuit.hpp`**

```cpp
#pragma once

namespace SushiDSP
{
    /// @brief A minimal one-stage WDF nonlinear circuit: Vs -> Rs -> antiparallel diode pair ->
    /// ground. Proves the WDF + Newton-Raphson solver pipeline end to end. Not a product — no
    /// NodeDescriptor; JCM800/Tube Screamer (D3/D4) reuse this pattern, not this exact class.
    class DiodeClipperCircuit
    {
    public:
        /// @param source_resistance_ohms Rs, the source's series resistance and WDF port resistance.
        /// @param saturation_current_amps Diode saturation current Is (Shockley model).
        /// @param thermal_voltage_volts Diode thermal voltage Vt (~0.02585 V at room temperature).
        DiodeClipperCircuit(float source_resistance_ohms, float saturation_current_amps, float thermal_voltage_volts);

        /// @brief Processes one sample: given the source voltage, returns the clipped output voltage.
        /// @param source_voltage Vs for this sample.
        /// @return The voltage across the diode pair (the clipped output).
        float process_sample(float source_voltage);

        /// @brief Clears the Newton-Raphson warm-start state.
        void reset();

    private:
        float source_resistance_;
        float saturation_current_;
        float thermal_voltage_;
        float previous_voltage_ = 0.0f;
    };
}
```

- [ ] **Step 9: Write `src/circuits/DiodeClipperCircuit.cpp`**

```cpp
#include "SushiDSP/circuits/DiodeClipperCircuit.hpp"

#include <cmath>

#include "SushiDSP/math/NewtonRaphsonSolver.hpp"

namespace SushiDSP
{
    DiodeClipperCircuit::DiodeClipperCircuit(
        float source_resistance_ohms, float saturation_current_amps, float thermal_voltage_volts)
        : source_resistance_(source_resistance_ohms)
        , saturation_current_(saturation_current_amps)
        , thermal_voltage_(thermal_voltage_volts)
    {
    }

    float DiodeClipperCircuit::process_sample(float source_voltage)
    {
        const float r = source_resistance_;
        const float is = saturation_current_;
        const float vt = thermal_voltage_;
        const float a = source_voltage;

        auto residual = [r, is, vt, a](float v)
        {
            return r * is * (std::exp(v / vt) - std::exp(-v / vt)) + v - a;
        };
        auto derivative = [r, is, vt](float v)
        {
            return r * is / vt * (std::exp(v / vt) + std::exp(-v / vt)) + 1.0f;
        };

        const float voltage = NewtonRaphsonSolver::solve(residual, derivative, previous_voltage_, 4);
        previous_voltage_ = voltage;
        return voltage;
    }

    void DiodeClipperCircuit::reset()
    {
        previous_voltage_ = 0.0f;
    }
}
```

**Note for the implementer:** for large `source_voltage` (e.g. the plausibility test's `5.0f`), `std::exp(v / vt)` can overflow if the Newton iteration overshoots before converging (v/vt can reach into the hundreds transiently). If the "compresses large input" or "DC stable" tests produce `inf`/`nan` from `std::exp` overflow during early iterations, this is a real numerical-stability bug worth fixing — not a test to weaken. A robust fix (standard in diode-clipper WDF implementations) is clamping the trial `v` each Newton step to a safe range (e.g. `±20*vt`, since `exp(20) ≈ 5e8` already dwarfs any audio-relevant current) before evaluating `residual`/`derivative`; if you need this, add the clamp inside `process_sample`'s Newton loop — which may mean not using `NewtonRaphsonSolver::solve` directly for this circuit and instead inlining a clamped variant, or extending `NewtonRaphsonSolver` with an optional per-iteration clamp callback. Use your judgment on the cleanest fix; document what you chose and why in your report.

- [ ] **Step 10: Run tests to verify they pass**

Run: `sd test`
Expected: PASS — all `DiodeClipperCircuit*` tests.

- [ ] **Step 11: Add the license header to all four new files, add `src/math/NewtonRaphsonSolver.cpp` and `src/circuits/DiodeClipperCircuit.cpp` to the root `CMakeLists.txt`'s `sushi_dsp` source list, then commit**

```bash
git add include/SushiDSP/math/NewtonRaphsonSolver.hpp src/math/NewtonRaphsonSolver.cpp \
        include/SushiDSP/circuits/DiodeClipperCircuit.hpp src/circuits/DiodeClipperCircuit.cpp \
        tests/functional/unit/test_newton_raphson_solver.cpp tests/functional/unit/test_diode_clipper_circuit.cpp \
        tests/CMakeLists.txt CMakeLists.txt
git commit -m "Add NewtonRaphsonSolver and the D2 diode-clipper proof circuit"
```

---

## Self-Review Notes

- **Spec coverage:** §4.2 partitioned convolution → Task 1. §4.4 resamplers (oversampler cascade) → Task 2. §4.5 WDF/DK circuit-solving core → Tasks 3-4, explicitly scoped down to one-port primitives + Newton-Raphson + one proof circuit, NOT the full series/parallel adaptor tree (documented as a deliberate deferral to D3/D4 in this plan's Architecture section, not a silent gap). §9 D2 roadmap line ("WDF/DK shared nonlinear-circuit infrastructure... partitioned convolution... oversampling... golden-vector tests against known WDF reference circuits. Ships: a minimal one-stage WDF test circuit processes correctly") → satisfied by Task 4's `DiodeClipperCircuit`, which is exactly a minimal one-stage WDF circuit with golden/plausibility tests (DC stability, odd symmetry, small-signal linearity, large-signal compression).
- **Gap flagged explicitly (not silently dropped):** `WdfSeriesAdaptor`/`WdfParallelAdaptor` (spec §4.5's "series/parallel adaptor" primitives) are NOT built in this plan. They combine multiple one-ports into an arbitrary tree topology; D2 has no multi-element circuit to validate that combining logic against, and inventing one risks getting the adaptor math wrong with nothing to catch it. D3 (JCM800, whose tone stack genuinely needs a 3-port network) or D4 (Tube Screamer, whose op-amp clipping stage needs a feedback-loop topology) is where the real circuit drives the right adaptor abstraction.
- **Placeholder scan:** none found — every step has concrete code or a concrete shell command; Task 4 Step 9's note about potential `exp` overflow is a real numerical-stability concern flagged for the implementer to resolve, not a placeholder.
- **Type consistency:** `WdfPort`'s field names (`port_resistance`, `incident_wave`, `reflected_wave`) are declared in Task 3 but not directly used by Task 4's `DiodeClipperCircuit` (which inlines the same a/b/R algebra without instantiating the struct, since its linear side is a Thevenin source rather than a general one-port) — this is intentional per the Interfaces note in Task 4, not a drift. `Fft` (D0) is consumed by `PartitionedConvolution` (Task 1) with the exact `forward(const float*, float*, float*)`/`inverse(const float*, const float*, float*)`/`size()` signatures verified against `include/SushiDSP/math/Fft.hpp` as it exists after D0/D1 merge.
