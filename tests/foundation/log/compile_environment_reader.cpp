/**************************************************************************/
/* compile_environment_reader.cpp                                         */
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
 * @file compile_environment_reader.cpp
 * @brief Compiles environment_reader.hpp and checks that the port is an abstract type.
 * @author Mustafa Garip
 */

#include <type_traits>

#include <SushiDSP/foundation/log/environment_reader.hpp>

static_assert(std::is_abstract_v<SushiDSP::EnvironmentReader>,
              "an environment reader is a port, implemented by an adapter");
static_assert(std::has_virtual_destructor_v<SushiDSP::EnvironmentReader>,
              "an adapter is deleted through the port");
