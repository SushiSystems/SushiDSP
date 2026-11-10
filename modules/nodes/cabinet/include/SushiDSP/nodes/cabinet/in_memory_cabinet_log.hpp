/**************************************************************************/
/* in_memory_cabinet_log.hpp                                              */
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
 * @file in_memory_cabinet_log.hpp
 * @brief Declares the in-memory CabinetLog, which keeps every refusal it receives.
 * @author Mustafa Garip
 */

#pragma once

#include <vector>

#include <SushiDSP/nodes/cabinet/cabinet_log.hpp>

namespace SushiDSP
{
    /// @brief A CabinetLog that keeps every refusal in the order it arrived, writing nothing.
    class InMemoryCabinetLog final : public CabinetLog
    {
        public:
            /// @brief Keeps @p refusal.
            void report_refusal(const CabinetRefusal& refusal) override
            {
                refusals_.push_back(refusal);
            }

            /// @brief Returns the refusals received so far, in order.
            [[nodiscard]] const std::vector<CabinetRefusal>& refusals() const
            {
                return refusals_;
            }

        private:
            /// @brief The refusals received, in order.
            std::vector<CabinetRefusal> refusals_;
    };
} // namespace SushiDSP
