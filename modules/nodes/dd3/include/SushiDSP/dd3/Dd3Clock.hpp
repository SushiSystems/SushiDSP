/****************************************************************/
/* Dd3Clock.hpp                                                 */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file Dd3Clock.hpp
 * @brief The DD-3's TIME-pot VCO law and its R56/C52 slew on log(fs).
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include "SushiDSP/math/SmoothedParameter.hpp"

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

        static constexpr double kFastRateHz              = 82500.0;
        static constexpr double kSlowRateHz              = 20000.0;
        static constexpr double kSlewResistanceOhms      = 22000.0;    // R56
        static constexpr double kSlewCapacitanceFarads   = 2.7e-8;     // C52 0.027 uF
        static constexpr double kSlewTimeConstantSeconds = 5.94e-4;    // 22k * 0.027u
        static constexpr float  kSlewTimeConstantMs      = 0.594f;

    private:
        SushiDSP::SmoothedParameter control_ { static_cast<float>(std::log(kFastRateHz)) };
    };
}
