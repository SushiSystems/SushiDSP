/**************************************************************************/
/* test_poisonable_port.cpp                                               */
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
 * @file test_poisonable_port.cpp
 * @brief Tests that the poisonable port is a linear branch while healthy and an infinite current while poisoned.
 * @author Mustafa Garip
 */

#include <array>
#include <cmath>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/math/dk/poisonable_port.hpp>

TEST(Small_PoisonablePort, AHealthyBranchCurrentIsTheConductanceTimesItsVoltage)
{
    const SushiDSP::dk::PoisonablePort port{0.5};
    std::array<double, 1> current{};
    std::array<std::array<double, 1>, 1> jacobian{};
    port.currents_and_jacobian({3.0}, current, jacobian);
    EXPECT_EQ(current[0], 1.5);
}

TEST(Small_PoisonablePort, APoisonedBranchCurrentIsInfinite)
{
    const SushiDSP::dk::PoisonablePort port{0.5, true};
    std::array<double, 1> current{};
    std::array<std::array<double, 1>, 1> jacobian{};
    port.currents_and_jacobian({3.0}, current, jacobian);
    EXPECT_TRUE(std::isinf(current[0]));
}

TEST(Small_PoisonablePort, APoisonedBranchKeepsTheConductanceAsItsJacobian)
{
    const SushiDSP::dk::PoisonablePort port{0.5, true};
    std::array<double, 1> current{};
    std::array<std::array<double, 1>, 1> jacobian{};
    port.currents_and_jacobian({3.0}, current, jacobian);
    EXPECT_EQ(jacobian[0][0], 0.5);
}
