/**************************************************************************/
/* garage_benchmark_scene.hpp                                             */
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
 * @file garage_benchmark_scene.hpp
 * @brief Declares the garage pose every room benchmark case drives the layers at.
 * @author Mustafa Garip
 */

#pragma once

#include <SushiDSP/nodes/room/room_scene.hpp>
#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>

namespace SushiDSP::test
{
    /// @brief Returns the garage with the cabinet at (0.4, 0.5, 0.5) and the ears and an omni
    /// microphone together at (5.4, 3.4, 2.0), the pose the modal, early and tail layers are all
    /// driven at (`modules/nodes/room/README.md`, "The wave layer and the seam").
    inline room::RoomScene garage_benchmark_scene()
    {
        room::RoomScene scene = garage_scene();
        scene.source_position = {0.4, 0.5, 0.5};
        scene.listener_position = {5.4, 3.4, 2.0};
        scene.microphone_position = {5.4, 3.4, 2.0};
        scene.microphone_pattern = room::PolarPattern::Omni;
        return scene;
    }
} // namespace SushiDSP::test
