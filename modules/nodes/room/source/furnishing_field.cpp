/**************************************************************************/
/* furnishing_field.cpp                                                   */
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
 * @file furnishing_field.cpp
 * @brief Tabulates each placed furnishing's density and weighs paths and modes by it.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/furnishing_field.hpp"

#include <algorithm>
#include <cmath>

#include "SushiDSP/room/air_absorption.hpp"
#include "SushiDSP/room/furniture.hpp"

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds pi.
        constexpr double K_PI = 3.14159265358979323846;

        /// @brief Holds the distance between two samples of a path, in metres.
        constexpr double K_PATH_STEP_M = 0.25;

        /// @brief Bounds the samples one path takes from below.
        constexpr int K_MIN_PATH_SAMPLES = 8;

        /// @brief Bounds the samples one path takes from above.
        constexpr int K_MAX_PATH_SAMPLES = 96;

        /// @brief Counts the midpoints one axis's integrals are taken over.
        constexpr int K_AXIS_POINTS = 512;

        /// @brief Folds an unfolded coordinate back into [0, length] across the mirrored rooms.
        double fold(double coordinate, double length) noexcept
        {
            const double period = 2.0 * length;
            double folded = std::fmod(coordinate, period);
            if (folded < 0.0)
            {
                folded += period;
            }
            return folded > length ? period - folded : folded;
        }

        /// @brief Returns exp(-(u - centre)^2 / (2 spread^2)).
        double bell(double u, double centre, double spread) noexcept
        {
            const double offset = (u - centre) / spread;
            return std::exp(-0.5 * offset * offset);
        }

        /// @brief Returns the integral of bell() over [0, length].
        double bell_integral(double centre, double spread, double length) noexcept
        {
            const double root = spread * std::sqrt(2.0);
            return spread * std::sqrt(0.5 * K_PI) *
                   (std::erf((length - centre) / root) + std::erf(centre / root));
        }
    } // namespace

    FurnishingField::FurnishingField(const RoomScene& scene)
        : lengths_{scene.dimensions.x, scene.dimensions.y, scene.dimensions.z},
          volume_(scene.volume_m3()), medium_(scene)
    {
        const double four_volumes = 4.0 * volume_;
        const std::uint32_t live = std::min<std::uint32_t>(
            scene.furnishing_count, static_cast<std::uint32_t>(RoomScene::K_MAX_FURNISHINGS));
        for (std::uint32_t index = 0; index < live; ++index)
        {
            const RoomScene::Furnishing& entry = scene.furnishings[index];
            const FurnitureItem& row = furniture(entry.item);
            std::array<double, K_BAND_COUNT> coefficient{};
            for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
            {
                coefficient[band] =
                    static_cast<double>(row.absorption_area[band]) * entry.quantity / four_volumes;
            }
            if (!entry.placed)
            {
                for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
                {
                    uniform_[band] += coefficient[band];
                }
                continue;
            }
            Item item{};
            const double width = static_cast<double>(row.box_m[0]);
            const double depth = static_cast<double>(row.box_m[1]);
            const double height = static_cast<double>(row.box_m[2]);
            const double across = std::max(0.5 * std::sqrt(width * depth), K_MIN_SPREAD_M);
            item.centre = Vec3{entry.x_m, entry.y_m, std::min(0.5 * height, lengths_[2])};
            item.spread = {across, across, std::max(0.5 * height, K_MIN_SPREAD_M)};
            item.coefficient = coefficient;
            const std::array<double, 3> centre = {item.centre.x, item.centre.y, item.centre.z};
            double integral = 1.0;
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                integral *= bell_integral(centre[axis], item.spread[axis], lengths_[axis]);
                item.mode_weight[axis] =
                    axis_mode_weights(centre[axis], item.spread[axis], lengths_[axis]);
            }
            item.scale = integral > 0.0 ? 1.0 / integral : 0.0;
            items_.push_back(item);
        }
    }

    std::array<double, FurnishingField::K_MAX_MODE_INDEX + 1>
    FurnishingField::axis_mode_weights(double centre, double spread, double length) noexcept
    {
        std::array<double, K_MAX_MODE_INDEX + 1> weights{};
        std::array<double, K_AXIS_POINTS> bells{};
        double total = 0.0;
        for (int point = 0; point < K_AXIS_POINTS; ++point)
        {
            const double u = (static_cast<double>(point) + 0.5) * length / K_AXIS_POINTS;
            bells[static_cast<std::size_t>(point)] = bell(u, centre, spread);
            total += bells[static_cast<std::size_t>(point)];
        }
        weights[0] = 1.0;
        for (std::uint32_t index = 1; index <= K_MAX_MODE_INDEX; ++index)
        {
            // cos^2 over its mean of one half is 1 + cos of twice the argument.
            double sum = 0.0;
            for (int point = 0; point < K_AXIS_POINTS; ++point)
            {
                const double u = (static_cast<double>(point) + 0.5) / K_AXIS_POINTS;
                sum += bells[static_cast<std::size_t>(point)] *
                       std::cos(2.0 * K_PI * static_cast<double>(index) * u);
            }
            weights[index] = total > 0.0 ? 1.0 + sum / total : 1.0;
        }
        return weights;
    }

    bool FurnishingField::placed() const noexcept
    {
        return !items_.empty();
    }

    std::size_t FurnishingField::placed_count() const noexcept
    {
        return items_.size();
    }

    double FurnishingField::density(std::size_t furnishing, const Vec3& point) const noexcept
    {
        if (furnishing >= items_.size())
        {
            return 0.0;
        }
        const Item& item = items_[furnishing];
        return item.scale * bell(point.x, item.centre.x, item.spread[0]) *
               bell(point.y, item.centre.y, item.spread[1]) *
               bell(point.z, item.centre.z, item.spread[2]);
    }

    double FurnishingField::path_weight(std::size_t furnishing, const Vec3& image,
                                        const Vec3& receiver, double metres) const noexcept
    {
        if (furnishing >= items_.size())
        {
            return 1.0;
        }
        const int samples = std::clamp(static_cast<int>(std::ceil(metres / K_PATH_STEP_M)),
                                       K_MIN_PATH_SAMPLES, K_MAX_PATH_SAMPLES);
        double sum = 0.0;
        for (int sample = 0; sample < samples; ++sample)
        {
            const double along = (static_cast<double>(sample) + 0.5) / samples;
            const Vec3 point{fold(image.x + (receiver.x - image.x) * along, lengths_[0]),
                             fold(image.y + (receiver.y - image.y) * along, lengths_[1]),
                             fold(image.z + (receiver.z - image.z) * along, lengths_[2])};
            sum += density(furnishing, point);
        }
        return volume_ * sum / samples;
    }

    BandResponse FurnishingField::over_path(const Vec3& image, const Vec3& receiver,
                                            double metres) const noexcept
    {
        if (items_.empty())
        {
            return medium_.over_path(metres);
        }
        std::array<double, K_BAND_COUNT> coefficient = uniform_;
        for (std::size_t index = 0; index < items_.size(); ++index)
        {
            const double weight = path_weight(index, image, receiver, metres);
            for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
            {
                coefficient[band] += weight * items_[index].coefficient[band];
            }
        }
        BandResponse response = AirAbsorption::over_path(metres);
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            response.gain[band] *= static_cast<float>(std::exp(-0.5 * coefficient[band] * metres));
        }
        return response;
    }

    double FurnishingField::mode_coefficient_per_metre(std::size_t band, std::uint32_t nx,
                                                       std::uint32_t ny,
                                                       std::uint32_t nz) const noexcept
    {
        if (items_.empty())
        {
            return medium_.furnishing_coefficient_per_metre(band);
        }
        const std::array<std::uint32_t, 3> indices = {nx, ny, nz};
        double coefficient = uniform_[band];
        for (const Item& item : items_)
        {
            double weight = 1.0;
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                weight *= indices[axis] <= K_MAX_MODE_INDEX
                              ? item.mode_weight[axis][indices[axis]]
                              : 1.0;
            }
            coefficient += weight * item.coefficient[band];
        }
        return coefficient;
    }
} // namespace SushiDSP::room
