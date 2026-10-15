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
 * @brief Tests that one EL34 stage in its own 8x region keeps its alias floor inside and at its window.
 * @author Mustafa Garip
 */

#include <cstdio>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/circuits/stages/pentode_stage.hpp>
#include <SushiDSP/foundation/circuits/stages/power_tube_presets.hpp>
#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/math/oversampler.hpp>
#include <SushiDSP/foundation/regression_test/regression_test.hpp>
#include <SushiDSP/foundation/signal_measure/alias_floor.hpp>

namespace
{
    /// @brief A reference pentode for exercising the stage itself.
    ///
    /// The figures are the Philips EL34 datasheet's class B push-pull point, which is a real
    /// operating point rather than any product's tuning. A brick's test that reached into an
    /// amplifier module would fail whenever that amplifier was recalibrated.
    constexpr SushiDSP::PowerTubePreset K_REFERENCE_PENTODE{
        33831.9f,
        220000.0f,
        -38.0f,
        6.882361f,
        SushiDSP::K_PENTODE_EL34_GRID_CONDUCTION,
        SushiDSP::PlateSwing{25.5f, 280.2777f}};

    /// @brief One PentodeStage inside its own 8x region, shaped as OversampledTriodeStageNode is.
    class OversampledPentodeStageNode : public SushiDSP::AudioNode
    {
        public:
            /// @param stage The stage under test.
            /// @param drive_volts_peak The grid drive a full-scale -12 dBFS stimulus reaches.
            /// @param plate_scale The output divisor that brings the plate back inside full scale.
            OversampledPentodeStageNode(SushiDSP::PentodeStage stage, float drive_volts_peak,
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
            SushiDSP::PentodeStage stage_;

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

SUSHIDSP_REGRESSION_TEST(PentodeStage, PentodeStageAliasFloorInsideAndAtItsWindow)
{
    // The EL34 folds under -80 dB at 8 V and -50 dB at 38 V. See tests/foundation/circuits/stages/test_pentode_stage/README.md.
    OversampledPentodeStageNode inside(SushiDSP::PentodeStage(K_REFERENCE_PENTODE), 8.0f, 300.0f);
    OversampledPentodeStageNode at_window(SushiDSP::PentodeStage(K_REFERENCE_PENTODE), 38.0f,
                                          300.0f);

    const double inside_floor = SushiDSP::measure::alias_floor(inside, 48000.0);
    const double window_floor = SushiDSP::measure::alias_floor(at_window, 48000.0);
    std::printf(
        "[ measure ] PentodeStage EL34 alias floor %.1f dB at 8 V, %.1f dB at its 38 V window\n",
        inside_floor, window_floor);
    EXPECT_LT(inside_floor, -80.0);
    EXPECT_LT(window_floor, -50.0);
}
