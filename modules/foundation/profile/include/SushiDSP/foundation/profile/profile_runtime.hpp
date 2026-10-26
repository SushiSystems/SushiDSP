/**************************************************************************/
/* profile_runtime.hpp                                                    */
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
 * @file profile_runtime.hpp
 * @brief Declares the calls a product makes around the recording macros: names, threads, blocks.
 * @author Mustafa Garip
 * @see modules/foundation/profile/README.md
 */

#pragma once

#include <SushiDSP/foundation/profile/profile.hpp>

#include <cstdint>
#include <string_view>

namespace SushiDSP::profile
{
    /// @brief Names one interned string, as SushiProfiler does; 0 is none.
    using NameID = std::uint32_t;

    /// @brief Holds whether this build records at all; false when SD_ENABLE_PROFILER is 0.
    inline constexpr bool K_PROFILER_BUILT = SD_ENABLE_PROFILER != 0;

    /**
     * @brief Returns the id of @p text for a named zone or a lane, interning it on first use.
     *
     * Takes a lock and may allocate, so it runs at prepare() or compile(), never on the audio
     * thread. Returns 0 when the build records nothing.
     */
    [[nodiscard]] NameID intern_name(std::string_view text);

    /**
     * @brief Gives the calling thread its ring, once, so its first record allocates nothing.
     *
     * The call that registers allocates; every later call on the thread costs one thread-local
     * read. A device calls it at the top of its callback, so the allocation falls in the first
     * block and never inside a zone.
     */
    void register_this_thread();

    /**
     * @brief Opens the next period of the block timeline, with the budget of one block.
     * @param frames The frames of the block that has just arrived.
     * @param sample_rate The rate the block is played at, in hertz.
     *
     * Does nothing while no session runs. The budget is `frames / sample_rate`, so a host whose
     * block size changes states each block's own budget.
     */
    void mark_block(std::uint32_t frames, double sample_rate) noexcept;
} // namespace SushiDSP::profile
