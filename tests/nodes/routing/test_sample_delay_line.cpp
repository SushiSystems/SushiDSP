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

/** @file test_sample_delay_line.cpp
 *  @brief Tests the whole-sample delay line a channel strip uses.
 *  @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/routing/sample_delay_line.hpp>

TEST(Small_SampleDelayLine, SampleDelayLineZeroDelayPassesTheSampleThrough)
{
    SushiDSP::SampleDelayLine line;
    line.prepare(4);
    line.set_delay(0);

    EXPECT_FLOAT_EQ(line.process(0.5f), 0.5f);
    EXPECT_FLOAT_EQ(line.process(-0.25f), -0.25f);
}

TEST(Small_SampleDelayLine, SampleDelayLineReturnsTheSampleFromNSamplesEarlier)
{
    SushiDSP::SampleDelayLine line;
    line.prepare(8);
    line.set_delay(3);

    const float inputs[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    const float expected[] = {0.0f, 0.0f, 0.0f, 1.0f, 2.0f};
    for (std::size_t i = 0; i < 5; ++i)
    {
        EXPECT_FLOAT_EQ(line.process(inputs[i]), expected[i]) << "sample " << i;
    }
}

TEST(Small_SampleDelayLine, SampleDelayLineClampsTheDelayToItsPreparedMaximum)
{
    SushiDSP::SampleDelayLine line;
    line.prepare(4);
    line.set_delay(100);

    EXPECT_EQ(line.max_delay(), 4u);
    EXPECT_FLOAT_EQ(line.process(1.0f), 0.0f);
    for (int i = 0; i < 3; ++i)
    {
        EXPECT_FLOAT_EQ(line.process(0.0f), 0.0f);
    }
    EXPECT_FLOAT_EQ(line.process(0.0f), 1.0f);
}

TEST(Small_SampleDelayLine, SampleDelayLineResetClearsTheStoredSamples)
{
    SushiDSP::SampleDelayLine line;
    line.prepare(4);
    line.set_delay(1);
    line.process(1.0f);

    line.reset();

    EXPECT_FLOAT_EQ(line.process(0.0f), 0.0f);
}
