/**************************************************************************/
/* ImageSourceSolver.cpp                                                  */
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
 * @file ImageSourceSolver.cpp
 * @brief The mirror lattice, its per-axis wall tables, and the reduction of each image to a tap.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/ImageSourceSolver.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "SushiDSP/room/AirAbsorption.hpp"
#include "SushiDSP/room/AmbisonicEncoder.hpp"
#include "SushiDSP/room/BandResponseFit.hpp"
#include "SushiDSP/room/CabinetDirectivity.hpp"
#include "SushiDSP/room/Materials.hpp"
#include "SushiDSP/room/MicrophoneModel.hpp"

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Names a band product below which a path carries nothing and is not fitted.
        constexpr double kSilentGain = 1.0e-10;

        /// @brief Holds one reflection count of one axis: where the image lands and what its
        /// bounces cost.
        struct AxisEntry
        {
            /// @brief Stores the image coordinate on this axis, in metres.
            double coordinate;

            /// @brief Stores the product of the crossed faces' reflection factors, per band.
            std::array<double, kBandCount> wall;

            /// @brief Stores the product of (1 - s) over the crossed faces, an energy share.
            double specular_share;

            /// @brief Stores -1 when the count is odd, so the outgoing direction flips, else 1.
            double direction_sign;
        };

        /// @brief Holds one axis's entries for every count in [-order, order], contiguous.
        struct AxisTable
        {
            /// @brief Stores the entries, index n + order.
            std::vector<AxisEntry> entries;

            /// @brief Returns the entry of one signed reflection count.
            const AxisEntry& at(int count, std::uint32_t order) const noexcept
            {
                return entries[static_cast<std::size_t>(count + static_cast<int>(order))];
            }
        };

        /// @brief Holds one image that survived enumeration, before it is fitted.
        struct Candidate
        {
            /// @brief Stores the arrival in frames at the solve's sample rate.
            double delay_frames;

            /// @brief Stores the signed reflection counts on x, y and z.
            std::array<int, 3> counts;
        };

        /// @brief Holds a receiver's pose: where the path ends and which way it faces.
        struct Receiver
        {
            /// @brief Stores the receiver's position.
            Vec3 position;

            /// @brief Stores the receiver's yaw, in radians, zero facing +x.
            double yaw_rad;
        };

        /// @brief Returns the area of one face of the shoebox, in square metres.
        double face_area_m2(Surface surface, const Vec3& dimensions) noexcept
        {
            switch (surface)
            {
            case Surface::MinusX:
            case Surface::PlusX:
                return dimensions.y * dimensions.z;
            case Surface::MinusY:
            case Surface::PlusY:
                return dimensions.x * dimensions.z;
            case Surface::Floor:
            case Surface::Ceiling:
                return dimensions.x * dimensions.y;
            }
            return 0.0;
        }

        /// @brief Returns sqrt(1 - alpha) per band for one face, alpha area-weighted over the
        /// face's own material and the patches on it.
        std::array<double, kBandCount> face_reflection(const RoomScene& scene, Surface surface)
        {
            const double face_area = face_area_m2(surface, scene.dimensions);
            std::array<double, kBandCount> absorption{};
            double patch_area = 0.0;
            for (std::uint32_t index = 0; index < scene.patch_count; ++index)
            {
                const RoomScene::Patch& patch = scene.patches[index];
                if (patch.surface != surface)
                {
                    continue;
                }
                const Material& row = material(patch.material);
                for (std::size_t band = 0; band < kBandCount; ++band)
                {
                    absorption[band] += patch.area_m2 * static_cast<double>(row.absorption[band]);
                }
                patch_area += patch.area_m2;
            }
            const Material& bare = material(scene.surfaces[static_cast<std::size_t>(surface)]);
            std::array<double, kBandCount> reflection{};
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                const double weighted = absorption[band]
                    + (face_area - patch_area) * static_cast<double>(bare.absorption[band]);
                reflection[band] = std::sqrt(std::max(0.0, 1.0 - weighted / face_area));
            }
            return reflection;
        }

        /// @brief Builds one axis's table: for count n the image sits at n L + (n even ? s :
        /// L - s), having crossed the far face ceil(|n| / 2) times and the near face floor(|n| / 2)
        /// times, the far face being the one on the side n points to.
        AxisTable build_axis_table(const RoomScene& scene, Surface minus_face, Surface plus_face,
                                   double length, double source, std::uint32_t order)
        {
            const std::array<double, kBandCount> minus_reflection =
                face_reflection(scene, minus_face);
            const std::array<double, kBandCount> plus_reflection =
                face_reflection(scene, plus_face);
            const double minus_share = 1.0
                - static_cast<double>(scattering_coefficient(
                    scene.scattering[static_cast<std::size_t>(minus_face)]));
            const double plus_share = 1.0
                - static_cast<double>(scattering_coefficient(
                    scene.scattering[static_cast<std::size_t>(plus_face)]));

            AxisTable table;
            table.entries.resize(2u * order + 1u);
            for (int count = -static_cast<int>(order); count <= static_cast<int>(order); ++count)
            {
                const int magnitude = std::abs(count);
                const bool even = (magnitude % 2) == 0;
                const int far_bounces = (magnitude + 1) / 2;
                const int near_bounces = magnitude / 2;
                const int plus_bounces = count > 0 ? far_bounces : near_bounces;
                const int minus_bounces = count > 0 ? near_bounces : far_bounces;

                AxisEntry& entry =
                    table.entries[static_cast<std::size_t>(count + static_cast<int>(order))];
                entry.coordinate =
                    static_cast<double>(count) * length + (even ? source : length - source);
                entry.direction_sign = even ? 1.0 : -1.0;
                entry.specular_share =
                    std::pow(minus_share, minus_bounces) * std::pow(plus_share, plus_bounces);
                for (std::size_t band = 0; band < kBandCount; ++band)
                {
                    entry.wall[band] = std::pow(minus_reflection[band], minus_bounces)
                        * std::pow(plus_reflection[band], plus_bounces);
                }
            }
            return table;
        }

        /// @brief Returns the receiver the listener kind names.
        Receiver receiver_for(const RoomScene& scene, ListenerKind listener) noexcept
        {
            if (listener == ListenerKind::Microphone)
            {
                return { scene.microphone_position, scene.microphone_yaw_rad };
            }
            return { scene.listener_position, scene.listener_yaw_rad };
        }

        /// @brief Returns the wall product of one image, multiplied over its three axes.
        std::array<double, kBandCount> wall_product(const AxisEntry& x, const AxisEntry& y,
                                                    const AxisEntry& z) noexcept
        {
            std::array<double, kBandCount> product{};
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                product[band] = x.wall[band] * y.wall[band] * z.wall[band];
            }
            return product;
        }

        /// @brief Returns true when every band of the product is below the silence floor.
        bool silent(const std::array<double, kBandCount>& product) noexcept
        {
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                if (product[band] > kSilentGain)
                {
                    return false;
                }
            }
            return true;
        }

        /// @brief Returns the distance from the receiver to the image the three entries place.
        double image_distance(const AxisEntry& x, const AxisEntry& y, const AxisEntry& z,
                              const Vec3& receiver) noexcept
        {
            const double dx = x.coordinate - receiver.x;
            const double dy = y.coordinate - receiver.y;
            const double dz = z.coordinate - receiver.z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        /// @brief Enumerates every lattice point with |n_x| + |n_y| + |n_z| <= order that
        /// arrives before the crossover and reflects off something, in lattice order.
        std::vector<Candidate> enumerate(const std::array<AxisTable, 3>& tables,
                                         std::uint32_t order, const Vec3& receiver,
                                         double frames_per_metre, double crossover_frames)
        {
            std::vector<Candidate> candidates;
            candidates.reserve(ImageSourceSolver::image_count(order));
            const int bound = static_cast<int>(order);
            for (int nx = -bound; nx <= bound; ++nx)
            {
                const AxisEntry& x = tables[0].at(nx, order);
                const int y_bound = bound - std::abs(nx);
                for (int ny = -y_bound; ny <= y_bound; ++ny)
                {
                    const AxisEntry& y = tables[1].at(ny, order);
                    const int z_bound = y_bound - std::abs(ny);
                    for (int nz = -z_bound; nz <= z_bound; ++nz)
                    {
                        const AxisEntry& z = tables[2].at(nz, order);
                        const double delay = image_distance(x, y, z, receiver) * frames_per_metre;
                        if (delay > crossover_frames || silent(wall_product(x, y, z)))
                        {
                            continue;
                        }
                        candidates.push_back({ delay, { nx, ny, nz } });
                    }
                }
            }
            return candidates;
        }

        /// @brief Returns the direction the real cabinet radiates along for one image, in the
        /// cabinet's frame: each odd axis count flips that component of the image-to-receiver
        /// direction.
        void outgoing_direction(const AxisEntry& x, const AxisEntry& y, const AxisEntry& z,
                                const Vec3& receiver, double distance, double source_yaw,
                                double& azimuth_rad, double& elevation_rad) noexcept
        {
            const double dx = x.direction_sign * (receiver.x - x.coordinate) / distance;
            const double dy = y.direction_sign * (receiver.y - y.coordinate) / distance;
            const double dz = z.direction_sign * (receiver.z - z.coordinate) / distance;
            azimuth_rad = std::atan2(dy, dx) - source_yaw;
            elevation_rad = std::asin(std::clamp(dz, -1.0, 1.0));
        }

        /// @brief Multiplies a BandResponse's float gains into a double band product.
        void fold(std::array<double, kBandCount>& product, const BandResponse& factor) noexcept
        {
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                product[band] *= static_cast<double>(factor.gain[band]);
            }
        }

        /// @brief Returns the mean band power of a product, what a diffuse field receives.
        double mean_power(const std::array<double, kBandCount>& product) noexcept
        {
            double sum = 0.0;
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                sum += product[band] * product[band];
            }
            return sum / static_cast<double>(kBandCount);
        }
    }

    std::uint32_t ImageSourceSolver::image_count(std::uint32_t order) noexcept
    {
        const std::uint32_t n = order;
        return (2u * n * (n + 1u) * (2u * n + 1u)) / 3u + 2u * n + 1u;
    }

    double ImageSourceSolver::crossover_time_s(const RoomScene& scene, std::uint32_t order) noexcept
    {
        const double shortest = std::min({ scene.dimensions.x, scene.dimensions.y,
                                           scene.dimensions.z });
        return static_cast<double>(order + 1u) * shortest / kSpeedOfSound;
    }

    EarlyTapSet ImageSourceSolver::solve(const RoomScene& scene, const Options& options)
    {
        const std::uint32_t order = options.max_order;
        const Receiver receiver = receiver_for(scene, options.listener);
        const double frames_per_metre = options.sample_rate / kSpeedOfSound;
        const double crossover_s = crossover_time_s(scene, order);
        const double crossover_frames = crossover_s * options.sample_rate;

        const std::array<AxisTable, 3> tables = {
            build_axis_table(scene, Surface::MinusX, Surface::PlusX, scene.dimensions.x,
                             scene.source_position.x, order),
            build_axis_table(scene, Surface::MinusY, Surface::PlusY, scene.dimensions.y,
                             scene.source_position.y, order),
            build_axis_table(scene, Surface::Floor, Surface::Ceiling, scene.dimensions.z,
                             scene.source_position.z, order)
        };

        std::vector<Candidate> candidates =
            enumerate(tables, order, receiver.position, frames_per_metre, crossover_frames);
        std::stable_sort(candidates.begin(), candidates.end(),
                         [](const Candidate& a, const Candidate& b)
                         { return a.delay_frames < b.delay_frames; });
        // The set never grows past its fixed capacity; the latest arrivals are the ones left out.
        const std::size_t kept = std::min(candidates.size(), EarlyTapSet::kMaxEarlyTaps);

        const CabinetDirectivity cabinet(scene.cabinet);
        const MicrophoneModel microphone(scene.microphone_pattern,
                                         kMicrophoneFlatCardioid.on_axis_db);
        const double forward_x = std::cos(receiver.yaw_rad);
        const double forward_y = std::sin(receiver.yaw_rad);

        EarlyTapSet set{};
        set.count = 0;
        set.crossover_time_s = static_cast<float>(crossover_s);
        double scattered_energy = 0.0;

        for (std::size_t index = 0; index < kept; ++index)
        {
            const Candidate& candidate = candidates[index];
            const AxisEntry& x = tables[0].at(candidate.counts[0], order);
            const AxisEntry& y = tables[1].at(candidate.counts[1], order);
            const AxisEntry& z = tables[2].at(candidate.counts[2], order);
            const double distance = candidate.delay_frames / frames_per_metre;

            std::array<double, kBandCount> product = wall_product(x, y, z);
            double azimuth = 0.0;
            double elevation = 0.0;
            outgoing_direction(x, y, z, receiver.position, distance, scene.source_yaw_rad,
                               azimuth, elevation);
            fold(product, cabinet.gain_for(azimuth, elevation));
            fold(product, AirAbsorption::over_path(distance));

            const double arrival_x = (x.coordinate - receiver.position.x) / distance;
            const double arrival_y = (y.coordinate - receiver.position.y) / distance;
            const double arrival_z = std::clamp((z.coordinate - receiver.position.z) / distance,
                                                -1.0, 1.0);
            EarlyTap& tap = set.taps[index];
            if (options.listener == ListenerKind::Microphone)
            {
                const double cosine = std::clamp(arrival_x * forward_x + arrival_y * forward_y,
                                                 -1.0, 1.0);
                fold(product, microphone.gain_for(std::acos(cosine), distance));
                tap.encode.fill(0.0f);
                tap.encode[0] = 1.0f;
            }
            else
            {
                AmbisonicEncoder::encode(std::atan2(arrival_y, arrival_x) - receiver.yaw_rad,
                                         std::asin(arrival_z), tap.encode);
            }

            BandResponse target{};
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                target.gain[band] = static_cast<float>(product[band]);
            }
            const BandResponseFit::Result fit = BandResponseFit::fit(target, options.sample_rate);
            tap.section_a = fit.coefficients.section_a;
            tap.section_b = fit.coefficients.section_b;

            const double specular_share = x.specular_share * y.specular_share * z.specular_share;
            tap.delay_frames = static_cast<float>(candidate.delay_frames);
            tap.gain = static_cast<float>(std::sqrt(specular_share) / distance);
            scattered_energy += mean_power(product) * (1.0 - specular_share) / (distance * distance);
            ++set.count;
        }

        set.scattered_energy = static_cast<float>(scattered_energy);
        return set;
    }
}
