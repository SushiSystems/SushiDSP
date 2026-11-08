/**************************************************************************/
/* in_memory_cabinet_host.hpp                                             */
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
 * @file in_memory_cabinet_host.hpp
 * @brief Declares the in-memory CabinetHost, which holds the installed cabinet and the ones it retired.
 * @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <optional>
#include <utility>

#include <SushiDSP/foundation/core/cabinet_host.hpp>
#include <SushiDSP/foundation/io/ir.hpp>

namespace SushiDSP
{
    /// @brief A CabinetHost held in memory, with no audio thread.
    ///
    /// It keeps every promise of CabinetHost: set_cabinet() refuses an empty impulse response, any
    /// call before prepare() and a rate other than the prepared one, and leaves the active cabinet in
    /// place when it refuses. A replaced cabinet waits for reclaim_cabinets(), as it would for an
    /// audio thread to move past it.
    class InMemoryCabinetHost final : public CabinetHost
    {
        public:
            /// @brief Prepares the host at @p sample_rate, the rate every cabinet must carry.
            void prepare(double sample_rate)
            {
                sample_rate_ = sample_rate;
            }

            /// @brief Installs @p impulse_response, or names why it refused it.
            [[nodiscard]] CabinetStatus set_cabinet(Ir impulse_response) override
            {
                if (impulse_response.samples.empty())
                {
                    return CabinetStatus::EmptyImpulseResponse;
                }
                if (!sample_rate_.has_value())
                {
                    return CabinetStatus::NotPrepared;
                }
                if (impulse_response.sample_rate != *sample_rate_)
                {
                    return CabinetStatus::SampleRateMismatch;
                }
                if (cabinet_.has_value())
                {
                    ++retired_;
                }
                cabinet_ = std::move(impulse_response);
                return CabinetStatus::Installed;
            }

            /// @brief Frees every retired cabinet; the installed one stays.
            void reclaim_cabinets() override
            {
                retired_ = 0;
            }

            /// @brief Returns whether a cabinet is installed.
            bool has_cabinet() const override
            {
                return cabinet_.has_value();
            }

            /// @brief Returns the installed cabinet, or null when none is.
            const Ir* cabinet() const
            {
                return cabinet_.has_value() ? &*cabinet_ : nullptr;
            }

            /// @brief Returns how many replaced cabinets wait for reclaim_cabinets().
            std::size_t retired_count() const
            {
                return retired_;
            }

        private:
            std::optional<double> sample_rate_;
            std::optional<Ir> cabinet_;
            std::size_t retired_ = 0;
    };
} // namespace SushiDSP
