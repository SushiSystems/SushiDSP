/**************************************************************************/
/* test_linear_port.cpp                                                   */
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
 * @file test_linear_port.cpp
 * @brief Tests that a linear port declares no nonlinear branch and needs no currents.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/math/dk/linear_port.hpp>

#include <array>

TEST(Small_LinearPort, CurrentsAndJacobianAcceptNoBranches)
{
    const SushiDSP::dk::LinearPort port;
    const std::array<double, 0> voltages{};
    std::array<double, 0> currents{};
    std::array<std::array<double, 0>, 0> jacobian{};
    port.currents_and_jacobian(voltages, currents, jacobian);
    EXPECT_EQ(currents.size(), 0u);
    EXPECT_EQ(jacobian.size(), 0u);
}
