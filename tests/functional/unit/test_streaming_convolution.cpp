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
 * @file test_streaming_convolution.cpp
 * @brief Tests StreamingConvolution against the direct form.
 * @author Mustafa Garip
 */

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
