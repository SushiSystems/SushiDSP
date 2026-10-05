/****************************************************************/
/* StarterScene.cpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
