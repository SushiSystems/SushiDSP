/**************************************************************************/
/* test_conductance_port.cpp                                              */
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
 * @file test_conductance_port.cpp
 * @brief Tests that every branch of the conductance port is the linear law i = g v with dI/dV = g.
 * @author Mustafa Garip
 */

#include <array>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/math/dk/conductance_port.hpp>

TEST(Small_ConductancePort, EveryBranchCurrentIsTheConductanceTimesItsVoltage)
{
    const SushiDSP::dk::ConductancePort<2> port{0.25};
    std::array<double, 2> currents{};
    std::array<std::array<double, 2>, 2> jacobian{};
    port.currents_and_jacobian({2.0, -4.0}, currents, jacobian);
    EXPECT_EQ(currents, (std::array<double, 2>{0.5, -1.0}));
}

TEST(Small_ConductancePort, TheJacobianDiagonalIsTheConductance)
{
    const SushiDSP::dk::ConductancePort<2> port{0.25};
    std::array<double, 2> currents{};
    std::array<std::array<double, 2>, 2> jacobian{};
    port.currents_and_jacobian({2.0, -4.0}, currents, jacobian);
    EXPECT_EQ(jacobian, (std::array<std::array<double, 2>, 2>{{{0.25, 0.0}, {0.0, 0.25}}}));
}
