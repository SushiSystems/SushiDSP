/**************************************************************************/
/* test_thd.cpp                                                           */
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
 * @file test_thd.cpp
 * @brief Tests the total harmonic distortion of a pure tone and of a cubic, and the shape of its harmonic table.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>
#include <SushiDSP/foundation/signal_measure/spectrum.hpp>
#include <SushiDSP/foundation/signal_measure/thd.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "support/tolerance.hpp"

namespace
{
    /// @brief y = x - x^3/3: third harmonic (A^3/12) / (A - A^3/4) of the fundamental.
    class CubicNode : public SushiDSP::AudioNode
    {
        public:
            /// @brief Holds no state, so there is nothing to prepare.
            void prepare(const SushiDSP::PrepareInfo&) override
            {
            }

            /// @brief Applies the transfer to every frame of the block.
            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t i = 0; i < context.frames; ++i)
                {
                    const float x = context.inputs[0][i];
                    context.outputs[0][i] = x - x * x * x / 3.0f;
                }
            }

            /// @brief Reports one input channel and one output channel.
            SushiDSP::PortLayout ports() const override
            {
                return {1, 1};
            }

            /// @brief Exposes no parameter, so every read is zero.
            float get_parameter(const char*) const override
            {
                return 0.0f;
            }

            /// @brief Exposes no parameter, so every write is ignored.
            void set_parameter_for_control_thread(const char*, float) override
            {
            }

            /// @brief Holds no state, so there is nothing to clear.
            void reset() override
            {
            }
    };

    /// @brief The rate of every signal here.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief The leakage of a bin-exact tone into any bin beyond the guard, relative to the tone:
    /// the -110 dB floor of tests/foundation/signal_measure/test_spectrum.cpp as a ratio.
    const double K_LEAKAGE_RATIO = std::pow(10.0, -110.0 / 20.0);

    /// @brief The third harmonic of y = x - x^3/3 at amplitude 0.5, relative to the fundamental:
    /// (A^2 / 12) / (1 - A^2 / 4) = 1 / 45.
    constexpr double K_CUBIC_HARMONIC_RATIO = 1.0 / 45.0;

    /// @brief Rounded operations on the cubic's longest path: x * x, times x, over 3, the subtraction
    /// and the conversion of each of the two bins' sums.
    constexpr std::size_t K_CUBIC_OPERATIONS = 8;

    /// @brief The distortion, in percent, under which an off-bin pure tone stays: -80 dB.
    constexpr double K_OFF_BIN_PERCENT = 0.01;

    /// @brief The distortion, in percent, under which a bin-exact pure tone stays: -100 dB.
    constexpr double K_EXACT_BIN_PERCENT = 1e-3;

    /// @brief The level, in decibels, under which the second harmonic of a bin-exact tone stays.
    constexpr double K_SECOND_HARMONIC_DB = -100.0;

    /// @brief Returns a tone of one transform at a frequency.
    std::vector<float> tone_at(double frequency_hz)
    {
        return SushiDSP::measure::sine_stimulus(frequency_hz, K_SAMPLE_RATE, SushiDSP::measure::K_FFT_SIZE, 0.5f);
    }

    /// @brief Returns 1000 Hz moved onto the nearest bin.
    double exact_bin_frequency()
    {
        return SushiDSP::measure::bin_exact_frequency(1000.0, K_SAMPLE_RATE, SushiDSP::measure::K_FFT_SIZE);
    }

    /// @brief A pure tone and the distortion it must stay under.
    struct ToneRow
    {
            const char* name;
            bool on_a_bin;
            double bound_percent;
    };

    const ToneRow kToneRows[] = {
        {"OffBin", false, K_OFF_BIN_PERCENT},
        {"ExactBin", true, K_EXACT_BIN_PERCENT},
    };

    std::string tone_name(const ::testing::TestParamInfo<ToneRow>& info)
    {
        return info.param.name;
    }

    class Small_ThdPureTone : public ::testing::TestWithParam<ToneRow>
    {
    };

    /// @brief Returns the distortion of the cubic's response to a bin-exact tone.
    SushiDSP::measure::ThdResult cubic_thd()
    {
        CubicNode node;
        const std::vector<float> output =
            SushiDSP::measure::render(node, tone_at(exact_bin_frequency()), K_SAMPLE_RATE, 256);
        return SushiDSP::measure::thd(output, exact_bin_frequency(), K_SAMPLE_RATE);
    }

    /// @brief The relative error the cubic's harmonic ratio may carry: the leakage of the fundamental
    /// into the harmonic's bin, over the harmonic itself, and the cubic's rounding.
    double cubic_relative_bound()
    {
        return K_LEAKAGE_RATIO / K_CUBIC_HARMONIC_RATIO +
               static_cast<double>(K_CUBIC_OPERATIONS) * TestSupport::unit_roundoff<float>();
    }
} // namespace

TEST_P(Small_ThdPureTone, PureToneHasNegligibleDistortion)
{
    const double frequency = GetParam().on_a_bin ? exact_bin_frequency() : 1000.0;

    const SushiDSP::measure::ThdResult result = SushiDSP::measure::thd(tone_at(frequency), frequency, K_SAMPLE_RATE);

    EXPECT_LT(result.thd_percent, GetParam().bound_percent);
}

INSTANTIATE_TEST_SUITE_P(Tones, Small_ThdPureTone, ::testing::ValuesIn(kToneRows), tone_name);

TEST(Small_Thd, PureSineOnAnExactBinHasTheSecondHarmonicBelowTheFloor)
{
    const SushiDSP::measure::ThdResult result =
        SushiDSP::measure::thd(tone_at(exact_bin_frequency()), exact_bin_frequency(), K_SAMPLE_RATE);

    EXPECT_LT(result.per_harmonic_db.at(1), K_SECOND_HARMONIC_DB);
}

TEST(Small_Thd, TheHarmonicTableHasTwentyEntriesByDefault)
{
    const SushiDSP::measure::ThdResult result =
        SushiDSP::measure::thd(tone_at(exact_bin_frequency()), exact_bin_frequency(), K_SAMPLE_RATE);

    EXPECT_EQ(result.per_harmonic_db.size(), 20u);
}

// A = 0.5: third harmonic 0.010417 / 0.46875 = 2.2222 % of the fundamental.
TEST(Small_Thd, CubicMatchesAnalyticThirdHarmonicDistortion)
{
    const double expected_percent = 100.0 * K_CUBIC_HARMONIC_RATIO;

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        cubic_thd().thd_percent, expected_percent, cubic_relative_bound() * expected_percent)));
}

TEST(Small_Thd, CubicMatchesAnalyticThirdHarmonicLevel)
{
    const double expected_db = 20.0 * std::log10(K_CUBIC_HARMONIC_RATIO);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        cubic_thd().per_harmonic_db.at(2), expected_db, 20.0 * std::log10(1.0 + cubic_relative_bound()))));
}

TEST(Small_Thd, ReportsOneEntryPerRequestedHarmonic)
{
    const SushiDSP::measure::ThdResult result = SushiDSP::measure::thd(tone_at(1000.0), 1000.0, K_SAMPLE_RATE, 5);

    EXPECT_EQ(result.per_harmonic_db.size(), 5u);
}

TEST(Small_Thd, TheFundamentalEntryIsTheFundamentalAgainstItself)
{
    const SushiDSP::measure::ThdResult result = SushiDSP::measure::thd(tone_at(1000.0), 1000.0, K_SAMPLE_RATE, 5);

    EXPECT_EQ(result.per_harmonic_db.at(0), 0.0);
}

TEST(Small_Thd, ShortSignalIsRefusedAsZero)
{
    const std::vector<float> short_signal(100, 0.1f);
    const SushiDSP::measure::ThdResult result = SushiDSP::measure::thd(short_signal, 1000.0, K_SAMPLE_RATE);
    EXPECT_EQ(result.thd_percent, 0.0);
}
