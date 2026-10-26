/**************************************************************************/
/* session_bounce.cpp                                                     */
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
 *  @file session_bounce.cpp
 *  @brief Implements the export's offline render and its two writers.
 *  @author Mustafa Garip & Sushi Systems
 */

#include "session_bounce.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <memory>
#include <utility>
#include <vector>

#include <SushiDSP/foundation/io/wav_writer.hpp>
#include <SushiDSP/foundation/log/log.hpp>
#include "host_gui_log_category.hpp"
#if defined(SUSHIDSP_HAVE_LAME)
#include "mp3_encoder.hpp"
#endif
#include "session.hpp"
#include "session_file.hpp"
#include "session_renderer.hpp"

namespace
{
    /// @brief Returns the timeline frame the last clip of a session ends at.
    std::uint64_t last_clip_end(const Session& session)
    {
        const SessionTimeline& timeline = session.timeline();
        std::uint64_t end = 0;
        for (std::size_t track = 0; track < session.track_count(); ++track)
        {
            for (std::size_t clip = 0; clip < timeline.clip_count(track); ++clip)
            {
                const SessionTimeline::ClipPlacement placement = timeline.clip(track, clip);
                end = std::max(end, placement.start_frame + placement.length_frames);
            }
        }
        return end;
    }

    /// @brief Returns the largest magnitude of a block's two channels.
    float block_peak(const float* left, const float* right, std::uint32_t frames) noexcept
    {
        float peak = 0.0f;
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            peak = std::max(peak, std::max(std::fabs(left[frame]), std::fabs(right[frame])));
        }
        return peak;
    }
} // namespace

SessionBounce::~SessionBounce()
{
    if (worker_.joinable())
    {
        worker_.join();
    }
}

bool SessionBounce::start(BounceRequest request)
{
    if (running_.load(std::memory_order_acquire))
    {
        return false;
    }
    if (worker_.joinable())
    {
        worker_.join();
    }
    progress_.store(0.0f, std::memory_order_release);
    finished_.store(false, std::memory_order_release);
    running_.store(true, std::memory_order_release);
    worker_ = std::thread(
        [this, request = std::move(request)]
        {
            error_ = render(request, &progress_);
            finished_.store(true, std::memory_order_release);
            running_.store(false, std::memory_order_release);
        });
    return true;
}

bool SessionBounce::running() const noexcept
{
    return running_.load(std::memory_order_acquire);
}

float SessionBounce::progress() const noexcept
{
    return progress_.load(std::memory_order_acquire);
}

bool SessionBounce::take_result(std::string& error_out)
{
    if (!finished_.exchange(false, std::memory_order_acq_rel))
    {
        return false;
    }
    error_out = error_;
    return true;
}

std::string SessionBounce::render(const BounceRequest& request, std::atomic<float>* progress)
{
    try
    {
        Session session(request.info);
        if (!request.hrtf_path.empty())
        {
            std::string ignored;
            session.set_hrtf_path(request.hrtf_path, ignored);
        }
        session_from_json(session, request.session_json);

        SessionTimeline& timeline = session.timeline();
        timeline.transport().clear_loop();
        timeline.transport().set_speed(1.0);
        timeline.metronome().set_enabled(false);
        timeline.transport().seek(0);
        timeline.transport().play();

        const std::uint32_t block = request.info.max_block;
        const std::uint64_t body = last_clip_end(session);
        if (body == 0)
        {
            return "The session holds no clip to export.";
        }
        const auto silent_blocks = static_cast<std::uint64_t>(
            SessionBounce::K_SILENT_SECONDS * request.info.sample_rate / block) + 1u;
        const auto tail_limit = static_cast<std::uint64_t>(
            SessionBounce::K_MAX_TAIL_SECONDS * request.info.sample_rate);

        std::unique_ptr<SushiDSP::WavWriter> wav;
#if defined(SUSHIDSP_HAVE_LAME)
        std::unique_ptr<SushiDSP::host::Mp3Encoder> mp3;
        if (request.format == BounceFormat::Mp3)
        {
            mp3 = std::make_unique<SushiDSP::host::Mp3Encoder>(
                request.path, request.info.sample_rate, request.mp3_kilobits_per_second);
        }
        else
#else
        // A build without LAME writes no MP3; the request is refused before anything renders.
        if (request.format == BounceFormat::Mp3)
        {
            return "This build has no MP3 encoder.";
        }
#endif
        {
            wav = std::make_unique<SushiDSP::WavWriter>(
                request.path, request.info.sample_rate, SessionRenderer::K_CHANNELS,
                request.format == BounceFormat::Wav16 ? SushiDSP::WavWriter::Format::Pcm16
                                                      : SushiDSP::WavWriter::Format::Float32);
        }

        SessionRenderer renderer;
        renderer.prepare(block);
        std::vector<float> left(block, 0.0f);
        std::vector<float> right(block, 0.0f);
        float* const outputs[SessionRenderer::K_CHANNELS] = {left.data(), right.data()};
        const float* const written[SessionRenderer::K_CHANNELS] = {left.data(), right.data()};

        std::uint64_t rendered = 0;
        std::uint64_t quiet = 0;
        while (true)
        {
            renderer.render(session, nullptr, outputs, block);
            session.reclaim();
            // A float sample past full scale would wrap in a 16-bit file and clip in an MP3.
            for (std::uint32_t frame = 0; frame < block; ++frame)
            {
                left[frame] = std::clamp(left[frame], -1.0f, 1.0f);
                right[frame] = std::clamp(right[frame], -1.0f, 1.0f);
            }
#if defined(SUSHIDSP_HAVE_LAME)
            if (mp3 != nullptr)
            {
                mp3->append(left.data(), right.data(), block);
            }
#endif
            if (wav != nullptr)
            {
                wav->append(written, block);
            }
            rendered += block;
            if (progress != nullptr)
            {
                progress->store(std::min(1.0f, static_cast<float>(rendered) /
                                                   static_cast<float>(body + block)),
                                std::memory_order_release);
            }
            if (rendered < body)
            {
                continue;
            }
            quiet = block_peak(left.data(), right.data(), block) < SessionBounce::K_SILENCE
                        ? quiet + 1u
                        : 0u;
            if (quiet >= silent_blocks || rendered >= body + tail_limit)
            {
                break;
            }
        }

#if defined(SUSHIDSP_HAVE_LAME)
        if (mp3 != nullptr)
        {
            mp3->close();
        }
#endif
        if (wav != nullptr)
        {
            wav->close();
        }
        if (progress != nullptr)
        {
            progress->store(1.0f, std::memory_order_release);
        }
        return std::string();
    }
    catch (const std::exception& failure)
    {
        SD_LOG_ERROR(K_HOST_GUI_LOG_CATEGORY, "the bounce failed: {}; the file is incomplete",
                     failure.what());
        return failure.what();
    }
}
