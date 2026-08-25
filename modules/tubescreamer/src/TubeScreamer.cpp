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

        // Reaches the target in ~10 blocks (~53 ms at 256 frames / 48 kHz), which keeps a
        // parameter jump from stepping the clipper's operating point within one block.
        constexpr float kSlewPerBlock = 0.1f;

        float slew_toward(float current, float target)
        {
            return current + (target - current) * kSlewPerBlock;
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
        oversampler_ = std::make_unique<Oversampler8x>(info.max_block);
        oversampled_scratch_.assign(info.max_block, 0.0f);
        clipped_scratch_.assign(static_cast<std::size_t>(info.max_block) * kOversamplingFactor, 0.0f);
        reset();
    }

    void TubeScreamer::process(ProcessContext& context)
    {
        drive_current_ = slew_toward(drive_current_, drive_target_);
        tone_current_ = slew_toward(tone_current_, tone_target_);
        level_current_ = slew_toward(level_current_, level_target_);
        clipping_stage_.set_feedback_resistance(feedback_resistance_for_drive(drive_current_));

        const float* input = context.inputs[0];
        float* output = context.outputs[0];

        for (std::uint32_t i = 0; i < context.frames; ++i)
        {
            oversampled_scratch_[i] = input_highpass_.process_sample(input[i]);
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
            const float toned = (1.0f - tone_current_) * low + tone_current_ * high;
            output[i] = toned * level_current_;
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

    void TubeScreamer::set_drive(float normalized) { drive_target_ = normalized; }
    void TubeScreamer::set_tone(float normalized) { tone_target_ = normalized; }
    void TubeScreamer::set_level(float normalized) { level_target_ = normalized; }

    float TubeScreamer::get_parameter(const char* id) const
    {
        if (std::strcmp(id, "drive") == 0) return drive_target_;
        if (std::strcmp(id, "tone") == 0) return tone_target_;
        if (std::strcmp(id, "level") == 0) return level_target_;
        return 0.0f;
    }

    void TubeScreamer::set_parameter(const char* id, float value)
    {
        if (std::strcmp(id, "drive") == 0) { set_drive(value); return; }
        if (std::strcmp(id, "tone") == 0) { set_tone(value); return; }
        if (std::strcmp(id, "level") == 0) { set_level(value); return; }
    }

    namespace
    {
        constexpr ParameterDescriptor kTubeScreamerParameters[] = {
            { "drive", "Drive", 0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
            { "tone",  "Tone",  0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
            { "level", "Level", 0.0f, 1.0f, 0.7f, ParamKind::Linear, "" },
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
            3,
            PortLayout{ 1, 1 },
            &make_tube_screamer,
        };
        return descriptor;
    }
}
