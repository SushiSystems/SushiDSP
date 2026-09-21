/**************************************************************************/
/* TubeScreamer.cpp                                                       */
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

#include "SushiDSP/tubescreamer/TubeScreamer.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace SushiDSP
{
    namespace
    {
        // Four times is where the diode pair's aliasing products fall below the noise floor of
        // the committed halfbands; the DK solve costs too much to spend eight (approved C1).
        constexpr std::uint32_t kOversamplingFactor = 4;

        // Time constant of every knob's smoother: slow enough that a jump cannot step the
        // clipper's operating point audibly, fast enough to feel immediate under a hand.
        constexpr float kSmoothingMs = 20.0f;

        constexpr float kMinLevelDecibels = -60.0f;
        constexpr float kMaxLevelDecibels = 12.0f;
        constexpr float kMinInputTrimDecibels = -24.0f;
        constexpr float kMaxInputTrimDecibels = 24.0f;

        constexpr std::uint16_t kUnknownParameterIndex = 0xFFFF;

        std::uint16_t parameter_index_for(const char* id)
        {
            if (std::strcmp(id, "drive") == 0) return 0;
            if (std::strcmp(id, "tone") == 0) return 1;
            if (std::strcmp(id, "level") == 0) return 2;
            if (std::strcmp(id, "input_trim") == 0) return 3;
            return kUnknownParameterIndex;
        }
    }

    TubeScreamer::TubeScreamer() = default;

    void TubeScreamer::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;

        tube_screamer_circuits::ClipNetlist clip_netlist;
        drive_pot_ = tube_screamer_circuits::build_clip_block(clip_netlist);
        tube_screamer_circuits::ToneNetlist tone_netlist;
        const tube_screamer_circuits::TonePotentiometers tone_pots
            = tube_screamer_circuits::build_tone_block(tone_netlist);
        tone_pot_ = tone_pots.tone;

        // The DK blocks are stepped once per oversampled sample, so they are discretised at the
        // run rate, not at the host rate.
        const double run_rate = sample_rate_ * static_cast<double>(kOversamplingFactor);
        circuits_ready_ = clip_circuit_.prepare(clip_netlist, run_rate)
                          && tone_circuit_.prepare(tone_netlist, run_rate);

        clip_circuit_.set_potentiometer(
            drive_pot_, tube_screamer_circuits::drive_to_position(drive_.target()));
        tone_circuit_.set_potentiometer(
            tone_pot_, tube_screamer_circuits::tone_to_position(tone_.target()));
        tone_circuit_.set_potentiometer(tone_pots.level,
                                        tube_screamer_circuits::kLevelPotPosition);
        clip_circuit_.update_matrices_if_dirty();
        tone_circuit_.update_matrices_if_dirty();

        drive_.prepare(sample_rate_, kSmoothingMs);
        tone_.prepare(sample_rate_, kSmoothingMs);
        level_gain_.prepare(sample_rate_, kSmoothingMs);
        input_trim_gain_.prepare(sample_rate_, kSmoothingMs);
        drive_.snap(drive_.target());
        tone_.snap(tone_.target());
        level_gain_.snap(level_gain_.target());
        input_trim_gain_.snap(input_trim_gain_.target());
        oversampler_ = std::make_unique<Oversampler4x>(info.max_block);
        oversampled_scratch_.assign(info.max_block, 0.0f);
        clipped_scratch_.assign(static_cast<std::size_t>(info.max_block) * kOversamplingFactor, 0.0f);
        reset();
    }

    void TubeScreamer::process(ProcessContext& context)
    {
        const float* input = context.inputs[0];
        float*       output = context.outputs[0];

        if (!circuits_ready_)
        {
            // A derivation that failed never recovers, so the pedal degrades to what its own
            // bypass would give: the dry signal, copied. No solver runs and the input is
            // already bounded, so this path cannot alias or blow up. The guard sits above
            // every skip() so all four smoothers advance together or not at all.
            std::copy(input, input + context.frames, output);
            return;
        }

        const float drive = drive_.skip(context.frames);
        const float tone = tone_.skip(context.frames);
        const float input_gain = input_trim_gain_.skip(context.frames);

        // Wiper positions are a control-rate concern: set_potentiometer marks nothing dirty when
        // the value has not moved, so a still knob costs nothing and a moved one costs one
        // re-derivation per block (D6b plan, ruling 3).
        clip_circuit_.set_potentiometer(
            drive_pot_, tube_screamer_circuits::drive_to_position(static_cast<double>(drive)));
        tone_circuit_.set_potentiometer(
            tone_pot_, tube_screamer_circuits::tone_to_position(static_cast<double>(tone)));
        clip_circuit_.update_matrices_if_dirty();
        tone_circuit_.update_matrices_if_dirty();

        // Volts in, volts out: the DK blocks are the real circuit and kFullScaleVolts is what
        // 0 dBFS means at the instrument jack (spec D5b.5). The D6a 20 Hz input biquad is gone -
        // the clip block's own 1 uF into 510 kOhm is the pedal's real input high-pass, at 0.31 Hz.
        for (std::uint32_t frame = 0; frame < context.frames; ++frame)
        {
            oversampled_scratch_[frame] = input[frame] * input_gain * kFullScaleVolts;
        }

        const float* upsampled = oversampler_->upsample(oversampled_scratch_.data(), context.frames);

        const std::uint32_t frames_oversampled = context.frames * kOversamplingFactor;
        for (std::uint32_t index = 0; index < frames_oversampled; ++index)
        {
            std::array<double, 1> clipped{};
            std::array<double, 1> toned{};
            clip_circuit_.process_sample(clip_port_, { static_cast<double>(upsampled[index]) },
                                         clipped);
            // The clip stage's probe is its op-amp's output node, which has zero output
            // impedance, so the tone block cannot load it: cascading the two within one sample is
            // exact, not an approximation (D6c plan, ruling 8).
            tone_circuit_.process_sample(tone_port_, clipped, toned);
            clipped_scratch_[index] = static_cast<float>(toned[0]);
        }

        // oversampled_scratch_ is reused here: every element in [0, frames) is overwritten by
        // downsample() before the level loop reads it back.
        oversampler_->downsample(clipped_scratch_.data(), frames_oversampled,
                                 oversampled_scratch_.data());

        for (std::uint32_t frame = 0; frame < context.frames; ++frame)
        {
            // Level alone ramps per sample: it is a single multiply outside the oversampled loop,
            // and stepping it once per block would make the audible smoothing time a function of
            // the host's block size.
            output[frame] = oversampled_scratch_[frame] / kFullScaleVolts * level_gain_.next()
                            * kLevelUnityGain;
        }
    }

    PortLayout TubeScreamer::ports() const
    {
        return { 1, 1 };
    }

    void TubeScreamer::reset()
    {
        // Zeroing the state vectors is the whole reset: a pedal has no bias point to settle, so
        // unlike JcmV1aStageNode::reset() there is no warm-up loop to run here.
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

    const char* TubeScreamer::type_id() const { return "sushidsp.tubescreamer"; }

    void TubeScreamer::set_drive(float normalized) { drive_.set_target(normalized); }
    void TubeScreamer::set_tone(float normalized) { tone_.set_target(normalized); }

    void TubeScreamer::set_level(float db)
    {
        level_db_ = std::clamp(db, kMinLevelDecibels, kMaxLevelDecibels);
        level_gain_.set_target(decibels_to_gain(level_db_));
    }

    void TubeScreamer::set_input_trim(float db)
    {
        input_trim_db_ = std::clamp(db, kMinInputTrimDecibels, kMaxInputTrimDecibels);
        input_trim_gain_.set_target(decibels_to_gain(input_trim_db_));
    }

    float TubeScreamer::get_parameter(const char* id) const
    {
        switch (parameter_index_for(id))
        {
            case 0: return drive_.target();
            case 1: return tone_.target();
            case 2: return level_db_;
            case 3: return input_trim_db_;
            default: return 0.0f;
        }
    }

    void TubeScreamer::set_parameter_for_control_thread(const char* id, float value)
    {
        set_parameter_for_audio_thread(parameter_index_for(id), value);
    }

    void TubeScreamer::set_parameter_for_audio_thread(std::uint16_t index, float value)
    {
        switch (index)
        {
            case 0: set_drive(value); return;
            case 1: set_tone(value); return;
            case 2: set_level(value); return;
            case 3: set_input_trim(value); return;
            default: return;
        }
    }

    namespace
    {
        constexpr ParameterDescriptor kTubeScreamerParameters[] = {
            { "drive",      "Drive",        0.0f,  1.0f, 0.5f, ParamKind::Linear,  "" },
            { "tone",       "Tone",         0.0f,  1.0f, 0.5f, ParamKind::Linear,  "" },
            { "level",      "Level",      kMinLevelDecibels, kMaxLevelDecibels, 0.0f,
              ParamKind::Decibel, "dB" },
            { "input_trim", "Input Trim", kMinInputTrimDecibels, kMaxInputTrimDecibels, 0.0f,
              ParamKind::Decibel, "dB" },
        };

        std::unique_ptr<IAudioNode> make_tube_screamer(const PrepareInfo&)
        {
            return std::make_unique<TubeScreamer>();
        }
    }

    const NodeDescriptor& tube_screamer_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.tubescreamer",
            "Tube Screamer",
            kTubeScreamerParameters,
            4,
            PortLayout{ 1, 1 },
            &make_tube_screamer,
        };
        return descriptor;
    }
}
