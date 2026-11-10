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
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_furniture.cpp
 * @brief Tests for Furniture, the equivalent absorption area table of the room module.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <set>
#include <string>
#include <string_view>

#include <SushiDSP/nodes/room/furniture.hpp>

namespace
{
    using SushiDSP::room::FurnitureId;

    /// @brief Holds the transcribed row of one furnishing, as the shipped table spells it.
    struct TranscribedRow
    {
            FurnitureId id;
            const char* key;
            std::array<float, 6> area;
    };

    const std::array<TranscribedRow, SushiDSP::room::K_FURNITURE_COUNT> kTranscribedRows = {{
        {FurnitureId::SofaTwoSeat, "sofa_two_seat", {2.6f, 3.5f, 3.4f, 3.6f, 3.2f, 3.8f}},
        {FurnitureId::BedDouble, "bed_double", {2.05f, 2.51f, 3.32f, 3.51f, 3.64f, 3.42f}},
        {FurnitureId::Desk, "desk_wooden", {0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f}},
        {FurnitureId::WardrobeModule, "wardrobe_module", {1.22f, 0.31f, 0.26f, 0.14f, 0.22f, 0.18f}},
        {FurnitureId::ChairUpholstered, "chair_upholstered", {0.10f, 0.20f, 0.25f, 0.30f, 0.35f, 0.35f}},
        {FurnitureId::BookcaseFilled, "bookcase_filled", {0.57f, 0.57f, 0.57f, 0.57f, 0.57f, 0.57f}},
        {FurnitureId::Person, "person_in_room", {0.19f, 0.28f, 0.37f, 0.46f, 0.46f, 0.37f}},
    }};

    /// @brief Names a parameterised case after its row key, which holds only letters and underscores.
    std::string row_name(const ::testing::TestParamInfo<TranscribedRow>& info)
    {
        return info.param.key;
    }

    /// @brief Returns whether every area of the row is a number and not negative.
    bool areas_are_finite_and_non_negative(const SushiDSP::room::FurnitureItem& row)
    {
        return std::all_of(row.absorption_area.begin(), row.absorption_area.end(),
                           [](float area) { return std::isfinite(area) && area >= 0.0f; });
    }

    /// @brief Returns how many distinct keys the shipped table carries.
    std::size_t distinct_key_count()
    {
        std::set<std::string> keys;
        for (const TranscribedRow& row : kTranscribedRows)
        {
            keys.insert(SushiDSP::room::furniture(row.id).key);
        }
        return keys.size();
    }
} // namespace

class Small_FurnitureRow : public ::testing::TestWithParam<TranscribedRow>
{
};

TEST_P(Small_FurnitureRow, CarriesTheTranscribedKeyAndAreasExactly)
{
    const SushiDSP::room::FurnitureItem& found = SushiDSP::room::furniture(GetParam().id);

    EXPECT_STREQ(found.key, GetParam().key);
    EXPECT_EQ(found.absorption_area, GetParam().area);
}

TEST_P(Small_FurnitureRow, IsNamedAndCited)
{
    const SushiDSP::room::FurnitureItem& found = SushiDSP::room::furniture(GetParam().id);

    ASSERT_NE(found.name, nullptr);
    ASSERT_NE(found.citation, nullptr);
    EXPECT_FALSE(std::string_view(found.name).empty());
    EXPECT_FALSE(std::string_view(found.citation).empty());
}

TEST_P(Small_FurnitureRow, HasFiniteAndNonNegativeAreas)
{
    EXPECT_TRUE(areas_are_finite_and_non_negative(SushiDSP::room::furniture(GetParam().id)));
}

TEST_P(Small_FurnitureRow, IsFoundBackByItsOwnKey)
{
    FurnitureId found = GetParam().id == FurnitureId::SofaTwoSeat ? FurnitureId::Person : FurnitureId::SofaTwoSeat;

    const bool was_found = SushiDSP::room::find_furniture(GetParam().key, found);

    EXPECT_TRUE(was_found);
    EXPECT_EQ(found, GetParam().id);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_FurnitureRow, ::testing::ValuesIn(kTranscribedRows), row_name);

TEST(Small_Furniture, FurnitureLookupReturnsTheRowOfTheId)
{
    const SushiDSP::room::FurnitureItem& sofa = SushiDSP::room::furniture(FurnitureId::SofaTwoSeat);
    EXPECT_STREQ(sofa.name, "Sofa, two-seat, upholstered");
    const SushiDSP::room::FurnitureItem& bed = SushiDSP::room::furniture(FurnitureId::BedDouble);
    EXPECT_STREQ(bed.name, "Bed, double, with mattress");
}

TEST(Small_Furniture, FurnitureKeysAreUnique)
{
    EXPECT_EQ(distinct_key_count(), SushiDSP::room::K_FURNITURE_COUNT);
}

TEST(Small_Furniture, FurnitureFindRefusesAnUnknownKeyAndLeavesTheIdUntouched)
{
    FurnitureId found = FurnitureId::SofaTwoSeat;
    EXPECT_FALSE(SushiDSP::room::find_furniture("no_such_row", found));
    EXPECT_EQ(found, FurnitureId::SofaTwoSeat);
    EXPECT_FALSE(SushiDSP::room::find_furniture("", found));
    EXPECT_EQ(found, FurnitureId::SofaTwoSeat);
}
