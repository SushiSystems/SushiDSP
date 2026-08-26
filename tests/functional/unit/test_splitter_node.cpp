/**************************************************************************/
/* test_splitter_node.cpp                                                 */
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

#include <memory>
#include <new>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/routing/SplitterNode.hpp"

TEST(Unit_Dsp, SplitterNodeCopiesInputToOutput)
{
    SushiDSP::SplitterNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });

    std::vector<float> input(64, 3.5f);
    std::vector<float> output(64, 0.0f);
    const float* inputs[1] = { input.data() };
    float* outputs[1] = { output.data() };
    SushiDSP::ProcessContext context{ outputs, inputs, 64, 0.0 };

    node.process(context);

    for (float sample : output)
    {
        EXPECT_FLOAT_EQ(sample, 3.5f);
    }
}

TEST(Unit_Dsp, SplitterNodeHasNoParameters)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::splitter_descriptor();
    EXPECT_STREQ(descriptor.id, "sushidsp.splitter");
    EXPECT_EQ(descriptor.parameter_count, 0u);
}
