/**************************************************************************/
/* test_band_response_fit.cpp                                             */
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
 * @file test_band_response_fit.cpp
 * @brief Tests for BandResponseFit, the two-biquad fit of six band gains.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "SushiDSP/room/BandResponseFit.hpp"
#include "SushiDSP/room/Materials.hpp"

namespace
{
    /// @brief Returns true when the section's poles lie inside the stability triangle.
    bool inside_stability_triangle(const std::array<double, 5>& section)
    {
        const double a1 = section[3];
        const double a2 = section[4];
        return std::fabs(a2) < 1.0 && std::fabs(a1) < 1.0 + a2;
    }
}

TEST(Unit_Dsp, BandResponseFitClearsOneDecibelForEveryMaterialToEveryOrder)
{
    for (std::uint8_t id = 0; id < SushiDSP::room::kMaterialCount; ++id)
    {
        const auto material_id = static_cast<SushiDSP::room::MaterialId>(id);
        if (material_id == SushiDSP::room::MaterialId::OpenDoorway)
        {
            continue;
        }
        for (const double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            for (const double order : { 1.0, 2.0, 3.0, 4.0 })
            {
                const SushiDSP::room::BandResponse target =
                    SushiDSP::room::reflection_factor(material_id).powered(order);
                const SushiDSP::room::BandResponseFit::Result result =
                    SushiDSP::room::BandResponseFit::fit(target, rate);
                EXPECT_LE(result.worst_error_db, 1.0)
                    << "material " << int(id) << " order " << order << " rate " << rate;
                for (std::size_t band = 0; band < 6u; ++band)
                {
                    const double wanted =
                        20.0 * std::log10(std::max(target.gain[band], 1e-9f));
                    const double got = SushiDSP::room::BandResponseFit::magnitude_db(
                        result.coefficients, SushiDSP::room::kBandCentresHz[band], rate);
                    EXPECT_NEAR(got, wanted, 1.0)
                        << "material " << int(id) << " order " << order << " rate " << rate
                        << " band " << band;
                }
            }
        }
    }
}

TEST(Unit_Dsp, BandResponseFitReproducesAFlatTargetExactly)
{
    const SushiDSP::room::BandResponseFit::Result result =
        SushiDSP::room::BandResponseFit::fit(SushiDSP::room::BandResponse::unity(), 48000.0);
    EXPECT_LT(result.worst_error_db, 0.01);
    EXPECT_NEAR(SushiDSP::room::BandResponseFit::magnitude_db(result.coefficients, 1000.0, 48000.0),
                0.0, 0.01);
}

TEST(Unit_Dsp, BandResponseFitKeepsEveryPoleInsideTheUnitCircle)
{
    for (std::uint8_t id = 0; id < SushiDSP::room::kMaterialCount; ++id)
    {
        const auto material_id = static_cast<SushiDSP::room::MaterialId>(id);
        if (material_id == SushiDSP::room::MaterialId::OpenDoorway)
        {
            continue;
        }
        for (const double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            for (const double order : { 1.0, 2.0, 3.0, 4.0 })
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

TEST(Unit_Dsp, BandResponseFitCostsUnderAMillisecondForOneHundredAndFiftyFits)
{
    const SushiDSP::room::BandResponse carpet =
        SushiDSP::room::reflection_factor(SushiDSP::room::MaterialId::HeavyCarpetOnConcrete);
    double sink = 0.0;
    const auto start = std::chrono::steady_clock::now();
    for (int index = 0; index < 150; ++index)
    {
        sink += SushiDSP::room::BandResponseFit::fit(carpet, 48000.0).worst_error_db;
    }
    const auto stop = std::chrono::steady_clock::now();
    const double milliseconds =
        std::chrono::duration<double, std::milli>(stop - start).count();
    EXPECT_LT(milliseconds, 1.0) << "sink " << sink;
}
