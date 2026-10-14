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
 * @file test_rig_adapter.cpp
 * @brief Tests RigAdapter alone: pass-through, preset loading, state and parameters.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <array>
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
    void prepare_and_load(RigAdapter& adapter, const std::string& json_text)
    {
        std::string error;
        ASSERT_EQ(adapter.prepare(K_SAMPLE_RATE, K_BLOCK, error), RigStateStatus::Ok) << error;
        ASSERT_EQ(adapter.load_state(json_text, error), RigStateStatus::Ok) << error;
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

TEST(Unit_Dsp, RigAdapterStartsEmptyWithNoParametersAndNoLatency)
{
    RigAdapter adapter;
    EXPECT_TRUE(adapter.parameters().empty());
    EXPECT_EQ(adapter.latency_frames(), 0u);
}

TEST(Unit_Dsp, RigAdapterWithAnEmptyRigPassesEachChannelThrough)
{
    RigAdapter adapter;
    std::string error;
    ASSERT_EQ(adapter.prepare(K_SAMPLE_RATE, K_BLOCK, error), RigStateStatus::Ok) << error;

    const Stereo input = make_stereo_signal();
    const Stereo output = render_through_adapter(adapter, input);

    EXPECT_EQ(output[0], input[0]);
    EXPECT_EQ(output[1], input[1]);
}

TEST(Unit_Dsp, RigAdapterWithAnEmptyRigPassesThroughWhenRenderingInPlace)
{
    RigAdapter adapter;
    std::string error;
    ASSERT_EQ(adapter.prepare(K_SAMPLE_RATE, K_BLOCK, error), RigStateStatus::Ok) << error;

    const Stereo input = make_stereo_signal();
    Stereo buffer = input;
    float* channels[2] = {buffer[0].data(), buffer[1].data()};
    adapter.process(channels, channels, RigAdapter::K_CHANNELS, K_BLOCK);

    for (std::size_t frame = 0; frame < K_BLOCK; ++frame)
    {
        ASSERT_EQ(buffer[0][frame], input[0][frame]);
        ASSERT_EQ(buffer[1][frame], input[1][frame]);
    }
}

TEST(Unit_Dsp, RigAdapterRendersWhatTheControllerThatSavedThePresetRenders)
{
    SushiDSP::PrepareInfo info{K_SAMPLE_RATE, K_BLOCK, RigAdapter::K_CHANNELS};
    RigController controller(info);
    const std::string json_text = build_and_save_rig(controller);

    RigAdapter adapter;
    prepare_and_load(adapter, json_text);

    const Stereo input = make_stereo_signal();
    const Stereo expected = render_through_controller(controller, input);
    const Stereo actual = render_through_adapter(adapter, input);

    float peak = 0.0f;
    for (std::size_t channel = 0; channel < expected.size(); ++channel)
    {
        for (std::size_t frame = 0; frame < expected[channel].size(); ++frame)
        {
            ASSERT_NEAR(actual[channel][frame], expected[channel][frame], 1.0e-5f)
                << "channel " << channel << ", frame " << frame;
            peak = std::max(peak, std::abs(actual[channel][frame]));
        }
    }
    EXPECT_GT(peak, 0.0f);
    EXPECT_NE(actual[0], input[0]);
    EXPECT_EQ(adapter.latency_frames(), controller.active_graph()->latency_samples());
}

TEST(Unit_Dsp, RigAdapterListsEveryParameterOfEverySlotInRigOrder)
{
    SushiDSP::PrepareInfo info{K_SAMPLE_RATE, K_BLOCK, RigAdapter::K_CHANNELS};
    RigController controller(info);
    const std::string json_text = build_and_save_rig(controller);

    RigAdapter adapter;
    prepare_and_load(adapter, json_text);

    const std::vector<RigParameter>& parameters = adapter.parameters();
    ASSERT_FALSE(parameters.empty());
    EXPECT_EQ(SushiDSP::Plugin::rig_parameter_slot(parameters.front().id), 0u);
    EXPECT_EQ(SushiDSP::Plugin::rig_parameter_slot(parameters.back().id), 1u);
    for (std::size_t position = 1; position < parameters.size(); ++position)
    {
        EXPECT_LT(parameters[position - 1].id, parameters[position].id);
    }

    const std::optional<RigParameter> drive = find_by_name(adapter, "Drive");
    ASSERT_TRUE(drive.has_value());
    EXPECT_EQ(drive->id, SushiDSP::Plugin::rig_parameter_id(0, 0));
    EXPECT_EQ(drive->module, "1 Tube Screamer");
    EXPECT_FLOAT_EQ(drive->minimum, 0.0f);
    EXPECT_FLOAT_EQ(drive->maximum, 1.0f);
    const std::optional<float> value = adapter.parameter_value(drive->id);
    ASSERT_TRUE(value.has_value());
    EXPECT_FLOAT_EQ(*value, 0.77f);
}

TEST(Unit_Dsp, RigAdapterStateRoundTripsThroughASecondAdapter)
{
    SushiDSP::PrepareInfo info{K_SAMPLE_RATE, K_BLOCK, RigAdapter::K_CHANNELS};
    RigController controller(info);
    const std::string json_text = build_and_save_rig(controller);

    RigAdapter first;
    prepare_and_load(first, json_text);
    std::string saved;
    std::string error;
    ASSERT_EQ(first.save_state(saved, error), RigStateStatus::Ok) << error;

    RigAdapter second;
    prepare_and_load(second, saved);
    std::string saved_again;
    ASSERT_EQ(second.save_state(saved_again, error), RigStateStatus::Ok) << error;

    EXPECT_EQ(saved_again, saved);
    ASSERT_EQ(second.parameters().size(), first.parameters().size());
    for (const RigParameter& parameter : first.parameters())
    {
        const std::optional<float> original = first.parameter_value(parameter.id);
        const std::optional<float> reloaded = second.parameter_value(parameter.id);
        ASSERT_TRUE(original.has_value());
        ASSERT_TRUE(reloaded.has_value());
        EXPECT_FLOAT_EQ(*reloaded, *original) << parameter.module << " " << parameter.name;
    }
}

TEST(Unit_Dsp, RigAdapterKeepsItsRigAcrossAChangeOfSampleRate)
{
    SushiDSP::PrepareInfo info{K_SAMPLE_RATE, K_BLOCK, RigAdapter::K_CHANNELS};
    RigController controller(info);
    const std::string json_text = build_and_save_rig(controller);

    RigAdapter adapter;
    prepare_and_load(adapter, json_text);
    const std::size_t parameter_count = adapter.parameters().size();

    std::string error;
    ASSERT_EQ(adapter.prepare(44100.0, 512, error), RigStateStatus::Ok) << error;

    EXPECT_EQ(adapter.parameters().size(), parameter_count);
}

TEST(Unit_Dsp, RigAdapterReadsBackAParameterSetOnTheControlThread)
{
    SushiDSP::PrepareInfo info{K_SAMPLE_RATE, K_BLOCK, RigAdapter::K_CHANNELS};
    RigController controller(info);
    const std::string json_text = build_and_save_rig(controller);

    RigAdapter adapter;
    prepare_and_load(adapter, json_text);
    const RigParameter parameter = adapter.parameters().front();
    const float target = parameter.minimum + 0.25f * (parameter.maximum - parameter.minimum);

    ASSERT_TRUE(adapter.set_parameter(parameter.id, target));

    const std::optional<float> value = adapter.parameter_value(parameter.id);
    ASSERT_TRUE(value.has_value());
    EXPECT_FLOAT_EQ(*value, target);
}

TEST(Unit_Dsp, RigAdapterClampsAParameterToItsRange)
{
    SushiDSP::PrepareInfo info{K_SAMPLE_RATE, K_BLOCK, RigAdapter::K_CHANNELS};
    RigController controller(info);
    const std::string json_text = build_and_save_rig(controller);

    RigAdapter adapter;
    prepare_and_load(adapter, json_text);
    const RigParameter parameter = adapter.parameters().front();

    ASSERT_TRUE(adapter.set_parameter(parameter.id, parameter.maximum + 1000.0f));

    const std::optional<float> value = adapter.parameter_value(parameter.id);
    ASSERT_TRUE(value.has_value());
    EXPECT_FLOAT_EQ(*value, parameter.maximum);
}

TEST(Unit_Dsp, RigAdapterRefusesAParameterTheRigDoesNotHold)
{
    RigAdapter adapter;
    const std::uint32_t missing = SushiDSP::Plugin::rig_parameter_id(3, 0);

    EXPECT_FALSE(adapter.set_parameter(missing, 0.5f));
    EXPECT_FALSE(adapter.parameter_value(missing).has_value());
}

TEST(Unit_Dsp, RigAdapterAppliesAParameterQueuedOnTheAudioThreadAndSavesIt)
{
    SushiDSP::PrepareInfo info{K_SAMPLE_RATE, K_BLOCK, RigAdapter::K_CHANNELS};
    RigController controller(info);
    const std::string json_text = build_and_save_rig(controller);

    RigAdapter adapter;
    prepare_and_load(adapter, json_text);
    const RigParameter parameter = adapter.parameters().front();
    const float target = parameter.minimum + 0.6f * (parameter.maximum - parameter.minimum);

    adapter.queue_parameter_for_audio_thread(parameter.id, target);
    const Stereo input = make_stereo_signal();
    render_through_adapter(adapter, input);

    const std::optional<float> value = adapter.parameter_value(parameter.id);
    ASSERT_TRUE(value.has_value());
    EXPECT_FLOAT_EQ(*value, target);

    std::string saved;
    std::string error;
    ASSERT_EQ(adapter.save_state(saved, error), RigStateStatus::Ok) << error;
    RigAdapter reloaded;
    prepare_and_load(reloaded, saved);
    const std::optional<float> carried = reloaded.parameter_value(parameter.id);
    ASSERT_TRUE(carried.has_value());
    EXPECT_FLOAT_EQ(*carried, target);
}

TEST(Unit_Dsp, RigAdapterAudioThreadParameterChangesWhatTheRigRenders)
{
    SushiDSP::PrepareInfo info{K_SAMPLE_RATE, K_BLOCK, RigAdapter::K_CHANNELS};
    RigController controller(info);
    const std::string json_text = build_and_save_rig(controller);

    RigAdapter untouched;
    prepare_and_load(untouched, json_text);
    RigAdapter changed;
    prepare_and_load(changed, json_text);
    const std::optional<RigParameter> drive = find_by_name(changed, "Drive");
    ASSERT_TRUE(drive.has_value());

    changed.queue_parameter_for_audio_thread(drive->id, drive->minimum);
    const Stereo input = make_stereo_signal();

    EXPECT_NE(render_through_adapter(changed, input), render_through_adapter(untouched, input));
}

TEST(Unit_Dsp, RigAdapterRefusesAMalformedPresetAndKeepsItsRig)
{
    SushiDSP::PrepareInfo info{K_SAMPLE_RATE, K_BLOCK, RigAdapter::K_CHANNELS};
    RigController controller(info);
    const std::string json_text = build_and_save_rig(controller);

    RigAdapter adapter;
    prepare_and_load(adapter, json_text);
    const std::size_t parameter_count = adapter.parameters().size();

    std::string error;
    EXPECT_EQ(adapter.load_state("{ this is not a preset", error), RigStateStatus::Refused);

    EXPECT_FALSE(error.empty());
    EXPECT_EQ(adapter.parameters().size(), parameter_count);
}

TEST(Unit_Dsp, RigAdapterRefusesAFormatWithNoFrames)
{
    RigAdapter adapter;
    std::string error;

    EXPECT_EQ(adapter.prepare(K_SAMPLE_RATE, 0, error), RigStateStatus::Refused);
    EXPECT_FALSE(error.empty());
}

TEST(Unit_Dsp, RigAdapterClearsTheOutputsOfABlockLongerThanItWasPreparedFor)
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
