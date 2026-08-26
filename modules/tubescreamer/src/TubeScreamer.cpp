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
#include <cstring>

namespace SushiDSP
{
    namespace
    {
        constexpr float kInputHighpassCutoffHz = 20.0f;
        constexpr float kToneCrossoverHz = 720.0f;
        constexpr float kInputResistanceOhms = 4700.0f;
        constexpr float kSaturationCurrentAmps = 1e-12f;
        constexpr float kThermalVoltageVolts = 0.02585f;
        constexpr double kFilterQ = 0.707;
        constexpr std::uint32_t kOversamplingFactor = 8;

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

        float feedback_resistance_for_drive(float drive)
        {
            return TubeScreamer::kMinFeedbackResistance
                + drive * (TubeScreamer::kMaxFeedbackResistance - TubeScreamer::kMinFeedbackResistance);
        }
    }

    TubeScreamer::TubeScreamer()
        : clipping_stage_(
            kInputResistanceOhms,
            feedback_resistance_for_drive(0.5f),
            kSaturationCurrentAmps,
            kThermalVoltageVolts)
    {
    }

    void TubeScreamer::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;
        input_highpass_.set_high_pass(sample_rate_, kInputHighpassCutoffHz, kFilterQ);
        tone_lowpass_.set_low_pass(sample_rate_, kToneCrossoverHz, kFilterQ);
        tone_highpass_.set_high_pass(sample_rate_, kToneCrossoverHz, kFilterQ);
        drive_.prepare(sample_rate_, kSmoothingMs);
        tone_.prepare(sample_rate_, kSmoothingMs);
        level_gain_.prepare(sample_rate_, kSmoothingMs);
        input_trim_gain_.prepare(sample_rate_, kSmoothingMs);
        drive_.snap(drive_.target());
        tone_.snap(tone_.target());
        level_gain_.snap(level_gain_.target());
        input_trim_gain_.snap(input_trim_gain_.target());
        oversampler_ = std::make_unique<Oversampler8x>(info.max_block);
        oversampled_scratch_.assign(info.max_block, 0.0f);
        clipped_scratch_.assign(static_cast<std::size_t>(info.max_block) * kOversamplingFactor, 0.0f);
        reset();
    }

    void TubeScreamer::process(ProcessContext& context)
    {
        const float drive = drive_.skip(context.frames);
        const float tone = tone_.skip(context.frames);
        const float input_gain = input_trim_gain_.skip(context.frames);
        clipping_stage_.set_feedback_resistance(feedback_resistance_for_drive(drive));

        const float* input = context.inputs[0];
        float* output = context.outputs[0];

        for (std::uint32_t i = 0; i < context.frames; ++i)
        {
            oversampled_scratch_[i] = input_highpass_.process_sample(input[i] * input_gain);
        }

        const float* upsampled = oversampler_->upsample(oversampled_scratch_.data(), context.frames);

        const std::uint32_t frames_oversampled = context.frames * kOversamplingFactor;
        for (std::uint32_t i = 0; i < frames_oversampled; ++i)
        {
            clipped_scratch_[i] = clipping_stage_.process_sample(upsampled[i]);
        }

        // oversampled_scratch_ is reused here: every element in [0, frames) is overwritten by
        // downsample() before the tone stage reads it back.
        oversampler_->downsample(clipped_scratch_.data(), frames_oversampled, oversampled_scratch_.data());

        for (std::uint32_t i = 0; i < context.frames; ++i)
        {
            const float low = tone_lowpass_.process_sample(oversampled_scratch_[i]);
            const float high = tone_highpass_.process_sample(oversampled_scratch_[i]);
            const float toned = (1.0f - tone) * low + tone * high;
            // Level alone ramps per sample: it is a single multiply outside the oversampled
            // loop, and stepping it once per block would make the audible smoothing time a
            // function of the host's block size.
            output[i] = toned * level_gain_.next() * kLevelUnityGain;
        }
    }

    PortLayout TubeScreamer::ports() const
    {
        return { 1, 1 };
    }

    void TubeScreamer::reset()
    {
        input_highpass_.reset();
        tone_lowpass_.reset();
        tone_highpass_.reset();
        clipping_stage_.reset();
        if (oversampler_)
        {
            oversampler_->reset();
        }
    }

    std::uint32_t TubeScreamer::latency_samples() const
    {
        return oversampler_ ? oversampler_->latency_samples() : 0u;
    }

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

    void TubeScreamer::set_parameter(const char* id, float value)
    {
        set_parameter(parameter_index_for(id), value);
    }

    void TubeScreamer::set_parameter(std::uint16_t index, float value)
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
