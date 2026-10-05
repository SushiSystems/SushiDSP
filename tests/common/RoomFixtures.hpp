/****************************************************************/
/* RoomFixtures.hpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file RoomFixtures.hpp
 * @brief The three reference rooms every room-acoustics acceptance test measures against.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include "SushiDSP/room/RoomScene.hpp"

namespace SushiDSP::test
{
    /// @brief Returns the 6.0 x 4.0 x 2.5 m garage: unpainted concrete everywhere, with a 6.0 m2
    /// open doorway in the 4.0 x 2.5 m wall at +x. Plan §"The three reference rooms".
    inline SushiDSP::room::RoomScene garage_scene()
    {
        using SushiDSP::room::MaterialId;
        using SushiDSP::room::Scattering;
        using SushiDSP::room::Surface;

        SushiDSP::room::RoomScene scene;
        scene.dimensions = { 6.0, 4.0, 2.5 };
        scene.surfaces.fill(MaterialId::ConcreteUnpainted);
        scene.scattering.fill(Scattering::Flat);
        scene.patches[0] = { Surface::PlusX, MaterialId::OpenDoorway, 6.0 };
        scene.patch_count = 1;
        return scene;
    }

    /// @brief Returns the 4.0 x 3.5 x 2.6 m bedroom: carpet on the floor, gypsum board on the
    /// ceiling and walls, with a 1.5 m2 glass window patch. Plan §"The three reference rooms".
    inline SushiDSP::room::RoomScene bedroom_scene()
    {
        using SushiDSP::room::MaterialId;
        using SushiDSP::room::Scattering;
        using SushiDSP::room::Surface;

        SushiDSP::room::RoomScene scene;
        scene.dimensions = { 4.0, 3.5, 2.6 };
        scene.surfaces.fill(MaterialId::GypsumBoardOnStuds);
        scene.surfaces[static_cast<std::size_t>(Surface::Floor)] = MaterialId::HeavyCarpetOnConcrete;
        scene.scattering.fill(Scattering::Flat);
        scene.patches[0] = { Surface::PlusX, MaterialId::GlassWindow, 1.5 };
        scene.patch_count = 1;
        return scene;
    }

    /// @brief Returns the 5.0 x 4.0 x 2.7 m block room: painted concrete block everywhere, no
    /// patches. Plan §"The three reference rooms".
    inline SushiDSP::room::RoomScene block_room_scene()
    {
        using SushiDSP::room::MaterialId;
        using SushiDSP::room::Scattering;

        SushiDSP::room::RoomScene scene;
        scene.dimensions = { 5.0, 4.0, 2.7 };
        scene.surfaces.fill(MaterialId::ConcreteBlockPainted);
        scene.scattering.fill(Scattering::Flat);
        scene.patch_count = 0;
        return scene;
    }
}
