/**************************************************************************/
/* test_log_thread.cpp                                                    */
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
 * @file test_log_thread.cpp
 * @brief Tests the number each thread's records carry.
 * @author Mustafa Garip
 */

#include <cstdint>
#include <thread>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/log_thread.hpp>

TEST(Small_LogThread, ThreadKeepsItsNumberAcrossCalls)
{
    const std::uint32_t first = SushiDSP::current_log_thread_number();
    EXPECT_GE(first, 1u);
    EXPECT_EQ(SushiDSP::current_log_thread_number(), first);
}

TEST(Medium_LogThread, TwoThreadsHaveTwoNumbers)
{
    const std::uint32_t mine = SushiDSP::current_log_thread_number();
    std::uint32_t theirs = 0;
    std::thread other(
        [&theirs]
        {
            theirs = SushiDSP::current_log_thread_number();
        });
    other.join();

    EXPECT_GE(theirs, 1u);
    EXPECT_NE(theirs, mine);
}
