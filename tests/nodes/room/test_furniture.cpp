/**************************************************************************/
/* test_furniture.cpp                                                     */
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
 * @file test_furniture.cpp
 * @brief Tests for Furniture, the equivalent absorption area table of the room module.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <SushiDSP/nodes/room/furniture.hpp>

TEST(Unit_Dsp, FurnitureRowsAreNamedAndCited)
{
    EXPECT_EQ(SushiDSP::room::K_FURNITURE_COUNT, 7u);
    for (std::size_t id = 0; id < SushiDSP::room::K_FURNITURE_COUNT; ++id)
    {
        const SushiDSP::room::FurnitureItem& row =
            SushiDSP::room::furniture(static_cast<SushiDSP::room::FurnitureId>(id));
        ASSERT_NE(row.name, nullptr);
        EXPECT_GT(std::strlen(row.name), 0u);
        ASSERT_NE(row.citation, nullptr);
        EXPECT_GT(std::strlen(row.citation), 0u);
    }
}

TEST(Unit_Dsp, FurnitureAreasAreFiniteAndNonNegative)
{
    for (std::size_t id = 0; id < SushiDSP::room::K_FURNITURE_COUNT; ++id)
    {
        const SushiDSP::room::FurnitureItem& row =
            SushiDSP::room::furniture(static_cast<SushiDSP::room::FurnitureId>(id));
        for (std::size_t band = 0; band < SushiDSP::room::K_BAND_COUNT; ++band)
        {
            EXPECT_TRUE(std::isfinite(row.absorption_area[band]));
            EXPECT_GE(row.absorption_area[band], 0.0f);
        }
    }
}

TEST(Unit_Dsp, FurnitureLookupReturnsTheRowOfTheId)
{
    using SushiDSP::room::FurnitureId;
    const SushiDSP::room::FurnitureItem& sofa = SushiDSP::room::furniture(FurnitureId::SofaTwoSeat);
    EXPECT_STREQ(sofa.name, "Sofa, two-seat, upholstered");
    const SushiDSP::room::FurnitureItem& bed = SushiDSP::room::furniture(FurnitureId::BedDouble);
    EXPECT_STREQ(bed.name, "Bed, double, with mattress");
}

TEST(Unit_Dsp, FurnitureCarriesTheTranscribedAreas)
{
    using SushiDSP::room::FurnitureId;
    const std::array<float, 6> sofa = {2.6f, 3.5f, 3.4f, 3.6f, 3.2f, 3.8f};
    for (std::size_t band = 0; band < 6u; ++band)
    {
        EXPECT_FLOAT_EQ(SushiDSP::room::furniture(FurnitureId::SofaTwoSeat).absorption_area[band],
                        sofa[band]);
    }
}

TEST(Unit_Dsp, FurnitureCarriesTheHouseholdRowsExactly)
{
    using SushiDSP::room::FurnitureId;
    struct Expected
    {
            FurnitureId id;
            const char* key;
            std::array<float, 6> area;
    };
    const std::array<Expected, 6> expected = {{
        {FurnitureId::BedDouble, "bed_double", {2.05f, 2.51f, 3.32f, 3.51f, 3.64f, 3.42f}},
        {FurnitureId::Desk, "desk_wooden", {0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f}},
        {FurnitureId::WardrobeModule,
         "wardrobe_module",
         {1.22f, 0.31f, 0.26f, 0.14f, 0.22f, 0.18f}},
        {FurnitureId::ChairUpholstered,
         "chair_upholstered",
         {0.10f, 0.20f, 0.25f, 0.30f, 0.35f, 0.35f}},
        {FurnitureId::BookcaseFilled,
         "bookcase_filled",
         {0.57f, 0.57f, 0.57f, 0.57f, 0.57f, 0.57f}},
        {FurnitureId::Person, "person_in_room", {0.19f, 0.28f, 0.37f, 0.46f, 0.46f, 0.37f}},
    }};
    for (const Expected& row : expected)
    {
        const SushiDSP::room::FurnitureItem& found = SushiDSP::room::furniture(row.id);
        EXPECT_STREQ(found.key, row.key);
        for (std::size_t band = 0; band < 6u; ++band)
        {
            EXPECT_FLOAT_EQ(found.absorption_area[band], row.area[band]) << row.key;
        }
    }
}

TEST(Unit_Dsp, FurnitureKeepsItsRowsInEnumerationOrder)
{
    using SushiDSP::room::FurnitureId;
    EXPECT_EQ(static_cast<std::uint8_t>(FurnitureId::SofaTwoSeat), 0u);
    EXPECT_EQ(static_cast<std::uint8_t>(FurnitureId::BedDouble), 1u);
    EXPECT_EQ(static_cast<std::uint8_t>(FurnitureId::Person), 6u);
}

TEST(Unit_Dsp, FurnitureKeysAreNonEmptyAndUnique)
{
    for (std::size_t id = 0; id < SushiDSP::room::K_FURNITURE_COUNT; ++id)
    {
        const char* key =
            SushiDSP::room::furniture(static_cast<SushiDSP::room::FurnitureId>(id)).key;
        ASSERT_NE(key, nullptr);
        EXPECT_GT(std::strlen(key), 0u);
        for (std::size_t other = id + 1; other < SushiDSP::room::K_FURNITURE_COUNT; ++other)
        {
            EXPECT_STRNE(
                key,
                SushiDSP::room::furniture(static_cast<SushiDSP::room::FurnitureId>(other)).key);
        }
    }
}

TEST(Unit_Dsp, FurnitureFindReturnsTheRowsOwnIdForItsKey)
{
    for (std::size_t id = 0; id < SushiDSP::room::K_FURNITURE_COUNT; ++id)
    {
        SushiDSP::room::FurnitureId found =
            static_cast<SushiDSP::room::FurnitureId>(id == 0 ? 1 : 0);
        ASSERT_TRUE(SushiDSP::room::find_furniture(
            SushiDSP::room::furniture(static_cast<SushiDSP::room::FurnitureId>(id)).key, found));
        EXPECT_EQ(found, static_cast<SushiDSP::room::FurnitureId>(id));
    }
}

TEST(Unit_Dsp, FurnitureFindRefusesAnUnknownKeyAndLeavesTheIdUntouched)
{
    SushiDSP::room::FurnitureId found = SushiDSP::room::FurnitureId::SofaTwoSeat;
    EXPECT_FALSE(SushiDSP::room::find_furniture("no_such_row", found));
    EXPECT_EQ(found, SushiDSP::room::FurnitureId::SofaTwoSeat);
    EXPECT_FALSE(SushiDSP::room::find_furniture("", found));
    EXPECT_EQ(found, SushiDSP::room::FurnitureId::SofaTwoSeat);
}
