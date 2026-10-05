/****************************************************************/
/* test_splitter_node.cpp                                       */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_splitter_node.cpp
 * @brief Tests that SplitterNode copies its input and has no parameters.
 * @author Mustafa Garip
 */

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
