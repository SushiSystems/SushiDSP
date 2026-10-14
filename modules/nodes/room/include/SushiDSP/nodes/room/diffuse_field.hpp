/**************************************************************************/
/* diffuse_field.hpp                                                      */
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
 * @file diffuse_field.hpp
 * @brief The steady-state reverberant field one cabinet raises in one room, per hertz.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <mutex>

#include <SushiDSP/nodes/room/band_response.hpp>
#include <SushiDSP/nodes/room/cabinet_directivity.hpp>
#include <SushiDSP/nodes/room/eyring_decay.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

namespace SushiDSP::room
{
    /// @brief Evaluates the room's steady-state reverberant power per hertz of a white input,
    /// relative to the cabinet's on-axis direct sound at one metre: 16 pi RT60(f) / (0.161 V)
    /// over the cabinet's directivity index, linear in log frequency between the band centres
    /// and flat outside them. The one reference the modal layer and the tail are both scaled
    /// to. Control thread. Spec §6.3 and §9a.4.
    class DiffuseField
    {
        public:
            /// @brief Evaluates the field for one scene from Eyring's decay and the cabinet's mean
            /// radiated power.
            explicit DiffuseField(const RoomScene& scene);

            /// @brief Returns the field's power per hertz at one frequency.
            double power_at(double frequency_hz) const noexcept;

            /// @brief Integrates power_at over [lower_hz, upper_hz], in power times hertz.
            /// @pre 0 <= lower_hz <= upper_hz.
            double power_between(double lower_hz, double upper_hz) const noexcept;

            /// @brief Returns the field's power per hertz at each band centre.
            const std::array<double, K_BAND_COUNT>& band_power() const noexcept;

        private:
            /// @brief Holds 16 pi, the diffuse-field pressure squared per unit input power over the
            /// equivalent absorption area, for a direct sound of unit gain at one metre.
            static constexpr double K_DIFFUSE_FIELD_FACTOR = 16.0 * 3.14159265358979323846;

            /// @brief Holds Sabine's constant 24 ln(10) / c at 20 degrees, in seconds per metre.
            static constexpr double K_SABINE_CONSTANT = 0.161;

            /// @brief Counts the geometries whose answer cabinet_mean_power() remembers.
            static constexpr std::size_t K_REMEMBERED_GEOMETRIES = 4;

            /// @brief Returns 10^(-DI / 10) per band for one geometry, the cabinet's mean radiated
            /// power relative to on axis, remembering the last few geometries' answers: the
            /// integral over the 2562-direction grid costs about 18 ms and a drag never changes it.
            static std::array<double, K_BAND_COUNT>
            cabinet_mean_power(const CabinetGeometry& geometry);

            /// @brief Integrates a + b ln(f) over [lower, upper].
            static double integrate_log_line(double a, double b, double lower,
                                             double upper) noexcept;

            /// @brief Stores the power per hertz at each band centre.
            std::array<double, K_BAND_COUNT> band_power_{};
    };

    inline DiffuseField::DiffuseField(const RoomScene& scene)
    {
        const std::array<double, K_BAND_COUNT> rt60 = EyringDecay::rt60_seconds(scene);
        const std::array<double, K_BAND_COUNT> mean_power = cabinet_mean_power(scene.cabinet);
        const double per_second = K_DIFFUSE_FIELD_FACTOR / (K_SABINE_CONSTANT * scene.volume_m3());
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            band_power_[band] = per_second * rt60[band] * mean_power[band];
        }
    }

    inline double DiffuseField::power_at(double frequency_hz) const noexcept
    {
        if (frequency_hz <= K_BAND_CENTRES_HZ.front())
        {
            return band_power_.front();
        }
        for (std::size_t band = 1; band < K_BAND_COUNT; ++band)
        {
            if (frequency_hz <= K_BAND_CENTRES_HZ[band])
            {
                const double position =
                    std::log(frequency_hz / K_BAND_CENTRES_HZ[band - 1]) /
                    std::log(K_BAND_CENTRES_HZ[band] / K_BAND_CENTRES_HZ[band - 1]);
                return band_power_[band - 1] +
                       (band_power_[band] - band_power_[band - 1]) * position;
            }
        }
        return band_power_.back();
    }

    inline double DiffuseField::power_between(double lower_hz, double upper_hz) const noexcept
    {
        double total = 0.0;
        for (std::size_t segment = 0; segment <= K_BAND_COUNT; ++segment)
        {
            const bool first = segment == 0;
            const bool last = segment == K_BAND_COUNT;
            const double from = first ? 0.0 : K_BAND_CENTRES_HZ[segment - 1];
            const double to =
                last ? std::numeric_limits<double>::infinity() : K_BAND_CENTRES_HZ[segment];
            const double piece_lower = std::max(lower_hz, from);
            const double piece_upper = std::min(upper_hz, to);
            if (piece_upper <= piece_lower)
            {
                continue;
            }
            double slope = 0.0;
            double intercept = first ? band_power_.front() : band_power_[segment - 1];
            if (!first && !last)
            {
                slope = (band_power_[segment] - band_power_[segment - 1]) / std::log(to / from);
                intercept = band_power_[segment - 1] - slope * std::log(from);
            }
            total += integrate_log_line(intercept, slope, piece_lower, piece_upper);
        }
        return total;
    }

    inline const std::array<double, K_BAND_COUNT>& DiffuseField::band_power() const noexcept
    {
        return band_power_;
    }

    inline std::array<double, K_BAND_COUNT>
    DiffuseField::cabinet_mean_power(const CabinetGeometry& geometry)
    {
        static std::mutex mutex;
        static std::array<CabinetGeometry, K_REMEMBERED_GEOMETRIES> keys{};
        static std::array<std::array<double, K_BAND_COUNT>, K_REMEMBERED_GEOMETRIES> values{};
        static std::size_t remembered = 0;
        static std::size_t next = 0;
        const std::lock_guard<std::mutex> lock(mutex);
        for (std::size_t slot = 0; slot < remembered; ++slot)
        {
            if (same_geometry(keys[slot], geometry))
            {
                return values[slot];
            }
        }
        const std::array<float, K_BAND_COUNT> index =
            CabinetDirectivity(geometry).directivity_index_db();
        std::array<double, K_BAND_COUNT>& value = values[next];
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            value[band] = std::pow(10.0, -0.1 * static_cast<double>(index[band]));
        }
        keys[next] = geometry;
        const std::array<double, K_BAND_COUNT> answer = value;
        // The oldest answer is the one a fifth geometry replaces.
        next = (next + 1u) % K_REMEMBERED_GEOMETRIES;
        remembered = std::min(remembered + 1u, K_REMEMBERED_GEOMETRIES);
        return answer;
    }

    inline double DiffuseField::integrate_log_line(double a, double b, double lower,
                                                   double upper) noexcept
    {
        // The lower limit's f ln f term is taken as zero at f = 0.
        const double lower_term = lower > 0.0 ? lower * std::log(lower) - lower : 0.0;
        return a * (upper - lower) + b * (upper * std::log(upper) - upper - lower_term);
    }
} // namespace SushiDSP::room
