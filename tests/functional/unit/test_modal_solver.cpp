/**************************************************************************/
/* test_modal_solver.cpp                                                  */
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
 * @file test_modal_solver.cpp
 * @brief Tests for ModalSolver, the shoebox's eigenmodes, their damping, their gains and the
 * seam level.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "RoomFixtures.hpp"
#include "SushiDSP/room/BandResponse.hpp"
#include "SushiDSP/room/EyringDecay.hpp"
#include "SushiDSP/room/Materials.hpp"
#include "SushiDSP/room/ModalDesign.hpp"
#include "SushiDSP/room/ModalSolver.hpp"
#include "SushiDSP/room/RoomScene.hpp"

namespace
{
    /// @brief Holds pi.
    constexpr double kPi = 3.14159265358979323846;

    /// @brief Holds the sample rate every design in this file is built at.
    constexpr double kSampleRate = 48000.0;

    /// @brief Holds one index triple of a mode.
    struct Triple
    {
        /// @brief Stores the index along x.
        std::uint32_t nx;

        /// @brief Stores the index along y.
        std::uint32_t ny;

        /// @brief Stores the index along z.
        std::uint32_t nz;
    };

    /// @brief Evaluates (c / 2) sqrt((nx / Lx)^2 + (ny / Ly)^2 + (nz / Lz)^2) for one triple.
    double eigenfrequency(const SushiDSP::room::RoomScene& scene, const Triple& triple)
    {
        const double x = triple.nx / scene.dimensions.x;
        const double y = triple.ny / scene.dimensions.y;
        const double z = triple.nz / scene.dimensions.z;
        return 0.5 * SushiDSP::room::kSpeedOfSound * std::sqrt(x * x + y * y + z * z);
    }

    /// @brief Evaluates Weyl's (4 pi / 3) V (f / c)^3 + (pi / 4) S (f / c)^2 + (L / 8) (f / c)
    /// with L = 4 (Lx + Ly + Lz).
    double weyl_count(const SushiDSP::room::RoomScene& scene, double frequency_hz)
    {
        const double k = frequency_hz / SushiDSP::room::kSpeedOfSound;
        const double edges = 4.0 * (scene.dimensions.x + scene.dimensions.y + scene.dimensions.z);
        return (4.0 * kPi / 3.0) * scene.volume_m3() * k * k * k
            + (kPi / 4.0) * scene.surface_area_m2() * k * k + (edges / 8.0) * k;
    }

    /// @brief Returns the mode with the given indices out of a sorted enumeration.
    const SushiDSP::room::ModalSolver::Mode& find_mode(
        const std::vector<SushiDSP::room::ModalSolver::Mode>& modes, const Triple& triple)
    {
        for (const SushiDSP::room::ModalSolver::Mode& mode : modes)
        {
            if (mode.nx == triple.nx && mode.ny == triple.ny && mode.nz == triple.nz)
            {
                return mode;
            }
        }
        ADD_FAILURE() << "mode (" << triple.nx << ", " << triple.ny << ", " << triple.nz
                      << ") is missing";
        return modes.front();
    }

    /// @brief Interpolates one face's 125 Hz and 250 Hz absorption in log frequency, held flat
    /// outside those two bands.
    double face_absorption_at(const SushiDSP::room::RoomScene& scene,
                              SushiDSP::room::Surface surface, double frequency_hz)
    {
        const std::array<double, SushiDSP::room::kBandCount> rows = scene.face_absorption(surface);
        const double position = std::log(frequency_hz / 125.0) / std::log(2.0);
        const double clamped = std::min(1.0, std::max(0.0, position));
        return rows[0] + (rows[1] - rows[0]) * clamped;
    }

    /// @brief Evaluates the perturbation decay constant (c / 16 V) sum over axes of
    /// epsilon_i (alpha_-i + alpha_+i) A_i, epsilon_i = 1 for a zero index and 2 otherwise.
    double damping_constant(const SushiDSP::room::RoomScene& scene, const Triple& triple)
    {
        using SushiDSP::room::Surface;
        const double frequency = eigenfrequency(scene, triple);
        const std::array<std::uint32_t, 3> indices = { triple.nx, triple.ny, triple.nz };
        double weighted = 0.0;
        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            const Surface low = static_cast<Surface>(2 * axis);
            const Surface high = static_cast<Surface>(2 * axis + 1);
            const double epsilon = indices[axis] == 0u ? 1.0 : 2.0;
            weighted += epsilon * scene.face_area_m2(low)
                * (face_absorption_at(scene, low, frequency)
                   + face_absorption_at(scene, high, frequency));
        }
        return SushiDSP::room::kSpeedOfSound * weighted / (16.0 * scene.volume_m3());
    }

    /// @brief Returns the 60 dB decay 3 ln(10) / delta of one mode, in seconds.
    double decay_seconds(const SushiDSP::room::RoomScene& scene, const Triple& triple)
    {
        return 3.0 * std::log(10.0) / damping_constant(scene, triple);
    }

    /// @brief Returns a 3.0 x 2.5 x 2.4 m room of unpainted concrete with every pose inside.
    SushiDSP::room::RoomScene small_concrete_room()
    {
        SushiDSP::room::RoomScene scene;
        scene.dimensions = { 3.0, 2.5, 2.4 };
        scene.surfaces.fill(SushiDSP::room::MaterialId::ConcreteUnpainted);
        scene.scattering.fill(SushiDSP::room::Scattering::Flat);
        scene.patch_count = 0;
        scene.source_position = { 0.3, 0.3, 0.3 };
        scene.listener_position = { 1.5, 1.25, 1.2 };
        scene.microphone_position = { 0.5, 0.5, 0.5 };
        return scene;
    }

    /// @brief Returns Eyring's decay time at frequency_hz, linear in log frequency between the
    /// band centres and held flat outside them.
    double rt60_at(const std::array<double, SushiDSP::room::kBandCount>& rt60, double frequency_hz)
    {
        const auto& centres = SushiDSP::room::kBandCentresHz;
        if (frequency_hz <= centres.front())
        {
            return rt60.front();
        }
        for (std::size_t band = 1; band < SushiDSP::room::kBandCount; ++band)
        {
            if (frequency_hz <= centres[band])
            {
                const double position = std::log(frequency_hz / centres[band - 1])
                    / std::log(centres[band] / centres[band - 1]);
                return rt60[band - 1] + (rt60[band] - rt60[band - 1]) * position;
            }
        }
        return rt60.back();
    }

    /// @brief Integrates 16 pi RT60(f) / (0.161 V) times a 2 / fs density over [lower, upper]
    /// by the midpoint rule.
    double eyring_band_power(const SushiDSP::room::RoomScene& scene, double lower, double upper)
    {
        constexpr int kSteps = 65536;
        const std::array<double, SushiDSP::room::kBandCount> rt60 =
            SushiDSP::room::EyringDecay::rt60_seconds(scene);
        const double width = (upper - lower) / kSteps;
        double sum = 0.0;
        for (int step = 0; step < kSteps; ++step)
        {
            sum += rt60_at(rt60, lower + (step + 0.5) * width) * width;
        }
        return 16.0 * kPi * (2.0 / kSampleRate) * sum / (0.161 * scene.volume_m3());
    }
}

TEST(Unit_Dsp, ModalSolverFindsTheGaragesEigenfrequencies)
{
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const std::vector<SushiDSP::room::ModalSolver::Mode> modes =
        SushiDSP::room::ModalSolver::enumerate(garage, 100.0);
    // (c / 2) sqrt((nx / 6)^2 + (ny / 4)^2 + (nz / 2.5)^2) at c = 343 for the first eight:
    // 28.5833, 42.8750, 51.5293, 57.1667, 68.6000, 71.4583, 74.3167 and 80.8964 Hz.
    const std::array<Triple, 8> triples = { Triple{ 1, 0, 0 }, Triple{ 0, 1, 0 },
                                            Triple{ 1, 1, 0 }, Triple{ 2, 0, 0 },
                                            Triple{ 0, 0, 1 }, Triple{ 2, 1, 0 },
                                            Triple{ 1, 0, 1 }, Triple{ 0, 1, 1 } };
    const std::array<double, 8> rounded =
        { 28.58, 42.88, 51.53, 57.17, 68.60, 71.46, 74.32, 80.90 };
    ASSERT_GE(modes.size(), 8u);
    for (std::size_t index = 0; index < 8u; ++index)
    {
        EXPECT_NEAR(modes[index].frequency_hz, eigenfrequency(garage, triples[index]), 1e-9)
            << "mode " << index;
        EXPECT_NEAR(modes[index].frequency_hz, rounded[index], 0.01) << "mode " << index;
        EXPECT_EQ(modes[index].nx, triples[index].nx) << "mode " << index;
        EXPECT_EQ(modes[index].ny, triples[index].ny) << "mode " << index;
        EXPECT_EQ(modes[index].nz, triples[index].nz) << "mode " << index;
    }
    for (std::size_t index = 1; index < modes.size(); ++index)
    {
        EXPECT_LE(modes[index - 1].frequency_hz, modes[index].frequency_hz) << "mode " << index;
    }
}

TEST(Unit_Dsp, ModalSolverCountsOneHundredAndEightySevenModesBelowTheGaragesSchroederFrequency)
{
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    EXPECT_EQ(SushiDSP::room::ModalSolver::enumerate(garage, 278.2).size(), 187u);
    // (4 pi / 3) 60 (278.2 / 343)^3 + (pi / 4) 98 (278.2 / 343)^2 + (50 / 8) (278.2 / 343)
    // = 134.11 + 50.63 + 5.07 = 189.80.
    EXPECT_NEAR(SushiDSP::room::ModalSolver::estimated_count(garage, 278.2),
                weyl_count(garage, 278.2), 1e-9);
    EXPECT_NEAR(SushiDSP::room::ModalSolver::estimated_count(garage, 278.2), 190.0, 1.0);
}

TEST(Unit_Dsp, ModalSolverStaysUnderTheModeCap)
{
    const std::array<SushiDSP::room::RoomScene, 3> rooms = { SushiDSP::test::garage_scene(),
                                                             SushiDSP::test::bedroom_scene(),
                                                             SushiDSP::test::block_room_scene() };
    for (std::size_t index = 0; index < rooms.size(); ++index)
    {
        const double schroeder = SushiDSP::room::EyringDecay::schroeder_frequency_hz(rooms[index]);
        const std::size_t count =
            SushiDSP::room::ModalSolver::enumerate(rooms[index], schroeder).size();
        EXPECT_LT(count, SushiDSP::room::ModalDesign::kMaxModes) << "room " << index;
        EXPECT_NEAR(static_cast<double>(count),
                    SushiDSP::room::ModalSolver::estimated_count(rooms[index], schroeder),
                    0.02 * static_cast<double>(count))
            << "room " << index;
    }
}

TEST(Unit_Dsp, ModalSolverLowersTheCrossoverWhenTheCountWouldOverflow)
{
    const SushiDSP::room::RoomScene room = small_concrete_room();
    ASSERT_GT(SushiDSP::room::ModalSolver::enumerate(room, 900.0).size(),
              SushiDSP::room::ModalDesign::kMaxModes);
    const auto design = std::make_unique<SushiDSP::room::ModalDesign>(
        SushiDSP::room::ModalSolver::design(room, kSampleRate, 900.0));
    EXPECT_EQ(design->count, SushiDSP::room::ModalDesign::kMaxModes);
    EXPECT_LT(design->crossover_hz, 900.0f);
    EXPECT_GT(design->crossover_hz, 0.0f);
    EXPECT_EQ(SushiDSP::room::ModalSolver::enumerate(room, design->crossover_hz).size(),
              SushiDSP::room::ModalDesign::kMaxModes);
}

TEST(Unit_Dsp, ModalSolverSwitchesOffUnderFortyHertz)
{
    const auto design = std::make_unique<SushiDSP::room::ModalDesign>(
        SushiDSP::room::ModalSolver::design(SushiDSP::test::garage_scene(), kSampleRate, 39.0));
    EXPECT_EQ(design->count, 0u);
    EXPECT_EQ(design->crossover_hz, 0.0f);
    EXPECT_EQ(design->level_scale, 0.0f);
}

TEST(Unit_Dsp, ModalSolverPutsThePressureInTheCorners)
{
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const SushiDSP::room::ModalSolver::Mode axial{ 1, 0, 0, 0.0, 0.0 };
    // cos(pi 0.05 / 6) = 0.999657325 and cos(pi / 2) = 0.
    EXPECT_NEAR(SushiDSP::room::ModalSolver::mode_shape(axial, garage, { 0.05, 1.0, 1.0 }),
                std::cos(kPi * 0.05 / 6.0), 1e-9);
    EXPECT_NEAR(SushiDSP::room::ModalSolver::mode_shape(axial, garage, { 3.0, 1.0, 1.0 }), 0.0,
                1e-9);

    SushiDSP::room::RoomScene corner = garage;
    corner.source_position = { 0.05, 1.0, 1.0 };
    const auto design = std::make_unique<SushiDSP::room::ModalDesign>(
        SushiDSP::room::ModalSolver::design(corner, kSampleRate, 100.0));
    ASSERT_GT(design->count, 0u);
    EXPECT_NEAR(design->modes[0].source_gain, std::cos(kPi * 0.05 / 6.0), 1e-6);
}

TEST(Unit_Dsp, ModalSolverDampsAnAxialModeByTheTwoWallsItBouncesBetween)
{
    using SushiDSP::room::MaterialId;
    using SushiDSP::room::Surface;
    SushiDSP::room::RoomScene soft = SushiDSP::test::garage_scene();
    soft.surfaces[static_cast<std::size_t>(Surface::MinusX)] = MaterialId::GypsumBoardOnStuds;
    soft.surfaces[static_cast<std::size_t>(Surface::PlusX)] = MaterialId::GypsumBoardOnStuds;

    const std::vector<SushiDSP::room::ModalSolver::Mode> modes =
        SushiDSP::room::ModalSolver::enumerate(soft, 100.0);
    const Triple along_x{ 1, 0, 0 };
    const Triple along_z{ 0, 0, 1 };
    const double decay_x = find_mode(modes, along_x).decay_seconds;
    const double decay_z = find_mode(modes, along_z).decay_seconds;

    // Both modes sit under 125 Hz, so gypsum holds 0.29, the doorway face 0.716 and concrete
    // 0.01. The soft pair counts twice for (1, 0, 0) and once for (0, 0, 1):
    // delta_x = (343 / 960) (2 (0.29 + 0.716) 10 + (0.02) 15 + (0.02) 24) = 7.467 nepers/s,
    // delta_z = (343 / 960) ((0.29 + 0.716) 10 + (0.02) 15 + 2 (0.02) 24) = 4.044 nepers/s,
    // a decay ratio of 1.846.
    EXPECT_NEAR(decay_x, decay_seconds(soft, along_x), 1e-9);
    EXPECT_NEAR(decay_z, decay_seconds(soft, along_z), 1e-9);
    EXPECT_NEAR(decay_z / decay_x, 1.8463, 1e-3);
    EXPECT_GT(decay_z / decay_x, 1.5);
}

TEST(Unit_Dsp, ModalSolverGivesTheTwoEarsDifferentGains)
{
    SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    // Facing +y puts the two ears 0.17 m apart along x, across the (1, 0, 0) gradient.
    garage.listener_yaw_rad = 0.5 * kPi;
    for (double x = 0.3; x < 5.85; x += 0.3)
    {
        garage.listener_position = { x, 2.0, 1.2 };
        const auto design = std::make_unique<SushiDSP::room::ModalDesign>(
            SushiDSP::room::ModalSolver::design(garage, kSampleRate, 100.0));
        ASSERT_GT(design->count, 0u);
        const double left = design->modes[0].ear_gain[0];
        const double right = design->modes[0].ear_gain[1];
        const double larger = std::max(std::abs(left), std::abs(right));
        EXPECT_GT(std::abs(left - right) / larger, 0.01) << "x = " << x;
        // Facing +y, the left ear sits at x - 0.085 and the right at x + 0.085.
        EXPECT_NEAR(left, std::cos(kPi * (x - 0.085) / 6.0), 1e-6) << "x = " << x;
        EXPECT_NEAR(right, std::cos(kPi * (x + 0.085) / 6.0), 1e-6) << "x = " << x;
    }
}

TEST(Unit_Dsp, ModalSolverWritesEachModesResonatorFromItsFrequencyAndDecay)
{
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const double crossover = 278.2;
    const std::vector<SushiDSP::room::ModalSolver::Mode> modes =
        SushiDSP::room::ModalSolver::enumerate(garage, crossover);
    const auto design = std::make_unique<SushiDSP::room::ModalDesign>(
        SushiDSP::room::ModalSolver::design(garage, kSampleRate, crossover));
    ASSERT_EQ(design->count, modes.size());
    for (std::size_t index = 0; index < modes.size(); ++index)
    {
        const double radius = std::pow(10.0, -3.0 / (modes[index].decay_seconds * kSampleRate));
        const double theta = 2.0 * kPi * modes[index].frequency_hz / kSampleRate;
        const std::array<double, 5>& section = design->modes[index].section;
        const int live_indices = (modes[index].nx != 0u) + (modes[index].ny != 0u)
            + (modes[index].nz != 0u);
        const double norm = std::pow(0.5, live_indices);
        EXPECT_NEAR(section[0], design->level_scale / norm, 1e-6 * section[0]) << "mode " << index;
        EXPECT_EQ(section[1], 0.0) << "mode " << index;
        EXPECT_EQ(section[2], -section[0]) << "mode " << index;
        EXPECT_NEAR(section[3], -2.0 * radius * std::cos(theta), 1e-12) << "mode " << index;
        EXPECT_NEAR(section[4], radius * radius, 1e-12) << "mode " << index;
    }
}

TEST(Unit_Dsp, ModalSolverMatchesTheOctaveBelowTheCrossoverToEyring)
{
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const double crossover = 278.2;
    const std::vector<SushiDSP::room::ModalSolver::Mode> modes =
        SushiDSP::room::ModalSolver::enumerate(garage, crossover);
    const auto design = std::make_unique<SushiDSP::room::ModalDesign>(
        SushiDSP::room::ModalSolver::design(garage, kSampleRate, crossover));

    // Averaged over the cabinet and the listener, one mode's power for unit white input is
    // level_scale^2 times the section's 2 / (1 - r^2); the octave [139.1, 278.2) sum must equal
    // the diffuse-field power Eyring leaves in that octave.
    double power_sum = 0.0;
    for (const SushiDSP::room::ModalSolver::Mode& mode : modes)
    {
        if (mode.frequency_hz < 0.5 * crossover)
        {
            continue;
        }
        const double radius = std::pow(10.0, -3.0 / (mode.decay_seconds * kSampleRate));
        power_sum += 2.0 / (1.0 - radius * radius);
    }
    const double expected = eyring_band_power(garage, 0.5 * crossover, crossover);
    const double scale = static_cast<double>(design->level_scale);
    EXPECT_GT(scale, 0.0);
    EXPECT_NEAR(scale * scale * power_sum, expected, 1e-5 * expected);
}
