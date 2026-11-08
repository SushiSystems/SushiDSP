/**************************************************************************/
/* test_cabinet_host.cpp                                                  */
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
 * @file test_cabinet_host.cpp
 * @brief Tests the promises every CabinetHost keeps, once per implementation, and the status texts.
 * @author Mustafa Garip
 */

#include <memory>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/cabinet_host.hpp>
#include <SushiDSP/foundation/core/in_memory_cabinet_host.hpp>
#include <SushiDSP/foundation/io/ir.hpp>

namespace
{
    /// @brief Makes the in-memory host for the contract.
    struct InMemoryCabinetHostSubject
    {
            /// @brief Makes a host prepared at @p sample_rate.
            static std::unique_ptr<SushiDSP::CabinetHost> make_prepared(double sample_rate)
            {
                auto host = std::make_unique<SushiDSP::InMemoryCabinetHost>();
                host->prepare(sample_rate);
                return host;
            }

            /// @brief Makes a host that was never prepared.
            static std::unique_ptr<SushiDSP::CabinetHost> make_unprepared()
            {
                return std::make_unique<SushiDSP::InMemoryCabinetHost>();
            }
    };

    /// @brief Returns a two-tap impulse response at @p sample_rate.
    SushiDSP::Ir two_taps_at(double sample_rate)
    {
        SushiDSP::Ir ir;
        ir.samples = {1.0f, 0.5f};
        ir.sample_rate = sample_rate;
        return ir;
    }

    template <typename Subject> class Small_CabinetHostContract : public ::testing::Test
    {
    };

    using CabinetHostSubjects = ::testing::Types<InMemoryCabinetHostSubject>;
} // namespace

TYPED_TEST_SUITE(Small_CabinetHostContract, CabinetHostSubjects);

TYPED_TEST(Small_CabinetHostContract, InstallsAnImpulseResponseAtThePreparedRate)
{
    auto host = TypeParam::make_prepared(48000.0);
    EXPECT_EQ(host->set_cabinet(two_taps_at(48000.0)), SushiDSP::CabinetStatus::Installed);
}

TYPED_TEST(Small_CabinetHostContract, HasACabinetOnceOneIsInstalled)
{
    auto host = TypeParam::make_prepared(48000.0);
    static_cast<void>(host->set_cabinet(two_taps_at(48000.0)));
    EXPECT_TRUE(host->has_cabinet());
}

TYPED_TEST(Small_CabinetHostContract, RefusesAnEmptyImpulseResponse)
{
    auto host = TypeParam::make_prepared(48000.0);
    EXPECT_EQ(host->set_cabinet(SushiDSP::Ir{{}, 48000.0}), SushiDSP::CabinetStatus::EmptyImpulseResponse);
}

TYPED_TEST(Small_CabinetHostContract, RefusesACabinetBeforePrepare)
{
    auto host = TypeParam::make_unprepared();
    EXPECT_EQ(host->set_cabinet(two_taps_at(48000.0)), SushiDSP::CabinetStatus::NotPrepared);
}

TYPED_TEST(Small_CabinetHostContract, RefusesARateOtherThanThePreparedOne)
{
    auto host = TypeParam::make_prepared(48000.0);
    EXPECT_EQ(host->set_cabinet(two_taps_at(44100.0)), SushiDSP::CabinetStatus::SampleRateMismatch);
}

TYPED_TEST(Small_CabinetHostContract, KeepsTheActiveCabinetWhenItRefusesOne)
{
    auto host = TypeParam::make_prepared(48000.0);
    static_cast<void>(host->set_cabinet(two_taps_at(48000.0)));
    static_cast<void>(host->set_cabinet(two_taps_at(44100.0)));
    EXPECT_TRUE(host->has_cabinet());
}

TYPED_TEST(Small_CabinetHostContract, KeepsTheInstalledCabinetThroughAReclaim)
{
    auto host = TypeParam::make_prepared(48000.0);
    static_cast<void>(host->set_cabinet(two_taps_at(48000.0)));
    host->reclaim_cabinets();
    EXPECT_TRUE(host->has_cabinet());
}

TEST(Small_CabinetHost, EachStatusHasItsOwnText)
{
    EXPECT_STREQ(SushiDSP::cabinet_status_text(SushiDSP::CabinetStatus::Installed), "installed");
    EXPECT_STRNE(SushiDSP::cabinet_status_text(SushiDSP::CabinetStatus::EmptyImpulseResponse),
                 SushiDSP::cabinet_status_text(SushiDSP::CabinetStatus::NotPrepared));
    EXPECT_STRNE(SushiDSP::cabinet_status_text(SushiDSP::CabinetStatus::NotPrepared),
                 SushiDSP::cabinet_status_text(SushiDSP::CabinetStatus::SampleRateMismatch));
}
