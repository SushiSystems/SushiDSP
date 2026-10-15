/**************************************************************************/
/* test_band_response.cpp                                                 */
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
 * @file test_band_response.cpp
 * @brief Tests for BandResponse, the six-band gain vector of the room module.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>

#include <SushiDSP/nodes/room/band_response.hpp>

TEST(Small_BandResponse, BandResponseUnityIsOneInEveryBand)
{
    const SushiDSP::room::BandResponse unity = SushiDSP::room::BandResponse::unity();
    for (std::size_t band = 0; band < SushiDSP::room::K_BAND_COUNT; ++band)
    {
        EXPECT_FLOAT_EQ(unity.gain[band], 1.0f);
    }
}

TEST(Small_BandResponse, BandResponseMultipliesBandByBand)
{
    SushiDSP::room::BandResponse first{{0.5f, 0.5f, 0.25f, 1.0f, 2.0f, 4.0f}};
    const SushiDSP::room::BandResponse second{{2.0f, 4.0f, 4.0f, 1.0f, 0.5f, 0.25f}};
    first *= second;
    const std::array<float, 6> expected = {1.0f, 2.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    for (std::size_t band = 0; band < 6u; ++band)
    {
        EXPECT_FLOAT_EQ(first.gain[band], expected[band]);
    }
}

TEST(Small_BandResponse, BandResponsePowerOfFourIsFourMultiplications)
{
    const SushiDSP::room::BandResponse one{{0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f}};
    const SushiDSP::room::BandResponse four = one.powered(4.0);
    for (std::size_t band = 0; band < 6u; ++band)
    {
        const float by_hand = one.gain[band] * one.gain[band] * one.gain[band] * one.gain[band];
        EXPECT_NEAR(four.gain[band], by_hand, 1e-6f);
    }
}

TEST(Small_BandResponse, BandResponseDecibelsRoundTrip)
{
    const std::array<float, 6> decibels = {-6.0f, -3.0f, 0.0f, 3.0f, 6.0f, -20.0f};
    const SushiDSP::room::BandResponse response =
        SushiDSP::room::BandResponse::from_decibels(decibels);
    EXPECT_NEAR(response.gain[0], 0.501187f, 1e-6f); // 10^(-6/20)
    EXPECT_NEAR(response.gain[5], 0.1f, 1e-6f);      // 10^(-20/20)
    const std::array<float, 6> back = response.decibels();
    for (std::size_t band = 0; band < 6u; ++band)
    {
        EXPECT_NEAR(back[band], decibels[band], 1e-4f);
    }
}

TEST(Small_BandResponse, BandResponsePowerMeanIsTheRootMeanSquare)
{
    const SushiDSP::room::BandResponse flat{{1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}};
    EXPECT_NEAR(flat.power_mean(), 1.0f, 1e-6f);
    const SushiDSP::room::BandResponse mixed{{0.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f}};
    EXPECT_NEAR(mixed.power_mean(), std::sqrt(2.0f), 1e-6f); // sqrt((3 * 4) / 6)
}

TEST(Small_BandResponse, BandResponseZeroGainReadsAsTheDecibelFloor)
{
    const SushiDSP::room::BandResponse silent{{0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}};
    const std::array<float, 6> decibels = silent.decibels();
    EXPECT_FLOAT_EQ(decibels[0], -200.0f);
    EXPECT_NEAR(decibels[1], 0.0f, 1e-6f);
}

TEST(Small_BandResponse, BandCentresAreTheSixIsoOctaves)
{
    const std::array<double, 6> expected = {125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0};
    for (std::size_t band = 0; band < 6u; ++band)
    {
        EXPECT_DOUBLE_EQ(SushiDSP::room::K_BAND_CENTRES_HZ[band], expected[band]);
    }
    EXPECT_DOUBLE_EQ(SushiDSP::room::K_SPEED_OF_SOUND, 343.0);
}
