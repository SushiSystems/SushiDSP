/****************************************************************/
/* TimingGate.hpp                                               */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/** @file TimingGate.hpp
 * @brief Skips a wall-clock bound when the binary under test is not optimised.
 * @author Mustafa Garip
 */

#ifndef SUSHIDSP_TESTS_TIMING_GATE_HPP
#define SUSHIDSP_TESTS_TIMING_GATE_HPP

#include <gtest/gtest.h>

/// @brief Skips the calling test unless the binary is optimised.
/// A budget is derived from what the product does on a listener's machine, which is a Release
/// build; asserting it against an -O0 binary compares two different programs. `sd build`
/// configures only an unconfigured tree, so a tree that was ever Debug stays Debug and the
/// suite runs unoptimised unless it is reconfigured with `sd build --clean --type release`.
#define SUSHIDSP_REQUIRE_OPTIMISED_BUILD()                                                    \
    do                                                                                        \
    {                                                                                         \
        /* NDEBUG is the only portable signal the tree gives that optimisation is on. */       \
        if (!SushiDSP::test::is_optimised_build())                                             \
        {                                                                                     \
            GTEST_SKIP() << "wall-clock bounds are Release figures; this build is unoptimised";\
        }                                                                                     \
    } while (false)

namespace SushiDSP::test
{
    /// @brief Whether the tests were compiled with optimisation and assertions off.
    inline bool is_optimised_build() noexcept
    {
#ifdef NDEBUG
        return true;
#else
        return false;
#endif
    }
}

#endif
