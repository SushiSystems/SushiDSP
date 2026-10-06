/**************************************************************************/
/* SincResampler.hpp                                                      */
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
 * @file SincResampler.hpp
 * @brief Declares the offline windowed-sinc sample-rate converter.
 * @author Mustafa Garip
 */

#pragma once

#include <vector>

namespace SushiDSP
{
    /// @brief Offline windowed-sinc sample-rate converter (64 taps, Kaiser beta 8.6).
    ///
    /// Control-thread only: allocates and is not real-time safe. Intended for load-time
    /// conversion of impulse responses to the host's sample rate.
    class SincResampler
    {
    public:
        /// @brief Resamples a signal from one sample rate to another.
        /// @param input Source samples.
        /// @param input_rate Sample rate the input was captured at, in Hz.
        /// @param output_rate Sample rate to produce, in Hz.
        /// @return The resampled signal, rounded to the nearest whole sample count.
        /// @throws std::invalid_argument if either rate is not positive.
        static std::vector<float> resample(const std::vector<float>& input, double input_rate, double output_rate);
    };
}
