/**************************************************************************/
/* test_sample_delay_line.cpp                                             */
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
 * @file test_sample_delay_line.cpp
 * @brief Tests the whole-sample delay line a channel strip uses.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/routing/sample_delay_line.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace
{
    /// @brief One delay line to run: prepared for @p prepared samples, asked for @p requested,
    /// fed @p inputs, and expected to answer @p expected.
    struct DelayRow
    {
            const char* name;
            std::size_t prepared;
            std::size_t requested;
            std::vector<float> inputs;
            std::vector<float> expected;
    };

    /// @brief Names a row by its name.
    std::string delay_row_name(const ::testing::TestParamInfo<DelayRow>& info)
    {
        return info.param.name;
    }

    /// @brief Feeds @p inputs to a line prepared for @p prepared samples and set to @p requested.
    std::vector<float> run_line(std::size_t prepared, std::size_t requested,
                                const std::vector<float>& inputs)
    {
        SushiDSP::SampleDelayLine line;
        line.prepare(prepared);
        line.set_delay(requested);
        std::vector<float> outputs;
        for (const float input : inputs)
            outputs.push_back(line.process(input));
        return outputs;
    }

    class Small_SampleDelayLineOutput : public ::testing::TestWithParam<DelayRow>
    {
    };
} // namespace

TEST(Small_SampleDelayLine, SampleDelayLineZeroDelayPassesTheSampleThrough)
{
    SushiDSP::SampleDelayLine line;
    line.prepare(4);
    line.set_delay(0);

    EXPECT_FLOAT_EQ(line.process(0.5f), 0.5f);
    EXPECT_FLOAT_EQ(line.process(-0.25f), -0.25f);
}

TEST_P(Small_SampleDelayLineOutput, SampleDelayLineAnswersTheSampleFromTheDelayEarlier)
{
    const DelayRow& row = GetParam();

    EXPECT_EQ(run_line(row.prepared, row.requested, row.inputs), row.expected);
}

// A delay of N answers N zeros and then the input from N samples back; a larger one clamps.
INSTANTIATE_TEST_SUITE_P(
    Delays, Small_SampleDelayLineOutput,
    ::testing::Values(DelayRow{"ThreeOfEight", 8, 3, {1, 2, 3, 4, 5}, {0, 0, 0, 1, 2}},
                      DelayRow{"OneOfEight", 8, 1, {1, 2, 3}, {0, 1, 2}},
                      DelayRow{"AskedBeyondThePreparedMaximum",
                               4,
                               100,
                               {1, 0, 0, 0, 0},
                               {0, 0, 0, 0, 1}}),
    delay_row_name);

TEST(Small_SampleDelayLine, SampleDelayLineResetClearsTheStoredSamples)
{
    SushiDSP::SampleDelayLine line;
    line.prepare(4);
    line.set_delay(1);
    line.process(1.0f);

    line.reset();

    EXPECT_FLOAT_EQ(line.process(0.0f), 0.0f);
}
