/**************************************************************************/
/* compile_early_tap_set.cpp                                              */
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
 * @file compile_early_tap_set.cpp
 * @brief Compiles early_tap_set.hpp and checks that a tap set holds its capacity of plain taps.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <tuple>
#include <type_traits>

#include <SushiDSP/nodes/room/early_tap_set.hpp>

static_assert(std::tuple_size_v<decltype(SushiDSP::room::EarlyTapSet::taps)> ==
                  SushiDSP::room::EarlyTapSet::K_MAX_EARLY_TAPS,
              "a tap set stores exactly its capacity of taps");

static_assert(std::is_trivially_copyable_v<SushiDSP::room::EarlyTap>,
              "a tap is a plain record the audio thread copies between banks");

static_assert(std::is_trivially_copyable_v<SushiDSP::room::EarlyTapSet>,
              "a tap set is a plain record the audio thread copies between banks");
