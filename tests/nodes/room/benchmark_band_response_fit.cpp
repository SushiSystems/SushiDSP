/**************************************************************************/
/* benchmark_band_response_fit.cpp                                        */
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
 * @file benchmark_band_response_fit.cpp
 * @brief Times one hundred and fifty BandResponseFit::fit calls, the cost of one rebuilt room.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>
#include <SushiDSP/nodes/room/band_response_fit.hpp>
#include <SushiDSP/nodes/room/materials.hpp>

namespace
{
    /// @brief Holds how many fits one repetition runs.
    constexpr int K_FITS = 150;

    /// @brief Times K_FITS fits of the heavy carpet at 48 kHz.
    SushiDSP::bench::Result measure_band_response_fits()
    {
        const SushiDSP::room::BandResponse carpet =
            SushiDSP::room::reflection_factor(SushiDSP::room::MaterialId::HeavyCarpetOnConcrete);
        double sink = 0.0;
        const SushiDSP::bench::Result result = SushiDSP::bench::measure(
            "BandResponseFit x150", SushiDSP::bench::K_BLOCK_SIZE, SushiDSP::bench::K_SAMPLE_RATE,
            [&]
            {
                for (int index = 0; index < K_FITS; ++index)
                {
                    sink += SushiDSP::room::BandResponseFit::fit(carpet, 48000.0).worst_error_db;
                }
            });
        static_cast<void>(sink);
        return result;
    }

    const SushiDSP::bench::BenchmarkRegistration K_BAND_RESPONSE_FIT_CASE{
        "RoomBandResponseFit",
        []
        {
            SushiDSP::bench::report(measure_band_response_fits(), 10.0,
                                    "nodes/room/band_response_fit.checks.json");
        }};
} // namespace
