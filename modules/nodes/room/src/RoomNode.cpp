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
 * @brief The room's topology: the acquire, the seam, the three paths, the swap and the mix.
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
#include "SushiDSP/room/EyringDecay.hpp"
#include "SushiDSP/room/StarterScene.hpp"

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds the time constant of the mix and level smoothers, in milliseconds.
        constexpr float kSmoothingMs = 20.0f;

        constexpr float kDefaultMix = 1.0f;
        constexpr float kDefaultLevelDb = 0.0f;
        constexpr float kDefaultListener = 0.0f;
        constexpr float kDefaultDirectDelay = 0.0f;
        constexpr float kMinLevelDb = -24.0f;
        constexpr float kMaxLevelDb = 24.0f;

        /// @brief Returns 10^(dB / 20).
        float decibels_to_gain(float decibels) noexcept
        {
            return std::pow(10.0f, decibels / 20.0f);
        }
    }

    RoomNode::RoomNode()
    {
        scene_ = starter_scene();
        schroeder_hz_ = EyringDecay::schroeder_frequency_hz(scene_);
    }

    bool RoomNode::same_resonators(const ModalDesign& retiring,
                                   const ModalDesign& arriving) noexcept
    {
        return ModalPath::same_resonators(retiring, arriving);
    }

    bool RoomNode::take_direct_image(const RoomRenderSet& set, EarlyTapSet& reflections,
                                     EarlyTap& direct) noexcept
    {
        return GeometricPath::take_direct_image(set, reflections, direct);
    }

    void RoomNode::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;
        max_block_ = std::max<std::uint32_t>(info.max_block, 1u);
        alignment_frames_ = StreamingConvolution::partition_size_for(sample_rate_);

        modal_.prepare(max_block_);
        geometric_.prepare(sample_rate_, alignment_frames_, max_block_);
        binaural_.prepare(alignment_frames_, max_block_);

        mix_.prepare(sample_rate_, kSmoothingMs);
        level_.prepare(sample_rate_, kSmoothingMs);
        mix_.snap(mix_.target());
        level_.snap(level_.target());

        binaural_.refresh_head(sample_rate_);
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
        BinauralDecoder* const decoder = binaural_.acquire_decoder();
        if (set != nullptr && set != adopted_)
        {
            adopt(*set);
            adopted_ = set;
        }
        split_input(input, frames);
        geometric_.write_input(frames);

        if (set == nullptr)
        {
            binaural_.silence();
            write_outputs(input, context.outputs, frames);
            return;
        }

        // Every layer fades under one window read at the output, so the seam sees one weight.
        const FadeWindow window{ fade_total_ - fade_remaining_, std::min(frames, fade_remaining_),
                                 fade_lag_ };
        apply_pending_when_due(window);
        geometric_.render_early(frames, window);
        geometric_.render_tail(frames, window);
        modal_.render(frames, window, geometric_.any_microphone());
        geometric_.render_direct(frames, window);
        if (const float* const* channels = geometric_.route_early(frames, window))
        {
            binaural_.decode(decoder, channels, frames);
        }
        else
        {
            binaural_.clear_decoded(frames);
        }
        const BinauralStage::EarSources sources{
            { geometric_.direct(0), geometric_.direct(1) },
            { geometric_.tail(0), geometric_.tail(1) },
            { modal_.field(0), modal_.field(1) },
            geometric_.early_mono()
        };
        binaural_.align(sources, frames);
        binaural_.join(sources, frames, window, geometric_.active_is_ears(),
                       window.fading > 0 && geometric_.listeners_differ());
        fade_remaining_ -= window.fading;
        if (fade_remaining_ == 0)
        {
            modal_.settle();
        }

        write_outputs(input, context.outputs, frames);
    }

    void RoomNode::apply_pending_when_due(const FadeWindow& window) noexcept
    {
        if (!pending_ || window.done < window.lag)
        {
            return;
        }
        geometric_.apply_pending();
        binaural_.apply_pending();
        pending_ = false;
    }

    void RoomNode::adopt(const RoomRenderSet& set) noexcept
    {
        input_crossover_.set_crossover(sample_rate_, set.crossover_hz);
        const bool first = adopted_ == nullptr;
        geometric_.stage_tail(set, first);
        binaural_.stage_diffuse(set);
        pending_ = true;
        if (first)
        {
            geometric_.adopt_first(set);
            modal_.adopt_first(set.modal);
            fade_remaining_ = 0;
            fade_total_ = 0;
            fade_lag_ = 0;
            apply_pending_when_due(FadeWindow{ 0, 0, 0 });
            return;
        }
        const bool ears_involved = geometric_.adopt(set);
        modal_.adopt(set.modal);
        fade_lag_ = ears_involved ? alignment_frames_ : 0u;
        fade_total_ = kCrossfadeFrames + fade_lag_;
        fade_remaining_ = fade_total_;
    }

    void RoomNode::split_input(const float* input, std::uint32_t frames) noexcept
    {
        float* const __restrict low = modal_.low_buffer();
        float* const __restrict high = geometric_.high_buffer();
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            input_crossover_.process_sample(input[frame], low[frame], high[frame]);
        }
    }

    void RoomNode::write_outputs(const float* input, float* const* outputs,
                                 std::uint32_t frames) noexcept
    {
        float* const __restrict left = outputs[0];
        float* const __restrict right = outputs[1];
        const float* const __restrict wet_left = binaural_.ear(0);
        const float* const __restrict wet_right = binaural_.ear(1);
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            const float mix = mix_.next();
            const float gain = level_.next();
            const float dry = (1.0f - mix) * input[frame];
            left[frame] = gain * (dry + mix * wet_left[frame]);
            right[frame] = gain * (dry + mix * wet_right[frame]);
        }
    }

    PortLayout RoomNode::ports() const
    {
        return { 1, 2 };
    }

    const ParameterTable<RoomNode, 4>& RoomNode::parameters()
    {
        static const ParameterTable<RoomNode, 4> table{ {{
            { { "mix", "Mix", 0.0f, 1.0f, kDefaultMix, ParamKind::Linear, "" },
              [](RoomNode& n, float v) { n.mix_.set_target(std::clamp(v, 0.0f, 1.0f)); },
              [](const RoomNode& n) { return n.mix_.target(); } },
            { { "level", "Level", kMinLevelDb, kMaxLevelDb, kDefaultLevelDb, ParamKind::Decibel,
                "dB" },
              [](RoomNode& n, float v) {
                  n.level_db_ = std::clamp(v, kMinLevelDb, kMaxLevelDb);
                  n.level_.set_target(decibels_to_gain(n.level_db_));
              },
              [](const RoomNode& n) { return n.level_db_; } },
            { { "listener", "Listener", 0.0f, 1.0f, kDefaultListener, ParamKind::Choice, "" },
              [](RoomNode& n, float v) {
                  n.listener_.store(std::clamp(std::round(v), 0.0f, 1.0f),
                                    std::memory_order_relaxed);
              },
              [](const RoomNode& n) { return n.listener_.load(std::memory_order_relaxed); } },
            { { "direct_delay", "Direct delay", 0.0f, 1.0f, kDefaultDirectDelay,
                ParamKind::Choice, "" },
              [](RoomNode& n, float v) {
                  n.direct_delay_.store(std::clamp(std::round(v), 0.0f, 1.0f),
                                        std::memory_order_relaxed);
              },
              [](const RoomNode& n) { return n.direct_delay_.load(std::memory_order_relaxed); } },
        }} };
        return table;
    }

    float RoomNode::get_parameter(const char* id) const { return parameters().read(*this, id); }

    void RoomNode::set_parameter_for_control_thread(const char* id, float value)
    {
        parameters().write(*this, id, value);
        const bool listener_moved =
            std::strcmp(id, "listener") == 0 && listener_kind() != published_listener_;
        const bool delay_moved =
            std::strcmp(id, "direct_delay") == 0 && direct_delay_physical() != published_physical_;
        if (prepared_ && (listener_moved || delay_moved))
        {
            std::string reason;
            publish(reason);
        }
    }

    void RoomNode::set_parameter_for_audio_thread(std::uint16_t index, float value)
    {
        parameters().write(*this, index, value);
    }

    void RoomNode::reset()
    {
        modal_.reset();
        geometric_.reset();
        binaural_.reset();
        input_crossover_.reset();
        fade_remaining_ = 0;
        fade_total_ = 0;
        fade_lag_ = 0;
        pending_ = false;
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
            binaural_.set_source(HrtfSource::SphericalHead);
            if (prepared_)
            {
                binaural_.refresh_head(sample_rate_);
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
        HrtfSet previous = binaural_.take_hrtf();
        const HrtfSource previous_source = binaural_.source();
        binaural_.set_hrtf(std::move(set));
        binaural_.set_source(HrtfSource::Measured);
        std::string reason;
        if (prepared_ && !publish(reason))
        {
            binaural_.set_hrtf(std::move(previous));
            binaural_.set_source(previous_source);
            return false;
        }
        if (decoder)
        {
            binaural_.publish_decoder(std::move(decoder));
        }
        return true;
    }

    HrtfSource RoomNode::hrtf_source() const
    {
        return binaural_.source();
    }

    double RoomNode::schroeder_frequency_hz() const
    {
        return schroeder_hz_;
    }

    void RoomNode::reclaim_rooms()
    {
        slot_.reclaim();
        binaural_.reclaim();
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
                build_render_set(scene_, binaural_.hrtf(), sample_rate_, listener);
            if (!physical)
            {
                compensate_direct_delay(*set);
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
        binaural_.publish_decoder(sample_rate_, max_block_);
    }

    namespace
    {
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
            RoomNode::parameters().descriptors(),
            RoomNode::parameters().count(),
            PortLayout{ 1, 2 },
            &make_room,
        };
        return descriptor;
    }
}
