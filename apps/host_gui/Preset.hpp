/**************************************************************************/
/* Preset.hpp                                                             */
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

#pragma once

#include <string>

#include "RigController.hpp"

/// @brief Writes the controller's chain (node ids in order) and every declared parameter's
/// current value to a JSON file at path. Throws std::runtime_error if the file can't be opened.
void save_preset(RigController& controller, const std::string& path);

/// @brief Reads a JSON preset from path and replaces controller's entire chain via
/// RigController::load_chain(). Throws std::runtime_error if the file can't be opened or parsed.
void load_preset(RigController& controller, const std::string& path);
