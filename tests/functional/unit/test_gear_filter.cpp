/****************************************************************/
/* test_gear_filter.cpp                                         */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#include <gtest/gtest.h>

#include "ui/GearFilter.hpp"

namespace
{
    /// @brief The catalog every test filters, mirroring the real registry's categories and ids.
    /// @return One item per registered descriptor, splitter and mixer included.
    std::vector<ui::GearItem> sample_catalog()
    {
        return {
            { "sushidsp.tubescreamer", "Tube Screamer", "Distortion" },
            { "sushidsp.jcm800", "JCM800", "Amp" },
            { "sushidsp.peavey5150", "Peavey 5150", "Amp" },
            { "sushidsp.mesa_mark2c", "Mesa Mark IIC+", "Amp" },
            { "sushidsp.diezel_vh4", "Diezel VH4", "Amp" },
            { "sushidsp.dd3", "DD-3 Delay", "Delay" },
            { "sushidsp.splitter", "Splitter", "Routing" },
            { "sushidsp.mixer", "Mixer", "Routing" },
            { "sushidsp.pan", "Pan", "Routing" },
            { "sushidsp.room", "Room", "Room" },
            { "sushidsp.cabinet", "4x12 Cabinet", "Cabinet" },
        };
    }

    /// @brief Counts every item across every group a filter_gear call returned.
    /// @param groups The grouped result to count.
    /// @return The total item count.
    std::size_t total_items(const std::vector<ui::GearGroup>& groups)
    {
        std::size_t count = 0;
        for (const ui::GearGroup& group : groups)
        {
            count += group.items.size();
        }
        return count;
    }
}

TEST(Unit_Dsp, GearFilterAllTabKeepsEveryNonRoutingItem)
{
    const std::vector<ui::GearItem> catalog = sample_catalog();
    const std::vector<ui::GearGroup> groups = ui::filter_gear(catalog, ui::GearTab::All, "");
    EXPECT_EQ(total_items(groups), catalog.size() - 3u);
}

TEST(Unit_Dsp, GearFilterAmpsTabKeepsOnlyAmps)
{
    const std::vector<ui::GearItem> catalog = sample_catalog();
    const std::vector<ui::GearGroup> groups = ui::filter_gear(catalog, ui::GearTab::Amps, "");
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].category, "Amp");
    EXPECT_EQ(groups[0].items.size(), 4u);
}

TEST(Unit_Dsp, GearFilterSearchIgnoresCase)
{
    const std::vector<ui::GearItem> catalog = sample_catalog();
    const std::vector<ui::GearGroup> groups =
        ui::filter_gear(catalog, ui::GearTab::All, "jcm800");
    ASSERT_EQ(total_items(groups), 1u);
    EXPECT_EQ(groups[0].items[0]->id, "sushidsp.jcm800");
}

TEST(Unit_Dsp, GearFilterGroupsKeepFirstSeenCategoryOrder)
{
    const std::vector<ui::GearItem> catalog = sample_catalog();
    const std::vector<ui::GearGroup> groups = ui::filter_gear(catalog, ui::GearTab::All, "");
    ASSERT_EQ(groups.size(), 5u);
    EXPECT_EQ(groups[0].category, "Distortion");
    EXPECT_EQ(groups[1].category, "Amp");
    EXPECT_EQ(groups[2].category, "Delay");
    EXPECT_EQ(groups[3].category, "Room");
    EXPECT_EQ(groups[4].category, "Cabinet");
}

TEST(Unit_Dsp, GearFilterNeverListsSplitterOrMixer)
{
    const std::vector<ui::GearItem> catalog = sample_catalog();
    const std::vector<ui::GearGroup> groups = ui::filter_gear(catalog, ui::GearTab::All, "");
    for (const ui::GearGroup& group : groups)
    {
        for (const ui::GearItem* item : group.items)
        {
            EXPECT_NE(item->id, "sushidsp.splitter");
            EXPECT_NE(item->id, "sushidsp.mixer");
        }
    }
}
