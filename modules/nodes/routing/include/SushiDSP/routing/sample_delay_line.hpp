/**************************************************************************/
/* sample_delay_line.hpp                                                  */
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
 * @file sample_delay_line.hpp
 * @brief Declares the whole-sample delay line a channel strip uses for its delay control.
 * @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <vector>

namespace SushiDSP
{
    /// @brief Delays one channel by a whole number of samples, from zero up to a fixed maximum.
    class SampleDelayLine
    {
        public:
            /// @brief Allocates the line for up to max_samples of delay. Not real-time safe.
            /// @param max_samples The longest delay set_delay() may select.
            void prepare(std::size_t max_samples);

            /// @brief Selects the delay, clamped to the prepared maximum. Real-time safe.
            /// @param samples The delay in samples; 0 passes the input through unchanged.
            void set_delay(std::size_t samples) noexcept;

            /// @brief Pushes one sample and returns the sample delay() samples earlier. Real-time safe.
            /// @param sample The input sample.
            /// @return The delayed sample.
            float process(float sample) noexcept;

            /// @brief Clears the stored samples. Control thread only, never concurrent with process().
            void reset() noexcept;

            /// @brief Reports the longest delay the line was prepared for.
            /// @return The maximum delay in samples.
            std::size_t max_delay() const noexcept;

        private:
            /// @brief Holds max_delay + 1 samples, so the longest delay still reads a stored sample.
            std::vector<float> buffer_;

            /// @brief Holds the index the next sample is written to.
            std::size_t write_index_ = 0;

            /// @brief Holds the selected delay in samples.
            std::size_t delay_ = 0;
    };
} // namespace SushiDSP
