/**************************************************************************/
/* RoomNode.cpp                                                           */
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
 * @file RoomNode.cpp
 * @brief The room's topology: the acquire, the two-bank crossfade, the seam, the ears, the tail and the mix.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/RoomNode.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <utility>

#include "SushiDSP/core/DenormalGuard.hpp"
#include "SushiDSP/math/StreamingConvolution.hpp"
#include "SushiDSP/room/BandResponse.hpp"
#include "SushiDSP/room/EyringDecay.hpp"
#include "SushiDSP/room/ImageSourceSolver.hpp"
#include "SushiDSP/room/ModalSolver.hpp"
#include "SushiDSP/room/SphericalHeadHrtf.hpp"

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds pi.
        constexpr double kPi = 3.14159265358979323846;

        /// @brief Holds the time constant of the mix and level smoothers, in milliseconds.
        constexpr float kSmoothingMs = 20.0f;

        /// @brief Holds the reflection order every set is solved to, RoomRenderSet's own.
        constexpr std::uint32_t kImageOrder = 4;

        /// @brief Holds the longest line the tail may need, in seconds: an RT60 of 3 s gives a
        /// mean line of 262.5 ms and a longest of 525 ms. Spec §6.2.
        constexpr double kMaxLineSeconds = 0.525;

        /// @brief Holds the frames the direct path's ring keeps past the longest tap for the
        /// far ear's ITD, 2.7 ms at 48 kHz against Woodworth's 0.66 ms.
        constexpr std::uint32_t kItdMarginFrames = 128;

        constexpr std::uint16_t kMixIndex = 0;
        constexpr std::uint16_t kLevelIndex = 1;
        constexpr std::uint16_t kListenerIndex = 2;
        constexpr std::uint16_t kDirectDelayIndex = 3;
        constexpr std::uint16_t kUnknownIndex = 0xFFFF;

        constexpr float kDefaultMix = 1.0f;
        constexpr float kDefaultLevelDb = 0.0f;
        constexpr float kDefaultListener = 0.0f;
        constexpr float kDefaultDirectDelay = 0.0f;
        constexpr float kMinLevelDb = -24.0f;
        constexpr float kMaxLevelDb = 24.0f;

        /// @brief Returns the descriptor index of an id, or kUnknownIndex.
        std::uint16_t parameter_index_for(const char* id) noexcept
        {
            if (std::strcmp(id, "mix") == 0) return kMixIndex;
            if (std::strcmp(id, "level") == 0) return kLevelIndex;
            if (std::strcmp(id, "listener") == 0) return kListenerIndex;
            if (std::strcmp(id, "direct_delay") == 0) return kDirectDelayIndex;
            return kUnknownIndex;
        }

        /// @brief Returns the frames the longest t_c the scene bounds allow takes at one rate:
        /// (order + 1) * kMaxDimension / c, 8397 at 48 kHz.
        std::uint32_t max_early_delay_frames(double sample_rate) noexcept
        {
            const double seconds = static_cast<double>(kImageOrder + 1u)
                * RoomScene::kMaxDimension / kSpeedOfSound;
            return static_cast<std::uint32_t>(std::ceil(seconds * sample_rate));
        }

        /// @brief Returns 10^(dB / 20).
        float decibels_to_gain(float decibels) noexcept
        {
            return std::pow(10.0f, decibels / 20.0f);
        }

        /// @brief Returns the smallest power of two at or above a count.
        std::uint32_t next_power_of_two(std::uint32_t count) noexcept
        {
            std::uint32_t power = 1;
            while (power < count)
            {
                power <<= 1u;
            }
            return power;
        }

        /// @brief Bounds how far a tap's delay may move, in frames, for its state to be carried.
        constexpr float kMatchDelayFrames = 2.0f;

        /// @brief Bounds how far any one of a tap's ten coefficients may move for its state to
        /// be carried. Measured in modules/room/README.md, "The two-bank crossfade".
        constexpr double kMatchCoefficientMove = 0.2;

        /// @brief Returns the largest move among the two sections' coefficients.
        double coefficient_move(const EarlyTap& retiring, const EarlyTap& arriving) noexcept
        {
            double move = 0.0;
            for (std::size_t index = 0; index < retiring.section_a.size(); ++index)
            {
                move = std::max(move, std::fabs(retiring.section_a[index]
                                                - arriving.section_a[index]));
                move = std::max(move, std::fabs(retiring.section_b[index]
                                                - arriving.section_b[index]));
            }
            return move;
        }

        /// @brief Returns true when an arriving tap is the retiring tap moved a little, its
        /// delay within kMatchDelayFrames and every coefficient within kMatchCoefficientMove.
        bool taps_match(const EarlyTap& retiring, const EarlyTap& arriving) noexcept
        {
            return std::fabs(retiring.delay_frames - arriving.delay_frames) <= kMatchDelayFrames
                && coefficient_move(retiring, arriving) <= kMatchCoefficientMove;
        }

        /// @brief Returns the arriving set's raised-cosine weight at one frame of the fade, one
        /// past its end: a path that runs ahead of the fade by its own latency reads past it.
        float fade_weight(std::uint32_t frame, std::uint32_t total) noexcept
        {
            if (frame + 1u >= total)
            {
                return 1.0f;
            }
            const double phase = static_cast<double>(frame + 1u) / static_cast<double>(total);
            return static_cast<float>(0.5 * (1.0 - std::cos(kPi * phase)));
        }

        /// @brief Returns true when two designs give any paired mode a different source or ear
        /// gain.
        bool gains_moved(const ModalDesign& retiring, const ModalDesign& arriving) noexcept
        {
            for (std::uint32_t index = 0; index < arriving.count; ++index)
            {
                const ModalDesign::Mode& before = retiring.modes[index];
                const ModalDesign::Mode& after = arriving.modes[index];
                if (before.source_gain != after.source_gain || before.ear_gain != after.ear_gain)
                {
                    return true;
                }
            }
            return false;
        }


        /// @brief Holds the four Lagrange weights of one read, already scaled by its gain.
        struct LagrangeKernel
        {
            /// @brief Stores the weight of the sample two frames behind the integer delay.
            float c0;

            /// @brief Stores the weight of the sample one frame behind the integer delay.
            float c1;

            /// @brief Stores the weight of the sample at the integer delay.
            float c2;

            /// @brief Stores the weight of the sample one frame ahead of the integer delay.
            float c3;
        };

        /// @brief Returns the third-order Lagrange weights the early renderer reads with, for
        /// a read at fraction frac behind the integer delay, scaled by gain.
        LagrangeKernel lagrange_kernel(double frac, double gain) noexcept
        {
            const double s = 2.0 - frac;
            const double s1 = s - 1.0;
            const double s2 = s - 2.0;
            const double s3 = s - 3.0;
            return LagrangeKernel{ static_cast<float>(-s1 * s2 * s3 * gain / 6.0),
                                   static_cast<float>(s * s2 * s3 * gain / 2.0),
                                   static_cast<float>(-s * s1 * s3 * gain / 2.0),
                                   static_cast<float>(s * s1 * s2 * gain / 6.0) };
        }

        /// @brief Runs a tap's two sections over a block in place, in the early renderer's
        /// arithmetic: double state, the first section's output fed to the second unrounded.
        void filter_sections(const EarlyTap& tap, std::array<double, 8>& state, float* samples,
                             std::uint32_t frames) noexcept
        {
            const double b0a = tap.section_a[0];
            const double b1a = tap.section_a[1];
            const double b2a = tap.section_a[2];
            const double a1a = tap.section_a[3];
            const double a2a = tap.section_a[4];
            const double b0b = tap.section_b[0];
            const double b1b = tap.section_b[1];
            const double b2b = tap.section_b[2];
            const double a1b = tap.section_b[3];
            const double a2b = tap.section_b[4];
            double xa1 = state[0];
            double xa2 = state[1];
            double ya1 = state[2];
            double ya2 = state[3];
            double xb1 = state[4];
            double xb2 = state[5];
            double yb1 = state[6];
            double yb2 = state[7];
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                const double x = static_cast<double>(samples[frame]);
                const double ya = b0a * x + b1a * xa1 + b2a * xa2 - a1a * ya1 - a2a * ya2;
                xa2 = xa1;
                xa1 = x;
                ya2 = ya1;
                ya1 = ya;
                const double yb = b0b * ya + b1b * xb1 + b2b * xb2 - a1b * yb1 - a2b * yb2;
                xb2 = xb1;
                xb1 = ya;
                yb2 = yb1;
                yb1 = yb;
                samples[frame] = static_cast<float>(yb);
            }
            state = { xa1, xa2, ya1, ya2, xb1, xb2, yb1, yb2 };
        }
    }

    namespace
    {
        /// @brief Returns the arriving set's weight at one frame of a window: read at the
        /// window's lag by a path that blends at the output, unlagged by a path whose output
        /// waits that lag downstream.
        float window_weight(const RoomNode::FadeWindow& window, std::uint32_t frame,
                            bool lagged) noexcept
        {
            const std::uint32_t index = window.done + frame;
            if (!lagged)
            {
                return fade_weight(index, RoomNode::kCrossfadeFrames);
            }
            return index < window.lag ? 0.0f
                                      : fade_weight(index - window.lag, RoomNode::kCrossfadeFrames);
        }

        /// @brief Blends a retiring pair of buffers into an arriving pair under the fade.
        void blend_pair(float* const __restrict left, float* const __restrict right,
                        const float* const __restrict old_left,
                        const float* const __restrict old_right,
                        const RoomNode::FadeWindow& window, bool lagged) noexcept
        {
            for (std::uint32_t frame = 0; frame < window.fading; ++frame)
            {
                const float weight = window_weight(window, frame, lagged);
                left[frame] = old_left[frame] + weight * (left[frame] - old_left[frame]);
                right[frame] = old_right[frame] + weight * (right[frame] - old_right[frame]);
            }
        }
    }

    void RoomNode::FirFilter::prepare(std::uint32_t max_block_frames)
    {
        history_.assign(static_cast<std::size_t>(kFirTaps) - 1u + max_block_frames, 0.0f);
    }

    void RoomNode::FirFilter::load(const float* taps) noexcept
    {
        // The taps are kept reversed, so the inner product runs forward over the history.
        for (std::uint32_t index = 0; index < kFirTaps; ++index)
        {
            taps_[index] = taps[kFirTaps - 1u - index];
        }
    }

    void RoomNode::FirFilter::reset() noexcept
    {
        std::fill(history_.begin(), history_.end(), 0.0f);
    }

    void RoomNode::FirFilter::carry_from(const FirFilter& other) noexcept
    {
        std::copy(other.history_.begin(), other.history_.end(), history_.begin());
    }

    void RoomNode::FirFilter::process(const float* input, float* output,
                                      std::uint32_t frames) noexcept
    {
        float* const history = history_.data();
        const float* const __restrict taps = taps_.data();
        std::copy_n(input, frames, history + (kFirTaps - 1u));
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            const float* const __restrict window = history + frame;
            float sum = 0.0f;
            for (std::uint32_t index = 0; index < kFirTaps; ++index)
            {
                sum += taps[index] * window[index];
            }
            output[frame] = sum;
        }
        std::copy(history + frames, history + frames + (kFirTaps - 1u), history);
    }

    void RoomNode::FrameDelay::prepare(std::uint32_t delay_frames, std::uint32_t max_block_frames)
    {
        delay_frames_ = delay_frames;
        line_.assign(static_cast<std::size_t>(delay_frames) + max_block_frames, 0.0f);
    }

    void RoomNode::FrameDelay::reset() noexcept
    {
        std::fill(line_.begin(), line_.end(), 0.0f);
    }

    void RoomNode::FrameDelay::process(const float* input, float* output,
                                       std::uint32_t frames) noexcept
    {
        float* const line = line_.data();
        std::copy_n(input, frames, line + delay_frames_);
        std::copy_n(line, frames, output);
        std::copy(line + frames, line + frames + delay_frames_, line);
    }

    void RoomNode::DelayRing::prepare(std::uint32_t max_delay_frames,
                                      std::uint32_t max_block_frames)
    {
        max_delay_frames_ = max_delay_frames;
        const std::uint32_t capacity = next_power_of_two(max_delay_frames + max_block_frames + 4u);
        ring_.assign(capacity, 0.0f);
        mask_ = capacity - 1u;
        block_start_ = 0;
        write_head_ = 0;
    }

    void RoomNode::DelayRing::reset() noexcept
    {
        std::fill(ring_.begin(), ring_.end(), 0.0f);
        block_start_ = 0;
        write_head_ = 0;
    }

    void RoomNode::DelayRing::write(const float* input, std::uint32_t frames) noexcept
    {
        float* const ring = ring_.data();
        block_start_ = write_head_;
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            ring[(write_head_ + frame) & mask_] = input[frame];
        }
        write_head_ = (write_head_ + frames) & mask_;
    }

    void RoomNode::DelayRing::read(double delay_frames, float gain, float* output,
                                   std::uint32_t frames) const noexcept
    {
        const double delay = std::min(std::max(delay_frames, 1.0),
                                      static_cast<double>(max_delay_frames_));
        const double whole = std::floor(delay);
        const LagrangeKernel kernel = lagrange_kernel(delay - whole, static_cast<double>(gain));
        // The window starts two frames behind the integer delay, as the early renderer's does.
        const std::uint32_t start =
            (block_start_ + static_cast<std::uint32_t>(ring_.size())
             - static_cast<std::uint32_t>(whole) - 2u) & mask_;
        const float* const ring = ring_.data();
        const std::uint32_t mask = mask_;
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            const std::uint32_t index = start + frame;
            output[frame] = kernel.c0 * ring[index & mask] + kernel.c1 * ring[(index + 1u) & mask]
                + kernel.c2 * ring[(index + 2u) & mask] + kernel.c3 * ring[(index + 3u) & mask];
        }
    }

    void RoomNode::DirectPath::prepare(std::uint32_t alignment_frames,
                                       std::uint32_t max_block_frames)
    {
        alignment_frames_ = alignment_frames;
        scratch_.assign(max_block_frames, 0.0f);
        for (std::uint32_t bank = 0; bank < kBanks; ++bank)
        {
            for (Ear& ear : banks_[bank].ears)
            {
                ear.hrir.prepare(max_block_frames);
            }
            clear_bank(bank);
        }
    }

    void RoomNode::DirectPath::reset() noexcept
    {
        for (std::uint32_t bank = 0; bank < kBanks; ++bank)
        {
            clear_bank(bank);
        }
    }

    void RoomNode::DirectPath::load_bank(std::uint32_t bank, const EarlyTap& tap,
                                         const RoomRenderSet& set) noexcept
    {
        Bank& target = banks_[bank];
        target.tap = tap;
        target.active = true;
        target.ears[0].hrir.load(set.direct_left.data());
        target.ears[1].hrir.load(set.direct_right.data());
        // A positive ITD puts the right ear later; the near ear reads at the tap's own delay.
        const double itd = static_cast<double>(set.direct_itd_frames);
        target.ears[0].extra_delay = std::max(-itd, 0.0);
        target.ears[1].extra_delay = std::max(itd, 0.0);
    }

    void RoomNode::DirectPath::clear_bank(std::uint32_t bank) noexcept
    {
        Bank& target = banks_[bank];
        target.active = false;
        for (Ear& ear : target.ears)
        {
            ear.filter_state.fill(0.0);
            ear.hrir.reset();
        }
    }

    void RoomNode::DirectPath::carry_bank(std::uint32_t from, std::uint32_t to) noexcept
    {
        if (from == to)
        {
            return;
        }
        for (std::uint32_t ear = 0; ear < kEars; ++ear)
        {
            banks_[to].ears[ear].filter_state = banks_[from].ears[ear].filter_state;
            banks_[to].ears[ear].hrir.carry_from(banks_[from].ears[ear].hrir);
        }
    }

    const EarlyTap& RoomNode::DirectPath::tap(std::uint32_t bank) const noexcept
    {
        return banks_[bank].tap;
    }

    bool RoomNode::DirectPath::active(std::uint32_t bank) const noexcept
    {
        return banks_[bank].active;
    }

    void RoomNode::DirectPath::render(const DelayRing& ring, std::uint32_t bank, float* left,
                                      float* right, std::uint32_t frames) noexcept
    {
        Bank& source = banks_[bank];
        if (!source.active)
        {
            std::fill_n(left, frames, 0.0f);
            std::fill_n(right, frames, 0.0f);
            return;
        }
        render_ear(ring, source, 0, left, frames);
        render_ear(ring, source, 1, right, frames);
    }

    void RoomNode::DirectPath::render_ear(const DelayRing& ring, Bank& bank, std::uint32_t ear,
                                          float* output, std::uint32_t frames) noexcept
    {
        Ear& chain = bank.ears[ear];
        float* const scratch = scratch_.data();
        ring.read(static_cast<double>(alignment_frames_) + bank.tap.delay_frames
                      + chain.extra_delay,
                  bank.tap.gain, scratch, frames);
        filter_sections(bank.tap, chain.filter_state, scratch, frames);
        chain.hrir.process(scratch, output, frames);
    }

    RoomNode::RoomNode()
    {
        schroeder_hz_ = EyringDecay::schroeder_frequency_hz(scene_);
    }

    bool RoomNode::same_resonators(const ModalDesign& retiring,
                                   const ModalDesign& arriving) noexcept
    {
        if (retiring.count != arriving.count)
        {
            return false;
        }
        for (std::uint32_t index = 0; index < arriving.count; ++index)
        {
            if (retiring.modes[index].section != arriving.modes[index].section)
            {
                return false;
            }
        }
        return true;
    }

    void RoomNode::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;
        max_block_ = std::max<std::uint32_t>(info.max_block, 1u);
        alignment_frames_ = StreamingConvolution::partition_size_for(sample_rate_);

        const std::uint32_t max_delay = max_early_delay_frames(sample_rate_);
        early_.prepare(max_delay, max_block_);
        geometric_ring_.prepare(alignment_frames_ + max_delay + kItdMarginFrames, max_block_);
        direct_.prepare(alignment_frames_, max_block_);
        late_.prepare(static_cast<std::uint32_t>(std::ceil(kMaxLineSeconds * sample_rate_)));
        for (ModalBank& bank : modal_banks_)
        {
            bank.prepare(max_block_);
        }
        for (std::uint32_t ear = 0; ear < kEars; ++ear)
        {
            diffuse_[ear].prepare(max_block_);
            tail_delay_[ear].prepare(alignment_frames_, max_block_);
            modal_delay_[ear].prepare(alignment_frames_, max_block_);
            tail_ears_[ear].assign(max_block_, 0.0f);
            modal_ears_[ear].assign(max_block_, 0.0f);
        }

        accumulation_.assign(static_cast<std::size_t>(kBanks) * kChannels * max_block_, 0.0f);
        for (std::uint32_t bank = 0; bank < kBanks; ++bank)
        {
            for (std::uint32_t channel = 0; channel < kChannels; ++channel)
            {
                accumulation_pointers_[bank][channel] = accumulation_.data()
                    + (static_cast<std::size_t>(bank) * kChannels + channel) * max_block_;
            }
        }
        tail_.assign(max_block_, 0.0f);
        tail_right_.assign(max_block_, 0.0f);
        tail_outputs_ = { tail_.data(), tail_.data() };
        tail_input_.assign(max_block_, 0.0f);
        tail_input_retiring_.assign(max_block_, 0.0f);
        modal_low_.assign(max_block_, 0.0f);
        geometric_high_.assign(max_block_, 0.0f);
        modal_left_.assign(max_block_, 0.0f);
        modal_right_.assign(max_block_, 0.0f);
        retiring_left_.assign(max_block_, 0.0f);
        retiring_right_.assign(max_block_, 0.0f);
        decoded_left_.assign(max_block_, 0.0f);
        decoded_right_.assign(max_block_, 0.0f);
        early_mono_.assign(max_block_, 0.0f);
        direct_left_.assign(max_block_, 0.0f);
        direct_right_.assign(max_block_, 0.0f);
        retiring_direct_left_.assign(max_block_, 0.0f);
        retiring_direct_right_.assign(max_block_, 0.0f);
        ear_.assign(static_cast<std::size_t>(kEars) * max_block_, 0.0f);
        for (std::uint32_t ear = 0; ear < kEars; ++ear)
        {
            ear_pointers_[ear] = ear_.data() + static_cast<std::size_t>(ear) * max_block_;
        }

        mix_.prepare(sample_rate_, kSmoothingMs);
        level_.prepare(sample_rate_, kSmoothingMs);
        mix_.snap(mix_.target());
        level_.snap(level_.target());

        // A measured set at another rate cannot be resampled here; the fallback takes over.
        if (hrtf_source_ == HrtfSource::Measured && hrtf_.sample_rate() != sample_rate_)
        {
            hrtf_source_ = HrtfSource::SphericalHead;
        }
        if (hrtf_source_ == HrtfSource::SphericalHead)
        {
            hrtf_ = SphericalHeadHrtf::build(sample_rate_);
        }
        publish_decoder();

        prepared_ = true;
        reset();

        std::string reason;
        publish(reason);
    }

    void RoomNode::process(ProcessContext& context)
    {
        const DenormalGuard guard;
        const std::uint32_t frames = std::min(context.frames, max_block_);
        const float* input = context.inputs[0];

        // One atomic acquire per slot is the audio thread's whole share of a publish; the set
        // pointer is compared against the last one and read only inside this block.
        const RoomRenderSet* const set = slot_.acquire_for_audio_thread();
        BinauralDecoder* const decoder = decoder_slot_.acquire_for_audio_thread();
        if (set != nullptr && set != adopted_)
        {
            adopt(*set);
            adopted_ = set;
        }
        split_input(input, frames);
        early_.write(geometric_high_.data(), frames);
        geometric_ring_.write(geometric_high_.data(), frames);

        if (set == nullptr)
        {
            std::fill_n(ear_.data(), ear_.size(), 0.0f);
            write_outputs(input, context.outputs, frames);
            return;
        }

        // Every layer fades under one window read at the output, so the seam sees one weight.
        const FadeWindow window{ fade_total_ - fade_remaining_, std::min(frames, fade_remaining_),
                                 fade_lag_ };
        apply_tail_when_due(window);
        render_early(frames, window);
        read_tail_input(frames, window);
        late_.process(tail_design_, tail_input_.data(), tail_outputs_[0], tail_outputs_[1],
                      frames);
        render_modal(frames, window);
        render_direct(frames, window);
        route_early(decoder, frames, window);
        align_ears(frames);
        join_ears(frames, window);
        fade_remaining_ -= window.fading;
        if (fade_remaining_ == 0)
        {
            modal_fading_ = false;
            modal_ramping_ = false;
        }

        write_outputs(input, context.outputs, frames);
    }

    void RoomNode::stage_tail(const RoomRenderSet& set) noexcept
    {
        pending_tail_.design = set.late;
        // The tail's delay to t_c is read out of the ring, so the tail itself waits nothing.
        pending_tail_.design.delay_to_crossover_frames = 0.0f;
        pending_tail_.two_ears = set.listener == ListenerKind::Ears;
        pending_tail_.diffuse_left = set.diffuse_left;
        pending_tail_.diffuse_right = set.diffuse_right;
        // The read delay moves under the fade from its lag on; the coefficients wait for it.
        tail_delay_from_ = adopted_ == nullptr ? set.late.delay_to_crossover_frames
                                               : tail_delay_to_;
        tail_delay_to_ = set.late.delay_to_crossover_frames;
        tail_pending_ = true;
    }

    void RoomNode::apply_tail_when_due(const FadeWindow& window) noexcept
    {
        if (!tail_pending_ || window.done < window.lag)
        {
            return;
        }
        tail_design_ = pending_tail_.design;
        // The microphone's tail is one signal read twice; the ears' passes the coherence pair.
        tail_outputs_[1] = pending_tail_.two_ears ? tail_right_.data() : tail_.data();
        // The direct pair, the decoder and this diffuse pair come from one head (spec §9.4).
        diffuse_[0].load(pending_tail_.diffuse_left.data());
        diffuse_[1].load(pending_tail_.diffuse_right.data());
        tail_pending_ = false;
    }

    void RoomNode::adopt(const RoomRenderSet& set) noexcept
    {
        input_crossover_.set_crossover(sample_rate_, set.crossover_hz);
        stage_tail(set);
        EarlyTap direct{};
        if (adopted_ == nullptr)
        {
            const bool has_direct = take_direct_image(set, bank_taps_[active_bank_], direct);
            early_.clear_bank(active_bank_);
            direct_.clear_bank(active_bank_);
            if (has_direct)
            {
                direct_.load_bank(active_bank_, direct, set);
            }
            bank_listener_[active_bank_] = set.listener;
            bank_modes_[active_modal_bank_] = set.modal;
            modal_banks_[active_modal_bank_].reset();
            fade_remaining_ = 0;
            fade_total_ = 0;
            fade_lag_ = 0;
            apply_tail_when_due(FadeWindow{ 0, 0, 0 });
            return;
        }
        // A set arriving mid-fade restarts the fade from the bank now active; the host
        // coalesces edits so the node never holds more than two sets (spec §5.2).
        const std::uint32_t arriving = active_bank_ == 0 ? 1u : 0u;
        const bool has_direct = take_direct_image(set, bank_taps_[arriving], direct);
        const EarlyTapSet& retiring = bank_taps_[active_bank_];
        const EarlyTapSet& reflections = bank_taps_[arriving];
        const std::uint32_t paired = std::min(retiring.count, reflections.count);
        early_.clear_bank(arriving);
        for (std::uint32_t index = 0; index < paired; ++index)
        {
            if (taps_match(retiring.taps[index], reflections.taps[index]))
            {
                early_.carry_tap(active_bank_, arriving, index);
            }
        }
        direct_.clear_bank(arriving);
        if (has_direct)
        {
            if (direct_.active(active_bank_) && taps_match(direct_.tap(active_bank_), direct))
            {
                direct_.carry_bank(active_bank_, arriving);
            }
            direct_.load_bank(arriving, direct, set);
        }
        // The ears' fade starts when the decoder's first partition of the set comes out.
        const bool ears_involved = set.listener == ListenerKind::Ears
            || bank_listener_[active_bank_] == ListenerKind::Ears;
        bank_listener_[arriving] = set.listener;
        active_bank_ = arriving;
        adopt_modal(set.modal);
        fade_lag_ = ears_involved ? alignment_frames_ : 0u;
        fade_total_ = kCrossfadeFrames + fade_lag_;
        fade_remaining_ = fade_total_;
    }

    void RoomNode::adopt_modal(const ModalDesign& design) noexcept
    {
        // A running ramp's last step is the design the active bank rendered last.
        ModalDesign& active = bank_modes_[active_modal_bank_];
        if (modal_ramping_)
        {
            active = modal_blend_;
        }
        if (same_resonators(active, design))
        {
            // Same resonators keep their state and ramp their gains alone (spec §9a.2).
            modal_fading_ = false;
            modal_ramping_ = gains_moved(active, design);
            modal_from_ = active;
            modal_blend_ = design;
            active = design;
            return;
        }
        const std::uint32_t arriving = active_modal_bank_ == 0 ? 1u : 0u;
        modal_banks_[arriving].reset();
        bank_modes_[arriving] = design;
        active_modal_bank_ = arriving;
        modal_fading_ = true;
        modal_ramping_ = false;
    }

    void RoomNode::render_early(std::uint32_t frames, const FadeWindow& window) noexcept
    {
        clear_accumulation(active_bank_, frames);
        early_.render(bank_taps_[active_bank_], active_bank_,
                      accumulation_pointers_[active_bank_].data(), frames);
        if (window.fading == 0)
        {
            return;
        }
        const std::uint32_t retiring = active_bank_ == 0 ? 1u : 0u;
        clear_accumulation(retiring, frames);
        early_.render(bank_taps_[retiring], retiring,
                      accumulation_pointers_[retiring].data(), frames);
    }

    void RoomNode::route_early(BinauralDecoder* decoder, std::uint32_t frames,
                               const FadeWindow& window) noexcept
    {
        const std::uint32_t retiring = active_bank_ == 0 ? 1u : 0u;
        const bool fading = window.fading > 0;
        const bool active_ears = bank_listener_[active_bank_] == ListenerKind::Ears;
        const bool retiring_ears = fading && bank_listener_[retiring] == ListenerKind::Ears;
        float* const __restrict mono = early_mono_.data();
        float* const* const active = accumulation_pointers_[active_bank_].data();
        float* const* const old = accumulation_pointers_[retiring].data();

        // The decoded channels read the fade unlagged and come out at the lag; channel 0 waits nothing.
        if (active_ears && (!fading || retiring_ears))
        {
            for (std::uint32_t channel = 0; channel < kChannels; ++channel)
            {
                for (std::uint32_t frame = 0; frame < window.fading; ++frame)
                {
                    const float weight = window_weight(window, frame, false);
                    active[channel][frame] = old[channel][frame]
                        + weight * (active[channel][frame] - old[channel][frame]);
                }
            }
            std::fill_n(mono, frames, 0.0f);
            decode(decoder, active, frames);
            return;
        }
        if (active_ears)
        {
            // The ears arrive over the microphone: its channel 0 fades out on its own.
            for (std::uint32_t frame = 0; frame < window.fading; ++frame)
            {
                const float weight = window_weight(window, frame, false);
                for (std::uint32_t channel = 0; channel < kChannels; ++channel)
                {
                    active[channel][frame] *= weight;
                }
                mono[frame] = (1.0f - window_weight(window, frame, true)) * old[0][frame];
            }
            std::fill(mono + window.fading, mono + frames, 0.0f);
            decode(decoder, active, frames);
            return;
        }
        if (retiring_ears)
        {
            // The microphone arrives over the ears: their sixteen channels fade out decoded.
            for (std::uint32_t frame = 0; frame < window.fading; ++frame)
            {
                const float weight = window_weight(window, frame, false);
                for (std::uint32_t channel = 0; channel < kChannels; ++channel)
                {
                    old[channel][frame] *= 1.0f - weight;
                }
                mono[frame] = window_weight(window, frame, true) * active[0][frame];
            }
            for (std::uint32_t channel = 0; channel < kChannels; ++channel)
            {
                std::fill(old[channel] + window.fading, old[channel] + frames, 0.0f);
            }
            std::copy(active[0] + window.fading, active[0] + frames, mono + window.fading);
            decode(decoder, old, frames);
            return;
        }
        for (std::uint32_t frame = 0; frame < window.fading; ++frame)
        {
            const float weight = window_weight(window, frame, true);
            mono[frame] = old[0][frame] + weight * (active[0][frame] - old[0][frame]);
        }
        std::copy(active[0] + window.fading, active[0] + frames, mono + window.fading);
        std::fill_n(decoded_left_.data(), frames, 0.0f);
        std::fill_n(decoded_right_.data(), frames, 0.0f);
        decoder_fed_ = false;
    }

    void RoomNode::decode(BinauralDecoder* decoder, const float* const* channels,
                          std::uint32_t frames) noexcept
    {
        if (decoder == nullptr || !decoder->ready())
        {
            std::fill_n(decoded_left_.data(), frames, 0.0f);
            std::fill_n(decoded_right_.data(), frames, 0.0f);
            decoder_fed_ = false;
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

    void RoomNode::render_direct(std::uint32_t frames, const FadeWindow& window) noexcept
    {
        direct_.render(geometric_ring_, active_bank_, direct_left_.data(), direct_right_.data(),
                       frames);
        if (window.fading == 0)
        {
            return;
        }
        const std::uint32_t retiring = active_bank_ == 0 ? 1u : 0u;
        direct_.render(geometric_ring_, retiring, retiring_direct_left_.data(),
                       retiring_direct_right_.data(), frames);
        blend_pair(direct_left_.data(), direct_right_.data(), retiring_direct_left_.data(),
                   retiring_direct_right_.data(), window, true);
    }

    void RoomNode::read_tail_input(std::uint32_t frames, const FadeWindow& window) noexcept
    {
        float* const __restrict input = tail_input_.data();
        geometric_ring_.read(static_cast<double>(tail_delay_to_), 1.0f, input, frames);
        if (window.fading == 0 || tail_delay_from_ == tail_delay_to_)
        {
            return;
        }
        float* const __restrict old = tail_input_retiring_.data();
        geometric_ring_.read(static_cast<double>(tail_delay_from_), 1.0f, old, window.fading);
        for (std::uint32_t frame = 0; frame < window.fading; ++frame)
        {
            const float weight = window_weight(window, frame, true);
            input[frame] = old[frame] + weight * (input[frame] - old[frame]);
        }
    }

    void RoomNode::split_input(const float* input, std::uint32_t frames) noexcept
    {
        float* const __restrict low = modal_low_.data();
        float* const __restrict high = geometric_high_.data();
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            input_crossover_.process_sample(input[frame], low[frame], high[frame]);
        }
    }

    void RoomNode::render_modal(std::uint32_t frames, const FadeWindow& window) noexcept
    {
        // The ears' field waits the alignment delay after the bank; the microphone's waits nothing.
        const bool microphone_arriving =
            bank_listener_[0] == ListenerKind::Microphone
            || bank_listener_[1] == ListenerKind::Microphone;
        const float* const low = modal_low_.data();
        float* const left = modal_left_.data();
        float* const right = modal_right_.data();
        std::fill_n(left, frames, 0.0f);
        std::fill_n(right, frames, 0.0f);

        if (modal_fading_ && window.fading > 0)
        {
            const std::uint32_t retiring = active_modal_bank_ == 0 ? 1u : 0u;
            float* const old_left = retiring_left_.data();
            float* const old_right = retiring_right_.data();
            std::fill_n(old_left, window.fading, 0.0f);
            std::fill_n(old_right, window.fading, 0.0f);
            modal_banks_[active_modal_bank_].process(bank_modes_[active_modal_bank_], low,
                                                     left, right, frames);
            modal_banks_[retiring].process(bank_modes_[retiring], low, old_left, old_right,
                                           window.fading);
            blend_pair(left, right, old_left, old_right, window, microphone_arriving);
            return;
        }
        if (modal_ramping_ && window.fading > 0)
        {
            for (std::uint32_t frame = 0; frame < window.fading; ++frame)
            {
                render_modal_ramp(frame, window_weight(window, frame, microphone_arriving));
            }
            modal_banks_[active_modal_bank_].process(bank_modes_[active_modal_bank_],
                                                     low + window.fading, left + window.fading,
                                                     right + window.fading,
                                                     frames - window.fading);
            return;
        }
        modal_banks_[active_modal_bank_].process(bank_modes_[active_modal_bank_], low, left,
                                                 right, frames);
    }

    void RoomNode::render_modal_ramp(std::uint32_t frame, float weight) noexcept
    {
        const ModalDesign& to = bank_modes_[active_modal_bank_];
        for (std::uint32_t index = 0; index < to.count; ++index)
        {
            const ModalDesign::Mode& from = modal_from_.modes[index];
            const ModalDesign::Mode& target = to.modes[index];
            ModalDesign::Mode& blend = modal_blend_.modes[index];
            blend.source_gain = from.source_gain
                + weight * (target.source_gain - from.source_gain);
            for (std::size_t ear = 0; ear < kEars; ++ear)
            {
                blend.ear_gain[ear] = from.ear_gain[ear]
                    + weight * (target.ear_gain[ear] - from.ear_gain[ear]);
            }
        }
        modal_banks_[active_modal_bank_].process(modal_blend_, modal_low_.data() + frame,
                                                 modal_left_.data() + frame,
                                                 modal_right_.data() + frame, 1u);
    }

    void RoomNode::align_ears(std::uint32_t frames) noexcept
    {
        const std::array<const float*, kEars> modal = { modal_left_.data(),
                                                        modal_right_.data() };
        for (std::uint32_t ear = 0; ear < kEars; ++ear)
        {
            float* const tail = tail_ears_[ear].data();
            diffuse_[ear].process(tail_outputs_[ear], tail, frames);
            tail_delay_[ear].process(tail, tail, frames);
            modal_delay_[ear].process(modal[ear], modal_ears_[ear].data(), frames);
        }
    }

    void RoomNode::join_ears(std::uint32_t frames, const FadeWindow& window) noexcept
    {
        const std::uint32_t retiring = active_bank_ == 0 ? 1u : 0u;
        const bool active_ears = bank_listener_[active_bank_] == ListenerKind::Ears;
        const bool mixed = window.fading > 0 && bank_listener_[retiring] != bank_listener_[active_bank_];
        const std::array<const float*, kEars> decoded = { decoded_left_.data(),
                                                          decoded_right_.data() };
        const std::array<const float*, kEars> direct = { direct_left_.data(),
                                                         direct_right_.data() };
        const std::array<const float*, kEars> modal = { modal_left_.data(),
                                                        modal_right_.data() };
        const float* const __restrict mono = early_mono_.data();
        for (std::uint32_t ear = 0; ear < kEars; ++ear)
        {
            float* const __restrict out = ear_pointers_[ear];
            const float* const __restrict decoded_ear = decoded[ear];
            const float* const __restrict direct_ear = direct[ear];
            const float* const __restrict tail_ear = tail_ears_[ear].data();
            const float* const __restrict modal_ear = modal_ears_[ear].data();
            const float* const __restrict tail_plain = tail_outputs_[ear];
            const float* const __restrict modal_plain = modal[ear];
            if (mixed)
            {
                // A fade across listeners weights each presentation by its bank's side of it.
                const float* const __restrict tail_mono = tail_outputs_[0];
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

    void RoomNode::write_outputs(const float* input, float* const* outputs,
                                 std::uint32_t frames) noexcept
    {
        float* const __restrict left = outputs[0];
        float* const __restrict right = outputs[1];
        const float* const __restrict wet_left = ear_pointers_[0];
        const float* const __restrict wet_right = ear_pointers_[1];
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            const float mix = mix_.next();
            const float gain = level_.next();
            const float dry = (1.0f - mix) * input[frame];
            left[frame] = gain * (dry + mix * wet_left[frame]);
            right[frame] = gain * (dry + mix * wet_right[frame]);
        }
    }

    void RoomNode::clear_accumulation(std::uint32_t set, std::uint32_t frames) noexcept
    {
        for (std::uint32_t channel = 0; channel < kChannels; ++channel)
        {
            std::fill_n(accumulation_pointers_[set][channel], frames, 0.0f);
        }
    }

    PortLayout RoomNode::ports() const
    {
        return { 1, 2 };
    }

    float RoomNode::get_parameter(const char* id) const
    {
        switch (parameter_index_for(id))
        {
            case kMixIndex: return mix_.target();
            case kLevelIndex: return level_db_;
            case kListenerIndex: return listener_.load(std::memory_order_relaxed);
            case kDirectDelayIndex: return direct_delay_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

    void RoomNode::set_parameter(const char* id, float value)
    {
        const std::uint16_t index = parameter_index_for(id);
        set_parameter(index, value);
        const bool listener_moved =
            index == kListenerIndex && listener_kind() != published_listener_;
        const bool delay_moved =
            index == kDirectDelayIndex && direct_delay_physical() != published_physical_;
        if (prepared_ && (listener_moved || delay_moved))
        {
            std::string reason;
            publish(reason);
        }
    }

    void RoomNode::set_parameter(std::uint16_t index, float value)
    {
        switch (index)
        {
            case kMixIndex:
                mix_.set_target(std::clamp(value, 0.0f, 1.0f));
                return;
            case kLevelIndex:
                level_db_ = std::clamp(value, kMinLevelDb, kMaxLevelDb);
                level_.set_target(decibels_to_gain(level_db_));
                return;
            case kListenerIndex:
                listener_.store(std::clamp(std::round(value), 0.0f, 1.0f),
                                std::memory_order_relaxed);
                return;
            case kDirectDelayIndex:
                direct_delay_.store(std::clamp(std::round(value), 0.0f, 1.0f),
                                    std::memory_order_relaxed);
                return;
            default:
                return;
        }
    }

    void RoomNode::reset()
    {
        early_.reset();
        geometric_ring_.reset();
        direct_.reset();
        late_.reset();
        for (ModalBank& bank : modal_banks_)
        {
            bank.reset();
        }
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
        input_crossover_.reset();
        modal_fading_ = false;
        modal_ramping_ = false;
        fade_remaining_ = 0;
        fade_total_ = 0;
        fade_lag_ = 0;
        tail_pending_ = false;
        adopted_ = nullptr;
        if (prepared_)
        {
            mix_.snap(mix_.target());
            level_.snap(level_.target());
        }
    }

    std::uint32_t RoomNode::latency_samples() const
    {
        if (!prepared_ || published_listener_ != ListenerKind::Ears)
        {
            return 0u;
        }
        return alignment_frames_ + published_direct_delay_frames_;
    }

    const char* RoomNode::type_id() const
    {
        return "sushidsp.room";
    }

    bool RoomNode::set_scene(const RoomScene& scene, std::string& reason_out)
    {
        if (!scene.is_valid(reason_out))
        {
            return false;
        }
        const RoomScene previous = scene_;
        scene_ = scene;
        if (prepared_ && !publish(reason_out))
        {
            scene_ = previous;
            return false;
        }
        if (!prepared_)
        {
            schroeder_hz_ = EyringDecay::schroeder_frequency_hz(scene_);
        }
        return true;
    }

    RoomScene RoomNode::scene() const
    {
        return scene_;
    }

    bool RoomNode::set_hrtf(HrtfSet set)
    {
        if (set.empty())
        {
            hrtf_source_ = HrtfSource::SphericalHead;
            if (prepared_)
            {
                hrtf_ = SphericalHeadHrtf::build(sample_rate_);
                publish_decoder();
                std::string reason;
                publish(reason);
            }
            return true;
        }
        if (prepared_ && set.sample_rate() != sample_rate_)
        {
            return false;
        }
        // A set the decoder refuses is refused whole, before anything in place is touched.
        std::unique_ptr<BinauralDecoder> decoder;
        if (prepared_)
        {
            try
            {
                decoder = std::make_unique<BinauralDecoder>();
                decoder->prepare(set, sample_rate_, max_block_);
            }
            catch (const std::exception&)
            {
                return false;
            }
        }
        HrtfSet previous = std::move(hrtf_);
        const HrtfSource previous_source = hrtf_source_;
        hrtf_ = std::move(set);
        hrtf_source_ = HrtfSource::Measured;
        std::string reason;
        if (prepared_ && !publish(reason))
        {
            hrtf_ = std::move(previous);
            hrtf_source_ = previous_source;
            return false;
        }
        if (decoder)
        {
            decoder_slot_.publish(std::move(decoder));
        }
        return true;
    }

    HrtfSource RoomNode::hrtf_source() const
    {
        return hrtf_source_;
    }

    double RoomNode::schroeder_frequency_hz() const
    {
        return schroeder_hz_;
    }

    void RoomNode::reclaim_rooms()
    {
        slot_.reclaim();
        decoder_slot_.reclaim();
    }

    ListenerKind RoomNode::listener_kind() const noexcept
    {
        return listener_.load(std::memory_order_relaxed) >= 0.5f ? ListenerKind::Microphone
                                                                  : ListenerKind::Ears;
    }

    bool RoomNode::direct_delay_physical() const noexcept
    {
        return direct_delay_.load(std::memory_order_relaxed) >= 0.5f;
    }

    bool RoomNode::place_modal_receiver(RoomRenderSet& set, const RoomScene& scene)
    {
        if (set.listener != ListenerKind::Microphone || set.modal.count == 0u)
        {
            return true;
        }
        // The solver's crossover is the one at which enumerate() returns the design's modes.
        const std::vector<ModalSolver::Mode> modes =
            ModalSolver::enumerate(scene, static_cast<double>(set.modal.crossover_hz));
        if (modes.size() != static_cast<std::size_t>(set.modal.count))
        {
            return false;
        }
        for (std::size_t index = 0; index < modes.size(); ++index)
        {
            const float psi = static_cast<float>(
                ModalSolver::mode_shape(modes[index], scene, scene.microphone_position));
            set.modal.modes[index].ear_gain = { psi, psi };
        }
        return true;
    }

    bool RoomNode::take_direct_image(const RoomRenderSet& set, EarlyTapSet& reflections,
                                     EarlyTap& direct) noexcept
    {
        if (set.listener != ListenerKind::Ears || set.early.count == 0)
        {
            reflections = set.early;
            return false;
        }
        // For the ears the direct sound is the HRIR pair's alone; the bank holds reflections.
        direct = set.early.taps[0];
        reflections.count = set.early.count - 1u;
        reflections.crossover_time_s = set.early.crossover_time_s;
        reflections.scattered_energy = set.early.scattered_energy;
        std::copy(set.early.taps.begin() + 1,
                  set.early.taps.begin() + static_cast<std::ptrdiff_t>(set.early.count),
                  reflections.taps.begin());
        return true;
    }

    void RoomNode::compensate_direct_delay(RoomRenderSet& set) noexcept
    {
        if (set.listener != ListenerKind::Ears || set.early.count == 0)
        {
            return;
        }
        const float shift = set.direct_delay_frames;
        // Tap 0 may land under a frame: the direct path reads it past the alignment delay.
        set.early.taps[0].delay_frames -= shift;
        for (std::uint32_t index = 1; index < set.early.count; ++index)
        {
            EarlyTap& tap = set.early.taps[index];
            tap.delay_frames = std::max(tap.delay_frames - shift, 1.0f);
        }
        set.late.delay_to_crossover_frames =
            std::max(set.late.delay_to_crossover_frames - shift, 0.0f);
    }

    bool RoomNode::publish(std::string& reason_out)
    {
        try
        {
            const ListenerKind listener = listener_kind();
            const bool physical = direct_delay_physical();
            std::unique_ptr<RoomRenderSet> set =
                build_render_set(scene_, hrtf_, sample_rate_, listener);
            if (!physical)
            {
                compensate_direct_delay(*set);
            }
            if (!place_modal_receiver(*set, scene_))
            {
                reason_out = "RoomNode: the modal enumeration at the set's crossover does not "
                             "hold the set's mode count";
                return false;
            }
            // The report is the split the set renders at, or Eyring's figure with the layer off.
            schroeder_hz_ = set->modal.count > 0u
                ? static_cast<double>(set->crossover_hz)
                : EyringDecay::schroeder_frequency_hz(scene_);
            const auto propagation =
                static_cast<std::uint32_t>(std::lround(set->direct_delay_frames));
            slot_.publish(std::move(set));
            published_listener_ = listener;
            published_physical_ = physical;
            published_direct_delay_frames_ = physical ? propagation : 0u;
            return true;
        }
        catch (const std::exception& error)
        {
            reason_out = error.what();
            return false;
        }
    }

    void RoomNode::publish_decoder()
    {
        auto decoder = std::make_unique<BinauralDecoder>();
        decoder->prepare(hrtf_, sample_rate_, max_block_);
        decoder_slot_.publish(std::move(decoder));
    }

    namespace
    {
        constexpr ParameterDescriptor kRoomParameters[] = {
            { "mix", "Mix", 0.0f, 1.0f, kDefaultMix, ParamKind::Linear, "" },
            { "level", "Level", kMinLevelDb, kMaxLevelDb, kDefaultLevelDb, ParamKind::Decibel,
              "dB" },
            { "listener", "Listener", 0.0f, 1.0f, kDefaultListener, ParamKind::Choice, "" },
            { "direct_delay", "Direct delay", 0.0f, 1.0f, kDefaultDirectDelay,
              ParamKind::Choice, "" },
        };

        std::unique_ptr<IAudioNode> make_room(const PrepareInfo&)
        {
            return std::make_unique<RoomNode>();
        }
    }

    const NodeDescriptor& room_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.room",
            "Room",
            kRoomParameters,
            4,
            PortLayout{ 1, 2 },
            &make_room,
        };
        return descriptor;
    }
}
