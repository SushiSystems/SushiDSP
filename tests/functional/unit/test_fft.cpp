/**************************************************************************/
/* test_fft.cpp                                                           */
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
 * @file test_fft.cpp
 * @brief Tests Fft against known bins, a brute-force DFT and its own inverse.
 * @author Mustafa Garip
 */

#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdlib>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/math/fft.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(Unit_Dsp, FftOfDcSignalHasEnergyOnlyInBinZero)
{
    constexpr std::uint32_t size = 64;
    SushiDSP::Fft fft(size);

    std::vector<float> time_domain(size, 1.0f);
    std::vector<float> real_out(size, 0.0f);
    std::vector<float> imag_out(size, 0.0f);

    fft.forward(time_domain.data(), real_out.data(), imag_out.data());

    EXPECT_NEAR(real_out[0], static_cast<float>(size), 1e-3f);
    for (std::uint32_t bin = 1; bin < size; ++bin)
    {
        EXPECT_NEAR(real_out[bin], 0.0f, 1e-3f);
        EXPECT_NEAR(imag_out[bin], 0.0f, 1e-3f);
    }
}

TEST(Unit_Dsp, FftOfSingleBinSineMatchesKnownBin)
{
    constexpr std::uint32_t size = 64;
    constexpr std::uint32_t target_bin = 4;
    SushiDSP::Fft fft(size);

    std::vector<float> time_domain(size);
    for (std::uint32_t n = 0; n < size; ++n)
    {
        time_domain[n] = std::cos(2.0f * static_cast<float>(M_PI) * target_bin * n / size);
    }
    std::vector<float> real_out(size, 0.0f);
    std::vector<float> imag_out(size, 0.0f);

    fft.forward(time_domain.data(), real_out.data(), imag_out.data());

    const float expected_magnitude = static_cast<float>(size) / 2.0f;
    const float magnitude_at_target = std::sqrt(real_out[target_bin] * real_out[target_bin] +
                                                imag_out[target_bin] * imag_out[target_bin]);
    EXPECT_NEAR(magnitude_at_target, expected_magnitude, 1e-2f);
}

TEST(Unit_Dsp, FftMatchesBruteForceDftAcrossSizes)
{
    for (std::uint32_t transform_size : {1u, 2u, 8u, 256u})
    {
        SushiDSP::Fft fft(transform_size);
        EXPECT_EQ(fft.size(), transform_size);

        std::vector<float> input(transform_size);
        std::vector<float> real_out(transform_size);
        std::vector<float> imag_out(transform_size);
        std::vector<float> reconstructed(transform_size);
        std::srand(transform_size);
        for (auto& sample : input)
        {
            sample = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) - 0.5f;
        }

        fft.forward(input.data(), real_out.data(), imag_out.data());
        fft.inverse(real_out.data(), imag_out.data(), reconstructed.data());

        for (std::uint32_t n = 0; n < transform_size; ++n)
        {
            EXPECT_NEAR(reconstructed[n], input[n], 1e-4f) << "size=" << transform_size;
        }

        for (std::uint32_t k = 0; k < transform_size; ++k)
        {
            double expected_real = 0.0;
            double expected_imag = 0.0;
            for (std::uint32_t n = 0; n < transform_size; ++n)
            {
                const double theta = -2.0 * M_PI * static_cast<double>(k) * static_cast<double>(n) /
                                     static_cast<double>(transform_size);
                expected_real += input[n] * std::cos(theta);
                expected_imag += input[n] * std::sin(theta);
            }
            EXPECT_NEAR(real_out[k], static_cast<float>(expected_real), 1e-3f)
                << "size=" << transform_size << " k=" << k;
            EXPECT_NEAR(imag_out[k], static_cast<float>(expected_imag), 1e-3f)
                << "size=" << transform_size << " k=" << k;
        }
    }
}

TEST(Unit_Dsp, ForwardThenInverseFftIsIdentity)
{
    constexpr std::uint32_t size = 64;
    SushiDSP::Fft fft(size);

    std::vector<float> original(size);
    for (std::uint32_t n = 0; n < size; ++n)
    {
        original[n] = std::sin(2.0f * static_cast<float>(M_PI) * 3.0f * n / size) + 0.5f;
    }

    std::vector<float> real_out(size, 0.0f);
    std::vector<float> imag_out(size, 0.0f);
    std::vector<float> reconstructed(size, 0.0f);

    fft.forward(original.data(), real_out.data(), imag_out.data());
    fft.inverse(real_out.data(), imag_out.data(), reconstructed.data());

    for (std::uint32_t n = 0; n < size; ++n)
    {
        EXPECT_NEAR(reconstructed[n], original[n], 1e-3f);
    }
}
