/**************************************************************************/
/* test_spice_csv.cpp                                                     */
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
 * @file test_spice_csv.cpp
 * @brief Tests the parsing and loading of LTspice CSV exports in SpiceCsv.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/spice_reference/spice_csv.hpp>

namespace
{
    /// The rows of example_ac.csv, whose fifth row the assertions read.
    const char* const kAcText =
        "frequency_hz,setting,magnitude_db,phase_degrees\n"
        "10,0,0.0,0.0\n"
        "100,0,-0.1,-5.0\n"
        "1000,0,-1.0,-20.0\n"
        "10000,0,-2.5,-40.0\n"
        "20000,0,-3.5,-45.0\n";

    /// The rows of example_tran.csv: a 5.20833333333e-06 s grid, input rising 0.1 per row.
    const char* const kTransientText =
        "time_s,setting,input_v,output_v\n"
        "0,0,0.000000,0.000000000\n"
        "5.20833333333e-06,0,0.100000,0.017857143\n"
        "1.04166666667e-05,0,0.200000,0.035714286\n"
        "1.5625e-05,0,0.300000,0.053571429\n"
        "2.08333333333e-05,0,0.400000,0.071428571\n"
        "2.60416666667e-05,0,0.500000,0.089285714\n"
        "3.125e-05,0,0.600000,0.107142857\n"
        "3.64583333333e-05,0,0.700000,0.125000000\n";

    /// @brief Parses the text as an AC sweep through an in-memory stream.
    SushiDSP::measure::AcSweep parse_ac(const std::string& text)
    {
        std::istringstream source(text);
        return SushiDSP::measure::parse_ac_csv(source);
    }

    /// @brief Parses the text as a transient through an in-memory stream.
    SushiDSP::measure::Transient parse_transient(const std::string& text)
    {
        std::istringstream source(text);
        return SushiDSP::measure::parse_transient_csv(source);
    }
} // namespace

TEST(Small_SpiceCsv, SpiceCsvParsesAcSweepColumns)
{
    const auto sweep = parse_ac(kAcText);

    ASSERT_EQ(sweep.frequency_hz.size(), 5u);
    EXPECT_DOUBLE_EQ(sweep.frequency_hz[0], 10.0);
    EXPECT_EQ(sweep.setting[4], 0);
    EXPECT_DOUBLE_EQ(sweep.magnitude_db[4], -3.5);
    EXPECT_DOUBLE_EQ(sweep.phase_degrees[4], -45.0);
}

// The grid step is printed with 12 significant digits, so the rate it gives is good to 5e-12 relative.
TEST(Small_SpiceCsv, SpiceCsvParsesTransientAtItsRate)
{
    const auto transient = parse_transient(kTransientText);

    ASSERT_EQ(transient.input.size(), 8u);
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(transient.sample_rate, 192000.0, 192000.0 * 5e-12, 1.0)));
    EXPECT_FLOAT_EQ(transient.output[7], 0.125f);
}

TEST(Medium_SpiceCsv, SpiceCsvLoadsAnAcSweepFromAFile)
{
    const auto sweep =
        SushiDSP::measure::load_ac_csv(std::string(SUSHIDSP_FIXTURE_DIR) + "/example_ac.csv");

    ASSERT_EQ(sweep.frequency_hz.size(), 5u);
    EXPECT_DOUBLE_EQ(sweep.frequency_hz[4], 20000.0);
}

TEST(Medium_SpiceCsv, SpiceCsvMissingFileYieldsEmptySweep)
{
    const auto sweep = SushiDSP::measure::load_ac_csv(std::string(SUSHIDSP_FIXTURE_DIR) +
                                                      "/does_not_exist.csv");
    EXPECT_TRUE(sweep.frequency_hz.empty());
}
