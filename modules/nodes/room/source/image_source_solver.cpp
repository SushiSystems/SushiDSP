/**************************************************************************/
/* image_source_solver.cpp                                                */
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
 * @file image_source_solver.cpp
 * @brief The mirror lattice, its per-axis wall tables, and the reduction of each image to a tap.
 * @author Mustafa Garip & Sushi Systems
 */

#include <SushiDSP/nodes/room/image_source_solver.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include <SushiDSP/foundation/log/log.hpp>
#include <SushiDSP/nodes/room/air_absorption.hpp>
#include <SushiDSP/nodes/room/ambisonic_encoder.hpp>
#include <SushiDSP/nodes/room/band_response_fit.hpp>
#include <SushiDSP/nodes/room/cabinet_directivity.hpp>
#include <SushiDSP/nodes/room/driver_field.hpp>
#include <SushiDSP/nodes/room/furnishing_field.hpp>
#include <SushiDSP/nodes/room/materials.hpp>
#include <SushiDSP/nodes/room/medium_absorption.hpp>
#include <SushiDSP/nodes/room/microphone_model.hpp>
#include <SushiDSP/nodes/room/room_log_category.hpp>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Names a band product below which a path carries nothing and is not fitted.
        constexpr double K_SILENT_GAIN = 1.0e-10;

        /// @brief Holds one reflection count of one axis: where the image lands and what its
        /// bounces cost.
        struct AxisEntry
        {
                /// @brief Stores the image coordinate on this axis, in metres.
                double coordinate;

                /// @brief Stores the product of the crossed faces' reflection factors, per band.
                std::array<double, K_BAND_COUNT> wall;

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

        /// @brief Returns sqrt(1 - alpha) per band for one face, the absorption coming from
        /// RoomScene so the solver and Eyring weight a patched face the same way.
        std::array<double, K_BAND_COUNT> face_reflection(const RoomScene& scene, Surface surface)
        {
            const std::array<double, K_BAND_COUNT> absorption = scene.face_absorption(surface);
            std::array<double, K_BAND_COUNT> reflection{};
            for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
            {
                reflection[band] = std::sqrt(std::max(0.0, 1.0 - absorption[band]));
            }
            return reflection;
        }

        /// @brief Builds one axis's table: for count n the image sits at n L + (n even ? s :
        /// L - s), having crossed the far face ceil(|n| / 2) times and the near face floor(|n| / 2)
        /// times, the far face being the one on the side n points to.
        AxisTable build_axis_table(const RoomScene& scene, Surface minus_face, Surface plus_face,
                                   double length, double source, std::uint32_t order)
        {
            const std::array<double, K_BAND_COUNT> minus_reflection =
                face_reflection(scene, minus_face);
            const std::array<double, K_BAND_COUNT> plus_reflection =
                face_reflection(scene, plus_face);
            const double minus_share =
                1.0 - static_cast<double>(scattering_coefficient(
                          scene.scattering[static_cast<std::size_t>(minus_face)]));
            const double plus_share =
                1.0 - static_cast<double>(scattering_coefficient(
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
                for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
                {
                    entry.wall[band] = std::pow(minus_reflection[band], minus_bounces) *
                                       std::pow(plus_reflection[band], plus_bounces);
                }
            }
            return table;
        }

        /// @brief Returns the receiver the listener kind names.
        Receiver receiver_for(const RoomScene& scene, ListenerKind listener) noexcept
        {
            if (listener == ListenerKind::Microphone)
            {
                return {scene.microphone_position, scene.microphone_yaw_rad};
            }
            return {scene.listener_position, scene.listener_yaw_rad};
        }

        /// @brief Returns the wall product of one image, multiplied over its three axes.
        std::array<double, K_BAND_COUNT> wall_product(const AxisEntry& x, const AxisEntry& y,
                                                      const AxisEntry& z) noexcept
        {
            std::array<double, K_BAND_COUNT> product{};
            for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
            {
                product[band] = x.wall[band] * y.wall[band] * z.wall[band];
            }
            return product;
        }

        /// @brief Returns true when every band of the product is below the silence floor.
        bool silent(const std::array<double, K_BAND_COUNT>& product) noexcept
        {
            for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
            {
                if (product[band] > K_SILENT_GAIN)
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
                        candidates.push_back({delay, {nx, ny, nz}});
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
        void fold(std::array<double, K_BAND_COUNT>& product, const BandResponse& factor) noexcept
        {
            for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
            {
                product[band] *= static_cast<double>(factor.gain[band]);
            }
        }

        /// @brief Returns true when the candidate is the line of sight: no reflection on any axis.
        bool is_direct(const Candidate& candidate) noexcept
        {
            return candidate.counts[0] == 0 && candidate.counts[1] == 0 && candidate.counts[2] == 0;
        }

        /// @brief Fits a band product into a tap's two sections.
        void fit_sections(EarlyTap& tap, const std::array<double, K_BAND_COUNT>& product,
                          double sample_rate)
        {
            BandResponse target{};
            for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
            {
                target.gain[band] = static_cast<float>(product[band]);
            }
            const BandResponseFit::Result fit = BandResponseFit::fit(target, sample_rate);
            tap.section_a = fit.coefficients.section_a;
            tap.section_b = fit.coefficients.section_b;
        }

        /// @brief Writes a microphone's line of sight as one tap per radiating element of the
        /// cabinet: the element's share, the air, and the capsule's pattern and proximity at the
        /// element's own distance and direction.
        /// @return The number of taps written, at the head of the set.
        std::uint32_t write_element_taps(EarlyTapSet& set, const RoomScene& scene,
                                         const Receiver& receiver,
                                         const MicrophoneModel& microphone, double sample_rate)
        {
            const DriverField::Field field = DriverField(scene.cabinet)
                                                 .toward(scene.source_position,
                                                         scene.source_yaw_rad, receiver.position);
            const double forward_x = std::cos(receiver.yaw_rad);
            const double forward_y = std::sin(receiver.yaw_rad);
            for (std::uint32_t index = 0; index < field.count; ++index)
            {
                const DriverField::Arrival& arrival = field.arrivals[index];
                std::array<double, K_BAND_COUNT> product{};
                for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
                {
                    product[band] = static_cast<double>(arrival.gain.gain[band]);
                }
                fold(product, AirAbsorption::over_path(arrival.distance_m));
                // The capsule sees the element along the path reversed.
                const double cosine = std::clamp(
                    -(arrival.direction.x * forward_x + arrival.direction.y * forward_y), -1.0,
                    1.0);
                fold(product, microphone.gain_for(std::acos(cosine), arrival.distance_m));

                EarlyTap& tap = set.taps[index];
                fit_sections(tap, product, sample_rate);
                tap.delay_frames =
                    static_cast<float>(arrival.distance_m * sample_rate / K_SPEED_OF_SOUND);
                tap.gain = static_cast<float>(1.0 / arrival.distance_m);
                tap.encode.fill(0.0f);
                tap.encode[0] = 1.0f;
            }
            return field.count;
        }

        /// @brief Returns the mean band power of a product, what a diffuse field receives.
        double mean_power(const std::array<double, K_BAND_COUNT>& product) noexcept
        {
            double sum = 0.0;
            for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
            {
                sum += product[band] * product[band];
            }
            return sum / static_cast<double>(K_BAND_COUNT);
        }
    } // namespace

    std::uint32_t ImageSourceSolver::image_count(std::uint32_t order) noexcept
    {
        const std::uint32_t n = order;
        return (2u * n * (n + 1u) * (2u * n + 1u)) / 3u + 2u * n + 1u;
    }

    double ImageSourceSolver::crossover_time_s(const RoomScene& scene, std::uint32_t order) noexcept
    {
        const double shortest =
            std::min({scene.dimensions.x, scene.dimensions.y, scene.dimensions.z});
        return static_cast<double>(order + 1u) * shortest / K_SPEED_OF_SOUND;
    }

    EarlyTapSet ImageSourceSolver::solve(const RoomScene& scene, const Options& options)
    {
        const std::uint32_t order = options.max_order;
        const Receiver receiver = receiver_for(scene, options.listener);
        const double frames_per_metre = options.sample_rate / K_SPEED_OF_SOUND;
        const double crossover_s = crossover_time_s(scene, order);
        const double crossover_frames = crossover_s * options.sample_rate;

        const std::array<AxisTable, 3> tables = {
            build_axis_table(scene, Surface::MinusX, Surface::PlusX, scene.dimensions.x,
                             scene.source_position.x, order),
            build_axis_table(scene, Surface::MinusY, Surface::PlusY, scene.dimensions.y,
                             scene.source_position.y, order),
            build_axis_table(scene, Surface::Floor, Surface::Ceiling, scene.dimensions.z,
                             scene.source_position.z, order)};

        std::vector<Candidate> candidates =
            enumerate(tables, order, receiver.position, frames_per_metre, crossover_frames);
        std::stable_sort(candidates.begin(), candidates.end(),
                         [](const Candidate& a, const Candidate& b)
                         {
                             return a.delay_frames < b.delay_frames;
                         });
        const CabinetDirectivity cabinet(scene.cabinet);
        const FurnishingField furnishings(scene);
        const MicrophoneModel microphone(scene.microphone_pattern,
                                         microphone_preset(scene.microphone_kind).on_axis_db,
                                         scene.microphone_proximity);
        const double forward_x = std::cos(receiver.yaw_rad);
        const double forward_y = std::sin(receiver.yaw_rad);

        EarlyTapSet set{};
        set.count = 0;
        set.crossover_time_s = static_cast<float>(crossover_s);
        double scattered_energy = 0.0;

        const bool to_microphone = options.listener == ListenerKind::Microphone;
        if (to_microphone)
        {
            set.count = write_element_taps(set, scene, receiver, microphone, options.sample_rate);
            set.direct_count = set.count;
        }

        for (const Candidate& candidate : candidates)
        {
            // A microphone's line of sight is the element taps already written.
            if (to_microphone && is_direct(candidate))
            {
                continue;
            }
            // The set never grows past its capacity; the latest arrivals are the ones left out.
            if (set.count >= EarlyTapSet::K_MAX_EARLY_TAPS)
            {
                const std::size_t left_out =
                    candidates.size() - static_cast<std::size_t>(&candidate - candidates.data());
                SD_LOG_WARN(K_ROOM_LOG_CATEGORY,
                            "the image-source solve kept {} early taps, the capacity; {} later "
                            "arrivals of the {} candidate images are left out",
                            set.count, left_out, candidates.size());
                break;
            }
            const AxisEntry& x = tables[0].at(candidate.counts[0], order);
            const AxisEntry& y = tables[1].at(candidate.counts[1], order);
            const AxisEntry& z = tables[2].at(candidate.counts[2], order);
            const double distance = candidate.delay_frames / frames_per_metre;

            std::array<double, K_BAND_COUNT> product = wall_product(x, y, z);
            double azimuth = 0.0;
            double elevation = 0.0;
            outgoing_direction(x, y, z, receiver.position, distance, scene.source_yaw_rad, azimuth,
                               elevation);
            fold(product, cabinet.gain_for(azimuth, elevation));
            // Furniture does not thin the line of sight; a reflected path loses by where it runs.
            const Vec3 image{x.coordinate, y.coordinate, z.coordinate};
            fold(product, is_direct(candidate)
                              ? AirAbsorption::over_path(distance)
                              : furnishings.over_path(image, receiver.position, distance));

            const double arrival_x = (x.coordinate - receiver.position.x) / distance;
            const double arrival_y = (y.coordinate - receiver.position.y) / distance;
            const double arrival_z =
                std::clamp((z.coordinate - receiver.position.z) / distance, -1.0, 1.0);
            EarlyTap& tap = set.taps[set.count];
            if (to_microphone)
            {
                const double cosine =
                    std::clamp(arrival_x * forward_x + arrival_y * forward_y, -1.0, 1.0);
                fold(product, microphone.gain_for(std::acos(cosine), distance));
                tap.encode.fill(0.0f);
                tap.encode[0] = 1.0f;
            }
            else
            {
                AmbisonicEncoder::encode(std::atan2(arrival_y, arrival_x) - receiver.yaw_rad,
                                         std::asin(arrival_z), tap.encode);
            }

            fit_sections(tap, product, options.sample_rate);

            const double specular_share = x.specular_share * y.specular_share * z.specular_share;
            tap.delay_frames = static_cast<float>(candidate.delay_frames);
            tap.gain = static_cast<float>(std::sqrt(specular_share) / distance);
            scattered_energy +=
                mean_power(product) * (1.0 - specular_share) / (distance * distance);
            ++set.count;
        }

        set.scattered_energy = static_cast<float>(scattered_energy);
        return set;
    }
} // namespace SushiDSP::room
