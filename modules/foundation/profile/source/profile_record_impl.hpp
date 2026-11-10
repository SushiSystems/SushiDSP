/**************************************************************************/
/* profile_record_impl.hpp                                                */
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
 * @file profile_record_impl.hpp
 * @brief Declares what a ProfileRecord holds: SushiProfiler's run and the subjects read from it.
 * @author Mustafa Garip
 */

#pragma once

#include <SushiDSP/foundation/profile/profile_record.hpp>

#include <SushiProfiler/record/run_record.hpp>

namespace SushiDSP::profile
{
    /// @brief The captured run and its subjects, shared by every copy of a ProfileRecord.
    struct ProfileRecord::Impl
    {
            /// @brief Takes @p record and reads its subjects and period counts.
            explicit Impl(SushiProfiler::RunRecord record);

            /// @brief Takes @p listed as the subjects of a run that recorded no period.
            explicit Impl(std::vector<ProfileSubject> listed);

            SushiProfiler::RunRecord run;         /**< The run SushiProfiler recorded. */
            std::vector<ProfileSubject> subjects; /**< Every subject with samples. */
            std::size_t active = 0U;              /**< Active periods. */
            std::size_t warmup = 0U;              /**< Warm-up periods. */
    };
} // namespace SushiDSP::profile
