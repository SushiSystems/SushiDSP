/**************************************************************************/
/* test_diode_clipper_circuit.cpp                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/DiodeClipperCircuit.hpp"

namespace
{
    // Typical silicon small-signal diode values.
    constexpr float kSourceResistance = 1000.0f;
    constexpr float kSaturationCurrent = 1e-12f;
    constexpr float kThermalVoltage = 0.02585f;
}

TEST(Unit_Dsp, DiodeClipperCircuitIsDcStableForConstantInput)
{
    SushiDSP::DiodeClipperCircuit circuit(kSourceResistance, kSaturationCurrent, kThermalVoltage);

    float output = 0.0f;
    for (int i = 0; i < 1000; ++i)
    {
        output = circuit.process_sample(0.1f);
    }

    EXPECT_TRUE(std::isfinite(output));
}

TEST(Unit_Dsp, DiodeClipperCircuitIsApproximatelyOddSymmetric)
{
    SushiDSP::DiodeClipperCircuit positive_circuit(kSourceResistance, kSaturationCurrent, kThermalVoltage);
    SushiDSP::DiodeClipperCircuit negative_circuit(kSourceResistance, kSaturationCurrent, kThermalVoltage);

    float positive_output = 0.0f;
    float negative_output = 0.0f;
    for (int i = 0; i < 50; ++i)
    {
        positive_output = positive_circuit.process_sample(0.3f);
        negative_output = negative_circuit.process_sample(-0.3f);
    }

    EXPECT_NEAR(positive_output, -negative_output, 1e-3f);
}

TEST(Unit_Dsp, DiodeClipperCircuitIsNearlyLinearForTinyInput)
{
    SushiDSP::DiodeClipperCircuit circuit(kSourceResistance, kSaturationCurrent, kThermalVoltage);

    float output = 0.0f;
    for (int i = 0; i < 10; ++i)
    {
        output = circuit.process_sample(0.001f);
    }

    EXPECT_NEAR(output, 0.001f, 2e-4f);
}

TEST(Unit_Dsp, DiodeClipperCircuitCompressesLargeInput)
{
    SushiDSP::DiodeClipperCircuit circuit(kSourceResistance, kSaturationCurrent, kThermalVoltage);

    float output = 0.0f;
    for (int i = 0; i < 50; ++i)
    {
        output = circuit.process_sample(5.0f);
    }

    EXPECT_LT(std::abs(output), 1.0f);
    EXPECT_GT(output, 0.0f);
}

TEST(Unit_Dsp, DiodeClipperCircuitResetClearsWarmStartState)
{
    SushiDSP::DiodeClipperCircuit circuit_a(kSourceResistance, kSaturationCurrent, kThermalVoltage);
    SushiDSP::DiodeClipperCircuit circuit_b(kSourceResistance, kSaturationCurrent, kThermalVoltage);

    circuit_a.process_sample(2.0f);
    circuit_a.reset();

    const float from_reset = circuit_a.process_sample(0.05f);
    const float from_fresh = circuit_b.process_sample(0.05f);

    EXPECT_NEAR(from_reset, from_fresh, 1e-5f);
}
