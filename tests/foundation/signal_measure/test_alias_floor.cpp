/**************************************************************************/
/* test_alias_floor.cpp                                                   */
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
 * @file test_alias_floor.cpp
 * @brief Tests that the alias floor sits at the analysis floor for a linear node and counts folded harmonics as aliases.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/signal_measure/alias_floor.hpp>

#include <cmath>
#include <cstdint>

namespace
{
    /// @brief y = g x, one input and one output; at unity it is the pass-through reference.
    class GainNode : public SushiDSP::AudioNode
    {
        public:
            /// @brief Builds the node with its gain.
            explicit GainNode(float gain) : gain_(gain)
            {
            }

            /// @brief Holds no state, so there is nothing to prepare.
            void prepare(const SushiDSP::PrepareInfo&) override
            {
            }

            /// @brief Applies the transfer to every frame of the block.
            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t i = 0; i < context.frames; ++i)
                {
                    const float x = context.inputs[0][i];
                    context.outputs[0][i] = gain_ * x;
                }
            }

            /// @brief Reports one input channel and one output channel.
            SushiDSP::PortLayout ports() const override
            {
                return {1, 1};
            }

            /// @brief Exposes no parameter, so every read is zero.
            float get_parameter(const char*) const override
            {
                return 0.0f;
            }

            /// @brief Exposes no parameter, so every write is ignored.
            void set_parameter_for_control_thread(const char*, float) override
            {
            }

            /// @brief Holds no state, so there is nothing to clear.
            void reset() override
            {
            }

        private:
            /// @brief The gain applied to every sample.
            float gain_;
    };

    /// @brief y = tanh(g x): a known nonlinearity whose harmonic series is easy to bound.
    class TanhNode : public SushiDSP::AudioNode
    {
        public:
            /// @brief Builds the node with its gain.
            explicit TanhNode(float gain) : gain_(gain)
            {
            }

            /// @brief Holds no state, so there is nothing to prepare.
            void prepare(const SushiDSP::PrepareInfo&) override
            {
            }

            /// @brief Applies the transfer to every frame of the block.
            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t i = 0; i < context.frames; ++i)
                {
                    const float x = context.inputs[0][i];
                    context.outputs[0][i] = std::tanh(gain_ * x);
                }
            }

            /// @brief Reports one input channel and one output channel.
            SushiDSP::PortLayout ports() const override
            {
                return {1, 1};
            }

            /// @brief Exposes no parameter, so every read is zero.
            float get_parameter(const char*) const override
            {
                return 0.0f;
            }

            /// @brief Exposes no parameter, so every write is ignored.
            void set_parameter_for_control_thread(const char*, float) override
            {
            }

            /// @brief Holds no state, so there is nothing to clear.
            void reset() override
            {
            }

        private:
            /// @brief The drive into the tanh.
            float gain_;
    };
} // namespace

TEST(Small_AliasFloor, IdentityIsAtTheAnalysisFloor)
{
    GainNode node(1.0f);
    EXPECT_LE(SushiDSP::measure::alias_floor(node, 48000.0), -110.0);
}

TEST(Small_AliasFloor, UnoversampledTanhIsPoor)
{
    // Folded harmonics count as aliases. See tests/foundation/signal_measure/README.md.
    TanhNode node(4.0f);
    EXPECT_GT(SushiDSP::measure::alias_floor(node, 48000.0), -40.0);
}
