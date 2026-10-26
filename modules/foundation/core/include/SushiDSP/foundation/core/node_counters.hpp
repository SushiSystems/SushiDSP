/**************************************************************************/
/* node_counters.hpp                                                      */
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
 * @file node_counters.hpp
 * @brief Declares the process-wide counters the nodes' block paths raise and the calls that fill them.
 * @author Mustafa Garip
 * @see modules/foundation/core/README.md
 */

#pragma once

#include <SushiDSP/foundation/core/rt_counter.hpp>
#include <SushiDSP/foundation/core/rt_counter_report.hpp>
#include <SushiDSP/foundation/profile/profile.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

#include <cstdint>

namespace SushiDSP
{
    /**
     * @brief Holds what the nodes of every rig count at the end of a block.
     *
     * The counters are shared by every node of the process, because a node a host replaces must
     * not take its count with it. Each is one relaxed atomic add per block that moved.
     */
    struct NodeCounters
    {
            /// @brief Samples an amplifier output clamp cut to the rail or found not a number.
            RtCounter output_clamped;

            /// @brief DK circuit solves that stopped at their iteration cap.
            RtCounter dk_non_converged;

            /// @brief DK circuit samples replaced from the backup state because they were not
            /// finite.
            RtCounter dk_nan_fallback;

            /// @brief Newton solves of the tube stages that stopped at their iteration cap.
            RtCounter newton_iteration_cap;

            /// @brief Newton solves of the tube stages that met a non-finite step.
            RtCounter newton_non_finite;

            /// @brief Adds every counter to @p reporter under its name.
            void add_to(RtCounterReporter& reporter) const
            {
                reporter.add("output_clamped", output_clamped);
                reporter.add("dk_non_converged", dk_non_converged);
                reporter.add("dk_nan_fallback", dk_nan_fallback);
                reporter.add("newton_iteration_cap", newton_iteration_cap);
                reporter.add("newton_non_finite", newton_non_finite);
            }
    };

    /// @brief Returns the counters every node raises. Audio thread and control thread.
    [[nodiscard]] inline NodeCounters& node_counters() noexcept
    {
        static NodeCounters counters;
        return counters;
    }

    /**
     * @brief Counts the samples one block's output clamp cut, and records them as a counter.
     *
     * The record is made every block, as 0 when nothing was cut, so a `counter:` check always has
     * samples.
     */
    inline void note_output_clamp(std::uint32_t samples) noexcept
    {
        if (samples != 0U)
            node_counters().output_clamped.increment(samples);
        SD_COUNTER(profile::names::K_CLAMPED_COUNTER, profile::names::K_COUNT_UNIT, Add,
                   static_cast<double>(samples));
    }

    /**
     * @brief Counts one block's Newton solves of the tube stages, and records them as counters.
     * @param solves The solves run.
     * @param iteration_caps The solves that stopped at their iteration cap.
     * @param non_finite_steps The solves that met a non-finite step.
     * @param worst_iterations The most iterations any one solve took.
     */
    inline void note_newton_solves(std::uint64_t solves, std::uint64_t iteration_caps,
                                   std::uint64_t non_finite_steps,
                                   std::uint32_t worst_iterations) noexcept
    {
        NodeCounters& counters = node_counters();
        if (iteration_caps != 0U)
            counters.newton_iteration_cap.increment(iteration_caps);
        if (non_finite_steps != 0U)
            counters.newton_non_finite.increment(non_finite_steps);
        SD_COUNTER(profile::names::K_NEWTON_SOLVES_COUNTER, profile::names::K_COUNT_UNIT, Add,
                   static_cast<double>(solves));
        SD_COUNTER(profile::names::K_NEWTON_CAPS_COUNTER, profile::names::K_COUNT_UNIT, Add,
                   static_cast<double>(iteration_caps));
        SD_COUNTER(profile::names::K_NEWTON_NON_FINITE_COUNTER, profile::names::K_COUNT_UNIT, Add,
                   static_cast<double>(non_finite_steps));
        SD_COUNTER(profile::names::K_NEWTON_WORST_ITERATIONS_COUNTER, profile::names::K_COUNT_UNIT,
                   Max, static_cast<double>(worst_iterations));
    }

    /**
     * @brief Moves a DK circuit's per-block statistics into the counters and the record, then
     * clears them.
     * @tparam Circuit A type with the statistics of SushiDSP::dk::DkCircuit.
     *
     * The statistics are plain integers the audio thread writes, so the flush runs on that thread,
     * at the end of a block.
     */
    template <typename Circuit> void flush_dk_statistics(Circuit& circuit) noexcept
    {
        const std::uint64_t solves = circuit.total_samples();
        const std::uint64_t non_converged = circuit.non_converged_samples();
        const std::uint64_t nan_fallback = circuit.nan_fallback_samples();
        const std::uint32_t worst = circuit.maximum_iterations_seen();
        NodeCounters& counters = node_counters();
        if (non_converged != 0U)
            counters.dk_non_converged.increment(non_converged);
        if (nan_fallback != 0U)
            counters.dk_nan_fallback.increment(nan_fallback);
        SD_COUNTER(profile::names::K_SOLVES_COUNTER, profile::names::K_COUNT_UNIT, Add,
                   static_cast<double>(solves));
        SD_COUNTER(profile::names::K_NON_CONVERGED_COUNTER, profile::names::K_COUNT_UNIT, Add,
                   static_cast<double>(non_converged));
        SD_COUNTER(profile::names::K_NAN_FALLBACK_COUNTER, profile::names::K_COUNT_UNIT, Add,
                   static_cast<double>(nan_fallback));
        SD_COUNTER(profile::names::K_WORST_ITERATIONS_COUNTER, profile::names::K_COUNT_UNIT, Max,
                   static_cast<double>(worst));
        circuit.reset_statistics();
    }
} // namespace SushiDSP
