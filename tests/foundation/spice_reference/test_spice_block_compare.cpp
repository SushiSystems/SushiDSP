/**************************************************************************/
/* test_spice_block_compare.cpp                                           */
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
 * @file test_spice_block_compare.cpp
 * @brief Tests the reference path of a block and the band-limit applied before a comparison.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/spice_reference/spice_block_compare.hpp>

#include <cmath>
#include <string>
#include <vector>

TEST(Medium_SpiceBlockCompare, SpicePathNamesTheBlockFolderAndItsKind)
{
    EXPECT_EQ(SushiDSP::test_circuits::spice_path("ts_tone", "ac"),
              std::string(SUSHIDSP_SPICE_DIR) + "/ts_tone/ts_tone_ac.csv");
}

TEST(Medium_SpiceBlockCompare, BandLimitKeepsADcSignalAtItsLevel)
{
    std::vector<float> signal(20000, 0.5f);
    SushiDSP::test_circuits::band_limit(signal, 48000.0, 1000.0);
    EXPECT_NEAR(signal.back(), 0.5f, 1e-3f);
}

TEST(Medium_SpiceBlockCompare, BandLimitRemovesNearNyquistContent)
{
    std::vector<float> signal(20000);
    for (std::size_t i = 0; i < signal.size(); ++i)
    {
        signal[i] = (i % 2 == 0) ? 1.0f : -1.0f;
    }
    SushiDSP::test_circuits::band_limit(signal, 48000.0, 1000.0);
    float tail_peak = 0.0f;
    for (std::size_t i = signal.size() - 1000; i < signal.size(); ++i)
    {
        tail_peak = std::max(tail_peak, std::fabs(signal[i]));
    }
    EXPECT_LT(tail_peak, 0.01f);
}
