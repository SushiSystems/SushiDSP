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
 * @brief The room's topology: the acquire, the two-bank crossfade, the seam, the tail and the mix.
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

        /// @brief Blends the retiring modal bank into the arriving one, one buffer per ear.
        void blend_pair(float* const __restrict left, float* const __restrict right,
                        const float* const __restrict old_left,
                        const float* const __restrict old_right,
                        std::uint32_t done, std::uint32_t fading) noexcept
        {
            for (std::uint32_t frame = 0; frame < fading; ++frame)
            {
                const float weight = fade_weight(done + frame, RoomNode::kCrossfadeFrames);
                left[frame] = old_left[frame] + weight * (left[frame] - old_left[frame]);
                right[frame] = old_right[frame] + weight * (right[frame] - old_right[frame]);
            }
        }
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

        early_.prepare(max_early_delay_frames(sample_rate_), max_block_);
        late_.prepare(static_cast<std::uint32_t>(std::ceil(kMaxLineSeconds * sample_rate_)));
        for (ModalBank& bank : modal_banks_)
        {
            bank.prepare(max_block_);
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
        modal_low_.assign(max_block_, 0.0f);
        geometric_high_.assign(max_block_, 0.0f);
        modal_left_.assign(max_block_, 0.0f);
        modal_right_.assign(max_block_, 0.0f);
        retiring_left_.assign(max_block_, 0.0f);
        retiring_right_.assign(max_block_, 0.0f);
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
        if (set != nullptr && set != adopted_)
        {
            adopt(*set);
            adopted_ = set;
        }
        split_input(input, frames);
        early_.write(geometric_high_.data(), frames);

        if (set == nullptr)
        {
            std::fill_n(ear_.data(), ear_.size(), 0.0f);
            write_outputs(input, context.outputs, frames);
            return;
        }

        // Both layers fade under one window, so the seam never sees two different weights.
        const FadeWindow window{ kCrossfadeFrames - fade_remaining_,
                                 std::min(frames, fade_remaining_) };
        render_early(frames, window);
        late_.process(set->late, geometric_high_.data(), tail_outputs_[0], tail_outputs_[1],
                      frames);
        render_modal(frames, window);
        fade_remaining_ -= window.fading;
        if (fade_remaining_ == 0)
        {
            modal_fading_ = false;
            modal_ramping_ = false;
        }

        join_ears(frames);
        write_outputs(input, context.outputs, frames);
    }

    void RoomNode::adopt(const RoomRenderSet& set) noexcept
    {
        input_crossover_.set_crossover(sample_rate_, set.crossover_hz);
        // The microphone's tail is one signal read twice; the ears' passes the coherence pair.
        tail_outputs_[1] = set.listener == ListenerKind::Ears ? tail_right_.data() : tail_.data();
        if (adopted_ == nullptr)
        {
            bank_taps_[active_bank_] = set.early;
            early_.clear_bank(active_bank_);
            bank_modes_[active_modal_bank_] = set.modal;
            modal_banks_[active_modal_bank_].reset();
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
        adopt_modal(set.modal);
        fade_remaining_ = kCrossfadeFrames;
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

        for (std::uint32_t channel = 0; channel < kChannels; ++channel)
        {
            float* const __restrict active = accumulation_pointers_[active_bank_][channel];
            const float* const __restrict old = accumulation_pointers_[retiring][channel];
            for (std::uint32_t frame = 0; frame < window.fading; ++frame)
            {
                const float weight = fade_weight(window.done + frame, kCrossfadeFrames);
                active[frame] = old[frame] + weight * (active[frame] - old[frame]);
            }
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
            blend_pair(left, right, old_left, old_right, window.done, window.fading);
            return;
        }
        if (modal_ramping_ && window.fading > 0)
        {
            for (std::uint32_t frame = 0; frame < window.fading; ++frame)
            {
                render_modal_ramp(frame, fade_weight(window.done + frame, kCrossfadeFrames));
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

    void RoomNode::join_ears(std::uint32_t frames) noexcept
    {
        const float* const __restrict early = accumulation_pointers_[active_bank_][0];
        const std::array<const float*, kEars> modal = { modal_left_.data(),
                                                        modal_right_.data() };
        for (std::uint32_t ear = 0; ear < kEars; ++ear)
        {
            float* const __restrict out = ear_pointers_[ear];
            const float* const __restrict tail = tail_outputs_[ear];
            const float* const __restrict waves = modal[ear];
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                out[frame] = early[frame] + tail[frame] + waves[frame];
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
            case kDirectDelayIndex: return direct_delay_;
            default: return 0.0f;
        }
    }

    void RoomNode::set_parameter(const char* id, float value)
    {
        const std::uint16_t index = parameter_index_for(id);
        set_parameter(index, value);
        if (index == kListenerIndex && prepared_ && listener_kind() != published_listener_)
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
        for (ModalBank& bank : modal_banks_)
        {
            bank.reset();
        }
        input_crossover_.reset();
        modal_fading_ = false;
        modal_ramping_ = false;
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

    ListenerKind RoomNode::listener_kind() const noexcept
    {
        return listener_.load(std::memory_order_relaxed) >= 0.5f ? ListenerKind::Microphone
                                                                  : ListenerKind::Ears;
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
            const ListenerKind listener = listener_kind();
            std::unique_ptr<RoomRenderSet> set =
                build_render_set(scene_, hrtf_, sample_rate_, listener);
            // The ears keep the direct image until the HRIR pair renders it (README, "What each set renders today").
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
            slot_.publish(std::move(set));
            published_listener_ = listener;
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
