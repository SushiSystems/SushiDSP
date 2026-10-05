/****************************************************************/
/* CrossoverPair.hpp                                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file CrossoverPair.hpp
 * @brief The power-complementary pair that splits the field between the wave and geometric layers.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include "SushiDSP/math/Biquad.hpp"

namespace SushiDSP::room
{
    /// @brief Splits a signal at fourth order into two halves whose powers sum to one at every
    /// frequency, so two uncorrelated fields, the wave layer's and the geometric layers', meet
    /// at the seam with no dip and no hump; the amplitude sum of one correlated signal is not
    /// flat through this pair. Spec §9a.4.
    class CrossoverPair
    {
    public:
        /// @brief Places the crossover; control thread, before or between blocks.
        /// @param crossover_hz Zero or below routes everything to the high half untouched.
        /// @pre crossover_hz < sample_rate / 2.
        void set_crossover(double sample_rate, double crossover_hz) noexcept;

        /// @brief Splits one sample on the audio thread through the two sections of a
        /// fourth-order Butterworth per half.
        void process_sample(float input, float& low_out, float& high_out) noexcept;

        /// @brief Clears every section's state.
        void reset() noexcept;

    private:
        /// @brief Stores the low-Q section of the Butterworth low-pass.
        Biquad low_first_;

        /// @brief Stores the high-Q section of the Butterworth low-pass.
        Biquad low_second_;

        /// @brief Stores the low-Q section of the Butterworth high-pass.
        Biquad high_first_;

        /// @brief Stores the high-Q section of the Butterworth high-pass.
        Biquad high_second_;

        /// @brief Records that the crossover is off and the high half carries everything.
        bool bypassed_ = true;
    };
}
