/**************************************************************************/
/* benchmark_binaural_decoder.cpp                                         */
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
 * @file benchmark_binaural_decoder.cpp
 * @brief Times BinauralDecoder::process alone.
 * @author Mustafa Garip
 */

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>
#include <SushiDSP/foundation/benchmark_harness/reference_tone.hpp>
#include <SushiDSP/nodes/room/binaural_decoder.hpp>
#include <SushiDSP/nodes/room/hrtf_set.hpp>
#include <SushiDSP/nodes/room/spherical_head_hrtf.hpp>

namespace
{
    using SushiDSP::bench::K_BLOCK_SIZE;
    using SushiDSP::bench::K_SAMPLE_RATE;

    /// @brief Times one block of the decoder; its cost is fixed by the decoder's order and does
    /// not depend on the scene (`binaural_decoder.hpp`). Plan Task 29 step 4.
    SushiDSP::bench::Result measure_binaural_decoder()
    {
        const SushiDSP::room::HrtfSet hrtf =
            SushiDSP::room::SphericalHeadHrtf::build(K_SAMPLE_RATE);
        SushiDSP::room::BinauralDecoder decoder;
        decoder.prepare(hrtf, K_SAMPLE_RATE, K_BLOCK_SIZE);

        const std::vector<float> tone = SushiDSP::bench::reference_tone(K_BLOCK_SIZE);
        std::vector<float> channels(SushiDSP::room::BinauralDecoder::K_CHANNELS * K_BLOCK_SIZE,
                                    0.0f);
        std::array<const float*, SushiDSP::room::BinauralDecoder::K_CHANNELS> channel_pointers{};
        for (std::uint32_t channel = 0; channel < SushiDSP::room::BinauralDecoder::K_CHANNELS;
             ++channel)
        {
            float* row = channels.data() + channel * K_BLOCK_SIZE;
            std::memcpy(row, tone.data(), K_BLOCK_SIZE * sizeof(float));
            channel_pointers[channel] = row;
        }
        std::vector<float> left(K_BLOCK_SIZE, 0.0f);
        std::vector<float> right(K_BLOCK_SIZE, 0.0f);
        return SushiDSP::bench::measure("BinauralDecoder::process", K_BLOCK_SIZE, K_SAMPLE_RATE,
                                        [&]
                                        {
                                            decoder.process(channel_pointers.data(), left.data(),
                                                            right.data(), K_BLOCK_SIZE);
                                        });
    }

    const SushiDSP::bench::BenchmarkRegistration K_BINAURAL_DECODER_CASE{
        "RoomBinauralDecoder", [] { SushiDSP::bench::print(measure_binaural_decoder()); }};
} // namespace
