/**************************************************************************/
/* tube_screamer.cpp                                                      */
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
 * @file tube_screamer.cpp
 * @brief Implements the Tube Screamer pedal: the clip block, the tone block and the level gain.
 * @author Mustafa Garip
 */

#include <SushiDSP/nodes/tubescreamer/tube_screamer.hpp>

#include <SushiDSP/foundation/core/node_counters.hpp>
#include <SushiDSP/foundation/log/log.hpp>
#include <SushiDSP/foundation/profile/profile.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>
#include <SushiDSP/nodes/tubescreamer/tubescreamer_log_category.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>

namespace SushiDSP
{
    namespace regions = profile::names::tubescreamer;

    namespace
    {
        // Four is where the alias floor was measured; see modules/nodes/tubescreamer/README.md.
        constexpr std::uint32_t K_OVERSAMPLING_FACTOR = 4;

        // One time constant serves every knob's smoother; see modules/nodes/tubescreamer/README.md.
        constexpr float K_SMOOTHING_MS = 20.0f;

        constexpr float K_MIN_LEVEL_DECIBELS = -60.0f;
        constexpr float K_MAX_LEVEL_DECIBELS = 12.0f;
        constexpr float K_MIN_INPUT_TRIM_DECIBELS = -24.0f;
        constexpr float K_MAX_INPUT_TRIM_DECIBELS = 24.0f;
    } // namespace

    TubeScreamer::TubeScreamer() = default;

    void TubeScreamer::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;

        tube_screamer_circuits::ClipNetlist clip_netlist;
        drive_pot_ = tube_screamer_circuits::build_clip_block(clip_netlist);
        tube_screamer_circuits::ToneNetlist tone_netlist;
        const tube_screamer_circuits::TonePotentiometers tone_pots =
            tube_screamer_circuits::build_tone_block(tone_netlist);
        tone_pot_ = tone_pots.tone;

        // The DK blocks are discretised at the oversampled run rate, not at the host rate.
        const double run_rate = sample_rate_ * static_cast<double>(K_OVERSAMPLING_FACTOR);
        circuits_ready_ = false;
        if (!(clip_circuit_.prepare(clip_netlist, run_rate) &&
              tone_circuit_.prepare(tone_netlist, run_rate)))
        {
            SD_LOG_ERROR(K_TUBESCREAMER_LOG_CATEGORY,
                         "TubeScreamer could not derive its clip and tone circuits at {} Hz; "
                         "prepare() throws and the node stays unprepared",
                         run_rate);
            throw std::runtime_error("TubeScreamer::prepare: the DK derivation failed");
        }
        circuits_ready_ = true;

        clip_circuit_.set_potentiometer(drive_pot_,
                                        tube_screamer_circuits::drive_to_position(drive_.target()));
        tone_circuit_.set_potentiometer(tone_pot_,
                                        tube_screamer_circuits::tone_to_position(tone_.target()));
        tone_circuit_.set_potentiometer(tone_pots.level,
                                        tube_screamer_circuits::K_LEVEL_POT_POSITION);
        clip_circuit_.update_matrices_if_dirty();
        tone_circuit_.update_matrices_if_dirty();

        drive_.prepare(sample_rate_, K_SMOOTHING_MS);
        tone_.prepare(sample_rate_, K_SMOOTHING_MS);
        level_gain_.prepare(sample_rate_, K_SMOOTHING_MS);
        input_trim_gain_.prepare(sample_rate_, K_SMOOTHING_MS);
        drive_.snap(drive_.target());
        tone_.snap(tone_.target());
        level_gain_.snap(level_gain_.target());
        input_trim_gain_.snap(input_trim_gain_.target());
        oversampler_ = std::make_unique<Oversampler4x>(info.max_block);
        oversampled_scratch_.assign(info.max_block, 0.0f);
        clipped_scratch_.assign(static_cast<std::size_t>(info.max_block) * K_OVERSAMPLING_FACTOR,
                                0.0f);
        reset();
    }

    void TubeScreamer::process(ProcessContext& context)
    {
        const float* input = context.inputs[0];
        float* output = context.outputs[0];

        if (!circuits_ready_)
        {
            // A node that is not prepared copies the dry signal (tubescreamer/README.md).
            std::copy(input, input + context.frames, output);
            return;
        }

        const float drive = drive_.skip(context.frames);
        const float tone = tone_.skip(context.frames);
        const float input_gain = input_trim_gain_.skip(context.frames);

        // A still knob marks nothing dirty; see modules/nodes/tubescreamer/README.md.
        clip_circuit_.set_potentiometer(
            drive_pot_, tube_screamer_circuits::drive_to_position(static_cast<double>(drive)));
        tone_circuit_.set_potentiometer(
            tone_pot_, tube_screamer_circuits::tone_to_position(static_cast<double>(tone)));
        clip_circuit_.update_matrices_if_dirty();
        tone_circuit_.update_matrices_if_dirty();

        // The loop works in volts in and volts out; see modules/nodes/tubescreamer/README.md.
        {
            SD_ZONE_GROUP(regions::K_INPUT_ZONE, regions::K_GROUP);
            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
            {
                oversampled_scratch_[frame] = input[frame] * input_gain * K_FULL_SCALE_VOLTS;
            }
        }

        const float* upsampled = nullptr;
        {
            SD_ZONE_GROUP(regions::K_UPSAMPLE_ZONE, regions::K_GROUP);
            upsampled = oversampler_->upsample(oversampled_scratch_.data(), context.frames);
        }

        const std::uint32_t frames_oversampled = context.frames * K_OVERSAMPLING_FACTOR;
        {
            SD_ZONE_GROUP(regions::K_LOOP_ZONE, regions::K_GROUP);
            for (std::uint32_t index = 0; index < frames_oversampled; ++index)
            {
                std::array<double, 1> clipped{};
                std::array<double, 1> toned{};
                clip_circuit_.process_sample(clip_port_, {static_cast<double>(upsampled[index])},
                                             clipped);
                // Cascading both blocks in a sample is exact (tubescreamer/README.md).
                tone_circuit_.process_sample(tone_port_, clipped, toned);
                clipped_scratch_[index] = static_cast<float>(toned[0]);
            }
        }
        flush_dk_statistics(clip_circuit_);

        {
            SD_ZONE_GROUP(regions::K_DOWNSAMPLE_ZONE, regions::K_GROUP);
            // downsample() overwrites [0, frames) of oversampled_scratch_ before the read-back.
            oversampler_->downsample(clipped_scratch_.data(), frames_oversampled,
                                     oversampled_scratch_.data());
        }

        SD_ZONE_GROUP(regions::K_OUTPUT_ZONE, regions::K_GROUP);
        for (std::uint32_t frame = 0; frame < context.frames; ++frame)
        {
            // Level alone ramps per sample; see modules/nodes/tubescreamer/README.md.
            output[frame] = oversampled_scratch_[frame] / K_FULL_SCALE_VOLTS * level_gain_.next() *
                            K_LEVEL_UNITY_GAIN;
        }
    }

    PortLayout TubeScreamer::ports() const
    {
        return {1, 1};
    }

    void TubeScreamer::reset()
    {
        // Zeroing the state vectors is the whole reset; see modules/nodes/tubescreamer/README.md.
        clip_circuit_.reset();
        tone_circuit_.reset();
        if (oversampler_)
        {
            oversampler_->reset();
        }
    }

    std::uint32_t TubeScreamer::latency_samples() const
    {
        return oversampler_ ? oversampler_->latency_samples() : 0u;
    }

    const char* TubeScreamer::type_id() const
    {
        return "sushidsp.tubescreamer";
    }

    void TubeScreamer::set_drive(float normalized)
    {
        drive_.set_target(normalized);
    }
    void TubeScreamer::set_tone(float normalized)
    {
        tone_.set_target(normalized);
    }

    void TubeScreamer::set_level(float db)
    {
        level_db_ = std::clamp(db, K_MIN_LEVEL_DECIBELS, K_MAX_LEVEL_DECIBELS);
        level_gain_.set_target(decibels_to_gain(level_db_));
    }

    void TubeScreamer::set_input_trim(float db)
    {
        input_trim_db_ = std::clamp(db, K_MIN_INPUT_TRIM_DECIBELS, K_MAX_INPUT_TRIM_DECIBELS);
        input_trim_gain_.set_target(decibels_to_gain(input_trim_db_));
    }

    const ParameterTable<TubeScreamer, 4>& TubeScreamer::parameters()
    {
        static const ParameterTable<TubeScreamer, 4> table{{{
            {{"drive", "Drive", 0.0f, 1.0f, 0.5f, ParamKind::Linear, ""},
             [](TubeScreamer& n, float v)
             {
                 n.set_drive(v);
             },
             [](const TubeScreamer& n)
             {
                 return n.drive_.target();
             }},
            {{"tone", "Tone", 0.0f, 1.0f, 0.5f, ParamKind::Linear, ""},
             [](TubeScreamer& n, float v)
             {
                 n.set_tone(v);
             },
             [](const TubeScreamer& n)
             {
                 return n.tone_.target();
             }},
            {{"level", "Level", K_MIN_LEVEL_DECIBELS, K_MAX_LEVEL_DECIBELS, 0.0f,
              ParamKind::Decibel, "dB"},
             [](TubeScreamer& n, float v)
             {
                 n.set_level(v);
             },
             [](const TubeScreamer& n)
             {
                 return n.level_db_;
             }},
            {{"input_trim", "Input Trim", K_MIN_INPUT_TRIM_DECIBELS, K_MAX_INPUT_TRIM_DECIBELS,
              0.0f, ParamKind::Decibel, "dB"},
             [](TubeScreamer& n, float v)
             {
                 n.set_input_trim(v);
             },
             [](const TubeScreamer& n)
             {
                 return n.input_trim_db_;
             }},
        }}};
        return table;
    }

    float TubeScreamer::get_parameter(const char* id) const
    {
        return parameters().read(*this, id);
    }

    void TubeScreamer::set_parameter_for_control_thread(const char* id, float value)
    {
        parameters().write(*this, id, value);
    }

    void TubeScreamer::set_parameter_for_audio_thread(std::uint16_t index, float value)
    {
        parameters().write(*this, index, value);
    }

    namespace
    {
        std::unique_ptr<AudioNode> make_tube_screamer(const PrepareInfo&)
        {
            return std::make_unique<TubeScreamer>();
        }
    } // namespace

    const NodeDescriptor& tube_screamer_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.tubescreamer",
            "Tube Screamer",
            TubeScreamer::parameters().descriptors(),
            TubeScreamer::parameters().count(),
            PortLayout{1, 1},
            &make_tube_screamer,
        };
        return descriptor;
    }
} // namespace SushiDSP
