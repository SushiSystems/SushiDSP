/****************************************************************/
/* test_spice_csv.cpp                                           */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_spice_csv.cpp
 * @brief Tests the loading of LTspice CSV exports in SpiceCsv.
 * @author Mustafa Garip
 */

#include <string>

#include <gtest/gtest.h>

#include "measure/SpiceCsv.hpp"

TEST(Unit_Dsp, SpiceCsvLoadsAcSweepColumns)
{
    const auto sweep = SushiDSP::measure::load_ac_csv(std::string(SUSHIDSP_FIXTURE_DIR) + "/spice/example_ac.csv");
    ASSERT_EQ(sweep.frequency_hz.size(), 5u);
    EXPECT_DOUBLE_EQ(sweep.frequency_hz[0], 10.0);
    EXPECT_EQ(sweep.setting[4], 0);
    EXPECT_DOUBLE_EQ(sweep.magnitude_db[4], -3.5);
    EXPECT_DOUBLE_EQ(sweep.phase_degrees[4], -45.0);
}

TEST(Unit_Dsp, SpiceCsvLoadsTransientAtItsRate)
{
    const auto transient = SushiDSP::measure::load_transient_csv(std::string(SUSHIDSP_FIXTURE_DIR) + "/spice/example_tran.csv");
    ASSERT_EQ(transient.input.size(), 8u);
    EXPECT_NEAR(transient.sample_rate, 192000.0, 1.0);
    EXPECT_FLOAT_EQ(transient.output[7], 0.125f);
}

TEST(Unit_Dsp, SpiceCsvMissingFileYieldsEmptySweep)
{
    const auto sweep = SushiDSP::measure::load_ac_csv(std::string(SUSHIDSP_FIXTURE_DIR) + "/spice/does_not_exist.csv");
    EXPECT_TRUE(sweep.frequency_hz.empty());
}
