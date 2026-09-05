/**************************************************************************/
/* test_resonance_control.cpp                                            */
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

#include "SushiDSP/circuits/stages/ResonanceControl.hpp"

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    float steady_gain_db(SushiDSP::ResonanceControl& control, double sample_rate, double frequency_hz)
    {
        const int settle = 48000, measure = 48000;
        double sum_squares_out = 0.0, sum_squares_in = 0.0;
        for (int i = 0; i < settle + measure; ++i)
        {
            const float x = static_cast<float>(std::sin(2.0 * kPi * frequency_hz * i / sample_rate));
            const float y = control.process_sample(x);
            if (i >= settle) { sum_squares_out += y * y; sum_squares_in += x * x; }
        }
        return static_cast<float>(10.0 * std::log10(sum_squares_out / sum_squares_in));
    }
}

TEST(Unit_Dsp, ResonanceControlAtZeroDepthIsUnity)
{
    SushiDSP::ResonanceControl control(48000.0, 120.0f, 9.0f);
    control.set_depth(0.0f);
    EXPECT_NEAR(steady_gain_db(control, 48000.0, 40.0), 0.0f, 0.05f);
    EXPECT_NEAR(steady_gain_db(control, 48000.0, 4000.0), 0.0f, 0.05f);
}

TEST(Unit_Dsp, ResonanceControlAtFullDepthBoostsLowsByMaximumAndLeavesHighsAlone)
{
    SushiDSP::ResonanceControl control(48000.0, 120.0f, 9.0f);
    control.set_depth(1.0f);
    EXPECT_NEAR(steady_gain_db(control, 48000.0, 20.0), 9.0f, 0.5f);
    EXPECT_NEAR(steady_gain_db(control, 48000.0, 4000.0), 0.0f, 0.1f);
}

TEST(Unit_Dsp, ResonanceControlClampsDepthAndReportsBoost)
{
    SushiDSP::ResonanceControl control(48000.0, 120.0f, 9.0f);
    control.set_depth(2.0f);
    EXPECT_FLOAT_EQ(control.boost_db(), 9.0f);
    control.set_depth(-1.0f);
    EXPECT_FLOAT_EQ(control.boost_db(), 0.0f);
    control.set_depth(0.5f);
    EXPECT_FLOAT_EQ(control.boost_db(), 4.5f);
}
