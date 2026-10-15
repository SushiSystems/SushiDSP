/**************************************************************************/
/* test_thd.cpp                                                           */
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
 * @file test_thd.cpp
 * @brief Tests the total harmonic distortion of a pure tone and the shape of its harmonic table.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/signal_measure/render.hpp>
#include <SushiDSP/foundation/signal_measure/thd.hpp>

#include <vector>

TEST(Thd, PureToneHasNegligibleDistortion)
{
    const std::vector<float> tone = SushiDSP::measure::sine_stimulus(1000.0, 48000.0,
                                                                     SushiDSP::measure::K_FFT_SIZE, 0.5f);
    const SushiDSP::measure::ThdResult result = SushiDSP::measure::thd(tone, 1000.0, 48000.0);
    EXPECT_LT(result.thd_percent, 0.01);
}

TEST(Thd, ReportsOneEntryPerRequestedHarmonic)
{
    const std::vector<float> tone = SushiDSP::measure::sine_stimulus(1000.0, 48000.0,
                                                                     SushiDSP::measure::K_FFT_SIZE, 0.5f);
    const SushiDSP::measure::ThdResult result = SushiDSP::measure::thd(tone, 1000.0, 48000.0, 5);
    EXPECT_EQ(result.per_harmonic_db.size(), 5u);
    EXPECT_NEAR(result.per_harmonic_db[0], 0.0, 1e-9);
}

TEST(Thd, ShortSignalIsRefusedAsZero)
{
    const std::vector<float> short_signal(100, 0.1f);
    const SushiDSP::measure::ThdResult result = SushiDSP::measure::thd(short_signal, 1000.0, 48000.0);
    EXPECT_EQ(result.thd_percent, 0.0);
}
