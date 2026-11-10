/**************************************************************************/
/* clip_stage_reference.hpp                                               */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file clip_stage_reference.hpp
 * @brief A host solve of the Tube Screamer clip stage's equations, written without the DK solver.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace TestSupport
{
    /// @brief The component values of docs/reference/spice/ts_clip/README.md and the 1N914 law.
    ///
    /// They are typed here again, not read from the production header, so a value that drifts in
    /// the block is a difference between the block and this solve.
    struct ClipStageValues
    {
            double input_farads = 1.0e-6;
            double input_leak_ohms = 510.0e3;
            double feedback_fixed_ohms = 51.0e3;
            double drive_pot_ohms = 500.0e3;
            double feedback_farads = 51.0e-12 + 2.0 * 4.0e-12;
            double ground_leg_ohms = 4700.0;
            double ground_leg_farads = 47.0e-9;
            double saturation_amps = 2.52e-9;
            double ideality = 1.752;
            double thermal_volts = 0.025864;
    };

    /// @brief Which frames of the solve to return, and how finely to step to reach them.
    struct ClipStageRun
    {
            double output_rate = 1.0;       ///< Rate, in hertz, of the output frames.
            std::size_t substeps = 1;       ///< Solver steps per output frame.
            std::size_t first_frame = 1;    ///< Index of the first frame returned; frame 0 is never returned.
            std::size_t frames = 1;         ///< Number of frames returned.
            double drive = 1.0;             ///< The drive pot's position, 0 to 1: Rf is 51 kOhm + drive * 500 kOhm.
    };

    /// @brief Solves the ideal-op-amp clip stage for one stimulus, from zero state.
    /// The states are the input capacitor, the ground leg capacitor and the feedback network, the
    /// nullator tying the inverting node to the input node (tests/README.md has the equations).
    /// @tparam Stimulus A callable taking seconds and returning the source voltage; zero at time zero.
    /// @param stimulus The source voltage over time.
    /// @param run The frames to return and the step.
    /// @param values The component values.
    /// @return The output voltage at the frames of run.
    template <typename Stimulus>
    std::vector<double> solve_clip_stage(Stimulus stimulus, const ClipStageRun& run,
                                         const ClipStageValues& values = ClipStageValues{})
    {
        const double step = 1.0 / (run.output_rate * static_cast<double>(run.substeps));
        const double scaled_thermal = values.ideality * values.thermal_volts;
        const double feedback_ohms = values.feedback_fixed_ohms + run.drive * values.drive_pot_ohms;
        const double input_time = values.input_leak_ohms * values.input_farads;
        const double ground_time = values.ground_leg_ohms * values.ground_leg_farads;
        constexpr double K_MAX_NEWTON_STEP_VOLTS = 0.1;
        constexpr int K_MAX_NEWTON_ITERATIONS = 200;
        constexpr double K_NEWTON_STOP_VOLTS = 1.0e-14;

        std::vector<double> output;
        output.reserve(run.frames);

        double h = 0.0;
        double g = 0.0;
        double v = 0.0;
        double previous_h = 0.0;
        double previous_g = 0.0;
        double previous_v = 0.0;
        const std::size_t last_step = (run.first_frame + run.frames - 1u) * run.substeps;
        for (std::size_t index = 1; index <= last_step; ++index)
        {
            // BDF1 for the first step, BDF2 after: x1 = history + weight * step * f(x1).
            const bool first = index == 1;
            const double now_weight = first ? 1.0 : 4.0 / 3.0;
            const double past_weight = first ? 0.0 : -1.0 / 3.0;
            const double rate_weight = first ? 1.0 : 2.0 / 3.0;

            const double source = stimulus(static_cast<double>(index) * step);

            const double input_gain = rate_weight * step / input_time;
            const double next_h =
                (now_weight * h + past_weight * previous_h + input_gain * source) / (1.0 + input_gain);
            const double u = source - next_h;

            const double ground_gain = rate_weight * step / ground_time;
            const double next_g =
                (now_weight * g + past_weight * previous_g + ground_gain * u) / (1.0 + ground_gain);

            const double leg_amps = (u - next_g) / values.ground_leg_ohms;
            const double history = now_weight * v + past_weight * previous_v;
            const double weight = rate_weight * step / values.feedback_farads;
            double next_v = first ? v : 2.0 * v - previous_v;
            for (int iteration = 0; iteration < K_MAX_NEWTON_ITERATIONS; ++iteration)
            {
                const double growth = std::exp(next_v / scaled_thermal);
                const double shrink = 1.0 / growth;
                const double pair_amps = values.saturation_amps * (growth - shrink);
                const double pair_siemens = values.saturation_amps * (growth + shrink) / scaled_thermal;
                const double residual = next_v - history - weight * (leg_amps - next_v / feedback_ohms - pair_amps);
                const double slope = 1.0 + weight * (1.0 / feedback_ohms + pair_siemens);
                const double correction =
                    std::clamp(residual / slope, -K_MAX_NEWTON_STEP_VOLTS, K_MAX_NEWTON_STEP_VOLTS);
                next_v -= correction;
                if (std::fabs(correction) < K_NEWTON_STOP_VOLTS)
                {
                    break;
                }
            }

            previous_h = h;
            previous_g = g;
            previous_v = v;
            h = next_h;
            g = next_g;
            v = next_v;

            if (index % run.substeps == 0 && index / run.substeps >= run.first_frame)
            {
                output.push_back(u + v);
            }
        }
        return output;
    }
}
