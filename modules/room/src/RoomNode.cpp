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
 * @brief The room's topology: the acquire, the two-bank crossfade, the tail and the mix.
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
#include "SushiDSP/room/BandResponse.hpp"
#include "SushiDSP/room/EyringDecay.hpp"
#include "SushiDSP/room/ImageSourceSolver.hpp"
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

        /// @brief Returns the arriving set's raised-cosine weight at one frame of the fade.
        float fade_weight(std::uint32_t frame, std::uint32_t total) noexcept
        {
            const double phase = static_cast<double>(frame + 1u) / static_cast<double>(total);
            return static_cast<float>(0.5 * (1.0 - std::cos(kPi * phase)));
        }
    }

    RoomNode::RoomNode()
    {
        schroeder_hz_ = EyringDecay::schroeder_frequency_hz(scene_);
    }

    void RoomNode::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;
        max_block_ = std::max<std::uint32_t>(info.max_block, 1u);

        early_.prepare(max_early_delay_frames(sample_rate_), max_block_);
        late_.prepare(static_cast<std::uint32_t>(std::ceil(kMaxLineSeconds * sample_rate_)));

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
        wet_.assign(max_block_, 0.0f);

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

        // One atomic acquire is the audio thread's whole share of a publish; the pointer is
        // compared against the last one and read only inside this block.
        const RoomRenderSet* const set = slot_.acquire_for_audio_thread();
        early_.write(input, frames);

        if (set == nullptr)
        {
            std::fill(wet_.begin(), wet_.begin() + frames, 0.0f);
            write_outputs(input, context.outputs, frames);
            return;
        }
        if (set != adopted_)
        {
            adopt(*set);
            adopted_ = set;
        }

        render_early(frames);
        late_.process(set->late, input, tail_.data(), tail_.data(), frames);
        sum_wet(frames);
        write_outputs(input, context.outputs, frames);
    }

    void RoomNode::adopt(const RoomRenderSet& set) noexcept
    {
        if (adopted_ == nullptr)
        {
            bank_taps_[active_bank_] = set.early;
            early_.clear_bank(active_bank_);
            fade_remaining_ = 0;
            return;
        }
        // A set arriving mid-fade restarts the fade from the bank now active; the host
        // coalesces edits so the node never holds more than two sets (spec §5.2).
        const std::uint32_t arriving = active_bank_ == 0 ? 1u : 0u;
        const EarlyTapSet& retiring = bank_taps_[active_bank_];
        const std::uint32_t paired = std::min(retiring.count, set.early.count);
        early_.clear_bank(arriving);
        for (std::uint32_t index = 0; index < paired; ++index)
        {
            if (taps_match(retiring.taps[index], set.early.taps[index]))
            {
                early_.carry_tap(active_bank_, arriving, index);
            }
        }
        bank_taps_[arriving] = set.early;
        active_bank_ = arriving;
        fade_remaining_ = kCrossfadeFrames;
    }

    void RoomNode::render_early(std::uint32_t frames) noexcept
    {
        clear_accumulation(active_bank_, frames);
        early_.render(bank_taps_[active_bank_], active_bank_,
                      accumulation_pointers_[active_bank_].data(), frames);
        if (fade_remaining_ == 0)
        {
            return;
        }

        const std::uint32_t retiring = active_bank_ == 0 ? 1u : 0u;
        clear_accumulation(retiring, frames);
        early_.render(bank_taps_[retiring], retiring,
                      accumulation_pointers_[retiring].data(), frames);

        const std::uint32_t fading = std::min(frames, fade_remaining_);
        const std::uint32_t done = kCrossfadeFrames - fade_remaining_;
        for (std::uint32_t channel = 0; channel < kChannels; ++channel)
        {
            float* const __restrict active = accumulation_pointers_[active_bank_][channel];
            const float* const __restrict old = accumulation_pointers_[retiring][channel];
            for (std::uint32_t frame = 0; frame < fading; ++frame)
            {
                const float weight = fade_weight(done + frame, kCrossfadeFrames);
                active[frame] = old[frame] + weight * (active[frame] - old[frame]);
            }
        }
        fade_remaining_ -= fading;
    }

    void RoomNode::sum_wet(std::uint32_t frames) noexcept
    {
        // Every set is rendered for the microphone, whose encode is channel 0 alone.
        const float* const __restrict early = accumulation_pointers_[active_bank_][0];
        const float* const __restrict tail = tail_.data();
        float* const __restrict wet = wet_.data();
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            wet[frame] = early[frame] + tail[frame];
        }
    }

    void RoomNode::write_outputs(const float* input, float* const* outputs,
                                 std::uint32_t frames) noexcept
    {
        float* const __restrict left = outputs[0];
        float* const __restrict right = outputs[1];
        const float* const __restrict wet = wet_.data();
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            const float mix = mix_.next();
            const float gain = level_.next();
            const float sample = gain * ((1.0f - mix) * input[frame] + mix * wet[frame]);
            left[frame] = sample;
            right[frame] = sample;
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
            case kListenerIndex: return listener_;
            case kDirectDelayIndex: return direct_delay_;
            default: return 0.0f;
        }
    }

    void RoomNode::set_parameter(const char* id, float value)
    {
        set_parameter(parameter_index_for(id), value);
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
                listener_ = std::clamp(std::round(value), 0.0f, 1.0f);
                return;
            case kDirectDelayIndex:
                direct_delay_ = std::clamp(std::round(value), 0.0f, 1.0f);
                return;
            default:
                return;
        }
    }

    void RoomNode::reset()
    {
        early_.reset();
        late_.reset();
        fade_remaining_ = 0;
        adopted_ = nullptr;
        if (prepared_)
        {
            mix_.snap(mix_.target());
            level_.snap(level_.target());
        }
    }

    std::uint32_t RoomNode::latency_samples() const
    {
        return 0u;
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
        schroeder_hz_ = EyringDecay::schroeder_frequency_hz(scene_);
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
                std::string reason;
                publish(reason);
            }
            return true;
        }
        if (prepared_ && set.sample_rate() != sample_rate_)
        {
            return false;
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
    }

    void RoomNode::remove_direct_image(RoomRenderSet& set) noexcept
    {
        if (set.listener != ListenerKind::Ears || set.early.count == 0)
        {
            return;
        }
        // For the ears the direct sound is the HRIR pair's alone; the taps hold reflections.
        for (std::uint32_t index = 1; index < set.early.count; ++index)
        {
            set.early.taps[index - 1u] = set.early.taps[index];
        }
        --set.early.count;
    }

    bool RoomNode::publish(std::string& reason_out)
    {
        try
        {
            // The ears' decoder is a later phase; every set renders the microphone for now.
            std::unique_ptr<RoomRenderSet> set =
                build_render_set(scene_, hrtf_, sample_rate_, ListenerKind::Microphone);
            remove_direct_image(*set);
            slot_.publish(std::move(set));
            return true;
        }
        catch (const std::exception& error)
        {
            reason_out = error.what();
            return false;
        }
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
