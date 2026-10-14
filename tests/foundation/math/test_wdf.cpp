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
 * @file test_wdf.cpp
 * @brief Tests the wave digital filter elements of Wdf.
 * @author Mustafa Garip
 */

#include <cmath>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/math/wdf.hpp>

TEST(Wdf, WdfResistorReportsConfiguredPortResistance)
{
    SushiDSP::WdfResistor resistor(1000.0f);
    EXPECT_FLOAT_EQ(resistor.port_resistance(), 1000.0f);
}

TEST(Wdf, WdfResistorAlwaysReflectsZero)
{
    SushiDSP::WdfResistor resistor(1000.0f);
    EXPECT_FLOAT_EQ(resistor.reflect(5.0f), 0.0f);
    EXPECT_FLOAT_EQ(resistor.reflect(-3.0f), 0.0f);
    EXPECT_FLOAT_EQ(resistor.reflect(0.0f), 0.0f);
}

TEST(Wdf, WdfCapacitorPortResistanceMatchesBilinearFormula)
{
    constexpr double sample_rate = 48000.0;
    constexpr float capacitance = 1e-6f; // 1 microfarad
    SushiDSP::WdfCapacitor capacitor(sample_rate, capacitance);

    const float expected_resistance = static_cast<float>(1.0 / sample_rate) / (2.0f * capacitance);
    EXPECT_NEAR(capacitor.port_resistance(), expected_resistance, expected_resistance * 1e-4f);
}

TEST(Wdf, WdfCapacitorReflectsPreviousIncidentWave)
{
    SushiDSP::WdfCapacitor capacitor(48000.0, 1e-6f);

    EXPECT_FLOAT_EQ(capacitor.reflect(0.0f), 0.0f);  // initial state is zero
    EXPECT_FLOAT_EQ(capacitor.reflect(2.0f), 0.0f);  // b[1] = a[0] = 0
    EXPECT_FLOAT_EQ(capacitor.reflect(3.0f), 2.0f);  // b[2] = a[1] = 2
    EXPECT_FLOAT_EQ(capacitor.reflect(-1.0f), 3.0f); // b[3] = a[2] = 3
}

TEST(Wdf, WdfCapacitorResetClearsState)
{
    SushiDSP::WdfCapacitor capacitor(48000.0, 1e-6f);
    capacitor.reflect(5.0f);
    capacitor.reset();
    EXPECT_FLOAT_EQ(capacitor.reflect(1.0f), 0.0f);
}
