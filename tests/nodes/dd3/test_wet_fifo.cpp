/**************************************************************************/
/* test_wet_fifo.cpp                                                      */
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
 * @file test_wet_fifo.cpp
 * @brief Tests the wet FIFO's priming, its capacity and its starvation.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/dd3/wet_fifo.hpp>

TEST(Small_WetFifo, StartsPrimingAndPopsSilence)
{
    SushiDSP::dd3::WetFifo fifo;
    fifo.reset();
    EXPECT_TRUE(fifo.priming());
    EXPECT_EQ(fifo.pop(), 0.0f);
}

TEST(Small_WetFifo, PopsTheOldestSampleOnceTheFifoIsPrimed)
{
    SushiDSP::dd3::WetFifo fifo;
    fifo.reset();
    for (int i = 1; i <= static_cast<int>(SushiDSP::dd3::WetFifo::K_PRIME_FRAMES); ++i)
    {
        fifo.push(static_cast<float>(i));
    }
    EXPECT_FLOAT_EQ(fifo.pop(), 1.0f);
    EXPECT_EQ(fifo.depth(), SushiDSP::dd3::WetFifo::K_PRIME_FRAMES - 1u);
}

TEST(Small_WetFifo, DepthNeverExceedsItsCapacity)
{
    SushiDSP::dd3::WetFifo fifo;
    fifo.reset();
    for (int i = 0; i < 100; ++i)
    {
        fifo.push(0.5f);
    }
    EXPECT_EQ(fifo.depth(), SushiDSP::dd3::WetFifo::K_CAPACITY);
}

TEST(Small_WetFifo, StarvedFifoPopsSilence)
{
    SushiDSP::dd3::WetFifo fifo;
    fifo.reset();
    for (std::uint32_t i = 0; i < SushiDSP::dd3::WetFifo::K_PRIME_FRAMES; ++i)
    {
        fifo.push(1.0f);
    }
    for (std::uint32_t i = 0; i < SushiDSP::dd3::WetFifo::K_PRIME_FRAMES; ++i)
    {
        fifo.pop();
    }
    EXPECT_EQ(fifo.depth(), 0u);
    EXPECT_EQ(fifo.pop(), 0.0f);
}
