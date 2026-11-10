/**************************************************************************/
/* test_phase_inverter_presets.cpp                                        */
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
 * @file test_phase_inverter_presets.cpp
 * @brief The single-triode inverter preset's operating point and the stage it drives.
 * @author Mustafa Garip & Sushi Systems
 */

#include <cmath>
#include <string>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/circuits/stages/phase_inverter_presets.hpp>
#include <SushiDSP/foundation/circuits/stages/triode_stage.hpp>

namespace
{
    const SushiDSP::SingleTriodeInverter& inverter()
    {
        return SushiDSP::K_TRIODE12_AX7_INVERTER82K_PLATE;
    }

    SushiDSP::TriodeStage make_stage()
    {
        return SushiDSP::TriodeStage(10000.0f, 1000000.0f, inverter().cathode_bias_volts, inverter().gain,
                                     SushiDSP::K_TRIODE12_AX7_GRID_CONDUCTION, inverter().plate_swing);
    }

    /// @brief Returns how far past the bottom headroom the output may read: one float rounding of the headroom on the cast, and the Hermite error of the stage's tabulated antiderivative, h^4 / 384 times gain^3 / headroom over the input step of 0.1 V, with h = headroom / gain / 64 the knot spacing.
    double bottom_slack()
    {
        const double u = TestSupport::unit_roundoff<float>();
        const double headroom = inverter().plate_swing.headroom_to_bottom_volts;
        const double gain = inverter().gain;
        const double spacing = headroom / gain / 64.0;
        return 2.0 * u * headroom + 2.0 * std::pow(spacing, 4.0) / 384.0 * gain * gain * gain / headroom / 0.1;
    }

    /// @brief Holds a small input and the name its row is reported under.
    struct Input
    {
            const char* name;
            float volts;
    };

    const Input kInputs[] = {{"Positive", 0.5f}, {"Negative", -0.5f}};
} // namespace

class Small_PhaseInverterPresetsInversion : public ::testing::TestWithParam<Input>
{
};

TEST_P(Small_PhaseInverterPresetsInversion, TheSolvedStageOutputsTheOppositeSign)
{
    SushiDSP::TriodeStage stage = make_stage();
    const float input = GetParam().volts;
    const float output = stage.process_sample(input);

    EXPECT_LT(output * input, 0.0f);
}

INSTANTIATE_TEST_SUITE_P(Inputs, Small_PhaseInverterPresetsInversion, ::testing::ValuesIn(kInputs),
                         [](const ::testing::TestParamInfo<Input>& info) { return std::string(info.param.name); });

// A large positive input drives the plate toward the bottom of its swing, which it cannot pass.
TEST(Small_PhaseInverterPresets, Triode12AX7Inverter82kPlateStageBoundsItsOutputAtTheBottomHeadroom)
{
    SushiDSP::TriodeStage stage = make_stage();

    EXPECT_GE(stage.process_sample(1000.0f), -inverter().plate_swing.headroom_to_bottom_volts - bottom_slack());
}
