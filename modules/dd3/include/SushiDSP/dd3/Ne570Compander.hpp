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

    private:
        const CompanderHalf half_;
        double              sample_rate_ = 48000.0;
        double              coefficient_ = 0.0;
        double              envelope_    = 0.0;
    };
}
