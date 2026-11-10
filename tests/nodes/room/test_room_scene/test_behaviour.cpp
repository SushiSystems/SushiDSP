/**************************************************************************/
/* test_behaviour.cpp                                                     */
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
 * @file test_behaviour.cpp
 * @brief Tests for RoomScene, the shoebox: its geometry, its validity checks, its patches, its
 * decay bound and its built-room loss.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>
#include <string>

#include <SushiDSP/nodes/room/band_response.hpp>
#include <SushiDSP/nodes/room/built_room_loss.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>
#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>

#include <support/tolerance.hpp>

namespace
{
    using SushiDSP::room::K_BAND_COUNT;
    using SushiDSP::room::MaterialId;
    using SushiDSP::room::RoomScene;
    using SushiDSP::room::Surface;

    /// @brief Holds one value per octave band, 125 Hz to 4 kHz.
    using Bands = std::array<double, K_BAND_COUNT>;

    /// @brief Counts the rounded operations on the longest path to a mean absorption: a face's
    /// weighted sum of at most three terms, its division, the sum over six faces and the final
    /// division, with a margin for the order the code adds in. The table's coefficients are
    /// rounded to float once, and the expected values below use the same rounded coefficients.
    constexpr std::size_t K_MEAN_OPERATIONS = 16;

    /// @brief Counts the rounded operations the decay floor 1 - exp(-x) is held to. The
    /// subtraction amplifies the rounding of the exponential by 1 / x, and x is at least 0.0058
    /// for the smallest room below, an amplification of 172 units of roundoff.
    constexpr std::size_t K_FLOOR_OPERATIONS = 32;

    /// @brief Counts the rounded operations of a face's absorption plus the built-room loss.
    constexpr std::size_t K_FACE_OPERATIONS = 8;

    /// @brief Checks every band of a computed array against an expected one.
    void expect_bands_within(const Bands& computed, const Bands& expected, std::size_t operations)
    {
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<double>(
                computed[band], expected[band], operations)))
                << "band " << band;
        }
    }

    /// @brief Builds the cube of one material and edge, with nothing in it.
    RoomScene cube_of(MaterialId material, double edge)
    {
        RoomScene cube;
        cube.dimensions = {edge, edge, edge};
        cube.surfaces.fill(material);
        return cube;
    }

    /// @brief Builds the 12 m cube whose four walls are open doorways over carpet.
    RoomScene dead_room()
    {
        RoomScene dead = cube_of(MaterialId::HeavyCarpetOnConcrete, 12.0);
        dead.patches[0] = {Surface::MinusX, MaterialId::OpenDoorway, 144.0};
        dead.patches[1] = {Surface::PlusX, MaterialId::OpenDoorway, 144.0};
        dead.patches[2] = {Surface::MinusY, MaterialId::OpenDoorway, 144.0};
        dead.patches[3] = {Surface::PlusY, MaterialId::OpenDoorway, 144.0};
        dead.patch_count = 4;
        dead.source_position = {3.0, 4.0, 1.0};
        dead.listener_position = {7.0, 5.0, 1.5};
        dead.microphone_position = {7.0, 5.0, 1.5};
        return dead;
    }

    /// @brief Builds the 3.0 x 2.5 x 2.4 m unpainted concrete room.
    RoomScene small_concrete_room()
    {
        RoomScene room;
        room.dimensions = {3.0, 2.5, 2.4};
        room.surfaces.fill(MaterialId::ConcreteUnpainted);
        room.source_position = {0.3, 0.3, 0.3};
        room.listener_position = {1.5, 1.25, 1.2};
        room.microphone_position = {1.5, 1.25, 1.2};
        return room;
    }

    /// @brief Returns a face's absorption with the built-room loss added band by band.
    Bands plus_built_room_loss(const Bands& face)
    {
        Bands result{};
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            result[band] = face[band] + SushiDSP::room::K_BUILT_ROOM_LOSS[band];
        }
        return result;
    }

    /// @brief Builds the 10 m cube of gypsum board on studs.
    RoomScene gypsum_cube()
    {
        return cube_of(MaterialId::GypsumBoardOnStuds, 10.0);
    }

    /// @brief Builds the scene a fresh slot holds.
    RoomScene fresh_scene()
    {
        return RoomScene{};
    }

    /// @brief Holds a scene, the mean absorption its areas and table rows give, and the decay
    /// floor of its volume over its area.
    struct ReferenceRoom
    {
            const char* name;
            RoomScene (*build)();
            Bands mean;
            double floor;
    };

    /// @brief Holds the reference rooms. Every value is derived in
    /// tests/nodes/room/test_room_scene/README.md.
    const std::array<ReferenceRoom, 7> K_REFERENCE_ROOMS = {{
        {"Fresh",
         fresh_scene,
         {0.2238775446098678, 0.09020408242940903, 0.07204081703509603, 0.12081632702326288,
          0.19979592443120722, 0.22714285400449014},
         0.008180640655788274},
        {"Garage",
         SushiDSP::test::garage_scene,
         {0.0706122446881265, 0.0706122446881265, 0.07999999958033464, 0.07999999958033464,
          0.07999999958033464, 0.08938775447254278},
         0.008180640655788274},
        {"Bedroom",
         SushiDSP::test::bedroom_scene,
         {0.2349253664933034, 0.09500000086515697, 0.07171641876782055, 0.11074626890581045,
          0.18074627387434689, 0.2058955201350931},
         0.007262553994486387},
        {"BlockRoom",
         SushiDSP::test::block_room_scene,
         {0.10000000149011612, 0.05000000074505806, 0.059999998658895486, 0.07000000029802322,
          0.09000000357627869, 0.07999999821186066},
         0.008143858539927828},
        {"DeadRoom",
         dead_room,
         {0.6733333331843218, 0.6866666662196318, 0.7133333335320154, 0.7900000015894572,
          0.8666666746139526, 0.8833333253860474},
         0.026476518082418576},
        {"SmallConcreteRoom",
         small_concrete_room,
         {0.009999999776482582, 0.009999999776482582, 0.019999999552965164, 0.019999999552965164,
          0.019999999552965164, 0.029999999329447746},
         0.005816352478816955},
        {"GypsumCube",
         gypsum_cube,
         {0.29f, 0.10f, 0.05f, 0.04f, 0.07f, 0.09f},
         0.022112954592812484},
    }};

    /// @brief Names a reference room case after the room.
    std::string reference_room_name(const ::testing::TestParamInfo<ReferenceRoom>& info)
    {
        return info.param.name;
    }

    /// @brief Holds the edge of a concrete cube and whether the decay bound admits it.
    struct CubeEdge
    {
            const char* name;
            double edge_m;
            bool valid;
    };

    /// @brief Holds an edge under and one over the 4.495 m where the bound is crossed.
    const std::array<CubeEdge, 2> K_CUBE_EDGES = {{{"UnderTheCrossing", 4.4, true},
                                                   {"OverTheCrossing", 4.6, false}}};

    /// @brief Names a cube edge case.
    std::string cube_edge_name(const ::testing::TestParamInfo<CubeEdge>& info)
    {
        return info.param.name;
    }

    /// @brief Holds an area a patch must be refused for.
    struct BadArea
    {
            const char* name;
            double area_m2;
    };

    /// @brief Holds the four areas that are not a positive finite number.
    const std::array<BadArea, 4> K_BAD_AREAS = {
        {{"Zero", 0.0},
         {"Negative", -1.0},
         {"NotANumber", std::numeric_limits<double>::quiet_NaN()},
         {"Infinity", std::numeric_limits<double>::infinity()}}};

    /// @brief Names a bad area case.
    std::string bad_area_name(const ::testing::TestParamInfo<BadArea>& info)
    {
        return info.param.name;
    }

    /// @brief Holds one face of the default scene.
    struct Face
    {
            const char* name;
            Surface surface;
    };

    /// @brief Holds the six faces.
    const std::array<Face, 6> K_FACES = {{{"MinusX", Surface::MinusX},
                                          {"PlusX", Surface::PlusX},
                                          {"MinusY", Surface::MinusY},
                                          {"PlusY", Surface::PlusY},
                                          {"Floor", Surface::Floor},
                                          {"Ceiling", Surface::Ceiling}}};

    /// @brief Names a face case.
    std::string face_name(const ::testing::TestParamInfo<Face>& info)
    {
        return info.param.name;
    }
} // namespace

class Small_RoomSceneReferenceRoom : public ::testing::TestWithParam<ReferenceRoom>
{
};

class Small_RoomSceneCubeEdge : public ::testing::TestWithParam<CubeEdge>
{
};

class Small_RoomSceneBadArea : public ::testing::TestWithParam<BadArea>
{
};

class Small_RoomSceneBuiltRoomLoss : public ::testing::TestWithParam<Face>
{
};

TEST(Small_RoomScene, RoomSceneComputesTheGaragesVolumeAndArea)
{
    const RoomScene garage = SushiDSP::test::garage_scene();
    EXPECT_EQ(garage.volume_m3(), 60.0);
    EXPECT_EQ(garage.surface_area_m2(), 98.0);
}

TEST_P(Small_RoomSceneReferenceRoom, RoomSceneAcceptsTheReferenceRoom)
{
    std::string reason;
    EXPECT_TRUE(GetParam().build().is_valid(reason)) << reason;
}

TEST_P(Small_RoomSceneReferenceRoom, RoomSceneWeightsEachPatchAndFaceByItsArea)
{
    // Every row is derived in tests/nodes/room/test_room_scene/README.md.
    expect_bands_within(GetParam().build().mean_absorption(), GetParam().mean, K_MEAN_OPERATIONS);
}

TEST_P(Small_RoomSceneReferenceRoom, RoomSceneFloorIsEyringInvertedAtTheDecayBound)
{
    // The floor is 1 - exp(-0.161 V / (12 S)); see tests/nodes/room/test_room_scene/README.md.
    const double floor = GetParam().build().min_mean_absorption();
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<double>(floor, GetParam().floor, K_FLOOR_OPERATIONS)));
}

INSTANTIATE_TEST_SUITE_P(Rooms, Small_RoomSceneReferenceRoom, ::testing::ValuesIn(K_REFERENCE_ROOMS),
                         reference_room_name);

TEST(Small_RoomScene, RoomSceneRefusesADimensionOutsideItsBounds)
{
    RoomScene too_small = SushiDSP::test::garage_scene();
    too_small.dimensions.x = 1.0;
    std::string reason;
    EXPECT_FALSE(too_small.is_valid(reason));
    EXPECT_NE(reason.find('x'), std::string::npos);
    EXPECT_NE(reason.find("1.5"), std::string::npos);
    EXPECT_NE(reason.find("12"), std::string::npos);

    RoomScene too_large = SushiDSP::test::garage_scene();
    too_large.dimensions.x = 15.0;
    reason.clear();
    EXPECT_FALSE(too_large.is_valid(reason));
    EXPECT_NE(reason.find('x'), std::string::npos);
    EXPECT_NE(reason.find("1.5"), std::string::npos);
    EXPECT_NE(reason.find("12"), std::string::npos);
}

TEST(Small_RoomScene, RoomSceneRefusesAPoseOutsideTheRoom)
{
    RoomScene behind_wall = SushiDSP::test::garage_scene();
    behind_wall.source_position.x = -0.1;
    std::string reason;
    EXPECT_FALSE(behind_wall.is_valid(reason));
    EXPECT_NE(reason.find("source"), std::string::npos);
    EXPECT_NE(reason.find('x'), std::string::npos);

    RoomScene past_wall = SushiDSP::test::garage_scene();
    past_wall.source_position.x = 6.1;
    reason.clear();
    EXPECT_FALSE(past_wall.is_valid(reason));
    EXPECT_NE(reason.find("source"), std::string::npos);
    EXPECT_NE(reason.find('x'), std::string::npos);
}

TEST(Small_RoomScene, RoomSceneRefusesAPatchLargerThanItsFace)
{
    RoomScene garage = SushiDSP::test::garage_scene();
    garage.patches[0] = {Surface::PlusX, MaterialId::OpenDoorway, 12.0};
    std::string reason;
    EXPECT_FALSE(garage.is_valid(reason));
    EXPECT_NE(reason.find("12"), std::string::npos);
    EXPECT_NE(reason.find("10"), std::string::npos);
}

TEST(Small_RoomScene, RoomSceneRefusesPatchesThatTogetherOverfillAFace)
{
    RoomScene garage = SushiDSP::test::garage_scene();
    garage.patch_count = 2;
    garage.patches[0] = {Surface::PlusY, MaterialId::GlassWindow, 8.0};
    garage.patches[1] = {Surface::PlusY, MaterialId::WoodenDoor, 8.0};
    std::string reason;
    EXPECT_FALSE(garage.is_valid(reason));
    EXPECT_NE(reason.find("total"), std::string::npos);

    garage.patches[1].surface = Surface::MinusY;
    EXPECT_TRUE(garage.is_valid(reason)) << reason;
}

TEST_P(Small_RoomSceneBadArea, RoomSceneRefusesAPatchWhoseAreaIsNotPositiveAndFinite)
{
    RoomScene garage = SushiDSP::test::garage_scene();
    garage.patches[0] = {Surface::PlusX, MaterialId::GlassWindow, GetParam().area_m2};
    std::string reason;
    EXPECT_FALSE(garage.is_valid(reason));
    EXPECT_NE(reason.find("non-positive area"), std::string::npos) << reason;
}

INSTANTIATE_TEST_SUITE_P(Areas, Small_RoomSceneBadArea, ::testing::ValuesIn(K_BAD_AREAS), bad_area_name);

TEST(Small_RoomScene, RoomSceneRefusesMorePatchesThanItHolds)
{
    RoomScene garage = SushiDSP::test::garage_scene();
    garage.patches[0] = {Surface::PlusX, MaterialId::GlassWindow, 1.0};
    garage.patch_count = RoomScene::K_MAX_PATCHES + 1;
    std::string reason;
    EXPECT_FALSE(garage.is_valid(reason));
    EXPECT_NE(reason.find("patches"), std::string::npos);
}

TEST(Small_RoomScene, RoomSceneRefusalNamesTheFailingBandItsFloorAndTheBound)
{
    // The concrete cube rings 32.04 s against the 12 s bound. See tests/nodes/room/test_room_scene/README.md.
    std::string reason;
    EXPECT_FALSE(cube_of(MaterialId::ConcreteUnpainted, 12.0).is_valid(reason));
    EXPECT_NE(reason.find("0.010000"), std::string::npos) << reason;
    EXPECT_NE(reason.find("125"), std::string::npos) << reason;
    EXPECT_NE(reason.find("0.026477"), std::string::npos) << reason;
    EXPECT_NE(reason.find("12 s"), std::string::npos) << reason;
}

TEST(Small_RoomScene, RoomSceneRefusesARoomWhoseLowestBandAloneIsUnderTheFloor)
{
    // Carpet clears the floor from 250 Hz up and misses it at 125 Hz alone. See tests/nodes/room/test_room_scene/README.md.
    std::string reason;
    EXPECT_FALSE(cube_of(MaterialId::HeavyCarpetOnConcrete, 12.0).is_valid(reason));
    EXPECT_NE(reason.find("0.020000 at 125 Hz"), std::string::npos) << reason;
}

TEST_P(Small_RoomSceneCubeEdge, RoomSceneAdmitsAConcreteCubeOnlyUnderTheBound)
{
    // A bare concrete cube crosses the bound at 4.495 m. See tests/nodes/room/test_room_scene/README.md.
    std::string reason;
    EXPECT_EQ(cube_of(MaterialId::ConcreteUnpainted, GetParam().edge_m).is_valid(reason), GetParam().valid)
        << reason;
}

INSTANTIATE_TEST_SUITE_P(Edges, Small_RoomSceneCubeEdge, ::testing::ValuesIn(K_CUBE_EDGES), cube_edge_name);

TEST(Small_RoomScene, RoomSceneFloorRisesWithVolumeOverArea)
{
    // The same concrete passes in a 6.0 x 4.0 x 2.5 m shoebox. See tests/nodes/room/test_room_scene/README.md.
    RoomScene shoebox;
    shoebox.surfaces.fill(MaterialId::ConcreteUnpainted);
    const RoomScene cube = cube_of(MaterialId::ConcreteUnpainted, 4.6);
    std::string reason;
    EXPECT_TRUE(shoebox.is_valid(reason)) << reason;
    EXPECT_LT(shoebox.min_mean_absorption(), cube.min_mean_absorption());
}

TEST_P(Small_RoomSceneBuiltRoomLoss, RoomSceneAddsTheBuiltRoomLossToTheFaceWhenSet)
{
    const RoomScene plain;
    RoomScene built = plain;
    built.built_room_loss = true;

    expect_bands_within(built.face_absorption(GetParam().surface),
                        plus_built_room_loss(plain.face_absorption(GetParam().surface)), K_FACE_OPERATIONS);
}

INSTANTIATE_TEST_SUITE_P(Faces, Small_RoomSceneBuiltRoomLoss, ::testing::ValuesIn(K_FACES), face_name);

TEST(Small_RoomScene, RoomSceneCapsTheBuiltRoomLossAtFullAbsorption)
{
    RoomScene scene;
    scene.surfaces.fill(MaterialId::OpenDoorway);
    scene.built_room_loss = true;

    EXPECT_EQ(scene.mean_absorption(), (Bands{1.0, 1.0, 1.0, 1.0, 1.0, 1.0}));
}
