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
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
/**************************************************************************/

/**
 * @file test_partitioned_convolution.cpp
 * @brief Tests PartitionedConvolution against direct convolution.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/math/partitioned_convolution.hpp>

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
} // namespace

TEST(Small_PartitionedConvolution, PartitionedConvolutionOfImpulseReproducesImpulseResponse)
{
    constexpr std::uint32_t partition_size = 64;
    std::vector<float> impulse_response(100);
    for (std::size_t i = 0; i < impulse_response.size(); ++i)
    {
        impulse_response[i] = std::sin(0.1f * static_cast<float>(i)) * 0.5f;
    }

    SushiDSP::PartitionedConvolution convolution(
        partition_size, impulse_response.data(),
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

TEST(Small_PartitionedConvolution, PartitionedConvolutionMatchesDirectConvolutionForWhiteNoise)
{
    constexpr std::uint32_t partition_size = 64;
    constexpr std::uint32_t impulse_length = 150; // spans 3 partitions, last one padded
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

    SushiDSP::PartitionedConvolution convolution(partition_size, impulse_response.data(),
                                                 impulse_length);
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

TEST(Small_PartitionedConvolution, PartitionedConvolutionResetClearsTail)
{
    const float ir[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    SushiDSP::PartitionedConvolution convolution(4, ir, 4);
    float block[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float out[4];
    convolution.process(block, out);
    convolution.reset();
    float silence[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    convolution.process(silence, out);
    for (float sample : out)
    {
        EXPECT_NEAR(sample, 0.0f, 1e-7f);
    }
}

TEST(Small_PartitionedConvolution, ProcessPerformsNoHeapAllocation)
{
    const std::uint32_t partition_size = 64;

    std::vector<float> impulse_response(160, 0.0f);
    for (std::size_t i = 0; i < impulse_response.size(); ++i)
    {
        impulse_response[i] = 1.0f / (1.0f + static_cast<float>(i));
    }

    SushiDSP::PartitionedConvolution convolution(
        partition_size, impulse_response.data(),
        static_cast<std::uint32_t>(impulse_response.size()));

    std::vector<float> input(partition_size, 0.0f);
    std::vector<float> output(partition_size, 0.0f);
    for (std::uint32_t i = 0; i < partition_size; ++i)
    {
        input[i] = static_cast<float>(i % 5) * 0.1f - 0.2f;
    }

    SushiDSP::test::AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 1000; ++block)
    {
        convolution.process(input.data(), output.data());
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}
