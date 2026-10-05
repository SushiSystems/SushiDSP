/****************************************************************/
/* FurnishingList.cpp                                           */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#include "ui/FurnishingList.hpp"

#include <cmath>

namespace ui
{
    namespace
    {
        using SushiDSP::room::FurnitureId;
        using SushiDSP::room::RoomScene;

        /// @brief Tells whether a quantity is one a furnishing may hold.
        bool is_valid_quantity(double quantity) noexcept
        {
            return std::isfinite(quantity) && quantity > 0.0;
        }
    }

    bool add_furnishing(RoomScene& scene, FurnitureId item, double quantity) noexcept
    {
        if (scene.furnishing_count >= RoomScene::kMaxFurnishings || !is_valid_quantity(quantity))
        {
            return false;
        }
        scene.furnishings[scene.furnishing_count] = RoomScene::Furnishing{ item, quantity };
        ++scene.furnishing_count;
        return true;
    }

    void remove_furnishing(RoomScene& scene, std::uint32_t index) noexcept
    {
        if (index >= scene.furnishing_count)
        {
            return;
        }
        for (std::uint32_t entry = index + 1; entry < scene.furnishing_count; ++entry)
        {
            scene.furnishings[entry - 1] = scene.furnishings[entry];
        }
        --scene.furnishing_count;
        scene.furnishings[scene.furnishing_count] = RoomScene::Furnishing{};
    }

    bool set_furnishing_quantity(RoomScene& scene, std::uint32_t index, double quantity) noexcept
    {
        if (index >= scene.furnishing_count || !is_valid_quantity(quantity))
        {
            return false;
        }
        scene.furnishings[index].quantity = quantity;
        return true;
    }

    double default_furnishing_quantity(FurnitureId) noexcept
    {
        return 1.0;
    }
}
