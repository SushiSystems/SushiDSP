/**************************************************************************/
/* TubeScreamer.hpp                                                       */
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

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "SushiDSP/core/Calibration.hpp"
#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"
#include "SushiDSP/math/Biquad.hpp"
#include "SushiDSP/math/Oversampler.hpp"
#include "SushiDSP/math/SmoothedParameter.hpp"
#include "SushiDSP/tubescreamer/TubeScreamerClippingStage.hpp"

namespace SushiDSP
{
    /// @brief The Tube Screamer overdrive pedal: input highpass, 8x-oversampled op-amp
    /// clipping stage, tone crossfade, output level. Exposes tube_screamer_descriptor().
    class TubeScreamer : public IAudioNode
    {
    public:
        TubeScreamer();

        /// @brief Sizes every scratch buffer and builds the oversampler for this block size.
        /// @param info Sample rate, max block size, and channel count for this node's lifetime.
        void prepare(const PrepareInfo& info) override;

        /// @brief Runs one block through highpass, oversampled clipper, tone and level.
        /// @param context The per-block input/output buffers and frame count.
        void process(ProcessContext& context) override;

        /// @brief Returns this node's fixed input/output channel counts.
        /// @return Mono in, mono out.
        PortLayout ports() const override;

        /// @brief Clears every filter and solver state for a click-free restart.
        void reset() override;

        /// @brief Sets the drive control, smoothed toward this value over subsequent samples.
        /// @param normalized Drive amount in [0, 1].
        void set_drive(float normalized);

        /// @brief Sets the tone control, smoothed toward this value over subsequent samples.
        /// @param normalized Tone position in [0, 1] (0 = dark, 1 = bright).
        void set_tone(float normalized);

        /// @brief Sets the output level, smoothed toward this value in the gain domain.
        /// @param db Output level in [-60, +12] dB; 0 dB is the pedal's unity-ish output.
        void set_level(float db);

        /// @brief Sets the trim applied to the input before the pedal's own highpass.
        /// @param db Input trim in [-24, +24] dB.
        void set_input_trim(float db);

        using IAudioNode::set_parameter;

        /// @brief Reads a parameter by string id.
        /// @param id One of "drive", "tone", "level", "input_trim".
        /// @return The parameter's target value, or 0 for an unknown id.
        float get_parameter(const char* id) const override;

        /// @brief Sets a parameter by string id.
        /// @param id One of "drive", "tone", "level", "input_trim".
        /// @param value The new target value, in that parameter's own units.
        void set_parameter(const char* id, float value) override;

        /// @brief Sets a parameter by descriptor index, RT-safe and free of string compares.
        /// @param index The parameter's index in tube_screamer_descriptor().
        /// @param value The new target value, in that parameter's own units.
        void set_parameter(std::uint16_t index, float value) override;

        /// @brief Linear output gain at 0 dB on the Level knob: the real pedal's unity-ish
        /// output sits around Level 7, which the pre-dB implementation hard-coded as 0.7.
        static constexpr float kLevelUnityGain = 0.7f;

        // Public so the free function feedback_resistance_for_drive() in TubeScreamer.cpp's
        // anonymous namespace can reference them without befriending the class.
        static constexpr float kMinFeedbackResistance = 51000.0f;
        static constexpr float kMaxFeedbackResistance = 551000.0f;

    private:
        double sample_rate_ = 48000.0;

        SmoothedParameter drive_{ 0.5f };
        SmoothedParameter tone_{ 0.5f };
        SmoothedParameter level_gain_{ 1.0f };
        SmoothedParameter input_trim_gain_{ 1.0f };
        float level_db_ = 0.0f;
        float input_trim_db_ = 0.0f;

        Biquad input_highpass_;
        Biquad tone_lowpass_;
        Biquad tone_highpass_;
        TubeScreamerClippingStage clipping_stage_;
        std::unique_ptr<Oversampler8x> oversampler_;

        std::vector<float> oversampled_scratch_;
        std::vector<float> clipped_scratch_;
    };

    /// @brief The Tube Screamer's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.tubescreamer".
    const NodeDescriptor& tube_screamer_descriptor();
}
