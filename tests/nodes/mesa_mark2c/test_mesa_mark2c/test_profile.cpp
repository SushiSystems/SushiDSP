/**************************************************************************/
/* test_profile.cpp                                                       */
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
 * @file test_profile.cpp
 * @brief Tests that MesaMark2c writes its region zones and its per-block counters.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/benchmark_harness/profiled_node.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>
#include <SushiDSP/nodes/mesa_mark2c/mesa_mark2c.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace
{
    /// @brief Holds the warm-up periods of the run below.
    constexpr std::uint32_t K_WARMUP = 2;

    /// @brief Holds the active periods of the run below.
    constexpr std::uint32_t K_ACTIVE = 4;

    /// @brief Holds the region zones one block writes, one per control period.
    constexpr std::uint32_t K_ZONES_PER_BLOCK =
        SushiDSP::bench::K_BLOCK_SIZE / SushiDSP::MesaMark2c::K_CONTROL_PERIOD_FRAMES;

    /// @brief Fails for every count that is not above zero.
    void expect_all_positive(const std::vector<double>& counts)
    {
        for (const double count : counts)
        {
            EXPECT_GT(count, 0.0);
        }
    }
} // namespace

/// @brief Runs the profiled node once for a test and skips the test in a build that records nothing.
class Medium_MesaMark2cProfile : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            run_ = SushiDSP::bench::profile_node<SushiDSP::MesaMark2c>(K_WARMUP, K_ACTIVE);
            if (!run_->start.started())
            {
                GTEST_SKIP() << "this build records nothing";
            }
        }

        std::optional<SushiDSP::profile::ProfiledRun> run_;
};

TEST_F(Medium_MesaMark2cProfile, ProcessWritesEachRegionZoneOncePerControlPeriodAndItsCounters)
{
    namespace regions = SushiDSP::profile::names::mesa_mark2c;
    const SushiDSP::profile::ProfiledRun& run = *run_;

    EXPECT_EQ(run.record.active_periods(), K_ACTIVE);
    EXPECT_EQ(run.record.zone_calls(regions::K_INPUT_ZONE), K_ACTIVE * K_ZONES_PER_BLOCK);
    EXPECT_EQ(run.record.zone_calls(regions::K_UPSAMPLE_ZONE), K_ACTIVE * K_ZONES_PER_BLOCK);
    EXPECT_EQ(run.record.zone_calls(regions::K_LOOP_ZONE), K_ACTIVE * K_ZONES_PER_BLOCK);
    EXPECT_EQ(run.record.zone_calls(regions::K_DOWNSAMPLE_ZONE), K_ACTIVE * K_ZONES_PER_BLOCK);
    EXPECT_EQ(run.record.zone_calls(regions::K_CLAMP_ZONE), K_ACTIVE * K_ZONES_PER_BLOCK);
    const SushiDSP::profile::ProfileSubject* const clamped =
        run.record.find("counter:output.clamped");
    ASSERT_NE(clamped, nullptr);
    EXPECT_EQ(clamped->samples.size(), K_ACTIVE);
    const SushiDSP::profile::ProfileSubject* const solves =
        run.record.find("counter:newton.solves");
    ASSERT_NE(solves, nullptr);
    expect_all_positive(solves->samples);
}
