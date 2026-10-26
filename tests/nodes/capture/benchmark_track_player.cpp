/**************************************************************************/
/* benchmark_track_player.cpp                                             */
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
 * @file benchmark_track_player.cpp
 * @brief Times eight stereo tracks mixed into one stereo block.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>
#include <SushiDSP/nodes/capture/take.hpp>
#include <SushiDSP/nodes/capture/track_player.hpp>
#include <SushiDSP/nodes/capture/transport.hpp>

namespace
{
    using SushiDSP::bench::K_BLOCK_SIZE;
    using SushiDSP::bench::K_SAMPLE_RATE;

    /// @brief Times eight stereo tracks mixed into one stereo block, the load spec section 10 puts
    /// on the audio thread when a whole session plays back.
    SushiDSP::bench::Result measure_eight_track_mix()
    {
        constexpr std::size_t K_TRACKS = SushiDSP::TrackPlayer::K_MAX_TRACKS;
        constexpr std::size_t K_TAKE_FRAMES = 48000;
        constexpr double K_TWO_PI = 6.283185307179586476925;

        static SushiDSP::TrackPlayer player;
        static SushiDSP::Transport transport;
        for (std::size_t track = 0; track < K_TRACKS; ++track)
        {
            auto take = std::make_unique<SushiDSP::Take>(2u, K_SAMPLE_RATE, K_TAKE_FRAMES);
            for (std::uint32_t channel = 0; channel < 2u; ++channel)
            {
                float* samples = take->mutable_channel(channel);
                for (std::size_t frame = 0; frame < K_TAKE_FRAMES; ++frame)
                {
                    samples[frame] = 0.1f * static_cast<float>(std::sin(
                                                K_TWO_PI * 220.0 * static_cast<double>(frame) /
                                                K_SAMPLE_RATE));
                }
            }
            player.set_take(track, std::move(take));
            player.set_pan(track, -1.0f + 0.25f * static_cast<float>(track));
        }

        static std::vector<float> left(K_BLOCK_SIZE, 0.0f);
        static std::vector<float> right(K_BLOCK_SIZE, 0.0f);
        float* outputs[2] = {left.data(), right.data()};

        return SushiDSP::bench::measure(
            "TrackPlayer (8 stereo tracks)", K_BLOCK_SIZE, K_SAMPLE_RATE,
            [&]
            {
                std::fill(left.begin(), left.end(), 0.0f);
                std::fill(right.begin(), right.end(), 0.0f);
                player.process(outputs, 2u, K_BLOCK_SIZE, transport);
                transport.advance(K_BLOCK_SIZE);
                if (transport.position_frames() + K_BLOCK_SIZE >= K_TAKE_FRAMES)
                {
                    transport.seek(0);
                }
            });
    }

    const SushiDSP::bench::BenchmarkRegistration K_TRACK_PLAYER_CASE{
        "TrackPlayer",
        []
        {
            // 256 frames at 48 kHz leave the audio thread 5.33 ms for everything in the block.
            SushiDSP::bench::report(measure_eight_track_mix(), 5.33,
                                    "nodes/capture/track_player.checks.json");
        }};
} // namespace
