/**************************************************************************/
/* test_smoothed_parameter.cpp                                            */
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
 * @file test_smoothed_parameter.cpp
 * @brief Tests the time constant and snapping of SmoothedParameter.
 * @author Mustafa Garip
 */

#include <cmath>
#include <gtest/gtest.h>
#include <SushiDSP/foundation/math/smoothed_parameter.hpp>

TEST(Small_SmoothedParameter, SmoothedParameterReachesSixtyThreePercentAtOneTimeConstant)
{
    SushiDSP::SmoothedParameter parameter(0.0f);
    parameter.prepare(48000.0, 20.0f);
    parameter.set_target(1.0f);
    float value = 0.0f;
    for (int i = 0; i < 960; ++i) // 20 ms at 48 kHz
    {
        value = parameter.next();
    }
    EXPECT_NEAR(value, 1.0f - std::exp(-1.0f), 2e-3f);
}

TEST(Small_SmoothedParameter, SmoothedParameterSkipMatchesRepeatedNext)
{
    SushiDSP::SmoothedParameter a(0.0f), b(0.0f);
    a.prepare(48000.0, 20.0f);
    b.prepare(48000.0, 20.0f);
    a.set_target(0.7f);
    b.set_target(0.7f);
    float via_next = 0.0f;
    for (int i = 0; i < 4096; ++i)
    {
        via_next = a.next();
    }
    const float via_skip = b.skip(4096);
    EXPECT_NEAR(via_next, via_skip, 1e-5f);
}

TEST(Small_SmoothedParameter, SmoothedParameterTimeConstantIsSampleRateIndependent)
{
    SushiDSP::SmoothedParameter a(0.0f), b(0.0f);
    a.prepare(48000.0, 20.0f);
    b.prepare(96000.0, 20.0f);
    a.set_target(1.0f);
    b.set_target(1.0f);
    const float at_48k = a.skip(960);
    const float at_96k = b.skip(1920);
    EXPECT_NEAR(at_48k, at_96k, 1e-4f);
}

TEST(Small_SmoothedParameter, SmoothedParameterSnapSettlesImmediately)
{
    SushiDSP::SmoothedParameter parameter(0.0f);
    parameter.prepare(48000.0, 20.0f);
    parameter.snap(0.3f);
    EXPECT_TRUE(parameter.is_settled());
    EXPECT_FLOAT_EQ(parameter.next(), 0.3f);
}
