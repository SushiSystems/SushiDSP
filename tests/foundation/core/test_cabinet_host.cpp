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
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
/**************************************************************************/

/**
 * @file test_cabinet_host.cpp
 * @brief Tests the contract of the cabinet host through a minimal implementation.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/cabinet_host.hpp>
#include <SushiDSP/foundation/io/ir.hpp>

namespace
{
    class SingleSlotCabinetHost final : public SushiDSP::CabinetHost
    {
        public:
            bool set_cabinet(SushiDSP::Ir impulse_response) override
            {
                loaded_ = !impulse_response.samples.empty();
                return loaded_;
            }

            void reclaim_cabinets() override
            {
                loaded_ = false;
            }

            bool has_cabinet() const override
            {
                return loaded_;
            }

        private:
            bool loaded_ = false;
    };
} // namespace

TEST(Small_CabinetHost, RefusesAnEmptyImpulseResponse)
{
    SingleSlotCabinetHost host;
    EXPECT_FALSE(host.set_cabinet(SushiDSP::Ir{}));
    EXPECT_FALSE(host.has_cabinet());
}

TEST(Small_CabinetHost, AcceptsANonEmptyImpulseResponse)
{
    SingleSlotCabinetHost host;
    SushiDSP::Ir ir;
    ir.samples = {1.0f, 0.5f};
    ir.sample_rate = 48000.0;
    EXPECT_TRUE(host.set_cabinet(ir));
    EXPECT_TRUE(host.has_cabinet());
}

TEST(Small_CabinetHost, ReclaimLeavesNoCabinetLoaded)
{
    SingleSlotCabinetHost host;
    SushiDSP::Ir ir;
    ir.samples = {1.0f};
    host.set_cabinet(ir);
    host.reclaim_cabinets();
    EXPECT_FALSE(host.has_cabinet());
}
