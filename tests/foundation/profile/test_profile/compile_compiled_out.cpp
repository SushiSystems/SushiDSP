/**************************************************************************/
/* compile_compiled_out.cpp                                               */
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
 * @file compile_compiled_out.cpp
 * @brief Compiles profile.hpp with the profiler off and checks that no macro needs its header.
 * @author Mustafa Garip
 */

// The generated config comes first, and this file then sees a build with the profiler off.
#include <SushiDSP/foundation/profile/profile_config.hpp>

#undef SD_ENABLE_PROFILER
#define SD_ENABLE_PROFILER 0

#include <SushiDSP/foundation/profile/profile.hpp>

#ifdef SP_ZONE
#error "profile.hpp included a SushiProfiler header with SD_ENABLE_PROFILER 0"
#endif

namespace
{
    /// @brief Uses every macro as a statement, which a constant expression accepts only when it
    /// does nothing at run time.
    constexpr int use_every_macro()
    {
        SD_ZONE("test.zone");
        SD_ZONE_GROUP("test.zone", "Test");
        SD_ZONE_NAMED("test.named", "Test", 1);
        SD_ZONE_FINE("test.fine");
        SD_COUNTER("test.counter", "count", Add, 1.0);
        SD_INSTANT("test.instant");
        SD_LANE(1);
        return 0;
    }

    static_assert(use_every_macro() == 0, "every macro is a statement that does nothing");
} // namespace
