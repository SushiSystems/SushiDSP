/**************************************************************************/
/* test_resampler_design.cpp                                              */
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
 * @file test_resampler_design.cpp
 * @brief Tests that the resampler prototype's constants describe a consistent filter.
 * @author Mustafa Garip
 */

#include <cstdint>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/math/resampler_design.hpp>

TEST(Small_ResamplerDesign, ResamplerDesignKernelLengthHoldsTheWingThePeakAndOneGuard)
{
    // 34 * 512 + 1 endpoint + 1 guard = 17410. See tests/foundation/math/README.md.
    EXPECT_EQ(SushiDSP::K_RESAMPLER_HALF_WIDTH_ZERO_CROSSINGS *
                  SushiDSP::K_RESAMPLER_PHASES_PER_ZERO_CROSSING,
              17408u);
    EXPECT_EQ(SushiDSP::K_RESAMPLER_KERNEL_LENGTH,
              SushiDSP::K_RESAMPLER_HALF_WIDTH_ZERO_CROSSINGS *
                      SushiDSP::K_RESAMPLER_PHASES_PER_ZERO_CROSSING +
                  2u);
}

TEST(Small_ResamplerDesign, ResamplerDesignCutoffSitsBetweenThePassbandEdgeAndNyquist)
{
    EXPECT_LT(SushiDSP::K_RESAMPLER_PASSBAND_EDGE, SushiDSP::K_RESAMPLER_CUTOFF);
    EXPECT_LT(SushiDSP::K_RESAMPLER_CUTOFF, 0.5f);
}

TEST(Small_ResamplerDesign, ResamplerDesignStopbandMeetsTheHundredDecibelTarget)
{
    EXPECT_LE(SushiDSP::K_RESAMPLER_STOPBAND_DECIBELS, -100.0f);
}