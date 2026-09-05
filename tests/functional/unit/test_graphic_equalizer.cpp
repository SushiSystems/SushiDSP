/**************************************************************************/
/* test_graphic_equalizer.cpp                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/GraphicEqualizer.hpp"

namespace
{
    /// @brief Drives a sine tone through the equalizer and returns its steady-state gain in dB.
    float steady_gain_db(SushiDSP::GraphicEqualizer& equalizer, double sample_rate, double frequency_hz)
    {
        constexpr double kPi = 3.14159265358979323846;
        const double angular_step = 2.0 * kPi * frequency_hz / sample_rate;

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

        const double input_rms = std::sqrt(input_sum_squares / 48000.0);
        const double output_rms = std::sqrt(output_sum_squares / 48000.0);
        return static_cast<float>(20.0 * std::log10(output_rms / input_rms));
    }
}

TEST(Unit_Dsp, GraphicEqualizerHoldsFiveBandsAndIsFlatAtRest)
{
    SushiDSP::GraphicEqualizer equalizer(48000.0, { 80.0f, 240.0f, 750.0f, 2200.0f, 6600.0f }, 1.4f, 12.0f);
    EXPECT_EQ(equalizer.band_count(), 5u);
    EXPECT_NEAR(steady_gain_db(equalizer, 48000.0, 750.0), 0.0f, 0.05f);
}

TEST(Unit_Dsp, GraphicEqualizerBoostsOneBandAtItsCentre)
{
    SushiDSP::GraphicEqualizer equalizer(48000.0, { 80.0f, 240.0f, 750.0f, 2200.0f, 6600.0f }, 1.4f, 12.0f);
    equalizer.set_band(2, 1.0f);
    EXPECT_NEAR(steady_gain_db(equalizer, 48000.0, 750.0), 12.0f, 0.3f);
    EXPECT_NEAR(steady_gain_db(equalizer, 48000.0, 80.0), 0.0f, 0.5f);
}

TEST(Unit_Dsp, GraphicEqualizerCutsOneBandAndClampsSliders)
{
    SushiDSP::GraphicEqualizer equalizer(48000.0, { 80.0f, 240.0f, 750.0f, 2200.0f, 6600.0f }, 1.4f, 12.0f);
    equalizer.set_band(0, -3.0f);
    EXPECT_NEAR(steady_gain_db(equalizer, 48000.0, 80.0), -12.0f, 0.3f);
    equalizer.set_band(7, 1.0f);
    EXPECT_EQ(equalizer.band_count(), 5u);
}
