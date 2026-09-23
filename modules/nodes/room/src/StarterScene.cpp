/**************************************************************************/
/* StarterScene.cpp                                                       */
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
 * @file StarterScene.cpp
 * @brief The maintainer's room: its finishes, its coverings and its furnishings.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/StarterScene.hpp"

namespace SushiDSP::room
{
    RoomScene starter_scene()
    {
        RoomScene scene;
        scene.dimensions = { 6.0, 4.0, 2.6 };
        scene.surfaces = { MaterialId::PlasterOnMasonryPainted,
                           MaterialId::PlasterOnMasonryPainted,
                           MaterialId::PlasterOnMasonryPainted,
                           MaterialId::PlasterOnMasonryPainted,
                           MaterialId::ParquetOnConcrete,
                           MaterialId::PlasterOnMasonryPainted };

        scene.patches[0] = { Surface::PlusY, MaterialId::CurtainHeavyVelour, 4.0 };
        scene.patches[1] = { Surface::Floor, MaterialId::ThinCarpetOnConcrete, 12.0 };
        scene.patches[2] = { Surface::MinusX, MaterialId::WoodenDoor, 1.9 };
        scene.patch_count = 3;

        scene.furnishings[0] = { FurnitureId::BedDouble, 1.0 };
        scene.furnishings[1] = { FurnitureId::Desk, 1.0 };
        scene.furnishings[2] = { FurnitureId::WardrobeModule, 3.0 };
        scene.furnishings[3] = { FurnitureId::ChairUpholstered, 1.0 };
        scene.furnishing_count = 4;

        scene.built_room_loss = true;
        return scene;
    }
}
