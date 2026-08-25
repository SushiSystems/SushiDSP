// FftTest.cpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/math/Fft.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(UnitDsp, FftOfDcSignalHasEnergyOnlyInBinZero)
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

TEST(UnitDsp, FftOfSingleBinSineMatchesKnownBin)
{
    constexpr std::uint32_t size       = 64;
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
    const float magnitude_at_target =
        std::sqrt(real_out[target_bin] * real_out[target_bin] + imag_out[target_bin] * imag_out[target_bin]);
    EXPECT_NEAR(magnitude_at_target, expected_magnitude, 1e-2f);
}

TEST(UnitDsp, ForwardThenInverseFftIsIdentity)
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
