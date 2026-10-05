/****************************************************************/
/* PlateSwing.hpp                                               */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file PlateSwing.hpp
 * @brief Defines a stage's plate headrooms and the tanh ceiling that bounds its output by them.
 * @author Mustafa Garip
 */

#pragma once

#include <cmath>

namespace SushiDSP
{
    /// @brief How far a stage's plate can swing from its idle point before it saturates, and the
    /// two-sided tanh ceiling that bounds a linear plate output by it
    /// (docs/archive/superpowers/specs/2026-09-07-triode-plate-swing-design.md section 3).
    struct PlateSwing
    {
        /// @brief The swing toward the supply, the cutoff side: idle plate current times the load.
        float headroom_to_supply_volts;

        /// @brief The swing toward the plate bottom, the conduction side: the plate's travel from
        /// idle to Vgk = 0 on the load line.
        float headroom_to_bottom_volts;

        /// @brief Bounds an inverting linear plate output by the two headrooms.
        /// @param linear_plate_volts The unbounded plate output, positive toward the supply.
        /// @return The plate output inside (-headroom_to_bottom, +headroom_to_supply). Always finite.
        float bound(float linear_plate_volts) const
        {
            if (linear_plate_volts > 0.0f)
            {
                return headroom_to_supply_volts * std::tanh(linear_plate_volts / headroom_to_supply_volts);
            }
            return -headroom_to_bottom_volts * std::tanh(-linear_plate_volts / headroom_to_bottom_volts);
        }

        /// @brief Bounds an inverting linear plate output by the two headrooms, in double.
        /// @param linear_plate_volts The unbounded plate output, positive toward the supply.
        /// @return The same value bound(float) returns, evaluated in double. Always finite.
        double bound(double linear_plate_volts) const
        {
            const double headroom = headroom_for(linear_plate_volts);
            return headroom * std::tanh(linear_plate_volts / headroom);
        }

        /// @brief Integrates bound() from zero to the given linear plate output.
        /// @param linear_plate_volts The upper limit of the integral, positive toward the supply.
        /// @return H * H * ln cosh(linear_plate_volts / H) with H the side's headroom; even in its
        /// argument, zero at zero, and its derivative is bound(). Always finite.
        double bound_antiderivative(double linear_plate_volts) const
        {
            const double headroom = headroom_for(linear_plate_volts);
            const double argument = std::fabs(linear_plate_volts / headroom);
            // ln cosh z = |z| + ln(1 + exp(-2|z|)) - ln 2, which never overflows.
            const double log_cosh = argument + std::log1p(std::exp(-2.0 * argument)) - 0.6931471805599453;
            return headroom * headroom * log_cosh;
        }

    private:
        /// @brief Selects the headroom of the side a linear plate output swings toward.
        double headroom_for(double linear_plate_volts) const
        {
            return linear_plate_volts > 0.0 ? static_cast<double>(headroom_to_supply_volts)
                                            : static_cast<double>(headroom_to_bottom_volts);
        }
    };
}
