/**************************************************************************/
/* recording_build_test.hpp                                               */
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
 * @file recording_build_test.hpp
 * @brief Declares the fixture base of the tests that need a build that records.
 * @author Mustafa Garip
 */

#pragma once

#include <SushiDSP/foundation/profile/profile_runtime.hpp>

#include <gtest/gtest.h>

namespace SushiDSP::profile::test
{
    /// @brief Skips each test of a build whose profiler records nothing, so no test body does.
    class RecordingBuildTest : public ::testing::Test
    {
        protected:
            /// @brief Skips the test when SD_ENABLE_PROFILER is 0.
            void SetUp() override
            {
                if (!K_PROFILER_BUILT)
                    GTEST_SKIP() << "this build records nothing";
            }
    };
} // namespace SushiDSP::profile::test
