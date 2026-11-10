/**************************************************************************/
/* cabinet_log.hpp                                                        */
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
 * @file cabinet_log.hpp
 * @brief Declares the port through which CabinetNode reports a refused impulse response.
 * @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace SushiDSP
{
    /// @brief The reason CabinetNode::set_cabinet() refused an impulse response.
    enum class CabinetRefusalCause : std::uint8_t
    {
        EmptyImpulseResponse, /**< The impulse response held no samples. */
        NotPrepared,          /**< The node had not been prepared. */
        SampleRateMismatch,   /**< The impulse response was recorded at another rate. */
    };

    /// @brief What CabinetNode reports about one refused impulse response.
    struct CabinetRefusal
    {
        CabinetRefusalCause cause;      /**< Why it was refused. */
        std::size_t samples;            /**< The length of the impulse response. */
        double impulse_response_rate;   /**< The rate the impulse response carries. */
        double node_rate;               /**< The rate the node runs at. */
    };

    /// @brief Receives each refusal CabinetNode makes. The product's adapter is
    /// SushiLogCabinetLog; InMemoryCabinetLog keeps the refusals for a test.
    class CabinetLog
    {
        public:
            CabinetLog() = default;
            CabinetLog(const CabinetLog&) = delete;
            CabinetLog& operator=(const CabinetLog&) = delete;
            CabinetLog(CabinetLog&&) = delete;
            CabinetLog& operator=(CabinetLog&&) = delete;
            virtual ~CabinetLog() = default;

            /// @brief Takes @p refusal, made on the control thread.
            virtual void report_refusal(const CabinetRefusal& refusal) = 0;
    };
} // namespace SushiDSP
