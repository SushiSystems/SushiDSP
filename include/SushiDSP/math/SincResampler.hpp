/****************************************************************/
/* SincResampler.hpp                                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
