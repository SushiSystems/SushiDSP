/**************************************************************************/
/* compile_linear_port.cpp                                              */
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
 * @file compile_linear_port.cpp
 * @brief Compiles linear_port.hpp and checks that the port has the glue call of a block with no nonlinear branch.
 * @author Mustafa Garip
 */

#include <array>
#include <type_traits>

#include <SushiDSP/foundation/math/dk/linear_port.hpp>

static_assert(std::is_empty_v<SushiDSP::dk::LinearPort>, "a linear port holds no state");
static_assert(std::is_invocable_v<decltype(&SushiDSP::dk::LinearPort::currents_and_jacobian), const SushiDSP::dk::LinearPort&,
                                  const std::array<double, 0>&, std::array<double, 0>&,
                                  std::array<std::array<double, 0>, 0>&>,
              "the port answers the glue call DkCircuit makes with zero branches");
