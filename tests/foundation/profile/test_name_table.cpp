/**************************************************************************/
/* test_name_table.cpp                                                    */
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
 * @file test_name_table.cpp
 * @brief Tests the name table port through its in-memory adapter and the collector's.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/profile/name_table.hpp>

#include "foundation/profile/recording_build_test.hpp"

namespace
{
    using SushiDSP::profile::InMemoryNameTable;
    using SushiDSP::profile::NameTable;

    /// @brief Holds a text the name tests intern.
    constexpr const char* K_TEXT = "test.interned";

    /// @brief Holds a second text the name tests intern.
    constexpr const char* K_OTHER_TEXT = "test.interned.other";

    /// @brief Expects @p table to give one text the same non-zero id each time.
    void expect_the_same_text_gets_the_same_id(NameTable& table)
    {
        const SushiDSP::profile::NameID first = table.intern(K_TEXT);
        const SushiDSP::profile::NameID again = table.intern(K_TEXT);

        EXPECT_NE(first, 0u);
        EXPECT_EQ(first, again);
    }

    /// @brief Expects @p table to give two different texts two different ids.
    void expect_different_texts_get_different_ids(NameTable& table)
    {
        EXPECT_NE(table.intern(K_TEXT), table.intern(K_OTHER_TEXT));
    }

    /// @brief Expects @p table to give the empty text the id 0, which means none.
    void expect_the_empty_text_has_no_id(NameTable& table)
    {
        EXPECT_EQ(table.intern(""), 0u);
    }

    class Medium_CollectorNameTable : public SushiDSP::profile::test::RecordingBuildTest
    {
    };
} // namespace

TEST(Small_InMemoryNameTable, TheSameTextGetsTheSameIdEachTime)
{
    InMemoryNameTable table;

    expect_the_same_text_gets_the_same_id(table);
}

TEST(Small_InMemoryNameTable, DifferentTextsGetDifferentIds)
{
    InMemoryNameTable table;

    expect_different_texts_get_different_ids(table);
}

TEST(Small_InMemoryNameTable, TheEmptyTextHasNoId)
{
    InMemoryNameTable table;

    expect_the_empty_text_has_no_id(table);
}

TEST_F(Medium_CollectorNameTable, TheSameTextGetsTheSameIdEachTime)
{
    expect_the_same_text_gets_the_same_id(SushiDSP::profile::collector_name_table());
}

TEST_F(Medium_CollectorNameTable, DifferentTextsGetDifferentIds)
{
    expect_different_texts_get_different_ids(SushiDSP::profile::collector_name_table());
}

TEST_F(Medium_CollectorNameTable, TheEmptyTextHasNoId)
{
    expect_the_empty_text_has_no_id(SushiDSP::profile::collector_name_table());
}

TEST_F(Medium_CollectorNameTable, InternNameGivesTheIdTheCollectorTableGives)
{
    const SushiDSP::profile::NameID interned = SushiDSP::profile::intern_name(K_TEXT);

    EXPECT_NE(interned, 0u);
    EXPECT_EQ(interned, SushiDSP::profile::collector_name_table().intern(K_TEXT));
}
