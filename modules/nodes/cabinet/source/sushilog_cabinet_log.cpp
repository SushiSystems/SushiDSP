/**************************************************************************/
/* sushilog_cabinet_log.cpp                                               */
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
 * @file sushilog_cabinet_log.cpp
 * @brief Implements the CabinetLog that writes each refusal as a SushiLog record.
 * @author Mustafa Garip
 */

#include <SushiDSP/nodes/cabinet/sushilog_cabinet_log.hpp>

#include <SushiDSP/foundation/log/log.hpp>
#include <SushiDSP/nodes/cabinet/cabinet_log_category.hpp>

namespace SushiDSP
{
    void SushiLogCabinetLog::report_refusal(const CabinetRefusal& refusal)
    {
        switch (refusal.cause)
        {
            case CabinetRefusalCause::EmptyImpulseResponse:
                SD_LOG_WARN(K_CABINET_LOG_CATEGORY,
                            "set_cabinet() refused an impulse response of no samples; the active "
                            "cabinet stays");
                break;
            case CabinetRefusalCause::NotPrepared:
                SD_LOG_WARN(K_CABINET_LOG_CATEGORY,
                            "set_cabinet() refused an impulse response of {} samples because the "
                            "node is not prepared; the active cabinet stays",
                            refusal.samples);
                break;
            case CabinetRefusalCause::SampleRateMismatch:
                SD_LOG_WARN(K_CABINET_LOG_CATEGORY,
                            "set_cabinet() refused an impulse response at {} Hz for a node at {} "
                            "Hz; the active cabinet stays",
                            refusal.impulse_response_rate, refusal.node_rate);
                break;
        }
    }
} // namespace SushiDSP
