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
 * @file test_modal_solver.cpp
 * @brief Tests for ModalSolver, the shoebox's eigenmodes, their damping, their gains and the
 * seam level.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <SushiDSP/foundation/log_capture/log_capture.hpp>
#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>
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

#include "support/tolerance.hpp"

namespace
{
    using SushiDSP::room::DiffuseField;
    using SushiDSP::room::EyringDecay;
    using SushiDSP::room::ListenerKind;
    using SushiDSP::room::ModalDesign;
    using SushiDSP::room::ModalSolver;
    using SushiDSP::room::RoomScene;

    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Holds the sample rate every design in this file is built at.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief Holds the ratio of error to bound above which a derived check fails.
    constexpr double K_LIMIT = TestSupport::K_DEFAULT_THRESHOLD;

    /// @brief Holds the double roundoff.
    constexpr double K_U = TestSupport::unit_roundoff<double>();

    /// @brief Holds how far the midpoint rule over a field's power may sit from the field's own
    /// integral, relative. Characterised, not derived: the rule's own error is about 1e-10 by
    /// Euler-Maclaurin at 65536 steps, and this is the figure the old test held.
    constexpr double K_INTEGRAL_RELATIVE_TOLERANCE = 1e-6;

    /// @brief Holds how far Weyl's estimate may sit from the enumerated count, relative.
    /// Characterised, not derived: Weyl's law is asymptotic.
    constexpr double K_ESTIMATE_RELATIVE_TOLERANCE = 0.02;

    /// @brief Holds how far the modal power of the octave below the crossover may sit from the
    /// diffuse field's, relative. Characterised, not derived: the octave's edges split modes.
    constexpr double K_OCTAVE_POWER_RELATIVE_TOLERANCE = 1e-5;

    /// @brief Holds the least relative difference between the two ears' gains at a mode, which the
    /// ears 0.17 m apart across the gradient must show. Characterised, not derived.
    constexpr double K_EAR_DIFFERENCE_FRACTION = 0.01;

    /// @brief Holds the decay ratio of the two garage modes the soft walls separate, worked out
    /// by hand to three decimals.
    constexpr double K_SOFT_DECAY_RATIO = 1.846;

    /// @brief Holds the crossover the modal designs of the garage are built at.
    constexpr double K_CROSSOVER = 278.2;

    /// @brief Holds the energy decay constant of 60 dB over one second, 3 ln 10, in nepers.
    constexpr double K_DECAY_NEPERS = 3.0 * 2.302585092994046;

    /// @brief Returns half a unit of the last digit of a figure typed with the given number of
    /// decimals.
    double half_unit_of_last_digit(int decimals)
    {
        return 0.5 * std::pow(10.0, -decimals);
    }

    /// @brief Returns how many times its bound a double differs from a reference that took the
    /// given number of rounded operations.
    double ratio_double(double got, double reference, std::size_t operations)
    {
        return TestSupport::measure_relative_error<double>(got, reference, operations).ratio;
    }

    /// @brief Returns how many times its bound a float differs from a reference that took the
    /// given number of rounded operations.
    double ratio_float(double got, double reference, std::size_t operations)
    {
        return TestSupport::measure_relative_error<float>(got, reference, operations).ratio;
    }

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
    double eigenfrequency(const RoomScene& scene, const Triple& triple)
    {
        const double x = triple.nx / scene.dimensions.x;
        const double y = triple.ny / scene.dimensions.y;
        const double z = triple.nz / scene.dimensions.z;
        return 0.5 * SushiDSP::room::K_SPEED_OF_SOUND * std::sqrt(x * x + y * y + z * z);
    }

    /// @brief Evaluates Weyl's (4 pi / 3) V (f / c)^3 + (pi / 4) S (f / c)^2 + (L / 8) (f / c)
    /// with L = 4 (Lx + Ly + Lz).
    double weyl_count(const RoomScene& scene, double frequency_hz)
    {
        const double k = frequency_hz / SushiDSP::room::K_SPEED_OF_SOUND;
        const double edges = 4.0 * (scene.dimensions.x + scene.dimensions.y + scene.dimensions.z);
        return (4.0 * K_PI / 3.0) * scene.volume_m3() * k * k * k +
               (K_PI / 4.0) * scene.surface_area_m2() * k * k + (edges / 8.0) * k;
    }

    /// @brief Returns the mode with the given indices out of a sorted enumeration.
    const ModalSolver::Mode& find_mode(const std::vector<ModalSolver::Mode>& modes,
                                       const Triple& triple)
    {
        for (const ModalSolver::Mode& mode : modes)
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
    double face_absorption_at(const RoomScene& scene, SushiDSP::room::Surface surface,
                              double frequency_hz)
    {
        const std::array<double, SushiDSP::room::K_BAND_COUNT> rows =
            scene.face_absorption(surface);
        const double position = std::log(frequency_hz / 125.0) / std::log(2.0);
        const double clamped = std::min(1.0, std::max(0.0, position));
        return rows[0] + (rows[1] - rows[0]) * clamped;
    }

    /// @brief Evaluates the perturbation decay constant (c / 16 V) sum over axes of
    /// epsilon_i (alpha_-i + alpha_+i) A_i, epsilon_i = 1 for a zero index and 2 otherwise.
    double damping_constant(const RoomScene& scene, const Triple& triple)
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
    double decay_seconds(const RoomScene& scene, const Triple& triple)
    {
        return 3.0 * std::log(10.0) / damping_constant(scene, triple);
    }

    /// @brief Returns a 3.0 x 2.5 x 2.4 m room of unpainted concrete with every pose inside.
    RoomScene small_concrete_room()
    {
        RoomScene scene;
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
    std::array<double, SushiDSP::room::K_BAND_COUNT> diffuse_band_power(const RoomScene& scene)
    {
        const std::array<double, SushiDSP::room::K_BAND_COUNT> rt60 =
            EyringDecay::rt60_seconds(scene);
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
    double diffuse_band_power(const RoomScene& scene, double lower, double upper)
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

    /// @brief Integrates the Eyring power alone, without the cabinet, over the garage's octave
    /// 139.09 to 278.18 Hz by the midpoint rule with 4096 steps, times 2 / fs.
    double eyring_alone_octave(const RoomScene& garage)
    {
        constexpr int K_STEPS = 4096;
        const std::array<double, SushiDSP::room::K_BAND_COUNT> rt60 =
            EyringDecay::rt60_seconds(garage);
        std::array<double, SushiDSP::room::K_BAND_COUNT> eyring{};
        for (std::size_t band = 0; band < SushiDSP::room::K_BAND_COUNT; ++band)
        {
            eyring[band] = 16.0 * K_PI * rt60[band] / (0.161 * garage.volume_m3());
        }
        double total = 0.0;
        for (int step = 0; step < K_STEPS; ++step)
        {
            total += band_value_at(eyring, 139.09 + (step + 0.5) * 139.09 / K_STEPS) * 139.09 /
                     K_STEPS * 2.0 / K_SAMPLE_RATE;
        }
        return total;
    }

    /// @brief Returns the garage with three wardrobe modules and two two-seat sofas standing in
    /// it.
    RoomScene furnished_garage()
    {
        RoomScene scene = SushiDSP::test::garage_scene();
        scene.furnishings[0] = {SushiDSP::room::FurnitureId::WardrobeModule, 3.0};
        scene.furnishings[1] = {SushiDSP::room::FurnitureId::SofaTwoSeat, 2.0};
        scene.furnishing_count = 2;
        return scene;
    }

    /// @brief Evaluates A_f / (4 V) at the 125 Hz and 250 Hz bands and interpolates it in log
    /// frequency, held flat below 125 Hz, in energy attenuation per metre.
    double furnishing_coefficient_at(const RoomScene& scene, double frequency_hz)
    {
        const std::array<double, SushiDSP::room::K_BAND_COUNT> area =
            scene.furnishing_absorption_area();
        const double low = area[0] / (4.0 * scene.volume_m3());
        const double high = area[1] / (4.0 * scene.volume_m3());
        const double position = std::log(frequency_hz / 125.0) / std::log(2.0);
        const double clamped = std::min(1.0, std::max(0.0, position));
        return low + (high - low) * clamped;
    }

    /// @brief Returns the modal design of a scene, heap-held.
    std::unique_ptr<ModalDesign> design_of(const RoomScene& scene, double crossover_hz,
                                           ListenerKind listener = ListenerKind::Ears)
    {
        return std::make_unique<ModalDesign>(
            ModalSolver::design(scene, K_SAMPLE_RATE, crossover_hz, listener));
    }

    /// @brief Returns the frequencies of a list of modes.
    std::vector<double> frequencies_of(const std::vector<ModalSolver::Mode>& modes)
    {
        std::vector<double> out;
        for (const ModalSolver::Mode& mode : modes)
        {
            out.push_back(mode.frequency_hz);
        }
        return out;
    }

    /// @brief Returns the decay times of a list of modes.
    std::vector<double> decays_of(const std::vector<ModalSolver::Mode>& modes)
    {
        std::vector<double> out;
        for (const ModalSolver::Mode& mode : modes)
        {
            out.push_back(mode.decay_seconds);
        }
        return out;
    }

    /// @brief Returns the index triples of a list of modes, packed in 3 values each.
    std::vector<std::uint32_t> indices_of(const std::vector<ModalSolver::Mode>& modes)
    {
        std::vector<std::uint32_t> out;
        for (const ModalSolver::Mode& mode : modes)
        {
            out.insert(out.end(), {mode.nx, mode.ny, mode.nz});
        }
        return out;
    }

    /// @brief Returns the resonator sections of a design, one per mode.
    std::vector<std::array<double, 5>> sections_of(const ModalDesign& design)
    {
        std::vector<std::array<double, 5>> out;
        for (std::uint32_t index = 0; index < design.count; ++index)
        {
            out.push_back(design.modes[index].section);
        }
        return out;
    }

    /// @brief Returns the source gains of a design, one per mode.
    std::vector<float> source_gains_of(const ModalDesign& design)
    {
        std::vector<float> out;
        for (std::uint32_t index = 0; index < design.count; ++index)
        {
            out.push_back(design.modes[index].source_gain);
        }
        return out;
    }

    /// @brief Returns the ear gains of a design, one pair per mode.
    std::vector<std::array<float, 2>> ear_gains_of(const ModalDesign& design)
    {
        std::vector<std::array<float, 2>> out;
        for (std::uint32_t index = 0; index < design.count; ++index)
        {
            out.push_back(design.modes[index].ear_gain);
        }
        return out;
    }

    /// @brief Returns the largest ratio of error to bound over the modes of a design, for one
    /// check of the mode's resonator against its frequency and decay.
    template <typename Check>
    double worst_over_modes(const std::vector<ModalSolver::Mode>& modes, const ModalDesign& design,
                            Check check)
    {
        double worst = 0.0;
        for (std::size_t index = 0; index < modes.size(); ++index)
        {
            worst = std::max(worst, check(modes[index], design.modes[index]));
        }
        return worst;
    }

    /// @brief Holds one room and the name its test carries.
    struct Room
    {
            const char* name;
            RoomScene (*scene)();
    };

    /// @brief Holds the three reference rooms.
    const std::array<Room, 3> kRooms = {{{"Garage", &SushiDSP::test::garage_scene},
                                         {"Bedroom", &SushiDSP::test::bedroom_scene},
                                         {"BlockRoom", &SushiDSP::test::block_room_scene}}};

    /// @brief Names a room case after its room.
    std::string room_name(const ::testing::TestParamInfo<Room>& info)
    {
        return info.param.name;
    }

    /// @brief Holds one band of the garage's diffuse field and the figure worked out by hand for
    /// it, with the decimals it was written with.
    struct FieldBand
    {
            const char* name;
            std::size_t band;
            double worked;
            int decimals;
    };

    /// @brief Holds the garage's field per band: Eyring's 1.342, 1.333, 1.161, 1.146, 1.108 and
    /// 0.893 s over 0.161 * 60 m3 times 16 pi, and the 4x12's directivity index of 1.12, 2.99,
    /// 7.59, 12.01, 15.74 and 18.82 dB taken back out.
    const std::array<FieldBand, 6> kFieldBands = {{{"Hz125", 0, 5.39, 2},
                                                   {"Hz250", 1, 3.48, 2},
                                                   {"Hz500", 2, 1.05, 2},
                                                   {"Hz1000", 3, 0.376, 3},
                                                   {"Hz2000", 4, 0.154, 3},
                                                   {"Hz4000", 5, 0.0610, 4}}};

    /// @brief Names a field band case after its centre.
    std::string field_band_name(const ::testing::TestParamInfo<FieldBand>& info)
    {
        return info.param.name;
    }

    /// @brief Holds one span of frequencies the field's power is integrated over.
    struct Span
    {
            const char* name;
            double lower_hz;
            double upper_hz;
    };

    /// @brief Holds a span across two centres and a span past the last one.
    const std::array<Span, 2> kSpans = {
        {{"AcrossTwoCentres", 139.09, 278.18}, {"PastTheLastCentre", 3000.0, 6000.0}}};

    /// @brief Names a span case after its place.
    std::string span_name(const ::testing::TestParamInfo<Span>& info)
    {
        return info.param.name;
    }

    /// @brief Holds one of the garage's first eight modes, where it sits in the enumeration and
    /// the frequency worked out by hand to four decimals: (c / 2) sqrt((nx / 6)^2 + (ny / 4)^2 +
    /// (nz / 2.5)^2) at c = 343.
    struct GarageMode
    {
            const char* name;
            std::size_t index;
            Triple triple;
            double worked_hz;
    };

    /// @brief Holds the first eight modes of the garage.
    const std::array<GarageMode, 8> kGarageModes = {{{"X1", 0, {1, 0, 0}, 28.5833},
                                                     {"Y1", 1, {0, 1, 0}, 42.8750},
                                                     {"X1Y1", 2, {1, 1, 0}, 51.5293},
                                                     {"X2", 3, {2, 0, 0}, 57.1667},
                                                     {"Z1", 4, {0, 0, 1}, 68.6000},
                                                     {"X2Y1", 5, {2, 1, 0}, 71.4583},
                                                     {"X1Z1", 6, {1, 0, 1}, 74.3167},
                                                     {"Y1Z1", 7, {0, 1, 1}, 80.8964}}};

    /// @brief Names a garage mode case after its indices.
    std::string garage_mode_name(const ::testing::TestParamInfo<GarageMode>& info)
    {
        return info.param.name;
    }

    /// @brief Returns the listener positions along x the ears are read at, 0.3 m apart.
    std::vector<double> listener_positions()
    {
        std::vector<double> positions;
        for (double x = 0.3; x < 5.85; x += 0.3)
        {
            positions.push_back(x);
        }
        return positions;
    }

    /// @brief Names a listener position case after its position in the sweep.
    std::string position_name(const ::testing::TestParamInfo<double>& info)
    {
        return "Step" + std::to_string(info.index);
    }

    /// @brief Returns the garage with the listener at x facing +y, which puts the two ears 0.17 m
    /// apart along x, across the (1, 0, 0) gradient.
    RoomScene garage_listener_at(double x)
    {
        RoomScene garage = SushiDSP::test::garage_scene();
        garage.listener_yaw_rad = 0.5 * K_PI;
        garage.listener_position = {x, 2.0, 1.2};
        return garage;
    }

    /// @brief Returns the garage's modes with the two x walls made of gypsum board on studs.
    std::vector<ModalSolver::Mode> soft_garage_modes(RoomScene& soft)
    {
        using SushiDSP::room::MaterialId;
        using SushiDSP::room::Surface;
        soft = SushiDSP::test::garage_scene();
        soft.surfaces[static_cast<std::size_t>(Surface::MinusX)] = MaterialId::GypsumBoardOnStuds;
        soft.surfaces[static_cast<std::size_t>(Surface::PlusX)] = MaterialId::GypsumBoardOnStuds;
        return ModalSolver::enumerate(soft, 100.0);
    }
} // namespace

class Small_ModalSolverField : public ::testing::TestWithParam<FieldBand>
{
};

class Small_ModalSolverSpan : public ::testing::TestWithParam<Span>
{
};

class Small_ModalSolverGarageModes : public ::testing::TestWithParam<GarageMode>
{
};

class Small_ModalSolverRooms : public ::testing::TestWithParam<Room>
{
};

class Small_ModalSolverEars : public ::testing::TestWithParam<double>
{
};

TEST_P(Small_ModalSolverField, BandPowerIsEyringOverTheCabinetsDirectivity)
{
    // The field is Eyring over the 4x12's directivity. See tests/nodes/room/README.md.
    const RoomScene garage = SushiDSP::test::garage_scene();
    const DiffuseField field(garage);
    EXPECT_LE(ratio_double(field.band_power()[GetParam().band],
                           diffuse_band_power(garage)[GetParam().band], 64),
              K_LIMIT);
}

TEST_P(Small_ModalSolverField, BandPowerIsTheWorkedOutFigure)
{
    const DiffuseField field(SushiDSP::test::garage_scene());
    EXPECT_NEAR(field.band_power()[GetParam().band], GetParam().worked,
                half_unit_of_last_digit(GetParam().decimals));
}

INSTANTIATE_TEST_SUITE_P(Bands, Small_ModalSolverField, ::testing::ValuesIn(kFieldBands),
                         field_band_name);

TEST(Small_ModalSolver, FieldIsFlatBelowTheFirstCentre)
{
    // Between the centres the field is linear in log frequency, flat outside them.
    const RoomScene garage = SushiDSP::test::garage_scene();
    EXPECT_LE(ratio_double(DiffuseField(garage).power_at(60.0), diffuse_band_power(garage)[0], 16),
              K_LIMIT);
}

TEST(Small_ModalSolver, FieldAtTheGeometricMeanOfTwoCentresIsTheirMidpoint)
{
    const RoomScene garage = SushiDSP::test::garage_scene();
    const std::array<double, SushiDSP::room::K_BAND_COUNT> expected = diffuse_band_power(garage);
    EXPECT_LE(ratio_double(DiffuseField(garage).power_at(std::sqrt(250.0 * 500.0)),
                           0.5 * (expected[1] + expected[2]), 16),
              K_LIMIT);
}

TEST(Small_ModalSolver, FieldIsFlatPastTheLastCentre)
{
    const RoomScene garage = SushiDSP::test::garage_scene();
    EXPECT_LE(ratio_double(DiffuseField(garage).power_at(10000.0), diffuse_band_power(garage)[5], 16),
              K_LIMIT);
}

TEST_P(Small_ModalSolverSpan, PowerBetweenMatchesTheMidpointRule)
{
    // The integral across two centres and past the last one matches the midpoint rule.
    const RoomScene garage = SushiDSP::test::garage_scene();
    const double midpoint =
        diffuse_band_power(garage, GetParam().lower_hz, GetParam().upper_hz) * K_SAMPLE_RATE / 2.0;
    EXPECT_NEAR(DiffuseField(garage).power_between(GetParam().lower_hz, GetParam().upper_hz),
                midpoint, K_INTEGRAL_RELATIVE_TOLERANCE * midpoint);
}

INSTANTIATE_TEST_SUITE_P(Spans, Small_ModalSolverSpan, ::testing::ValuesIn(kSpans), span_name);

TEST(Small_ModalSolver, FieldFollowsTheCabinetAndNotTheRoomAlone)
{
    // The field is the cabinet's, not the room's alone. See tests/nodes/room/README.md.
    const RoomScene garage = SushiDSP::test::garage_scene();
    RoomScene single = garage;
    single.cabinet = SushiDSP::room::K_CABINET1X12;
    EXPECT_GT(DiffuseField(single).band_power()[2], DiffuseField(garage).band_power()[2]);
}

TEST(Small_ModalSolver, SingleCabinetFieldIsEyringOverItsOwnDirectivity)
{
    RoomScene single = SushiDSP::test::garage_scene();
    single.cabinet = SushiDSP::room::K_CABINET1X12;
    EXPECT_LE(ratio_double(DiffuseField(single).band_power()[2], diffuse_band_power(single)[2], 64),
              K_LIMIT);
}

TEST_P(Small_ModalSolverGarageModes, FrequencyIsTheClosedForm)
{
    // f = (c/2) sqrt((nx/6)^2 + (ny/4)^2 + (nz/2.5)^2). See tests/nodes/room/README.md.
    const RoomScene garage = SushiDSP::test::garage_scene();
    const std::vector<ModalSolver::Mode> modes = ModalSolver::enumerate(garage, 100.0);
    ASSERT_GT(modes.size(), GetParam().index);
    EXPECT_LE(ratio_double(modes[GetParam().index].frequency_hz,
                           eigenfrequency(garage, GetParam().triple), 16),
              K_LIMIT);
}

TEST_P(Small_ModalSolverGarageModes, FrequencyIsTheWorkedOutFigure)
{
    const std::vector<ModalSolver::Mode> modes =
        ModalSolver::enumerate(SushiDSP::test::garage_scene(), 100.0);
    ASSERT_GT(modes.size(), GetParam().index);
    EXPECT_NEAR(modes[GetParam().index].frequency_hz, GetParam().worked_hz,
                half_unit_of_last_digit(4));
}

TEST_P(Small_ModalSolverGarageModes, IndicesAreTheTriple)
{
    const std::vector<ModalSolver::Mode> modes =
        ModalSolver::enumerate(SushiDSP::test::garage_scene(), 100.0);
    ASSERT_GT(modes.size(), GetParam().index);
    EXPECT_EQ(indices_of({modes[GetParam().index]}),
              (std::vector<std::uint32_t>{GetParam().triple.nx, GetParam().triple.ny,
                                          GetParam().triple.nz}));
}

INSTANTIATE_TEST_SUITE_P(Modes, Small_ModalSolverGarageModes, ::testing::ValuesIn(kGarageModes),
                         garage_mode_name);

TEST(Small_ModalSolver, ModesAreSortedAscendingByFrequency)
{
    const std::vector<double> frequencies =
        frequencies_of(ModalSolver::enumerate(SushiDSP::test::garage_scene(), 100.0));
    EXPECT_TRUE(std::is_sorted(frequencies.begin(), frequencies.end()));
}

TEST(Small_ModalSolver, EnumerationBelowTheGaragesSchroederFrequencyHoldsOneHundredAndEightySevenModes)
{
    EXPECT_EQ(ModalSolver::enumerate(SushiDSP::test::garage_scene(), 278.2).size(), 187u);
}

TEST(Small_ModalSolver, EstimatedCountIsWeylsCount)
{
    // Weyl's count is 134.11 + 50.63 + 5.07 = 189.80. See tests/nodes/room/README.md.
    const RoomScene garage = SushiDSP::test::garage_scene();
    EXPECT_LE(ratio_double(ModalSolver::estimated_count(garage, 278.2), weyl_count(garage, 278.2),
                           32),
              K_LIMIT);
}

TEST(Small_ModalSolver, EstimatedCountIsTheWorkedOutFigure)
{
    EXPECT_NEAR(ModalSolver::estimated_count(SushiDSP::test::garage_scene(), 278.2), 189.80,
                half_unit_of_last_digit(2));
}

TEST_P(Small_ModalSolverRooms, ModesBelowTheSchroederFrequencyStayUnderTheCap)
{
    const RoomScene room = GetParam().scene();
    const double schroeder = EyringDecay::schroeder_frequency_hz(room);
    EXPECT_LT(ModalSolver::enumerate(room, schroeder).size(), ModalDesign::K_MAX_MODES);
}

TEST_P(Small_ModalSolverRooms, EstimatedCountIsWithinTwoPercentOfTheEnumeration)
{
    const RoomScene room = GetParam().scene();
    const double schroeder = EyringDecay::schroeder_frequency_hz(room);
    const auto count = static_cast<double>(ModalSolver::enumerate(room, schroeder).size());
    EXPECT_NEAR(count, ModalSolver::estimated_count(room, schroeder),
                K_ESTIMATE_RELATIVE_TOLERANCE * count);
}

TEST_P(Small_ModalSolverRooms, ModalPowerOfTheOctaveBelowTheCrossoverIsTheDiffuseFieldsPower)
{
    // The octave below f_s holds the diffuse field's power. See tests/nodes/room/README.md.
    const RoomScene room = GetParam().scene();
    const double crossover = EyringDecay::schroeder_frequency_hz(room);
    const std::vector<ModalSolver::Mode> modes = ModalSolver::enumerate(room, crossover);
    const auto design = design_of(room, crossover);
    double power_sum = 0.0;
    for (const ModalSolver::Mode& mode : modes)
    {
        if (mode.frequency_hz >= 0.5 * crossover)
        {
            const double radius = std::pow(10.0, -3.0 / (mode.decay_seconds * K_SAMPLE_RATE));
            power_sum += 2.0 / (1.0 - radius * radius);
        }
    }
    const double expected = diffuse_band_power(room, 0.5 * static_cast<double>(design->crossover_hz),
                                               static_cast<double>(design->crossover_hz));
    const double scale = static_cast<double>(design->level_scale);
    ASSERT_GT(scale, 0.0);
    EXPECT_NEAR(scale * scale * power_sum, expected, K_OCTAVE_POWER_RELATIVE_TOLERANCE * expected);
}

INSTANTIATE_TEST_SUITE_P(Rooms, Small_ModalSolverRooms, ::testing::ValuesIn(kRooms), room_name);

TEST(Small_ModalSolver, GarageOctaveHoldsTheWorkedOutVarianceOfTheDiffuseField)
{
    // The garage's octave holds 2.334e-2 of the variance. See tests/nodes/room/README.md.
    EXPECT_NEAR(diffuse_band_power(SushiDSP::test::garage_scene(), 139.09, 278.18), 2.334e-2,
                0.0005e-2);
}

TEST(Small_ModalSolver, EyringAloneOctaveHoldsTheWorkedOutVariance)
{
    EXPECT_NEAR(eyring_alone_octave(SushiDSP::test::garage_scene()), 4.02e-2, 0.005e-2);
}

TEST(Small_ModalSolver, GarageOctaveSitsBetweenTheCabinetsPowerAtTheTwoBandsUnderTheEyringAlone)
{
    const RoomScene garage = SushiDSP::test::garage_scene();
    const double ratio =
        diffuse_band_power(garage, 139.09, 278.18) / eyring_alone_octave(garage);
    EXPECT_GT(ratio, std::pow(10.0, -0.299));
    EXPECT_LT(ratio, std::pow(10.0, -0.112));
}

TEST(Small_ModalSolver, DesignCapsAtTheModeCapWhenTheCountWouldOverflow)
{
    const RoomScene room = small_concrete_room();
    ASSERT_GT(ModalSolver::enumerate(room, 900.0).size(), ModalDesign::K_MAX_MODES);
    EXPECT_EQ(design_of(room, 900.0)->count, ModalDesign::K_MAX_MODES);
}

TEST(Small_ModalSolver, DesignLowersTheCrossoverBelowTheRequestButAboveZeroWhenTheCountWouldOverflow)
{
    const auto design = design_of(small_concrete_room(), 900.0);
    EXPECT_LT(design->crossover_hz, 900.0f);
    EXPECT_GT(design->crossover_hz, 0.0f);
}

TEST(Small_ModalSolver, LoweredCrossoverEnumeratesExactlyTheCap)
{
    const RoomScene room = small_concrete_room();
    EXPECT_EQ(ModalSolver::enumerate(room, design_of(room, 900.0)->crossover_hz).size(),
              ModalDesign::K_MAX_MODES);
}

TEST(Small_ModalSolver, LoweredCrossoverIsTheFirstModeLeftOutRoundedDownToAFloat)
{
    const RoomScene room = small_concrete_room();
    const std::vector<ModalSolver::Mode> modes = ModalSolver::enumerate(room, 900.0);
    ASSERT_GT(modes.size(), ModalDesign::K_MAX_MODES);
    const double first_left_out = modes[ModalDesign::K_MAX_MODES].frequency_hz;
    const float crossover = design_of(room, 900.0)->crossover_hz;
    EXPECT_LE(static_cast<double>(crossover), first_left_out);
    EXPECT_GT(static_cast<double>(std::nextafter(crossover, std::numeric_limits<float>::infinity())),
              first_left_out);
}

TEST(Small_ModalSolver, ModalSolverSwitchesOffUnderFortyHertz)
{
    const auto design = design_of(SushiDSP::test::garage_scene(), 39.0);
    EXPECT_EQ(design->count, 0u);
    EXPECT_EQ(design->crossover_hz, 0.0f);
    EXPECT_EQ(design->level_scale, 0.0f);
}

TEST(Small_ModalSolver, ModeShapeAtAPointIsTheCosineOfItsPosition)
{
    // cos(pi 0.05 / 6) = 0.999657325 and cos(pi / 2) = 0.
    const RoomScene garage = SushiDSP::test::garage_scene();
    const ModalSolver::Mode axial{1, 0, 0, 0.0, 0.0};
    EXPECT_LE(ratio_double(ModalSolver::mode_shape(axial, garage, {0.05, 1.0, 1.0}),
                           std::cos(K_PI * 0.05 / 6.0), 8),
              K_LIMIT);
}

TEST(Small_ModalSolver, ModeShapeAtANodeIsTheRoundingResidueOfCosineOfAQuarterTurn)
{
    // The argument pi x / L rounds by at most u pi / 2 of its size and cos has slope 1 there.
    const RoomScene garage = SushiDSP::test::garage_scene();
    const ModalSolver::Mode axial{1, 0, 0, 0.0, 0.0};
    EXPECT_LE(std::fabs(ModalSolver::mode_shape(axial, garage, {3.0, 1.0, 1.0})), 4.0 * K_U);
}

TEST(Small_ModalSolver, SourceGainIsTheModeShapeAtTheSource)
{
    RoomScene corner = SushiDSP::test::garage_scene();
    corner.source_position = {0.05, 1.0, 1.0};
    const auto design = design_of(corner, 100.0);
    ASSERT_GT(design->count, 0u);
    EXPECT_LE(ratio_float(design->modes[0].source_gain, std::cos(K_PI * 0.05 / 6.0), 2), K_LIMIT);
}

TEST(Small_ModalSolver, SoftWallsGiveTheAxialModesTheirClosedFormDecays)
{
    // The decay ratio of the two modes is 1.846. See tests/nodes/room/README.md.
    RoomScene soft;
    const std::vector<ModalSolver::Mode> modes = soft_garage_modes(soft);
    const Triple along_x{1, 0, 0};
    const Triple along_z{0, 0, 1};
    EXPECT_LE(ratio_double(find_mode(modes, along_x).decay_seconds, decay_seconds(soft, along_x), 64),
              K_LIMIT);
    EXPECT_LE(ratio_double(find_mode(modes, along_z).decay_seconds, decay_seconds(soft, along_z), 64),
              K_LIMIT);
}

TEST(Small_ModalSolver, SoftXWallsLetTheZModeRingOneEightFourSixTimesLongerThanTheXMode)
{
    RoomScene soft;
    const std::vector<ModalSolver::Mode> modes = soft_garage_modes(soft);
    const double ratio =
        find_mode(modes, {0, 0, 1}).decay_seconds / find_mode(modes, {1, 0, 0}).decay_seconds;
    EXPECT_NEAR(ratio, K_SOFT_DECAY_RATIO, half_unit_of_last_digit(3));
}

TEST_P(Small_ModalSolverEars, EarsReadDifferentGainsAcrossTheGradient)
{
    const auto design = design_of(garage_listener_at(GetParam()), 100.0);
    ASSERT_GT(design->count, 0u);
    const double left = design->modes[0].ear_gain[0];
    const double right = design->modes[0].ear_gain[1];
    EXPECT_GT(std::abs(left - right) / std::max(std::abs(left), std::abs(right)),
              K_EAR_DIFFERENCE_FRACTION);
}

TEST_P(Small_ModalSolverEars, LeftEarReadsItsPointEightyFiveMillimetresBelowTheListener)
{
    // Facing +y, the left ear sits at x - 0.085 and the right at x + 0.085.
    const auto design = design_of(garage_listener_at(GetParam()), 100.0);
    ASSERT_GT(design->count, 0u);
    EXPECT_LE(ratio_float(design->modes[0].ear_gain[0], std::cos(K_PI * (GetParam() - 0.085) / 6.0), 4),
              K_LIMIT);
}

TEST_P(Small_ModalSolverEars, RightEarReadsItsPointEightyFiveMillimetresAboveTheListener)
{
    const auto design = design_of(garage_listener_at(GetParam()), 100.0);
    ASSERT_GT(design->count, 0u);
    EXPECT_LE(ratio_float(design->modes[0].ear_gain[1], std::cos(K_PI * (GetParam() + 0.085) / 6.0), 4),
              K_LIMIT);
}

INSTANTIATE_TEST_SUITE_P(Positions, Small_ModalSolverEars, ::testing::ValuesIn(listener_positions()),
                         position_name);

class Small_ModalSolverResonators : public ::testing::Test
{
    protected:
        /// @brief Designs the garage at 278.2 Hz and enumerates the modes it holds.
        void SetUp() override
        {
            modes = ModalSolver::enumerate(SushiDSP::test::garage_scene(), K_CROSSOVER);
            design = design_of(SushiDSP::test::garage_scene(), K_CROSSOVER);
            ASSERT_EQ(design->count, modes.size());
        }

        /// @brief Stores the enumerated modes.
        std::vector<ModalSolver::Mode> modes;

        /// @brief Stores the modal design.
        std::unique_ptr<ModalDesign> design;
};

TEST_F(Small_ModalSolverResonators, NumeratorGainIsTheLevelScaleOverTheModeNormalisation)
{
    const float level_scale = design->level_scale;
    const double worst = worst_over_modes(
        modes, *design,
        [level_scale](const ModalSolver::Mode& mode, const ModalDesign::Mode& designed)
        {
            const int live = (mode.nx != 0u) + (mode.ny != 0u) + (mode.nz != 0u);
            return ratio_float(designed.section[0], level_scale / std::pow(0.5, live), 2);
        });
    EXPECT_LE(worst, K_LIMIT);
}

TEST_F(Small_ModalSolverResonators, NumeratorHasNoMiddleTermAndItsLastTermMirrorsTheFirst)
{
    const auto numerators_are_mirrored = [&]()
    {
        for (std::uint32_t index = 0; index < design->count; ++index)
        {
            const auto& section = design->modes[index].section;
            if (section[1] != 0.0 || section[2] != -section[0])
            {
                return false;
            }
        }
        return true;
    };
    EXPECT_TRUE(numerators_are_mirrored());
}

TEST_F(Small_ModalSolverResonators, FeedbackIsTheResonatorOfTheModesFrequencyAndDecay)
{
    const double worst = worst_over_modes(
        modes, *design,
        [](const ModalSolver::Mode& mode, const ModalDesign::Mode& designed)
        {
            const double radius = std::pow(10.0, -3.0 / (mode.decay_seconds * K_SAMPLE_RATE));
            const double theta = 2.0 * K_PI * mode.frequency_hz / K_SAMPLE_RATE;
            return std::max(ratio_double(designed.section[3], -2.0 * radius * std::cos(theta), 16),
                            ratio_double(designed.section[4], radius * radius, 16));
        });
    EXPECT_LE(worst, K_LIMIT);
}

class Small_ModalSolverMicrophone : public ::testing::Test
{
    protected:
        /// @brief Designs the garage for the ears and for the microphone, with the source in a
        /// corner and the listener and the microphone elsewhere.
        void SetUp() override
        {
            scene = SushiDSP::test::garage_scene();
            scene.source_position = {0.4, 0.5, 0.5};
            scene.listener_position = {5.4, 3.4, 2.0};
            scene.microphone_position = {1.0, 0.5, 0.5};
            crossover = EyringDecay::schroeder_frequency_hz(scene);
            ears = design_of(scene, crossover, ListenerKind::Ears);
            microphone = design_of(scene, crossover, ListenerKind::Microphone);
            modes = ModalSolver::enumerate(scene, crossover);
            ASSERT_EQ(ears->count, modes.size());
            ASSERT_EQ(microphone->count, ears->count);
        }

        /// @brief Returns the mode shape of a mode at a point, as the float a gain holds.
        float shape_at(std::size_t index, const SushiDSP::room::Vec3& point) const
        {
            return static_cast<float>(ModalSolver::mode_shape(modes[index], scene, point));
        }

        /// @brief Stores the scene.
        RoomScene scene;

        /// @brief Stores the crossover, the Schroeder frequency of the scene.
        double crossover = 0.0;

        /// @brief Stores the design for the ears.
        std::unique_ptr<ModalDesign> ears;

        /// @brief Stores the design for the microphone.
        std::unique_ptr<ModalDesign> microphone;

        /// @brief Stores the modes both designs hold.
        std::vector<ModalSolver::Mode> modes;
};

TEST_F(Small_ModalSolverMicrophone, MicrophoneAndEarsShareTheCrossoverAndTheLevel)
{
    EXPECT_EQ(microphone->crossover_hz, ears->crossover_hz);
    EXPECT_EQ(microphone->level_scale, ears->level_scale);
}

TEST_F(Small_ModalSolverMicrophone, MicrophoneReadsOnePointForBothEars)
{
    // The ears sit 8.5 cm to either side of the listener. See tests/nodes/room/README.md.
    std::vector<std::array<float, 2>> expected;
    for (std::size_t index = 0; index < modes.size(); ++index)
    {
        const float at_microphone = shape_at(index, scene.microphone_position);
        expected.push_back({at_microphone, at_microphone});
    }
    EXPECT_EQ(ear_gains_of(*microphone), expected);
}

TEST_F(Small_ModalSolverMicrophone, MicrophoneSectionsMatchTheEars)
{
    EXPECT_EQ(sections_of(*microphone), sections_of(*ears));
}

TEST_F(Small_ModalSolverMicrophone, MicrophoneSourceGainsMatchTheEars)
{
    EXPECT_EQ(source_gains_of(*microphone), source_gains_of(*ears));
}

TEST_F(Small_ModalSolverMicrophone, LeftEarReadsThePointEightyFiveMillimetresToItsSide)
{
    double worst = 0.0;
    for (std::size_t index = 0; index < modes.size(); ++index)
    {
        worst = std::max(worst, ratio_float(ears->modes[index].ear_gain[0],
                                            shape_at(index, {5.4, 3.485, 2.0}), 4));
    }
    EXPECT_LE(worst, K_LIMIT);
}

TEST_F(Small_ModalSolverMicrophone, RightEarReadsThePointEightyFiveMillimetresToItsSide)
{
    double worst = 0.0;
    for (std::size_t index = 0; index < modes.size(); ++index)
    {
        worst = std::max(worst, ratio_float(ears->modes[index].ear_gain[1],
                                            shape_at(index, {5.4, 3.315, 2.0}), 4));
    }
    EXPECT_LE(worst, K_LIMIT);
}

TEST_F(Small_ModalSolverMicrophone, MicrophoneAndLeftEarDifferAtMoreThanHalfTheModes)
{
    const std::size_t differing = static_cast<std::size_t>(std::count_if(
        modes.begin(), modes.end(),
        [&](const ModalSolver::Mode& mode)
        {
            const std::size_t index = static_cast<std::size_t>(&mode - modes.data());
            return microphone->modes[index].ear_gain[0] != ears->modes[index].ear_gain[0];
        }));
    EXPECT_GT(differing, modes.size() / 2);
}

class Small_ModalSolverFurnishing : public ::testing::Test
{
    protected:
        /// @brief Enumerates and designs the bare garage and the furnished one at 278.2 Hz.
        void SetUp() override
        {
            bare = SushiDSP::test::garage_scene();
            furnished = furnished_garage();
            bare_modes = ModalSolver::enumerate(bare, K_CROSSOVER);
            furnished_modes = ModalSolver::enumerate(furnished, K_CROSSOVER);
            ASSERT_EQ(furnished_modes.size(), bare_modes.size());
            bare_design = design_of(bare, K_CROSSOVER);
            furnished_design = design_of(furnished, K_CROSSOVER);
            ASSERT_EQ(furnished_design->count, bare_design->count);
        }

        /// @brief Stores the bare garage.
        RoomScene bare;

        /// @brief Stores the furnished garage.
        RoomScene furnished;

        /// @brief Stores the bare garage's modes.
        std::vector<ModalSolver::Mode> bare_modes;

        /// @brief Stores the furnished garage's modes.
        std::vector<ModalSolver::Mode> furnished_modes;

        /// @brief Stores the bare garage's design.
        std::unique_ptr<ModalDesign> bare_design;

        /// @brief Stores the furnished garage's design.
        std::unique_ptr<ModalDesign> furnished_design;
};

TEST_F(Small_ModalSolverFurnishing, AnUnfurnishedRoomWithJunkInItsTableKeepsItsModes)
{
    RoomScene junk = bare;
    junk.furnishings.fill({SushiDSP::room::FurnitureId::SofaTwoSeat, 9.0});
    junk.furnishing_count = 0;
    const std::vector<ModalSolver::Mode> junk_modes = ModalSolver::enumerate(junk, K_CROSSOVER);
    EXPECT_EQ(frequencies_of(junk_modes), frequencies_of(bare_modes));
    EXPECT_EQ(decays_of(junk_modes), decays_of(bare_modes));
}

TEST_F(Small_ModalSolverFurnishing, AnUnfurnishedRoomWithJunkInItsTableKeepsItsDesign)
{
    RoomScene junk = bare;
    junk.furnishings.fill({SushiDSP::room::FurnitureId::SofaTwoSeat, 9.0});
    junk.furnishing_count = 0;
    const auto junk_design = design_of(junk, K_CROSSOVER);
    EXPECT_EQ(sections_of(*junk_design), sections_of(*bare_design));
    EXPECT_EQ(source_gains_of(*junk_design), source_gains_of(*bare_design));
    EXPECT_EQ(ear_gains_of(*junk_design), ear_gains_of(*bare_design));
}

TEST_F(Small_ModalSolverFurnishing, AnUnfurnishedRoomWithJunkInItsTableKeepsItsCrossoverAndLevel)
{
    RoomScene junk = bare;
    junk.furnishings.fill({SushiDSP::room::FurnitureId::SofaTwoSeat, 9.0});
    junk.furnishing_count = 0;
    const auto junk_design = design_of(junk, K_CROSSOVER);
    EXPECT_EQ(junk_design->crossover_hz, bare_design->crossover_hz);
    EXPECT_EQ(junk_design->level_scale, bare_design->level_scale);
}

TEST_F(Small_ModalSolverFurnishing, FurnishingKeepsTheModeFrequenciesAndIndices)
{
    EXPECT_EQ(frequencies_of(furnished_modes), frequencies_of(bare_modes));
    EXPECT_EQ(indices_of(furnished_modes), indices_of(bare_modes));
}

TEST_F(Small_ModalSolverFurnishing, FurnishingShortensEveryModesDecay)
{
    const std::vector<double> bare_decays = decays_of(bare_modes);
    const std::vector<double> furnished_decays = decays_of(furnished_modes);
    EXPECT_TRUE(std::equal(furnished_decays.begin(), furnished_decays.end(), bare_decays.begin(),
                           [](double furnished_decay, double bare_decay)
                           { return furnished_decay < bare_decay; }));
}

TEST_F(Small_ModalSolverFurnishing, FurnishingKeepsTheDesignGainsAndTheCrossover)
{
    EXPECT_EQ(source_gains_of(*furnished_design), source_gains_of(*bare_design));
    EXPECT_EQ(ear_gains_of(*furnished_design), ear_gains_of(*bare_design));
    EXPECT_EQ(furnished_design->crossover_hz, bare_design->crossover_hz);
}

TEST_F(Small_ModalSolverFurnishing, FurnishingCoefficientIsTheCitedTablePerFourVolumes)
{
    // The table's 3 wardrobe modules and 2 sofas over 4 V = 240 m3.
    const auto& wardrobe =
        SushiDSP::room::furniture(SushiDSP::room::FurnitureId::WardrobeModule).absorption_area;
    const auto& sofa =
        SushiDSP::room::furniture(SushiDSP::room::FurnitureId::SofaTwoSeat).absorption_area;
    EXPECT_LE(ratio_double(furnishing_coefficient_at(furnished, 125.0),
                           (3.0 * wardrobe[0] + 2.0 * sofa[0]) / 240.0, 16),
              K_LIMIT);
    EXPECT_LE(ratio_double(furnishing_coefficient_at(furnished, 250.0),
                           (3.0 * wardrobe[1] + 2.0 * sofa[1]) / 240.0, 16),
              K_LIMIT);
}

TEST_F(Small_ModalSolverFurnishing, FurnishingAddsItsDampingToEveryMode)
{
    // A decay of T seconds is delta = 3 ln(10) / T nepers per second, held to 4 u of its size.
    double worst = 0.0;
    for (std::size_t index = 0; index < bare_modes.size(); ++index)
    {
        const double bare_delta = K_DECAY_NEPERS / bare_modes[index].decay_seconds;
        const double furnished_delta = K_DECAY_NEPERS / furnished_modes[index].decay_seconds;
        const double expected = 0.5 * SushiDSP::room::K_SPEED_OF_SOUND *
                                furnishing_coefficient_at(furnished, bare_modes[index].frequency_hz);
        const double bound = 4.0 * K_U * (bare_delta + furnished_delta) + 16.0 * K_U * expected;
        worst = std::max(worst, TestSupport::measure_error(furnished_delta - bare_delta, expected, bound)
                                    .ratio);
    }
    EXPECT_LE(worst, K_LIMIT);
}

TEST_F(Small_ModalSolverFurnishing, DesignedPoleRadiusCarriesTheFurnishedDamping)
{
    // The pole radius r = 10^(-3 / (T fs)) gives r^2 = exp(-2 delta / fs). Rounding r^2 moves delta.
    const double bound = 0.5 * K_SAMPLE_RATE * 4.0 * K_U;
    double worst = 0.0;
    for (std::size_t index = 0; index < furnished_modes.size(); ++index)
    {
        const double furnished_delta = K_DECAY_NEPERS / furnished_modes[index].decay_seconds;
        const double designed_delta =
            -0.5 * K_SAMPLE_RATE * std::log(furnished_design->modes[index].section[4]);
        worst = std::max(
            worst, TestSupport::measure_error(designed_delta, furnished_delta, bound).ratio);
    }
    EXPECT_LE(worst, K_LIMIT);
}

TEST_F(Small_ModalSolverFurnishing, GarageHasModesBelowBetweenAndAboveTheTwoFurnishingBands)
{
    const std::vector<double> frequencies = frequencies_of(bare_modes);
    EXPECT_TRUE(std::any_of(frequencies.begin(), frequencies.end(), [](double f) { return f < 125.0; }));
    EXPECT_TRUE(std::any_of(frequencies.begin(), frequencies.end(),
                            [](double f) { return f > 125.0 && f < 250.0; }));
    EXPECT_TRUE(std::any_of(frequencies.begin(), frequencies.end(), [](double f) { return f > 250.0; }));
}

TEST(Small_ModalSolverLogging, DesignWarnsOnceWhenTheModesOverflowAndTheCrossoverMovesDown)
{
    const RoomScene room = small_concrete_room();

    SushiDSP::LogCapture capture;
    const auto design = design_of(room, 900.0);

    EXPECT_EQ(design->count, ModalDesign::K_MAX_MODES);
    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "room");
    EXPECT_NE(lines[0].message.find("the design holds 512"), std::string::npos) << lines[0].message;
}

TEST(Small_ModalSolverLogging, DesignWritesNothingWhenEveryModeFits)
{
    SushiDSP::LogCapture capture;
    const auto design = design_of(SushiDSP::test::garage_scene(), 120.0);

    EXPECT_LT(design->count, ModalDesign::K_MAX_MODES);
    EXPECT_TRUE(capture.lines().empty());
}
