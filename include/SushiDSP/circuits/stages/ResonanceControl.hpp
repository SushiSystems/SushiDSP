/****************************************************************/
/* ResonanceControl.hpp                                         */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file ResonanceControl.hpp
 * @brief Declares the low shelf of a power section's Resonance or Deep control.
 * @author Mustafa Garip
 */

#pragma once

#include "SushiDSP/math/Biquad.hpp"

namespace SushiDSP
{
    /// @brief The low-frequency shelf a Resonance or Deep knob places in a power section's
    /// negative-feedback path, mirrored to the forward path as a low shelf of the same corner.
    class ResonanceControl
    {
    public:
        /// @param sample_rate The rate the stage runs at, in hertz.
        /// @param corner_hz Shelf corner frequency in hertz.
        /// @param maximum_boost_db Boost at depth 1; depth 0 is flat and depth 0.5 is half the boost.
        ResonanceControl(double sample_rate, float corner_hz, float maximum_boost_db);

        /// @brief Sets the knob position.
        /// @param depth In [0, 1]; values outside are clamped.
        void set_depth(float depth);

        /// @brief Processes one sample. Audio-thread safe.
        float process_sample(float input);

        /// @brief Clears the filter state.
        void reset();

        /// @brief Returns the boost in decibels the current depth applies.
        float boost_db() const;

    private:
        double sample_rate_;
        float corner_hz_;
        float maximum_boost_db_;
        float depth_ = 0.0f;
        Biquad shelf_;
    };
}
