/**************************************************************************/
/* in_memory_capture_log.hpp                                              */
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
 * @file in_memory_capture_log.hpp
 * @brief Declares the in-memory CaptureLog, which keeps every failure it receives.
 * @author Mustafa Garip
 */

#pragma once

#include <mutex>
#include <vector>

#include <SushiDSP/nodes/capture/capture_log.hpp>

namespace SushiDSP
{
    /// @brief A CaptureLog that keeps every failure in the order it arrived, writing nothing. A
    /// mutex guards the list, because the control thread and the writer thread both report.
    class InMemoryCaptureLog final : public CaptureLog
    {
        public:
            /// @brief Keeps @p failure.
            void report_failure(const CaptureFailure& failure) override
            {
                const std::lock_guard<std::mutex> lock(mutex_);
                failures_.push_back(failure);
            }

            /// @brief Returns a copy of the failures received so far, in order.
            [[nodiscard]] std::vector<CaptureFailure> failures() const
            {
                const std::lock_guard<std::mutex> lock(mutex_);
                return failures_;
            }

        private:
            /// @brief Guards failures_.
            mutable std::mutex mutex_;

            /// @brief The failures received, in order.
            std::vector<CaptureFailure> failures_;
    };
} // namespace SushiDSP
