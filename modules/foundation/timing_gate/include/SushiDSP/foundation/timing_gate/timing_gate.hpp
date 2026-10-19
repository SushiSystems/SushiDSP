/**************************************************************************/
/* timing_gate.hpp                                                        */
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

/** @file timing_gate.hpp
 * @brief Skips a wall-clock bound when the binary under test is not optimised or is sanitised.
 * @author Mustafa Garip
 */

#pragma once

#include <gtest/gtest.h>

/// @brief Skips the calling test unless the binary is optimised and free of sanitiser
/// instrumentation.
/// A budget is derived from what the product does on a listener's machine, which is a Release
/// build; asserting it against an -O0 binary compares two different programs. `sd build`
/// configures only an unconfigured tree, so a tree that was ever Debug stays Debug and the
/// suite runs unoptimised unless it is reconfigured with `sd build --clean --type release`. A
/// sanitiser build is optimised but runs several times slower than the product, so it is skipped
/// the same way.
#define SUSHIDSP_REQUIRE_OPTIMISED_BUILD()                                                         \
    do                                                                                             \
    {                                                                                              \
        /* NDEBUG is the only portable signal the tree gives that optimisation is on. */           \
        if (!SushiDSP::test::is_optimised_build())                                                 \
        {                                                                                          \
            GTEST_SKIP() << "wall-clock bounds are Release figures; this build is unoptimised";    \
        }                                                                                          \
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

    /// @brief Whether the tests were compiled with AddressSanitizer or ThreadSanitizer.
    inline bool is_sanitised_build() noexcept
    {
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
        return true;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
        return true;
#else
        return false;
#endif
#else
        return false;
#endif
    }
} // namespace SushiDSP::test
