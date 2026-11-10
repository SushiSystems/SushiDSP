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
 * @file test_frequency_response.cpp
 * @brief Tests the measured frequency response against flat, gain and closed-form first-order references.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/signal_measure/frequency_response.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
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

            /// @brief Returns the exact magnitude of the difference equation, in decibels.
            /// @param frequency_hz Frequency to evaluate at.
            /// @param sample_rate Sample rate in hertz.
            static double magnitude_db(double frequency_hz, double sample_rate)
            {
                const double a = std::exp(-2.0 * K_PI * 1000.0 / sample_rate);
                const double w = 2.0 * K_PI * frequency_hz / sample_rate;
                return 20.0 * std::log10((1.0 - a) / std::hypot(1.0 - a * std::cos(w), a * std::sin(w)));
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

    /// @brief The sweep length every case runs, in seconds: a quarter second of tail follows it.
    /// The default of two seconds took 0.22 to 0.97 s a case; the FFT size follows the total length, so
    /// this one is a quarter of the work. Below it the 20 Hz reading of the highpass case climbs: a
    /// float64 replica of the sweep read 0.119 dB at 0.4 s against 0.043 dB at 0.5 s.
    constexpr double K_SWEEP_SECONDS = 0.5;

    /// @brief The worst error of the short sweep from 20 Hz up, in decibels: a characterised constant.
    /// The Debug run of 2026-11-10 recorded 0.043106 dB as the worst `worst_sweep_error_db` of the four
    /// cases; the bound is that figure plus half of its last decimal.
    constexpr double K_SWEEP_ERROR_DB = 0.043106 + 0.0000005;

    /// @brief The rate of the bottom-of-the-band test: high enough for a 5 Hz sweep to be sampled well.
    constexpr double K_RUN_RATE = 192000.0;

    /// @brief Returns the largest distance of a reading from the expected magnitude at its frequency.
    /// @param response The readings.
    /// @param expected_db Maps a frequency to the magnitude it must read, in decibels.
    template <typename Expected>
    double worst_deviation_db(const std::vector<SushiDSP::measure::ResponsePoint>& response, Expected expected_db)
    {
        double worst = 0.0;
        for (const SushiDSP::measure::ResponsePoint& point : response)
        {
            worst = std::max(worst, std::fabs(point.magnitude_db - expected_db(point.frequency_hz)));
        }
        return worst;
    }

    /// @brief Records the worst deviation as the `worst_sweep_error_db` property, and returns it.
    /// @param worst The worst deviation of the case, in decibels.
    inline double recorded(double worst)
    {
        ::testing::Test::RecordProperty("worst_sweep_error_db", std::to_string(worst));
        return worst;
    }
} // namespace

TEST(Small_FrequencyResponse, IdentityIsFlat)
{
    GainNode node(1.0f);

    const std::vector<SushiDSP::measure::ResponsePoint> response =
        SushiDSP::measure::frequency_response(node, 48000.0, {100.0, 1000.0, 10000.0, 20000.0}, K_SWEEP_SECONDS);

    ASSERT_EQ(response.size(), 4u);
    EXPECT_LE(recorded(worst_deviation_db(response, [](double) { return 0.0; })), K_SWEEP_ERROR_DB);
}

TEST(Small_FrequencyResponse, HalfGainReadsAsMinusSixDecibels)
{
    GainNode node(0.5f);

    const std::vector<SushiDSP::measure::ResponsePoint> points =
        SushiDSP::measure::frequency_response(node, 48000.0, {1000.0}, K_SWEEP_SECONDS);

    ASSERT_EQ(points.size(), 1u);
    EXPECT_LE(recorded(worst_deviation_db(points, [](double) { return 20.0 * std::log10(0.5); })),
              K_SWEEP_ERROR_DB);
}

// The expectations are the difference equation's own |H|. See tests/foundation/signal_measure/README.md.
TEST(Small_FrequencyResponse, TracksAOnePoleLowpass)
{
    OnePoleNode node;

    const std::vector<SushiDSP::measure::ResponsePoint> response =
        SushiDSP::measure::frequency_response(node, 48000.0, {1000.0, 10000.0}, K_SWEEP_SECONDS);

    ASSERT_EQ(response.size(), 2u);
    EXPECT_LE(recorded(worst_deviation_db(response, [](double frequency_hz)
                                          { return OnePoleNode::magnitude_db(frequency_hz, 48000.0); })),
              K_SWEEP_ERROR_DB);
}

// The sweep starts at 5 Hz, two octaves below the band. See tests/foundation/signal_measure/README.md.
TEST(Medium_FrequencyResponse, IsAccurateAtTheBottomOfTheAudioBand)
{
    OnePoleHighPassNode node;

    const std::vector<SushiDSP::measure::ResponsePoint> response =
        SushiDSP::measure::frequency_response(node, K_RUN_RATE, {20.0, 30.0, 50.0, 100.0}, K_SWEEP_SECONDS);

    ASSERT_EQ(response.size(), 4u);
    EXPECT_LE(recorded(worst_deviation_db(response, [](double frequency_hz)
                                          { return OnePoleHighPassNode::magnitude_db(frequency_hz, K_RUN_RATE); })),
              K_SWEEP_ERROR_DB);
}
