/**************************************************************************/
/* room_node.cpp                                                          */
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
 * @file room_node.cpp
 * @brief Implements the room node: its stages, its one render set and the crossfade between sets.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/room_node.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <utility>

#include "SushiDSP/core/denormal_guard.hpp"
#include "SushiDSP/log/log.hpp"
#include "SushiDSP/math/streaming_convolution.hpp"
#include "SushiDSP/room/eyring_decay.hpp"
#include "SushiDSP/room/room_log_category.hpp"
#include "SushiDSP/room/starter_scene.hpp"

namespace SushiDSP::room
{
    namespace
    {
        constexpr float K_DEFAULT_DIRECT_DELAY = 0.0f;

        /// @brief Counts the receivers behind one cabinet at the limits.
        constexpr std::uint32_t K_RECEIVERS =
            static_cast<std::uint32_t>(Room::Arrangement::K_MAX_RECEIVERS);
    } // namespace

    struct RoomNode::Stages
    {
            /// @brief Renders what each cabinet puts into the room.
            std::array<Room::SourceStage, K_MAX_SOURCES> sources;

            /// @brief Renders each pair, indexed [cabinet][receiver]; receiver 0 is the listener.
            std::array<std::array<Room::PairStage, K_RECEIVERS>, K_MAX_SOURCES> pairs;

            /// @brief Sums the listener's pairs at the two ears.
            Room::EarsOutput ears;

            /// @brief Sums each microphone's pairs.
            std::array<Room::MicrophoneOutput, K_MAX_MICROPHONES> microphones;
    };

    RoomNode::RoomNode()
        : arrangement_(Room::Arrangement::from_scene(starter_scene())),
          stages_(std::make_unique<Stages>())
    {
        schroeder_hz_ = EyringDecay::schroeder_frequency_hz(arrangement_.pair_scene(0, 0));
    }

    RoomNode::~RoomNode() = default;

    bool RoomNode::same_resonators(const ModalDesign& retiring,
                                   const ModalDesign& arriving) noexcept
    {
        return Room::SourceStage::same_resonators(retiring, arriving);
    }

    bool RoomNode::take_direct_image(const RoomRenderSet& set, EarlyTapSet& reflections,
                                     EarlyTap& direct) noexcept
    {
        return Room::PairStage::take_direct_image(set, reflections, direct);
    }

    void RoomNode::compensate_direct_delay(RoomRenderSet& set) noexcept
    {
        Room::compensate_direct_delay(set);
    }

    void RoomNode::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;
        max_block_ = std::max<std::uint32_t>(info.max_block, 1u);
        alignment_frames_ = StreamingConvolution::partition_size_for(sample_rate_);
        scratch_.assign(max_block_, 0.0f);

        Stages& stages = *stages_;
        for (std::uint32_t source = 0; source < K_MAX_SOURCES; ++source)
        {
            stages.sources[source].prepare(sample_rate_, alignment_frames_, max_block_);
            for (std::uint32_t receiver = 0; receiver < K_RECEIVERS; ++receiver)
            {
                const ListenerKind kind =
                    receiver == 0 ? ListenerKind::Ears : ListenerKind::Microphone;
                stages.pairs[source][receiver].prepare(kind, sample_rate_, alignment_frames_,
                                                       max_block_);
            }
        }
        stages.ears.prepare(alignment_frames_, max_block_);
        for (Room::MicrophoneOutput& microphone : stages.microphones)
        {
            microphone.prepare(alignment_frames_, max_block_);
        }

        head_.refresh_head(sample_rate_);
        head_.publish_decoder(sample_rate_, max_block_);
        builder_.invalidate();

        prepared_ = true;
        reset();

        std::string reason;
        publish(reason);
    }

    void RoomNode::process(ProcessContext& context)
    {
        const DenormalGuard guard;
        const std::uint32_t frames = std::min(context.frames, max_block_);

        // One atomic acquire per slot is this thread's share; see modules/nodes/room/README.md.
        const Room::EnvironmentRenderSet* const set = slot_.acquire_for_audio_thread();
        BinauralDecoder* const decoder = head_.acquire_decoder();
        if (set == nullptr)
        {
            write_silence(context, frames);
            return;
        }
        if (set != adopted_)
        {
            adopt(*set);
            adopted_ = set;
        }

        // Every stage fades under one window read at the output, so the seam sees one weight.
        Stages& stages = *stages_;
        const FadeWindow window{fade_total_ - fade_remaining_, std::min(frames, fade_remaining_),
                                fade_lag_};
        for (std::uint32_t source = 0; source < source_count_; ++source)
        {
            Room::SourceStage& stage = stages.sources[source];
            const float* const input =
                context.inputs != nullptr ? context.inputs[K_SLOT_CHANNELS * source] : nullptr;
            stage.apply_pending_when_due(window);
            stage.write_input(input, frames);
            stage.render_tail(frames);
            stage.render_modal(frames, window);
            for (std::uint32_t receiver = 0; receiver <= microphone_count_; ++receiver)
            {
                stages.pairs[source][receiver].write_input(stage.high(), frames);
            }
        }

        const bool has_ports = context.port_outputs != nullptr;
        float* const left =
            has_ports ? context.port_outputs[K_LISTENER_PORT][0] : context.outputs[0];
        float* const right =
            has_ports ? context.port_outputs[K_LISTENER_PORT][1] : context.outputs[1];
        stages.ears.apply_pending_when_due(window);
        stages.ears.begin();
        for (std::uint32_t source = 0; source < source_count_; ++source)
        {
            Room::SourceStage& stage = stages.sources[source];
            Room::PairStage& pair = stages.pairs[source][0];
            pair.render(stage.ring(), stage.tail_ring(0), stage.tail_ring(1), frames, window);
            stages.ears.add(pair, stage.modal(0), stage.modal(1), frames);
        }
        stages.ears.finish(decoder, left, right, frames);

        for (std::uint32_t microphone = 0; microphone < K_MAX_MICROPHONES; ++microphone)
        {
            // With no port table a microphone still renders, so its state is current for a graph.
            float* const output =
                has_ports ? context.port_outputs[microphone_port(microphone)][0] : scratch_.data();
            if (microphone >= microphone_count_)
            {
                std::fill_n(output, frames, 0.0f);
                continue;
            }
            Room::MicrophoneOutput& sum = stages.microphones[microphone];
            sum.begin();
            for (std::uint32_t source = 0; source < source_count_; ++source)
            {
                Room::SourceStage& stage = stages.sources[source];
                Room::PairStage& pair = stages.pairs[source][1u + microphone];
                pair.render(stage.ring(), stage.tail_ring(0), stage.tail_ring(1), frames, window);
                sum.add(pair, stage.modal(2u + microphone), frames);
            }
            sum.finish(output, frames);
        }

        fade_remaining_ -= window.fading;
        if (fade_remaining_ == 0)
        {
            for (std::uint32_t source = 0; source < source_count_; ++source)
            {
                stages.sources[source].settle();
            }
        }
    }

    void RoomNode::write_silence(ProcessContext& context, std::uint32_t frames) noexcept
    {
        if (context.port_outputs == nullptr)
        {
            std::fill_n(context.outputs[0], frames, 0.0f);
            std::fill_n(context.outputs[1], frames, 0.0f);
            return;
        }
        std::fill_n(context.port_outputs[K_LISTENER_PORT][0], frames, 0.0f);
        std::fill_n(context.port_outputs[K_LISTENER_PORT][1], frames, 0.0f);
        for (std::uint32_t microphone = 0; microphone < K_MAX_MICROPHONES; ++microphone)
        {
            std::fill_n(context.port_outputs[microphone_port(microphone)][0], frames, 0.0f);
        }
    }

    void RoomNode::adopt(const Room::EnvironmentRenderSet& set) noexcept
    {
        Stages& stages = *stages_;
        const bool first = adopted_ == nullptr;
        const std::uint32_t sources = std::min(set.source_count, K_MAX_SOURCES);
        const std::uint32_t microphones = std::min(set.microphone_count, K_MAX_MICROPHONES);

        if (sources > 0)
        {
            stages.ears.stage_diffuse(set.pairs[0][0].diffuse_left.data(),
                                      set.pairs[0][0].diffuse_right.data());
        }
        for (std::uint32_t source = 0; source < K_MAX_SOURCES; ++source)
        {
            const bool held = source < sources;
            if (held)
            {
                stages.sources[source].adopt(set, source);
            }
            else if (stages.sources[source].live())
            {
                stages.sources[source].reset();
            }
            for (std::uint32_t receiver = 0; receiver < K_RECEIVERS; ++receiver)
            {
                Room::PairStage& pair = stages.pairs[source][receiver];
                if (held && receiver <= microphones)
                {
                    pair.adopt(set.pairs[source][receiver]);
                }
                else if (pair.live())
                {
                    // A pair that left the room starts from silence when it comes back.
                    pair.reset();
                }
            }
        }
        for (std::uint32_t microphone = microphones; microphone < microphone_count_; ++microphone)
        {
            stages.microphones[microphone].reset();
        }
        source_count_ = sources;
        microphone_count_ = microphones;

        if (first)
        {
            fade_remaining_ = 0;
            fade_total_ = 0;
            fade_lag_ = 0;
            stages.ears.apply_pending_when_due(FadeWindow{0, 0, 0});
            return;
        }
        // The ears' fade starts when the decoder's first partition of the set comes out.
        fade_lag_ = alignment_frames_;
        fade_total_ = K_CROSSFADE_FRAMES + fade_lag_;
        fade_remaining_ = fade_total_;
    }

    PortLayout RoomNode::ports() const
    {
        return {K_MAX_SOURCES * K_SLOT_CHANNELS, 2};
    }

    std::uint32_t RoomNode::output_port_count() const
    {
        return K_OUTPUT_PORTS;
    }

    std::uint32_t RoomNode::output_port_channels(std::uint32_t port) const
    {
        if (port == K_LISTENER_PORT)
        {
            return 2u;
        }
        return port < K_OUTPUT_PORTS ? 1u : 0u;
    }

    const ParameterTable<RoomNode, 1>& RoomNode::parameters()
    {
        static const ParameterTable<RoomNode, 1> table{{{
            {{"direct_delay", "Direct delay", 0.0f, 1.0f, K_DEFAULT_DIRECT_DELAY, ParamKind::Choice,
              ""},
             [](RoomNode& n, float v)
             {
                 n.direct_delay_.store(std::clamp(std::round(v), 0.0f, 1.0f),
                                       std::memory_order_relaxed);
             },
             [](const RoomNode& n)
             {
                 return n.direct_delay_.load(std::memory_order_relaxed);
             }},
        }}};
        return table;
    }

    float RoomNode::get_parameter(const char* id) const
    {
        return parameters().read(*this, id);
    }

    void RoomNode::set_parameter_for_control_thread(const char* id, float value)
    {
        parameters().write(*this, id, value);
        const bool delay_moved =
            std::strcmp(id, "direct_delay") == 0 && direct_delay_physical() != published_physical_;
        if (prepared_ && delay_moved)
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
        Stages& stages = *stages_;
        if (prepared_)
        {
            for (std::uint32_t source = 0; source < K_MAX_SOURCES; ++source)
            {
                stages.sources[source].reset();
                for (Room::PairStage& pair : stages.pairs[source])
                {
                    pair.reset();
                }
            }
            stages.ears.reset();
            for (Room::MicrophoneOutput& microphone : stages.microphones)
            {
                microphone.reset();
            }
            head_.reset_decoder();
        }
        fade_remaining_ = 0;
        fade_total_ = 0;
        fade_lag_ = 0;
        adopted_ = nullptr;
        source_count_ = 0;
        microphone_count_ = 0;
    }

    std::uint32_t RoomNode::latency_samples() const
    {
        return prepared_ ? alignment_frames_ + published_direct_delay_frames_ : 0u;
    }

    const char* RoomNode::type_id() const
    {
        return "sushidsp.room";
    }

    bool RoomNode::set_arrangement(const Room::Arrangement& arrangement, std::string& reason_out)
    {
        if (!arrangement.is_valid(reason_out))
        {
            return false;
        }
        Room::Arrangement previous = arrangement_;
        arrangement_ = arrangement;
        if (prepared_ && !publish(reason_out))
        {
            arrangement_ = std::move(previous);
            return false;
        }
        if (!prepared_)
        {
            schroeder_hz_ = EyringDecay::schroeder_frequency_hz(arrangement_.pair_scene(0, 0));
        }
        return true;
    }

    Room::Arrangement RoomNode::arrangement() const
    {
        return arrangement_;
    }

    bool RoomNode::set_scene(const RoomScene& scene, std::string& reason_out)
    {
        if (!scene.is_valid(reason_out))
        {
            return false;
        }
        Room::Arrangement next = arrangement_;
        next.set_shell(scene);
        next.set_listener(Room::Pose{scene.listener_position, scene.listener_yaw_rad});
        const Room::Pose cabinet{scene.source_position, scene.source_yaw_rad};
        if (next.sources().empty())
        {
            next.add_source(Room::SourcePlacement{0u, cabinet}, reason_out);
        }
        else
        {
            next.move_source(next.sources().front().id, cabinet);
        }
        return set_arrangement(next, reason_out);
    }

    RoomScene RoomNode::scene() const
    {
        RoomScene scene = arrangement_.shell();
        scene.listener_position = arrangement_.listener().position;
        scene.listener_yaw_rad = arrangement_.listener().yaw_rad;
        if (!arrangement_.sources().empty())
        {
            scene.source_position = arrangement_.sources().front().pose.position;
            scene.source_yaw_rad = arrangement_.sources().front().pose.yaw_rad;
        }
        return scene;
    }

    bool RoomNode::set_hrtf(HrtfSet set)
    {
        if (set.empty())
        {
            head_.set_source(HrtfSource::SphericalHead);
            if (prepared_)
            {
                head_.refresh_head(sample_rate_);
                head_.publish_decoder(sample_rate_, max_block_);
                builder_.invalidate();
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
            catch (const std::exception& error)
            {
                SD_LOG_ERROR(K_ROOM_LOG_CATEGORY,
                             "set_hrtf() could not prepare a decoder for the set of {} directions "
                             "at {} Hz: {}; the set is refused and the HRTF in place stays",
                             set.direction_count(), sample_rate_, error.what());
                return false;
            }
        }
        HrtfSet previous = head_.take_hrtf();
        const HrtfSource previous_source = head_.source();
        head_.set_hrtf(std::move(set));
        head_.set_source(HrtfSource::Measured);
        builder_.invalidate();
        std::string reason;
        if (prepared_ && !publish(reason))
        {
            head_.set_hrtf(std::move(previous));
            head_.set_source(previous_source);
            return false;
        }
        if (decoder)
        {
            head_.publish_decoder(std::move(decoder));
        }
        return true;
    }

    HrtfSource RoomNode::hrtf_source() const
    {
        return head_.source();
    }

    double RoomNode::schroeder_frequency_hz() const
    {
        return schroeder_hz_;
    }

    void RoomNode::reclaim_rooms()
    {
        slot_.reclaim();
        head_.reclaim();
    }

    bool RoomNode::direct_delay_physical() const noexcept
    {
        return direct_delay_.load(std::memory_order_relaxed) >= 0.5f;
    }

    bool RoomNode::publish(std::string& reason_out)
    {
        try
        {
            const bool physical = direct_delay_physical();
            std::unique_ptr<Room::EnvironmentRenderSet> set = builder_.build(
                arrangement_, head_.hrtf(), sample_rate_, physical, slot_.active_for_publisher());
            // The report is the split the set renders at, or Eyring's figure with the layer off.
            const bool modal_runs = set->source_count > 0u && set->pairs[0][0].modal.count > 0u;
            schroeder_hz_ =
                modal_runs
                    ? static_cast<double>(set->pairs[0][0].crossover_hz)
                    : EyringDecay::schroeder_frequency_hz(arrangement_.pair_scene(0, 0));
            const auto propagation =
                set->source_count > 0u
                    ? static_cast<std::uint32_t>(std::lround(set->pairs[0][0].direct_delay_frames))
                    : 0u;
            slot_.publish(std::move(set));
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

    namespace
    {
        std::unique_ptr<AudioNode> make_room(const PrepareInfo&)
        {
            return std::make_unique<RoomNode>();
        }
    } // namespace

    const NodeDescriptor& room_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.room",
            "Room",
            RoomNode::parameters().descriptors(),
            RoomNode::parameters().count(),
            PortLayout{RoomNode::K_MAX_SOURCES * RoomNode::K_SLOT_CHANNELS, 2},
            &make_room,
        };
        return descriptor;
    }
} // namespace SushiDSP::room
