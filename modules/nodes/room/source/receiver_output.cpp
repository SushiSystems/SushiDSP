/**************************************************************************/
/* receiver_output.cpp                                                    */
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
 * @file receiver_output.cpp
 * @brief Implements the head holder, the ears' output stage and the microphone's.
 * @author Mustafa Garip & Sushi Systems
 */

#include <SushiDSP/nodes/room/receiver_output.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>

#include <SushiDSP/nodes/room/spherical_head_hrtf.hpp>

namespace SushiDSP::Room
{
    const room::HrtfSet& HeadHolder::hrtf() const noexcept
    {
        return hrtf_;
    }

    void HeadHolder::set_hrtf(room::HrtfSet set)
    {
        hrtf_ = std::move(set);
    }

    room::HrtfSet HeadHolder::take_hrtf() noexcept
    {
        return std::move(hrtf_);
    }

    room::HrtfSource HeadHolder::source() const noexcept
    {
        return source_;
    }

    void HeadHolder::set_source(room::HrtfSource source) noexcept
    {
        source_ = source;
    }

    void HeadHolder::refresh_head(double sample_rate)
    {
        // A measured set at another rate cannot be resampled here; the fallback takes over.
        if (source_ == room::HrtfSource::Measured && hrtf_.sample_rate() != sample_rate)
        {
            source_ = room::HrtfSource::SphericalHead;
        }
        if (source_ == room::HrtfSource::SphericalHead)
        {
            hrtf_ = room::SphericalHeadHrtf::build(sample_rate);
        }
    }

    void HeadHolder::publish_decoder(double sample_rate, std::uint32_t max_block_frames)
    {
        auto decoder = std::make_unique<room::BinauralDecoder>();
        decoder->prepare(hrtf_, sample_rate, max_block_frames);
        decoder_slot_.publish(std::move(decoder));
    }

    void HeadHolder::publish_decoder(std::unique_ptr<room::BinauralDecoder> decoder)
    {
        decoder_slot_.publish(std::move(decoder));
    }

    room::BinauralDecoder* HeadHolder::acquire_decoder() noexcept
    {
        return decoder_slot_.acquire_for_audio_thread();
    }

    void HeadHolder::reset_decoder() noexcept
    {
        if (room::BinauralDecoder* const decoder = decoder_slot_.active_for_publisher())
        {
            decoder->reset();
        }
    }

    void HeadHolder::reclaim()
    {
        decoder_slot_.reclaim();
    }

    void EarsOutput::prepare(std::uint32_t alignment_frames, std::uint32_t max_block_frames)
    {
        max_block_ = max_block_frames;
        channels_.assign(static_cast<std::size_t>(K_CHANNELS) * max_block_frames, 0.0f);
        for (std::uint32_t channel = 0; channel < K_CHANNELS; ++channel)
        {
            channel_pointers_[channel] =
                channels_.data() + static_cast<std::size_t>(channel) * max_block_frames;
        }
        for (std::uint32_t ear = 0; ear < room::K_EARS; ++ear)
        {
            direct_[ear].assign(max_block_frames, 0.0f);
            tail_[ear].assign(max_block_frames, 0.0f);
            tail_ears_[ear].assign(max_block_frames, 0.0f);
            modal_[ear].assign(max_block_frames, 0.0f);
            decoded_[ear].assign(max_block_frames, 0.0f);
            diffuse_[ear].prepare(max_block_frames);
            tail_delay_[ear].prepare(alignment_frames, max_block_frames);
            modal_delay_[ear].prepare(alignment_frames, max_block_frames);
        }
        reset();
    }

    void EarsOutput::reset() noexcept
    {
        for (std::uint32_t ear = 0; ear < room::K_EARS; ++ear)
        {
            diffuse_[ear].reset();
            tail_delay_[ear].reset();
            modal_delay_[ear].reset();
        }
        decoder_fed_ = false;
        pending_waiting_ = false;
        added_ = 0;
    }

    void EarsOutput::stage_diffuse(const float* left, const float* right) noexcept
    {
        std::copy_n(left, room::FirFilter::K_TAPS, pending_[0].begin());
        std::copy_n(right, room::FirFilter::K_TAPS, pending_[1].begin());
        pending_waiting_ = true;
    }

    void EarsOutput::apply_pending_when_due(const room::FadeWindow& window) noexcept
    {
        if (!pending_waiting_ || window.done < window.lag)
        {
            return;
        }
        // The direct pair, the decoder and this diffuse pair come from one head.
        diffuse_[0].load(pending_[0].data());
        diffuse_[1].load(pending_[1].data());
        pending_waiting_ = false;
    }

    void EarsOutput::begin() noexcept
    {
        added_ = 0;
    }

    void EarsOutput::accumulate(float* sum, const float* block,
                                std::uint32_t frames) const noexcept
    {
        float* const __restrict into = sum;
        const float* const __restrict from = block;
        if (added_ == 0)
        {
            std::copy(from, from + frames, into);
            return;
        }
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            into[frame] += from[frame];
        }
    }

    void EarsOutput::add(const PairStage& pair, const float* modal_left, const float* modal_right,
                         std::uint32_t frames) noexcept
    {
        const float* const* const channels = pair.ambisonic();
        for (std::uint32_t channel = 0; channel < K_CHANNELS; ++channel)
        {
            accumulate(channel_pointers_[channel], channels[channel], frames);
        }
        const std::array<const float*, room::K_EARS> modal = {modal_left, modal_right};
        for (std::uint32_t ear = 0; ear < room::K_EARS; ++ear)
        {
            accumulate(direct_[ear].data(), pair.direct(ear), frames);
            accumulate(tail_[ear].data(), pair.tail(ear), frames);
            accumulate(modal_[ear].data(), modal[ear], frames);
        }
        ++added_;
    }

    void EarsOutput::finish(room::BinauralDecoder* decoder, float* left, float* right,
                            std::uint32_t frames) noexcept
    {
        if (added_ == 0)
        {
            for (std::uint32_t ear = 0; ear < room::K_EARS; ++ear)
            {
                std::fill_n(direct_[ear].data(), frames, 0.0f);
                std::fill_n(tail_[ear].data(), frames, 0.0f);
                std::fill_n(modal_[ear].data(), frames, 0.0f);
            }
        }
        if (added_ == 0 || decoder == nullptr || !decoder->ready())
        {
            std::fill_n(decoded_[0].data(), frames, 0.0f);
            std::fill_n(decoded_[1].data(), frames, 0.0f);
            decoder_fed_ = false;
        }
        else
        {
            // A decoder that sat idle holds spectra of blocks long gone.
            if (!decoder_fed_)
            {
                decoder->reset();
            }
            decoder->process(channel_pointers_.data(), decoded_[0].data(), decoded_[1].data(),
                             frames);
            decoder_fed_ = true;
        }

        const std::array<float*, room::K_EARS> outputs = {left, right};
        for (std::uint32_t ear = 0; ear < room::K_EARS; ++ear)
        {
            float* const tail = tail_ears_[ear].data();
            diffuse_[ear].process(tail_[ear].data(), tail, frames);
            tail_delay_[ear].process(tail, tail, frames);
            modal_delay_[ear].process(modal_[ear].data(), modal_[ear].data(), frames);

            float* const __restrict out = outputs[ear];
            const float* const __restrict decoded_ear = decoded_[ear].data();
            const float* const __restrict direct_ear = direct_[ear].data();
            const float* const __restrict tail_ear = tail;
            const float* const __restrict modal_ear = modal_[ear].data();
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                out[frame] =
                    decoded_ear[frame] + direct_ear[frame] + tail_ear[frame] + modal_ear[frame];
            }
        }
    }

    void MicrophoneOutput::prepare(std::uint32_t alignment_frames, std::uint32_t max_block_frames)
    {
        sum_.assign(max_block_frames, 0.0f);
        delay_.prepare(alignment_frames, max_block_frames);
        reset();
    }

    void MicrophoneOutput::reset() noexcept
    {
        delay_.reset();
        added_ = 0;
    }

    void MicrophoneOutput::begin() noexcept
    {
        added_ = 0;
    }

    void MicrophoneOutput::add(const PairStage& pair, const float* modal,
                               std::uint32_t frames) noexcept
    {
        float* const __restrict sum = sum_.data();
        const float* const __restrict mono = pair.early_mono();
        const float* const __restrict tail = pair.tail(0);
        const float* const __restrict field = modal;
        if (added_ == 0)
        {
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                sum[frame] = mono[frame] + tail[frame] + field[frame];
            }
        }
        else
        {
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                sum[frame] += mono[frame] + tail[frame] + field[frame];
            }
        }
        ++added_;
    }

    void MicrophoneOutput::finish(float* output, std::uint32_t frames) noexcept
    {
        if (added_ == 0)
        {
            std::fill_n(sum_.data(), frames, 0.0f);
        }
        delay_.process(sum_.data(), output, frames);
    }
} // namespace SushiDSP::Room
