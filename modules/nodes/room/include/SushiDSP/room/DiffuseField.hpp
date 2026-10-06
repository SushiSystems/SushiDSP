/**************************************************************************/
/* DiffuseField.hpp                                                       */
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
 * @file DiffuseField.hpp
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

#include "SushiDSP/room/BandResponse.hpp"
#include "SushiDSP/room/CabinetDirectivity.hpp"
#include "SushiDSP/room/EyringDecay.hpp"
#include "SushiDSP/room/RoomScene.hpp"

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
        const std::array<double, kBandCount>& band_power() const noexcept;

    private:
        /// @brief Holds 16 pi, the diffuse-field pressure squared per unit input power over the
        /// equivalent absorption area, for a direct sound of unit gain at one metre.
        static constexpr double kDiffuseFieldFactor = 16.0 * 3.14159265358979323846;

        /// @brief Holds Sabine's constant 24 ln(10) / c at 20 degrees, in seconds per metre.
        static constexpr double kSabineConstant = 0.161;

        /// @brief Returns 10^(-DI / 10) per band for one geometry, the cabinet's mean radiated
        /// power relative to on axis, remembering the last geometry's answer: the integral over
        /// the 2562-direction grid costs about 18 ms and a scene drag never changes it.
        static std::array<double, kBandCount> cabinet_mean_power(const CabinetGeometry& geometry);

        /// @brief Returns whether two geometries are the same four numbers.
        static bool same_geometry(const CabinetGeometry& a, const CabinetGeometry& b) noexcept;

        /// @brief Integrates a + b ln(f) over [lower, upper].
        static double integrate_log_line(double a, double b, double lower, double upper) noexcept;

        /// @brief Stores the power per hertz at each band centre.
        std::array<double, kBandCount> band_power_{};
    };

    inline DiffuseField::DiffuseField(const RoomScene& scene)
    {
        const std::array<double, kBandCount> rt60 = EyringDecay::rt60_seconds(scene);
        const std::array<double, kBandCount> mean_power = cabinet_mean_power(scene.cabinet);
        const double per_second = kDiffuseFieldFactor / (kSabineConstant * scene.volume_m3());
        for (std::size_t band = 0; band < kBandCount; ++band)
        {
            band_power_[band] = per_second * rt60[band] * mean_power[band];
        }
    }

    inline double DiffuseField::power_at(double frequency_hz) const noexcept
    {
        if (frequency_hz <= kBandCentresHz.front())
        {
            return band_power_.front();
        }
        for (std::size_t band = 1; band < kBandCount; ++band)
        {
            if (frequency_hz <= kBandCentresHz[band])
            {
                const double position = std::log(frequency_hz / kBandCentresHz[band - 1])
                    / std::log(kBandCentresHz[band] / kBandCentresHz[band - 1]);
                return band_power_[band - 1]
                    + (band_power_[band] - band_power_[band - 1]) * position;
            }
        }
        return band_power_.back();
    }

    inline double DiffuseField::power_between(double lower_hz, double upper_hz) const noexcept
    {
        double total = 0.0;
        for (std::size_t segment = 0; segment <= kBandCount; ++segment)
        {
            const bool first = segment == 0;
            const bool last = segment == kBandCount;
            const double from = first ? 0.0 : kBandCentresHz[segment - 1];
            const double to = last ? std::numeric_limits<double>::infinity()
                                   : kBandCentresHz[segment];
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

    inline const std::array<double, kBandCount>& DiffuseField::band_power() const noexcept
    {
        return band_power_;
    }

    inline std::array<double, kBandCount>
        DiffuseField::cabinet_mean_power(const CabinetGeometry& geometry)
    {
        static std::mutex mutex;
        static bool cached = false;
        static CabinetGeometry key{};
        static std::array<double, kBandCount> value{};
        const std::lock_guard<std::mutex> lock(mutex);
        if (!cached || !same_geometry(key, geometry))
        {
            const std::array<float, kBandCount> index =
                CabinetDirectivity(geometry).directivity_index_db();
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                value[band] = std::pow(10.0, -0.1 * static_cast<double>(index[band]));
            }
            key = geometry;
            cached = true;
        }
        return value;
    }

    inline bool DiffuseField::same_geometry(const CabinetGeometry& a,
                                            const CabinetGeometry& b) noexcept
    {
        return a.cone_radius_m == b.cone_radius_m && a.driver_spacing_m == b.driver_spacing_m
            && a.baffle_width_m == b.baffle_width_m
            && a.breakup_frequency_hz == b.breakup_frequency_hz;
    }

    inline double DiffuseField::integrate_log_line(double a, double b, double lower,
                                                   double upper) noexcept
    {
        // The lower limit's f ln f term is taken as zero at f = 0.
        const double lower_term = lower > 0.0 ? lower * std::log(lower) - lower : 0.0;
        return a * (upper - lower) + b * (upper * std::log(upper) - upper - lower_term);
    }
}
