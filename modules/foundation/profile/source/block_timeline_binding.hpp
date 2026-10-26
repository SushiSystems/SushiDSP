/**************************************************************************/
/* block_timeline_binding.hpp                                             */
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
 * @file block_timeline_binding.hpp
 * @brief Declares how a session hands the block timeline to the marking calls.
 * @author Mustafa Garip
 */

#pragma once

namespace SushiProfiler
{
    class PeriodTimeline;
} // namespace SushiProfiler

namespace SushiDSP::profile::Detail
{
    /// @brief Makes mark_block open the periods of @p timeline; null makes it do nothing.
    void bind_block_timeline(SushiProfiler::PeriodTimeline* timeline) noexcept;
} // namespace SushiDSP::profile::Detail
