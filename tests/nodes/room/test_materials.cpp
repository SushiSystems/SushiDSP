/**************************************************************************/
/* test_materials.cpp                                                     */
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
 * @file test_materials.cpp
 * @brief Tests for Materials, the absorption table of the room module.
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

#include <SushiDSP/nodes/room/materials.hpp>

#include "support/tolerance.hpp"

namespace
{
    using SushiDSP::room::MaterialId;
    using SushiDSP::room::Scattering;

    /// @brief Holds the transcribed row of one material: the key a preset spells and the six octave
    /// absorption coefficients.
    struct TranscribedRow
    {
            MaterialId id;
            const char* key;
            std::array<float, 6> absorption;
    };

    const std::array<TranscribedRow, SushiDSP::room::K_MATERIAL_COUNT> kRows = {{
        {MaterialId::ConcreteUnpainted, "concrete_unpainted", {0.01f, 0.01f, 0.02f, 0.02f, 0.02f, 0.03f}},
        {MaterialId::ConcreteBlockPainted, "concrete_block_painted", {0.10f, 0.05f, 0.06f, 0.07f, 0.09f, 0.08f}},
        {MaterialId::GypsumBoardOnStuds, "gypsum_board_on_studs", {0.29f, 0.10f, 0.05f, 0.04f, 0.07f, 0.09f}},
        {MaterialId::HeavyCarpetOnConcrete, "heavy_carpet_on_concrete", {0.02f, 0.06f, 0.14f, 0.37f, 0.60f, 0.65f}},
        {MaterialId::GlassWindow, "glass_window", {0.35f, 0.25f, 0.18f, 0.12f, 0.07f, 0.04f}},
        {MaterialId::OpenDoorway, "open_doorway", {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}},
        {MaterialId::PlasterOnMasonryPainted, "plaster_on_masonry_painted", {0.01f, 0.01f, 0.01f, 0.02f, 0.02f, 0.03f}},
        {MaterialId::CeramicTile, "ceramic_tile", {0.01f, 0.01f, 0.01f, 0.01f, 0.02f, 0.02f}},
        {MaterialId::ParquetOnConcrete, "parquet_on_concrete", {0.02f, 0.03f, 0.04f, 0.05f, 0.05f, 0.06f}},
        {MaterialId::WoodPanel, "wood_panel", {0.28f, 0.22f, 0.17f, 0.09f, 0.10f, 0.11f}},
        {MaterialId::WindowDoubleGlazed, "window_double_glazed", {0.12f, 0.08f, 0.05f, 0.04f, 0.03f, 0.02f}},
        {MaterialId::WoodenDoor, "wooden_door", {0.14f, 0.10f, 0.08f, 0.08f, 0.08f, 0.08f}},
        {MaterialId::ThinCarpetOnConcrete, "thin_carpet_on_concrete", {0.02f, 0.03f, 0.06f, 0.15f, 0.30f, 0.40f}},
        {MaterialId::CurtainHeavyVelour, "curtain_heavy_velour", {0.14f, 0.35f, 0.55f, 0.72f, 0.70f, 0.65f}},
        {MaterialId::CurtainLight, "curtain_light", {0.05f, 0.06f, 0.09f, 0.12f, 0.18f, 0.22f}},
    }};

    /// @brief Names a row case after its key, which holds only lower-case letters, digits and underscores.
    std::string row_name(const ::testing::TestParamInfo<TranscribedRow>& info)
    {
        return info.param.key;
    }

    /// @brief Returns whether every coefficient of the row lies in [0, 1].
    bool absorption_is_a_fraction(const SushiDSP::room::Material& row)
    {
        return std::all_of(row.absorption.begin(), row.absorption.end(), [](float a) { return a >= 0.0f && a <= 1.0f; });
    }

    /// @brief Returns how many distinct keys the shipped table carries.
    std::size_t distinct_key_count()
    {
        std::set<std::string> keys;
        for (const TranscribedRow& row : kRows)
        {
            keys.insert(SushiDSP::room::material(row.id).key);
        }
        return keys.size();
    }

    /// @brief Holds the reflection factor of one material at one band, from the formula sqrt(1 - a).
    struct ReflectionCase
    {
            const char* name;
            MaterialId id;
            std::size_t band;
            double absorption;
    };

    const std::array<ReflectionCase, 3> kReflectionCases = {{
        {"GypsumAt125Hz", MaterialId::GypsumBoardOnStuds, 0, 0.29},
        {"GypsumAt1kHz", MaterialId::GypsumBoardOnStuds, 3, 0.04},
        {"CarpetAt4kHz", MaterialId::HeavyCarpetOnConcrete, 5, 0.65},
    }};

    /// @brief Names a reflection case after the material and the band.
    std::string reflection_name(const ::testing::TestParamInfo<ReflectionCase>& info)
    {
        return info.param.name;
    }

    /// @brief Holds the coefficient one scattering word names.
    struct ScatteringCase
    {
            const char* name;
            Scattering word;
            float coefficient;
    };

    const std::array<ScatteringCase, 3> kScatteringCases = {{
        {"Flat", Scattering::Flat, 0.10f},
        {"Textured", Scattering::Textured, 0.25f},
        {"Cluttered", Scattering::Cluttered, 0.40f},
    }};

    /// @brief Names a scattering case after the word.
    std::string scattering_name(const ::testing::TestParamInfo<ScatteringCase>& info)
    {
        return info.param.name;
    }
} // namespace

class Small_MaterialsRow : public ::testing::TestWithParam<TranscribedRow>
{
};

TEST_P(Small_MaterialsRow, MaterialsCarryTheTranscribedKeyAndAbsorptionExactly)
{
    const SushiDSP::room::Material& found = SushiDSP::room::material(GetParam().id);

    EXPECT_STREQ(found.key, GetParam().key);
    EXPECT_EQ(found.absorption, GetParam().absorption);
}

TEST_P(Small_MaterialsRow, MaterialsAreNamedAndCited)
{
    const SushiDSP::room::Material& found = SushiDSP::room::material(GetParam().id);

    ASSERT_NE(found.name, nullptr);
    ASSERT_NE(found.citation, nullptr);
    EXPECT_FALSE(std::string_view(found.name).empty());
    EXPECT_FALSE(std::string_view(found.citation).empty());
}

TEST_P(Small_MaterialsRow, MaterialsAbsorbAFractionOfTheEnergyInEveryBand)
{
    EXPECT_TRUE(absorption_is_a_fraction(SushiDSP::room::material(GetParam().id)));
}

TEST_P(Small_MaterialsRow, MaterialsFindReturnsTheRowsOwnIdForItsKey)
{
    MaterialId found = GetParam().id == MaterialId::ConcreteUnpainted ? MaterialId::GlassWindow : MaterialId::ConcreteUnpainted;

    const bool was_found = SushiDSP::room::find_material(GetParam().key, found);

    EXPECT_TRUE(was_found);
    EXPECT_EQ(found, GetParam().id);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_MaterialsRow, ::testing::ValuesIn(kRows), row_name);

TEST(Small_Materials, MaterialsCarryTheCurtainNames)
{
    EXPECT_STREQ(SushiDSP::room::material(MaterialId::CurtainHeavyVelour).name,
                 "Curtain, heavy velour, 18 oz/sq yd, draped to half area");
    EXPECT_STREQ(SushiDSP::room::material(MaterialId::CurtainLight).name, "Curtain, light, under 0.2 kg/m2");
}

class Small_MaterialsReflection : public ::testing::TestWithParam<ReflectionCase>
{
};

TEST_P(Small_MaterialsReflection, MaterialsReflectionFactorIsTheSquareRootOfWhatIsLeft)
{
    const SushiDSP::room::BandResponse response = SushiDSP::room::reflection_factor(GetParam().id);
    const double expected = std::sqrt(1.0 - GetParam().absorption);

    // The function casts the root once to float, from an absorption stored as a float, which moves the root by under half a roundoff: three units bound the cast, the input and the double root.
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<float>(response.gain[GetParam().band], expected, 3, 1.0)));
}

INSTANTIATE_TEST_SUITE_P(Cases, Small_MaterialsReflection, ::testing::ValuesIn(kReflectionCases), reflection_name);

TEST(Small_Materials, MaterialsOpenDoorwayReflectsNothing)
{
    const SushiDSP::room::BandResponse open = SushiDSP::room::reflection_factor(MaterialId::OpenDoorway);

    EXPECT_EQ(open.gain, (std::array<float, SushiDSP::room::K_BAND_COUNT>{}));
}

class Small_MaterialsScattering : public ::testing::TestWithParam<ScatteringCase>
{
};

TEST_P(Small_MaterialsScattering, MaterialsScatteringCoefficientIsTheCitedValueOfItsWord)
{
    EXPECT_EQ(SushiDSP::room::scattering_coefficient(GetParam().word), GetParam().coefficient);
}

INSTANTIATE_TEST_SUITE_P(Words, Small_MaterialsScattering, ::testing::ValuesIn(kScatteringCases), scattering_name);

TEST(Small_Materials, MaterialsKeysAreUnique)
{
    EXPECT_EQ(distinct_key_count(), SushiDSP::room::K_MATERIAL_COUNT);
}

TEST(Small_Materials, MaterialsFindRefusesAnUnknownKeyAndLeavesTheIdUntouched)
{
    MaterialId found = MaterialId::GlassWindow;
    EXPECT_FALSE(SushiDSP::room::find_material("no_such_row", found));
    EXPECT_EQ(found, MaterialId::GlassWindow);
    EXPECT_FALSE(SushiDSP::room::find_material("", found));
    EXPECT_EQ(found, MaterialId::GlassWindow);
}
