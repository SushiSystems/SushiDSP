/**************************************************************************/
/* test_graphic_equalizer.cpp                                             */
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
 * @file test_graphic_equalizer.cpp
 * @brief Tests the bands of GraphicEqualizer at rest, boosted and cut.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/circuits/graphic_equalizer.hpp>

namespace
{
    constexpr double K_PI = 3.14159265358979323846;
    constexpr double K_SAMPLE_RATE = 48000.0;
    constexpr std::size_t K_BANDS = 5;

    const double kU = TestSupport::unit_roundoff<float>();

    /// @brief The gain error in decibels of the measurement: the input and the output are floats and the filter state is double, so the rms carries 8 u of relative error, and 20 / ln 10 turns it into decibels. One second of settle leaves e^(-1 s / tau) of the start, with tau = Q / (pi f) at most 6 ms at the lowest band, which is below 1e-70.
    const double kMeasurementDb = 8.686 * 8.0 * kU;

    /// @brief Holds the shape of one equalizer: the centres, the Q of each band and the gain at slider 1.
    struct Bank
    {
            std::array<double, K_BANDS> centres_hz;
            std::array<double, K_BANDS> qs;
            double maximum_gain_db;
    };

    const Bank kUniformBank{{80.0, 240.0, 750.0, 2200.0, 6600.0}, {1.4, 1.4, 1.4, 1.4, 1.4}, 12.0};
    const Bank kPerBandQBank{{87.61, 371.74, 723.43, 1575.87, 4822.88}, {1.171, 1.938, 2.128, 0.673, 1.0}, 17.0};

    SushiDSP::GraphicEqualizer make_uniform_equalizer()
    {
        return SushiDSP::GraphicEqualizer(K_SAMPLE_RATE, {80.0f, 240.0f, 750.0f, 2200.0f, 6600.0f}, 1.4f, 12.0f);
    }

    SushiDSP::GraphicEqualizer make_per_band_q_equalizer()
    {
        return SushiDSP::GraphicEqualizer(K_SAMPLE_RATE, {87.61f, 371.74f, 723.43f, 1575.87f, 4822.88f},
                                          {1.171f, 1.938f, 2.128f, 0.673f, 1.0f}, 17.0f);
    }

    /// @brief Drives a sine tone through the equalizer and returns its steady-state gain in dB.
    double steady_gain_db(SushiDSP::GraphicEqualizer& equalizer, double frequency_hz)
    {
        const double angular_step = 2.0 * K_PI * frequency_hz / K_SAMPLE_RATE;

        for (int i = 0; i < 48000; ++i)
        {
            const float sample = static_cast<float>(std::sin(angular_step * i));
            equalizer.process_sample(sample);
        }

        double input_sum_squares = 0.0;
        double output_sum_squares = 0.0;
        for (int i = 48000; i < 96000; ++i)
        {
            const float input_sample = static_cast<float>(std::sin(angular_step * i));
            const float output_sample = equalizer.process_sample(input_sample);
            input_sum_squares += static_cast<double>(input_sample) * input_sample;
            output_sum_squares += static_cast<double>(output_sample) * output_sample;
        }

        return 20.0 * std::log10(std::sqrt(output_sum_squares / 48000.0) / std::sqrt(input_sum_squares / 48000.0));
    }

    /// @brief The magnitude in decibels of one peaking band at one frequency, from the Audio EQ Cookbook's peaking transfer function evaluated on the unit circle.
    double peaking_gain_db(double centre_hz, double gain_db, double q, double frequency_hz)
    {
        const double a = std::pow(10.0, gain_db / 40.0);
        const double centre_omega = 2.0 * K_PI * centre_hz / K_SAMPLE_RATE;
        const double alpha = std::sin(centre_omega) / (2.0 * q);
        const double b1 = -2.0 * std::cos(centre_omega);
        const double omega = 2.0 * K_PI * frequency_hz / K_SAMPLE_RATE;
        const std::complex<double> z1(std::cos(omega), -std::sin(omega));
        const std::complex<double> z2 = z1 * z1;
        const std::complex<double> numerator = (1.0 + alpha * a) + b1 * z1 + (1.0 - alpha * a) * z2;
        const std::complex<double> denominator = (1.0 + alpha / a) + b1 * z1 + (1.0 - alpha / a) * z2;
        return 20.0 * std::log10(std::abs(numerator) / std::abs(denominator));
    }

    /// @brief The bank's gain at a frequency with one slider moved: the bands are in series, so their decibels add.
    /// @param moved_band The band whose slider is at @p position; a band the bank does not have moves nothing.
    /// @param position The slider, clamped to [-1, 1] as the interface promises.
    double cascade_gain_db(const Bank& bank, std::size_t moved_band, double position, double frequency_hz)
    {
        double total = 0.0;
        for (std::size_t band = 0; band < K_BANDS; ++band)
        {
            const double slider = band == moved_band ? std::clamp(position, -1.0, 1.0) : 0.0;
            total += peaking_gain_db(bank.centres_hz[band], slider * bank.maximum_gain_db, bank.qs[band], frequency_hz);
        }
        return total;
    }

    /// @brief Holds one slider move and the frequency the bank's gain is read at.
    struct SliderRow
    {
            const char* name;
            std::size_t band;
            float position;
            double probe_hz;
    };

    const SliderRow kSliderRows[] = {
        {"FlatAtRest", 2, 0.0f, 750.0},
        {"BoostAtItsCentre", 2, 1.0f, 750.0},
        {"BoostLeavesADistantBand", 2, 1.0f, 80.0},
        {"CutAtItsCentre", 0, -1.0f, 80.0},
        {"SliderPastFullCutClampsToFullCut", 0, -3.0f, 80.0},
        {"SliderPastFullBoostClampsToFullBoost", 4, 3.0f, 6600.0},
        {"SliderOfAMissingBandIsIgnored", 7, 1.0f, 750.0},
    };

    /// @brief Holds a probe frequency for the per-band Q rows.
    struct ProbeRow
    {
            const char* name;
            double probe_hz;
    };

    const ProbeRow kProbeRows[] = {
        {"AtTheBoostedCentre", 371.74},
        {"AnOctaveAbove", 743.48},
        {"AtTheLowerNeighbour", 87.61},
        {"AtTheUpperNeighbour", 723.43},
    };

    /// @brief Runs 512 samples of a ramp through @p unit, the way an audio block drives it.
    template <typename Unit> void process_ramp_block(Unit& unit)
    {
        for (int sample = 0; sample < 512; ++sample)
        {
            static_cast<void>(unit.process_sample(static_cast<float>(sample % 64) / 64.0f - 0.5f));
        }
    }
} // namespace

TEST(Small_GraphicEqualizer, GraphicEqualizerHoldsTheFiveBandsItIsGiven)
{
    SushiDSP::GraphicEqualizer equalizer = make_uniform_equalizer();

    EXPECT_EQ(equalizer.band_count(), K_BANDS);
}

class Small_GraphicEqualizerSliders : public ::testing::TestWithParam<SliderRow>
{
};

TEST_P(Small_GraphicEqualizerSliders, GraphicEqualizerGainIsTheCookbookCascadeAtTheSliderPositions)
{
    const SliderRow& row = GetParam();
    SushiDSP::GraphicEqualizer equalizer = make_uniform_equalizer();

    equalizer.set_band(row.band, row.position);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        steady_gain_db(equalizer, row.probe_hz), cascade_gain_db(kUniformBank, row.band, row.position, row.probe_hz),
        kMeasurementDb)));
}

INSTANTIATE_TEST_SUITE_P(Moves, Small_GraphicEqualizerSliders, ::testing::ValuesIn(kSliderRows),
                         [](const ::testing::TestParamInfo<SliderRow>& info) { return std::string(info.param.name); });

TEST(Small_GraphicEqualizer, GraphicEqualizerIgnoresASliderOfABandItDoesNotHave)
{
    SushiDSP::GraphicEqualizer equalizer = make_uniform_equalizer();

    equalizer.set_band(7, 1.0f);

    EXPECT_EQ(equalizer.band_count(), K_BANDS);
}

class Small_GraphicEqualizerBandQ : public ::testing::TestWithParam<ProbeRow>
{
};

TEST_P(Small_GraphicEqualizerBandQ, GraphicEqualizerPerBandQShapesOneBandAndLeavesItsNeighboursAlone)
{
    const ProbeRow& row = GetParam();
    SushiDSP::GraphicEqualizer equalizer = make_per_band_q_equalizer();

    equalizer.set_band(1, 1.0f);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        steady_gain_db(equalizer, row.probe_hz), cascade_gain_db(kPerBandQBank, 1, 1.0, row.probe_hz), kMeasurementDb)));
}

INSTANTIATE_TEST_SUITE_P(Probes, Small_GraphicEqualizerBandQ, ::testing::ValuesIn(kProbeRows),
                         [](const ::testing::TestParamInfo<ProbeRow>& info) { return std::string(info.param.name); });

TEST(Medium_GraphicEqualizer, ProcessSamplePerformsNoHeapAllocation)
{
    SushiDSP::GraphicEqualizer equalizer = make_uniform_equalizer();
    equalizer.set_band(2, 0.5f);
    process_ramp_block(equalizer);
    SushiDSP::test::AllocationTrap trap;

    trap.arm();
    process_ramp_block(equalizer);
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}
