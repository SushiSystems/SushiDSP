/**************************************************************************/
/* test_tracks.cpp                                                        */
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
 * @file test_tracks.cpp
 * @brief Tests the tracks a RigController loads, the re-amp leg that plays one into the rig, and the
 * mixer state a preset carries for them.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "rig_controller.hpp"
#include "support/rig_products_in_memory_files.hpp"
#include "support/wav_fixture.hpp"
#include <SushiDSP/nodes/capture/transport.hpp>

namespace
{
    /// @brief Holds the rigs the track tests build, on the catalogue and the in-memory files of the
    /// fixture.
    class Small_RigControllerTracks : public TestSupport::RigProductsInMemoryFiles
    {
    };

    /// @brief Holds the rigs the re-amp tests build that read no file.
    class Small_RigControllerReamp : public TestSupport::RigProducts
    {
    };

    /// @brief Builds a rig with one loaded track named "rhythm" at gain 0.5, pan -0.75, muted,
    /// a loop from frame 4800 to 9600 and the take folder "D:/takes", and saves it.
    /// @param source The rig to build in.
    /// @param path The track's file.
    /// @return The preset text.
    std::string save_rig_with_one_track(RigController& source, const std::string& path)
    {
        source.append("sushidsp.tubescreamer");
        std::string error;
        EXPECT_TRUE(source.load_track(0, path, error)) << error;
        source.set_track_name(0, "rhythm");
        source.player().set_gain(0, 0.5f);
        source.player().set_pan(0, -0.75f);
        source.player().set_mute(0, true);
        source.transport().set_loop(4800, 9600);
        source.set_take_folder("D:/takes");
        return source.save_to_json();
    }

    const char* const K_ONE_SCREAMER_PRESET = R"({"nodes":[
        {"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":[]}
    ]})";

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
            const float* input_pointer[1] = {input.data() + offset};
            float* output_pointer[1] = {output.data() + offset};
            SushiDSP::ProcessContext context{output_pointer, input_pointer, block, 0.0};
            controller.active_graph()->process(context);
        }
        return output;
    }

    /// @brief Plays a re-amp take through a rig in the host's own callback order: assemble the
    /// input, run the rig, advance the clock.
    /// @param controller The rig, its re-amp track set and its transport playing.
    /// @param frames The take's length, a multiple of block.
    /// @param block Frames per callback.
    /// @return The rig's output, frames long.
    std::vector<float> reamp_through_rig(RigController& controller, std::size_t frames,
                                         std::uint32_t block)
    {
        std::vector<float> reamped(frames, 0.0f);
        std::vector<float> assembled(block, 0.0f);
        for (std::size_t offset = 0; offset + block <= frames; offset += block)
        {
            std::fill(assembled.begin(), assembled.end(), 0.0f);
            float* assembled_pointer[1] = {assembled.data()};
            controller.reamp_player().process(assembled_pointer, 1, block, controller.transport());

            const float* input_pointer[1] = {assembled.data()};
            float* output_pointer[1] = {reamped.data() + offset};
            SushiDSP::ProcessContext context{output_pointer, input_pointer, block, 0.0};
            controller.active_graph()->process(context);

            controller.transport().advance(block);
        }
        return reamped;
    }
} // namespace

TEST_F(Small_RigControllerTracks, NamesALoadedTrackAfterItsFile)
{
    const std::string path =
        add_wav("track.wav", TestSupport::make_test_signal(256), 48000);
    RigController controller = rig({48000.0, 64, 2});

    std::string error;
    ASSERT_TRUE(controller.load_track(0, path, error)) << error;

    EXPECT_TRUE(controller.track_is_loaded(0));
    EXPECT_EQ(controller.track_name(0), TestSupport::path_stem_of(path));
    EXPECT_EQ(controller.track_path(0), path);
}

TEST_F(Small_RigControllerTracks, RefusesATrackFileItCannotRead)
{
    RigController controller = rig({48000.0, 64, 2});

    std::string error;
    EXPECT_FALSE(controller.load_track(0, "D:/no/such/take.wav", error));

    EXPECT_FALSE(controller.track_is_loaded(0));
    EXPECT_FALSE(error.empty());
}

TEST_F(Small_RigControllerTracks, ClearingATrackEmptiesItsNameAndPath)
{
    const std::string path =
        add_wav("track.wav", TestSupport::make_test_signal(256), 48000);
    RigController controller = rig({48000.0, 64, 2});
    std::string error;
    ASSERT_TRUE(controller.load_track(1, path, error)) << error;

    controller.clear_track(1);

    EXPECT_FALSE(controller.track_is_loaded(1));
    EXPECT_EQ(controller.track_name(1), "");
    EXPECT_EQ(controller.track_path(1), "");
}

TEST_F(Small_RigControllerTracks, SendsALoadedTrackToTheReampLeg)
{
    const std::string path =
        add_wav("track.wav", TestSupport::make_test_signal(256), 48000);
    RigController controller = rig({48000.0, 64, 2});
    std::string error;
    ASSERT_TRUE(controller.load_track(2, path, error)) << error;

    ASSERT_TRUE(controller.set_reamp_track(2, error)) << error;

    ASSERT_TRUE(controller.reamp_track().has_value());
    EXPECT_EQ(*controller.reamp_track(), 2u);
}

TEST_F(Small_RigControllerReamp, RefusesToReampAnEmptyTrack)
{
    RigController controller = rig({48000.0, 64, 2});

    std::string error;
    EXPECT_FALSE(controller.set_reamp_track(3, error));

    EXPECT_FALSE(controller.reamp_track().has_value());
}

TEST_F(Small_RigControllerTracks, DropsTheReampLegWhenItsTrackIsCleared)
{
    const std::string path =
        add_wav("track.wav", TestSupport::make_test_signal(256), 48000);
    RigController controller = rig({48000.0, 64, 2});
    std::string error;
    ASSERT_TRUE(controller.load_track(0, path, error)) << error;
    ASSERT_TRUE(controller.set_reamp_track(0, error)) << error;

    controller.clear_track(0);

    EXPECT_FALSE(controller.reamp_track().has_value());
}

TEST_F(Small_RigControllerTracks, ReampLegAddsTheTakeIntoAnEmptyInputBuffer)
{
    const std::uint32_t block = 64;
    const std::vector<float> signal = TestSupport::make_test_signal(block);
    const std::string path = add_wav("track.wav", signal, 48000);
    RigController controller = rig({48000.0, block, 2});
    std::string error;
    ASSERT_TRUE(controller.load_track(0, path, error)) << error;
    ASSERT_TRUE(controller.set_reamp_track(0, error)) << error;
    std::vector<float> assembled(block, 0.0f);
    float* assembled_pointer[1] = {assembled.data()};
    controller.transport().play();

    controller.reamp_player().process(assembled_pointer, 1, block, controller.transport());

    // The leg runs at unity gain and speed, so it adds each float32 sample of the take to zero.
    TestSupport::expect_same_signal(assembled, signal);
}

TEST_F(Small_RigControllerTracks, PresetCarriesTheTrackNameAndPath)
{
    const std::string path =
        add_wav("track.wav", TestSupport::make_test_signal(256), 48000);
    RigController source = rig({48000.0, 64, 2});
    const std::string saved = save_rig_with_one_track(source, path);
    RigController target = rig({48000.0, 64, 2});

    target.load_from_json(saved);

    EXPECT_TRUE(target.track_is_loaded(0));
    EXPECT_EQ(target.track_name(0), "rhythm");
    EXPECT_EQ(target.track_path(0), path);
}

TEST_F(Small_RigControllerTracks, PresetCarriesTheTrackMixerState)
{
    const std::string path =
        add_wav("track.wav", TestSupport::make_test_signal(256), 48000);
    RigController source = rig({48000.0, 64, 2});
    const std::string saved = save_rig_with_one_track(source, path);
    RigController target = rig({48000.0, 64, 2});

    target.load_from_json(saved);

    EXPECT_EQ(target.player().gain(0), 0.5f);
    EXPECT_EQ(target.player().pan(0), -0.75f);
    EXPECT_TRUE(target.player().muted(0));
}

TEST_F(Small_RigControllerTracks, PresetCarriesTheLoopRange)
{
    const std::string path =
        add_wav("track.wav", TestSupport::make_test_signal(256), 48000);
    RigController source = rig({48000.0, 64, 2});
    const std::string saved = save_rig_with_one_track(source, path);
    RigController target = rig({48000.0, 64, 2});

    target.load_from_json(saved);

    EXPECT_TRUE(target.transport().has_loop());
    EXPECT_EQ(target.transport().loop_begin_frames(), 4800u);
    EXPECT_EQ(target.transport().loop_end_frames(), 9600u);
}

TEST_F(Small_RigControllerTracks, PresetCarriesTheTakeFolder)
{
    const std::string path =
        add_wav("track.wav", TestSupport::make_test_signal(256), 48000);
    RigController source = rig({48000.0, 64, 2});
    const std::string saved = save_rig_with_one_track(source, path);
    RigController target = rig({48000.0, 64, 2});

    target.load_from_json(saved);

    EXPECT_EQ(target.take_folder(), "D:/takes");
}

TEST_F(Small_RigControllerTracks, PresetWithNoTrackKeyLoadsWithAnEmptyTrackList)
{
    const std::string path =
        add_wav("track.wav", TestSupport::make_test_signal(256), 48000);
    RigController controller = rig({48000.0, 64, 2});
    std::string error;
    ASSERT_TRUE(controller.load_track(0, path, error)) << error;

    controller.load_from_json(K_ONE_SCREAMER_PRESET);

    EXPECT_FALSE(controller.track_is_loaded(0));
}

TEST_F(Small_RigControllerReamp, PresetWithNoLoopKeyLoadsWithNoLoop)
{
    RigController controller = rig({48000.0, 64, 2});
    controller.transport().set_loop(100, 200);

    controller.load_from_json(K_ONE_SCREAMER_PRESET);

    EXPECT_FALSE(controller.transport().has_loop());
}

TEST_F(Small_RigControllerTracks, PresetBringsItsOwnMixerStateOverASession)
{
    const std::string path =
        add_wav("track.wav", TestSupport::make_test_signal(256), 48000);
    RigController controller = rig({48000.0, 64, 2});
    std::string error;
    ASSERT_TRUE(controller.load_track(0, path, error)) << error;
    controller.player().set_gain(0, 0.25f);
    controller.player().set_pan(0, 0.8f);
    controller.player().set_mute(0, true);
    controller.player().set_solo(0, true);

    controller.load_from_json(K_ONE_SCREAMER_PRESET);

    EXPECT_EQ(controller.player().gain(0), 1.0f);
    EXPECT_EQ(controller.player().pan(0), 0.0f);
    EXPECT_FALSE(controller.player().muted(0));
    EXPECT_FALSE(controller.player().soloed(0));
}

TEST_F(Small_RigControllerTracks, ReampingADryTakeReproducesTheRigsOwnOutput)
{
    const std::uint32_t block = 64;
    const std::vector<float> dry = TestSupport::make_test_signal(block * 8);
    RigController live = rig({48000.0, block, 1});
    live.append("sushidsp.tubescreamer");
    const std::vector<float> processed = render_through_rig(live, dry, block);
    const std::string take_path = add_wav("dry.wav", dry, 48000);
    RigController reamp = rig({48000.0, block, 1});
    reamp.append("sushidsp.tubescreamer");
    std::string error;
    ASSERT_TRUE(reamp.load_track(0, take_path, error)) << error;
    ASSERT_TRUE(reamp.set_reamp_track(0, error)) << error;
    reamp.set_input_source(RigInputSource::Track);
    reamp.transport().play();

    const std::vector<float> reamped = reamp_through_rig(reamp, dry.size(), block);

    // Both rigs run the same node code over the same input samples, so they agree bit for bit.
    TestSupport::expect_same_signal(reamped, processed);
}
