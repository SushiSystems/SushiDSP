/**************************************************************************/
/* test_frequency_response.cpp                                            */
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
 * @file test_frequency_response.cpp
 * @brief Tests the measured frequency response against flat, gain and closed-form first-order references.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/signal_measure/frequency_response.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
    /// @brief Pi to double precision, spelled out so the tests do not depend on M_PI.
    constexpr double K_PI = 3.14159265358979323846;

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

    /// @brief First-order lowpass at 1 kHz; its response is the difference equation's own.
    ///
    /// With a = exp(-2*pi*1000/48000), |H(w)| = (1 - a) / |1 - a*exp(-jw)| is -3.004 dB at
    /// 1 kHz and -19.415 dB at 10 kHz.
    class OnePoleNode : public SushiDSP::AudioNode
    {
        public:
            /// @brief Snaps the coefficient to the run rate.
            void prepare(const SushiDSP::PrepareInfo& info) override
            {
                coefficient_ = static_cast<float>(std::exp(-2.0 * K_PI * 1000.0 / info.sample_rate));
            }

            /// @brief Runs the difference equation over one block.
            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t i = 0; i < context.frames; ++i)
                {
                    state_ = (1.0f - coefficient_) * context.inputs[0][i] + coefficient_ * state_;
                    context.outputs[0][i] = state_;
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

            /// @brief Clears the filter memory.
            void reset() override
            {
                state_ = 0.0f;
            }

        private:
            /// @brief The pole, exp(-2 pi fc / fs).
            float coefficient_ = 0.0f;

            /// @brief The previous output.
            float state_ = 0.0f;
    };

    /// @brief First-order highpass at 87 Hz, the corner of the jcm_v1a cathode bypass.
    ///
    /// y[n] = a * (y[n-1] + x[n] - x[n-1]) with a = exp(-2*pi*87/fs), so the exact magnitude
    /// is |H(w)| = a * |1 - exp(-jw)| / |1 - a * exp(-jw)|.
    class OnePoleHighPassNode : public SushiDSP::AudioNode
    {
        public:
            /// @brief Snaps the coefficient to the run rate.
            void prepare(const SushiDSP::PrepareInfo& info) override
            {
                coefficient_ = static_cast<float>(std::exp(-2.0 * K_PI * K_CORNER_HZ / info.sample_rate));
                reset();
            }

            /// @brief Runs the difference equation over one block.
            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t i = 0; i < context.frames; ++i)
                {
                    const float x = context.inputs[0][i];
                    state_ = coefficient_ * (state_ + x - previous_);
                    previous_ = x;
                    context.outputs[0][i] = state_;
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

            /// @brief Clears the filter memory.
            void reset() override
            {
                state_ = 0.0f;
                previous_ = 0.0f;
            }

            /// @brief Returns the exact magnitude of the difference equation, in decibels.
            /// @param frequency_hz Frequency to evaluate at.
            /// @param sample_rate Sample rate in hertz.
            static double magnitude_db(double frequency_hz, double sample_rate)
            {
                const double a = std::exp(-2.0 * K_PI * K_CORNER_HZ / sample_rate);
                const double w = 2.0 * K_PI * frequency_hz / sample_rate;
                const double numerator = std::hypot(1.0 - std::cos(w), std::sin(w));
                const double denominator = std::hypot(1.0 - a * std::cos(w), a * std::sin(w));
                return 20.0 * std::log10(a * numerator / denominator);
            }

        private:
            /// @brief The highpass corner in hertz.
            static constexpr double K_CORNER_HZ = 87.0;

            /// @brief The pole, exp(-2 pi fc / fs).
            float coefficient_{0.0f};

            /// @brief The previous output.
            float state_{0.0f};

            /// @brief The previous input.
            float previous_{0.0f};
    };
} // namespace

TEST(FrequencyResponse, IdentityIsFlat)
{
    GainNode node(1.0f);
    const std::vector<SushiDSP::measure::ResponsePoint> response =
        SushiDSP::measure::frequency_response(node, 48000.0, {100.0, 1000.0, 10000.0, 20000.0});
    ASSERT_EQ(response.size(), 4u);
    for (const SushiDSP::measure::ResponsePoint& point : response)
    {
        EXPECT_NEAR(point.magnitude_db, 0.0, 0.05) << point.frequency_hz;
    }
}

TEST(FrequencyResponse, HalfGainReadsAsMinusSixDecibels)
{
    GainNode node(0.5f);
    const std::vector<SushiDSP::measure::ResponsePoint> points =
        SushiDSP::measure::frequency_response(node, 48000.0, {1000.0});
    ASSERT_EQ(points.size(), 1u);
    EXPECT_NEAR(points[0].magnitude_db, 20.0 * std::log10(0.5), 0.5);
}

TEST(FrequencyResponse, TracksAOnePoleLowpass)
{
    OnePoleNode node;
    const std::vector<SushiDSP::measure::ResponsePoint> response =
        SushiDSP::measure::frequency_response(node, 48000.0, {1000.0, 10000.0});
    ASSERT_EQ(response.size(), 2u);
    // The expectations are the difference equation's own |H|. See tests/foundation/signal_measure/README.md.
    EXPECT_NEAR(response[0].magnitude_db, -3.004, 0.1);
    EXPECT_NEAR(response[1].magnitude_db, -19.415, 0.1);
}

TEST(FrequencyResponse, IsAccurateAtTheBottomOfTheAudioBand)
{
    // The sweep starts at 5 Hz, two octaves below the band. See tests/foundation/signal_measure/README.md.
    constexpr double K_RUN_RATE = 192000.0;

    OnePoleHighPassNode node;
    const std::vector<SushiDSP::measure::ResponsePoint> response =
        SushiDSP::measure::frequency_response(node, K_RUN_RATE, {20.0, 30.0, 50.0, 100.0});
    ASSERT_EQ(response.size(), 4u);
    for (const SushiDSP::measure::ResponsePoint& point : response)
    {
        const double exact = OnePoleHighPassNode::magnitude_db(point.frequency_hz, K_RUN_RATE);
        EXPECT_NEAR(point.magnitude_db, exact, 0.2) << point.frequency_hz << " Hz";
    }
}
