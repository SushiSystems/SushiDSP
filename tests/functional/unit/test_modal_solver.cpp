/**************************************************************************/
/* test_modal_solver.cpp                                                  */
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

#include "room_fixtures.hpp"
#include <SushiDSP/nodes/room/band_response.hpp>
#include <SushiDSP/nodes/room/cabinet_directivity.hpp>
#include <SushiDSP/nodes/room/diffuse_field.hpp>
#include <SushiDSP/nodes/room/eyring_decay.hpp>
#include <SushiDSP/nodes/room/furniture.hpp>
#include <SushiDSP/nodes/room/image_source_solver.hpp>
#include <SushiDSP/nodes/room/materials.hpp>
#include <SushiDSP/nodes/room/modal_design.hpp>
#include <SushiDSP/nodes/room/modal_solver.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

namespace
{
    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Holds the sample rate every design in this file is built at.
    constexpr double K_SAMPLE_RATE = 48000.0;

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
        return 0.5 * SushiDSP::room::K_SPEED_OF_SOUND * std::sqrt(x * x + y * y + z * z);
    }

    /// @brief Evaluates Weyl's (4 pi / 3) V (f / c)^3 + (pi / 4) S (f / c)^2 + (L / 8) (f / c)
    /// with L = 4 (Lx + Ly + Lz).
    double weyl_count(const SushiDSP::room::RoomScene& scene, double frequency_hz)
    {
        const double k = frequency_hz / SushiDSP::room::K_SPEED_OF_SOUND;
        const double edges = 4.0 * (scene.dimensions.x + scene.dimensions.y + scene.dimensions.z);
        return (4.0 * K_PI / 3.0) * scene.volume_m3() * k * k * k +
               (K_PI / 4.0) * scene.surface_area_m2() * k * k + (edges / 8.0) * k;
    }

    /// @brief Returns the mode with the given indices out of a sorted enumeration.
    const SushiDSP::room::ModalSolver::Mode&
    find_mode(const std::vector<SushiDSP::room::ModalSolver::Mode>& modes, const Triple& triple)
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
        const std::array<double, SushiDSP::room::K_BAND_COUNT> rows =
            scene.face_absorption(surface);
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
        const std::array<std::uint32_t, 3> indices = {triple.nx, triple.ny, triple.nz};
        double weighted = 0.0;
        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            const Surface low = static_cast<Surface>(2 * axis);
            const Surface high = static_cast<Surface>(2 * axis + 1);
            const double epsilon = indices[axis] == 0u ? 1.0 : 2.0;
            weighted += epsilon * scene.face_area_m2(low) *
                        (face_absorption_at(scene, low, frequency) +
                         face_absorption_at(scene, high, frequency));
        }
        return SushiDSP::room::K_SPEED_OF_SOUND * weighted / (16.0 * scene.volume_m3());
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
        scene.dimensions = {3.0, 2.5, 2.4};
        scene.surfaces.fill(SushiDSP::room::MaterialId::ConcreteUnpainted);
        scene.scattering.fill(SushiDSP::room::Scattering::Flat);
        scene.patch_count = 0;
        scene.source_position = {0.3, 0.3, 0.3};
        scene.listener_position = {1.5, 1.25, 1.2};
        scene.microphone_position = {0.5, 0.5, 0.5};
        return scene;
    }

    /// @brief Interpolates six band values in log frequency, held flat outside the centres.
    double band_value_at(const std::array<double, SushiDSP::room::K_BAND_COUNT>& values,
                         double frequency_hz)
    {
        const auto& centres = SushiDSP::room::K_BAND_CENTRES_HZ;
        if (frequency_hz <= centres.front())
        {
            return values.front();
        }
        for (std::size_t band = 1; band < SushiDSP::room::K_BAND_COUNT; ++band)
        {
            if (frequency_hz <= centres[band])
            {
                const double position = std::log(frequency_hz / centres[band - 1]) /
                                        std::log(centres[band] / centres[band - 1]);
                return values[band - 1] + (values[band] - values[band - 1]) * position;
            }
        }
        return values.back();
    }

    /// @brief Returns 16 pi RT60(f) / (0.161 V) times the cabinet's mean radiated power
    /// 10^(-DI(f) / 10) at each band centre, from EyringDecay and CabinetDirectivity alone.
    std::array<double, SushiDSP::room::K_BAND_COUNT>
    diffuse_band_power(const SushiDSP::room::RoomScene& scene)
    {
        const std::array<double, SushiDSP::room::K_BAND_COUNT> rt60 =
            SushiDSP::room::EyringDecay::rt60_seconds(scene);
        const std::array<float, SushiDSP::room::K_BAND_COUNT> index =
            SushiDSP::room::CabinetDirectivity(scene.cabinet).directivity_index_db();
        std::array<double, SushiDSP::room::K_BAND_COUNT> power{};
        for (std::size_t band = 0; band < SushiDSP::room::K_BAND_COUNT; ++band)
        {
            power[band] = 16.0 * K_PI * rt60[band] / (0.161 * scene.volume_m3()) *
                          std::pow(10.0, -0.1 * static_cast<double>(index[band]));
        }
        return power;
    }

    /// @brief Integrates the diffuse field's power per hertz times a 2 / fs density over
    /// [lower, upper] by the midpoint rule.
    double diffuse_band_power(const SushiDSP::room::RoomScene& scene, double lower, double upper)
    {
        constexpr int K_STEPS = 65536;
        const std::array<double, SushiDSP::room::K_BAND_COUNT> power = diffuse_band_power(scene);
        const double width = (upper - lower) / K_STEPS;
        double sum = 0.0;
        for (int step = 0; step < K_STEPS; ++step)
        {
            sum += band_value_at(power, lower + (step + 0.5) * width) * width;
        }
        return (2.0 / K_SAMPLE_RATE) * sum;
    }

    /// @brief Returns the garage with three wardrobe modules and two two-seat sofas standing in
    /// it.
    SushiDSP::room::RoomScene furnished_garage()
    {
        SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
        scene.furnishings[0] = {SushiDSP::room::FurnitureId::WardrobeModule, 3.0};
        scene.furnishings[1] = {SushiDSP::room::FurnitureId::SofaTwoSeat, 2.0};
        scene.furnishing_count = 2;
        return scene;
    }

    /// @brief Evaluates A_f / (4 V) at the 125 Hz and 250 Hz bands and interpolates it in log
    /// frequency, held flat below 125 Hz, in energy attenuation per metre.
    double furnishing_coefficient_at(const SushiDSP::room::RoomScene& scene, double frequency_hz)
    {
        const std::array<double, SushiDSP::room::K_BAND_COUNT> area =
            scene.furnishing_absorption_area();
        const double low = area[0] / (4.0 * scene.volume_m3());
        const double high = area[1] / (4.0 * scene.volume_m3());
        const double position = std::log(frequency_hz / 125.0) / std::log(2.0);
        const double clamped = std::min(1.0, std::max(0.0, position));
        return low + (high - low) * clamped;
    }

    /// @brief Returns the modal design of a scene for the ears, heap-held.
    std::unique_ptr<SushiDSP::room::ModalDesign> design_of(const SushiDSP::room::RoomScene& scene,
                                                           double crossover_hz)
    {
        return std::make_unique<SushiDSP::room::ModalDesign>(SushiDSP::room::ModalSolver::design(
            scene, K_SAMPLE_RATE, crossover_hz, SushiDSP::room::ListenerKind::Ears));
    }
} // namespace

TEST(Unit_Dsp, DiffuseFieldIsEyringOverTheCabinetsDirectivity)
{
    // The field is Eyring over the 4x12's directivity. See tests/functional/unit/README.md.
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const SushiDSP::room::DiffuseField field(garage);
    const std::array<double, SushiDSP::room::K_BAND_COUNT> expected = diffuse_band_power(garage);
    const std::array<double, 6> rounded = {5.39, 3.48, 1.05, 0.376, 0.154, 0.0610};
    for (std::size_t band = 0; band < SushiDSP::room::K_BAND_COUNT; ++band)
    {
        EXPECT_NEAR(field.band_power()[band], expected[band], 1e-9 * expected[band])
            << "band " << band;
        EXPECT_NEAR(field.band_power()[band], rounded[band], 0.005 * rounded[band])
            << "band " << band;
    }

    // Between the centres the field is linear in log frequency, flat outside them.
    EXPECT_NEAR(field.power_at(60.0), expected[0], 1e-12);
    EXPECT_NEAR(field.power_at(std::sqrt(250.0 * 500.0)), 0.5 * (expected[1] + expected[2]), 1e-12);
    EXPECT_NEAR(field.power_at(10000.0), expected[5], 1e-12);

    // The integral across two centres and past the last one matches the midpoint rule.
    const double across = diffuse_band_power(garage, 139.09, 278.18) * K_SAMPLE_RATE / 2.0;
    EXPECT_NEAR(field.power_between(139.09, 278.18), across, 1e-6 * across);
    const double past = diffuse_band_power(garage, 3000.0, 6000.0) * K_SAMPLE_RATE / 2.0;
    EXPECT_NEAR(field.power_between(3000.0, 6000.0), past, 1e-6 * past);

    // The field is the cabinet's, not the room's alone. See tests/functional/unit/README.md.
    SushiDSP::room::RoomScene single = garage;
    single.cabinet = SushiDSP::room::K_CABINET1X12;
    const SushiDSP::room::DiffuseField single_field(single);
    EXPECT_GT(single_field.band_power()[2], field.band_power()[2]);
    EXPECT_NEAR(single_field.band_power()[2], diffuse_band_power(single)[2],
                1e-9 * single_field.band_power()[2]);
}

TEST(Unit_Dsp, ModalSolverFindsTheGaragesEigenfrequencies)
{
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const std::vector<SushiDSP::room::ModalSolver::Mode> modes =
        SushiDSP::room::ModalSolver::enumerate(garage, 100.0);
    // f = (c/2) sqrt((nx/6)^2 + (ny/4)^2 + (nz/2.5)^2). See tests/functional/unit/README.md.
    const std::array<Triple, 8> triples = {Triple{1, 0, 0}, Triple{0, 1, 0}, Triple{1, 1, 0},
                                           Triple{2, 0, 0}, Triple{0, 0, 1}, Triple{2, 1, 0},
                                           Triple{1, 0, 1}, Triple{0, 1, 1}};
    const std::array<double, 8> rounded = {28.58, 42.88, 51.53, 57.17, 68.60, 71.46, 74.32, 80.90};
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
    // Weyl's count is 134.11 + 50.63 + 5.07 = 189.80. See tests/functional/unit/README.md.
    EXPECT_NEAR(SushiDSP::room::ModalSolver::estimated_count(garage, 278.2),
                weyl_count(garage, 278.2), 1e-9);
    EXPECT_NEAR(SushiDSP::room::ModalSolver::estimated_count(garage, 278.2), 190.0, 1.0);
}

TEST(Unit_Dsp, ModalSolverStaysUnderTheModeCap)
{
    const std::array<SushiDSP::room::RoomScene, 3> rooms = {SushiDSP::test::garage_scene(),
                                                            SushiDSP::test::bedroom_scene(),
                                                            SushiDSP::test::block_room_scene()};
    for (std::size_t index = 0; index < rooms.size(); ++index)
    {
        const double schroeder = SushiDSP::room::EyringDecay::schroeder_frequency_hz(rooms[index]);
        const std::size_t count =
            SushiDSP::room::ModalSolver::enumerate(rooms[index], schroeder).size();
        EXPECT_LT(count, SushiDSP::room::ModalDesign::K_MAX_MODES) << "room " << index;
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
              SushiDSP::room::ModalDesign::K_MAX_MODES);
    const auto design =
        std::make_unique<SushiDSP::room::ModalDesign>(SushiDSP::room::ModalSolver::design(
            room, K_SAMPLE_RATE, 900.0, SushiDSP::room::ListenerKind::Ears));
    EXPECT_EQ(design->count, SushiDSP::room::ModalDesign::K_MAX_MODES);
    EXPECT_LT(design->crossover_hz, 900.0f);
    EXPECT_GT(design->crossover_hz, 0.0f);
    EXPECT_EQ(SushiDSP::room::ModalSolver::enumerate(room, design->crossover_hz).size(),
              SushiDSP::room::ModalDesign::K_MAX_MODES);
}

TEST(Unit_Dsp, ModalSolverSwitchesOffUnderFortyHertz)
{
    const auto design = std::make_unique<SushiDSP::room::ModalDesign>(
        SushiDSP::room::ModalSolver::design(SushiDSP::test::garage_scene(), K_SAMPLE_RATE, 39.0,
                                            SushiDSP::room::ListenerKind::Ears));
    EXPECT_EQ(design->count, 0u);
    EXPECT_EQ(design->crossover_hz, 0.0f);
    EXPECT_EQ(design->level_scale, 0.0f);
}

TEST(Unit_Dsp, ModalSolverPutsThePressureInTheCorners)
{
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const SushiDSP::room::ModalSolver::Mode axial{1, 0, 0, 0.0, 0.0};
    // cos(pi 0.05 / 6) = 0.999657325 and cos(pi / 2) = 0.
    EXPECT_NEAR(SushiDSP::room::ModalSolver::mode_shape(axial, garage, {0.05, 1.0, 1.0}),
                std::cos(K_PI * 0.05 / 6.0), 1e-9);
    EXPECT_NEAR(SushiDSP::room::ModalSolver::mode_shape(axial, garage, {3.0, 1.0, 1.0}), 0.0, 1e-9);

    SushiDSP::room::RoomScene corner = garage;
    corner.source_position = {0.05, 1.0, 1.0};
    const auto design =
        std::make_unique<SushiDSP::room::ModalDesign>(SushiDSP::room::ModalSolver::design(
            corner, K_SAMPLE_RATE, 100.0, SushiDSP::room::ListenerKind::Ears));
    ASSERT_GT(design->count, 0u);
    EXPECT_NEAR(design->modes[0].source_gain, std::cos(K_PI * 0.05 / 6.0), 1e-6);
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
    const Triple along_x{1, 0, 0};
    const Triple along_z{0, 0, 1};
    const double decay_x = find_mode(modes, along_x).decay_seconds;
    const double decay_z = find_mode(modes, along_z).decay_seconds;

    // The decay ratio of the two modes is 1.846. See tests/functional/unit/README.md.
    EXPECT_NEAR(decay_x, decay_seconds(soft, along_x), 1e-9);
    EXPECT_NEAR(decay_z, decay_seconds(soft, along_z), 1e-9);
    EXPECT_NEAR(decay_z / decay_x, 1.8463, 1e-3);
    EXPECT_GT(decay_z / decay_x, 1.5);
}

TEST(Unit_Dsp, ModalSolverGivesTheTwoEarsDifferentGains)
{
    SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    // Facing +y puts the two ears 0.17 m apart along x, across the (1, 0, 0) gradient.
    garage.listener_yaw_rad = 0.5 * K_PI;
    for (double x = 0.3; x < 5.85; x += 0.3)
    {
        garage.listener_position = {x, 2.0, 1.2};
        const auto design =
            std::make_unique<SushiDSP::room::ModalDesign>(SushiDSP::room::ModalSolver::design(
                garage, K_SAMPLE_RATE, 100.0, SushiDSP::room::ListenerKind::Ears));
        ASSERT_GT(design->count, 0u);
        const double left = design->modes[0].ear_gain[0];
        const double right = design->modes[0].ear_gain[1];
        const double larger = std::max(std::abs(left), std::abs(right));
        EXPECT_GT(std::abs(left - right) / larger, 0.01) << "x = " << x;
        // Facing +y, the left ear sits at x - 0.085 and the right at x + 0.085.
        EXPECT_NEAR(left, std::cos(K_PI * (x - 0.085) / 6.0), 1e-6) << "x = " << x;
        EXPECT_NEAR(right, std::cos(K_PI * (x + 0.085) / 6.0), 1e-6) << "x = " << x;
    }
}

TEST(Unit_Dsp, ModalSolverWritesEachModesResonatorFromItsFrequencyAndDecay)
{
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const double crossover = 278.2;
    const std::vector<SushiDSP::room::ModalSolver::Mode> modes =
        SushiDSP::room::ModalSolver::enumerate(garage, crossover);
    const auto design =
        std::make_unique<SushiDSP::room::ModalDesign>(SushiDSP::room::ModalSolver::design(
            garage, K_SAMPLE_RATE, crossover, SushiDSP::room::ListenerKind::Ears));
    ASSERT_EQ(design->count, modes.size());
    for (std::size_t index = 0; index < modes.size(); ++index)
    {
        const double radius = std::pow(10.0, -3.0 / (modes[index].decay_seconds * K_SAMPLE_RATE));
        const double theta = 2.0 * K_PI * modes[index].frequency_hz / K_SAMPLE_RATE;
        const std::array<double, 5>& section = design->modes[index].section;
        const int live_indices =
            (modes[index].nx != 0u) + (modes[index].ny != 0u) + (modes[index].nz != 0u);
        const double norm = std::pow(0.5, live_indices);
        EXPECT_NEAR(section[0], design->level_scale / norm, 1e-6 * section[0]) << "mode " << index;
        EXPECT_EQ(section[1], 0.0) << "mode " << index;
        EXPECT_EQ(section[2], -section[0]) << "mode " << index;
        EXPECT_NEAR(section[3], -2.0 * radius * std::cos(theta), 1e-12) << "mode " << index;
        EXPECT_NEAR(section[4], radius * radius, 1e-12) << "mode " << index;
    }
}

TEST(Unit_Dsp, ModalSolverMatchesTheOctaveBelowTheCrossoverToTheDiffuseField)
{
    // The octave below f_s holds the diffuse field's power. See tests/functional/unit/README.md.
    const std::array<SushiDSP::room::RoomScene, 3> rooms = {SushiDSP::test::garage_scene(),
                                                            SushiDSP::test::bedroom_scene(),
                                                            SushiDSP::test::block_room_scene()};
    for (std::size_t room = 0; room < rooms.size(); ++room)
    {
        const double crossover = SushiDSP::room::EyringDecay::schroeder_frequency_hz(rooms[room]);
        const std::vector<SushiDSP::room::ModalSolver::Mode> modes =
            SushiDSP::room::ModalSolver::enumerate(rooms[room], crossover);
        const auto design =
            std::make_unique<SushiDSP::room::ModalDesign>(SushiDSP::room::ModalSolver::design(
                rooms[room], K_SAMPLE_RATE, crossover, SushiDSP::room::ListenerKind::Ears));
        double power_sum = 0.0;
        for (const SushiDSP::room::ModalSolver::Mode& mode : modes)
        {
            if (mode.frequency_hz < 0.5 * crossover)
            {
                continue;
            }
            const double radius = std::pow(10.0, -3.0 / (mode.decay_seconds * K_SAMPLE_RATE));
            power_sum += 2.0 / (1.0 - radius * radius);
        }
        const double expected =
            diffuse_band_power(rooms[room], 0.5 * static_cast<double>(design->crossover_hz),
                               static_cast<double>(design->crossover_hz));
        const double scale = static_cast<double>(design->level_scale);
        EXPECT_GT(scale, 0.0) << "room " << room;
        EXPECT_NEAR(scale * scale * power_sum, expected, 1e-5 * expected) << "room " << room;
    }

    // The garage's octave holds 2.334e-2 of the variance. See tests/functional/unit/README.md.
    const SushiDSP::room::RoomScene garage = rooms[0];
    const double garage_octave = diffuse_band_power(garage, 139.09, 278.18);
    EXPECT_NEAR(garage_octave, 2.334e-2, 0.003e-2);
    const std::array<double, SushiDSP::room::K_BAND_COUNT> rt60 =
        SushiDSP::room::EyringDecay::rt60_seconds(garage);
    std::array<double, SushiDSP::room::K_BAND_COUNT> eyring{};
    for (std::size_t band = 0; band < SushiDSP::room::K_BAND_COUNT; ++band)
    {
        eyring[band] = 16.0 * K_PI * rt60[band] / (0.161 * garage.volume_m3());
    }
    double eyring_alone = 0.0;
    constexpr int K_STEPS = 4096;
    for (int step = 0; step < K_STEPS; ++step)
    {
        eyring_alone += band_value_at(eyring, 139.09 + (step + 0.5) * 139.09 / K_STEPS) * 139.09 /
                        K_STEPS * 2.0 / K_SAMPLE_RATE;
    }
    EXPECT_NEAR(eyring_alone, 4.02e-2, 0.01e-2);
    EXPECT_GT(garage_octave / eyring_alone, std::pow(10.0, -0.299));
    EXPECT_LT(garage_octave / eyring_alone, std::pow(10.0, -0.112));
}

TEST(Unit_Dsp, ModalSolverReadsTheMicrophoneAtOnePoint)
{
    // The ears sit 8.5 cm to either side of the listener. See tests/functional/unit/README.md.
    SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
    scene.source_position = {0.4, 0.5, 0.5};
    scene.listener_position = {5.4, 3.4, 2.0};
    scene.microphone_position = {1.0, 0.5, 0.5};
    const double crossover = SushiDSP::room::EyringDecay::schroeder_frequency_hz(scene);
    const auto ears =
        std::make_unique<SushiDSP::room::ModalDesign>(SushiDSP::room::ModalSolver::design(
            scene, K_SAMPLE_RATE, crossover, SushiDSP::room::ListenerKind::Ears));
    const auto microphone =
        std::make_unique<SushiDSP::room::ModalDesign>(SushiDSP::room::ModalSolver::design(
            scene, K_SAMPLE_RATE, crossover, SushiDSP::room::ListenerKind::Microphone));
    const std::vector<SushiDSP::room::ModalSolver::Mode> modes =
        SushiDSP::room::ModalSolver::enumerate(scene, crossover);
    ASSERT_EQ(ears->count, modes.size());
    ASSERT_EQ(microphone->count, ears->count);
    EXPECT_EQ(microphone->crossover_hz, ears->crossover_hz);
    EXPECT_EQ(microphone->level_scale, ears->level_scale);

    const SushiDSP::room::Vec3 left = {5.4, 3.485, 2.0};
    const SushiDSP::room::Vec3 right = {5.4, 3.315, 2.0};
    std::uint32_t differing = 0;
    for (std::size_t index = 0; index < modes.size(); ++index)
    {
        const float at_microphone = static_cast<float>(SushiDSP::room::ModalSolver::mode_shape(
            modes[index], scene, scene.microphone_position));
        EXPECT_EQ(microphone->modes[index].ear_gain[0], at_microphone) << "mode " << index;
        EXPECT_EQ(microphone->modes[index].ear_gain[1], at_microphone) << "mode " << index;
        EXPECT_EQ(microphone->modes[index].section, ears->modes[index].section) << "mode " << index;
        EXPECT_EQ(microphone->modes[index].source_gain, ears->modes[index].source_gain)
            << "mode " << index;
        EXPECT_FLOAT_EQ(
            ears->modes[index].ear_gain[0],
            static_cast<float>(SushiDSP::room::ModalSolver::mode_shape(modes[index], scene, left)))
            << "mode " << index;
        EXPECT_FLOAT_EQ(
            ears->modes[index].ear_gain[1],
            static_cast<float>(SushiDSP::room::ModalSolver::mode_shape(modes[index], scene, right)))
            << "mode " << index;
        differing +=
            microphone->modes[index].ear_gain[0] != ears->modes[index].ear_gain[0] ? 1u : 0u;
    }
    EXPECT_GT(differing, modes.size() / 2);
}

TEST(Unit_Dsp, ModalSolverLeavesAnUnfurnishedRoomAlone)
{
    const SushiDSP::room::RoomScene bare = SushiDSP::test::garage_scene();
    SushiDSP::room::RoomScene junk = bare;
    junk.furnishings.fill({SushiDSP::room::FurnitureId::SofaTwoSeat, 9.0});
    junk.furnishing_count = 0;

    constexpr double K_CROSSOVER = 278.2;
    const std::vector<SushiDSP::room::ModalSolver::Mode> bare_modes =
        SushiDSP::room::ModalSolver::enumerate(bare, K_CROSSOVER);
    const std::vector<SushiDSP::room::ModalSolver::Mode> junk_modes =
        SushiDSP::room::ModalSolver::enumerate(junk, K_CROSSOVER);
    ASSERT_EQ(junk_modes.size(), bare_modes.size());

    const auto bare_design = design_of(bare, K_CROSSOVER);
    const auto junk_design = design_of(junk, K_CROSSOVER);
    ASSERT_EQ(junk_design->count, bare_design->count);
    EXPECT_EQ(junk_design->crossover_hz, bare_design->crossover_hz);
    EXPECT_EQ(junk_design->level_scale, bare_design->level_scale);
    for (std::size_t index = 0; index < bare_modes.size(); ++index)
    {
        EXPECT_EQ(junk_modes[index].frequency_hz, bare_modes[index].frequency_hz)
            << "mode " << index;
        EXPECT_EQ(junk_modes[index].decay_seconds, bare_modes[index].decay_seconds)
            << "mode " << index;
        EXPECT_EQ(junk_design->modes[index].section, bare_design->modes[index].section)
            << "mode " << index;
        EXPECT_EQ(junk_design->modes[index].source_gain, bare_design->modes[index].source_gain)
            << "mode " << index;
        EXPECT_EQ(junk_design->modes[index].ear_gain, bare_design->modes[index].ear_gain)
            << "mode " << index;
    }
}

TEST(Unit_Dsp, ModalSolverKeepsTheModeFrequenciesWhenFurnished)
{
    const SushiDSP::room::RoomScene bare = SushiDSP::test::garage_scene();
    const SushiDSP::room::RoomScene furnished = furnished_garage();

    constexpr double K_CROSSOVER = 278.2;
    const std::vector<SushiDSP::room::ModalSolver::Mode> bare_modes =
        SushiDSP::room::ModalSolver::enumerate(bare, K_CROSSOVER);
    const std::vector<SushiDSP::room::ModalSolver::Mode> furnished_modes =
        SushiDSP::room::ModalSolver::enumerate(furnished, K_CROSSOVER);
    ASSERT_EQ(furnished_modes.size(), bare_modes.size());
    for (std::size_t index = 0; index < bare_modes.size(); ++index)
    {
        EXPECT_EQ(furnished_modes[index].frequency_hz, bare_modes[index].frequency_hz)
            << "mode " << index;
        EXPECT_EQ(furnished_modes[index].nx, bare_modes[index].nx) << "mode " << index;
        EXPECT_EQ(furnished_modes[index].ny, bare_modes[index].ny) << "mode " << index;
        EXPECT_EQ(furnished_modes[index].nz, bare_modes[index].nz) << "mode " << index;
        EXPECT_LT(furnished_modes[index].decay_seconds, bare_modes[index].decay_seconds)
            << "mode " << index;
    }

    const auto bare_design = design_of(bare, K_CROSSOVER);
    const auto furnished_design = design_of(furnished, K_CROSSOVER);
    ASSERT_EQ(furnished_design->count, bare_design->count);
    EXPECT_EQ(furnished_design->crossover_hz, bare_design->crossover_hz);
    for (std::size_t index = 0; index < bare_modes.size(); ++index)
    {
        EXPECT_EQ(furnished_design->modes[index].source_gain, bare_design->modes[index].source_gain)
            << "mode " << index;
        EXPECT_EQ(furnished_design->modes[index].ear_gain, bare_design->modes[index].ear_gain)
            << "mode " << index;
    }
}

TEST(Unit_Dsp, ModalSolverAddsTheFurnishingDampingToEveryMode)
{
    const SushiDSP::room::RoomScene bare = SushiDSP::test::garage_scene();
    const SushiDSP::room::RoomScene furnished = furnished_garage();
    constexpr double K_DECAY_NEPERS = 3.0 * 2.302585092994046;

    // The table's 3 wardrobe modules and 2 sofas over 4 V = 240 m3.
    const auto& wardrobe =
        SushiDSP::room::furniture(SushiDSP::room::FurnitureId::WardrobeModule).absorption_area;
    const auto& sofa =
        SushiDSP::room::furniture(SushiDSP::room::FurnitureId::SofaTwoSeat).absorption_area;
    const double low = furnishing_coefficient_at(furnished, 125.0);
    const double high = furnishing_coefficient_at(furnished, 250.0);
    EXPECT_NEAR(low, (3.0 * wardrobe[0] + 2.0 * sofa[0]) / 240.0, 1e-9);
    EXPECT_NEAR(high, (3.0 * wardrobe[1] + 2.0 * sofa[1]) / 240.0, 1e-9);

    constexpr double K_CROSSOVER = 278.2;
    const std::vector<SushiDSP::room::ModalSolver::Mode> bare_modes =
        SushiDSP::room::ModalSolver::enumerate(bare, K_CROSSOVER);
    const std::vector<SushiDSP::room::ModalSolver::Mode> furnished_modes =
        SushiDSP::room::ModalSolver::enumerate(furnished, K_CROSSOVER);
    ASSERT_EQ(furnished_modes.size(), bare_modes.size());
    const auto design = design_of(furnished, K_CROSSOVER);
    ASSERT_EQ(design->count, furnished_modes.size());

    bool below_low_band = false;
    bool between_bands = false;
    bool above_high_band = false;
    for (std::size_t index = 0; index < bare_modes.size(); ++index)
    {
        const double frequency = bare_modes[index].frequency_hz;
        below_low_band = below_low_band || frequency < 125.0;
        between_bands = between_bands || (frequency > 125.0 && frequency < 250.0);
        above_high_band = above_high_band || frequency > 250.0;

        // A decay of T seconds is delta = 3 ln(10) / T nepers per second.
        const double bare_delta = K_DECAY_NEPERS / bare_modes[index].decay_seconds;
        const double furnished_delta = K_DECAY_NEPERS / furnished_modes[index].decay_seconds;
        const double expected = 0.5 * SushiDSP::room::K_SPEED_OF_SOUND *
                                furnishing_coefficient_at(furnished, frequency);
        EXPECT_GT(expected, 0.0) << "mode " << index;
        EXPECT_NEAR(furnished_delta - bare_delta, expected, 1e-9 * expected) << "mode " << index;

        // The pole radius r = 10^(-3 / (T fs)) gives r^2 = exp(-2 delta / fs).
        const double designed_delta =
            -0.5 * K_SAMPLE_RATE * std::log(design->modes[index].section[4]);
        EXPECT_NEAR(designed_delta, furnished_delta, 1e-9 * furnished_delta) << "mode " << index;
    }
    EXPECT_TRUE(below_low_band);
    EXPECT_TRUE(between_bands);
    EXPECT_TRUE(above_high_band);
}
