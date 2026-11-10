/**************************************************************************/
/* test_wdf.cpp                                                           */
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
 * @file test_wdf.cpp
 * @brief Tests the wave digital filter elements of Wdf.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/math/wdf.hpp>

TEST(Small_Wdf, WdfResistorAlwaysReflectsZero)
{
    SushiDSP::WdfResistor resistor(1000.0f);
    EXPECT_FLOAT_EQ(resistor.reflect(5.0f), 0.0f);
    EXPECT_FLOAT_EQ(resistor.reflect(-3.0f), 0.0f);
    EXPECT_FLOAT_EQ(resistor.reflect(0.0f), 0.0f);
}

// The port resistance T / 2C has two rounded operations: the float sample period and the quotient.
TEST(Small_Wdf, WdfCapacitorPortResistanceIsThePeriodOverTwiceTheCapacitance)
{
    constexpr double sample_rate = 48000.0;
    constexpr float capacitance = 1e-6f; // 1 microfarad
    const SushiDSP::WdfCapacitor capacitor(sample_rate, capacitance);

    const double expected = (1.0 / sample_rate) / (2.0 * static_cast<double>(capacitance));
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<float>(capacitor.port_resistance(), expected, 2)));
}

TEST(Small_Wdf, WdfCapacitorReflectsPreviousIncidentWave)
{
    SushiDSP::WdfCapacitor capacitor(48000.0, 1e-6f);

    EXPECT_FLOAT_EQ(capacitor.reflect(0.0f), 0.0f);  // initial state is zero
    EXPECT_FLOAT_EQ(capacitor.reflect(2.0f), 0.0f);  // b[1] = a[0] = 0
    EXPECT_FLOAT_EQ(capacitor.reflect(3.0f), 2.0f);  // b[2] = a[1] = 2
    EXPECT_FLOAT_EQ(capacitor.reflect(-1.0f), 3.0f); // b[3] = a[2] = 3
}

TEST(Small_Wdf, WdfCapacitorResetClearsState)
{
    SushiDSP::WdfCapacitor capacitor(48000.0, 1e-6f);
    capacitor.reflect(5.0f);
    capacitor.reset();
    EXPECT_FLOAT_EQ(capacitor.reflect(1.0f), 0.0f);
}
