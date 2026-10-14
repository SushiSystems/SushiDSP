/**************************************************************************/
/* modal_solver.cpp                                                       */
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
 * @file modal_solver.cpp
 * @brief The rigid-shoebox eigenmodes, the perturbation damping, the resonator sections and the
 * seam level that matches the modal layer to the diffuse field.
 * @author Mustafa Garip & Sushi Systems
 */

#include <SushiDSP/nodes/room/modal_solver.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include <SushiDSP/nodes/room/band_response.hpp>
#include <SushiDSP/nodes/room/diffuse_field.hpp>
#include <SushiDSP/nodes/room/furnishing_field.hpp>
#include <SushiDSP/nodes/room/medium_absorption.hpp>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds pi.
        constexpr double K_PI = 3.14159265358979323846;

        /// @brief Holds 3 ln(10), the amplitude e-folds a 60 dB decay spans.
        constexpr double K_DECAY_NEPERS = 6.90775527898213705205;

        /// @brief Names the band whose absorption row anchors the low end of the interpolation.
        constexpr std::size_t K_LOW_BAND = 0;

        /// @brief Names the band whose absorption row anchors the high end of the interpolation.
        constexpr std::size_t K_HIGH_BAND = 1;

        /// @brief Bounds the enumeration ceiling as a multiple of K_MAX_MODES in Weyl's count.
        constexpr double K_ENUMERATION_MARGIN = 2.0;

        /// @brief Holds the three shoebox lengths, indexed by axis.
        using Lengths = std::array<double, 3>;

        /// @brief Holds each face's absorption at K_LOW_BAND and K_HIGH_BAND, indexed by Surface.
        using FaceRows = std::array<std::array<double, 2>, K_SURFACE_COUNT>;

        /// @brief Returns the shoebox's lengths as an axis-indexed array.
        Lengths lengths_of(const RoomScene& scene) noexcept
        {
            return {scene.dimensions.x, scene.dimensions.y, scene.dimensions.z};
        }

        /// @brief Returns a mode's indices as an axis-indexed array.
        std::array<std::uint32_t, 3> indices_of(const ModalSolver::Mode& mode) noexcept
        {
            return {mode.nx, mode.ny, mode.nz};
        }

        /// @brief Returns (c / 2) sqrt(sum (n_i / L_i)^2) for one index triple.
        double eigenfrequency(const Lengths& lengths, std::uint32_t nx, std::uint32_t ny,
                              std::uint32_t nz) noexcept
        {
            const double x = static_cast<double>(nx) / lengths[0];
            const double y = static_cast<double>(ny) / lengths[1];
            const double z = static_cast<double>(nz) / lengths[2];
            return 0.5 * K_SPEED_OF_SOUND * std::sqrt(x * x + y * y + z * z);
        }

        /// @brief Returns the largest index along one axis whose axial frequency stays under
        /// max_frequency_hz.
        std::uint32_t index_bound(double length, double max_frequency_hz) noexcept
        {
            const double bound = 2.0 * max_frequency_hz * length / K_SPEED_OF_SOUND;
            return bound <= 0.0 ? 0u : static_cast<std::uint32_t>(std::ceil(bound) - 1.0);
        }

        /// @brief Returns each face's absorption at the two low bands, patches folded in.
        FaceRows face_rows(const RoomScene& scene)
        {
            FaceRows rows{};
            for (std::size_t index = 0; index < K_SURFACE_COUNT; ++index)
            {
                const std::array<double, K_BAND_COUNT> absorption =
                    scene.face_absorption(static_cast<Surface>(index));
                rows[index] = {absorption[K_LOW_BAND], absorption[K_HIGH_BAND]};
            }
            return rows;
        }

        /// @brief Holds the furnishings' energy attenuation per metre at K_LOW_BAND and K_HIGH_BAND.
        using FurnishingRow = std::array<double, 2>;

        /// @brief Returns the furnishings' energy attenuation per metre at the two low bands, as
        /// one mode sees it from where the furnishings stand.
        FurnishingRow furnishing_row(const FurnishingField& field, std::uint32_t nx,
                                     std::uint32_t ny, std::uint32_t nz) noexcept
        {
            return {field.mode_coefficient_per_metre(K_LOW_BAND, nx, ny, nz),
                    field.mode_coefficient_per_metre(K_HIGH_BAND, nx, ny, nz)};
        }

        /// @brief Interpolates one row of two band values in log frequency to frequency_hz, held
        /// flat outside the two anchoring bands.
        double face_absorption_at(const std::array<double, 2>& row, double frequency_hz) noexcept
        {
            const double low = K_BAND_CENTRES_HZ[K_LOW_BAND];
            const double high = K_BAND_CENTRES_HZ[K_HIGH_BAND];
            const double position = std::log(frequency_hz / low) / std::log(high / low);
            const double clamped = std::clamp(position, 0.0, 1.0);
            return row[0] + (row[1] - row[0]) * clamped;
        }

        /// @brief Returns the amplitude decay constant delta = (c / 16 V) sum over axes of
        /// epsilon_i (alpha_-i + alpha_+i) A_i with epsilon_i = 1 for a zero index and 2
        /// otherwise, plus c m_f / 2 for the furnishings, in nepers per second. Spec §9a.2.
        double damping_constant(const RoomScene& scene, const FaceRows& rows,
                                const FurnishingRow& furnishing,
                                const ModalSolver::Mode& mode) noexcept
        {
            const std::array<std::uint32_t, 3> indices = indices_of(mode);
            double weighted_area = 0.0;
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                const Surface low_face = static_cast<Surface>(2 * axis);
                const Surface high_face = static_cast<Surface>(2 * axis + 1);
                const double epsilon = indices[axis] == 0u ? 1.0 : 2.0;
                const double absorption =
                    face_absorption_at(rows[static_cast<std::size_t>(low_face)],
                                       mode.frequency_hz) +
                    face_absorption_at(rows[static_cast<std::size_t>(high_face)],
                                       mode.frequency_hz);
                weighted_area += epsilon * absorption * scene.face_area_m2(low_face);
            }
            const double furnishing_coefficient = face_absorption_at(furnishing, mode.frequency_hz);
            return K_SPEED_OF_SOUND * weighted_area / (16.0 * scene.volume_m3()) +
                   0.5 * K_SPEED_OF_SOUND * furnishing_coefficient;
        }

        /// @brief Returns the volume mean of psi squared: one half per non-zero index.
        double mode_norm(const ModalSolver::Mode& mode) noexcept
        {
            double norm = 1.0;
            for (const std::uint32_t index : indices_of(mode))
            {
                norm *= index == 0u ? 1.0 : 0.5;
            }
            return norm;
        }

        /// @brief Returns the ear position K_EAR_HALF_SPACING_METRES to the listener's side, sign
        /// +1 for the left ear and -1 for the right, held inside the room.
        Vec3 ear_position(const RoomScene& scene, double side) noexcept
        {
            const double offset = side * ModalSolver::K_EAR_HALF_SPACING_METRES;
            Vec3 ear{scene.listener_position.x - offset * std::sin(scene.listener_yaw_rad),
                     scene.listener_position.y + offset * std::cos(scene.listener_yaw_rad),
                     scene.listener_position.z};
            ear.x = std::clamp(ear.x, 0.0, scene.dimensions.x);
            ear.y = std::clamp(ear.y, 0.0, scene.dimensions.y);
            return ear;
        }

        /// @brief Returns the two points the field is read at: the ears about the listener, or
        /// the microphone twice.
        std::array<Vec3, 2> receiver_points(const RoomScene& scene, ListenerKind listener) noexcept
        {
            if (listener == ListenerKind::Microphone)
            {
                return {scene.microphone_position, scene.microphone_position};
            }
            return {ear_position(scene, 1.0), ear_position(scene, -1.0)};
        }

        /// @brief Returns the pole radius 10^(-3 / (T60 fs)) of a resonator with the given decay.
        double pole_radius(double decay_seconds, double sample_rate) noexcept
        {
            return std::pow(10.0, -3.0 / (decay_seconds * sample_rate));
        }

        /// @brief Returns the white-noise power gain 2 / (1 - r^2) of the bandpass section
        /// (1 - z^-2) / (1 - 2 r cos(theta) z^-1 + r^2 z^-2).
        double section_power_gain(double radius) noexcept
        {
            return 2.0 / (1.0 - radius * radius);
        }

        /// @brief Returns the power a unit-variance white input leaves in the diffuse field
        /// between lower and upper hertz: the field's integral times the input's 2 / fs per
        /// hertz.
        double diffuse_band_power(const RoomScene& scene, double sample_rate, double lower,
                                  double upper)
        {
            const DiffuseField field(scene);
            return (2.0 / sample_rate) * field.power_between(lower, upper);
        }

        /// @brief Returns the frequency at which Weyl's count reaches K_ENUMERATION_MARGIN times
        /// K_MAX_MODES, by bisection.
        double enumeration_ceiling(const RoomScene& scene) noexcept
        {
            const double target =
                K_ENUMERATION_MARGIN * static_cast<double>(ModalDesign::K_MAX_MODES);
            double lower = 0.0;
            double upper = K_SPEED_OF_SOUND;
            while (ModalSolver::estimated_count(scene, upper) < target)
            {
                lower = upper;
                upper *= 2.0;
            }
            for (int step = 0; step < 60; ++step)
            {
                const double middle = 0.5 * (lower + upper);
                (ModalSolver::estimated_count(scene, middle) < target ? lower : upper) = middle;
            }
            return upper;
        }

        /// @brief Returns the largest float that, widened, does not exceed value.
        float floor_to_float(double value) noexcept
        {
            float result = static_cast<float>(value);
            while (static_cast<double>(result) > value)
            {
                result = std::nextafter(result, 0.0f);
            }
            return result;
        }
    } // namespace

    std::vector<ModalSolver::Mode> ModalSolver::enumerate(const RoomScene& scene,
                                                          double max_frequency_hz)
    {
        const Lengths lengths = lengths_of(scene);
        const FaceRows rows = face_rows(scene);
        const FurnishingField field(scene);
        std::vector<Mode> modes;
        modes.reserve(
            static_cast<std::size_t>(std::max(0.0, estimated_count(scene, max_frequency_hz))) +
            16u);

        const std::uint32_t bound_x = index_bound(lengths[0], max_frequency_hz);
        for (std::uint32_t nx = 0; nx <= bound_x; ++nx)
        {
            const std::uint32_t bound_y = index_bound(lengths[1], max_frequency_hz);
            for (std::uint32_t ny = 0; ny <= bound_y; ++ny)
            {
                if (eigenfrequency(lengths, nx, ny, 0u) >= max_frequency_hz)
                {
                    break;
                }
                const std::uint32_t bound_z = index_bound(lengths[2], max_frequency_hz);
                for (std::uint32_t nz = 0; nz <= bound_z; ++nz)
                {
                    // The (0, 0, 0) pressure mode has no standing wave to render.
                    if (nx == 0u && ny == 0u && nz == 0u)
                    {
                        continue;
                    }
                    const double frequency = eigenfrequency(lengths, nx, ny, nz);
                    if (frequency >= max_frequency_hz)
                    {
                        break;
                    }
                    Mode mode{nx, ny, nz, frequency, 0.0};
                    mode.decay_seconds =
                        K_DECAY_NEPERS /
                        damping_constant(scene, rows, furnishing_row(field, nx, ny, nz), mode);
                    modes.push_back(mode);
                }
            }
        }

        std::sort(modes.begin(), modes.end(),
                  [](const Mode& left, const Mode& right) noexcept
                  {
                      return left.frequency_hz < right.frequency_hz;
                  });
        return modes;
    }

    double ModalSolver::estimated_count(const RoomScene& scene, double frequency_hz)
    {
        const double wavenumber = frequency_hz / K_SPEED_OF_SOUND;
        const double edge_length =
            4.0 * (scene.dimensions.x + scene.dimensions.y + scene.dimensions.z);
        return (4.0 * K_PI / 3.0) * scene.volume_m3() * wavenumber * wavenumber * wavenumber +
               (K_PI / 4.0) * scene.surface_area_m2() * wavenumber * wavenumber +
               (edge_length / 8.0) * wavenumber;
    }

    double ModalSolver::mode_shape(const Mode& mode, const RoomScene& scene,
                                   const Vec3& position) noexcept
    {
        const Lengths lengths = lengths_of(scene);
        const std::array<std::uint32_t, 3> indices = indices_of(mode);
        const std::array<double, 3> coordinate = {position.x, position.y, position.z};
        double shape = 1.0;
        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            shape *= std::cos(static_cast<double>(indices[axis]) * K_PI * coordinate[axis] /
                              lengths[axis]);
        }
        return shape;
    }

    ModalDesign ModalSolver::design(const RoomScene& scene, double sample_rate, double crossover_hz,
                                    ListenerKind listener)
    {
        return design(shell(scene, sample_rate, crossover_hz), scene, listener);
    }

    ModalDesign ModalSolver::design(const Shell& shell, const RoomScene& scene,
                                    ListenerKind listener)
    {
        ModalDesign result = shell.design;
        const std::array<Vec3, 2> receiver = receiver_points(scene, listener);
        for (std::size_t index = 0; index < shell.modes.size(); ++index)
        {
            const Mode& mode = shell.modes[index];
            ModalDesign::Mode& entry = result.modes[index];
            entry.source_gain = static_cast<float>(mode_shape(mode, scene, scene.source_position));
            entry.ear_gain = {static_cast<float>(mode_shape(mode, scene, receiver[0])),
                              static_cast<float>(mode_shape(mode, scene, receiver[1]))};
        }
        return result;
    }

    ModalSolver::Shell ModalSolver::shell(const RoomScene& scene, double sample_rate,
                                          double crossover_hz)
    {
        Shell shell{};
        ModalDesign& result = shell.design;
        if (crossover_hz < K_MIN_CROSSOVER_HZ)
        {
            return shell;
        }

        const double ceiling = std::min(crossover_hz, enumeration_ceiling(scene));
        std::vector<Mode> modes = enumerate(scene, ceiling);
        float crossover = static_cast<float>(crossover_hz);
        std::size_t count = modes.size();
        if (count > ModalDesign::K_MAX_MODES)
        {
            // The reported crossover is the first mode left out; see modules/nodes/room/README.md.
            crossover = floor_to_float(modes[ModalDesign::K_MAX_MODES].frequency_hz);
            count = static_cast<std::size_t>(
                std::lower_bound(modes.begin(), modes.end(), static_cast<double>(crossover),
                                 [](const Mode& mode, double limit) noexcept
                                 {
                                     return mode.frequency_hz < limit;
                                 }) -
                modes.begin());
        }

        std::vector<double> radius(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            radius[index] = pole_radius(modes[index].decay_seconds, sample_rate);
        }

        const double upper = static_cast<double>(crossover);
        double lower = 0.5 * upper;
        double power_sum = 0.0;
        for (std::size_t index = 0; index < count; ++index)
        {
            if (modes[index].frequency_hz >= lower)
            {
                power_sum += section_power_gain(radius[index]);
            }
        }
        if (power_sum == 0.0 && count > 0u)
        {
            // An octave without a mode widens the match down to the lowest mode there is.
            lower = modes[0].frequency_hz;
            for (std::size_t index = 0; index < count; ++index)
            {
                power_sum += section_power_gain(radius[index]);
            }
        }
        const double level_scale =
            power_sum > 0.0
                ? std::sqrt(diffuse_band_power(scene, sample_rate, lower, upper) / power_sum)
                : 0.0;

        for (std::size_t index = 0; index < count; ++index)
        {
            const Mode& mode = modes[index];
            const double theta = 2.0 * K_PI * mode.frequency_hz / sample_rate;
            const double feed = level_scale / mode_norm(mode);
            ModalDesign::Mode& entry = result.modes[index];
            entry.section = {feed, 0.0, -feed, -2.0 * radius[index] * std::cos(theta),
                             radius[index] * radius[index]};
        }
        result.count = static_cast<std::uint32_t>(count);
        result.crossover_hz = crossover;
        result.level_scale = static_cast<float>(level_scale);
        modes.resize(count);
        shell.modes = std::move(modes);
        return shell;
    }
} // namespace SushiDSP::room
