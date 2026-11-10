/**************************************************************************/
/* test_power_supply_sag_stage.cpp                                        */
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
 * @file test_power_supply_sag_stage.cpp
 * @brief Tests the sag and recovery of PowerSupplySagStage.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/circuits/stages/power_supply_sag_stage.hpp>

namespace
{
    constexpr double K_SAMPLE_RATE = 48000.0;
    constexpr float K_SAG = 0.5f;
    constexpr float K_RECOVERY_SECONDS = 0.05f;
    constexpr double K_GAIN_FLOOR = 0.2;

    const double kFloatU = TestSupport::unit_roundoff<float>();

    SushiDSP::PowerSupplySagStage make_stage()
    {
        return SushiDSP::PowerSupplySagStage(K_SAMPLE_RATE, K_SAG, K_RECOVERY_SECONDS);
    }

    /// @brief Returns the envelope coefficient 1 - exp(-1 / (T fs)) in double.
    double coefficient()
    {
        return 1.0 - std::exp(-1.0 / (static_cast<double>(K_RECOVERY_SECONDS) * K_SAMPLE_RATE));
    }

    /// @brief Returns the last output after feeding @p count samples of @p value.
    float feed(SushiDSP::PowerSupplySagStage& stage, float value, std::size_t count)
    {
        float last = 0.0f;
        for (std::size_t i = 0; i < count; ++i)
        {
            last = stage.process_sample(value);
        }
        return last;
    }

    /// @brief Returns the bound on a value read after @p samples steps of the envelope: the coefficient is formed in float by an exponential and a subtraction (two roundings of at most u), each step multiplies that error in, and each step adds one more rounding of the envelope; all scaled by the sag.
    double envelope_bound(std::size_t samples)
    {
        return static_cast<double>(K_SAG) * 3.0 * static_cast<double>(samples) * kFloatU;
    }

    /// @brief Returns the gain after the envelope reaches @p envelope, as the stage forms it.
    double gain_at(double envelope)
    {
        return std::max(K_GAIN_FLOOR, 1.0 - static_cast<double>(K_SAG) * envelope);
    }

    /// @brief Holds a count of loud samples and the name its row is reported under.
    struct LoudRun
    {
            const char* name;
            std::size_t samples;
    };

    const LoudRun kLoudRuns[] = {
        {"OneSample", 1}, {"HundredSamples", 100}, {"FiveThousandSamples", 5000}, {"OneSecond", 48000},
    };

    /// @brief Holds a loud run followed by a silent one, and the name its row is reported under.
    struct Recovery
    {
            const char* name;
            std::size_t loud;
            std::size_t silent;
    };

    const Recovery kRecoveries[] = {
        {"AfterTenThousandSilentSamples", 5000, 10000},
        {"AfterTwentyThousandSilentSamples", 5000, 20000},
    };
} // namespace

class Small_PowerSupplySagStageLoud : public ::testing::TestWithParam<LoudRun>
{
};

// A constant input of 1 drives the envelope to 1 - (1 - c)^n after n samples, so the n-th output is the gain at that envelope.
TEST_P(Small_PowerSupplySagStageLoud, GainFollowsTheEnvelopeOfAConstantInput)
{
    SushiDSP::PowerSupplySagStage stage = make_stage();
    const std::size_t count = GetParam().samples;
    const double envelope = 1.0 - std::pow(1.0 - coefficient(), static_cast<double>(count));

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(feed(stage, 1.0f, count), gain_at(envelope), envelope_bound(count))));
}

INSTANTIATE_TEST_SUITE_P(Runs, Small_PowerSupplySagStageLoud, ::testing::ValuesIn(kLoudRuns),
                         [](const ::testing::TestParamInfo<LoudRun>& info) { return std::string(info.param.name); });

class Small_PowerSupplySagStageRecovery : public ::testing::TestWithParam<Recovery>
{
};

// After the loud run the envelope is 1 - a^L with a = 1 - c; silence multiplies it by a^S, and the next unit sample steps it to a e + c.
TEST_P(Small_PowerSupplySagStageRecovery, GainRecoversAlongTheEnvelopeAfterSilence)
{
    SushiDSP::PowerSupplySagStage stage = make_stage();
    const Recovery& row = GetParam();
    const double decay = 1.0 - coefficient();
    const double loud = 1.0 - std::pow(decay, static_cast<double>(row.loud));
    const double settled = loud * std::pow(decay, static_cast<double>(row.silent));
    const double expected = gain_at(decay * settled + coefficient());

    feed(stage, 1.0f, row.loud);
    feed(stage, 0.0f, row.silent);
    const float output = stage.process_sample(1.0f);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(output, expected, envelope_bound(row.loud + row.silent + 1))));
}

INSTANTIATE_TEST_SUITE_P(Runs, Small_PowerSupplySagStageRecovery, ::testing::ValuesIn(kRecoveries),
                         [](const ::testing::TestParamInfo<Recovery>& info) { return std::string(info.param.name); });

TEST(Small_PowerSupplySagStage, PowerSupplySagStageNeverProducesNonFiniteOutput)
{
    SushiDSP::PowerSupplySagStage stage(K_SAMPLE_RATE, 0.9f, 0.01f);
    std::vector<float> outputs(2000);

    std::generate(outputs.begin(), outputs.end(), [&] { return stage.process_sample(10.0f); });

    EXPECT_TRUE(std::all_of(outputs.begin(), outputs.end(), [](float value) { return std::isfinite(value); }));
}

// After a reset the envelope is zero, the constructed state, and the arithmetic is deterministic, so the next output equals a fresh stage's bit for bit.
TEST(Small_PowerSupplySagStage, PowerSupplySagStageResetClearsEnvelope)
{
    SushiDSP::PowerSupplySagStage used = make_stage();
    SushiDSP::PowerSupplySagStage fresh = make_stage();

    feed(used, 1.0f, 1000);
    used.reset();

    EXPECT_EQ(used.process_sample(1.0f), fresh.process_sample(1.0f));
}
