/****************************************************************/
/* PowerSupplySagStage.hpp                                      */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file PowerSupplySagStage.hpp
 * @brief Declares the power-supply sag stage.
 * @author Mustafa Garip
 */

#pragma once

namespace SushiDSP
{
    /// @brief Models power-supply sag: an envelope follower on the rectified signal drives a
    /// dynamic gain-reduction factor, so sustained loud playing compresses ("bloom").
    class PowerSupplySagStage
    {
    public:
        /// @param sample_rate The sample rate in Hz.
        /// @param sag_amount How much gain reduction the envelope can cause, in [0, 1].
        /// @param recovery_time_seconds The envelope follower's one-pole time constant.
        PowerSupplySagStage(double sample_rate, float sag_amount, float recovery_time_seconds);

        /// @brief Processes one sample: updates the envelope, then applies gain reduction.
        /// @param input The input sample.
        /// @return The dynamically gain-reduced output sample. Always finite.
        float process_sample(float input);

        /// @brief Clears the envelope follower's state.
        void reset();

    private:
        float envelope_coefficient_;
        float sag_amount_;
        float envelope_ = 0.0f;
    };
}
