/**************************************************************************/
/* test_behaviour.cpp                                                     */
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
 * @file test_behaviour.cpp
 * @brief Tests the TubeScreamer node's parameters, levels, smoothing and latency.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log_capture/log_capture.hpp>
#include <SushiDSP/foundation/math/biquad.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>
#include <SushiDSP/nodes/tubescreamer/tube_screamer.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    constexpr double K_RATE = 48000.0;
    constexpr std::uint32_t K_BLOCK = 256u;

    /// @brief Characterized: a node at level -60 dB scales its settled output by 10^(-60/20) = 1e-3,
    /// and the clip chain keeps the pre-level signal under one full scale at the stimulus below.
    constexpr double K_MINIMUM_LEVEL_BOUND = 1e-3;

    /// @brief Characterized: the +12 dB trim must at least double the settled peak. The linear ratio
    /// is 3.98; the clipper compresses it, and half of it is the floor.
    constexpr double K_TRIM_BOOST_MIN_RATIO = 2.0;

    /// @brief Characterized: the -12 dB trim must at most halve the settled peak. The linear ratio
    /// is 0.251; the clipper keeps the small-signal ratio near it, and 0.5 is the ceiling.
    constexpr double K_TRIM_CUT_MAX_RATIO = 0.5;

    /// @brief The relative bound on the 63 % crossing time between two block sizes. 5 % of the 20 ms
    /// smoothing time constant is 1 ms; the 4 kHz probe locates the crossing to half a period,
    /// 0.125 ms (README).
    constexpr double K_CROSSING_RELATIVE_BOUND = 0.05;

    /// @brief Characterized: the group delay the two DK blocks add past the oversampler's latency,
    /// in samples (README, the D6c plan task 8).
    constexpr std::uint32_t K_MAX_BLOCK_GROUP_DELAY_SAMPLES = 4u;

    /// @brief Characterized: the bright tone must carry at least this multiple of the dark tone's
    /// energy above 2 kHz.
    constexpr double K_TONE_BRIGHT_TO_DARK_MIN_RATIO = 2.0;

    /// @brief Characterized: the band, in dBFS, a -12 dBFS 82 Hz sine at default settings must land
    /// in. The README puts the calibrated chain within a couple of dB of -12 dBFS.
    constexpr float K_DEFAULT_PEAK_LOW_DBFS = -14.0f;
    constexpr float K_DEFAULT_PEAK_HIGH_DBFS = -10.0f;

    /// @brief Characterized: the 30 Hz to 10 Hz RMS ratio must stay under this. The chain measures
    /// 1.11 and a second-order 20 Hz high-pass would give 3.9 (README).
    constexpr double K_LOW_END_RATIO_CEILING = 1.5;

    /// @brief Characterized: a drive sweep may step no more than this multiple of the largest first
    /// difference of the loudest still render.
    constexpr double K_SWEEP_STEP_MAX_RATIO = 2.0;

    /// @brief Returns whether every sample is finite.
    bool all_finite(const std::vector<float>& samples)
    {
        return std::all_of(samples.begin(), samples.end(), [](float sample) { return std::isfinite(sample); });
    }

    /// @brief Returns the largest magnitude from a sample index on.
    double peak_from(const std::vector<float>& samples, std::size_t first)
    {
        double peak = 0.0;
        for (std::size_t i = first; i < samples.size(); ++i)
        {
            peak = std::max(peak, std::fabs(static_cast<double>(samples[i])));
        }
        return peak;
    }

    /// @brief Returns the RMS of `count` samples from a sample index on.
    double rms_of(const std::vector<float>& samples, std::size_t first, std::size_t count)
    {
        double sum_squares = 0.0;
        for (std::size_t i = first; i < first + count; ++i)
        {
            sum_squares += static_cast<double>(samples[i]) * samples[i];
        }
        return std::sqrt(sum_squares / static_cast<double>(count));
    }

    /// @brief Streams a sine through a node block by block, keeping the phase across blocks.
    class SineStream
    {
        public:
            SineStream(double frequency_hz, double amplitude, std::uint32_t block)
                : frequency_hz_(frequency_hz), amplitude_(amplitude), input_(block, 0.0f), output_(block, 0.0f)
            {
            }

            /// @brief Runs the next block and returns the node's output for it.
            const std::vector<float>& next(SushiDSP::TubeScreamer& node)
            {
                for (std::size_t i = 0; i < input_.size(); ++i)
                {
                    input_[i] = static_cast<float>(amplitude_ * std::sin(2.0 * M_PI * frequency_hz_ * (index_ + static_cast<double>(i)) / K_RATE));
                }
                index_ += static_cast<double>(input_.size());
                const float* inputs[1] = {input_.data()};
                float* outputs[1] = {output_.data()};
                SushiDSP::ProcessContext context{outputs, inputs, static_cast<std::uint32_t>(input_.size()), 0.0};
                node.process(context);
                return output_;
            }

        private:
            double frequency_hz_;
            double amplitude_;
            double index_ = 0.0;
            std::vector<float> input_;
            std::vector<float> output_;
    };

    /// @brief Sets the four knobs; set before prepare(), the smoothers snap to them.
    void configure_node(SushiDSP::TubeScreamer& node, float drive, float tone, float level_db, float trim_db = 0.0f)
    {
        node.set_drive(drive);
        node.set_tone(tone);
        node.set_level(level_db);
        node.set_input_trim(trim_db);
    }

    /// @brief Renders a node over a stimulus at the run rate and the test block size.
    std::vector<float> render_node(SushiDSP::TubeScreamer& node, const std::vector<float>& stimulus)
    {
        return SushiDSP::measure::render(node, stimulus, K_RATE, K_BLOCK);
    }

    /// @brief Returns the settled peak of a 440 Hz, 0.02 sine at drive 0 for an input trim.
    double settled_peak_at_trim(float trim_db)
    {
        SushiDSP::TubeScreamer node;
        configure_node(node, 0.0f, 0.5f, 0.0f, trim_db);
        const std::vector<float> output = render_node(node, SushiDSP::measure::sine_stimulus(440.0, K_RATE, 40u * K_BLOCK, 0.02f));
        return peak_from(output, 20u * K_BLOCK);
    }

    /// @brief Returns when, in milliseconds after a level step from -60 to 0 dB, the output first
    /// passes 63 % of its settled peak, or -1 when it never does within a second.
    double level_step_crossing_ms(std::uint32_t block)
    {
        constexpr double K_PROBE_HZ = 4000.0;
        constexpr double K_PROBE_AMPLITUDE = 0.02;
        const std::uint32_t one_second_of_blocks = (48000u + block - 1u) / block;

        SushiDSP::TubeScreamer settled;
        settled.prepare(SushiDSP::PrepareInfo{K_RATE, block, 1});
        settled.set_parameter_for_control_thread("level", 0.0f);
        SineStream settled_stream(K_PROBE_HZ, K_PROBE_AMPLITUDE, block);
        std::vector<float> settled_block;
        for (std::uint32_t i = 0; i < one_second_of_blocks; ++i)
        {
            settled_block = settled_stream.next(settled);
        }
        const double final_peak = peak_from(settled_block, 0);

        SushiDSP::TubeScreamer stepping;
        stepping.prepare(SushiDSP::PrepareInfo{K_RATE, block, 1});
        stepping.set_parameter_for_control_thread("level", -60.0f);
        SineStream stepping_stream(K_PROBE_HZ, K_PROBE_AMPLITUDE, block);
        for (std::uint32_t i = 0; i < one_second_of_blocks; ++i)
        {
            stepping_stream.next(stepping);
        }

        stepping.set_parameter_for_control_thread("level", 0.0f);
        double elapsed_samples = 0.0;
        for (std::uint32_t i = 0; i < one_second_of_blocks; ++i)
        {
            const std::vector<float>& output = stepping_stream.next(stepping);
            for (std::uint32_t sample = 0; sample < block; ++sample)
            {
                if (std::fabs(static_cast<double>(output[sample])) > 0.63 * final_peak)
                {
                    return (elapsed_samples + sample) / (K_RATE / 1000.0);
                }
            }
            elapsed_samples += block;
        }
        return -1.0;
    }

    /// @brief Returns the index of the output peak of a 0.001 impulse over two blocks.
    std::uint32_t impulse_peak_index(SushiDSP::TubeScreamer& node, std::uint32_t block)
    {
        std::vector<float> input(block, 0.0f);
        std::vector<float> output(block, 0.0f);
        input[0] = 0.001f;
        const float* inputs[1] = {input.data()};
        float* outputs[1] = {output.data()};

        std::vector<float> observed;
        for (int b = 0; b < 2; ++b)
        {
            SushiDSP::ProcessContext context{outputs, inputs, block, 0.0};
            node.process(context);
            observed.insert(observed.end(), output.begin(), output.end());
            std::fill(input.begin(), input.end(), 0.0f);
        }

        std::size_t peak_index = 0;
        for (std::size_t i = 1; i < observed.size(); ++i)
        {
            if (std::fabs(observed[i]) > std::fabs(observed[peak_index]))
            {
                peak_index = i;
            }
        }
        return static_cast<std::uint32_t>(peak_index);
    }

    /// @brief Returns the RMS deviation of the settled half of a 220 Hz render from its best-fit
    /// sine, over the render's own RMS: a proxy for THD that grows with clipping severity.
    double thd_proxy_at_drive(float drive)
    {
        constexpr double K_FREQUENCY = 220.0;
        SushiDSP::TubeScreamer node;
        configure_node(node, drive, 0.5f, 0.0f);
        const std::vector<float> output = render_node(node, SushiDSP::measure::sine_stimulus(K_FREQUENCY, K_RATE, 30u * K_BLOCK, 0.2f));

        const std::size_t settle_start = output.size() / 2;
        double sum_squares = 0.0, sum_sin = 0.0, sum_cos = 0.0;
        for (std::size_t i = settle_start; i < output.size(); ++i)
        {
            const double phase = 2.0 * M_PI * K_FREQUENCY * static_cast<double>(i) / K_RATE;
            sum_squares += static_cast<double>(output[i]) * output[i];
            sum_sin += output[i] * std::sin(phase);
            sum_cos += output[i] * std::cos(phase);
        }
        const double n = static_cast<double>(output.size() - settle_start);
        const double fundamental_amplitude = 2.0 * std::sqrt(sum_sin * sum_sin + sum_cos * sum_cos) / n;
        const double fundamental_power = 0.5 * fundamental_amplitude * fundamental_amplitude * n;
        const double residual_power = std::max(0.0, sum_squares - fundamental_power);
        return std::sqrt(residual_power / std::max(1e-12, sum_squares));
    }

    /// @brief Returns the RMS above 2 kHz of a 100 Hz plus 5 kHz render at drive 0.2.
    double high_frequency_rms_at_tone(float tone)
    {
        SushiDSP::TubeScreamer node;
        configure_node(node, 0.2f, tone, 0.0f);
        std::vector<float> stimulus(40u * K_BLOCK);
        for (std::size_t i = 0; i < stimulus.size(); ++i)
        {
            const double t = static_cast<double>(i) / K_RATE;
            stimulus[i] = static_cast<float>(0.15 * std::sin(2.0 * M_PI * 100.0 * t) + 0.15 * std::sin(2.0 * M_PI * 5000.0 * t));
        }
        const std::vector<float> output = render_node(node, stimulus);

        SushiDSP::Biquad reference_highpass;
        reference_highpass.set_high_pass(K_RATE, 2000.0, 0.707);
        std::vector<float> filtered(output.size());
        for (std::size_t i = 0; i < output.size(); ++i)
        {
            filtered[i] = reference_highpass.process_sample(output[i]);
        }
        return rms_of(filtered, 20u * K_BLOCK, filtered.size() - 20u * K_BLOCK);
    }

    /// @brief Returns the settled RMS of a 0.01 sine at a frequency, over a whole number of cycles.
    ///
    /// The window is truncated to whole cycles of the tone: blocks 100 to 199 hold 25600 samples,
    /// 16 cycles at 30 Hz but 5.33 at 10 Hz, and the partial cycle biased the 10 Hz RMS low by 1.2 %.
    double settled_rms_at_frequency(double frequency)
    {
        SushiDSP::TubeScreamer node;
        configure_node(node, 0.0f, 0.5f, 0.0f);
        const std::vector<float> output = render_node(node, SushiDSP::measure::sine_stimulus(frequency, K_RATE, 200u * K_BLOCK, 0.01f));
        const std::size_t cycle_samples = static_cast<std::size_t>(K_RATE / frequency);
        const std::size_t window = 25600u / cycle_samples * cycle_samples;
        return rms_of(output, 100u * K_BLOCK, window);
    }

    /// @brief Returns the largest sample-to-sample step of a 220 Hz render from block 10 on, with
    /// the drive knob either swept 0 to 1 over a second or held at 1.
    double largest_first_difference(bool sweeping)
    {
        constexpr std::uint32_t K_BLOCKS_PER_SECOND = 48000u / K_BLOCK;
        SushiDSP::TubeScreamer node;
        configure_node(node, sweeping ? 0.0f : 1.0f, 0.5f, 0.0f);
        node.prepare(SushiDSP::PrepareInfo{K_RATE, K_BLOCK, 1});
        SineStream stream(220.0, 0.1, K_BLOCK);
        double largest = 0.0;
        float previous = 0.0f;
        for (std::uint32_t b = 0; b < K_BLOCKS_PER_SECOND; ++b)
        {
            if (sweeping)
            {
                node.set_drive(static_cast<float>(b) / static_cast<float>(K_BLOCKS_PER_SECOND - 1u));
            }
            for (const float sample : stream.next(node))
            {
                if (b >= 10)
                {
                    largest = std::max(largest, std::fabs(static_cast<double>(sample - previous)));
                }
                previous = sample;
            }
        }
        return largest;
    }

    /// @brief Returns the peak, in dBFS, of a -12 dBFS 82 Hz sine through a default node, after
    /// 20 blocks of settling.
    float default_node_peak_dbfs()
    {
        SushiDSP::TubeScreamer node;
        const std::vector<float> output = render_node(node, SushiDSP::measure::sine_stimulus(82.0, K_RATE, 188u * K_BLOCK, 0.251f));
        return 20.0f * std::log10(static_cast<float>(peak_from(output, 21u * K_BLOCK)));
    }

    /// @brief Calls prepare() and swallows the std::runtime_error a failed derivation throws.
    void prepare_ignoring_failure(SushiDSP::TubeScreamer& node, const SushiDSP::PrepareInfo& info)
    {
        try
        {
            node.prepare(info);
        }
        catch (const std::runtime_error&)
        {
        }
    }

    /// @brief Names a stimulus and the knobs it is rendered under.
    struct FiniteRow
    {
            const char* name;
            float drive;
            float tone;
            float level_db;
            float trim_db;
            bool dc;
            float amplitude;
            std::uint32_t blocks;
    };

    /// @brief Builds the stimulus of a finite row: a 220 Hz sine, or a constant for a DC row.
    std::vector<float> finite_row_stimulus(const FiniteRow& row)
    {
        const std::size_t length = static_cast<std::size_t>(row.blocks) * K_BLOCK;
        return row.dc ? std::vector<float>(length, row.amplitude) : SushiDSP::measure::sine_stimulus(220.0, K_RATE, length, row.amplitude);
    }

    /// @brief Names a parameter by index with the id and kind the contract gives it.
    struct DescriptorRow
    {
            const char* name;
            std::uint16_t index;
            const char* id;
            SushiDSP::ParamKind kind;
    };

    /// @brief Names a parameter and a value for the by-index against by-id comparison.
    struct IndexRow
    {
            const char* name;
            std::uint16_t index;
            const char* id;
            float value;
    };

    /// @brief Names a setter, the id it is read back by, a request and the clamped value.
    struct ClampRow
    {
            const char* name;
            void (SushiDSP::TubeScreamer::*setter)(float);
            const char* id;
            float requested;
            float expected;
    };

    /// @brief Names a parameter and a value that must read back as written.
    struct RoundTripRow
    {
            const char* name;
            const char* id;
            float value;
    };

    /// @brief Names a tone position.
    struct ToneRow
    {
            const char* name;
            float tone;
    };

    /// @brief Returns the row's name, for the parameterised test names.
    template <typename Row>
    std::string row_name(const ::testing::TestParamInfo<Row>& info)
    {
        return std::string(info.param.name);
    }
} // namespace

class Small_TubeScreamerOutputFinite : public ::testing::TestWithParam<FiniteRow>
{
};

TEST_P(Small_TubeScreamerOutputFinite, TubeScreamerOutputStaysFiniteForTheRow)
{
    const FiniteRow& row = GetParam();
    SushiDSP::TubeScreamer node;
    configure_node(node, row.drive, row.tone, row.level_db, row.trim_db);

    EXPECT_TRUE(all_finite(render_node(node, finite_row_stimulus(row))));
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_TubeScreamerOutputFinite,
                         ::testing::Values(FiniteRow{"SineAtMidSettings", 0.5f, 0.5f, 0.0f, 0.0f, false, 0.3f, 20},
                                           FiniteRow{"DcAtHighDrive", 1.0f, 0.5f, 0.0f, 0.0f, true, 0.1f, 200},
                                           FiniteRow{"InputTrimAtMinimum", 0.5f, 0.5f, 0.0f, -24.0f, false, 0.3f, 20},
                                           FiniteRow{"InputTrimAtMaximum", 0.5f, 0.5f, 0.0f, 24.0f, false, 0.3f, 20},
                                           FiniteRow{"LevelAtMinimum", 0.5f, 0.5f, -60.0f, 0.0f, false, 0.3f, 20},
                                           FiniteRow{"LevelAtMaximum", 0.5f, 0.5f, 12.0f, 0.0f, false, 0.3f, 20}),
                         row_name<FiniteRow>);

TEST(Small_TubeScreamer, TubeScreamerMinimumLevelProducesNearSilenceAfterSettling)
{
    SushiDSP::TubeScreamer node;
    configure_node(node, 0.5f, 0.5f, -60.0f);

    const std::vector<float> output = render_node(node, std::vector<float>(20u * K_BLOCK, 0.2f));

    EXPECT_LE(peak_from(output, 19u * K_BLOCK), K_MINIMUM_LEVEL_BOUND);
}

class Small_TubeScreamerDescriptor : public ::testing::TestWithParam<DescriptorRow>
{
};

TEST_P(Small_TubeScreamerDescriptor, TubeScreamerDescriptorListsTheParameterAtItsContractIndex)
{
    const DescriptorRow& row = GetParam();
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::tube_screamer_descriptor();

    ASSERT_LT(row.index, descriptor.parameter_count);
    EXPECT_STREQ(descriptor.parameters[row.index].id, row.id);
    EXPECT_EQ(descriptor.parameters[row.index].kind, row.kind);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_TubeScreamerDescriptor,
                         ::testing::Values(DescriptorRow{"Drive", 0, "drive", SushiDSP::ParamKind::Linear},
                                           DescriptorRow{"Tone", 1, "tone", SushiDSP::ParamKind::Linear},
                                           DescriptorRow{"Level", 2, "level", SushiDSP::ParamKind::Decibel},
                                           DescriptorRow{"InputTrim", 3, "input_trim", SushiDSP::ParamKind::Decibel}),
                         row_name<DescriptorRow>);

class Small_TubeScreamerIndexedParameter : public ::testing::TestWithParam<IndexRow>
{
};

TEST_P(Small_TubeScreamerIndexedParameter, TubeScreamerIndexSetParameterMatchesStringSetParameter)
{
    const IndexRow& row = GetParam();
    SushiDSP::TubeScreamer by_name, by_index;

    by_name.set_parameter_for_control_thread(row.id, row.value);
    by_index.set_parameter_for_audio_thread(row.index, row.value);

    EXPECT_EQ(by_name.get_parameter(row.id), by_index.get_parameter(row.id));
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_TubeScreamerIndexedParameter,
                         ::testing::Values(IndexRow{"Drive", 0, "drive", 0.8f}, IndexRow{"Tone", 1, "tone", 0.8f},
                                           IndexRow{"Level", 2, "level", -6.0f}, IndexRow{"InputTrim", 3, "input_trim", -6.0f}),
                         row_name<IndexRow>);

class Small_TubeScreamerClamp : public ::testing::TestWithParam<ClampRow>
{
};

TEST_P(Small_TubeScreamerClamp, TubeScreamerSetterClampsToTheDescriptorRange)
{
    const ClampRow& row = GetParam();
    SushiDSP::TubeScreamer node;

    (node.*row.setter)(row.requested);

    EXPECT_EQ(node.get_parameter(row.id), row.expected);
}

// The ranges are the descriptor's: level -60 to +12 dB, input trim -24 to +24 dB.
INSTANTIATE_TEST_SUITE_P(Rows, Small_TubeScreamerClamp,
                         ::testing::Values(ClampRow{"LevelAboveRange", &SushiDSP::TubeScreamer::set_level, "level", 99.0f, 12.0f},
                                           ClampRow{"LevelBelowRange", &SushiDSP::TubeScreamer::set_level, "level", -200.0f, -60.0f},
                                           ClampRow{"InputTrimAboveRange", &SushiDSP::TubeScreamer::set_input_trim, "input_trim", 99.0f, 24.0f},
                                           ClampRow{"InputTrimBelowRange", &SushiDSP::TubeScreamer::set_input_trim, "input_trim", -99.0f, -24.0f}),
                         row_name<ClampRow>);

TEST(Small_TubeScreamer, TubeScreamerInputTrimBoostRaisesTheSettledPeak)
{
    EXPECT_GT(settled_peak_at_trim(12.0f), settled_peak_at_trim(0.0f) * K_TRIM_BOOST_MIN_RATIO);
}

TEST(Small_TubeScreamer, TubeScreamerInputTrimCutLowersTheSettledPeak)
{
    EXPECT_LT(settled_peak_at_trim(-12.0f), settled_peak_at_trim(0.0f) * K_TRIM_CUT_MAX_RATIO);
}

TEST(Medium_TubeScreamer, TubeScreamerSmoothingTimeIsBlockSizeIndependent)
{
    // The 63 % crossing must not depend on the block size. See tests/nodes/tubescreamer/test_tube_screamer/README.md.
    const double at_64 = level_step_crossing_ms(64);
    const double at_4096 = level_step_crossing_ms(4096);

    ASSERT_GT(at_64, 0.0);
    ASSERT_GT(at_4096, 0.0);
    EXPECT_NEAR(at_64, at_4096, K_CROSSING_RELATIVE_BOUND * at_64);
}

TEST(Small_TubeScreamer, TubeScreamerLatencyIsZeroBeforePrepare)
{
    SushiDSP::TubeScreamer unprepared;

    EXPECT_EQ(unprepared.latency_samples(), 0u);
}

class Small_TubeScreamerLatency : public ::testing::TestWithParam<ToneRow>
{
};

TEST_P(Small_TubeScreamerLatency, TubeScreamerImpulsePeakLandsWithinTheBlocksPhaseLagPastTheReportedLatency)
{
    // Only the 4x oversampler's round trip is latency. See tests/nodes/tubescreamer/test_tube_screamer/README.md.
    constexpr std::uint32_t K_LATENCY_BLOCK = 64u;
    SushiDSP::TubeScreamer node;
    configure_node(node, 0.0f, GetParam().tone, 0.0f);
    node.prepare(SushiDSP::PrepareInfo{K_RATE, K_LATENCY_BLOCK, 1});

    const std::uint32_t peak = impulse_peak_index(node, K_LATENCY_BLOCK);

    EXPECT_GE(peak, node.latency_samples());
    EXPECT_LE(peak, node.latency_samples() + K_MAX_BLOCK_GROUP_DELAY_SAMPLES);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_TubeScreamerLatency,
                         ::testing::Values(ToneRow{"DarkTone", 0.0f}, ToneRow{"MidTone", 0.5f}, ToneRow{"BrightTone", 1.0f}),
                         row_name<ToneRow>);

TEST(Small_TubeScreamer, TubeScreamerThdIncreasesWithDrive)
{
    // The THD proxy is RMS deviation from a fitted sine. See tests/nodes/tubescreamer/test_tube_screamer/README.md.
    EXPECT_LT(thd_proxy_at_drive(0.05f), thd_proxy_at_drive(1.0f));
}

TEST(Small_TubeScreamer, TubeScreamerToneControlsHighFrequencyBalance)
{
    EXPECT_GT(high_frequency_rms_at_tone(1.0f), high_frequency_rms_at_tone(0.0f) * K_TONE_BRIGHT_TO_DARK_MIN_RATIO);
}

class Small_TubeScreamerParameterRoundTrip : public ::testing::TestWithParam<RoundTripRow>
{
};

TEST_P(Small_TubeScreamerParameterRoundTrip, TubeScreamerGenericParameterReadsBackWhatWasWritten)
{
    const RoundTripRow& row = GetParam();
    SushiDSP::TubeScreamer node;

    node.set_parameter_for_control_thread(row.id, row.value);

    EXPECT_EQ(node.get_parameter(row.id), row.value);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_TubeScreamerParameterRoundTrip,
                         ::testing::Values(RoundTripRow{"Drive", "drive", 0.25f}, RoundTripRow{"Tone", "tone", 0.75f},
                                           RoundTripRow{"Level", "level", -3.0f}, RoundTripRow{"InputTrim", "input_trim", 6.0f}),
                         row_name<RoundTripRow>);

TEST(Small_TubeScreamer, TubeScreamerUnknownParameterIsSafeNoOp)
{
    SushiDSP::TubeScreamer node;
    node.set_parameter_for_control_thread("does_not_exist", 1.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("does_not_exist"), 0.0f);
}

TEST(Medium_TubeScreamer, TubeScreamerDefaultSettingsPeakNearMinusTwelveDbfsForAMinusTwelveDbfsInput)
{
    // At default drive the pedal is near unity at 82 Hz. See tests/nodes/tubescreamer/test_tube_screamer/README.md.
    const float peak_decibels = default_node_peak_dbfs();

    EXPECT_GT(peak_decibels, K_DEFAULT_PEAK_LOW_DBFS) << "measured peak " << peak_decibels << " dBFS";
    EXPECT_LT(peak_decibels, K_DEFAULT_PEAK_HIGH_DBFS) << "measured peak " << peak_decibels << " dBFS";
}

TEST(Medium_TubeScreamer, TubeScreamerInputResponseIsFlatBetweenTenAndThirtyHertz)
{
    // The input corner is 0.312069 Hz, so 20 Hz is flat. See tests/nodes/tubescreamer/test_tube_screamer/README.md.
    const double rms_at_30_hz = settled_rms_at_frequency(30.0);
    const double rms_at_10_hz = settled_rms_at_frequency(10.0);

    EXPECT_LT(rms_at_30_hz / rms_at_10_hz, K_LOW_END_RATIO_CEILING) << "30 Hz " << rms_at_30_hz << ", 10 Hz " << rms_at_10_hz;
}

TEST(Medium_TubeScreamer, TubeScreamerDriveSweepStepsNoMoreThanTwiceTheLoudestStillRender)
{
    // A drive sweep must not produce a sample-to-sample step. See tests/nodes/tubescreamer/test_tube_screamer/README.md.
    const double still = largest_first_difference(false);
    const double swept = largest_first_difference(true);

    EXPECT_LT(swept, still * K_SWEEP_STEP_MAX_RATIO) << "swept " << swept << ", still " << still;
}

TEST(Small_TubeScreamer, TubeScreamerPrepareThrowsWhenTheDerivationFails)
{
    SushiDSP::TubeScreamer node;

    EXPECT_THROW(node.prepare(SushiDSP::PrepareInfo{0.0, K_BLOCK, 1}), std::runtime_error);
}

TEST(Small_TubeScreamer, TubeScreamerPrepareLogsOneErrorWhenTheDerivationFails)
{
    SushiDSP::TubeScreamer node;
    SushiDSP::LogCapture capture;

    prepare_ignoring_failure(node, SushiDSP::PrepareInfo{0.0, K_BLOCK, 1});

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Error);
    EXPECT_EQ(lines[0].category, "tubescreamer");
    EXPECT_NE(lines[0].message.find("prepare() throws"), std::string::npos) << lines[0].message;
}
