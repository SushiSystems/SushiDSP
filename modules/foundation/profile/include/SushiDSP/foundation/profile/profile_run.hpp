/**************************************************************************/
/* profile_run.hpp                                                        */
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
 * @file profile_run.hpp
 * @brief Declares the loop that runs a body once per period of a timeline inside a session.
 * @author Mustafa Garip
 * @see modules/foundation/profile/README.md
 */

#pragma once

#include <SushiDSP/foundation/profile/profile_record.hpp>
#include <SushiDSP/foundation/profile/profile_runtime.hpp>
#include <SushiDSP/foundation/profile/profile_session.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace SushiDSP::profile
{
    /// @brief What run_profiled hands back: how the session started, and the run when it did.
    struct ProfiledRun
    {
            ProfileStart start;                /**< How the session started; the body ran only when
                                                    it did. */
            ProfileRecord record;              /**< The run, empty when the session did not start. */
            std::vector<std::string> problems; /**< What went wrong outside the record. */
    };

    /**
     * @brief Runs @p body once per period of @p timeline inside a session and returns the record.
     * @param timeline The timeline the window follows.
     * @param warmup The periods recorded and marked warm-up, before the @p active ones.
     * @param budget_ns The budget of every period, or 0 for none.
     * @param body Called once per period, on the calling thread, after the period is opened.
     * When the session does not start, @p body does not run and `start` says why.
     */
    template <typename Body>
    [[nodiscard]] ProfiledRun run_profiled(std::string_view timeline, std::uint32_t warmup,
                                           std::uint32_t active, std::uint64_t budget_ns,
                                           Body&& body)
    {
        ProfiledRun run;
        ProfileSession session;
        run.start = session.start_window(timeline, warmup, active);
        if (!run.start.started())
            return run;
        register_this_thread();
        for (std::uint32_t period = 0; period < warmup + active; ++period)
        {
            session.mark(timeline, budget_ns);
            body();
        }
        session.mark(timeline, budget_ns);
        ProfileFinish finished = session.finish();
        run.record = std::move(finished.record);
        run.problems = std::move(finished.problems);
        return run;
    }
} // namespace SushiDSP::profile
