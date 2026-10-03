/**************************************************************************/
/* Ne570Compander.hpp                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file Ne570Compander.hpp
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
    ///
    /// The compressor rectifies its own output (2:1, `sqrt(kReferenceVolts * volts)`); the
    /// expander rectifies its input (1:2, `volts^2 / kReferenceVolts`). The compressor's gain
    /// therefore lags its own rectifier by one sample: the loop is otherwise algebraic, and at a
    /// 6.8 ms time constant one sample of lag is 0.3 % of one time constant at 48 kHz.
    ///
    /// The rectifier and its pole run at @ref kRectifierOversampling times the host rate; the
    /// gain cell does not. Rectification is the one nonlinearity here whose aliasing is a
    /// discretisation artefact rather than modelled hardware, and
    /// docs/archive/superpowers/specs/2026-09-05-dd3-spx90-design.md, Model corrections, carries the
    /// ruling and the measurements.
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

        static constexpr double kRectifierResistanceOhms    = 10000.0;   ///< NE570 internal R1.
        static constexpr double kRectifierCapacitanceFarads = 6.8e-7;    ///< C32 = C33 = 0.68 uF.
        static constexpr double kTimeConstantSeconds        = 6.8e-3;    ///< 10k * 0.68u.
        static constexpr double kRippleCornerHz             = 23.4051;   ///< 1/(2*pi*10k*0.68u).
        static constexpr double kReferenceVolts             = 6.6125;    ///< From check points [2],[3].
        static constexpr double kEnvelopeFloor              = 1e-4;      ///< 80 dB maximum gain.

        /// @brief Sub-steps the rectifier and its pole take per host sample.
        ///
        /// The board's rectifier is continuous time, so the harmonics `|x|` carries at every
        /// 2k*f0 never fold. Sampling it at the host rate does fold them, and the fold lands
        /// where the pole cannot reach: at 48 kHz an 8 kHz tone puts 6*f0 on 48001.5 Hz, which
        /// aliases to 1.46 Hz and modulates the gain cell at full weight. Eight sub-steps move
        /// the surviving fold from the 6th harmonic of `|x|` to the 48th, whose Fourier
        /// coefficient is 1/2303 against 1/35, and take the DD-3's alias floor from -35.86 dB to
        /// -55.32 dB at the service note's stimulus.
        static constexpr int kRectifierOversampling = 8;

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
        double              sample_rate_ = 48000.0;
        double              coefficient_ = 0.0;
        double              envelope_    = 0.0;
        double              previous_    = 0.0;   // what advance_envelope() last rectified
    };
}
