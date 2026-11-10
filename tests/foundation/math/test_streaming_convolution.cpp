/**************************************************************************/
/* test_streaming_convolution.cpp                                         */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_streaming_convolution.cpp
 * @brief Tests StreamingConvolution against the direct form.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "foundation/math/fft_error_bound.hpp"
#include "support/tolerance.hpp"

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/math/streaming_convolution.hpp>

namespace
{
    constexpr std::uint32_t K_PARTITION = 128;
    constexpr std::uint32_t K_MAX_BLOCK = 4096;

    /// @brief Returns the first x.size() samples of the convolution of x and h, in double precision.
    std::vector<double> direct_convolution(const std::vector<float>& x, const std::vector<float>& h)
    {
        std::vector<double> y(x.size(), 0.0);
        for (std::size_t n = 0; n < x.size(); ++n)
        {
            for (std::size_t k = 0; k < h.size() && k <= n; ++k)
            {
                y[n] += static_cast<double>(x[n - k]) * h[k];
            }
        }
        return y;
    }

    /// @brief Returns the decaying sine impulse response of 300 samples.
    std::vector<float> decaying_response()
    {
        std::vector<float> response(300);
        for (std::size_t i = 0; i < response.size(); ++i)
        {
            response[i] = std::sin(0.37f * static_cast<float>(i)) * std::exp(-0.01f * static_cast<float>(i));
        }
        return response;
    }

    /// @brief Returns 5000 samples of two summed tones.
    std::vector<float> two_tone_signal()
    {
        std::vector<float> signal(5000);
        for (std::size_t i = 0; i < signal.size(); ++i)
        {
            signal[i] = std::sin(0.05f * static_cast<float>(i)) + 0.3f * std::cos(0.9f * static_cast<float>(i));
        }
        return signal;
    }

    /// @brief Feeds the signal through the convolution in chunks of the given sizes, repeating them as needed.
    std::vector<float> stream_in_chunks(SushiDSP::StreamingConvolution& convolution, const std::vector<float>& signal,
                                        const std::vector<std::uint32_t>& chunks)
    {
        std::vector<float> streamed;
        std::vector<float> out(K_MAX_BLOCK);
        std::size_t cursor = 0;
        std::size_t chunk_index = 0;
        while (cursor < signal.size())
        {
            const std::uint32_t frames = static_cast<std::uint32_t>(
                std::min<std::size_t>(chunks[chunk_index++ % chunks.size()], signal.size() - cursor));
            convolution.process(signal.data() + cursor, out.data(), frames);
            streamed.insert(streamed.end(), out.begin(), out.begin() + frames);
            cursor += frames;
        }
        return streamed;
    }

    /// @brief Returns the largest difference between the stream, delayed by the latency, and the direct form.
    double worst_difference_after_latency(const std::vector<float>& streamed, const std::vector<double>& expected,
                                          std::uint32_t latency)
    {
        double worst = 0.0;
        for (std::size_t n = latency; n < streamed.size(); ++n)
        {
            worst = std::max(worst, std::abs(static_cast<double>(streamed[n]) - expected[n - latency]));
        }
        return worst;
    }

    /// @brief Processes the same block repeatedly under an armed trap and returns the allocations counted.
    int allocations_over_blocks(SushiDSP::StreamingConvolution& convolution, const std::vector<float>& input,
                                int blocks)
    {
        std::vector<float> output(input.size(), 0.0f);
        SushiDSP::test::AllocationTrap trap;
        trap.arm();
        for (int block = 0; block < blocks; ++block)
        {
            convolution.process(input.data(), output.data(), static_cast<std::uint32_t>(input.size()));
        }
        trap.disarm();
        return trap.count();
    }
} // namespace

TEST(Small_StreamingConvolution, MatchesDirectFormForIrregularBlockSizes)
{
    const std::vector<float> response = decaying_response();
    const std::vector<float> signal = two_tone_signal();
    SushiDSP::StreamingConvolution convolution(K_PARTITION, response.data(),
                                               static_cast<std::uint32_t>(response.size()), K_MAX_BLOCK);

    const std::vector<float> streamed = stream_in_chunks(convolution, signal, {1, 7, 64, 100, 256, 1000, 4096});

    const double bound = TestSupport::partitioned_block_error_bound(
        K_PARTITION, 3, TestSupport::largest_block_norm(signal, K_PARTITION),
        TestSupport::largest_slice_l1(response, K_PARTITION));
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        worst_difference_after_latency(streamed, direct_convolution(signal, response), convolution.latency_samples()),
        0.0, bound)));
}

TEST(Small_StreamingConvolution, LatencyIsOnePartition)
{
    const std::vector<float> response = decaying_response();
    const SushiDSP::StreamingConvolution convolution(K_PARTITION, response.data(),
                                                     static_cast<std::uint32_t>(response.size()), K_MAX_BLOCK);

    EXPECT_EQ(convolution.latency_samples(), K_PARTITION);
}

TEST(Small_StreamingConvolution, PartitionSizeFollowsSampleRate)
{
    EXPECT_EQ(SushiDSP::StreamingConvolution::partition_size_for(44100.0), 128u);
    EXPECT_EQ(SushiDSP::StreamingConvolution::partition_size_for(48000.0), 128u);
    EXPECT_EQ(SushiDSP::StreamingConvolution::partition_size_for(96000.0), 256u);
}

TEST(Small_StreamingConvolution, ProcessPerformsNoHeapAllocation)
{
    std::vector<float> response(300, 0.0f);
    for (std::size_t i = 0; i < response.size(); ++i)
    {
        response[i] = 1.0f / (1.0f + static_cast<float>(i));
    }
    SushiDSP::StreamingConvolution convolution(K_PARTITION, response.data(),
                                               static_cast<std::uint32_t>(response.size()), K_MAX_BLOCK);
    std::vector<float> input(37, 0.0f);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = static_cast<float>(i % 5) * 0.1f - 0.2f;
    }

    // Six blocks of 37 frames are 222, so at least one full partition of 128 is convolved under the trap.
    EXPECT_EQ(allocations_over_blocks(convolution, input, 6), 0);
}
