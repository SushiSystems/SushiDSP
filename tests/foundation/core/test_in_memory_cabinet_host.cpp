/**************************************************************************/
/* test_in_memory_cabinet_host.cpp                                        */
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
 * @file test_in_memory_cabinet_host.cpp
 * @brief Tests what the in-memory CabinetHost adds to the port: the installed cabinet and the retired count.
 * @author Mustafa Garip
 */

#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/in_memory_cabinet_host.hpp>
#include <SushiDSP/foundation/io/ir.hpp>

namespace
{
    /// @brief Returns a one-tap impulse response of @p tap at 48 kHz.
    SushiDSP::Ir one_tap(float tap)
    {
        SushiDSP::Ir ir;
        ir.samples = {tap};
        ir.sample_rate = 48000.0;
        return ir;
    }
} // namespace

TEST(Small_InMemoryCabinetHost, HoldsTheLastCabinetItInstalled)
{
    SushiDSP::InMemoryCabinetHost host;
    host.prepare(48000.0);
    static_cast<void>(host.set_cabinet(one_tap(1.0f)));
    static_cast<void>(host.set_cabinet(one_tap(0.5f)));
    ASSERT_NE(host.cabinet(), nullptr);
    EXPECT_EQ(host.cabinet()->samples, std::vector<float>({0.5f}));
}

TEST(Small_InMemoryCabinetHost, HoldsNoCabinetBeforeOneIsInstalled)
{
    SushiDSP::InMemoryCabinetHost host;
    EXPECT_EQ(host.cabinet(), nullptr);
}

TEST(Small_InMemoryCabinetHost, AReplacedCabinetWaitsForAReclaim)
{
    SushiDSP::InMemoryCabinetHost host;
    host.prepare(48000.0);
    static_cast<void>(host.set_cabinet(one_tap(1.0f)));
    static_cast<void>(host.set_cabinet(one_tap(0.5f)));
    EXPECT_EQ(host.retired_count(), 1u);
}

TEST(Small_InMemoryCabinetHost, AReclaimFreesEveryRetiredCabinet)
{
    SushiDSP::InMemoryCabinetHost host;
    host.prepare(48000.0);
    static_cast<void>(host.set_cabinet(one_tap(1.0f)));
    static_cast<void>(host.set_cabinet(one_tap(0.5f)));
    host.reclaim_cabinets();
    EXPECT_EQ(host.retired_count(), 0u);
}

TEST(Small_InMemoryCabinetHost, ARefusalRetiresNothing)
{
    SushiDSP::InMemoryCabinetHost host;
    host.prepare(48000.0);
    static_cast<void>(host.set_cabinet(one_tap(1.0f)));
    static_cast<void>(host.set_cabinet(SushiDSP::Ir{}));
    EXPECT_EQ(host.retired_count(), 0u);
}
