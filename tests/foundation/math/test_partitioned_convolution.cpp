/**************************************************************************/
/* test_partitioned_convolution.cpp                                       */
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
 * @file test_partitioned_convolution.cpp
 * @brief Tests PartitionedConvolution against direct convolution.
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
#include <SushiDSP/foundation/math/partitioned_convolution.hpp>

namespace
{
    constexpr std::uint32_t K_PARTITION = 64;

    /// @brief Returns the full convolution of x and h in double precision.
    std::vector<double> direct_convolution(const std::vector<float>& x, const std::vector<float>& h)
    {
        std::vector<double> y(x.size() + h.size() - 1, 0.0);
        for (std::size_t n = 0; n < x.size(); ++n)
        {
            for (std::size_t k = 0; k < h.size(); ++k)
            {
                y[n + k] += static_cast<double>(x[n]) * h[k];
            }
        }
        return y;
    }

    /// @brief Returns count samples in [-1, 1) from a linear congruential generator with a fixed seed.
    std::vector<float> noise(std::size_t count, unsigned seed)
    {
        std::vector<float> samples(count);
        for (float& sample : samples)
        {
            seed = seed * 1664525u + 1013904223u;
            sample = (static_cast<float>(seed >> 8) / static_cast<float>(1u << 24)) * 2.0f - 1.0f;
        }
        return samples;
    }

    /// @brief Returns the impulse response 0.5 sin(0.1 i) of the given length.
    std::vector<float> sine_response(std::size_t length)
    {
        std::vector<float> response(length);
        for (std::size_t i = 0; i < length; ++i)
        {
            response[i] = std::sin(0.1f * static_cast<float>(i)) * 0.5f;
        }
        return response;
    }

    /// @brief Feeds the input through the convolution block by block and returns what it emitted.
    std::vector<float> stream(SushiDSP::PartitionedConvolution& convolution, const std::vector<float>& input)
    {
        std::vector<float> streamed;
        for (std::size_t position = 0; position < input.size(); position += K_PARTITION)
        {
            std::vector<float> output(K_PARTITION, 0.0f);
            convolution.process(input.data() + position, output.data());
            streamed.insert(streamed.end(), output.begin(), output.end());
        }
        return streamed;
    }

    /// @brief Returns four blocks of a unit impulse followed by silence.
    std::vector<float> impulse_blocks()
    {
        std::vector<float> input(K_PARTITION * 4, 0.0f);
        input[0] = 1.0f;
        return input;
    }

    /// @brief Returns the largest absolute difference between a stream and a reference over [begin, end).
    template <typename Reference>
    double worst_difference(const std::vector<float>& streamed, const std::vector<Reference>& reference,
                            std::size_t begin, std::size_t end)
    {
        double worst = 0.0;
        for (std::size_t i = begin; i < end; ++i)
        {
            worst = std::max(worst, std::abs(static_cast<double>(streamed[i]) - static_cast<double>(reference[i])));
        }
        return worst;
    }

    /// The impulse response of 100 samples convolved with a unit impulse, over four blocks.
    struct ImpulseRun
    {
            std::vector<float> response = sine_response(100);
            std::vector<float> collected;
            double bound = 0.0;

            ImpulseRun()
            {
                SushiDSP::PartitionedConvolution convolution(K_PARTITION, response.data(),
                                                             static_cast<std::uint32_t>(response.size()));
                collected = stream(convolution, impulse_blocks());
                bound = TestSupport::partitioned_block_error_bound(K_PARTITION, 2, 1.0,
                                                                   TestSupport::largest_slice_l1(response, K_PARTITION));
            }
    };

    /// @brief Runs a reset-after-one-block convolution and returns the block it emits for silence.
    std::vector<float> output_for_silence_after_reset()
    {
        const float response[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        SushiDSP::PartitionedConvolution convolution(4, response, 4);
        const float block[4] = {1.0f, 0.0f, 0.0f, 0.0f};
        const float silence[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        std::vector<float> output(4, 0.0f);
        convolution.process(block, output.data());
        convolution.reset();
        convolution.process(silence, output.data());
        return output;
    }
} // namespace

TEST(Small_PartitionedConvolution, ImpulseReproducesTheImpulseResponse)
{
    const ImpulseRun run;

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        worst_difference(run.collected, run.response, 0, run.response.size()), 0.0, run.bound)));
}

TEST(Small_PartitionedConvolution, ImpulseLeavesNothingAfterTheResponse)
{
    const ImpulseRun run;

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        worst_difference(run.collected, std::vector<float>(run.collected.size(), 0.0f), run.response.size(),
                         run.collected.size()),
        0.0, run.bound)));
}

// The impulse response of 150 samples spans three partitions, the last one padded.
TEST(Small_PartitionedConvolution, MatchesDirectConvolutionForWhiteNoise)
{
    const std::vector<float> response = [] {
        std::vector<float> h = noise(150, 12345u);
        std::transform(h.begin(), h.end(), h.begin(), [](float value) { return value * 0.3f; });
        return h;
    }();
    const std::vector<float> input = noise(K_PARTITION * 6, 54321u);
    SushiDSP::PartitionedConvolution convolution(K_PARTITION, response.data(),
                                                 static_cast<std::uint32_t>(response.size()));

    const std::vector<float> streamed = stream(convolution, input);

    const std::vector<double> reference = direct_convolution(input, response);
    const double bound = TestSupport::partitioned_block_error_bound(
        K_PARTITION, 3, TestSupport::largest_block_norm(input, K_PARTITION),
        TestSupport::largest_slice_l1(response, K_PARTITION));
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(worst_difference(streamed, reference, 0, streamed.size()), 0.0, bound)));
}

TEST(Small_PartitionedConvolution, ResetClearsTheTail)
{
    EXPECT_EQ(output_for_silence_after_reset(), std::vector<float>(4, 0.0f));
}

TEST(Small_PartitionedConvolution, ProcessPerformsNoHeapAllocation)
{
    std::vector<float> response(160, 0.0f);
    for (std::size_t i = 0; i < response.size(); ++i)
    {
        response[i] = 1.0f / (1.0f + static_cast<float>(i));
    }
    SushiDSP::PartitionedConvolution convolution(K_PARTITION, response.data(),
                                                 static_cast<std::uint32_t>(response.size()));
    const std::vector<float> input = noise(K_PARTITION, 7u);
    std::vector<float> output(K_PARTITION, 0.0f);

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    convolution.process(input.data(), output.data());
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}
