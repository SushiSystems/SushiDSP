/**************************************************************************/
/* test_spectrum.cpp                                                      */
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
 * @file test_spectrum.cpp
 * @brief Tests the bin helpers, the magnitude spectrum of a pure tone and its leakage floor.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/signal_measure/render.hpp>
#include <SushiDSP/foundation/signal_measure/spectrum.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "support/tolerance.hpp"

namespace
{
    /// @brief The rate of every signal here.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief The bin of 1 kHz at this rate: 1000 * 65536 / 48000 = 1365.33, rounded.
    constexpr std::uint32_t K_ONE_KILOHERTZ_BIN = 1365;

    /// @brief Bins on each side of the fundamental that the floor measurement leaves out, where the
    /// main lobe of the window sits.
    constexpr std::uint32_t K_GUARD_BINS = 8;

    /// @brief The level, in decibels below the fundamental, under which a bin-exact tone's leakage
    /// must stay beyond the guard. A characterisation level: the source gives no derivation for it,
    /// and the 4-term Blackman-Harris window's first sidelobe is -92 dB, falling beyond it.
    constexpr double K_LEAKAGE_FLOOR_DB = -110.0;

    /// @brief A frequency and the bin it maps to.
    struct BinRow
    {
            const char* name;
            double frequency_hz;
            std::uint32_t expected_bin;
    };

    const BinRow kBinRows[] = {
        {"OneKilohertz", 1000.0, K_ONE_KILOHERTZ_BIN},
        {"Dc", 0.0, 0u},
    };

    std::string bin_name(const ::testing::TestParamInfo<BinRow>& info)
    {
        return info.param.name;
    }

    class Small_SpectrumBinOf : public ::testing::TestWithParam<BinRow>
    {
    };

    /// @brief Returns the spectrum of a bin-exact 1 kHz tone of one transform.
    std::vector<double> bin_exact_tone_spectrum()
    {
        const double f0 = SushiDSP::measure::bin_exact_frequency(1000.0, K_SAMPLE_RATE, SushiDSP::measure::K_FFT_SIZE);
        const std::vector<float> signal =
            SushiDSP::measure::sine_stimulus(f0, K_SAMPLE_RATE, SushiDSP::measure::K_FFT_SIZE, 0.5f);
        return SushiDSP::measure::magnitude_spectrum_db(signal, 0);
    }

    /// @brief Returns the index of the strongest bin, DC excluded.
    std::size_t strongest_bin(const std::vector<double>& spectrum)
    {
        std::size_t peak = 1;
        for (std::size_t i = 2; i < spectrum.size(); ++i)
        {
            if (spectrum[i] > spectrum[peak])
            {
                peak = i;
            }
        }
        return peak;
    }

    /// @brief Returns the highest level beyond the guard bins, relative to the fundamental's bin.
    /// @param spectrum A spectrum in decibels.
    /// @param fundamental_bin The bin of the tone.
    double worst_leakage_db(const std::vector<double>& spectrum, std::uint32_t fundamental_bin)
    {
        double worst = -300.0;
        for (std::uint32_t bin = K_GUARD_BINS; bin < spectrum.size(); ++bin)
        {
            const bool in_guard = bin + K_GUARD_BINS > fundamental_bin && bin < fundamental_bin + K_GUARD_BINS;
            if (!in_guard)
            {
                worst = std::max(worst, spectrum[bin] - spectrum[fundamental_bin]);
            }
        }
        return worst;
    }
} // namespace

TEST_P(Small_SpectrumBinOf, BinOfMapsAFrequencyToItsFftBin)
{
    EXPECT_EQ(SushiDSP::measure::bin_of(GetParam().frequency_hz, K_SAMPLE_RATE), GetParam().expected_bin);
}

INSTANTIATE_TEST_SUITE_P(Frequencies, Small_SpectrumBinOf, ::testing::ValuesIn(kBinRows), bin_name);

TEST(Small_Spectrum, BinExactFrequencyIsWithinHalfABinOfTheRequest)
{
    const double bin_width = K_SAMPLE_RATE / static_cast<double>(SushiDSP::measure::K_FFT_SIZE);
    const double exact = SushiDSP::measure::bin_exact_frequency(1000.0, K_SAMPLE_RATE, SushiDSP::measure::K_FFT_SIZE);

    // The rounding of the bin index multiplies and divides in double: four rounded operations on 1000 Hz.
    const double slack = 4.0 * TestSupport::unit_roundoff<double>() * 1000.0;
    EXPECT_LE(std::fabs(exact - 1000.0), 0.5 * bin_width + slack);
}

TEST(Small_Spectrum, PureToneSpectrumHasHalfAFrameOfBins)
{
    EXPECT_EQ(bin_exact_tone_spectrum().size(), SushiDSP::measure::K_FFT_SIZE / 2 + 1);
}

TEST(Small_Spectrum, PureToneSpectrumPeaksAtItsBin)
{
    // The tone sits on a bin, so the window's main lobe peaks on that bin and not beside it.
    EXPECT_EQ(strongest_bin(bin_exact_tone_spectrum()), K_ONE_KILOHERTZ_BIN);
}

TEST(Small_Spectrum, FloorOfPureSineIsBelowTheLeakageFloor)
{
    EXPECT_LE(worst_leakage_db(bin_exact_tone_spectrum(), K_ONE_KILOHERTZ_BIN), K_LEAKAGE_FLOOR_DB);
}
