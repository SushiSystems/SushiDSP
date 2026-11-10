/**************************************************************************/
/* oversampled_stage_node.hpp                                             */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file oversampled_stage_node.hpp
 * @brief Wraps one circuit stage in an 8x region as an AudioNode, for the alias-floor tests.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/math/oversampler.hpp>

namespace TestSupport
{
    /// @brief One stage inside its own 8x region, the input scaled to a peak grid drive in volts and
    /// the output back to full scale, as the DA5 diagnosis measures a stage.
    /// @tparam Stage Any type with float process_sample(float) and void reset().
    template <typename Stage> class OversampledStageNode : public SushiDSP::AudioNode
    {
        public:
            /// @param stage The stage under test.
            /// @param drive_volts_peak The grid drive a full-scale -12 dBFS stimulus reaches.
            /// @param plate_scale The output divisor that brings the plate back inside full scale.
            OversampledStageNode(Stage stage, float drive_volts_peak, float plate_scale)
                : stage_(stage), drive_(drive_volts_peak / 0.251189f),
                  output_scale_(1.0f / plate_scale)
            {
            }

            void prepare(const SushiDSP::PrepareInfo& info) override
            {
                oversampler_ = std::make_unique<SushiDSP::Oversampler8x>(info.max_block);
                scratch_.assign(static_cast<std::size_t>(info.max_block) * 8, 0.0f);
                reset();
            }

            void process(SushiDSP::ProcessContext& context) override
            {
                const float* upsampled = oversampler_->upsample(context.inputs[0], context.frames);
                for (std::uint32_t i = 0; i < context.frames * 8; ++i)
                {
                    scratch_[i] = stage_.process_sample(upsampled[i] * drive_) * output_scale_;
                }
                oversampler_->downsample(scratch_.data(), context.frames * 8, context.outputs[0]);
            }

            SushiDSP::PortLayout ports() const override
            {
                return {1, 1};
            }

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }
            void set_parameter_for_control_thread(const char*, float) override {}

            void reset() override
            {
                stage_.reset();
                if (oversampler_)
                {
                    oversampler_->reset();
                }
            }

        private:
            /// @brief The stage under test.
            Stage stage_;

            /// @brief Volts of grid drive per full-scale unit of stimulus.
            float drive_;

            /// @brief The factor that brings the plate swing back inside full scale.
            float output_scale_;

            /// @brief The 8x region the stage runs in.
            std::unique_ptr<SushiDSP::Oversampler8x> oversampler_;

            /// @brief The oversampled block between the stage and the decimator.
            std::vector<float> scratch_;
    };
} // namespace TestSupport
