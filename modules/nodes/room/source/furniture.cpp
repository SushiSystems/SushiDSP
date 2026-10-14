/**************************************************************************/
/* furniture.cpp                                                          */
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
 * @file furniture.cpp
 * @brief The transcribed furniture rows and their lookup.
 * @author Mustafa Garip & Sushi Systems
 */

#include <SushiDSP/nodes/room/furniture.hpp>

#include <cstring>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds the whole shipped table, one row per FurnitureId in enumeration order.
        constexpr std::array<FurnitureItem, K_FURNITURE_COUNT> K_TABLE = {{
            {"Sofa, two-seat, upholstered",
             "sofa_two_seat",
             {2.6f, 3.5f, 3.4f, 3.6f, 3.2f, 3.8f},
             "Offecct, Lucy sofa 2-seater, reverberation-room report to ISO 354 and SS 25269, "
             "octave values in m2 sabine, sofa standing on the floor",
             {1.6f, 0.85f, 0.8f}},
            {"Bed, double, with mattress",
             "bed_double",
             {2.05f, 2.51f, 3.32f, 3.51f, 3.64f, 3.42f},
             "Derived: 1.22 times the measured single bed of Tsuchiya, Toda Corporation technical "
             "report, JIS A 1409, curve B1; 1.22 is the mattress perimeter ratio 7.2 m over 5.88 m "
             "for a 1.6 x 2.0 m mattress; no bedding was measured",
             {1.6f, 2.0f, 0.5f}},
            {"Desk, wooden",
             "desk_wooden",
             {0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f},
             "Derived: the Korean apartment study, Jeong, Lee and Jeon, Trans. Korean Soc. Noise "
             "Vib. Eng. 25(4), 2015, gives a desk step of 0.0131 m2 per m3 of room, about 0.4 m2 "
             "at 30 m3; EN 12354-6 formula 4 (0.81 m2) is an upper bound; no measured desk found; "
             "frequency independent by construction",
             {1.4f, 0.7f, 0.75f}},
            {"Wardrobe, closed, one 0.59 m wide module",
             "wardrobe_module",
             {1.22f, 0.31f, 0.26f, 0.14f, 0.22f, 0.18f},
             "Measured: Tsuchiya, Toda Corporation technical report, JIS A 1409, curve T11, door "
             "type, 0.59 x 0.515 x 1.88 m, empty; the 125 Hz value is the cabinet's panel "
             "resonance; scaling to a wider wardrobe by module count is unverified",
             {0.59f, 0.515f, 1.88f}},
            {"Chair, upholstered",
             "chair_upholstered",
             {0.10f, 0.20f, 0.25f, 0.30f, 0.35f, 0.35f},
             "EN 12354-6:2003 Table C.1, \"upholstered chair\", tabulated; read from the Russian "
             "adoption GOST R EN 12354-6-2012 as OCR text; a padded office or theatre chair, a "
             "lower bound for a domestic armchair",
             {0.6f, 0.6f, 0.9f}},
            {"Bookcase, filled with books",
             "bookcase_filled",
             {0.57f, 0.57f, 0.57f, 0.57f, 0.57f, 0.57f},
             "Derived: EN 12354-6:2003 formula 4, A = V^(2/3), V = 0.8 x 0.3 x 1.8 m3; the weakest "
             "row, the formula is for hard objects",
             {0.8f, 0.3f, 1.8f}},
            {"Person, in a room",
             "person_in_room",
             {0.19f, 0.28f, 0.37f, 0.46f, 0.46f, 0.37f},
             "Everest, Master Handbook of Acoustics, 5th ed. table as reprinted at "
             "asiproaudio.com/acoustic_IOI/101_13.htm: 2, 3, 4, 5, 5, 4 sabins in square feet "
             "times 0.0929; tabulated, reprint only",
             {0.5f, 0.3f, 1.7f}},
        }};

        static_assert(K_TABLE.size() == K_FURNITURE_COUNT,
                      "the furniture table must hold one row per FurnitureId");
    } // namespace

    const FurnitureItem& furniture(FurnitureId id) noexcept
    {
        return K_TABLE[static_cast<std::size_t>(id)];
    }

    bool find_furniture(const char* key, FurnitureId& id) noexcept
    {
        for (std::size_t row = 0; row < K_FURNITURE_COUNT; ++row)
        {
            if (std::strcmp(K_TABLE[row].key, key) == 0)
            {
                id = static_cast<FurnitureId>(row);
                return true;
            }
        }
        return false;
    }
} // namespace SushiDSP::room
