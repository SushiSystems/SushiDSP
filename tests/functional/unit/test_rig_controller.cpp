/****************************************************************/
/* test_rig_controller.cpp                                      */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "RigController.hpp"
#include "RigPersistence.hpp"
#include "SessionTimeline.hpp"
#include "SlotGraph.hpp"
#include "SlotSelection.hpp"
#include "SushiDSP/capture/Transport.hpp"
#include "SushiDSP/node_registry/NodeRegistry.hpp"
#include "SushiDSP/room/IRoomHost.hpp"
#include "SushiDSP/room/RoomScene.hpp"
#include "SushiDSP/room/StarterScene.hpp"

namespace
{
    /// @brief Names a fixture file after the running case, so two cases ctest schedules in
    /// parallel never write the same path.
    /// @param name The file's name inside the test temporary directory.
    /// @return The full path, with the running case's name prefixed.
    std::string temp_fixture_path(const std::string& name)
    {
        const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
        const std::string          prefix = info != nullptr ? std::string(info->name()) + "_" : "";
        return std::string(::testing::TempDir()) + "sushidsp_" + prefix + name;
    }

    void append_u32_le(std::vector<std::uint8_t>& buffer, std::uint32_t value)
    {
        for (int byte = 0; byte < 4; ++byte)
        {
            buffer.push_back(static_cast<std::uint8_t>((value >> (8 * byte)) & 0xFF));
        }
    }

    void append_u16_le(std::vector<std::uint8_t>& buffer, std::uint16_t value)
    {
        buffer.push_back(static_cast<std::uint8_t>(value & 0xFF));
        buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    }

    void append_tag(std::vector<std::uint8_t>& buffer, const char* tag)
    {
        buffer.insert(buffer.end(), tag, tag + 4);
    }

    std::string write_temp_impulse_wav()
    {
        const std::uint32_t sample_rate = 48000;
        std::vector<std::int16_t> samples(64, 0);
        samples[0] = 16384;

        const std::uint32_t data_bytes = static_cast<std::uint32_t>(samples.size() * sizeof(std::int16_t));
        std::vector<std::uint8_t> bytes;
        append_tag(bytes, "RIFF");
        append_u32_le(bytes, 36 + data_bytes);
        append_tag(bytes, "WAVE");
        append_tag(bytes, "fmt ");
        append_u32_le(bytes, 16);
        append_u16_le(bytes, 1);
        append_u16_le(bytes, 1);
        append_u32_le(bytes, sample_rate);
        append_u32_le(bytes, sample_rate * 2);
        append_u16_le(bytes, 2);
        append_u16_le(bytes, 16);
        append_tag(bytes, "data");
        append_u32_le(bytes, data_bytes);
        for (std::int16_t sample : samples)
        {
            append_u16_le(bytes, static_cast<std::uint16_t>(sample));
        }

        const std::string path = temp_fixture_path("cabinet.wav");
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        return path;
    }


    /// @brief Writes a 32-bit-float mono WAV file, whose round trip through WavFile is exact.
    /// @param name The file's name inside the test temporary directory.
    /// @param samples The audio to write.
    /// @param sample_rate The rate to record in the header.
    /// @return The file's full path.
    std::string write_temp_float_wav(const char* name, const std::vector<float>& samples,
                                     std::uint32_t sample_rate)
    {
        const std::uint32_t data_bytes = static_cast<std::uint32_t>(samples.size() * sizeof(float));
        std::vector<std::uint8_t> bytes;
        append_tag(bytes, "RIFF");
        append_u32_le(bytes, 36 + data_bytes);
        append_tag(bytes, "WAVE");
        append_tag(bytes, "fmt ");
        append_u32_le(bytes, 16);
        append_u16_le(bytes, 3);
        append_u16_le(bytes, 1);
        append_u32_le(bytes, sample_rate);
        append_u32_le(bytes, sample_rate * 4);
        append_u16_le(bytes, 4);
        append_u16_le(bytes, 32);
        append_tag(bytes, "data");
        append_u32_le(bytes, data_bytes);
        for (float sample : samples)
        {
            std::uint32_t word = 0;
            std::memcpy(&word, &sample, sizeof(word));
            append_u32_le(bytes, word);
        }

        const std::string path = temp_fixture_path(name);
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        return path;
    }

    /// @brief Builds a repeatable guitar-shaped test signal.
    /// @param frames How many samples to produce.
    /// @return The signal.
    /// @brief Strips a path down to its file name without an extension, the way the controller
    /// names a lane after the file dropped onto it.
    /// @param path The path to reduce.
    /// @return The stem.
    std::string path_stem_of(const std::string& path)
    {
        const std::size_t cut = path.find_last_of("/\\");
        const std::string name = cut == std::string::npos ? path : path.substr(cut + 1);
        const std::size_t dot = name.find_last_of('.');
        return dot == std::string::npos ? name : name.substr(0, dot);
    }

    std::vector<float> make_test_signal(std::size_t frames)
    {
        std::vector<float> signal(frames, 0.0f);
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            const double phase = 2.0 * 3.14159265358979323846 * 220.0
                * static_cast<double>(frame) / 48000.0;
            signal[frame] = 0.4f * static_cast<float>(std::sin(phase));
        }
        return signal;
    }

    /// @brief Runs a controller's published graph over a signal, one block at a time.
    /// @param controller The rig to run.
    /// @param input The whole signal, a multiple of block long.
    /// @param block Frames per process() call.
    /// @return The rig's output, the same length as input.
    std::vector<float> render_through_rig(RigController& controller,
                                          const std::vector<float>& input, std::uint32_t block)
    {
        std::vector<float> output(input.size(), 0.0f);
        for (std::size_t offset = 0; offset + block <= input.size(); offset += block)
        {
            const float* input_pointer[1] = { input.data() + offset };
            float*       output_pointer[1] = { output.data() + offset };
            SushiDSP::ProcessContext context{ output_pointer, input_pointer, block, 0.0 };
            controller.active_graph()->process(context);
        }
        return output;
    }

    /// @brief Counts the live slots carrying one descriptor id.
    std::size_t count_slots_of_type(const RigController& controller, const std::string& descriptor_id)
    {
        std::size_t total = 0;
        for (RigController::SlotId slot : controller.slot_order())
        {
            if (controller.descriptor_id(slot) == descriptor_id)
            {
                ++total;
            }
        }
        return total;
    }

    /// @brief Finds the rig's first mixer.
    /// @return The mixer's slot id, or std::nullopt when the rig has none.
    std::optional<RigController::SlotId> find_mixer(const RigController& controller)
    {
        for (RigController::SlotId slot : controller.slot_order())
        {
            if (controller.descriptor_id(slot) == "sushidsp.mixer")
            {
                return slot;
            }
        }
        return std::nullopt;
    }

    /// @brief Reports whether a slot id appears in a list.
    bool contains(const std::vector<RigController::SlotId>& slots, RigController::SlotId slot)
    {
        return std::find(slots.begin(), slots.end(), slot) != slots.end();
    }
}

TEST(Unit_Dsp, RigControllerStartsEmpty)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    EXPECT_TRUE(controller.slot_order().empty());
}

TEST(Unit_Dsp, RigControllerAppendAddsNodeToEndOfChain)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId b = controller.append("sushidsp.jcm800");

    const auto order = controller.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], a);
    EXPECT_EQ(order[1], b);
    EXPECT_EQ(controller.descriptor_id(a), "sushidsp.tubescreamer");
    EXPECT_EQ(controller.descriptor_id(b), "sushidsp.jcm800");
    ASSERT_NE(controller.active_graph(), nullptr);
}

TEST(Unit_Dsp, RigControllerInsertBeforePlacesNewSlotImmediatelyBeforeAnchor)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId c = controller.append("sushidsp.jcm800");

    const RigController::SlotId b = controller.insert_before(c, "sushidsp.tubescreamer");

    const auto order = controller.slot_order();
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], a);
    EXPECT_EQ(order[1], b);
    EXPECT_EQ(order[2], c);
}

TEST(Unit_Dsp, RigControllerInsertBeforeNoAnchorPlacesNewSlotAtFront)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");

    const RigController::SlotId new_head = controller.insert_before(std::nullopt, "sushidsp.jcm800");

    const auto order = controller.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], new_head);
    EXPECT_EQ(order[1], a);
}

TEST(Unit_Dsp, RigControllerMoveBeforeReordersChain)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId b = controller.append("sushidsp.jcm800");

    controller.move_before(a, std::nullopt);  // a is already at the front; no-op shape, exercised separately below
    controller.move_before(b, a);             // move b to immediately before a

    const auto order = controller.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], b);
    EXPECT_EQ(order[1], a);
}

TEST(Unit_Dsp, RigControllerRemoveDropsFromChainAndReconnectsSurvivors)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId b = controller.append("sushidsp.jcm800");
    const RigController::SlotId c = controller.append("sushidsp.tubescreamer");

    controller.remove(b);

    const auto order = controller.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], a);
    EXPECT_EQ(order[1], c);
    // b's removal must not leave a and c disconnected — the chain must re-link around the gap,
    // matching today's behavior where removing a middle slot still produces one working chain.
    ASSERT_NE(controller.active_graph(), nullptr);
    const auto handles = controller.active_graph()->nodes();
    ASSERT_EQ(handles.size(), 2u);
}

TEST(Unit_Dsp, RigControllerBuildsSplitterTwoBranchMixerShapeAndProcessesCorrectly)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);

    const RigController::SlotId source = controller.add_node("sushidsp.tubescreamer", false);
    const RigController::SlotId splitter = controller.add_node("sushidsp.splitter", false);
    const RigController::SlotId branch_a = controller.add_node("sushidsp.jcm800", false);
    const RigController::SlotId branch_b = controller.add_node("sushidsp.tubescreamer", false);
    const RigController::SlotId mixer = controller.add_node("sushidsp.mixer", false);
    controller.connect(source, splitter, false);
    controller.connect(splitter, branch_a, false);
    controller.connect(splitter, branch_b, false);
    controller.connect(branch_a, mixer, false);
    controller.connect(branch_b, mixer, false);
    controller.publish();

    // Every node exists and the graph compiled/published without throwing — the split/mix shape
    // this test builds was, before the publish-batching parameter, impossible to construct
    // through the public API at all (the second add_node() would have republished a two-node,
    // zero-edge graph, which RigGraph::compile() rejects for having no sink). Confirm the rig
    // actually processes a block without crashing or throwing.
    ASSERT_NE(controller.active_graph(), nullptr);
    ASSERT_EQ(controller.active_graph()->nodes().size(), 5u);

    std::vector<float> in(256, 0.0f);
    std::vector<float> out(256, 0.0f);
    const float* ins[1] = { in.data() };
    float* outs[1] = { out.data() };
    SushiDSP::ProcessContext context{ outs, ins, 256, 0.0 };
    EXPECT_NO_THROW(controller.active_graph()->process(context));
}

TEST(Unit_Dsp, RigControllerAddBranchBuildsTheParallelShapeTheGuiOffers)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);

    // The whole point of the feature, in the order a user performs it: drag a source, a splitter
    // and one effect into a straight line, then drop a second effect on the splitter's open row.
    // add_branch() is what turns that one drop into the four-edge shape RigGraph::compile()
    // accepts, mixer and all.
    const RigController::SlotId source = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId splitter = controller.append("sushidsp.splitter");
    const RigController::SlotId branch_a = controller.append("sushidsp.jcm800");

    const std::optional<RigController::SlotId> branch_b =
        controller.add_branch(splitter, "sushidsp.tubescreamer");
    ASSERT_TRUE(branch_b.has_value());

    // Source, splitter, branch A, branch B, mixer.
    EXPECT_EQ(controller.slot_order().size(), 5u);
    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 5u);

    const std::optional<RigController::SlotId> mixer = find_mixer(controller);
    ASSERT_TRUE(mixer.has_value());
    EXPECT_EQ(count_slots_of_type(controller, "sushidsp.mixer"), 1u);

    EXPECT_EQ(controller.upstream_of(splitter), std::vector<RigController::SlotId>{ source });
    EXPECT_EQ(controller.upstream_of(branch_a), std::vector<RigController::SlotId>{ splitter });
    EXPECT_EQ(controller.upstream_of(*branch_b), std::vector<RigController::SlotId>{ splitter });

    const std::vector<RigController::SlotId> into_mixer = controller.upstream_of(*mixer);
    EXPECT_EQ(into_mixer.size(), 2u);
    EXPECT_TRUE(contains(into_mixer, branch_a));
    EXPECT_TRUE(contains(into_mixer, *branch_b));

    std::vector<float> in(256, 0.0f);
    std::vector<float> out(256, 0.0f);
    const float* ins[1] = { in.data() };
    float* outs[1] = { out.data() };
    SushiDSP::ProcessContext context{ outs, ins, 256, 0.0 };
    EXPECT_NO_THROW(controller.active_graph()->process(context));
}

TEST(Unit_Dsp, RigControllerAddBranchTwiceReusesTheOneMixer)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    controller.append("sushidsp.tubescreamer");
    const RigController::SlotId splitter = controller.append("sushidsp.splitter");
    const RigController::SlotId branch_a = controller.append("sushidsp.jcm800");

    const std::optional<RigController::SlotId> branch_b =
        controller.add_branch(splitter, "sushidsp.tubescreamer");
    const std::optional<RigController::SlotId> branch_c =
        controller.add_branch(splitter, "sushidsp.tubescreamer");
    ASSERT_TRUE(branch_b.has_value());
    ASSERT_TRUE(branch_c.has_value());

    EXPECT_EQ(count_slots_of_type(controller, "sushidsp.mixer"), 1u);
    EXPECT_EQ(controller.slot_order().size(), 6u);

    const std::optional<RigController::SlotId> mixer = find_mixer(controller);
    ASSERT_TRUE(mixer.has_value());
    const std::vector<RigController::SlotId> into_mixer = controller.upstream_of(*mixer);
    EXPECT_EQ(into_mixer.size(), 3u);
    EXPECT_TRUE(contains(into_mixer, branch_a));
    EXPECT_TRUE(contains(into_mixer, *branch_b));
    EXPECT_TRUE(contains(into_mixer, *branch_c));
}

TEST(Unit_Dsp, RigControllerAddBranchOnADanglingSplitterCreatesNoMixer)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId splitter = controller.append("sushidsp.splitter");

    const std::optional<RigController::SlotId> branch =
        controller.add_branch(splitter, "sushidsp.jcm800");
    ASSERT_TRUE(branch.has_value());

    EXPECT_EQ(controller.slot_order().size(), 2u);
    EXPECT_EQ(count_slots_of_type(controller, "sushidsp.mixer"), 0u);
    EXPECT_EQ(controller.upstream_of(*branch), std::vector<RigController::SlotId>{ splitter });
}

TEST(Unit_Dsp, RigControllerAddBranchOnASlotThatIsNotASplitterIsRefused)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId screamer = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId amp = controller.append("sushidsp.jcm800");

    EXPECT_FALSE(controller.add_branch(screamer, "sushidsp.jcm800").has_value());

    EXPECT_EQ(controller.slot_order(), (std::vector<RigController::SlotId>{ screamer, amp }));
    EXPECT_EQ(controller.upstream_of(amp), std::vector<RigController::SlotId>{ screamer });
}

TEST(Unit_Dsp, RigControllerAddBranchWithAnUnregisteredDescriptorIdIsRefused)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId splitter = controller.append("sushidsp.splitter");
    const RigController::SlotId amp = controller.append("sushidsp.jcm800");

    EXPECT_FALSE(controller.add_branch(splitter, "sushidsp.does_not_exist").has_value());

    EXPECT_EQ(controller.slot_order(), (std::vector<RigController::SlotId>{ splitter, amp }));
    EXPECT_EQ(count_slots_of_type(controller, "sushidsp.mixer"), 0u);
}

TEST(Unit_Dsp, RigControllerSaveAndLoadJsonRoundTripsAParallelBranch)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController original(info);
    original.append("sushidsp.tubescreamer");
    const RigController::SlotId splitter = original.append("sushidsp.splitter");
    original.append("sushidsp.jcm800");
    ASSERT_TRUE(original.add_branch(splitter, "sushidsp.tubescreamer").has_value());

    const std::string json_text = original.save_to_json();

    RigController reloaded(info);
    reloaded.load_from_json(json_text);

    EXPECT_EQ(reloaded.slot_order().size(), 5u);
    EXPECT_EQ(count_slots_of_type(reloaded, "sushidsp.mixer"), 1u);
    EXPECT_EQ(count_slots_of_type(reloaded, "sushidsp.splitter"), 1u);

    const std::optional<RigController::SlotId> mixer = find_mixer(reloaded);
    ASSERT_TRUE(mixer.has_value());
    EXPECT_EQ(reloaded.upstream_of(*mixer).size(), 2u);
}

TEST(Unit_Dsp, RigControllerAppendWithUnknownDescriptorIdIsSafeNoOp)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.does_not_exist");

    const auto order = controller.slot_order();
    ASSERT_EQ(order.size(), 1u);
    EXPECT_EQ(order[0], a);
    ASSERT_NE(controller.active_graph(), nullptr);
}

TEST(Unit_Dsp, RigControllerPreservesParameterValuesAcrossReorder)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");
    controller.set_parameter(ts, "drive", 0.42f);
    const RigController::SlotId amp = controller.append("sushidsp.jcm800");

    controller.move_before(ts, std::nullopt);
    (void)amp;

    EXPECT_FLOAT_EQ(controller.get_parameter(ts, "drive"), 0.42f);
}

TEST(Unit_Dsp, RigControllerGetParameterReturnsDescriptorDefaultBeforeAnySet)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");

    float expected_default = 0.0f;
    bool found = false;
    for (const auto& entry : SushiDSP::node_registry())
    {
        if (std::string("sushidsp.tubescreamer") == entry.descriptor->id)
        {
            for (std::size_t p = 0; p < entry.descriptor->parameter_count; ++p)
            {
                if (std::string("drive") == entry.descriptor->parameters[p].id)
                {
                    expected_default = entry.descriptor->parameters[p].default_value;
                    found = true;
                }
            }
        }
    }
    ASSERT_TRUE(found);
    ASSERT_NE(expected_default, 0.0f);

    EXPECT_FLOAT_EQ(controller.get_parameter(ts, "drive"), expected_default);
}

TEST(Unit_Dsp, RigControllerSetParameterReachesNodeOnlyAfterProcess)
{
    SushiDSP::PrepareInfo info{ 48000.0, 64, 1 };
    RigController controller(info);
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");
    controller.set_parameter(ts, "tone", 0.9f);

    SushiDSP::IAudioNode* node = controller.active_graph()->node(controller.active_graph()->nodes().front());
    ASSERT_NE(node, nullptr);
    EXPECT_FLOAT_EQ(node->get_parameter("tone"), 0.5f);

    std::vector<float> in(64, 0.0f);
    std::vector<float> out(64, 0.0f);
    const float* ins[1] = { in.data() };
    float* outs[1] = { out.data() };
    SushiDSP::ProcessContext context{ outs, ins, 64, 0.0 };
    controller.active_graph()->process(context);

    EXPECT_FLOAT_EQ(node->get_parameter("tone"), 0.9f);
    EXPECT_EQ(controller.dropped_parameter_changes(), 0u);
}

TEST(Unit_Dsp, RigControllerSetParameterStillMirroredByGetParameterImmediately)
{
    SushiDSP::PrepareInfo info{ 48000.0, 64, 1 };
    RigController controller(info);
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");
    controller.set_parameter(ts, "tone", 0.9f);

    EXPECT_FLOAT_EQ(controller.get_parameter(ts, "tone"), 0.9f);
}

TEST(Unit_Dsp, RigControllerRebuildSeedsFreshNodesWithStoredParameters)
{
    SushiDSP::PrepareInfo info{ 48000.0, 64, 1 };
    RigController controller(info);
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");
    controller.set_parameter(ts, "tone", 0.9f);
    controller.append("sushidsp.jcm800");

    SushiDSP::NodeHandle handle_for_ts = 0;   // ts was added first, so it is the graph's first handle
    SushiDSP::IAudioNode* node = controller.active_graph()->node(handle_for_ts);
    ASSERT_NE(node, nullptr);
    EXPECT_FLOAT_EQ(node->get_parameter("tone"), 0.9f);
}

TEST(Unit_Dsp, RigControllerSaveAndLoadJsonRoundTripsChainAndParameters)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController original(info);
    const RigController::SlotId ts = original.append("sushidsp.tubescreamer");
    original.set_parameter(ts, "drive", 0.77f);
    original.append("sushidsp.jcm800");

    const std::string json_text = original.save_to_json();

    RigController reloaded(info);
    reloaded.load_from_json(json_text);

    const auto order = reloaded.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(reloaded.descriptor_id(order[0]), "sushidsp.tubescreamer");
    EXPECT_EQ(reloaded.descriptor_id(order[1]), "sushidsp.jcm800");
    EXPECT_FLOAT_EQ(reloaded.get_parameter(order[0], "drive"), 0.77f);
}

TEST(Unit_Dsp, RigControllerSaveToJsonUsesOwnStoredValueNotLiveNodeState)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");
    controller.set_parameter(ts, "drive", 0.9f);
    // Deliberately no process() call and no further structural edit here — the live node still
    // reads the pre-edit default (0.5), but save_to_json() must report the controller's own
    // authoritative 0.9f, matching what get_parameter() and the GUI slider both already show.

    const std::string json_text = controller.save_to_json();
    const nlohmann::json root = nlohmann::json::parse(json_text);
    ASSERT_EQ(root.at("nodes").size(), 1u);
    EXPECT_FLOAT_EQ(root.at("nodes")[0].at("params").at("drive").get<float>(), 0.9f);
}

TEST(Unit_Dsp, RigControllerSaveAndLoadJsonRoundTripsCabinetIr)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    const std::string ir_path = write_temp_impulse_wav();

    RigController original(info);
    const RigController::SlotId amp = original.append("sushidsp.jcm800");
    const RigController::SlotId cabinet = original.append("sushidsp.cabinet");
    ASSERT_TRUE(original.set_cabinet(cabinet, ir_path));

    const std::string json_text = original.save_to_json();

    RigController reloaded(info);
    reloaded.load_from_json(json_text);

    const auto order = reloaded.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(reloaded.cabinet_path(order[1]), ir_path);

    std::remove(ir_path.c_str());
}

TEST(Unit_Dsp, RigControllerRefusesACabinetOnAnAmplifierSlot)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    const std::string ir_path = write_temp_impulse_wav();

    RigController controller(info);
    const RigController::SlotId amp = controller.append("sushidsp.peavey5150");
    EXPECT_FALSE(controller.set_cabinet(amp, ir_path));
    EXPECT_EQ(controller.cabinet_path(amp), "");
    EXPECT_FALSE(controller.hosts_cabinet(amp));

    std::remove(ir_path.c_str());
}

TEST(Unit_Dsp, RigControllerRefusesACabinetOnANodeThatHostsNone)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    const std::string ir_path = write_temp_impulse_wav();

    RigController controller(info);
    const RigController::SlotId delay = controller.append("sushidsp.dd3");
    EXPECT_FALSE(controller.set_cabinet(delay, ir_path));
    EXPECT_EQ(controller.cabinet_path(delay), "");

    std::remove(ir_path.c_str());
}

// --- Per-slot bypass ---
//
// A bypassed slot keeps its place in slot_order_, its parameters and its edges; only the
// published graph leaves it out, wiring its upstream neighbours straight to its downstream ones.

TEST(Unit_Dsp, RigControllerBypassBridgesTheSlotsNeighbours)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    const RigController::SlotId first = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId middle = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId last = controller.append("sushidsp.jcm800");

    controller.set_bypassed(middle, true);

    EXPECT_TRUE(controller.is_bypassed(middle));
    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 2u);
    // The bypassed slot keeps its place in the rig; only the graph skips it.
    EXPECT_EQ(controller.slot_order().size(), 3u);
    EXPECT_EQ(controller.upstream_of(last).size(), 1u);
    EXPECT_EQ(controller.upstream_of(last)[0], middle);
    (void)first;
}

TEST(Unit_Dsp, RigControllerUnbypassRestoresTheOriginalGraph)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    controller.append("sushidsp.tubescreamer");
    const RigController::SlotId middle = controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");

    controller.set_bypassed(middle, true);
    controller.set_bypassed(middle, false);

    EXPECT_FALSE(controller.is_bypassed(middle));
    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 3u);
}

TEST(Unit_Dsp, RigControllerBridgesTwoBypassedSlotsInARow)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    controller.append("sushidsp.tubescreamer");
    const RigController::SlotId second = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId third = controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");

    controller.set_bypassed(second, true);
    controller.set_bypassed(third, true);

    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 2u);
}

TEST(Unit_Dsp, RigControllerBypassingTheHeadSlotLeavesAWorkingGraph)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    const RigController::SlotId head = controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");

    controller.set_bypassed(head, true);

    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 1u);
    EXPECT_FALSE(controller.active_graph()->is_empty());
}

TEST(Unit_Dsp, RigControllerBypassingEverySlotLeavesAnEmptyGraph)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    const RigController::SlotId only = controller.append("sushidsp.tubescreamer");

    controller.set_bypassed(only, true);

    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_TRUE(controller.active_graph()->is_empty());
}

TEST(Unit_Dsp, RigControllerSaveToJsonKeepsABypassedNodeAndFlagsIt)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    controller.append("sushidsp.tubescreamer");
    const RigController::SlotId middle = controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");
    controller.set_bypassed(middle, true);

    const nlohmann::json parsed = nlohmann::json::parse(controller.save_to_json());

    ASSERT_EQ(parsed.at("nodes").size(), 3u);
    int flagged = 0;
    for (const auto& node : parsed.at("nodes"))
    {
        if (node.contains("bypassed") && node.at("bypassed").get<bool>())
        {
            ++flagged;
        }
    }
    EXPECT_EQ(flagged, 1);
}

TEST(Unit_Dsp, RigControllerLoadFromJsonRestoresTheBypassFlag)
{
    RigController source(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    source.append("sushidsp.tubescreamer");
    const RigController::SlotId middle = source.append("sushidsp.tubescreamer");
    source.append("sushidsp.jcm800");
    source.set_bypassed(middle, true);
    const std::string preset = source.save_to_json();

    RigController target(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    target.load_from_json(preset);

    const std::vector<RigController::SlotId> order = target.slot_order();
    ASSERT_EQ(order.size(), 3u);
    int bypassed = 0;
    for (RigController::SlotId slot : order)
    {
        if (target.is_bypassed(slot))
        {
            ++bypassed;
        }
    }
    EXPECT_EQ(bypassed, 1);
    ASSERT_NE(target.active_graph(), nullptr);
    EXPECT_EQ(target.active_graph()->nodes().size(), 2u);
}

TEST(Unit_Dsp, RigControllerLoadsAPresetWithNoBypassFieldWithEverySlotActive)
{
    const std::string preset = R"({"nodes":[
        {"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":[]},
        {"id":1,"type":"sushidsp.jcm800","params":{},"inputs":[0]}
    ]})";

    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    controller.load_from_json(preset);

    for (RigController::SlotId slot : controller.slot_order())
    {
        EXPECT_FALSE(controller.is_bypassed(slot));
    }
    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 2u);
}

TEST(Unit_Dsp, RigControllerIsBypassedIsFalseForASlotThatIsNotLive)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    EXPECT_FALSE(controller.is_bypassed(9999));
}

// --- Impulse-response folder (preset-level, no audio behind it) ---

TEST(Unit_Dsp, RigControllerRemembersTheIrFolder)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    EXPECT_EQ(controller.ir_folder(), "");

    controller.set_ir_folder("D:/irs/marshall");

    EXPECT_EQ(controller.ir_folder(), "D:/irs/marshall");
}

TEST(Unit_Dsp, RigControllerSaveToJsonWritesTheIrFolderWhenOneIsSet)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");
    controller.set_ir_folder("D:/irs/marshall");

    const nlohmann::json parsed = nlohmann::json::parse(controller.save_to_json());

    ASSERT_TRUE(parsed.contains("ir_folder"));
    EXPECT_EQ(parsed.at("ir_folder").get<std::string>(), "D:/irs/marshall");
}

TEST(Unit_Dsp, RigControllerSaveToJsonOmitsTheIrFolderWhenNoneIsSet)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");

    const nlohmann::json parsed = nlohmann::json::parse(controller.save_to_json());

    EXPECT_FALSE(parsed.contains("ir_folder"));
}

TEST(Unit_Dsp, RigControllerLoadFromJsonRestoresTheIrFolder)
{
    RigController source(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    source.append("sushidsp.tubescreamer");
    source.append("sushidsp.jcm800");
    source.set_ir_folder("D:/irs/marshall");
    const std::string preset = source.save_to_json();

    RigController target(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    target.load_from_json(preset);

    EXPECT_EQ(target.ir_folder(), "D:/irs/marshall");
}

TEST(Unit_Dsp, RigControllerLoadsAPresetWithNoIrFolderKeyWithAnEmptyFolder)
{
    const std::string preset = R"({"nodes":[
        {"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":[]},
        {"id":1,"type":"sushidsp.jcm800","params":{},"inputs":[0]}
    ]})";

    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    controller.set_ir_folder("D:/irs/left_over");
    controller.load_from_json(preset);

    EXPECT_EQ(controller.ir_folder(), "");
}

// --- Rack-list selection tracking (apps/host_gui/SlotSelection.hpp) ---
//
// A SlotId stays valid across insert/move/remove, unlike the old int-index scheme, so the only
// remaining job is dropping a selection that no longer names a live slot.

TEST(Unit_Dsp, SlotSelectionStillValidReturnsTrueWhenSlotIsInTheList)
{
    std::vector<RigController::SlotId> order{ 3, 7, 9 };
    EXPECT_TRUE(slot_selection::still_valid(std::optional<RigController::SlotId>(7), order));
}

TEST(Unit_Dsp, SlotSelectionStillValidReturnsFalseWhenSlotWasRemoved)
{
    std::vector<RigController::SlotId> order{ 3, 9 };
    EXPECT_FALSE(slot_selection::still_valid(std::optional<RigController::SlotId>(7), order));
}

TEST(Unit_Dsp, SlotSelectionStillValidReturnsFalseForNoSelection)
{
    std::vector<RigController::SlotId> order{ 3, 7, 9 };
    EXPECT_FALSE(slot_selection::still_valid(std::nullopt, order));
}


TEST(Unit_Dsp, RigControllerStartsWithTheLiveInputAsItsSource)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });

    EXPECT_EQ(controller.input_source(), RigInputSource::LiveInput);
}

TEST(Unit_Dsp, RigControllerRemembersTheInputSourceItWasGiven)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });

    controller.set_input_source(RigInputSource::Both);

    EXPECT_EQ(controller.input_source(), RigInputSource::Both);
}

TEST(Unit_Dsp, RigControllerNamesALoadedTrackAfterItsFile)
{
    const std::string path = write_temp_float_wav("track.wav",
                                                  make_test_signal(256), 48000);
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });

    std::string error;
    ASSERT_TRUE(controller.load_track(0, path, error)) << error;

    EXPECT_TRUE(controller.track_is_loaded(0));
    const std::size_t stem_start = path.find_last_of("/\\") + 1;
    EXPECT_EQ(controller.track_name(0), path.substr(stem_start, path.size() - stem_start - 4));
    EXPECT_EQ(controller.track_path(0), path);
}

TEST(Unit_Dsp, RigControllerRefusesATrackFileItCannotRead)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });

    std::string error;
    EXPECT_FALSE(controller.load_track(0, "D:/no/such/take.wav", error));
    EXPECT_FALSE(controller.track_is_loaded(0));
    EXPECT_FALSE(error.empty());
}

TEST(Unit_Dsp, RigControllerClearingATrackEmptiesItsNameAndPath)
{
    const std::string path = write_temp_float_wav("track.wav",
                                                  make_test_signal(256), 48000);
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    std::string error;
    ASSERT_TRUE(controller.load_track(1, path, error)) << error;

    controller.clear_track(1);

    EXPECT_FALSE(controller.track_is_loaded(1));
    EXPECT_EQ(controller.track_name(1), "");
    EXPECT_EQ(controller.track_path(1), "");
}

TEST(Unit_Dsp, RigControllerSendsALoadedTrackToTheReampLeg)
{
    const std::string path = write_temp_float_wav("track.wav",
                                                  make_test_signal(256), 48000);
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    std::string error;
    ASSERT_TRUE(controller.load_track(2, path, error)) << error;

    ASSERT_TRUE(controller.set_reamp_track(2, error)) << error;

    ASSERT_TRUE(controller.reamp_track().has_value());
    EXPECT_EQ(*controller.reamp_track(), 2u);
}

TEST(Unit_Dsp, RigControllerRefusesToReampAnEmptyTrack)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });

    std::string error;
    EXPECT_FALSE(controller.set_reamp_track(3, error));
    EXPECT_FALSE(controller.reamp_track().has_value());
}

TEST(Unit_Dsp, RigControllerDropsTheReampLegWhenItsTrackIsCleared)
{
    const std::string path = write_temp_float_wav("track.wav",
                                                  make_test_signal(256), 48000);
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    std::string error;
    ASSERT_TRUE(controller.load_track(0, path, error)) << error;
    ASSERT_TRUE(controller.set_reamp_track(0, error)) << error;

    controller.clear_track(0);

    EXPECT_FALSE(controller.reamp_track().has_value());
}

TEST(Unit_Dsp, RigControllerReampLegAddsTheTakeIntoAnEmptyInputBuffer)
{
    const std::uint32_t     block = 64;
    const std::vector<float> signal = make_test_signal(block);
    const std::string        path =
        write_temp_float_wav("track.wav", signal, 48000);

    RigController controller(SushiDSP::PrepareInfo{ 48000.0, block, 2 });
    std::string error;
    ASSERT_TRUE(controller.load_track(0, path, error)) << error;
    ASSERT_TRUE(controller.set_reamp_track(0, error)) << error;

    std::vector<float> assembled(block, 0.0f);
    float*             assembled_pointer[1] = { assembled.data() };
    controller.transport().play();
    controller.reamp_player().process(assembled_pointer, 1, block, controller.transport());

    for (std::uint32_t frame = 0; frame < block; ++frame)
    {
        EXPECT_NEAR(assembled[frame], signal[frame], 1e-6f) << "frame " << frame;
    }
}

TEST(Unit_Dsp, RigControllerPresetCarriesTheTrackListAndTheLoopRange)
{
    const std::string path = write_temp_float_wav("track.wav",
                                                  make_test_signal(256), 48000);
    RigController source(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    source.append("sushidsp.tubescreamer");
    std::string error;
    ASSERT_TRUE(source.load_track(0, path, error)) << error;
    source.set_track_name(0, "rhythm");
    source.player().set_gain(0, 0.5f);
    source.player().set_pan(0, -0.75f);
    source.player().set_mute(0, true);
    source.transport().set_loop(4800, 9600);
    source.set_take_folder("D:/takes");

    RigController target(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    target.load_from_json(source.save_to_json());

    EXPECT_TRUE(target.track_is_loaded(0));
    EXPECT_EQ(target.track_name(0), "rhythm");
    EXPECT_EQ(target.track_path(0), path);
    EXPECT_NEAR(target.player().gain(0), 0.5f, 1e-6f);
    EXPECT_NEAR(target.player().pan(0), -0.75f, 1e-6f);
    EXPECT_TRUE(target.player().muted(0));
    EXPECT_TRUE(target.transport().has_loop());
    EXPECT_EQ(target.transport().loop_begin_frames(), 4800u);
    EXPECT_EQ(target.transport().loop_end_frames(), 9600u);
    EXPECT_EQ(target.take_folder(), "D:/takes");
}

TEST(Unit_Dsp, RigControllerPresetWithNoTrackKeyLoadsWithAnEmptyTrackList)
{
    const std::string preset = R"({"nodes":[
        {"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":[]}
    ]})";
    const std::string path = write_temp_float_wav("track.wav",
                                                  make_test_signal(256), 48000);

    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    std::string error;
    ASSERT_TRUE(controller.load_track(0, path, error)) << error;
    controller.transport().set_loop(100, 200);

    controller.load_from_json(preset);

    EXPECT_FALSE(controller.track_is_loaded(0));
    EXPECT_FALSE(controller.transport().has_loop());
}

TEST(Unit_Dsp, RigControllerPresetBringsItsOwnMixerStateOverASession)
{
    const std::string preset = R"({"nodes":[
        {"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":[]}
    ]})";
    const std::string path = write_temp_float_wav("track.wav",
                                                  make_test_signal(256), 48000);

    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    std::string error;
    ASSERT_TRUE(controller.load_track(0, path, error)) << error;
    controller.player().set_gain(0, 0.25f);
    controller.player().set_pan(0, 0.8f);
    controller.player().set_mute(0, true);
    controller.player().set_solo(0, true);

    controller.load_from_json(preset);

    EXPECT_NEAR(controller.player().gain(0), 1.0f, 1e-6f);
    EXPECT_NEAR(controller.player().pan(0), 0.0f, 1e-6f);
    EXPECT_FALSE(controller.player().muted(0));
    EXPECT_FALSE(controller.player().soloed(0));
}


TEST(Unit_Dsp, RigControllerReampingADryTakeReproducesTheRigsOwnOutput)
{
    const std::uint32_t      block = 64;
    const std::vector<float> dry = make_test_signal(block * 8);

    RigController live(SushiDSP::PrepareInfo{ 48000.0, block, 1 });
    live.append("sushidsp.tubescreamer");
    const std::vector<float> processed = render_through_rig(live, dry, block);

    const std::string take_path = write_temp_float_wav("dry.wav", dry, 48000);

    RigController reamp(SushiDSP::PrepareInfo{ 48000.0, block, 1 });
    reamp.append("sushidsp.tubescreamer");
    std::string error;
    ASSERT_TRUE(reamp.load_track(0, take_path, error)) << error;
    ASSERT_TRUE(reamp.set_reamp_track(0, error)) << error;
    reamp.set_input_source(RigInputSource::Track);
    reamp.transport().play();

    // The host's own callback order: assemble the input, run the rig, advance the clock.
    std::vector<float> reamped(dry.size(), 0.0f);
    std::vector<float> assembled(block, 0.0f);
    for (std::size_t offset = 0; offset + block <= dry.size(); offset += block)
    {
        std::fill(assembled.begin(), assembled.end(), 0.0f);
        float* assembled_pointer[1] = { assembled.data() };
        reamp.reamp_player().process(assembled_pointer, 1, block, reamp.transport());

        const float* input_pointer[1] = { assembled.data() };
        float*       output_pointer[1] = { reamped.data() + offset };
        SushiDSP::ProcessContext context{ output_pointer, input_pointer, block, 0.0 };
        reamp.active_graph()->process(context);

        reamp.transport().advance(block);
    }

    for (std::size_t frame = 0; frame < dry.size(); ++frame)
    {
        EXPECT_NEAR(reamped[frame], processed[frame], 1e-6f) << "frame " << frame;
    }
}


TEST(Unit_Dsp, RigControllerPlacesAClipAtTheFrameAsked)
{
    const std::string path = write_temp_float_wav("clip.wav", make_test_signal(256), 48000);
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });

    std::string error;
    ASSERT_TRUE(controller.place_clip(1, path, 12345, error)) << error;

    ASSERT_EQ(controller.clip_count(1), 1u);
    EXPECT_EQ(controller.clip(1, 0).path, path);
    EXPECT_EQ(controller.clip(1, 0).start_frame, 12345u);
    EXPECT_EQ(controller.clip(1, 0).take_offset, 0u);
    EXPECT_EQ(controller.clip(1, 0).length_frames, 256u);
    EXPECT_TRUE(controller.track_is_loaded(1));
    EXPECT_EQ(controller.track_name(1), path_stem_of(path));
}

TEST(Unit_Dsp, RigControllerRefusesAFileThatWillNotParseWithTheBrowsersMessage)
{
    const std::string path = temp_fixture_path("not_a_wav.wav");
    std::ofstream    out(path, std::ios::binary);
    out << "this is not a RIFF file";
    out.close();

    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    std::string   error;
    EXPECT_FALSE(controller.place_clip(0, path, 480, error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(controller.clip_count(0), 0u);
    EXPECT_FALSE(controller.track_is_loaded(0));
}

TEST(Unit_Dsp, RigControllerRefusesASeventeenthClipOnALane)
{
    const std::string path = write_temp_float_wav("clip.wav", make_test_signal(256), 48000);
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });

    std::string error;
    for (std::size_t index = 0; index < SushiDSP::TrackTimeline::kMaxClips; ++index)
    {
        ASSERT_TRUE(controller.place_clip(0, path, index * 512u, error)) << error;
    }
    ASSERT_EQ(controller.clip_count(0), SushiDSP::TrackTimeline::kMaxClips);

    EXPECT_FALSE(controller.place_clip(0, path, 16u * 512u, error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(controller.clip_count(0), SushiDSP::TrackTimeline::kMaxClips);
}

TEST(Unit_Dsp, RigControllerPlacesTwoClipsFromOneFileWithOneTakeInMemory)
{
    const std::string path = write_temp_float_wav("clip.wav", make_test_signal(256), 48000);
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });

    std::string error;
    ASSERT_TRUE(controller.place_clip(0, path, 0, error)) << error;
    ASSERT_TRUE(controller.place_clip(0, path, 4800, error)) << error;

    EXPECT_EQ(controller.clip_count(0), 2u);
    EXPECT_EQ(controller.player().takes().size(), 1u);
}

TEST(Unit_Dsp, RigControllerRefusesAClipThatOverlapsOneTheLaneHolds)
{
    const std::string path = write_temp_float_wav("clip.wav", make_test_signal(256), 48000);
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });

    std::string error;
    ASSERT_TRUE(controller.place_clip(0, path, 1000, error)) << error;
    EXPECT_FALSE(controller.place_clip(0, path, 1100, error));
    EXPECT_NE(error.find("overlap"), std::string::npos) << error;
    EXPECT_EQ(controller.clip_count(0), 1u);
    EXPECT_EQ(controller.clip(0, 0).start_frame, 1000u);
}

TEST(Unit_Dsp, RigControllerMovesAClipAndKeepsTheLaneSortedByStartFrame)
{
    const std::string path = write_temp_float_wav("clip.wav", make_test_signal(256), 48000);
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });

    std::string error;
    ASSERT_TRUE(controller.place_clip(0, path, 0, error)) << error;
    ASSERT_TRUE(controller.place_clip(0, path, 1000, error)) << error;

    ASSERT_TRUE(controller.move_clip(0, 0, 5000, error)) << error;
    EXPECT_EQ(controller.clip(0, 0).start_frame, 1000u);
    EXPECT_EQ(controller.clip(0, 1).start_frame, 5000u);

    EXPECT_FALSE(controller.move_clip(0, 1, 900, error));
    EXPECT_NE(error.find("overlap"), std::string::npos) << error;
    EXPECT_EQ(controller.clip(0, 1).start_frame, 5000u);

    ASSERT_TRUE(controller.remove_clip(0, 0, error)) << error;
    ASSERT_EQ(controller.clip_count(0), 1u);
    EXPECT_EQ(controller.clip(0, 0).start_frame, 5000u);
}

TEST(Unit_Dsp, RigControllerStopsARecordingIntoAClipAtTheArmedFrame)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    controller.set_take_folder(::testing::TempDir());
    controller.set_record_track(2);
    controller.transport().seek(12345);

    std::string error;
    ASSERT_TRUE(controller.arm_recording("RigControllerStopsARecording", error)) << error;
    EXPECT_EQ(controller.armed_frame(), 12345u);

    const std::uint32_t block = 64;
    std::vector<float>  dry(block, 0.0f);
    dry[0] = 0.5f;
    dry[1] = -0.25f;
    std::vector<float>  left(block, 0.0f);
    std::vector<float>  right(block, 0.0f);
    const float*        processed[2] = { left.data(), right.data() };
    ASSERT_TRUE(controller.recording_bus().write(processed, dry.data(), block));

    controller.stop_recording(&error);
    EXPECT_TRUE(error.empty()) << error;

    ASSERT_EQ(controller.clip_count(2), 1u);
    EXPECT_EQ(controller.clip(2, 0).start_frame, 12345u);
    EXPECT_EQ(controller.clip(2, 0).length_frames, block);
    EXPECT_NE(controller.clip(2, 0).path.find("-dry.wav"), std::string::npos);
}

TEST(Unit_Dsp, RigControllerReAmpsARecordedTakeFromTheBarItWasRecordedAt)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    controller.set_take_folder(::testing::TempDir());
    controller.set_record_track(0);

    const std::uint64_t bar_five = controller.musical_clock().frame_of(5, 1);
    ASSERT_EQ(bar_five, 384000u);
    controller.transport().seek(bar_five);

    std::string error;
    ASSERT_TRUE(controller.arm_recording("RigControllerReAmpsARecordedTake", error)) << error;
    EXPECT_EQ(controller.armed_frame(), bar_five);

    const std::uint32_t block = 64;
    std::vector<float>  dry(block, 0.0f);
    dry[0] = 0.5f;
    dry[1] = -0.25f;
    std::vector<float>  left(block, 0.0f);
    std::vector<float>  right(block, 0.0f);
    const float*        processed[2] = { left.data(), right.data() };
    ASSERT_TRUE(controller.recording_bus().write(processed, dry.data(), block));
    controller.stop_recording(&error);
    ASSERT_TRUE(error.empty()) << error;
    ASSERT_EQ(controller.clip_count(0), 1u);
    ASSERT_EQ(controller.clip(0, 0).start_frame, bar_five);

    ASSERT_TRUE(controller.set_reamp_track(0, error)) << error;

    // The re-amp leg is silent before the bar the take was recorded at, which is the whole point
    // of the stamp: docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md §5.
    std::vector<float> assembled(block, 0.0f);
    float*             assembled_pointer[1] = { assembled.data() };
    controller.transport().seek(0);
    controller.transport().play();
    controller.reamp_player().process(assembled_pointer, 1, block, controller.transport());
    for (std::uint32_t frame = 0; frame < block; ++frame)
    {
        EXPECT_NEAR(assembled[frame], 0.0f, 1e-9f) << "frame " << frame;
    }

    std::fill(assembled.begin(), assembled.end(), 0.0f);
    controller.transport().seek(bar_five);
    controller.reamp_player().process(assembled_pointer, 1, block, controller.transport());
    EXPECT_NEAR(assembled[0], 0.5f, 1e-6f);
    EXPECT_NEAR(assembled[1], -0.25f, 1e-6f);
}

TEST(Unit_Dsp, RigControllerRoundTripsATempoAndItsClipsThroughJson)
{
    const std::string first = write_temp_float_wav("clip_a.wav", make_test_signal(256), 48000);
    const std::string second = write_temp_float_wav("clip_b.wav", make_test_signal(128), 48000);

    RigController source(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    source.append("sushidsp.tubescreamer");
    source.musical_clock().set_tempo(137.5);
    source.musical_clock().set_meter(7, 8);
    source.metronome().set_enabled(true);
    source.metronome().set_level(0.25f);
    source.set_take_folder("D:/takes");

    std::string error;
    ASSERT_TRUE(source.place_clip(0, first, 0, error)) << error;
    ASSERT_TRUE(source.place_clip(0, second, 96000, error)) << error;
    ASSERT_TRUE(source.place_clip(3, first, 48000, error)) << error;
    ASSERT_TRUE(source.place_clip(3, second, 192000, error)) << error;
    source.set_track_name(3, "lead");
    source.transport().set_loop(4800, 9600);

    const std::string saved = source.save_to_json();

    RigController target(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    target.load_from_json(saved);

    EXPECT_EQ(target.save_to_json(), saved);
    ASSERT_EQ(target.clip_count(0), 2u);
    ASSERT_EQ(target.clip_count(3), 2u);
    EXPECT_EQ(target.clip(0, 0).path, first);
    EXPECT_EQ(target.clip(0, 1).start_frame, 96000u);
    EXPECT_EQ(target.clip(0, 1).length_frames, 128u);
    EXPECT_EQ(target.clip(3, 0).start_frame, 48000u);
    EXPECT_EQ(target.clip(3, 1).start_frame, 192000u);
    EXPECT_EQ(target.track_name(3), "lead");
    EXPECT_NEAR(target.musical_clock().beats_per_minute(), 137.5, 1e-9);
    EXPECT_EQ(target.musical_clock().beats_per_bar(), 7u);
    EXPECT_EQ(target.musical_clock().beat_unit(), 8u);
    EXPECT_TRUE(target.metronome().enabled());
    EXPECT_NEAR(target.metronome().level(), 0.25f, 1e-6f);
    EXPECT_EQ(target.transport().loop_begin_frames(), 4800u);
    EXPECT_EQ(target.transport().loop_end_frames(), 9600u);
}

TEST(Unit_Dsp, RigControllerLoadsAPreDtPresetAsOneClipAtFrameZeroAtOneTwentyBpm)
{
    const std::string path = write_temp_float_wav("legacy.wav", make_test_signal(256), 48000);

    nlohmann::json preset = nlohmann::json::parse(
        R"({"nodes":[{"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":[]}]})");
    nlohmann::json row = nlohmann::json::object();
    row["index"] = 2;
    row["name"] = "rhythm";
    row["path"] = path;
    row["gain"] = 0.5f;
    row["pan"] = -0.25f;
    row["muted"] = false;
    row["soloed"] = false;
    preset["tracks"] = nlohmann::json::array({ row });

    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    controller.musical_clock().set_tempo(90.0);
    controller.musical_clock().set_meter(3, 8);
    controller.metronome().set_enabled(true);

    controller.load_from_json(preset.dump());

    ASSERT_EQ(controller.clip_count(2), 1u);
    EXPECT_EQ(controller.clip(2, 0).path, path);
    EXPECT_EQ(controller.clip(2, 0).start_frame, 0u);
    EXPECT_EQ(controller.clip(2, 0).take_offset, 0u);
    EXPECT_EQ(controller.clip(2, 0).length_frames, 256u);
    EXPECT_EQ(controller.track_name(2), "rhythm");
    EXPECT_NEAR(controller.player().gain(2), 0.5f, 1e-6f);
    EXPECT_NEAR(controller.musical_clock().beats_per_minute(), 120.0, 1e-9);
    EXPECT_EQ(controller.musical_clock().beats_per_bar(), 4u);
    EXPECT_EQ(controller.musical_clock().beat_unit(), 4u);
    EXPECT_FALSE(controller.metronome().enabled());
}

TEST(Unit_Dsp, RigControllerSavesThePlaybackSpeed)
{
    RigController source(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    source.append("sushidsp.tubescreamer");
    source.transport().set_speed(0.4);
    EXPECT_NEAR(source.transport().speed(), 0.5, 1e-9);

    const std::string saved = source.save_to_json();
    EXPECT_NE(saved.find("playback_speed"), std::string::npos) << saved;

    RigController target(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    target.transport().set_speed(2.0);
    target.load_from_json(saved);
    EXPECT_NEAR(target.transport().speed(), 0.5, 1e-9);
}

TEST(Unit_Dsp, RigControllerLoadsAPresetWithoutASpeedAtUnity)
{
    RigController source(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    source.append("sushidsp.tubescreamer");
    nlohmann::json document = nlohmann::json::parse(source.save_to_json());
    document.at("tempo").erase("playback_speed");

    RigController target(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    target.transport().set_speed(2.0);
    target.load_from_json(document.dump());
    EXPECT_NEAR(target.transport().speed(), 1.0, 1e-9);
}

// --- The room: its scene in the preset, its publish pacing and its HRTF (DE6) ---

namespace
{
    /// @brief Compares two points field by field.
    bool same_point(const SushiDSP::room::Vec3& left, const SushiDSP::room::Vec3& right)
    {
        return left.x == right.x && left.y == right.y && left.z == right.z;
    }

    /// @brief Compares two scenes field by field, every patch and the cabinet included.
    bool same_scene(const SushiDSP::room::RoomScene& left, const SushiDSP::room::RoomScene& right)
    {
        if (!same_point(left.dimensions, right.dimensions) || left.surfaces != right.surfaces
            || left.scattering != right.scattering || left.patch_count != right.patch_count)
        {
            return false;
        }
        for (std::uint32_t index = 0; index < left.patch_count; ++index)
        {
            if (left.patches[index].surface != right.patches[index].surface
                || left.patches[index].material != right.patches[index].material
                || left.patches[index].area_m2 != right.patches[index].area_m2)
            {
                return false;
            }
        }
        return same_point(left.source_position, right.source_position)
            && left.source_yaw_rad == right.source_yaw_rad
            && left.cabinet.cone_radius_m == right.cabinet.cone_radius_m
            && left.cabinet.driver_spacing_m == right.cabinet.driver_spacing_m
            && left.cabinet.baffle_width_m == right.cabinet.baffle_width_m
            && left.cabinet.breakup_frequency_hz == right.cabinet.breakup_frequency_hz
            && same_point(left.listener_position, right.listener_position)
            && left.listener_yaw_rad == right.listener_yaw_rad
            && same_point(left.microphone_position, right.microphone_position)
            && left.microphone_yaw_rad == right.microphone_yaw_rad
            && left.microphone_pattern == right.microphone_pattern;
    }

    /// @brief Builds a scene that differs from RoomScene{} in every field a preset writes,
    /// every face's material and scattering word included.
    SushiDSP::room::RoomScene non_default_scene()
    {
        SushiDSP::room::RoomScene scene;
        scene.dimensions = { 4.25, 3.5, 2.6 };
        scene.surfaces = { SushiDSP::room::MaterialId::ConcreteBlockPainted,
                           SushiDSP::room::MaterialId::ConcreteUnpainted,
                           SushiDSP::room::MaterialId::ConcreteBlockPainted,
                           SushiDSP::room::MaterialId::ConcreteUnpainted,
                           SushiDSP::room::MaterialId::ConcreteBlockPainted,
                           SushiDSP::room::MaterialId::ConcreteUnpainted };
        scene.scattering = { SushiDSP::room::Scattering::Textured,
                             SushiDSP::room::Scattering::Cluttered,
                             SushiDSP::room::Scattering::Cluttered,
                             SushiDSP::room::Scattering::Textured,
                             SushiDSP::room::Scattering::Cluttered,
                             SushiDSP::room::Scattering::Textured };
        scene.patch_count = 2;
        scene.patches[0] = { SushiDSP::room::Surface::PlusX,
                             SushiDSP::room::MaterialId::GlassWindow, 1.2 };
        scene.patches[1] = { SushiDSP::room::Surface::MinusY,
                             SushiDSP::room::MaterialId::OpenDoorway, 1.6 };
        scene.source_position = { 0.6, 0.7, 0.45 };
        scene.source_yaw_rad = 0.35;
        scene.cabinet = SushiDSP::room::kCabinet1x12;
        scene.listener_position = { 3.1, 2.2, 1.15 };
        scene.listener_yaw_rad = -2.9;
        scene.microphone_position = { 0.66, 0.7, 0.45 };
        scene.microphone_yaw_rad = 3.0;
        scene.microphone_pattern = SushiDSP::room::PolarPattern::Supercardioid;
        return scene;
    }

    /// @brief Returns the path of the six-direction HRTF fixture convert_hrtf.py's self-test
    /// writes.
    std::string hrtf_fixture_path()
    {
        return std::string(SUSHIDSP_FIXTURE_DIR) + "/hrtf_self_test.hrtf";
    }
}

TEST(Unit_Dsp, RigControllerRoundTripsARoomSceneThroughJson)
{
    RigController source(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    source.append("sushidsp.tubescreamer");
    const RigController::SlotId room = source.append("sushidsp.room");
    ASSERT_TRUE(source.hosts_room(room));

    std::string error;
    const SushiDSP::room::RoomScene scene = non_default_scene();
    ASSERT_TRUE(source.set_room_scene(room, scene, error)) << error;
    ASSERT_NE(source.room_host(room), nullptr);
    EXPECT_TRUE(same_scene(source.room_host(room)->scene(), scene));

    const std::string saved = source.save_to_json();
    const nlohmann::json parsed = nlohmann::json::parse(saved);
    ASSERT_EQ(parsed.at("nodes").size(), 2u);
    EXPECT_FALSE(parsed.at("nodes").at(0).contains("room"));
    ASSERT_TRUE(parsed.at("nodes").at(1).contains("room"));
    const nlohmann::json& written = parsed.at("nodes").at(1).at("room");
    EXPECT_EQ(written.at("surfaces").at(4).get<std::string>(), "concrete_block_painted");
    EXPECT_EQ(written.at("scattering").at(2).get<std::string>(), "cluttered");
    EXPECT_EQ(written.at("patches").size(), 2u);
    EXPECT_EQ(written.at("microphone").at("pattern").get<std::string>(), "supercardioid");

    RigController target(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    target.load_from_json(saved);

    EXPECT_EQ(target.save_to_json(), saved);
    const std::vector<RigController::SlotId> order = target.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_TRUE(same_scene(target.room_scene(order[1]), scene));
    ASSERT_NE(target.room_host(order[1]), nullptr);
    EXPECT_TRUE(same_scene(target.room_host(order[1])->scene(), scene));
}

TEST(Unit_Dsp, RigControllerLoadsAPresetWithNoRoomAsTheStarterScene)
{
    const std::string preset = R"({"nodes":[
        {"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":[]},
        {"id":1,"type":"sushidsp.room","params":{},"inputs":[0]}
    ]})";

    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    controller.load_from_json(preset);

    const std::vector<RigController::SlotId> order = controller.slot_order();
    ASSERT_EQ(order.size(), 2u);
    ASSERT_TRUE(controller.hosts_room(order[1]));
    const SushiDSP::room::RoomScene starter = SushiDSP::room::starter_scene();
    EXPECT_TRUE(same_scene(controller.room_scene(order[1]), starter));
    EXPECT_EQ(controller.room_scene(order[1]).furnishing_count, starter.furnishing_count);
    ASSERT_NE(controller.room_host(order[1]), nullptr);
    EXPECT_TRUE(same_scene(controller.room_host(order[1])->scene(), starter));
    EXPECT_EQ(controller.room_host(order[1])->scene().furnishing_count, starter.furnishing_count);

    // A fresh node carries the starter scene and no drag has landed, so the slot took the node's
    // scene rather than publishing one.
    EXPECT_EQ(controller.room_publishes(order[1]), 0u);
}

TEST(Unit_Dsp, RigControllerCoalescesADragToThirtyPublishesPerSecond)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    const RigController::SlotId room = controller.append("sushidsp.room");
    ASSERT_NE(controller.room_host(room), nullptr);
    ASSERT_EQ(controller.room_publishes(room), 0u);

    // Three hundred scene edits, each a 1 cm move of the cabinet, as fast as the loop runs.
    constexpr int kEdits = 300;
    SushiDSP::room::RoomScene scene;
    std::string error;
    const auto started = std::chrono::steady_clock::now();
    for (int edit = 0; edit < kEdits; ++edit)
    {
        scene.source_position.x = 0.5 + 0.01 * edit;
        ASSERT_TRUE(controller.set_room_scene(room, scene, error)) << error;
    }
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - started;

    // The first edit publishes at once and every later one only after 1 / 30 s has passed since
    // the previous publish, so the count is bounded by the loop's own wall time: at most
    // 1 + floor(30 T), which is 3 for a loop inside 100 ms.
    const std::size_t published = controller.room_publishes(room);
    const std::size_t allowed = 1u + static_cast<std::size_t>(
        std::floor(elapsed.count() * RigController::kRoomPublishHz));
    EXPECT_GE(published, 1u);
    EXPECT_LE(published, allowed) << "in " << elapsed.count() * 1000.0 << " ms";
    if (elapsed.count() < 0.1)
    {
        EXPECT_LE(published, 3u);
    }

    // The newest scene waits for a frame past the interval, then reaches the node whole.
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    controller.reclaim();
    EXPECT_EQ(controller.room_publishes(room), published + 1u);
    EXPECT_NEAR(controller.room_host(room)->scene().source_position.x,
                0.5 + 0.01 * (kEdits - 1), 1e-12);

    // Nothing is pending any more, so a further frame publishes nothing.
    controller.reclaim();
    EXPECT_EQ(controller.room_publishes(room), published + 1u);
}

TEST(Unit_Dsp, RigControllerRefusesASceneOutsideTheRoomWithoutPublishing)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    const RigController::SlotId room = controller.append("sushidsp.room");

    SushiDSP::room::RoomScene outside;
    outside.listener_position.x = outside.dimensions.x + 1.0;
    std::string error;
    EXPECT_FALSE(controller.set_room_scene(room, outside, error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(controller.room_publishes(room), 0u);
    EXPECT_TRUE(same_scene(controller.room_scene(room), SushiDSP::room::starter_scene()));
}

TEST(Unit_Dsp, RigControllerInstallsAMeasuredHrtfIntoEveryRoomAndKeepsItAcrossARebuild)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    const RigController::SlotId first = controller.append("sushidsp.room");
    ASSERT_NE(controller.room_host(first), nullptr);
    EXPECT_EQ(controller.room_host(first)->hrtf_source(),
              SushiDSP::room::HrtfSource::SphericalHead);
    EXPECT_EQ(controller.hrtf_path(), "");

    std::string error;
    ASSERT_TRUE(controller.set_hrtf_path(hrtf_fixture_path(), error)) << error;
    EXPECT_EQ(error, "");
    EXPECT_EQ(controller.hrtf_path(), hrtf_fixture_path());
    EXPECT_EQ(controller.room_host(first)->hrtf_source(), SushiDSP::room::HrtfSource::Measured);

    // A structural edit rebuilds every node; the set goes back in with the scene.
    const RigController::SlotId second = controller.append("sushidsp.room");
    ASSERT_NE(controller.room_host(first), nullptr);
    ASSERT_NE(controller.room_host(second), nullptr);
    EXPECT_EQ(controller.room_host(first)->hrtf_source(), SushiDSP::room::HrtfSource::Measured);
    EXPECT_EQ(controller.room_host(second)->hrtf_source(),
              SushiDSP::room::HrtfSource::Measured);
}

TEST(Unit_Dsp, RigControllerReportsAMissingHrtfAndLeavesEveryRoomOnTheFallback)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    const RigController::SlotId room = controller.append("sushidsp.room");
    std::string error;
    ASSERT_TRUE(controller.set_hrtf_path(hrtf_fixture_path(), error)) << error;
    ASSERT_EQ(controller.room_host(room)->hrtf_source(), SushiDSP::room::HrtfSource::Measured);

    const std::string missing = std::string(SUSHIDSP_FIXTURE_DIR) + "/no_such_set.hrtf";
    EXPECT_FALSE(controller.set_hrtf_path(missing, error));
    EXPECT_NE(error.find(missing), std::string::npos) << error;
    EXPECT_EQ(controller.hrtf_path(), "");
    EXPECT_EQ(controller.room_host(room)->hrtf_source(),
              SushiDSP::room::HrtfSource::SphericalHead);

    // The next rebuild has no set to install, so the fresh node stays on its own fallback.
    controller.append("sushidsp.tubescreamer");
    ASSERT_NE(controller.room_host(room), nullptr);
    EXPECT_EQ(controller.room_host(room)->hrtf_source(),
              SushiDSP::room::HrtfSource::SphericalHead);
}

// --- Snapshot and edge accessors (apps/host_gui, Phase 2 chain-type rewiring) ---

TEST(Unit_Dsp, RigControllerSnapshotRestoreKeepsParametersAndCabinetPath)
{
    const std::string ir_path = write_temp_impulse_wav();

    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    const RigController::SlotId amp = controller.append("sushidsp.jcm800");
    controller.set_parameter(amp, "preamp", 0.9f);
    const RigController::SlotId cabinet = controller.append("sushidsp.cabinet");
    ASSERT_TRUE(controller.set_cabinet(cabinet, ir_path));

    const std::optional<RigController::SlotSnapshot> saved_amp = controller.snapshot(amp);
    const std::optional<RigController::SlotSnapshot> saved_cabinet = controller.snapshot(cabinet);
    ASSERT_TRUE(saved_amp.has_value());
    ASSERT_TRUE(saved_cabinet.has_value());
    EXPECT_EQ(saved_amp->descriptor_id, "sushidsp.jcm800");

    controller.remove(cabinet, false);
    controller.remove(amp, false);
    const RigController::SlotId restored_amp = controller.restore(*saved_amp, false);
    const RigController::SlotId restored_cabinet = controller.restore(*saved_cabinet, false);
    controller.connect(restored_amp, restored_cabinet, false);
    controller.publish();

    EXPECT_EQ(controller.descriptor_id(restored_amp), "sushidsp.jcm800");
    EXPECT_FLOAT_EQ(controller.get_parameter(restored_amp, "preamp"), 0.9f);
    EXPECT_EQ(controller.cabinet_path(restored_cabinet), ir_path);

    std::remove(ir_path.c_str());
}

TEST(Unit_Dsp, RigControllerEdgesListsStoredEdgesInOrder)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId b = controller.append("sushidsp.jcm800");
    const RigController::SlotId c = controller.append("sushidsp.tubescreamer");

    const std::vector<std::pair<RigController::SlotId, RigController::SlotId>> edges =
        controller.edges();

    ASSERT_EQ(edges.size(), 2u);
    EXPECT_EQ(edges[0], (std::pair<RigController::SlotId, RigController::SlotId>{ a, b }));
    EXPECT_EQ(edges[1], (std::pair<RigController::SlotId, RigController::SlotId>{ b, c }));
}

TEST(Unit_Dsp, RigControllerClearEdgesLeavesSlotsLive)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");
    ASSERT_EQ(controller.edges().size(), 1u);

    controller.clear_edges(false);

    EXPECT_TRUE(controller.edges().empty());
    EXPECT_EQ(controller.slot_order().size(), 2u);
}

TEST(Unit_Dsp, SessionTimelineAddAfterRemoveReturnsTheCountToWhereItStarted)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    SessionTimeline       session(info);
    const std::size_t     before = session.track_count();
    ASSERT_GT(before, 0u);

    std::string error;
    ASSERT_TRUE(session.remove_track(before - 1, error)) << error;
    EXPECT_EQ(session.track_count(), before - 1);

    ASSERT_TRUE(session.add_track(error)) << error;
    EXPECT_EQ(session.track_count(), before);
    EXPECT_EQ(session.track_name(before - 1), SessionTimeline::default_track_name(before - 1));
    EXPECT_EQ(session.track_color(before - 1), SessionTimeline::palette_color(before - 1));

    EXPECT_FALSE(session.add_track(error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(session.track_count(), before);
}

TEST(Unit_Dsp, SessionTimelineRemovingTheMiddleTrackMovesTheOthersDownWithTheirNamesAndColors)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    SessionTimeline       session(info);
    for (std::size_t index = 0; index < session.track_count(); ++index)
    {
        session.set_track_name(index, "lane " + std::to_string(index));
    }
    const std::size_t                 before = session.track_count();
    const SessionTimeline::TrackColor color_of_two = session.track_color(2);
    const SessionTimeline::TrackColor color_of_four = session.track_color(4);

    std::string error;
    ASSERT_TRUE(session.remove_track(3, error)) << error;

    EXPECT_EQ(session.track_count(), before - 1);
    EXPECT_EQ(session.track_name(2), "lane 2");
    EXPECT_EQ(session.track_color(2), color_of_two);
    EXPECT_EQ(session.track_name(3), "lane 4");
    EXPECT_EQ(session.track_color(3), color_of_four);
}

TEST(Unit_Dsp, SessionTimelineTrackColorAndCountSurviveAPresetRoundTrip)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    SlotGraph             graph(info);
    SessionTimeline       session(info);
    RigPersistence        codec(info, graph, session);
    graph.append("sushidsp.tubescreamer");

    std::string error;
    ASSERT_TRUE(session.remove_track(session.track_count() - 1, error)) << error;
    session.set_track_name(1, "rhythm");
    session.set_track_color(1, 0x123456FFu);
    const std::size_t   expected_count = session.track_count();
    const std::string   json_text = codec.save_to_json();

    SlotGraph       reloaded_graph(info);
    SessionTimeline reloaded(info);
    RigPersistence  reloaded_codec(info, reloaded_graph, reloaded);
    reloaded_codec.load_from_json(json_text);

    EXPECT_EQ(reloaded.track_count(), expected_count);
    EXPECT_EQ(reloaded.track_name(1), "rhythm");
    EXPECT_EQ(reloaded.track_color(1), 0x123456FFu);
}

TEST(Unit_Dsp, SessionTimelinePresetWithoutColorsLoadsThePaletteColors)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    SlotGraph             graph(info);
    SessionTimeline       session(info);
    RigPersistence        codec(info, graph, session);
    graph.append("sushidsp.tubescreamer");
    session.set_track_name(0, "rhythm");

    nlohmann::json document = nlohmann::json::parse(codec.save_to_json());
    document.erase("track_count");
    for (nlohmann::json& track_json : document.at("tracks"))
    {
        track_json.erase("color");
    }

    SlotGraph       reloaded_graph(info);
    SessionTimeline reloaded(info);
    RigPersistence  reloaded_codec(info, reloaded_graph, reloaded);
    reloaded_codec.load_from_json(document.dump());

    EXPECT_EQ(reloaded.track_count(), SessionTimeline::kMaxTracks);
    EXPECT_EQ(reloaded.track_name(0), "rhythm");
    for (std::size_t index = 0; index < reloaded.track_count(); ++index)
    {
        EXPECT_EQ(reloaded.track_color(index), SessionTimeline::palette_color(index));
    }
}

TEST(Unit_Dsp, SessionTimelinePresetWithAMalformedColorLeavesTheTimelineAlone)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    SlotGraph             graph(info);
    SessionTimeline       session(info);
    RigPersistence        codec(info, graph, session);
    graph.append("sushidsp.tubescreamer");
    session.set_track_name(0, "rhythm");
    session.set_track_color(0, 0x123456FFu);

    nlohmann::json document = nlohmann::json::parse(codec.save_to_json());
    document.at("tracks").at(0).at("color") = nlohmann::json::array({ 1, 2 });

    EXPECT_THROW(codec.load_from_json(document.dump()), std::runtime_error);
    EXPECT_EQ(session.track_name(0), "rhythm");
    EXPECT_EQ(session.track_color(0), 0x123456FFu);
    EXPECT_EQ(session.track_count(), SessionTimeline::kMaxTracks);
}

TEST(Unit_Dsp, RigControllerRoundTripsThreeFurnishingsThroughJson)
{
    using SushiDSP::room::FurnitureId;
    RigController source(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    const RigController::SlotId room = source.append("sushidsp.room");
    ASSERT_TRUE(source.hosts_room(room));

    SushiDSP::room::RoomScene scene = non_default_scene();
    scene.furnishing_count = 3;
    scene.furnishings[0] = { FurnitureId::WardrobeModule, 4.5 };
    scene.furnishings[1] = { FurnitureId::SofaTwoSeat, 2.0 };
    scene.furnishings[2] = { FurnitureId::WardrobeModule, 1.25 };
    std::string error;
    ASSERT_TRUE(source.set_room_scene(room, scene, error)) << error;

    const std::string saved = source.save_to_json();
    const nlohmann::json written =
        nlohmann::json::parse(saved).at("nodes").at(0).at("room").at("furnishings");
    ASSERT_EQ(written.size(), 3u);
    EXPECT_EQ(written.at(1).at("item").get<std::string>(), "sofa_two_seat");
    EXPECT_EQ(written.at(2).at("quantity").get<double>(), 1.25);

    RigController target(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    target.load_from_json(saved);
    EXPECT_EQ(target.save_to_json(), saved);

    const std::vector<RigController::SlotId> order = target.slot_order();
    ASSERT_EQ(order.size(), 1u);
    const SushiDSP::room::RoomScene loaded = target.room_scene(order[0]);
    ASSERT_EQ(loaded.furnishing_count, 3u);
    for (std::uint32_t index = 0; index < 3u; ++index)
    {
        EXPECT_EQ(loaded.furnishings[index].item, scene.furnishings[index].item);
        EXPECT_EQ(loaded.furnishings[index].quantity, scene.furnishings[index].quantity);
    }
}

TEST(Unit_Dsp, RigControllerWritesNoFurnishingsFieldForABareRoom)
{
    RigController source(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    const RigController::SlotId room = source.append("sushidsp.room");
    std::string error;
    ASSERT_TRUE(source.set_room_scene(room, non_default_scene(), error)) << error;

    const nlohmann::json parsed = nlohmann::json::parse(source.save_to_json());
    EXPECT_FALSE(parsed.at("nodes").at(0).at("room").contains("furnishings"));
}

TEST(Unit_Dsp, RigControllerLoadsARoomWithoutFurnishingsAsAnUnfurnishedScene)
{
    const std::string preset = R"({"nodes":[
        {"id":0,"type":"sushidsp.room","params":{},"inputs":[],
         "room":{"dimensions":[4.0,3.0,2.5]}}
    ]})";

    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    controller.load_from_json(preset);

    const std::vector<RigController::SlotId> order = controller.slot_order();
    ASSERT_EQ(order.size(), 1u);
    const SushiDSP::room::RoomScene loaded = controller.room_scene(order[0]);
    EXPECT_EQ(loaded.dimensions.x, 4.0);
    EXPECT_EQ(loaded.furnishing_count, 0u);
}

TEST(Unit_Dsp, RigControllerRefusesAFurnishingNoTableRowNames)
{
    const std::string preset = R"({"nodes":[
        {"id":0,"type":"sushidsp.room","params":{},"inputs":[],
         "room":{"furnishings":[{"item":"no_such_row","quantity":1.0}]}}
    ]})";

    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    EXPECT_THROW(controller.load_from_json(preset), std::runtime_error);
}

TEST(Unit_Dsp, RigControllerRefusesMoreFurnishingRowsThanARoomCarries)
{
    nlohmann::json rows = nlohmann::json::array();
    for (std::size_t index = 0; index <= SushiDSP::room::RoomScene::kMaxFurnishings; ++index)
    {
        rows.push_back({ { "item", "sofa_two_seat" }, { "quantity", 1.0 } });
    }
    const nlohmann::json preset = { { "nodes", nlohmann::json::array(
        { { { "id", 0 }, { "type", "sushidsp.room" }, { "params", nlohmann::json::object() },
            { "inputs", nlohmann::json::array() },
            { "room", { { "furnishings", rows } } } } }) } };

    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    EXPECT_THROW(controller.load_from_json(preset.dump()), std::runtime_error);
}

TEST(Unit_Dsp, RigControllerRoundTripsTheBuiltRoomLossThroughJson)
{
    RigController source(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    const RigController::SlotId room = source.append("sushidsp.room");
    SushiDSP::room::RoomScene scene = non_default_scene();
    scene.built_room_loss = true;
    std::string error;
    ASSERT_TRUE(source.set_room_scene(room, scene, error)) << error;

    const std::string saved = source.save_to_json();
    EXPECT_TRUE(nlohmann::json::parse(saved).at("nodes").at(0).at("room")
                    .at("built_room_loss").get<bool>());

    RigController target(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    target.load_from_json(saved);
    EXPECT_EQ(target.save_to_json(), saved);
    EXPECT_TRUE(target.room_scene(target.slot_order().at(0)).built_room_loss);
}

TEST(Unit_Dsp, RigControllerLoadsARoomWithoutTheBuiltRoomLossFieldWithTheLossOff)
{
    const std::string preset = R"({"nodes":[
        {"id":0,"type":"sushidsp.room","params":{},"inputs":[],
         "room":{"dimensions":[4.0,3.0,2.5]}}
    ]})";

    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    controller.load_from_json(preset);
    EXPECT_FALSE(controller.room_scene(controller.slot_order().at(0)).built_room_loss);
}

TEST(Unit_Dsp, RigControllerRefusesABuiltRoomLossThatIsNotABoolean)
{
    const std::string preset = R"({"nodes":[
        {"id":0,"type":"sushidsp.room","params":{},"inputs":[],
         "room":{"built_room_loss":1}}
    ]})";

    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 2 });
    EXPECT_THROW(controller.load_from_json(preset), std::runtime_error);
}
