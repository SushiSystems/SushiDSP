/**************************************************************************/
/* test_band_response_fit.cpp                                             */
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
 * @file test_band_response_fit.cpp
 * @brief Tests for BandResponseFit, the two-biquad fit of six band gains.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/timing_gate/timing_gate.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>

#include <SushiDSP/nodes/room/air_absorption.hpp>
#include <SushiDSP/nodes/room/band_response_fit.hpp>
#include <SushiDSP/nodes/room/materials.hpp>

namespace
{
    /// @brief Returns true when the section's poles lie inside the stability triangle.
    bool inside_stability_triangle(const std::array<double, 5>& section)
    {
        const double a1 = section[3];
        const double a2 = section[4];
        return std::fabs(a2) < 1.0 && std::fabs(a1) < 1.0 + a2;
    }

    /// @brief Returns a section's gain at zero frequency.
    double dc_gain(const std::array<double, 5>& section)
    {
        return (section[0] + section[1] + section[2]) / (1.0 + section[3] + section[4]);
    }

    /// @brief Returns the largest absolute difference between two cascades' coefficients.
    double largest_coefficient_move(const SushiDSP::room::BandResponseFit::Coefficients& a,
                                    const SushiDSP::room::BandResponseFit::Coefficients& b)
    {
        double largest = 0.0;
        for (std::size_t index = 0; index < 5; ++index)
        {
            largest = std::max(largest, std::fabs(a.section_a[index] - b.section_a[index]));
            largest = std::max(largest, std::fabs(a.section_b[index] - b.section_b[index]));
        }
        return largest;
    }

    /// @brief Calls the visitor with every material but the doorway, raised to orders one to
    /// four, behind 0, 5 and 30 m of air, at four sample rates.
    void for_every_target(const std::function<void(const SushiDSP::room::BandResponse&, double,
                                                   const char*)>& visitor)
    {
        static char label[96];
        for (std::uint8_t id = 0; id < SushiDSP::room::K_MATERIAL_COUNT; ++id)
        {
            const auto material_id = static_cast<SushiDSP::room::MaterialId>(id);
            if (material_id == SushiDSP::room::MaterialId::OpenDoorway)
            {
                continue;
            }
            for (const double rate : {44100.0, 48000.0, 96000.0, 192000.0})
            {
                for (const double order : {1.0, 2.0, 3.0, 4.0})
                {
                    for (const double metres : {0.0, 5.0, 30.0})
                    {
                        SushiDSP::room::BandResponse target =
                            SushiDSP::room::reflection_factor(material_id).powered(order);
                        target *= SushiDSP::room::AirAbsorption::over_path(metres);
                        std::snprintf(label, sizeof label, "material %d order %g rate %g metres %g",
                                      int(id), order, rate, metres);
                        visitor(target, rate, label);
                    }
                }
            }
        }
    }

    /// @brief Returns the target with one band raised by the given decibels.
    SushiDSP::room::BandResponse nudged(const SushiDSP::room::BandResponse& target,
                                        std::size_t band, double decibels)
    {
        SushiDSP::room::BandResponse result = target;
        result.gain[band] *= static_cast<float>(std::pow(10.0, decibels / 20.0));
        return result;
    }
} // namespace

TEST(Small_BandResponseFit, BandResponseFitClearsOneDecibelForEveryMaterialToEveryOrder)
{
    for (std::uint8_t id = 0; id < SushiDSP::room::K_MATERIAL_COUNT; ++id)
    {
        const auto material_id = static_cast<SushiDSP::room::MaterialId>(id);
        if (material_id == SushiDSP::room::MaterialId::OpenDoorway)
        {
            continue;
        }
        for (const double rate : {44100.0, 48000.0, 96000.0, 192000.0})
        {
            for (const double order : {1.0, 2.0, 3.0, 4.0})
            {
                const SushiDSP::room::BandResponse target =
                    SushiDSP::room::reflection_factor(material_id).powered(order);
                const SushiDSP::room::BandResponseFit::Result result =
                    SushiDSP::room::BandResponseFit::fit(target, rate);
                EXPECT_LE(result.worst_error_db, 1.0)
                    << "material " << int(id) << " order " << order << " rate " << rate;
                for (std::size_t band = 0; band < 6u; ++band)
                {
                    const double wanted = 20.0 * std::log10(std::max(target.gain[band], 1e-9f));
                    const double got = SushiDSP::room::BandResponseFit::magnitude_db(
                        result.coefficients, SushiDSP::room::K_BAND_CENTRES_HZ[band], rate);
                    EXPECT_NEAR(got, wanted, 1.0) << "material " << int(id) << " order " << order
                                                  << " rate " << rate << " band " << band;
                }
            }
        }
    }
}

TEST(Small_BandResponseFit, BandResponseFitReproducesAFlatTargetExactly)
{
    const SushiDSP::room::BandResponseFit::Result result =
        SushiDSP::room::BandResponseFit::fit(SushiDSP::room::BandResponse::unity(), 48000.0);
    EXPECT_LT(result.worst_error_db, 0.01);
    EXPECT_NEAR(SushiDSP::room::BandResponseFit::magnitude_db(result.coefficients, 1000.0, 48000.0),
                0.0, 0.01);
}

TEST(Small_BandResponseFit, BandResponseFitKeepsEveryPoleInsideTheUnitCircle)
{
    for (std::uint8_t id = 0; id < SushiDSP::room::K_MATERIAL_COUNT; ++id)
    {
        const auto material_id = static_cast<SushiDSP::room::MaterialId>(id);
        if (material_id == SushiDSP::room::MaterialId::OpenDoorway)
        {
            continue;
        }
        for (const double rate : {44100.0, 48000.0, 96000.0, 192000.0})
        {
            for (const double order : {1.0, 2.0, 3.0, 4.0})
            {
                const SushiDSP::room::BandResponse target =
                    SushiDSP::room::reflection_factor(material_id).powered(order);
                const SushiDSP::room::BandResponseFit::Result result =
                    SushiDSP::room::BandResponseFit::fit(target, rate);
                EXPECT_TRUE(inside_stability_triangle(result.coefficients.section_a))
                    << "material " << int(id) << " order " << order << " rate " << rate;
                EXPECT_TRUE(inside_stability_triangle(result.coefficients.section_b))
                    << "material " << int(id) << " order " << order << " rate " << rate;
            }
        }
    }
}

TEST(Small_BandResponseFit, BandResponseFitPutsTheWholeGainInTheFirstSection)
{
    for_every_target(
        [](const SushiDSP::room::BandResponse& target, double rate, const char* label)
        {
            const SushiDSP::room::BandResponseFit::Result result =
                SushiDSP::room::BandResponseFit::fit(target, rate);
            EXPECT_NEAR(dc_gain(result.coefficients.section_b), 1.0, 1e-9) << label;
        });
}

TEST(Small_BandResponseFit, BandResponseFitReachesADipBetweenFlatBands)
{
    // Each section has a peak, since two shelves cannot dip. See tests/nodes/room/README.md.
    const std::array<std::array<float, 6>, 3> targets = {{
        {0.0f, 0.0f, 0.0f, -8.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f, -20.0f, 0.0f},
        {-0.05f, -0.19f, -0.76f, -3.11f, -11.50f, -3.49f},
    }};
    for (const std::array<float, 6>& decibels : targets)
    {
        const SushiDSP::room::BandResponse target =
            SushiDSP::room::BandResponse::from_decibels(decibels);
        const SushiDSP::room::BandResponseFit::Result result =
            SushiDSP::room::BandResponseFit::fit(target, 48000.0);
        EXPECT_LE(result.worst_error_db, 0.5) << "target at 2 kHz " << decibels[4];
        for (std::size_t band = 0; band < 6u; ++band)
        {
            EXPECT_NEAR(SushiDSP::room::BandResponseFit::magnitude_db(
                            result.coefficients, SushiDSP::room::K_BAND_CENTRES_HZ[band], 48000.0),
                        decibels[band], 0.5)
                << "target at 2 kHz " << decibels[4] << " band " << band;
        }
        EXPECT_TRUE(inside_stability_triangle(result.coefficients.section_a));
        EXPECT_TRUE(inside_stability_triangle(result.coefficients.section_b));
    }
}

TEST(Small_BandResponseFit, BandResponseFitMovesCoefficientsContinuouslyWithTheTarget)
{
    // The 0.05 dB bound is the node's carry threshold; the worst measured is 0.128, at 96 kHz.
    for_every_target(
        [](const SushiDSP::room::BandResponse& target, double rate, const char* label)
        {
            const SushiDSP::room::BandResponseFit::Result base =
                SushiDSP::room::BandResponseFit::fit(target, rate);
            for (std::size_t band = 0; band < SushiDSP::room::K_BAND_COUNT; ++band)
            {
                const SushiDSP::room::BandResponseFit::Result small =
                    SushiDSP::room::BandResponseFit::fit(nudged(target, band, 0.05), rate);
                EXPECT_LT(largest_coefficient_move(base.coefficients, small.coefficients), 0.2)
                    << label << " band " << band << " nudged 0.05 dB";
                const SushiDSP::room::BandResponseFit::Result large =
                    SushiDSP::room::BandResponseFit::fit(nudged(target, band, 0.5), rate);
                EXPECT_LT(largest_coefficient_move(base.coefficients, large.coefficients), 1.0)
                    << label << " band " << band << " nudged 0.5 dB";
            }
        });
}

TEST(Small_BandResponseFit, BandResponseFitCostsUnderTenMillisecondsForOneHundredAndFiftyFits)
{
    SUSHIDSP_REQUIRE_OPTIMISED_BUILD();
    const SushiDSP::room::BandResponse carpet =
        SushiDSP::room::reflection_factor(SushiDSP::room::MaterialId::HeavyCarpetOnConcrete);
    double sink = 0.0;
    const auto start = std::chrono::steady_clock::now();
    for (int index = 0; index < 150; ++index)
    {
        sink += SushiDSP::room::BandResponseFit::fit(carpet, 48000.0).worst_error_db;
    }
    const auto stop = std::chrono::steady_clock::now();
    const double milliseconds = std::chrono::duration<double, std::milli>(stop - start).count();
    EXPECT_LT(milliseconds, 10.0) << "sink " << sink;
}
