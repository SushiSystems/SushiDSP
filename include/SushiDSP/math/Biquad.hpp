/****************************************************************/
/* Biquad.hpp                                                   */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file Biquad.hpp
 * @brief Declares a Direct Form I biquad filter with RBJ coefficient derivation.
 * @author Mustafa Garip
 */

#pragma once

namespace SushiDSP
{
    /// @brief Direct Form I biquad filter with RBJ cookbook coefficient derivation, its
    /// coefficients and state in double behind a float sample interface
    /// (docs/archive/superpowers/specs/2026-09-07-grid-knee-antialiasing-design.md section 15).
    class Biquad
    {
    public:
        /// @brief Configures this filter as a low-pass with the given cutoff and Q.
        /// @param sample_rate Sample rate in Hz.
        /// @param cutoff_hz Cutoff frequency in Hz.
        /// @param q Quality factor (0.707 = Butterworth).
        void set_low_pass(double sample_rate, double cutoff_hz, double q);

        /// @brief Configures this filter as a first-order (one-pole) low-pass.
        ///
        /// A single RC section, bilinear-transformed with the corner prewarped, so the second
        /// numerator and denominator taps are zero. This is the right shape for a physical RC
        /// pole - a Butterworth biquad would roll off at twice the rate a real one does.
        /// @param sample_rate Sample rate in Hz.
        /// @param cutoff_hz Corner frequency in Hz, where the response is -3 dB.
        void set_one_pole_low_pass(double sample_rate, double cutoff_hz);

        /// @brief Configures this filter as a first-order (one-pole) high-pass.
        ///
        /// A single RC section mapped by the impulse-invariant pole R = exp(-2*pi*fc/fs): unity at
        /// Nyquist and -3 dB at the corner (include/SushiDSP/README.md).
        /// @param cutoff_hz Corner frequency in Hz, where the response is -3 dB.
        /// @pre 0 < cutoff_hz < sample_rate / 2
        void set_first_order_high_pass(double sample_rate, double cutoff_hz);

        /// @brief Configures this filter as a high-pass with the given cutoff and Q.
        ///
        /// @param sample_rate Sample rate in Hz.
        /// @param cutoff_hz Cutoff frequency in Hz.
        /// @param q Quality factor (0.707 = Butterworth).
        /// @pre cutoff_hz / sample_rate >= 1e-4; use set_first_order_high_pass for corners that low.
        void set_high_pass(double sample_rate, double cutoff_hz, double q);

        /// @brief Configures this filter as a low-shelf with the given corner, gain, and Q.
        /// @param sample_rate Sample rate in Hz.
        /// @param cutoff_hz Shelf corner frequency in Hz.
        /// @param gain_db Shelf gain in decibels (positive boosts, negative cuts).
        /// @param q Shelf slope parameter (RBJ cookbook S=1 equivalent at q=1.0).
        void set_low_shelf(double sample_rate, double cutoff_hz, double gain_db, double q);

        /// @brief Configures this filter as a high-shelf with the given corner, gain, and Q.
        /// @param sample_rate Sample rate in Hz.
        /// @param cutoff_hz Shelf corner frequency in Hz.
        /// @param gain_db Shelf gain in decibels.
        /// @param q Shelf slope parameter.
        void set_high_shelf(double sample_rate, double cutoff_hz, double gain_db, double q);

        /// @brief Configures this filter as a peaking EQ with the given center, gain, and Q.
        /// @param sample_rate Sample rate in Hz.
        /// @param center_hz Center frequency in Hz.
        /// @param gain_db Peak gain in decibels (positive boosts, negative cuts).
        /// @param q Bandwidth parameter (higher Q = narrower peak).
        void set_peaking(double sample_rate, double center_hz, double gain_db, double q);

        /// @brief Configures this filter as a first-order shelf with independent zero and pole.
        ///
        /// Both corners are prewarped separately, so a mirrored pair cancels exactly; see
        /// docs/archive/superpowers/specs/2026-09-05-dd3-spx90-design.md for the derivation.
        /// @param sample_rate Sample rate in Hz.
        /// @param zero_hz Frequency of the transfer function's zero, in Hz.
        /// @param pole_hz Frequency of its pole, in Hz.
        /// @param low_frequency_gain Gain below both corners; 1.0 is unity.
        void set_first_order_shelf(double sample_rate, double zero_hz, double pole_hz,
                                   double low_frequency_gain);

        /// @brief Processes one sample through the filter.
        /// @param input The input sample.
        /// @return The filtered output sample.
        float process_sample(float input)
        {
            const double x = input;
            const double output = b0_ * x + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;

            x2_ = x1_;
            x1_ = x;
            y2_ = y1_;
            y1_ = output;

            return static_cast<float>(output);
        }

        /// @brief Clears the filter's internal state.
        void reset();

        /// @brief Loads five coefficients computed elsewhere, leaving the state untouched.
        /// @param b0 Feed-forward tap on the current input, already divided by a0.
        /// @param b1 Feed-forward tap on the previous input, already divided by a0.
        /// @param b2 Feed-forward tap on the input before that, already divided by a0.
        /// @param a1 Feedback tap on the previous output, already divided by a0.
        /// @param a2 Feedback tap on the output before that, already divided by a0.
        /// @pre |a2| < 1 and |a1| < 1 + a2, so the poles lie inside the unit circle.
        void set_coefficients(double b0, double b1, double b2, double a1, double a2);

    private:
        /// @brief The feed-forward coefficients, normalised by a0.
        double b0_ = 1.0, b1_ = 0.0, b2_ = 0.0;

        /// @brief The feedback coefficients, normalised by a0.
        double a1_ = 0.0, a2_ = 0.0;

        /// @brief The two previous inputs.
        double x1_ = 0.0, x2_ = 0.0;

        /// @brief The two previous outputs.
        double y1_ = 0.0, y2_ = 0.0;
    };
}
