/**************************************************************************/
/* test_output.cpp                                                        */
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
 * @file test_output.cpp
 * @brief Tests that one triode stage in its own 8x region keeps its alias floor at three and twelve times its window.
 * @author Mustafa Garip
 */

#include <cstdio>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/circuits/stages/triode_stage.hpp>
#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/math/oversampler.hpp>
#include <SushiDSP/foundation/regression_test/regression_test.hpp>
#include <SushiDSP/foundation/signal_measure/alias_floor.hpp>

namespace
{
    /// @brief One TriodeStage inside its own 8x region, the input scaled to a peak grid drive in
    /// volts and the output back to full scale, as the DA5 diagnosis measures a stage.
    class OversampledTriodeStageNode : public SushiDSP::AudioNode
    {
        public:
            /// @param stage The stage under test.
            /// @param drive_volts_peak The grid drive a full-scale -12 dBFS stimulus reaches.
            /// @param plate_scale The output divisor that brings the plate back inside full scale.
            OversampledTriodeStageNode(SushiDSP::TriodeStage stage, float drive_volts_peak,
                                       float plate_scale)
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
            SushiDSP::TriodeStage stage_;

            /// @brief Volts of grid drive per full-scale unit of stimulus.
            float drive_;

            /// @brief The factor that brings the plate swing back inside full scale.
            float output_scale_;

            /// @brief The 8x region the stage runs in.
            std::unique_ptr<SushiDSP::Oversampler8x> oversampler_;

            /// @brief The oversampled block between the stage and the decimator.
            std::vector<float> scratch_;
    };
} // namespace

SUSHIDSP_REGRESSION_TEST(Small_TriodeStage, TriodeStageAliasFloorAtThreeAndTwelveTimesItsWindow)
{
    // V5A alone folds under -60 dB at 6 V and -40 dB at 24 V. See tests/foundation/circuits/stages/test_triode_stage/README.md.
    auto v5a = []
    {
        return SushiDSP::TriodeStage(671730.0f, 1000000.0f, -2.0f, 25.3f,
                                     SushiDSP::K_TRIODE12_AX7_GRID_CONDUCTION,
                                     SushiDSP::PlateSwing{90.742f, 134.001f});
    };
    OversampledTriodeStageNode three_times(v5a(), 6.0f, 134.0f);
    OversampledTriodeStageNode twelve_times(v5a(), 24.0f, 134.0f);

    const double at_three_times = SushiDSP::measure::alias_floor(three_times, 48000.0);
    const double at_twelve_times = SushiDSP::measure::alias_floor(twelve_times, 48000.0);
    std::printf(
        "[ measure ] TriodeStage V5A alias floor %.1f dB at 3x its window, %.1f dB at 12x\n",
        at_three_times, at_twelve_times);
    EXPECT_LT(at_three_times, -60.0);
    EXPECT_LT(at_twelve_times, -40.0);
}
