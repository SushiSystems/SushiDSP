/**************************************************************************/
/* ne570_compander.hpp                                                    */
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
 * @file ne570_compander.hpp
 * @brief One half of an NE570 companding pair, modelled as an open-loop rectifier and gain cell.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

namespace SushiDSP::dd3
{
    /// @brief Which half of the NE570 an instance models.
    enum class CompanderHalf
    {
        Compressor,
        Expander
    };

    /// @brief One half of an NE570 companding pair: an open-loop rectifier feeding a gain cell.
    /// @see modules/nodes/dd3/README.md, "The compander's rectifier"
    class Ne570Compander
    {
        public:
            /// @brief Fixes which half this instance models; a compressor never becomes an expander.
            /// @param half Compressor (rectifier on the output, 2:1) or expander (on the input, 1:2).
            explicit Ne570Compander(CompanderHalf half);

            /// @brief Derives the rectifier's one-pole coefficient. Control thread only.
            /// @param sample_rate Rate process_sample() will be called at.
            void prepare(double sample_rate);

            /// @brief Runs one sample through the rectifier and the gain cell.
            /// @param volts Input in volts.
            /// @return Output in volts.
            float process_sample(float volts);

            /// @brief Zeroes the envelope.
            void reset();

            /// @brief The rectifier's current output, in volts. For tests and for diagnostics.
            double envelope_volts() const;

            static constexpr double K_RECTIFIER_RESISTANCE_OHMS = 10000.0; ///< NE570 internal R1.
            static constexpr double K_RECTIFIER_CAPACITANCE_FARADS =
                6.8e-7;                                               ///< C32 = C33 = 0.68 uF.
            static constexpr double K_TIME_CONSTANT_SECONDS = 6.8e-3; ///< 10k * 0.68u.
            static constexpr double K_RIPPLE_CORNER_HZ = 23.4051;     ///< 1/(2*pi*10k*0.68u).
            static constexpr double K_REFERENCE_VOLTS = 6.6125; ///< From check points [2],[3].
            static constexpr double K_ENVELOPE_FLOOR = 1e-4;    ///< 80 dB maximum gain.

            /// @brief Sub-steps the rectifier and its pole take per host sample.
            /// @see modules/nodes/dd3/README.md, "The rectifier's oversampling"
            static constexpr int K_RECTIFIER_OVERSAMPLING = 8;

        private:
            /// @brief Runs the rectifier and its pole across one host sample.
            ///
            /// The sub-samples come from a linear reconstruction between this call's signal and the
            /// last one's. What the pole needs is the area under `|x|`, not its spectrum: replacing
            /// the line with a 64-tap windowed sinc moves the alias floor by 0.6 dB and a parabola
            /// through three host samples moves it by 0.9 dB, for four times the arithmetic.
            /// @param volts The signal this half rectifies, at the end of the host sample.
            void advance_envelope(double volts);

            const CompanderHalf half_;
            double sample_rate_ = 48000.0;
            double coefficient_ = 0.0;
            double envelope_ = 0.0;
            double previous_ = 0.0; // what advance_envelope() last rectified
    };
} // namespace SushiDSP::dd3
