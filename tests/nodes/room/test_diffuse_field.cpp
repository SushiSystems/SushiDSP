/**************************************************************************/
/* test_diffuse_field.cpp                                                 */
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
 * @file test_diffuse_field.cpp
 * @brief Tests that the diffuse field of a room has one non-negative power per band.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/diffuse_field.hpp>
#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>

#include <cstddef>

TEST(Small_DiffuseField, HasOnePowerPerBand)
{
    const SushiDSP::room::DiffuseField field(SushiDSP::test::garage_scene());
    EXPECT_EQ(field.band_power().size(), 6u);
}

TEST(Small_DiffuseField, PowerIsNonNegativeAcrossTheBand)
{
    const SushiDSP::room::DiffuseField field(SushiDSP::test::garage_scene());
    EXPECT_GE(field.power_at(1000.0), 0.0);
    EXPECT_GE(field.power_between(125.0, 4000.0), 0.0);
}

TEST(Small_DiffuseField, WiderBandCarriesAtLeastAsMuchPower)
{
    const SushiDSP::room::DiffuseField field(SushiDSP::test::garage_scene());
    EXPECT_GE(field.power_between(125.0, 8000.0), field.power_between(125.0, 1000.0));
}
