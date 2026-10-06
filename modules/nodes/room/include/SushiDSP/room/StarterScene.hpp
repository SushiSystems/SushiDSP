/**************************************************************************/
/* StarterScene.hpp                                                       */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
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
