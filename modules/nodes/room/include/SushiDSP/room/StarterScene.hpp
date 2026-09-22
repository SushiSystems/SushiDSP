/**************************************************************************/
/* StarterScene.hpp                                                       */
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


/**
 * @file StarterScene.hpp
 * @brief The scene a freshly created room slot loads.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include "SushiDSP/room/RoomScene.hpp"

namespace SushiDSP::room
{
    /// @brief Returns the maintainer's own room: a 6.0 x 4.0 x 2.6 m room of a Turkish TOKI 3+1
    /// flat.
    RoomScene starter_scene();
}
