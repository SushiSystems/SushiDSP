/**************************************************************************/
/* test_in_memory_cabinet_log.cpp                                         */
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
 * @file test_in_memory_cabinet_log.cpp
 * @brief Tests the in-memory CabinetLog.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/cabinet/in_memory_cabinet_log.hpp>

TEST(Small_InMemoryCabinetLog, KeepsEachRefusalInTheOrderItArrived)
{
    SushiDSP::InMemoryCabinetLog log;

    log.report_refusal({SushiDSP::CabinetRefusalCause::NotPrepared, 3, 48000.0, 48000.0});
    log.report_refusal({SushiDSP::CabinetRefusalCause::SampleRateMismatch, 5, 44100.0, 48000.0});

    ASSERT_EQ(log.refusals().size(), 2u);
    EXPECT_EQ(log.refusals()[0].cause, SushiDSP::CabinetRefusalCause::NotPrepared);
    EXPECT_EQ(log.refusals()[1].cause, SushiDSP::CabinetRefusalCause::SampleRateMismatch);
}
