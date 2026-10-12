/**************************************************************************/
/* smoothed_parameter.hpp                                                 */
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
 * @file smoothed_parameter.hpp
 * @brief Declares a one-pole parameter smoother.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>

namespace SushiDSP
{
    /// @brief First-order (one-pole) parameter smoother with a time constant in milliseconds,
    /// derived from the sample rate at prepare() so a knob feels the same at every rate and
    /// block size. set_target() is a single float store and is safe to call from the audio
    /// thread; all advancing happens on the audio thread.
    class SmoothedParameter
    {
        public:
            /// @param initial_value Value both current and target start at.
            explicit SmoothedParameter(float initial_value = 0.0f);

            /// @brief Derives the per-sample coefficient. Control thread only.
            /// @param sample_rate The rate next() will be called at (base or oversampled).
            /// @param time_constant_ms Time to reach 63 % of a step.
            void prepare(double sample_rate, float time_constant_ms);

            /// @brief Sets the value the smoother moves toward.
            /// @param target The new target.
            void set_target(float target);

            /// @brief Advances one sample.
            /// @return The smoothed value after this sample.
            float next();

            /// @brief Advances `samples` samples in one step (closed form).
            /// @param samples Number of samples to advance.
            /// @return The smoothed value after those samples.
            float skip(std::uint32_t samples);

            /// @brief The current smoothed value.
            float current() const
            {
                return current_;
            }

            /// @brief The target value.
            float target() const
            {
                return target_;
            }

            /// @brief Whether |current - target| is within tolerance.
            /// @param tolerance Absolute tolerance.
            /// @return True when settled.
            bool is_settled(float tolerance = 1e-6f) const;

            /// @brief Sets current and target to value with no ramp.
            /// @param value The value to snap to.
            void snap(float value);

        private:
            float current_;
            float target_;
            float coefficient_ = 0.0f; // fraction of the remaining distance kept per sample
    };
} // namespace SushiDSP
