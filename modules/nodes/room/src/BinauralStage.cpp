/****************************************************************/
/* BinauralStage.cpp                                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file BinauralStage.cpp
 * @brief The decode, the alignment and the sum each ear's block comes out of.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/BinauralStage.hpp"

#include <algorithm>

#include "SushiDSP/room/SphericalHeadHrtf.hpp"

namespace SushiDSP::room
{
    void BinauralStage::prepare(std::uint32_t alignment_frames, std::uint32_t max_block_frames)
    {
        for (std::uint32_t ear = 0; ear < kEars; ++ear)
        {
            diffuse_[ear].prepare(max_block_frames);
            tail_delay_[ear].prepare(alignment_frames, max_block_frames);
            modal_delay_[ear].prepare(alignment_frames, max_block_frames);
            tail_ears_[ear].assign(max_block_frames, 0.0f);
            modal_ears_[ear].assign(max_block_frames, 0.0f);
        }
        decoded_left_.assign(max_block_frames, 0.0f);
        decoded_right_.assign(max_block_frames, 0.0f);
        ear_.assign(static_cast<std::size_t>(kEars) * max_block_frames, 0.0f);
        for (std::uint32_t ear = 0; ear < kEars; ++ear)
        {
            ear_pointers_[ear] = ear_.data() + static_cast<std::size_t>(ear) * max_block_frames;
        }
    }

    void BinauralStage::reset() noexcept
    {
        for (std::uint32_t ear = 0; ear < kEars; ++ear)
        {
            diffuse_[ear].reset();
            tail_delay_[ear].reset();
            modal_delay_[ear].reset();
        }
        if (BinauralDecoder* const decoder = decoder_slot_.active_for_publisher())
        {
            decoder->reset();
        }
        decoder_fed_ = false;
    }

    const HrtfSet& BinauralStage::hrtf() const noexcept
    {
        return hrtf_;
    }

    void BinauralStage::set_hrtf(HrtfSet set)
    {
        hrtf_ = std::move(set);
    }

    HrtfSet BinauralStage::take_hrtf() noexcept
    {
        return std::move(hrtf_);
    }

    HrtfSource BinauralStage::source() const noexcept
    {
        return hrtf_source_;
    }

    void BinauralStage::set_source(HrtfSource source) noexcept
    {
        hrtf_source_ = source;
    }

    void BinauralStage::refresh_head(double sample_rate)
    {
        // A measured set at another rate cannot be resampled here; the fallback takes over.
        if (hrtf_source_ == HrtfSource::Measured && hrtf_.sample_rate() != sample_rate)
        {
            hrtf_source_ = HrtfSource::SphericalHead;
        }
        if (hrtf_source_ == HrtfSource::SphericalHead)
        {
            hrtf_ = SphericalHeadHrtf::build(sample_rate);
        }
    }

    void BinauralStage::publish_decoder(double sample_rate, std::uint32_t max_block_frames)
    {
        auto decoder = std::make_unique<BinauralDecoder>();
        decoder->prepare(hrtf_, sample_rate, max_block_frames);
        decoder_slot_.publish(std::move(decoder));
    }

    void BinauralStage::publish_decoder(std::unique_ptr<BinauralDecoder> decoder)
    {
        decoder_slot_.publish(std::move(decoder));
    }

    BinauralDecoder* BinauralStage::acquire_decoder() noexcept
    {
        return decoder_slot_.acquire_for_audio_thread();
    }

    void BinauralStage::reclaim()
    {
        decoder_slot_.reclaim();
    }

    void BinauralStage::stage_diffuse(const RoomRenderSet& set) noexcept
    {
        pending_.left = set.diffuse_left;
        pending_.right = set.diffuse_right;
    }

    void BinauralStage::apply_pending() noexcept
    {
        // The direct pair, the decoder and this diffuse pair come from one head (spec §9.4).
        diffuse_[0].load(pending_.left.data());
        diffuse_[1].load(pending_.right.data());
    }

    void BinauralStage::decode(BinauralDecoder* decoder, const float* const* channels,
                               std::uint32_t frames) noexcept
    {
        if (decoder == nullptr || !decoder->ready())
        {
            clear_decoded(frames);
            return;
        }
        // A decoder that sat idle behind the microphone holds spectra of blocks long gone.
        if (!decoder_fed_)
        {
            decoder->reset();
        }
        decoder->process(channels, decoded_left_.data(), decoded_right_.data(), frames);
        decoder_fed_ = true;
    }

    void BinauralStage::clear_decoded(std::uint32_t frames) noexcept
    {
        std::fill_n(decoded_left_.data(), frames, 0.0f);
        std::fill_n(decoded_right_.data(), frames, 0.0f);
        decoder_fed_ = false;
    }

    void BinauralStage::silence() noexcept
    {
        std::fill_n(ear_.data(), ear_.size(), 0.0f);
    }

    const float* BinauralStage::ear(std::uint32_t ear) const noexcept
    {
        return ear_pointers_[ear];
    }

    void BinauralStage::align(const EarSources& sources, std::uint32_t frames) noexcept
    {
        for (std::uint32_t ear = 0; ear < kEars; ++ear)
        {
            float* const tail = tail_ears_[ear].data();
            diffuse_[ear].process(sources.tail[ear], tail, frames);
            tail_delay_[ear].process(tail, tail, frames);
            modal_delay_[ear].process(sources.modal[ear], modal_ears_[ear].data(), frames);
        }
    }

    void BinauralStage::join(const EarSources& sources, std::uint32_t frames,
                             const FadeWindow& window, bool active_ears, bool mixed) noexcept
    {
        const std::array<const float*, kEars> decoded = { decoded_left_.data(),
                                                          decoded_right_.data() };
        const float* const __restrict mono = sources.early_mono;
        for (std::uint32_t ear = 0; ear < kEars; ++ear)
        {
            float* const __restrict out = ear_pointers_[ear];
            const float* const __restrict decoded_ear = decoded[ear];
            const float* const __restrict direct_ear = sources.direct[ear];
            const float* const __restrict tail_ear = tail_ears_[ear].data();
            const float* const __restrict modal_ear = modal_ears_[ear].data();
            const float* const __restrict tail_plain = sources.tail[ear];
            const float* const __restrict modal_plain = sources.modal[ear];
            if (mixed)
            {
                // A fade across listeners weights each presentation by its bank's side of it.
                const float* const __restrict tail_mono = sources.tail[0];
                for (std::uint32_t frame = 0; frame < frames; ++frame)
                {
                    const float arriving =
                        frame < window.fading ? window_weight(window, frame, true) : 1.0f;
                    const float ears = active_ears ? arriving : 1.0f - arriving;
                    const float microphone = 1.0f - ears;
                    // Each side sums in its steady order, so a weight of one is that render.
                    const float ears_side = ((decoded_ear[frame] + direct_ear[frame])
                                             + ears * tail_ear[frame])
                        + ears * modal_ear[frame];
                    const float microphone_side =
                        (mono[frame] + microphone * tail_mono[frame])
                        + microphone * modal_plain[frame];
                    out[frame] = ears_side + microphone_side;
                }
                continue;
            }
            if (active_ears)
            {
                for (std::uint32_t frame = 0; frame < frames; ++frame)
                {
                    out[frame] = decoded_ear[frame] + direct_ear[frame] + tail_ear[frame]
                        + modal_ear[frame];
                }
                continue;
            }
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                out[frame] = mono[frame] + tail_plain[frame] + modal_plain[frame];
            }
        }
    }
}
