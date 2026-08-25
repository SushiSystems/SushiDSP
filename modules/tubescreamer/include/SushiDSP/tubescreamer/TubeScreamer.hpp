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

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"
#include "SushiDSP/math/Biquad.hpp"
#include "SushiDSP/math/Oversampler.hpp"
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

        /// @brief Sets the drive control, slewed toward this value over subsequent blocks.
        /// @param normalized Drive amount in [0, 1].
        void set_drive(float normalized);

        /// @brief Sets the tone control, slewed toward this value over subsequent blocks.
        /// @param normalized Tone position in [0, 1] (0 = dark, 1 = bright).
        void set_tone(float normalized);

        /// @brief Sets the output level, slewed toward this value over subsequent blocks.
        /// @param normalized Output level in [0, 1].
        void set_level(float normalized);

        // Public so the free function feedback_resistance_for_drive() in TubeScreamer.cpp's
        // anonymous namespace can reference them without befriending the class.
        static constexpr float kMinFeedbackResistance = 51000.0f;
        static constexpr float kMaxFeedbackResistance = 551000.0f;

    private:
        double sample_rate_ = 48000.0;

        float drive_target_ = 0.5f, drive_current_ = 0.5f;
        float tone_target_ = 0.5f, tone_current_ = 0.5f;
        float level_target_ = 0.7f, level_current_ = 0.7f;

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
