/**************************************************************************/
/* test_counter_poll_thread.cpp                                           */
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
 * @file test_counter_poll_thread.cpp
 * @brief Tests that the poll thread calls its function repeatedly and stops when destroyed.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/counter_poll_thread.hpp>

#include <atomic>
#include <chrono>
#include <thread>

TEST(Medium_CounterPollThread, TheFunctionIsCalledMoreThanOnceWhileTheObjectLives)
{
    std::atomic<int> calls{0};
    {
        SushiDSP::CounterPollThread poller([&] { calls.fetch_add(1); });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (calls.load() < 2 && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_GE(calls.load(), 2);
}

TEST(Medium_CounterPollThread, NoCallHappensAfterTheObjectIsDestroyed)
{
    std::atomic<int> calls{0};
    {
        SushiDSP::CounterPollThread poller([&] { calls.fetch_add(1); });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (calls.load() < 1 && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    const int at_destruction = calls.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(700));

    EXPECT_EQ(calls.load(), at_destruction);
}
