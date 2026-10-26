/**************************************************************************/
/* test_node_counters.cpp                                                 */
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
 * @file test_node_counters.cpp
 * @brief Tests the calls that move one block's counts into the process-wide node counters.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/node_counters.hpp>

#include <cstdint>

namespace
{
    /// @brief Declares a circuit whose statistics are fixed, and counts how often they are cleared.
    struct FixedStatistics
    {
            std::uint64_t total_samples() const
            {
                return 1024;
            }

            std::uint64_t non_converged_samples() const
            {
                return 3;
            }

            std::uint64_t nan_fallback_samples() const
            {
                return 2;
            }

            std::uint32_t maximum_iterations_seen() const
            {
                return 6;
            }

            void reset_statistics()
            {
                ++resets;
            }

            int resets = 0;
    };
} // namespace

TEST(Small_NodeCounters, AClampedBlockRaisesTheOutputCounterByItsSamples)
{
    const std::uint64_t before = SushiDSP::node_counters().output_clamped.value();

    SushiDSP::note_output_clamp(12U);
    SushiDSP::note_output_clamp(0U);

    EXPECT_EQ(SushiDSP::node_counters().output_clamped.value(), before + 12U);
}

TEST(Small_NodeCounters, NewtonSolvesRaiseTheCapAndNonFiniteCounters)
{
    const std::uint64_t caps = SushiDSP::node_counters().newton_iteration_cap.value();
    const std::uint64_t non_finite = SushiDSP::node_counters().newton_non_finite.value();

    SushiDSP::note_newton_solves(500U, 4U, 1U, 4U);

    EXPECT_EQ(SushiDSP::node_counters().newton_iteration_cap.value(), caps + 4U);
    EXPECT_EQ(SushiDSP::node_counters().newton_non_finite.value(), non_finite + 1U);
}

TEST(Small_NodeCounters, ADkFlushMovesTheStatisticsIntoTheCountersAndClearsThem)
{
    const std::uint64_t converged = SushiDSP::node_counters().dk_non_converged.value();
    const std::uint64_t fallbacks = SushiDSP::node_counters().dk_nan_fallback.value();
    FixedStatistics circuit;

    SushiDSP::flush_dk_statistics(circuit);

    EXPECT_EQ(SushiDSP::node_counters().dk_non_converged.value(), converged + 3U);
    EXPECT_EQ(SushiDSP::node_counters().dk_nan_fallback.value(), fallbacks + 2U);
    EXPECT_EQ(circuit.resets, 1);
}
