/****************************************************************/
/* test_room_materials.cpp                                      */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_room_materials.cpp
 * @brief Tests for Materials, the absorption table of the room module.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "SushiDSP/room/Materials.hpp"

TEST(Unit_Dsp, MaterialsShipFifteenRowsAndNoMore)
{
    EXPECT_EQ(SushiDSP::room::kMaterialCount, 15u);
    for (std::size_t id = 0; id < SushiDSP::room::kMaterialCount; ++id)
    {
        const SushiDSP::room::Material& row =
            SushiDSP::room::material(static_cast<SushiDSP::room::MaterialId>(id));
        ASSERT_NE(row.citation, nullptr);
        EXPECT_GT(std::strlen(row.citation), 0u);
        ASSERT_NE(row.name, nullptr);
        EXPECT_GT(std::strlen(row.name), 0u);
        ASSERT_NE(row.key, nullptr);
        EXPECT_GT(std::strlen(row.key), 0u);
        for (std::size_t band = 0; band < SushiDSP::room::kBandCount; ++band)
        {
            EXPECT_GE(row.absorption[band], 0.0f);
            EXPECT_LE(row.absorption[band], 1.0f);
        }
    }
}

TEST(Unit_Dsp, MaterialsCarryTheFinishRowsExactly)
{
    using SushiDSP::room::MaterialId;
    struct Expected
    {
        MaterialId id;
        const char* key;
        std::array<float, 6> absorption;
    };
    const std::array<Expected, 7> expected = { {
        { MaterialId::PlasterOnMasonryPainted, "plaster_on_masonry_painted",
          { 0.01f, 0.01f, 0.01f, 0.02f, 0.02f, 0.03f } },
        { MaterialId::CeramicTile, "ceramic_tile", { 0.01f, 0.01f, 0.01f, 0.01f, 0.02f, 0.02f } },
        { MaterialId::ParquetOnConcrete, "parquet_on_concrete",
          { 0.02f, 0.03f, 0.04f, 0.05f, 0.05f, 0.06f } },
        { MaterialId::WoodPanel, "wood_panel", { 0.28f, 0.22f, 0.17f, 0.09f, 0.10f, 0.11f } },
        { MaterialId::WindowDoubleGlazed, "window_double_glazed",
          { 0.12f, 0.08f, 0.05f, 0.04f, 0.03f, 0.02f } },
        { MaterialId::WoodenDoor, "wooden_door", { 0.14f, 0.10f, 0.08f, 0.08f, 0.08f, 0.08f } },
        { MaterialId::ThinCarpetOnConcrete, "thin_carpet_on_concrete",
          { 0.02f, 0.03f, 0.06f, 0.15f, 0.30f, 0.40f } },
    } };
    for (const Expected& row : expected)
    {
        const SushiDSP::room::Material& found = SushiDSP::room::material(row.id);
        EXPECT_STREQ(found.key, row.key);
        for (std::size_t band = 0; band < 6u; ++band)
        {
            EXPECT_FLOAT_EQ(found.absorption[band], row.absorption[band]) << row.key;
        }
    }
}

TEST(Unit_Dsp, MaterialsCarryTheCurtainRowsExactly)
{
    using SushiDSP::room::MaterialId;
    struct Expected
    {
        MaterialId id;
        const char* key;
        const char* name;
        std::array<float, 6> absorption;
    };
    const std::array<Expected, 2> expected = { {
        { MaterialId::CurtainHeavyVelour, "curtain_heavy_velour",
          "Curtain, heavy velour, 18 oz/sq yd, draped to half area",
          { 0.14f, 0.35f, 0.55f, 0.72f, 0.70f, 0.65f } },
        { MaterialId::CurtainLight, "curtain_light", "Curtain, light, under 0.2 kg/m2",
          { 0.05f, 0.06f, 0.09f, 0.12f, 0.18f, 0.22f } },
    } };
    for (const Expected& row : expected)
    {
        const SushiDSP::room::Material& found = SushiDSP::room::material(row.id);
        EXPECT_STREQ(found.key, row.key);
        EXPECT_STREQ(found.name, row.name);
        for (std::size_t band = 0; band < 6u; ++band)
        {
            EXPECT_FLOAT_EQ(found.absorption[band], row.absorption[band]) << row.key;
        }
    }
}

TEST(Unit_Dsp, MaterialsAppendTheFinishRowsAfterTheOriginalSix)
{
    using SushiDSP::room::MaterialId;
    EXPECT_EQ(static_cast<std::uint8_t>(MaterialId::OpenDoorway), 5u);
    EXPECT_EQ(static_cast<std::uint8_t>(MaterialId::PlasterOnMasonryPainted), 6u);
    EXPECT_EQ(static_cast<std::uint8_t>(MaterialId::ThinCarpetOnConcrete), 12u);
    EXPECT_EQ(static_cast<std::uint8_t>(MaterialId::CurtainHeavyVelour), 13u);
    EXPECT_EQ(static_cast<std::uint8_t>(MaterialId::CurtainLight), 14u);
}

TEST(Unit_Dsp, MaterialsCarryTheTranscribedCoefficients)
{
    using SushiDSP::room::MaterialId;
    const std::array<float, 6> concrete = { 0.01f, 0.01f, 0.02f, 0.02f, 0.02f, 0.03f };
    const std::array<float, 6> block    = { 0.10f, 0.05f, 0.06f, 0.07f, 0.09f, 0.08f };
    const std::array<float, 6> gypsum   = { 0.29f, 0.10f, 0.05f, 0.04f, 0.07f, 0.09f };
    const std::array<float, 6> carpet   = { 0.02f, 0.06f, 0.14f, 0.37f, 0.60f, 0.65f };
    const std::array<float, 6> glass    = { 0.35f, 0.25f, 0.18f, 0.12f, 0.07f, 0.04f };
    for (std::size_t band = 0; band < 6u; ++band)
    {
        EXPECT_FLOAT_EQ(SushiDSP::room::material(MaterialId::ConcreteUnpainted).absorption[band],
                        concrete[band]);
        EXPECT_FLOAT_EQ(SushiDSP::room::material(MaterialId::ConcreteBlockPainted).absorption[band],
                        block[band]);
        EXPECT_FLOAT_EQ(SushiDSP::room::material(MaterialId::GypsumBoardOnStuds).absorption[band],
                        gypsum[band]);
        EXPECT_FLOAT_EQ(SushiDSP::room::material(MaterialId::HeavyCarpetOnConcrete).absorption[band],
                        carpet[band]);
        EXPECT_FLOAT_EQ(SushiDSP::room::material(MaterialId::GlassWindow).absorption[band],
                        glass[band]);
        EXPECT_FLOAT_EQ(SushiDSP::room::material(MaterialId::OpenDoorway).absorption[band], 1.0f);
    }
}

TEST(Unit_Dsp, MaterialsReflectionFactorIsTheSquareRootOfWhatIsLeft)
{
    const SushiDSP::room::BandResponse gypsum =
        SushiDSP::room::reflection_factor(SushiDSP::room::MaterialId::GypsumBoardOnStuds);
    EXPECT_NEAR(gypsum.gain[0], 0.842615f, 1e-6f);   // sqrt(1 - 0.29)
    EXPECT_NEAR(gypsum.gain[3], 0.979796f, 1e-6f);   // sqrt(1 - 0.04)
    const SushiDSP::room::BandResponse carpet =
        SushiDSP::room::reflection_factor(SushiDSP::room::MaterialId::HeavyCarpetOnConcrete);
    EXPECT_NEAR(carpet.gain[5], 0.591608f, 1e-6f);   // sqrt(1 - 0.65)
}

TEST(Unit_Dsp, MaterialsOpenDoorwayReflectsNothing)
{
    const SushiDSP::room::BandResponse open =
        SushiDSP::room::reflection_factor(SushiDSP::room::MaterialId::OpenDoorway);
    for (std::size_t band = 0; band < 6u; ++band)
    {
        EXPECT_FLOAT_EQ(open.gain[band], 0.0f);
    }
}

TEST(Unit_Dsp, MaterialsScatteringIsThreeWords)
{
    EXPECT_FLOAT_EQ(SushiDSP::room::scattering_coefficient(SushiDSP::room::Scattering::Flat),
                    0.10f);
    EXPECT_FLOAT_EQ(SushiDSP::room::scattering_coefficient(SushiDSP::room::Scattering::Textured),
                    0.25f);
    EXPECT_FLOAT_EQ(SushiDSP::room::scattering_coefficient(SushiDSP::room::Scattering::Cluttered),
                    0.40f);
}

TEST(Unit_Dsp, MaterialsKeysAreNonEmptyAndUnique)
{
    for (std::size_t id = 0; id < SushiDSP::room::kMaterialCount; ++id)
    {
        const char* key = SushiDSP::room::material(static_cast<SushiDSP::room::MaterialId>(id)).key;
        ASSERT_NE(key, nullptr);
        EXPECT_GT(std::strlen(key), 0u);
        for (std::size_t other = id + 1; other < SushiDSP::room::kMaterialCount; ++other)
        {
            EXPECT_STRNE(key,
                         SushiDSP::room::material(static_cast<SushiDSP::room::MaterialId>(other)).key);
        }
    }
}

TEST(Unit_Dsp, MaterialsFindReturnsTheRowsOwnIdForItsKey)
{
    for (std::size_t id = 0; id < SushiDSP::room::kMaterialCount; ++id)
    {
        SushiDSP::room::MaterialId found = static_cast<SushiDSP::room::MaterialId>(id == 0 ? 1 : 0);
        ASSERT_TRUE(SushiDSP::room::find_material(
            SushiDSP::room::material(static_cast<SushiDSP::room::MaterialId>(id)).key, found));
        EXPECT_EQ(found, static_cast<SushiDSP::room::MaterialId>(id));
    }
}

TEST(Unit_Dsp, MaterialsFindRefusesAnUnknownKeyAndLeavesTheIdUntouched)
{
    SushiDSP::room::MaterialId found = SushiDSP::room::MaterialId::GlassWindow;
    EXPECT_FALSE(SushiDSP::room::find_material("no_such_row", found));
    EXPECT_EQ(found, SushiDSP::room::MaterialId::GlassWindow);
    EXPECT_FALSE(SushiDSP::room::find_material("", found));
    EXPECT_EQ(found, SushiDSP::room::MaterialId::GlassWindow);
}
