/**************************************************************************/
/* test_room_materials.cpp                                                */
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

TEST(Unit_Dsp, MaterialsShipSixRowsAndNoMore)
{
    EXPECT_EQ(SushiDSP::room::kMaterialCount, 6u);
    for (std::uint8_t id = 0; id < 6u; ++id)
    {
        const SushiDSP::room::Material& row =
            SushiDSP::room::material(static_cast<SushiDSP::room::MaterialId>(id));
        ASSERT_NE(row.citation, nullptr);
        EXPECT_GT(std::strlen(row.citation), 0u);
        ASSERT_NE(row.name, nullptr);
        EXPECT_GT(std::strlen(row.name), 0u);
    }
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
