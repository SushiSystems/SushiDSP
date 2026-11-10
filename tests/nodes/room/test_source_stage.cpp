/**************************************************************************/
/* test_source_stage.cpp                                                  */
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
 * @file test_source_stage.cpp
 * @brief Tests that a source stage goes live when it adopts a set and is idle again after reset.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/environment_render_set.hpp>
#include <SushiDSP/nodes/room/source_stage.hpp>

#include <cstdint>
#include <memory>

namespace
{
    using SushiDSP::Room::EnvironmentRenderSet;
    using SushiDSP::Room::SourceStage;

    constexpr double K_RATE = 48000.0;
    constexpr std::uint32_t K_ALIGNMENT_FRAMES = 128;
    constexpr std::uint32_t K_BLOCK = 64;

    /// @brief Holds a crossover corner inside the band the stage's filter accepts.
    constexpr float K_CROSSOVER_HZ = 1000.0f;

    /// @brief Returns a set of one cabinet whose pairs are silent, with a usable crossover corner.
    std::unique_ptr<EnvironmentRenderSet> silent_set()
    {
        auto set = std::make_unique<EnvironmentRenderSet>();
        set->source_count = 1;
        set->pairs[0][0].crossover_hz = K_CROSSOVER_HZ;
        return set;
    }

    /// @brief Prepares a stage at K_RATE and lets it adopt the silent set.
    void prepare_and_adopt(SourceStage& stage)
    {
        stage.prepare(K_RATE, K_ALIGNMENT_FRAMES, K_BLOCK);
        stage.adopt(*silent_set(), 0);
    }
} // namespace

TEST(Small_SourceStage, IsNotLiveBeforeItAdoptsASet)
{
    SourceStage stage;
    stage.prepare(K_RATE, K_ALIGNMENT_FRAMES, K_BLOCK);

    EXPECT_FALSE(stage.live());
}

TEST(Small_SourceStage, IsLiveOnceItHasAdoptedASet)
{
    SourceStage stage;
    prepare_and_adopt(stage);

    EXPECT_TRUE(stage.live());
}

TEST(Small_SourceStage, IsNotLiveAfterResetOfAnAdoptedSet)
{
    SourceStage stage;
    prepare_and_adopt(stage);

    stage.reset();

    EXPECT_FALSE(stage.live());
}
