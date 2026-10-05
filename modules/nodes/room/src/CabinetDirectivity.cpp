/****************************************************************/
/* CabinetDirectivity.cpp                                       */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file CabinetDirectivity.cpp
 * @brief The piston, array and rear-shelf factors, their octave-band energy mean, and the grid
 * the index integrates over.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/CabinetDirectivity.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <utility>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds pi.
        constexpr double kPi = 3.14159265358979323846;

        /// @brief Counts the icosahedron subdivisions the grid takes; 10 * 4^n + 2 vertices.
        constexpr int kSubdivisions = 4;

        /// @brief Counts the log-spaced frequencies each octave band's energy mean samples.
        constexpr int kOctavePoints = 16;

        /// @brief Names the baffle-step constant, in hertz times metres. Spec §7.
        constexpr double kBaffleStepHzMetres = 115.0;

        /// @brief Bounds the ascending Bessel series at this argument; above it the asymptotic
        /// form takes over.
        constexpr double kSeriesLimit = 12.0;

        /// @brief Counts the ascending series terms.
        constexpr int kSeriesTerms = 20;

        /// @brief Returns J1(x) from its ascending series.
        double bessel_j1_series(double x) noexcept
        {
            const double half = 0.5 * x;
            const double half_squared = half * half;
            double term = half;
            double sum = term;
            for (int index = 1; index < kSeriesTerms; ++index)
            {
                term *= -half_squared / (static_cast<double>(index) * static_cast<double>(index + 1));
                sum += term;
            }
            return sum;
        }

        /// @brief Returns J1(x) from its large-argument asymptotic form.
        double bessel_j1_asymptotic(double x) noexcept
        {
            return std::sqrt(2.0 / (kPi * x)) * std::cos(x - 0.75 * kPi);
        }
    }

    CabinetDirectivity::CabinetDirectivity(const CabinetGeometry& geometry)
        : geometry_(geometry)
    {
    }

    double CabinetDirectivity::piston_factor(double x) noexcept
    {
        const double magnitude = std::fabs(x);
        if (magnitude < 1e-12)
        {
            return 1.0;
        }
        const double j1 = magnitude < kSeriesLimit ? bessel_j1_series(magnitude)
                                                   : bessel_j1_asymptotic(magnitude);
        return 2.0 * j1 / magnitude;
    }

    double CabinetDirectivity::effective_radius(double frequency_hz) const noexcept
    {
        const double ratio = frequency_hz / geometry_.breakup_frequency_hz;
        return geometry_.cone_radius_m / std::sqrt(1.0 + ratio * ratio);
    }

    double CabinetDirectivity::rear_shelf(double frequency_hz) const noexcept
    {
        // The pole sits at the baffle step and the zero an octave above it, so the shelf reads
        // 0 dB far below the step and -6 dB far above it.
        const double step_hz = kBaffleStepHzMetres / geometry_.baffle_width_m;
        const double zero_ratio = frequency_hz / (2.0 * step_hz);
        const double pole_ratio = frequency_hz / step_hz;
        return std::sqrt((1.0 + zero_ratio * zero_ratio) / (1.0 + pole_ratio * pole_ratio));
    }

    double CabinetDirectivity::factor_at(double frequency_hz, const Direction& direction,
                                         double sine_theta) const noexcept
    {
        const double wavenumber = 2.0 * kPi * frequency_hz / kSpeedOfSound;
        const double piston =
            piston_factor(wavenumber * effective_radius(frequency_hz) * sine_theta);
        const double half_spacing = 0.5 * wavenumber * geometry_.driver_spacing_m;
        const double array = std::fabs(std::cos(half_spacing * direction.left))
            * std::fabs(std::cos(half_spacing * direction.up));
        const double rear = direction.forward < 0.0 ? rear_shelf(frequency_hz) : 1.0;
        return std::fabs(piston) * array * rear;
    }

    BandResponse CabinetDirectivity::gain_for(const Direction& direction) const noexcept
    {
        const double sine_theta = std::sqrt(direction.left * direction.left
                                            + direction.up * direction.up);
        BandResponse response{};
        for (std::size_t band = 0; band < kBandCount; ++band)
        {
            // The mean samples the midpoints of kOctavePoints equal log-frequency intervals
            // from the centre over root two to the centre times root two.
            double power_sum = 0.0;
            for (int point = 0; point < kOctavePoints; ++point)
            {
                const double octaves = (static_cast<double>(point) + 0.5) / kOctavePoints - 0.5;
                const double frequency = kBandCentresHz[band] * std::exp2(octaves);
                const double factor = factor_at(frequency, direction, sine_theta);
                power_sum += factor * factor;
            }
            response.gain[band] = static_cast<float>(std::sqrt(power_sum / kOctavePoints));
        }
        return response;
    }

    BandResponse CabinetDirectivity::gain_for(double azimuth_rad, double elevation_rad) const noexcept
    {
        const double cosine_elevation = std::cos(elevation_rad);
        const Direction direction{ cosine_elevation * std::cos(azimuth_rad),
                                   cosine_elevation * std::sin(azimuth_rad),
                                   std::sin(elevation_rad) };
        return gain_for(direction);
    }

    std::array<float, kBandCount> CabinetDirectivity::directivity_index_db() const
    {
        static const std::vector<Direction> grid = build_grid();
        std::array<double, kBandCount> power_sum{};
        for (const Direction& direction : grid)
        {
            const BandResponse gain = gain_for(direction);
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                const double value = static_cast<double>(gain.gain[band]);
                power_sum[band] += value * value;
            }
        }
        const BandResponse on_axis = gain_for(Direction{ 1.0, 0.0, 0.0 });
        std::array<float, kBandCount> index{};
        for (std::size_t band = 0; band < kBandCount; ++band)
        {
            const double axis = static_cast<double>(on_axis.gain[band]);
            const double mean = power_sum[band] / static_cast<double>(grid.size());
            index[band] = static_cast<float>(10.0 * std::log10(axis * axis / mean));
        }
        return index;
    }

    std::vector<CabinetDirectivity::Direction> CabinetDirectivity::build_grid()
    {
        using Triangle = std::array<std::uint32_t, 3>;
        const double golden = 0.5 * (1.0 + std::sqrt(5.0));
        const double norm = std::sqrt(1.0 + golden * golden);
        const double one = 1.0 / norm;
        const double phi = golden / norm;
        std::vector<Direction> vertices = {
            { -one, phi, 0.0 }, { one, phi, 0.0 }, { -one, -phi, 0.0 }, { one, -phi, 0.0 },
            { 0.0, -one, phi }, { 0.0, one, phi }, { 0.0, -one, -phi }, { 0.0, one, -phi },
            { phi, 0.0, -one }, { phi, 0.0, one }, { -phi, 0.0, -one }, { -phi, 0.0, one }
        };
        std::vector<Triangle> faces = {
            { 0, 11, 5 }, { 0, 5, 1 }, { 0, 1, 7 }, { 0, 7, 10 }, { 0, 10, 11 },
            { 1, 5, 9 }, { 5, 11, 4 }, { 11, 10, 2 }, { 10, 7, 6 }, { 7, 1, 8 },
            { 3, 9, 4 }, { 3, 4, 2 }, { 3, 2, 6 }, { 3, 6, 8 }, { 3, 8, 9 },
            { 4, 9, 5 }, { 2, 4, 11 }, { 6, 2, 10 }, { 8, 6, 7 }, { 9, 8, 1 }
        };
        for (int level = 0; level < kSubdivisions; ++level)
        {
            std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> midpoints;
            const auto midpoint = [&](std::uint32_t first, std::uint32_t second)
            {
                const auto key = std::minmax(first, second);
                const auto found = midpoints.find(key);
                if (found != midpoints.end())
                {
                    return found->second;
                }
                const Direction& a = vertices[first];
                const Direction& b = vertices[second];
                Direction sum{ a.forward + b.forward, a.left + b.left, a.up + b.up };
                const double length = std::sqrt(sum.forward * sum.forward + sum.left * sum.left
                                                + sum.up * sum.up);
                sum.forward /= length;
                sum.left /= length;
                sum.up /= length;
                const auto index = static_cast<std::uint32_t>(vertices.size());
                vertices.push_back(sum);
                midpoints.emplace(key, index);
                return index;
            };
            std::vector<Triangle> refined;
            refined.reserve(faces.size() * 4);
            for (const Triangle& face : faces)
            {
                const std::uint32_t ab = midpoint(face[0], face[1]);
                const std::uint32_t bc = midpoint(face[1], face[2]);
                const std::uint32_t ca = midpoint(face[2], face[0]);
                refined.push_back({ face[0], ab, ca });
                refined.push_back({ face[1], bc, ab });
                refined.push_back({ face[2], ca, bc });
                refined.push_back({ ab, bc, ca });
            }
            faces = std::move(refined);
        }
        return vertices;
    }
}
