/**************************************************************************/
/* test_diode_pair_port_adapters.cpp                                      */
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
 * @file test_diode_pair_port_adapters.cpp
 * @brief Tests for the in-memory adapters of the clip stage's diode-pair port.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>

#include <SushiDSP/foundation/circuits/models/silicon_diode.hpp>
#include <SushiDSP/nodes/tubescreamer/diode_pair_port_adapters.hpp>

namespace
{
    using SushiDSP::tube_screamer_circuits::RecordingDiodePairPort;
    using SushiDSP::tube_screamer_circuits::StiffDiodePairPort;

    constexpr std::array<double, 2> K_FIRST_VOLTAGE{0.30, -0.30};
    constexpr std::array<double, 2> K_SECOND_VOLTAGE{0.45, -0.45};

    /// @brief |p0 + p1| / |p0| for p = {0.2, -0.1}: 0.1 / 0.2.
    constexpr double K_EXPECTED_ASYMMETRY = 0.5;

    /// @brief The bound on that quotient: the recorder adds 1e-30 to the scale, a relative 5e-30.
    constexpr double K_GUARD_BOUND = 1.0e-12;
}

TEST(Small_DiodePairPortAdapters, StiffPortKeepsTheVoltagesOfItsLastTwoEvaluations)
{
    StiffDiodePairPort port;
    std::array<double, 2> currents{};
    std::array<std::array<double, 2>, 2> jacobian{};

    port.currents_and_jacobian(K_FIRST_VOLTAGE, currents, jacobian);
    port.currents_and_jacobian(K_SECOND_VOLTAGE, currents, jacobian);

    EXPECT_EQ(port.previous_voltage, K_FIRST_VOLTAGE);
    EXPECT_EQ(port.last_voltage, K_SECOND_VOLTAGE);
}

TEST(Small_DiodePairPortAdapters, StiffPortReturnsTheCurrentsOfADiodeNineTimesStifferThanA1N914)
{
    const SushiDSP::SiliconDiode stiff{2.52e-9, 1.0, 5.0e-3};
    StiffDiodePairPort port;
    std::array<double, 2> currents{};
    std::array<std::array<double, 2>, 2> jacobian{};

    port.currents_and_jacobian(K_SECOND_VOLTAGE, currents, jacobian);

    EXPECT_DOUBLE_EQ(currents[0], stiff.current(K_SECOND_VOLTAGE[0]));
    EXPECT_DOUBLE_EQ(currents[1], stiff.current(K_SECOND_VOLTAGE[1]));
}

TEST(Small_DiodePairPortAdapters, RecordingPortSeesNoAsymmetryInAnExactlyReversedPair)
{
    RecordingDiodePairPort port;
    const std::array<double, 2> open_circuit{0.2, -0.2};
    const std::array<double, 2> voltage{0.1, -0.1};
    const std::array<std::array<double, 2>, 2> impedance{{{300.0, -300.0}, {-300.0, 300.0}}};
    std::array<double, 2> currents{1.0, -1.0};

    port.currents_for_update(open_circuit, voltage, impedance, currents);

    EXPECT_EQ(port.worst_open_circuit_asymmetry, 0.0);
    EXPECT_EQ(port.worst_voltage_asymmetry, 0.0);
    EXPECT_EQ(port.worst_impedance_asymmetry, 0.0);
}

TEST(Small_DiodePairPortAdapters, RecordingPortReportsAnAsymmetryAndTheLastImpedance)
{
    RecordingDiodePairPort port;
    const std::array<double, 2> open_circuit{0.2, -0.1};
    const std::array<double, 2> voltage{0.1, -0.1};
    const std::array<std::array<double, 2>, 2> impedance{{{300.0, -300.0}, {-300.0, 300.0}}};
    std::array<double, 2> currents{};

    port.currents_for_update(open_circuit, voltage, impedance, currents);

    EXPECT_NEAR(port.worst_open_circuit_asymmetry, K_EXPECTED_ASYMMETRY, K_GUARD_BOUND);
    EXPECT_EQ(port.last_impedance, 300.0);
}

TEST(Small_DiodePairPortAdapters, RecordingPortLeavesTheStampedCurrentsAlone)
{
    RecordingDiodePairPort port;
    const std::array<double, 2> open_circuit{0.2, -0.2};
    const std::array<double, 2> voltage{0.1, -0.1};
    const std::array<std::array<double, 2>, 2> impedance{{{300.0, -300.0}, {-300.0, 300.0}}};
    std::array<double, 2> currents{1.5e-6, -1.5e-6};

    port.currents_for_update(open_circuit, voltage, impedance, currents);

    EXPECT_EQ(currents[0], 1.5e-6);
    EXPECT_EQ(currents[1], -1.5e-6);
}
