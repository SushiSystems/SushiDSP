/**************************************************************************/
/* compile_grid_conduction_law.cpp                                        */
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
 * @file compile_grid_conduction_law.cpp
 * @brief Compiles grid_conduction_law.hpp and checks that the law is an aggregate of three floats.
 * @author Mustafa Garip
 */

#include <type_traits>

#include <SushiDSP/foundation/circuits/stages/grid_conduction_law.hpp>

static_assert(std::is_aggregate_v<SushiDSP::GridConductionLaw>, "the grid-conduction law is a plain aggregate");
static_assert(std::is_trivially_copyable_v<SushiDSP::GridConductionLaw>, "the law is copied by value into every stage");
