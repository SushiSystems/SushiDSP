/**************************************************************************/
/* test_splitter_node.cpp                                                 */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
/**************************************************************************/

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
