/**************************************************************************/
/* test_rig_adapter.cpp                                                   */
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
 * @file test_rig_adapter.cpp
 * @brief Tests RigAdapter alone: pass-through, preset loading, state and parameters.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "rig_adapter.hpp"
#include "rig_controller.hpp"
#include "rig_parameter.hpp"
#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/rig_graph.hpp>
#include <SushiDSP/foundation/core/rt_counter_report.hpp>

namespace
{
    using SushiDSP::Plugin::RigAdapter;
    using SushiDSP::Plugin::RigParameter;
    using SushiDSP::Plugin::RigStateStatus;

    /// @brief Names the rate every case prepares at.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief Names the block size every case prepares at.
    constexpr std::uint32_t K_BLOCK = 256;

    /// @brief Counts the blocks a rendering case runs.
    constexpr std::size_t K_BLOCKS = 16;

    /// @brief Holds two channels of audio, each K_BLOCKS blocks long.
    using Stereo = std::array<std::vector<float>, 2>;

    /// @brief Builds a repeatable two-channel signal whose channels differ.
    Stereo make_stereo_signal()
    {
        Stereo signal;
        const double step = 2.0 * 3.14159265358979323846 / K_SAMPLE_RATE;
        for (std::size_t channel = 0; channel < signal.size(); ++channel)
        {
            const double frequency = channel == 0 ? 220.0 : 330.0;
            signal[channel].resize(K_BLOCKS * K_BLOCK);
            for (std::size_t frame = 0; frame < signal[channel].size(); ++frame)
            {
                signal[channel][frame] = 0.4f * static_cast<float>(std::sin(
                                                    step * frequency * static_cast<double>(frame)));
            }
        }
        return signal;
    }

    /// @brief Runs a signal through an adapter, one block at a time.
    Stereo render_through_adapter(RigAdapter& adapter, const Stereo& input)
    {
        Stereo output{std::vector<float>(input[0].size(), 0.0f),
                      std::vector<float>(input[1].size(), 0.0f)};
        for (std::size_t offset = 0; offset + K_BLOCK <= input[0].size(); offset += K_BLOCK)
        {
            const float* inputs[2] = {input[0].data() + offset, input[1].data() + offset};
            float* outputs[2] = {output[0].data() + offset, output[1].data() + offset};
            adapter.process(inputs, outputs, RigAdapter::K_CHANNELS, K_BLOCK);
        }
        return output;
    }

    /// @brief Runs a signal through a controller's published graph, one block at a time.
    Stereo render_through_controller(RigController& controller, const Stereo& input)
    {
        Stereo output{std::vector<float>(input[0].size(), 0.0f),
                      std::vector<float>(input[1].size(), 0.0f)};
        for (std::size_t offset = 0; offset + K_BLOCK <= input[0].size(); offset += K_BLOCK)
        {
            const float* inputs[2] = {input[0].data() + offset, input[1].data() + offset};
            float* outputs[2] = {output[0].data() + offset, output[1].data() + offset};
            SushiDSP::ProcessContext context{outputs, inputs, K_BLOCK, 0.0};
            controller.active_graph()->process(context);
        }
        return output;
    }

    /// @brief Saves the preset of a two-pedal rig: a Tube Screamer into a JCM800, drive at 0.77.
    /// @param controller The controller to build the rig in; left holding a freshly built graph.
    /// @return The preset text the standalone host would write.
    std::string build_and_save_rig(RigController& controller)
    {
        const RigController::SlotId screamer = controller.append("sushidsp.tubescreamer");
        controller.append("sushidsp.jcm800");
        controller.set_parameter(screamer, "drive", 0.77f);
        // A publish rebuilds every node at its stored value, which is how a loaded preset starts.
        controller.publish();
        return controller.save_to_json();
    }

    /// @brief Prepares an adapter at the cases' rate and block size and loads a preset into it.
    void assert_prepare_and_load(RigAdapter& adapter, const std::string& json_text)
    {
        std::string error;
        ASSERT_EQ(adapter.prepare(K_SAMPLE_RATE, K_BLOCK, error), RigStateStatus::Ok) << error;
        ASSERT_EQ(adapter.load_state(json_text, error), RigStateStatus::Ok) << error;
    }

    /// @brief Builds the preset of the two-pedal rig in a fresh controller.
    std::string saved_two_pedal_preset()
    {
        SushiDSP::PrepareInfo info{K_SAMPLE_RATE, K_BLOCK, RigAdapter::K_CHANNELS};
        RigController controller(info);
        return build_and_save_rig(controller);
    }

    /// @brief The first K_BLOCK samples of a channel.
    std::vector<float> first_block(const std::vector<float>& channel)
    {
        return std::vector<float>(channel.begin(), channel.begin() + K_BLOCK);
    }

    /// @brief The bound, in full scale, on a sample of the adapter's render against the
    /// controller's. Characterised: the old test's bare literal, with no derivation behind it.
    constexpr float K_ADAPTER_VERSUS_CONTROLLER_BOUND = 1.0e-5f;

    /// @brief Checks that two renders agree sample for sample within a bound.
    /// @return Success, or the first channel and frame that differ by more.
    ::testing::AssertionResult renders_agree(const Stereo& actual, const Stereo& expected, float bound)
    {
        for (std::size_t channel = 0; channel < expected.size(); ++channel)
        {
            for (std::size_t frame = 0; frame < expected[channel].size(); ++frame)
            {
                if (std::abs(actual[channel][frame] - expected[channel][frame]) > bound)
                {
                    return ::testing::AssertionFailure()
                           << "channel " << channel << ", frame " << frame << ": "
                           << actual[channel][frame] << " against " << expected[channel][frame];
                }
            }
        }
        return ::testing::AssertionSuccess();
    }

    /// @brief The largest magnitude in a render.
    float peak_of(const Stereo& render)
    {
        float peak = 0.0f;
        for (const std::vector<float>& channel : render)
        {
            for (const float sample : channel)
            {
                peak = std::max(peak, std::abs(sample));
            }
        }
        return peak;
    }

    /// @brief Checks that the ids of a parameter list rise from the first to the last.
    bool ids_ascend(const std::vector<RigParameter>& parameters)
    {
        for (std::size_t position = 1; position < parameters.size(); ++position)
        {
            if (parameters[position - 1].id >= parameters[position].id)
            {
                return false;
            }
        }
        return true;
    }

    /// @brief Checks that every parameter of one adapter reads the same in another.
    ::testing::AssertionResult same_values(const RigAdapter& original, const RigAdapter& reloaded)
    {
        for (const RigParameter& parameter : original.parameters())
        {
            const std::optional<float> before = original.parameter_value(parameter.id);
            const std::optional<float> after = reloaded.parameter_value(parameter.id);
            if (!before.has_value() || !after.has_value() || *before != *after)
            {
                return ::testing::AssertionFailure() << parameter.module << " " << parameter.name;
            }
        }
        return ::testing::AssertionSuccess();
    }

    /// @brief Queues the same change for the audio thread count times.
    void queue_changes(RigAdapter& adapter, std::size_t count)
    {
        for (std::size_t index = 0; index < count; ++index)
        {
            adapter.queue_parameter_for_audio_thread(1u, 0.5f);
        }
    }

    /// @brief An adapter prepared and loaded with the two-pedal preset.
    class Small_RigAdapterLoaded : public ::testing::Test
    {
        protected:
            void SetUp() override
            {
                assert_prepare_and_load(adapter, saved_two_pedal_preset());
            }

            RigAdapter adapter;
    };

    /// @brief One block that does not fit what the adapter was prepared for.
    struct MisfitRow
    {
            const char* name;
            std::uint32_t channels;
            std::uint32_t frames;
    };

    /// @brief Returns the row's name, for the parameterised test names.
    std::string misfit_row_name(const ::testing::TestParamInfo<MisfitRow>& info)
    {
        return std::string(info.param.name);
    }

    /// @brief Finds the first listed parameter with a given label.
    /// @return The parameter, or std::nullopt when the list holds none by that name.
    std::optional<RigParameter> find_by_name(const RigAdapter& adapter, const std::string& name)
    {
        for (const RigParameter& parameter : adapter.parameters())
        {
            if (parameter.name == name)
            {
                return parameter;
            }
        }
        return std::nullopt;
    }
} // namespace

TEST(Small_RigAdapter, RigAdapterReportsNoLatencyForAnEmptyRig)
{
    RigAdapter adapter;

    EXPECT_EQ(adapter.latency_frames(), 0u);
}

TEST(Small_RigAdapter, RigAdapterWithAnEmptyRigPassesEachChannelThrough)
{
    RigAdapter adapter;
    std::string error;
    ASSERT_EQ(adapter.prepare(K_SAMPLE_RATE, K_BLOCK, error), RigStateStatus::Ok) << error;

    const Stereo input = make_stereo_signal();
    const Stereo output = render_through_adapter(adapter, input);

    EXPECT_EQ(output[0], input[0]);
    EXPECT_EQ(output[1], input[1]);
}

TEST(Small_RigAdapter, RigAdapterWithAnEmptyRigPassesThroughWhenRenderingInPlace)
{
    RigAdapter adapter;
    std::string error;
    ASSERT_EQ(adapter.prepare(K_SAMPLE_RATE, K_BLOCK, error), RigStateStatus::Ok) << error;

    const Stereo input = make_stereo_signal();
    Stereo buffer = input;
    float* channels[2] = {buffer[0].data(), buffer[1].data()};
    adapter.process(channels, channels, RigAdapter::K_CHANNELS, K_BLOCK);

    EXPECT_EQ(first_block(buffer[0]), first_block(input[0]));
    EXPECT_EQ(first_block(buffer[1]), first_block(input[1]));
}

TEST(Small_RigAdapter, RigAdapterRendersWhatTheControllerThatSavedThePresetRenders)
{
    SushiDSP::PrepareInfo info{K_SAMPLE_RATE, K_BLOCK, RigAdapter::K_CHANNELS};
    RigController controller(info);
    const std::string json_text = build_and_save_rig(controller);

    RigAdapter adapter;
    assert_prepare_and_load(adapter, json_text);

    const Stereo input = make_stereo_signal();
    const Stereo expected = render_through_controller(controller, input);
    const Stereo actual = render_through_adapter(adapter, input);

    EXPECT_TRUE(renders_agree(actual, expected, K_ADAPTER_VERSUS_CONTROLLER_BOUND));
    EXPECT_GT(peak_of(actual), 0.0f);
    EXPECT_NE(actual[0], input[0]);
    EXPECT_EQ(adapter.latency_frames(), controller.active_graph()->latency_samples());
}

TEST_F(Small_RigAdapterLoaded, RigAdapterListsTheFirstSlotFirstAndTheLastSlotLast)
{
    const std::vector<RigParameter>& parameters = adapter.parameters();
    ASSERT_FALSE(parameters.empty());

    EXPECT_EQ(SushiDSP::Plugin::rig_parameter_slot(parameters.front().id), 0u);
    EXPECT_EQ(SushiDSP::Plugin::rig_parameter_slot(parameters.back().id), 1u);
}

TEST_F(Small_RigAdapterLoaded, RigAdapterListsItsParametersInRisingIdOrder)
{
    EXPECT_TRUE(ids_ascend(adapter.parameters()));
}

TEST_F(Small_RigAdapterLoaded, RigAdapterListsTheDriveOfTheFirstSlotWithItsRange)
{
    const std::optional<RigParameter> drive = find_by_name(adapter, "Drive");
    ASSERT_TRUE(drive.has_value());

    EXPECT_EQ(drive->id, SushiDSP::Plugin::rig_parameter_id(0, 0));
    EXPECT_EQ(drive->module, "1 Tube Screamer");
    EXPECT_FLOAT_EQ(drive->minimum, 0.0f);
    EXPECT_FLOAT_EQ(drive->maximum, 1.0f);
}

TEST_F(Small_RigAdapterLoaded, RigAdapterReadsTheDriveBackAtTheSavedValue)
{
    const std::optional<RigParameter> drive = find_by_name(adapter, "Drive");
    ASSERT_TRUE(drive.has_value());

    const std::optional<float> value = adapter.parameter_value(drive->id);
    ASSERT_TRUE(value.has_value());
    EXPECT_FLOAT_EQ(*value, 0.77f);
}

TEST(Small_RigAdapter, RigAdapterStateRoundTripsThroughASecondAdapter)
{
    RigAdapter first;
    assert_prepare_and_load(first, saved_two_pedal_preset());
    std::string saved;
    std::string error;
    ASSERT_EQ(first.save_state(saved, error), RigStateStatus::Ok) << error;

    RigAdapter second;
    assert_prepare_and_load(second, saved);
    std::string saved_again;
    ASSERT_EQ(second.save_state(saved_again, error), RigStateStatus::Ok) << error;

    EXPECT_EQ(saved_again, saved);
    ASSERT_EQ(second.parameters().size(), first.parameters().size());
    EXPECT_TRUE(same_values(first, second));
}

TEST(Small_RigAdapter, RigAdapterKeepsItsRigAcrossAChangeOfSampleRate)
{
    SushiDSP::PrepareInfo info{K_SAMPLE_RATE, K_BLOCK, RigAdapter::K_CHANNELS};
    RigController controller(info);
    const std::string json_text = build_and_save_rig(controller);

    RigAdapter adapter;
    assert_prepare_and_load(adapter, json_text);
    const std::size_t parameter_count = adapter.parameters().size();

    std::string error;
    ASSERT_EQ(adapter.prepare(44100.0, 512, error), RigStateStatus::Ok) << error;

    EXPECT_EQ(adapter.parameters().size(), parameter_count);
}

TEST(Small_RigAdapter, RigAdapterReadsBackAParameterSetOnTheControlThread)
{
    SushiDSP::PrepareInfo info{K_SAMPLE_RATE, K_BLOCK, RigAdapter::K_CHANNELS};
    RigController controller(info);
    const std::string json_text = build_and_save_rig(controller);

    RigAdapter adapter;
    assert_prepare_and_load(adapter, json_text);
    const RigParameter parameter = adapter.parameters().front();
    const float target = parameter.minimum + 0.25f * (parameter.maximum - parameter.minimum);

    ASSERT_TRUE(adapter.set_parameter(parameter.id, target));

    const std::optional<float> value = adapter.parameter_value(parameter.id);
    ASSERT_TRUE(value.has_value());
    EXPECT_FLOAT_EQ(*value, target);
}

TEST(Small_RigAdapter, RigAdapterClampsAParameterToItsRange)
{
    SushiDSP::PrepareInfo info{K_SAMPLE_RATE, K_BLOCK, RigAdapter::K_CHANNELS};
    RigController controller(info);
    const std::string json_text = build_and_save_rig(controller);

    RigAdapter adapter;
    assert_prepare_and_load(adapter, json_text);
    const RigParameter parameter = adapter.parameters().front();

    ASSERT_TRUE(adapter.set_parameter(parameter.id, parameter.maximum + 1000.0f));

    const std::optional<float> value = adapter.parameter_value(parameter.id);
    ASSERT_TRUE(value.has_value());
    EXPECT_FLOAT_EQ(*value, parameter.maximum);
}

TEST(Small_RigAdapter, RigAdapterRefusesAParameterTheRigDoesNotHold)
{
    RigAdapter adapter;
    const std::uint32_t missing = SushiDSP::Plugin::rig_parameter_id(3, 0);

    EXPECT_FALSE(adapter.set_parameter(missing, 0.5f));
    EXPECT_FALSE(adapter.parameter_value(missing).has_value());
}

TEST(Small_RigAdapter, RigAdapterAppliesAParameterQueuedOnTheAudioThread)
{
    RigAdapter adapter;
    assert_prepare_and_load(adapter, saved_two_pedal_preset());
    const RigParameter parameter = adapter.parameters().front();
    const float target = parameter.minimum + 0.6f * (parameter.maximum - parameter.minimum);

    adapter.queue_parameter_for_audio_thread(parameter.id, target);
    static_cast<void>(render_through_adapter(adapter, make_stereo_signal()));

    const std::optional<float> value = adapter.parameter_value(parameter.id);
    ASSERT_TRUE(value.has_value());
    EXPECT_FLOAT_EQ(*value, target);
}

TEST(Small_RigAdapter, RigAdapterSavesAParameterQueuedOnTheAudioThread)
{
    RigAdapter adapter;
    assert_prepare_and_load(adapter, saved_two_pedal_preset());
    const RigParameter parameter = adapter.parameters().front();
    const float target = parameter.minimum + 0.6f * (parameter.maximum - parameter.minimum);
    adapter.queue_parameter_for_audio_thread(parameter.id, target);
    static_cast<void>(render_through_adapter(adapter, make_stereo_signal()));

    std::string saved;
    std::string error;
    ASSERT_EQ(adapter.save_state(saved, error), RigStateStatus::Ok) << error;
    RigAdapter reloaded;
    assert_prepare_and_load(reloaded, saved);

    const std::optional<float> carried = reloaded.parameter_value(parameter.id);
    ASSERT_TRUE(carried.has_value());
    EXPECT_FLOAT_EQ(*carried, target);
}

TEST(Small_RigAdapter, RigAdapterAudioThreadParameterChangesWhatTheRigRenders)
{
    SushiDSP::PrepareInfo info{K_SAMPLE_RATE, K_BLOCK, RigAdapter::K_CHANNELS};
    RigController controller(info);
    const std::string json_text = build_and_save_rig(controller);

    RigAdapter untouched;
    assert_prepare_and_load(untouched, json_text);
    RigAdapter changed;
    assert_prepare_and_load(changed, json_text);
    const std::optional<RigParameter> drive = find_by_name(changed, "Drive");
    ASSERT_TRUE(drive.has_value());

    changed.queue_parameter_for_audio_thread(drive->id, drive->minimum);
    const Stereo input = make_stereo_signal();

    EXPECT_NE(render_through_adapter(changed, input), render_through_adapter(untouched, input));
}

TEST(Small_RigAdapter, RigAdapterRefusesAMalformedPresetAndKeepsItsRig)
{
    SushiDSP::PrepareInfo info{K_SAMPLE_RATE, K_BLOCK, RigAdapter::K_CHANNELS};
    RigController controller(info);
    const std::string json_text = build_and_save_rig(controller);

    RigAdapter adapter;
    assert_prepare_and_load(adapter, json_text);
    const std::size_t parameter_count = adapter.parameters().size();

    std::string error;
    EXPECT_EQ(adapter.load_state("{ this is not a preset", error), RigStateStatus::Refused);

    EXPECT_FALSE(error.empty());
    EXPECT_EQ(adapter.parameters().size(), parameter_count);
}

TEST(Small_RigAdapter, RigAdapterRefusesAFormatWithNoFrames)
{
    RigAdapter adapter;
    std::string error;

    EXPECT_EQ(adapter.prepare(K_SAMPLE_RATE, 0, error), RigStateStatus::Refused);
    EXPECT_FALSE(error.empty());
}

TEST(Small_RigAdapter, RigAdapterClearsTheOutputsOfABlockLongerThanItWasPreparedFor)
{
    RigAdapter adapter;
    std::string error;
    ASSERT_EQ(adapter.prepare(K_SAMPLE_RATE, K_BLOCK, error), RigStateStatus::Ok) << error;

    const std::vector<float> input(2 * K_BLOCK, 0.5f);
    std::vector<float> left(2 * K_BLOCK, 1.0f);
    std::vector<float> right(2 * K_BLOCK, 1.0f);
    const float* inputs[2] = {input.data(), input.data()};
    float* outputs[2] = {left.data(), right.data()};

    adapter.process(inputs, outputs, RigAdapter::K_CHANNELS, 2 * K_BLOCK);

    EXPECT_EQ(left, std::vector<float>(2 * K_BLOCK, 0.0f));
    EXPECT_EQ(right, std::vector<float>(2 * K_BLOCK, 0.0f));
}

class Small_RigAdapterMisfit : public ::testing::TestWithParam<MisfitRow>
{
};

TEST_P(Small_RigAdapterMisfit, ABlockThatDoesNotFitIsCounted)
{
    RigAdapter adapter;
    std::string error;
    ASSERT_EQ(adapter.prepare(K_SAMPLE_RATE, K_BLOCK, error), RigStateStatus::Ok) << error;
    const MisfitRow& row = GetParam();
    const std::vector<float> input(row.frames, 0.5f);
    std::vector<float> left(row.frames, 1.0f);
    std::vector<float> right(row.frames, 1.0f);
    const float* inputs[2] = {input.data(), input.data()};
    float* outputs[2] = {left.data(), right.data()};

    adapter.process(inputs, outputs, row.channels, row.frames);

    EXPECT_EQ(adapter.counters().format_mismatch.value(), 1U);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_RigAdapterMisfit,
                         ::testing::Values(MisfitRow{"OneChannelInsteadOfTwo", 1u, K_BLOCK},
                                           MisfitRow{"LongerThanPrepared", RigAdapter::K_CHANNELS, 2 * K_BLOCK}),
                         misfit_row_name);

TEST(Small_RigAdapterCounters, ABlockWithTheWrongChannelCountIsCountedWithItsFrames)
{
    RigAdapter adapter;
    std::string error;
    ASSERT_EQ(adapter.prepare(K_SAMPLE_RATE, K_BLOCK, error), RigStateStatus::Ok) << error;
    const std::vector<float> input(K_BLOCK, 0.5f);
    std::vector<float> output(K_BLOCK, 1.0f);
    const float* inputs[1] = {input.data()};
    float* outputs[1] = {output.data()};

    adapter.process(inputs, outputs, 1u, K_BLOCK);

    EXPECT_EQ(adapter.counters().format_mismatch.peak(), K_BLOCK);
}

TEST(Small_RigAdapterCounters, AParameterChangeForAnUnknownIdIsCountedWhenTheBlockRuns)
{
    RigAdapter adapter;
    std::string error;
    ASSERT_EQ(adapter.prepare(K_SAMPLE_RATE, K_BLOCK, error), RigStateStatus::Ok) << error;
    adapter.queue_parameter_for_audio_thread(SushiDSP::Plugin::rig_parameter_id(3, 0), 0.5f);
    const std::vector<float> input(K_BLOCK, 0.5f);
    std::vector<float> left(K_BLOCK, 0.0f);
    std::vector<float> right(K_BLOCK, 0.0f);
    const float* inputs[2] = {input.data(), input.data()};
    float* outputs[2] = {left.data(), right.data()};

    adapter.process(inputs, outputs, RigAdapter::K_CHANNELS, K_BLOCK);

    EXPECT_EQ(adapter.counters().unknown_parameter.value(), 1U);
}

TEST(Small_RigAdapterCounters, AFullQueueIsReportedThroughTheReporter)
{
    RigAdapter adapter;
    std::string error;
    ASSERT_EQ(adapter.prepare(K_SAMPLE_RATE, K_BLOCK, error), RigStateStatus::Ok) << error;
    SushiDSP::RtCounterReporter reporter(std::chrono::seconds(5));
    adapter.add_counters_to(reporter);
    static_cast<void>(reporter.poll(SushiDSP::RtCounterReporter::Clock::time_point{}));
    queue_changes(adapter, RigAdapter::K_PENDING_CAPACITY + 3);

    const std::string line = reporter.poll(SushiDSP::RtCounterReporter::Clock::time_point{} +
                                           std::chrono::seconds(5));

    EXPECT_EQ(line, "3 dropped_parameter_changes in 5 s");
}

TEST(Small_RigAdapterCounters, APlainBlockCountsNothing)
{
    RigAdapter adapter;
    std::string error;
    ASSERT_EQ(adapter.prepare(K_SAMPLE_RATE, K_BLOCK, error), RigStateStatus::Ok) << error;
    const std::vector<float> input(K_BLOCK, 0.5f);
    std::vector<float> left(K_BLOCK, 0.0f);
    std::vector<float> right(K_BLOCK, 0.0f);
    const float* inputs[2] = {input.data(), input.data()};
    float* outputs[2] = {left.data(), right.data()};

    adapter.process(inputs, outputs, RigAdapter::K_CHANNELS, K_BLOCK);

    EXPECT_EQ(adapter.counters().format_mismatch.value(), 0U);
    EXPECT_EQ(adapter.counters().stale_routing.value(), 0U);
    EXPECT_EQ(adapter.counters().unknown_parameter.value(), 0U);
}
