/**************************************************************************/
/* session_renderer.cpp                                                   */
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
 *  @file session_renderer.cpp
 *  @brief Implements the block: lanes and live input through each track's rig into the master.
 *  @author Mustafa Garip & Sushi Systems
 */

#include "session_renderer.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "SushiDSP/capture/transport.hpp"
#include "SushiDSP/core/rig_graph.hpp"

void SessionRenderer::prepare(std::uint32_t max_block_frames)
{
    max_block_ = max_block_frames;
    input_.assign(static_cast<std::size_t>(K_CHANNELS) * max_block_frames, 0.0f);
    rig_out_.assign(static_cast<std::size_t>(K_CHANNELS) * max_block_frames, 0.0f);
    dry_.assign(max_block_frames, 0.0f);
}

float* SessionRenderer::channel(std::vector<float>& bus, std::uint32_t index) noexcept
{
    return bus.data() + static_cast<std::size_t>(index) * max_block_;
}

void SessionRenderer::render(Session& session, const float* const* live, float* const* outputs,
                             std::uint32_t frames) noexcept
{
    frames = std::min(frames, max_block_);
    const std::size_t bytes = static_cast<std::size_t>(frames) * sizeof(float);
    for (std::uint32_t index = 0; index < K_CHANNELS; ++index)
    {
        std::memset(outputs[index], 0, bytes);
    }

    Session::AudioTable& table = session.audio();
    SessionTimeline& timeline = session.timeline();
    SushiDSP::Transport& transport = timeline.transport();
    const bool rolling = transport.state() != SushiDSP::TransportState::Stopped;
    const std::uint32_t selected = table.selected.load(std::memory_order_acquire);

    float* const input[K_CHANNELS] = {channel(input_, 0), channel(input_, 1)};
    const float* const input_const[K_CHANNELS] = {input[0], input[1]};
    float* const rig_out[K_CHANNELS] = {channel(rig_out_, 0), channel(rig_out_, 1)};

    for (std::uint32_t index = 0; index < Session::K_MAX_TRACKS; ++index)
    {
        Session::AudioTrack& track = table.tracks[index];
        RigController* const rig = track.rig.load(std::memory_order_acquire);
        const bool monitored = index == selected && live != nullptr;
        // A track nobody plays into and no transport drives has nothing to render.
        if (rig == nullptr || (!rolling && !monitored))
        {
            continue;
        }

        for (std::uint32_t side = 0; side < K_CHANNELS; ++side)
        {
            if (monitored)
            {
                std::memcpy(input[side], live[side], bytes);
            }
            else
            {
                std::memset(input[side], 0, bytes);
            }
        }
        if (rolling)
        {
            timeline.player().process_lane(index, input, K_CHANNELS, frames, transport);
        }

        SushiDSP::RigGraph* const graph = rig->graph_slot().acquire_for_audio_thread();
        const float* const* heard = input_const;
        const bool through_rig = !track.rig_bypassed.load(std::memory_order_acquire) &&
                                 graph != nullptr && !graph->is_empty();
        if (through_rig)
        {
            SushiDSP::ProcessContext context{rig_out, input_const, frames, 0.0};
            graph->process(context);
            // A rig prepared for one channel writes one; it is heard on both sides.
            if (session.info().channels < K_CHANNELS)
            {
                std::memcpy(rig_out[1], rig_out[0], bytes);
            }
            heard = rig_out;
        }

        const float gains[K_CHANNELS] = {track.gain_left.load(std::memory_order_acquire),
                                         track.gain_right.load(std::memory_order_acquire)};
        for (std::uint32_t side = 0; side < K_CHANNELS; ++side)
        {
            const float gain = gains[side];
            const float* const __restrict from = heard[side];
            float* const __restrict to = outputs[side];
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                to[frame] += gain * from[frame];
            }
        }
    }

    SushiDSP::RecordingBus& bus = timeline.recording_bus();
    if (bus.is_recording())
    {
        // The dry leg is what was played, so the armed track's rig renders it again on playback.
        if (live != nullptr)
        {
            std::memcpy(dry_.data(), live[0], bytes);
        }
        else
        {
            std::memset(dry_.data(), 0, bytes);
        }
        const float* const master[K_CHANNELS] = {outputs[0], outputs[1]};
        bus.write(master, dry_.data(), frames);
    }

    // The click is added after both recorder legs; see applications/host_gui/README.md.
    timeline.metronome().process(outputs, K_CHANNELS, frames, transport, timeline.musical_clock());
    if (rolling)
    {
        transport.advance(frames);
    }
    table.blocks.fetch_add(1, std::memory_order_acq_rel);
}
