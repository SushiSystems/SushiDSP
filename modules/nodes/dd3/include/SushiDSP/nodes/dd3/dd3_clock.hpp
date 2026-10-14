/**************************************************************************/
/* dd3_clock.hpp                                                          */
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
 * @file dd3_clock.hpp
 * @brief The DD-3's TIME-pot VCO law and its R56/C52 slew on log(fs).
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <SushiDSP/foundation/math/smoothed_parameter.hpp>

#include <cmath>
#include <cstdint>

namespace SushiDSP::dd3
{
    /// @brief Models the DD-3's sample clock: an exponential VCO law in the TIME knob,
    /// slewed by the R56/C52 pole sitting on the VCO's control node.
    ///
    /// N5: the pole sits on log(fs) rather than fs directly, because the control node is a
    /// pitch (a VCO input), and a constant-rate glide in log(fs) is a constant-rate glide in
    /// pitch. Ruling 11: advance() is called once per 8-frame sub-block, not once per sample.
    class Dd3Clock
    {
        public:
            /// @brief Prepares the R56/C52 pole at this rate. Control thread only.
            /// @param sample_rate Rate advance() is counted in.
            void prepare(double sample_rate);

            /// @brief Sets the D.TIME knob position. Real-time safe.
            /// @param normalized 0 = fastest clock (shortest delay), 1 = slowest.
            void set_time(float normalized);

            /// @brief Advances the slew pole and reports the clock.
            /// @param frames Samples to advance; the DD-3 calls this once per 8-frame sub-block.
            /// @return The sample clock in Hz, inside [20000, 82500].
            double advance(std::uint32_t frames);

            /// @brief The clock without advancing.
            /// @return Hz.
            double sample_rate_hz() const;

            /// @brief Snaps the pole to the current knob position, for a click-free restart.
            void reset();

            /// @brief The VCO law with the log taper already folded in.
            /// @param normalized Knob position in [0, 1].
            /// @return 82500 * 4.125^(-normalized) Hz.
            static double clock_for_time(double normalized);

            static constexpr double K_FAST_RATE_HZ = 82500.0;
            static constexpr double K_SLOW_RATE_HZ = 20000.0;
            static constexpr double K_SLEW_RESISTANCE_OHMS = 22000.0;       // R56
            static constexpr double K_SLEW_CAPACITANCE_FARADS = 2.7e-8;     // C52 0.027 uF
            static constexpr double K_SLEW_TIME_CONSTANT_SECONDS = 5.94e-4; // 22k * 0.027u
            static constexpr float K_SLEW_TIME_CONSTANT_MS = 0.594f;

        private:
            SushiDSP::SmoothedParameter control_{static_cast<float>(std::log(K_FAST_RATE_HZ))};
    };
} // namespace SushiDSP::dd3
