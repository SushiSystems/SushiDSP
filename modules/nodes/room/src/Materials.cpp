/****************************************************************/
/* Materials.cpp                                                */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file Materials.cpp
 * @brief The transcribed absorption rows and the arithmetic on them.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/Materials.hpp"

#include <cmath>
#include <cstring>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Names the source every transcribed row below cites.
        constexpr const char* kEverest =
            "Everest and Pohlmann, Master Handbook of Acoustics, 5th ed., McGraw-Hill 2009, "
            "Appendix, Absorption coefficients";

        /// @brief Holds the whole shipped table, one row per MaterialId in enumeration order.
        constexpr std::array<Material, kMaterialCount> kTable = { {
            { "Concrete, unpainted", "concrete_unpainted", { 0.01f, 0.01f, 0.02f, 0.02f, 0.02f, 0.03f }, kEverest },
            { "Concrete block, painted", "concrete_block_painted", { 0.10f, 0.05f, 0.06f, 0.07f, 0.09f, 0.08f }, kEverest },
            { "Gypsum board 12.7 mm on studs", "gypsum_board_on_studs",
              { 0.29f, 0.10f, 0.05f, 0.04f, 0.07f, 0.09f }, kEverest },
            { "Heavy carpet on concrete", "heavy_carpet_on_concrete", { 0.02f, 0.06f, 0.14f, 0.37f, 0.60f, 0.65f }, kEverest },
            { "Glass, ordinary window", "glass_window", { 0.35f, 0.25f, 0.18f, 0.12f, 0.07f, 0.04f }, kEverest },
            { "Open doorway", "open_doorway", { 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f },
              "pseudo-material, alpha = 1 in every band; spec section 4.2" },
            { "Plaster on brick or concrete, painted", "plaster_on_masonry_painted",
              { 0.01f, 0.01f, 0.01f, 0.02f, 0.02f, 0.03f },
              "EN 12354-6:2003 Table B.1, \"concrete, plastered brick\", tabulated; read from the "
              "Russian adoption GOST R EN 12354-6-2012 as OCR text" },
            { "Glazed ceramic tile", "ceramic_tile", { 0.01f, 0.01f, 0.01f, 0.01f, 0.02f, 0.02f },
              "Everest, Master Handbook of Acoustics, as reprinted at asiproaudio.com/acoustic_IOI/101_13.htm "
              "in the 5th edition layout; tabulated, reprint only" },
            { "Parquet or laminate on concrete", "parquet_on_concrete",
              { 0.02f, 0.03f, 0.04f, 0.05f, 0.05f, 0.06f },
              "EN 12354-6:2003 Table B.1, \"hard floor coverings on heavy floor\", typical minimum, "
              "tabulated; read from the Russian adoption GOST R EN 12354-6-2012 as OCR text; "
              "Everest's parquet row is about twice as high" },
            { "Plywood panel, 3/8 in, on a frame", "wood_panel",
              { 0.28f, 0.22f, 0.17f, 0.09f, 0.10f, 0.11f },
              "Everest, Master Handbook of Acoustics, 4th ed., McGraw-Hill 2001, Appendix, p. 586; "
              "tabulated" },
            { "Window, double glazed", "window_double_glazed",
              { 0.12f, 0.08f, 0.05f, 0.04f, 0.03f, 0.02f },
              "EN 12354-6:2003 Table B.1, \"windows, glazed facade\", tabulated; read from the "
              "Russian adoption GOST R EN 12354-6-2012 as OCR text" },
            { "Wooden door", "wooden_door", { 0.14f, 0.10f, 0.08f, 0.08f, 0.08f, 0.08f },
              "EN 12354-6:2003 Table B.1, \"wooden doors\", tabulated; read from the Russian "
              "adoption GOST R EN 12354-6-2012 as OCR text" },
            { "Thin fitted carpet on concrete", "thin_carpet_on_concrete",
              { 0.02f, 0.03f, 0.06f, 0.15f, 0.30f, 0.40f },
              "EN 12354-6:2003 Table B.1, \"soft covering under 5 mm on heavy floor\", tabulated; "
              "read from the Russian adoption GOST R EN 12354-6-2012 as OCR text" },
            { "Curtain, heavy velour, 18 oz/sq yd, draped to half area", "curtain_heavy_velour",
              { 0.14f, 0.35f, 0.55f, 0.72f, 0.70f, 0.65f },
              "Everest, Master Handbook of Acoustics, 4th ed., McGraw-Hill 2001, Appendix, "
              "Selected absorption coefficients, p. 585; per square metre of wall or window the "
              "curtain covers; 5th ed. page not checked" },
            { "Curtain, light, under 0.2 kg/m2", "curtain_light",
              { 0.05f, 0.06f, 0.09f, 0.12f, 0.18f, 0.22f },
              "EN 12354-6:2003 Table B.1, \"light curtain\", typical minimum, tabulated; read from "
              "the Russian adoption GOST R EN 12354-6-2012 as OCR text; per square metre of wall or "
              "window covered; REPORT.md F6" },
        } };

        static_assert(kTable.size() == kMaterialCount,
                      "the absorption table must hold one row per MaterialId");

        /// @brief Holds the scattering coefficient of each Scattering word in enumeration order.
        constexpr std::array<float, 3> kScattering = { 0.10f, 0.25f, 0.40f };
    }

    const Material& material(MaterialId id) noexcept
    {
        return kTable[static_cast<std::size_t>(id)];
    }

    bool find_material(const char* key, MaterialId& id) noexcept
    {
        for (std::size_t row = 0; row < kMaterialCount; ++row)
        {
            if (std::strcmp(kTable[row].key, key) == 0)
            {
                id = static_cast<MaterialId>(row);
                return true;
            }
        }
        return false;
    }

    BandResponse reflection_factor(MaterialId id) noexcept
    {
        const Material& row = material(id);
        BandResponse response{};
        for (std::size_t band = 0; band < kBandCount; ++band)
        {
            response.gain[band] = static_cast<float>(
                std::sqrt(1.0 - static_cast<double>(row.absorption[band])));
        }
        return response;
    }

    float scattering_coefficient(Scattering scattering) noexcept
    {
        return kScattering[static_cast<std::size_t>(scattering)];
    }
}
