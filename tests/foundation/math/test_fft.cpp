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
 * @file test_fft.cpp
 * @brief Tests Fft against known bins, a host DFT and its own inverse.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "foundation/math/fft_error_bound.hpp"
#include "support/tolerance.hpp"

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/math/fft.hpp>

namespace
{
    const double kU = TestSupport::unit_roundoff<float>();

    /// Pi to double precision, spelled out so the tests do not depend on M_PI being visible.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Returns the relative 2-norm bound of a float radix-2 FFT of the given size.
    double fft_relative_bound(std::uint32_t size)
    {
        return TestSupport::fft_relative_bound(size);
    }

    /// @brief Returns count deterministic samples in [-0.5, 0.5) from a linear congruential generator.
    std::vector<float> noise_input(std::size_t count, std::uint32_t seed)
    {
        std::vector<float> samples(count);
        std::uint32_t state = seed;
        for (float& sample : samples)
        {
            state = state * 1103515245u + 12345u;
            sample = static_cast<float>(state >> 16) / 65536.0f - 0.5f;
        }
        return samples;
    }

    /// @brief Returns the 2-norm of a real vector.
    double norm_of(const std::vector<double>& values)
    {
        double sum = 0.0;
        for (const double value : values)
        {
            sum += value * value;
        }
        return std::sqrt(sum);
    }

    /// The spectrum of a transform, as the two float arrays it fills.
    struct Spectrum
    {
            std::vector<float> real;
            std::vector<float> imag;
    };

    /// @brief Runs the forward transform of the input.
    Spectrum forward_of(const SushiDSP::Fft& fft, const std::vector<float>& input)
    {
        Spectrum spectrum{std::vector<float>(input.size()), std::vector<float>(input.size())};
        fft.forward(input.data(), spectrum.real.data(), spectrum.imag.data());
        return spectrum;
    }

    /// @brief Returns the 2-norm of the difference between a spectrum and a double-precision one.
    double spectrum_error(const Spectrum& computed, const std::vector<std::complex<double>>& reference)
    {
        std::vector<double> differences;
        for (std::size_t k = 0; k < reference.size(); ++k)
        {
            differences.push_back(computed.real[k] - reference[k].real());
            differences.push_back(computed.imag[k] - reference[k].imag());
        }
        return norm_of(differences);
    }

    /// @brief Returns the 2-norm of a complex spectrum.
    double spectrum_norm(const std::vector<std::complex<double>>& reference)
    {
        std::vector<double> parts;
        for (const std::complex<double>& bin : reference)
        {
            parts.push_back(bin.real());
            parts.push_back(bin.imag());
        }
        return norm_of(parts);
    }

    /// @brief Returns the DFT of a real input by the definition, in double precision.
    std::vector<std::complex<double>> host_dft(const std::vector<float>& input)
    {
        const std::size_t size = input.size();
        std::vector<std::complex<double>> bins(size);
        for (std::size_t k = 0; k < size; ++k)
        {
            for (std::size_t n = 0; n < size; ++n)
            {
                const double theta = -2.0 * K_PI * static_cast<double>((k * n) % size) / static_cast<double>(size);
                bins[k] += static_cast<double>(input[n]) * std::complex<double>(std::cos(theta), std::sin(theta));
            }
        }
        return bins;
    }

    /// @brief Returns a cosine of the given bin over size samples, rounded to float.
    std::vector<float> cosine_input(std::uint32_t size, std::uint32_t bin)
    {
        std::vector<float> samples(size);
        for (std::uint32_t n = 0; n < size; ++n)
        {
            samples[n] = static_cast<float>(
                std::cos(2.0 * K_PI * static_cast<double>((bin * n) % size) / static_cast<double>(size)));
        }
        return samples;
    }

    /// @brief Returns the 2-norm of the difference between a reconstruction and its input.
    double reconstruction_error(const std::vector<float>& reconstructed, const std::vector<float>& input)
    {
        std::vector<double> differences;
        for (std::size_t n = 0; n < input.size(); ++n)
        {
            differences.push_back(static_cast<double>(reconstructed[n]) - input[n]);
        }
        return norm_of(differences);
    }

    /// @brief Returns the 2-norm of a float vector.
    double input_norm_of(const std::vector<float>& input)
    {
        return norm_of(std::vector<double>(input.begin(), input.end()));
    }

    std::string size_name(const ::testing::TestParamInfo<std::uint32_t>& info)
    {
        return "Size" + std::to_string(info.param);
    }

    class Small_FftSize : public ::testing::TestWithParam<std::uint32_t>
    {
    };
} // namespace

TEST_P(Small_FftSize, ReportsTheSizeItWasBuiltWith)
{
    EXPECT_EQ(SushiDSP::Fft(GetParam()).size(), GetParam());
}

TEST_P(Small_FftSize, ForwardMatchesTheHostDft)
{
    const SushiDSP::Fft fft(GetParam());
    const std::vector<float> input = noise_input(GetParam(), GetParam());
    const std::vector<std::complex<double>> expected = host_dft(input);

    const Spectrum spectrum = forward_of(fft, input);

    // The host DFT is the exact transform of the float input, so only the FFT's own error remains.
    const double bound = fft_relative_bound(GetParam()) * spectrum_norm(expected);
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(spectrum_error(spectrum, expected), 0.0, bound)));
}

// Each pass adds log2(N) eta of the norm; the scale adds u. See tests/foundation/math/README.md.
TEST_P(Small_FftSize, InverseReconstructsTheInput)
{
    const SushiDSP::Fft fft(GetParam());
    const std::vector<float> input = noise_input(GetParam(), GetParam());
    const Spectrum spectrum = forward_of(fft, input);
    std::vector<float> reconstructed(GetParam());

    fft.inverse(spectrum.real.data(), spectrum.imag.data(), reconstructed.data());

    const double bound = (2.0 * fft_relative_bound(GetParam()) + kU) * input_norm_of(input);
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(reconstruction_error(reconstructed, input), 0.0, bound)));
}

INSTANTIATE_TEST_SUITE_P(Sizes, Small_FftSize, ::testing::Values(1u, 2u, 8u, 256u), size_name);

TEST(Small_Fft, DcSignalHasEnergyOnlyInBinZero)
{
    constexpr std::uint32_t size = 64;
    const SushiDSP::Fft fft(size);
    std::vector<std::complex<double>> expected(size);
    expected[0] = static_cast<double>(size);

    const Spectrum spectrum = forward_of(fft, std::vector<float>(size, 1.0f));

    const double bound = fft_relative_bound(size) * spectrum_norm(expected);
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(spectrum_error(spectrum, expected), 0.0, bound)));
}

// The cosine puts N/2 on its bin and on the mirrored one. See tests/foundation/math/README.md.
TEST(Small_Fft, SingleBinCosineHasHalfNAtItsBinAndZeroElsewhere)
{
    constexpr std::uint32_t size = 64;
    constexpr std::uint32_t target_bin = 4;
    const SushiDSP::Fft fft(size);
    std::vector<std::complex<double>> expected(size);
    expected[target_bin] = size / 2.0;
    expected[size - target_bin] = size / 2.0;
    const std::vector<float> input = cosine_input(size, target_bin);

    const Spectrum spectrum = forward_of(fft, input);

    // Rounding the cosine to float adds sqrt(N) u ||x|| to the spectrum.
    const double bound =
        fft_relative_bound(size) * spectrum_norm(expected) + std::sqrt(size) * kU * input_norm_of(input);
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(spectrum_error(spectrum, expected), 0.0, bound)));
}

TEST(Small_Fft, ForwardAndInverseRoundTripPerformsNoHeapAllocation)
{
    constexpr std::uint32_t size = 256;
    const SushiDSP::Fft fft(size);
    const std::vector<float> input = noise_input(size, 1u);
    std::vector<float> real_bins(size);
    std::vector<float> imag_bins(size);
    std::vector<float> reconstructed(size);

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    fft.forward(input.data(), real_bins.data(), imag_bins.data());
    fft.inverse(real_bins.data(), imag_bins.data(), reconstructed.data());
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}
