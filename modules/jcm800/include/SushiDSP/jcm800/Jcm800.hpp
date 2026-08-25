/**************************************************************************/
/* Jcm800.hpp                                                             */
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
#include "SushiDSP/jcm800/JcmToneStack.hpp"
#include "SushiDSP/jcm800/OutputTransformerStage.hpp"
#include "SushiDSP/jcm800/PentodeStage.hpp"
#include "SushiDSP/jcm800/PowerSupplySagStage.hpp"
#include "SushiDSP/jcm800/TriodeStage.hpp"
#include "SushiDSP/math/Biquad.hpp"
#include "SushiDSP/math/Oversampler.hpp"
#include "SushiDSP/math/PartitionedConvolution.hpp"

namespace SushiDSP
{
    /// @brief The Marshall JCM800 2203 amp: bright cap, V1a/V1b/tone-stack/V2a/phase-inverter/
    /// EL34-push-pull, output transformer, power sag, presence, and (optional) cabinet
    /// convolution. Exposes jcm800_descriptor().
    class Jcm800 : public IAudioNode
    {
    public:
        /// @param cabinet_impulse_response Cabinet IR samples; empty means no cabinet stage.
        explicit Jcm800(std::vector<float> cabinet_impulse_response = {});

        /// @brief Sizes every scratch buffer and rebuilds every sample-rate-dependent filter.
        /// @param info Sample rate, max block size, and channel count for this node's lifetime.
        void prepare(const PrepareInfo& info) override;

        /// @brief Runs one block through the full amp chain. Allocation-free.
        /// @param context The per-block input/output buffers and frame count.
        void process(ProcessContext& context) override;

        /// @brief Returns this node's fixed input/output channel counts.
        /// @return Mono in, mono out.
        PortLayout ports() const override;

        /// @brief Clears every stage's state for a click-free restart.
        void reset() override;

        /// @brief Sets the preamp (gain) control.
        /// @param normalized Preamp amount in [0, 1]; slewed toward over subsequent blocks.
        void set_preamp(float normalized);

        /// @brief Sets the master volume control.
        /// @param normalized Master amount in [0, 1]; slewed toward over subsequent blocks.
        void set_master(float normalized);

        /// @brief Sets the tone stack's bass control.
        /// @param normalized Bass amount in [0, 1]; 0.5 is flat.
        void set_bass(float normalized);

        /// @brief Sets the tone stack's middle control.
        /// @param normalized Middle amount in [0, 1]; 0.5 is flat.
        void set_middle(float normalized);

        /// @brief Sets the tone stack's treble control.
        /// @param normalized Treble amount in [0, 1]; 0.5 is flat.
        void set_treble(float normalized);

        /// @brief Sets the presence (high-shelf) control.
        /// @param normalized Presence amount in [0, 1]; 0.5 is flat.
        void set_presence(float normalized);

        float get_parameter(const char* id) const override;
        void set_parameter(const char* id, float value) override;

    private:
        static constexpr std::uint32_t kOversamplingFactor = 8;

        double sample_rate_ = 48000.0;
        std::uint32_t cabinet_partition_size_ = 0;
        std::vector<float> cabinet_impulse_response_;

        float preamp_target_ = 0.5f, preamp_current_ = 0.5f;
        float master_target_ = 0.7f, master_current_ = 0.7f;
        float presence_target_ = 0.5f, presence_current_ = 0.5f;

        float bass_normalized_ = 0.5f;
        float middle_normalized_ = 0.5f;
        float treble_normalized_ = 0.5f;

        Biquad bright_cap_;
        Biquad coupling_cap_;
        Biquad presence_shelf_;

        TriodeStage v1a_;
        TriodeStage v1b_;
        JcmToneStack tone_stack_;
        TriodeStage v2a_;
        TriodeStage phase_inverter_;
        PentodeStage el34_a_;
        PentodeStage el34_b_;
        OutputTransformerStage output_transformer_;
        PowerSupplySagStage sag_;

        std::unique_ptr<Oversampler8x> oversampler_a_;  // spans V1a -> V1b
        std::unique_ptr<Oversampler8x> oversampler_b_;  // spans V2a -> output transformer
        std::unique_ptr<PartitionedConvolution> cabinet_;

        std::vector<float> scratch_base_;
        std::vector<float> scratch_oversampled_a_;
        std::vector<float> scratch_oversampled_b_;
    };

    /// @brief The JCM800's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.jcm800".
    const NodeDescriptor& jcm800_descriptor();
}
