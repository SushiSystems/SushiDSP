/**************************************************************************/
/* test_fmv_tone_stack.cpp                                                */
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

/**
 * @file test_fmv_tone_stack.cpp
 * @brief Nulls FmvToneStack against the jcm_fmv AC sweep and checks its knob extremes.
 * @author Mustafa Garip & Sushi Systems
 */

#include <cmath>
#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/FmvToneStack.hpp"
#include "measure/SpiceCsv.hpp"

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    float steady_gain_db(SushiDSP::FmvToneStack& stack, double sample_rate, double frequency_hz)
    {
        const int settle = 48000, measure = 48000;
        double sum_squares_out = 0.0, sum_squares_in = 0.0;
        for (int i = 0; i < settle + measure; ++i)
        {
            const float x = static_cast<float>(std::sin(2.0 * kPi * frequency_hz * i / sample_rate));
            const float y = stack.process_sample(x);
            if (i >= settle) { sum_squares_out += y * y; sum_squares_in += x * x; }
        }
        return static_cast<float>(10.0 * std::log10(sum_squares_out / sum_squares_in));
    }

    std::string spice_path(const std::string& block, const std::string& kind)
    {
        return std::string(SUSHIDSP_SPICE_DIR) + "/" + block + "/" + block + "_" + kind + ".csv";
    }
}

TEST(Unit_Dsp, FmvToneStackMatchesSpiceMidSweepWithinHalfDecibel)
{
    const auto sweep = SushiDSP::measure::load_ac_csv(spice_path("jcm_fmv", "ac"));
    const float mid_positions[3] = { 0.1f, 0.5f, 0.9f };
    const double sample_rate = 192000.0;
    for (int setting = 0; setting < 3; ++setting)
    {
        SushiDSP::FmvToneStack stack(sample_rate, SushiDSP::kJcm2203ToneStack);
        stack.set_bass(0.5f);
        stack.set_treble(0.5f);
        stack.set_middle(mid_positions[setting]);
        for (std::size_t row = 0; row < sweep.frequency_hz.size(); ++row)
        {
            if (sweep.setting[row] != setting) continue;
            const double f = sweep.frequency_hz[row];
            if (f < 20.0 || f > 20000.0) continue;
            stack.reset();
            EXPECT_NEAR(steady_gain_db(stack, sample_rate, f), sweep.magnitude_db[row], 0.5)
                << "setting " << setting << " at " << f << " Hz";
        }
    }
}

TEST(Unit_Dsp, FmvToneStackMidScoopDeepensAsMiddleCloses)
{
    SushiDSP::FmvToneStack open(48000.0, SushiDSP::kJcm2203ToneStack);
    SushiDSP::FmvToneStack closed(48000.0, SushiDSP::kJcm2203ToneStack);
    open.set_middle(0.9f);
    closed.set_middle(0.1f);
    EXPECT_LT(steady_gain_db(closed, 48000.0, 500.0), steady_gain_db(open, 48000.0, 500.0) - 6.0f);
}

TEST(Unit_Dsp, FmvToneStackIsStableAtEveryKnobExtreme)
{
    const float positions[2] = { 0.0f, 1.0f };
    for (float b : positions)
    {
        for (float m : positions)
        {
            for (float t : positions)
            {
                SushiDSP::FmvToneStack stack(48000.0, SushiDSP::kJcm2203ToneStack);
                stack.set_bass(b);
                stack.set_middle(m);
                stack.set_treble(t);
                float y = 0.0f;
                for (int i = 0; i < 48000; ++i) y = stack.process_sample(i == 0 ? 1.0f : 0.0f);
                EXPECT_LT(std::fabs(y), 1e-6f) << b << m << t;
            }
        }
    }
}
