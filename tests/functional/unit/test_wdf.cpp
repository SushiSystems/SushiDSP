/****************************************************************/
/* test_wdf.cpp                                                 */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_wdf.cpp
 * @brief Tests the wave digital filter elements of Wdf.
 * @author Mustafa Garip
 */

#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/math/Wdf.hpp"

TEST(Unit_Dsp, WdfResistorReportsConfiguredPortResistance)
{
    SushiDSP::WdfResistor resistor(1000.0f);
    EXPECT_FLOAT_EQ(resistor.port_resistance(), 1000.0f);
}

TEST(Unit_Dsp, WdfResistorAlwaysReflectsZero)
{
    SushiDSP::WdfResistor resistor(1000.0f);
    EXPECT_FLOAT_EQ(resistor.reflect(5.0f), 0.0f);
    EXPECT_FLOAT_EQ(resistor.reflect(-3.0f), 0.0f);
    EXPECT_FLOAT_EQ(resistor.reflect(0.0f), 0.0f);
}

TEST(Unit_Dsp, WdfCapacitorPortResistanceMatchesBilinearFormula)
{
    constexpr double sample_rate = 48000.0;
    constexpr float capacitance = 1e-6f;  // 1 microfarad
    SushiDSP::WdfCapacitor capacitor(sample_rate, capacitance);

    const float expected_resistance = static_cast<float>(1.0 / sample_rate) / (2.0f * capacitance);
    EXPECT_NEAR(capacitor.port_resistance(), expected_resistance, expected_resistance * 1e-4f);
}

TEST(Unit_Dsp, WdfCapacitorReflectsPreviousIncidentWave)
{
    SushiDSP::WdfCapacitor capacitor(48000.0, 1e-6f);

    EXPECT_FLOAT_EQ(capacitor.reflect(0.0f), 0.0f);  // initial state is zero
    EXPECT_FLOAT_EQ(capacitor.reflect(2.0f), 0.0f);  // b[1] = a[0] = 0
    EXPECT_FLOAT_EQ(capacitor.reflect(3.0f), 2.0f);  // b[2] = a[1] = 2
    EXPECT_FLOAT_EQ(capacitor.reflect(-1.0f), 3.0f); // b[3] = a[2] = 3
}

TEST(Unit_Dsp, WdfCapacitorResetClearsState)
{
    SushiDSP::WdfCapacitor capacitor(48000.0, 1e-6f);
    capacitor.reflect(5.0f);
    capacitor.reset();
    EXPECT_FLOAT_EQ(capacitor.reflect(1.0f), 0.0f);
}
