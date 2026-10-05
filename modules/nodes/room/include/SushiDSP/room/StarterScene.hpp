/****************************************************************/
/* StarterScene.hpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
