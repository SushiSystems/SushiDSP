/**************************************************************************/
/* test_image_source_solver.cpp                                           */
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
 * @file test_image_source_solver.cpp
 * @brief Tests for ImageSourceSolver, the mirror lattice of the room module.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>
#include <SushiDSP/foundation/log_capture/log_capture.hpp>
#include <SushiDSP/nodes/room/band_response_fit.hpp>
#include <SushiDSP/nodes/room/image_source_solver.hpp>
#include <SushiDSP/nodes/room/medium_absorption.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

#include "support/tolerance.hpp"

namespace
{
    using SushiDSP::room::EarlyTapSet;
    using SushiDSP::room::ImageSourceSolver;
    using SushiDSP::room::ListenerKind;
    using SushiDSP::room::RoomScene;
    using SushiDSP::room::Scattering;

    /// @brief Holds the frames one metre of path takes at 48 kHz and 343 m/s.
    constexpr double K_FRAMES_PER_METRE = 48000.0 / 343.0;

    /// @brief Counts the units of u that bound a value the solver stores as a float: its one cast,
    /// and one more for the double operations of the oracle that computes the reference.
    constexpr double K_STORE_UNITS = 2.0;

    /// @brief Holds the share of pressure one bounce keeps on a Flat surface, 1 - 0.10: the
    /// scattering table in docs/design/ROOM/README.md.
    constexpr double K_FLAT_SPECULAR_SHARE = 0.90;

    /// @brief Holds the share of pressure one bounce keeps on a Cluttered surface, 1 - 0.40.
    constexpr double K_CLUTTERED_SPECULAR_SHARE = 0.60;

    /// @brief Counts the units of u that bound the ratio of two stored floats: each carries its own
    /// store error, and the quotient adds one rounding.
    constexpr double K_RATIO_UNITS = 3.0;

    /// @brief Holds the decibels the fit of BandResponseFit stays within at every band centre, which
    /// test_band_response_fit.cpp holds it to for every material and order.
    constexpr double K_FIT_CLEARANCE_DB = 1.0;

    /// @brief Counts the reflected taps after the line of sight that the furnishing test reads.
    constexpr std::uint32_t K_REFLECTED_TAPS = 12;

    /// @brief Returns the bound of a float-stored value of the given size.
    double float_bound(double value)
    {
        return K_STORE_UNITS * TestSupport::unit_roundoff<float>() * std::fabs(value);
    }

    /// @brief Returns whether a float the solver stored equals its double reference within the bound.
    ::testing::AssertionResult stored_as(double computed, double reference)
    {
        return TestSupport::is_within(TestSupport::measure_error(computed, reference, float_bound(reference), 1.0));
    }

    /// @brief Returns the spec's section 4.3 geometry: the garage with the source at
    /// (1.5, 2.0, 0.5) and the listener at (4.0, 2.0, 1.2).
    RoomScene example_scene()
    {
        RoomScene scene = SushiDSP::test::garage_scene();
        scene.source_position = {1.5, 2.0, 0.5};
        scene.listener_position = {4.0, 2.0, 1.2};
        return scene;
    }

    /// @brief Returns the seven first-order path lengths of example_scene(), in metres, in
    /// arrival order: direct, floor, ceiling, -y, +y, -x, +x.
    std::array<double, 7> first_order_metres()
    {
        // These are the seven first-order path lengths. See tests/nodes/room/README.md.
        return {std::sqrt(2.5 * 2.5 + 0.7 * 0.7),
                std::sqrt(2.5 * 2.5 + 1.7 * 1.7),
                std::sqrt(2.5 * 2.5 + 3.3 * 3.3),
                std::sqrt(2.5 * 2.5 + 4.0 * 4.0 + 0.7 * 0.7),
                std::sqrt(2.5 * 2.5 + 4.0 * 4.0 + 0.7 * 0.7),
                std::sqrt(5.5 * 5.5 + 0.7 * 0.7),
                std::sqrt(6.5 * 6.5 + 0.7 * 0.7)};
    }

    /// @brief Returns the frames the path of the given first-order image takes.
    double first_order_frames(std::size_t image)
    {
        return first_order_metres()[image] * K_FRAMES_PER_METRE;
    }

    /// @brief Returns how many taps arrive at the given frame, within the float store bound.
    std::uint32_t arrivals_at(const EarlyTapSet& set, double frames)
    {
        std::uint32_t matches = 0;
        for (std::uint32_t tap = 0; tap < set.count; ++tap)
        {
            if (std::fabs(static_cast<double>(set.taps[tap].delay_frames) - frames) <= float_bound(frames))
            {
                ++matches;
            }
        }
        return matches;
    }

    /// @brief Returns the index of the one tap arriving at the given frame, or the set's count when
    /// there is none or more than one.
    std::uint32_t find_tap(const EarlyTapSet& set, double frames)
    {
        std::uint32_t found = set.count;
        for (std::uint32_t tap = 0; tap < set.count; ++tap)
        {
            if (std::fabs(static_cast<double>(set.taps[tap].delay_frames) - frames) <= float_bound(frames))
            {
                found = tap;
            }
        }
        return arrivals_at(set, frames) == 1u ? found : set.count;
    }

    /// @brief Returns an order-4 solve of the example scene with every surface at one scattering word.
    EarlyTapSet example_with(Scattering scattering)
    {
        RoomScene scene = example_scene();
        scene.scattering.fill(scattering);
        return ImageSourceSolver::solve(scene, {});
    }

    /// @brief Returns a solve of the example scene at the given order for the given listener.
    EarlyTapSet example_solved(std::uint32_t order, ListenerKind listener)
    {
        ImageSourceSolver::Options options;
        options.max_order = order;
        options.listener = listener;
        return ImageSourceSolver::solve(example_scene(), options);
    }

    /// @brief Returns the default scene holding the given bed and sofa counts.
    RoomScene furnished_scene(double beds, double sofas)
    {
        RoomScene scene;
        scene.furnishings[0] = {SushiDSP::room::FurnitureId::BedDouble, beds};
        scene.furnishings[1] = {SushiDSP::room::FurnitureId::SofaTwoSeat, sofas};
        scene.furnishing_count = 2;
        return scene;
    }

    /// @brief Returns the index of the first tap that arrives before its predecessor, or the count
    /// when the taps are sorted by arrival.
    std::uint32_t first_unsorted_tap(const EarlyTapSet& set)
    {
        for (std::uint32_t tap = 1; tap < set.count; ++tap)
        {
            if (set.taps[tap].delay_frames < set.taps[tap - 1].delay_frames)
            {
                return tap;
            }
        }
        return set.count;
    }

    /// @brief Returns how many taps of the set encode something other than a pure channel 0 of one.
    std::uint32_t taps_with_higher_orders(const EarlyTapSet& set)
    {
        std::uint32_t found = 0;
        for (std::uint32_t tap = 0; tap < set.count; ++tap)
        {
            for (std::size_t channel = 1; channel < set.taps[tap].encode.size(); ++channel)
            {
                if (set.taps[tap].encode[channel] != 0.0f)
                {
                    ++found;
                    break;
                }
            }
        }
        return found;
    }

    /// @brief Returns how many taps of the set do not carry exactly one in channel 0.
    std::uint32_t taps_without_unity_in_channel_zero(const EarlyTapSet& set)
    {
        return static_cast<std::uint32_t>(std::count_if(
            set.taps.begin(), set.taps.begin() + set.count, [](const SushiDSP::room::EarlyTap& tap) { return tap.encode[0] != 1.0f; }));
    }

    /// @brief Returns the index of the first tap whose delay or gain differs between two sets, or
    /// the count when none does.
    std::uint32_t first_tap_differing_in_delay_or_gain(const EarlyTapSet& a, const EarlyTapSet& b)
    {
        for (std::uint32_t tap = 0; tap < a.count; ++tap)
        {
            if (a.taps[tap].delay_frames != b.taps[tap].delay_frames || a.taps[tap].gain != b.taps[tap].gain)
            {
                return tap;
            }
        }
        return a.count;
    }

    /// @brief Returns the index of the first tap that differs in any field between two sets, or the
    /// count when none does.
    std::uint32_t first_tap_differing(const EarlyTapSet& a, const EarlyTapSet& b)
    {
        for (std::uint32_t tap = 0; tap < a.count; ++tap)
        {
            const SushiDSP::room::EarlyTap& x = a.taps[tap];
            const SushiDSP::room::EarlyTap& y = b.taps[tap];
            if (x.delay_frames != y.delay_frames || x.gain != y.gain || x.section_a != y.section_a ||
                x.section_b != y.section_b || x.encode != y.encode)
            {
                return tap;
            }
        }
        return a.count;
    }

    /// @brief Holds the largest miss, in decibels, between the furnishing's measured loss over the
    /// reflected taps and the medium's formula for it, and the largest reduction the formula gave.
    struct FurnishingLoss
    {
            double worst_miss_db = 0.0;
            double largest_reduction_db = 0.0;
    };

    /// @brief Measures the fitted loss the furnishing adds to the first reflected taps in one band.
    FurnishingLoss measure_furnishing_loss(const EarlyTapSet& bare, const EarlyTapSet& furnished, const RoomScene& scene,
                                           std::size_t band)
    {
        const SushiDSP::room::MediumAbsorption medium(scene);
        FurnishingLoss loss;
        for (std::uint32_t tap = 1; tap <= K_REFLECTED_TAPS; ++tap)
        {
            const double metres = static_cast<double>(bare.taps[tap].delay_frames) / K_FRAMES_PER_METRE;
            const double centre = SushiDSP::room::K_BAND_CENTRES_HZ[band];
            const double measured =
                SushiDSP::room::BandResponseFit::magnitude_db({furnished.taps[tap].section_a, furnished.taps[tap].section_b},
                                                              centre, 48000.0) -
                SushiDSP::room::BandResponseFit::magnitude_db({bare.taps[tap].section_a, bare.taps[tap].section_b}, centre,
                                                              48000.0);
            const double expected = 10.0 * std::log10(std::exp(-medium.furnishing_coefficient_per_metre(band) * metres));
            loss.largest_reduction_db = std::max(loss.largest_reduction_db, -expected);
            loss.worst_miss_db = std::max(loss.worst_miss_db, std::fabs(measured - expected));
        }
        return loss;
    }

    /// @brief Holds a named room for the sort property.
    struct NamedRoom
    {
            const char* name;
            RoomScene (*make)();
    };

    const std::array<NamedRoom, 3> kRooms = {{{"Garage", &SushiDSP::test::garage_scene},
                                              {"Bedroom", &SushiDSP::test::bedroom_scene},
                                              {"BlockRoom", &SushiDSP::test::block_room_scene}}};

    /// @brief Names a room case after the room.
    std::string room_name(const ::testing::TestParamInfo<NamedRoom>& info)
    {
        return info.param.name;
    }

    /// @brief Names an index case after the index.
    std::string index_name(const ::testing::TestParamInfo<std::size_t>& info)
    {
        return std::to_string(info.param);
    }

    const std::vector<std::size_t> kAllFirstOrderImages = {0, 1, 2, 3, 4, 5, 6};
    const std::vector<std::size_t> kKeptFirstOrderImages = {0, 1, 2, 3, 4};
    const std::vector<std::size_t> kSingleArrivalImages = {1, 2, 5, 6};
    const std::vector<std::size_t> kBands = {0, 1, 2, 3, 4, 5};
} // namespace

TEST(Small_ImageSourceSolver, ImageSourceSolverCountsTheLatticePoints)
{
    EXPECT_EQ(ImageSourceSolver::image_count(0u), 1u);
    EXPECT_EQ(ImageSourceSolver::image_count(1u), 7u);
    EXPECT_EQ(ImageSourceSolver::image_count(2u), 25u);
    EXPECT_EQ(ImageSourceSolver::image_count(3u), 63u);
    EXPECT_EQ(ImageSourceSolver::image_count(4u), 129u);
}

class Small_ImageSourceSolverArrivals : public ::testing::TestWithParam<std::size_t>
{
    protected:
        static void SetUpTestSuite()
        {
            set_ = ImageSourceSolver::solve(example_scene(), {});
        }

        static inline EarlyTapSet set_;
};

TEST_P(Small_ImageSourceSolverArrivals, ImageSourceSolverPlacesTheFirstOrderArrivalExactly)
{
    // The two y images share one arrival. See tests/nodes/room/README.md.
    const bool shares_an_arrival = GetParam() == 3u || GetParam() == 4u;

    EXPECT_EQ(arrivals_at(set_, first_order_frames(GetParam())), shares_an_arrival ? 2u : 1u);
}

INSTANTIATE_TEST_SUITE_P(Images, Small_ImageSourceSolverArrivals, ::testing::ValuesIn(kAllFirstOrderImages), index_name);

TEST(Small_ImageSourceSolver, ImageSourceSolverPutsTheDirectSoundFirst)
{
    const EarlyTapSet set = ImageSourceSolver::solve(example_scene(), {});

    EXPECT_TRUE(stored_as(set.taps[0].delay_frames, first_order_frames(0)));
}

TEST(Small_ImageSourceSolver, ImageSourceSolverPutsTheFloorReflectionSecond)
{
    const EarlyTapSet set = ImageSourceSolver::solve(example_scene(), {});

    EXPECT_TRUE(stored_as(set.taps[1].delay_frames, first_order_frames(1)));
}

class Small_ImageSourceSolverFirstOrder : public ::testing::TestWithParam<std::size_t>
{
    protected:
        static void SetUpTestSuite()
        {
            set_ = example_solved(1u, ListenerKind::Ears);
        }

        static inline EarlyTapSet set_;
};

TEST_P(Small_ImageSourceSolverFirstOrder, ImageSourceSolverKeepsTheFirstOrderArrivalBeforeItsHandover)
{
    // At order 1 t_c is 699.708 frames, so five taps remain. See tests/nodes/room/README.md.
    ASSERT_EQ(set_.count, 5u);

    EXPECT_TRUE(stored_as(set_.taps[GetParam()].delay_frames, first_order_frames(GetParam())));
}

INSTANTIATE_TEST_SUITE_P(Images, Small_ImageSourceSolverFirstOrder, ::testing::ValuesIn(kKeptFirstOrderImages), index_name);

TEST(Small_ImageSourceSolver, ImageSourceSolverCutsTheFirstOrderSetAtFiveTaps)
{
    // At order 1 t_c is 699.708 frames, so the -x and +x images are past it. See tests/nodes/room/README.md.
    EXPECT_EQ(example_solved(1u, ListenerKind::Ears).count, 5u);
}

TEST(Small_ImageSourceSolver, ImageSourceSolverReportsTheFirstOrderHandoverTime)
{
    const double crossover = 2.0 * 2.5 / 343.0;

    EXPECT_TRUE(stored_as(example_solved(1u, ListenerKind::Ears).crossover_time_s, crossover));
}

class Small_ImageSourceSolverGarage : public ::testing::Test
{
    protected:
        static void SetUpTestSuite()
        {
            garage_ = SushiDSP::test::garage_scene();
            set_ = ImageSourceSolver::solve(garage_, {});
        }

        static inline RoomScene garage_;
        static inline EarlyTapSet set_;
};

class Small_ImageSourceSolverRooms : public ::testing::TestWithParam<NamedRoom>
{
};

TEST_P(Small_ImageSourceSolverRooms, ImageSourceSolverSortsItsTapsByArrival)
{
    const EarlyTapSet set = ImageSourceSolver::solve(GetParam().make(), {});

    EXPECT_EQ(first_unsorted_tap(set), set.count);
}

TEST_P(Small_ImageSourceSolverRooms, ImageSourceSolverFillsMoreThanTheFirstOrderAndStaysInsideTheCapacity)
{
    const EarlyTapSet set = ImageSourceSolver::solve(GetParam().make(), {});

    EXPECT_GT(set.count, 7u);
    EXPECT_LE(set.count, EarlyTapSet::K_MAX_EARLY_TAPS);
}

INSTANTIATE_TEST_SUITE_P(Rooms, Small_ImageSourceSolverRooms, ::testing::ValuesIn(kRooms), room_name);

TEST_F(Small_ImageSourceSolverGarage, ImageSourceSolverComputesTheHandoverTimeFromTheShortestWall)
{
    // (4 + 1) * 2.5 / 343 = 0.0364431 s, 1749.27 frames at 48 kHz. The solver divides the same product by the same speed, so the two agree to a few roundings of a double.
    const double crossover = 5.0 * 2.5 / 343.0;
    const double bound = 4.0 * TestSupport::unit_roundoff<double>() * crossover;

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(ImageSourceSolver::crossover_time_s(garage_, 4u), crossover, bound, 1.0)));
}

TEST_F(Small_ImageSourceSolverGarage, ImageSourceSolverStoresTheHandoverTimeAsAFloat)
{
    EXPECT_TRUE(stored_as(set_.crossover_time_s, 5.0 * 2.5 / 343.0));
}

TEST_F(Small_ImageSourceSolverGarage, ImageSourceSolverDropsEveryTapPastTheHandoverTime)
{
    // A tap the solver kept at the crossover is rounded to float once more on the way in.
    const double crossover_frames = 5.0 * 2.5 / 343.0 * 48000.0;
    double latest = 0.0;
    for (std::uint32_t tap = 0; tap < set_.count; ++tap)
    {
        latest = std::max(latest, static_cast<double>(set_.taps[tap].delay_frames));
    }

    EXPECT_LE(latest, crossover_frames + float_bound(crossover_frames));
}

TEST_F(Small_ImageSourceSolverGarage, ImageSourceSolverKeepsFewerTapsThanTheLatticeHolds)
{
    // Order 4 enumerates 129 images; some arrive after t_c. See tests/nodes/room/README.md.
    EXPECT_LT(set_.count, 129u);
}

TEST_F(Small_ImageSourceSolverGarage, ImageSourceSolverSolvesTheGarageToTheSameTapsEveryTimeInsideTheCapacity)
{
    const EarlyTapSet second = ImageSourceSolver::solve(garage_, {});

    EXPECT_LE(set_.count, EarlyTapSet::K_MAX_EARLY_TAPS);
    EXPECT_EQ(second.count, set_.count);
    EXPECT_EQ(first_tap_differing(set_, second), set_.count);
}

TEST(Small_ImageSourceSolver, ImageSourceSolverFallsOffAsOneOverDistance)
{
    RoomScene scene = example_scene();
    scene.surfaces.fill(SushiDSP::room::MaterialId::OpenDoorway);
    scene.patch_count = 0;
    const EarlyTapSet set = ImageSourceSolver::solve(scene, {});
    ASSERT_EQ(set.count, 1u);

    EXPECT_TRUE(stored_as(set.taps[0].gain, 1.0 / first_order_metres()[0]));
}

TEST(Small_ImageSourceSolver, ImageSourceSolverScattersNothingInAnOpenRoom)
{
    RoomScene scene = example_scene();
    scene.surfaces.fill(SushiDSP::room::MaterialId::OpenDoorway);
    scene.patch_count = 0;

    // The line of sight keeps a specular share of exactly one, so it hands the tail exactly zero.
    EXPECT_EQ(ImageSourceSolver::solve(scene, {}).scattered_energy, 0.0f);
}

class Small_ImageSourceSolverEncoding : public ::testing::Test
{
    protected:
        static void SetUpTestSuite()
        {
            ears_ = example_solved(2u, ListenerKind::Ears);
            microphone_ = example_solved(2u, ListenerKind::Microphone);
        }

        static inline EarlyTapSet ears_;
        static inline EarlyTapSet microphone_;
};

TEST_F(Small_ImageSourceSolverEncoding, EarsCarryUnityInChannelZeroOnEveryTap)
{
    ASSERT_GT(ears_.count, 1u);

    EXPECT_EQ(taps_without_unity_in_channel_zero(ears_), 0u);
}

TEST_F(Small_ImageSourceSolverEncoding, EarsEncodeEveryTapIntoTheHigherOrders)
{
    EXPECT_EQ(taps_with_higher_orders(ears_), ears_.count);
}

TEST_F(Small_ImageSourceSolverEncoding, MicrophoneCarriesUnityInChannelZeroOnEveryTap)
{
    ASSERT_GT(microphone_.count, 1u);

    EXPECT_EQ(taps_without_unity_in_channel_zero(microphone_), 0u);
}

TEST_F(Small_ImageSourceSolverEncoding, MicrophoneLeavesTheHigherOrdersZeroOnEveryTap)
{
    EXPECT_EQ(taps_with_higher_orders(microphone_), 0u);
}

TEST_F(Small_ImageSourceSolverEncoding, EarsHaveOneLineOfSightTap)
{
    EXPECT_EQ(ears_.direct_count, 1u);
}

TEST_F(Small_ImageSourceSolverEncoding, MicrophoneHasOneLineOfSightTapPerElement)
{
    // The microphone's line of sight is one tap per element of the 4x12.
    EXPECT_EQ(microphone_.direct_count, 28u);
}

TEST_F(Small_ImageSourceSolverEncoding, MicrophoneLineOfSightStartsWithTheNearestElement)
{
    EXPECT_LT(microphone_.taps[0].delay_frames, 0.3 * K_FRAMES_PER_METRE);
    EXPECT_LT(microphone_.taps[0].delay_frames, ears_.taps[0].delay_frames);
}

TEST_F(Small_ImageSourceSolverEncoding, MicrophoneNearestElementIsLouderThanTheEarsDirectTap)
{
    EXPECT_GT(microphone_.taps[0].gain, ears_.taps[0].gain);
}

class Small_ImageSourceSolverScattering : public ::testing::TestWithParam<std::size_t>
{
    protected:
        static void SetUpTestSuite()
        {
            flat_ = example_with(Scattering::Flat);
            cluttered_ = example_with(Scattering::Cluttered);
        }

        static inline EarlyTapSet flat_;
        static inline EarlyTapSet cluttered_;
};

TEST_P(Small_ImageSourceSolverScattering, ImageSourceSolverKeepsTheSquareRootOfTheSpecularShareOnFlat)
{
    // One bounce keeps sqrt(share) of the pressure over r. See tests/nodes/room/README.md.
    const double frames = first_order_frames(GetParam());
    const std::uint32_t index = find_tap(flat_, frames);
    ASSERT_LT(index, flat_.count);

    EXPECT_TRUE(stored_as(flat_.taps[index].gain, std::sqrt(K_FLAT_SPECULAR_SHARE) / first_order_metres()[GetParam()]));
}

TEST_P(Small_ImageSourceSolverScattering, ImageSourceSolverKeepsTheSquareRootOfTheSpecularShareOnCluttered)
{
    const double frames = first_order_frames(GetParam());
    const std::uint32_t index = find_tap(cluttered_, frames);
    ASSERT_LT(index, cluttered_.count);

    EXPECT_TRUE(stored_as(cluttered_.taps[index].gain, std::sqrt(K_CLUTTERED_SPECULAR_SHARE) / first_order_metres()[GetParam()]));
}

TEST_P(Small_ImageSourceSolverScattering, ImageSourceSolverKeepsClutteredAtTheSquareRootOfTheSharesOverFlat)
{
    // Cluttered keeps sqrt(0.60 / 0.90) = 0.816497 of Flat. See tests/nodes/room/README.md.
    const double frames = first_order_frames(GetParam());
    const std::uint32_t index = find_tap(flat_, frames);
    ASSERT_LT(index, flat_.count);
    const double ratio = static_cast<double>(cluttered_.taps[index].gain) / flat_.taps[index].gain;
    const double expected = std::sqrt(K_CLUTTERED_SPECULAR_SHARE / K_FLAT_SPECULAR_SHARE);

    const double bound = K_RATIO_UNITS * TestSupport::unit_roundoff<float>() * expected;
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(ratio, expected, bound, 1.0)));
}

INSTANTIATE_TEST_SUITE_P(Images, Small_ImageSourceSolverScattering, ::testing::ValuesIn(kSingleArrivalImages), index_name);

TEST(Small_ImageSourceSolverDirect, ImageSourceSolverKeepsTheScatteringSetsTheSameSize)
{
    EXPECT_EQ(example_with(Scattering::Flat).count, example_with(Scattering::Cluttered).count);
}

TEST(Small_ImageSourceSolverDirect, ImageSourceSolverKeepsTheDirectTapOnFlat)
{
    const EarlyTapSet flat = example_with(Scattering::Flat);

    // The direct tap crosses no wall and keeps 1 / r whatever the scattering.
    EXPECT_TRUE(stored_as(flat.taps[0].gain, 1.0 / first_order_metres()[0]));
}

TEST(Small_ImageSourceSolverDirect, ImageSourceSolverKeepsTheDirectTapOnCluttered)
{
    const EarlyTapSet cluttered = example_with(Scattering::Cluttered);

    EXPECT_TRUE(stored_as(cluttered.taps[0].gain, 1.0 / first_order_metres()[0]));
}

TEST(Small_ImageSourceSolverDirect, ImageSourceSolverHandsFlatSurfacesScatteredEnergyToTheTail)
{
    EXPECT_GT(example_with(Scattering::Flat).scattered_energy, 0.0f);
}

TEST(Small_ImageSourceSolverDirect, ImageSourceSolverHandsClutteredSurfacesMoreScatteredEnergyThanFlatOnes)
{
    EXPECT_GT(example_with(Scattering::Cluttered).scattered_energy, example_with(Scattering::Flat).scattered_energy);
}

TEST(Small_ImageSourceSolverFurnished, ImageSourceSolverKeepsEveryDelayAndGainWhenFurnished)
{
    const EarlyTapSet bare = ImageSourceSolver::solve(RoomScene{}, {});
    const EarlyTapSet furnished = ImageSourceSolver::solve(furnished_scene(3.0, 2.0), {});
    ASSERT_GT(bare.count, 0u);
    ASSERT_EQ(bare.count, furnished.count);

    EXPECT_EQ(first_tap_differing_in_delay_or_gain(bare, furnished), bare.count);
    EXPECT_EQ(bare.taps[0].section_a, furnished.taps[0].section_a);
    EXPECT_EQ(bare.taps[0].section_b, furnished.taps[0].section_b);
}

class Small_ImageSourceSolverFurnishedSolves : public ::testing::Test
{
    protected:
        static void SetUpTestSuite()
        {
            scene_ = furnished_scene(4.0, 5.0);
            bare_ = ImageSourceSolver::solve(RoomScene{}, {});
            furnished_ = ImageSourceSolver::solve(scene_, {});
        }

        static inline RoomScene scene_;
        static inline EarlyTapSet bare_;
        static inline EarlyTapSet furnished_;
};

class Small_ImageSourceSolverFurnishing : public Small_ImageSourceSolverFurnishedSolves,
                                          public ::testing::WithParamInterface<std::size_t>
{
};

TEST_P(Small_ImageSourceSolverFurnishing, ImageSourceSolverThinsAReflectedTapByTheFurnishingLoss)
{
    ASSERT_GT(bare_.count, K_REFLECTED_TAPS);
    ASSERT_EQ(bare_.count, furnished_.count);

    // Both fits are held to K_FIT_CLEARANCE_DB of their targets, and the targets differ by the furnishing's loss alone, so the measured difference is within twice that of the formula.
    EXPECT_LE(measure_furnishing_loss(bare_, furnished_, scene_, GetParam()).worst_miss_db, 2.0 * K_FIT_CLEARANCE_DB);
}

INSTANTIATE_TEST_SUITE_P(Bands, Small_ImageSourceSolverFurnishing, ::testing::ValuesIn(kBands), index_name);

TEST_F(Small_ImageSourceSolverFurnishedSolves, ImageSourceSolverThinsAReflectedTapByMoreThanTheMissBoundInSomeBand)
{
    // The thinning test can fail: the loss the formula gives somewhere is larger than the bound that test holds the miss to.
    double largest = 0.0;
    for (const std::size_t band : kBands)
    {
        largest = std::max(largest, measure_furnishing_loss(bare_, furnished_, scene_, band).largest_reduction_db);
    }

    EXPECT_GT(largest, 2.0 * K_FIT_CLEARANCE_DB);
}

TEST(Small_ImageSourceSolverLogging, SolveWarnsOnceWhenTheTapsAreCutAtTheCapacity)
{
    ImageSourceSolver::Options options;
    options.max_order = 8u;

    SushiDSP::LogCapture capture;
    const EarlyTapSet set = ImageSourceSolver::solve(SushiDSP::test::garage_scene(), options);

    EXPECT_EQ(set.count, EarlyTapSet::K_MAX_EARLY_TAPS);
    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "room");
    EXPECT_NE(lines[0].message.find("kept 128 early taps"), std::string::npos) << lines[0].message;
}

TEST(Small_ImageSourceSolverLogging, SolveWritesNothingWhenNoTapIsLeftOut)
{
    SushiDSP::LogCapture capture;
    const EarlyTapSet set = example_solved(1u, ListenerKind::Ears);

    EXPECT_EQ(set.count, 5u);
    EXPECT_TRUE(capture.lines().empty());
}
