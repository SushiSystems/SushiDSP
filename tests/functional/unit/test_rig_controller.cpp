/**************************************************************************/
/* test_rig_controller.cpp                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#include <gtest/gtest.h>

#include "RigController.hpp"

TEST(Unit_Dsp, RigControllerStartsEmpty)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    EXPECT_TRUE(controller.chain_ids().empty());
}

TEST(Unit_Dsp, RigControllerAppendAddsNodeToEndOfChain)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");

    const auto ids = controller.chain_ids();
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], "sushidsp.tubescreamer");
    EXPECT_EQ(ids[1], "sushidsp.jcm800");
    ASSERT_NE(controller.active_graph(), nullptr);
}

TEST(Unit_Dsp, RigControllerMoveReordersChain)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");

    controller.move(0, 1);  // move index 0 to after index 1

    const auto ids = controller.chain_ids();
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], "sushidsp.jcm800");
    EXPECT_EQ(ids[1], "sushidsp.tubescreamer");
}

TEST(Unit_Dsp, RigControllerRemoveDropsFromChain)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");

    controller.remove(0);

    const auto ids = controller.chain_ids();
    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(ids[0], "sushidsp.jcm800");
}

TEST(Unit_Dsp, RigControllerPreservesParameterValuesAcrossReorder)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    controller.append("sushidsp.tubescreamer");
    controller.set_parameter(0, "drive", 0.42f);

    controller.append("sushidsp.jcm800");
    controller.move(0, 1);

    // tube_screamer is now at index 1 after the move.
    EXPECT_FLOAT_EQ(controller.get_parameter(1, "drive"), 0.42f);
}
