/**************************************************************************/
/* AntiderivativeAntialiaser.hpp                                          */
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

#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <vector>

#include "SushiDSP/circuits/models/SoftTransition.hpp"
#include "SushiDSP/circuits/stages/GridConductionLaw.hpp"
#include "SushiDSP/circuits/stages/PlateSwing.hpp"

namespace SushiDSP
{
    /// @brief First-order antiderivative antialiasing of one grid-and-plate stage, evaluated in
    /// the grid's own coordinate u = Vg - Vk: y = (G(u1) - G(u0)) / (g(u1) - g(u0)), where g(u) is
    /// the input that puts the grid at u and G is the antiderivative of the plate output with
    /// respect to that input. The tables are filled at construction and only read afterwards.
    /// Derived in docs/archive/superpowers/specs/2026-09-07-grid-knee-antialiasing-design.md
    /// section 15.
    class AntiderivativeAntialiaser
    {
    public:
        /// @brief The input difference under which the divided difference gives way to the
        /// transfer at the midpoint; both branches agree to under 1e-7 V there.
        static constexpr double kMinimumInputDifferenceVolts = 1.0e-4;

        /// @brief The half-width of the tabulated knee, in transition widths; the logistic is
        /// within 4e-11 of 0 or 1 outside it.
        static constexpr double kWindowTransitions = 24.0;

        /// @brief Knot intervals per transition width inside the tabulated knee.
        static constexpr std::size_t kIntervalsPerTransition = 16;

        /// @brief Knots in the knee table, one more than its intervals.
        static constexpr std::size_t kKnotCount
            = 2 * static_cast<std::size_t>(kWindowTransitions) * kIntervalsPerTransition + 1;

        /// @brief The half-width of the tabulated plate antiderivative on each side, in units of
        /// that side's tanh scale H / gain; the solver's grid bounds sit at the same reach.
        static constexpr double kPlateReach = 10.0;

        /// @brief Knot intervals per tanh scale inside the plate table.
        static constexpr std::size_t kIntervalsPerPlateScale = 64;

        /// @brief Knots on each side of the plate table, the rest knot included.
        static constexpr std::size_t kPlateKnotCount
            = static_cast<std::size_t>(kPlateReach) * kIntervalsPerPlateScale + 1;

        /// @brief Builds the antialiaser for the stage the same six constants describe.
        /// @param source_resistance_ohms Effective source resistance feeding the grid.
        /// @param grid_leak_resistance_ohms The grid-leak resistor to the cathode-bias voltage.
        /// @param cathode_bias_volts The grid's DC voltage against the cathode at rest.
        /// @param gain The inverting linear gain from grid to plate.
        /// @param grid_conduction The grid-current law's three constants.
        /// @param plate_swing The two headroom figures that bound the plate output.
        /// @pre gain > 0, grid_conduction.transition_volts > 0, both headrooms > 0.
        AntiderivativeAntialiaser(
            float source_resistance_ohms, float grid_leak_resistance_ohms, float cathode_bias_volts,
            float gain, const GridConductionLaw& grid_conduction, const PlateSwing& plate_swing)
            : linear_slope_(1.0 + static_cast<double>(source_resistance_ohms) / grid_leak_resistance_ohms)
            , conduction_scale_volts_(static_cast<double>(source_resistance_ohms) * grid_conduction.scale_amps)
            , threshold_excursion_(static_cast<double>(grid_conduction.threshold_volts) - cathode_bias_volts)
            , transition_(grid_conduction.transition_volts)
            , gain_(gain)
            , plate_swing_(plate_swing)
            , window_low_(threshold_excursion_ - kWindowTransitions * transition_)
            , knot_spacing_(transition_ / static_cast<double>(kIntervalsPerTransition))
            , window_high_(window_low_ + knot_spacing_ * static_cast<double>(kKnotCount - 1))
            , supply_plate_spacing_(plate_swing.headroom_to_supply_volts / gain / static_cast<double>(kIntervalsPerPlateScale))
            , bottom_plate_spacing_(plate_swing.headroom_to_bottom_volts / gain / static_cast<double>(kIntervalsPerPlateScale))
            , remainder_(kKnotCount, 0.0)
            , integrand_(kKnotCount, 0.0)
            , supply_plate_(kPlateKnotCount, 0.0)
            , supply_plate_slope_(kPlateKnotCount, 0.0)
            , bottom_plate_(kPlateKnotCount, 0.0)
            , bottom_plate_slope_(kPlateKnotCount, 0.0)
        {
            tabulate_remainder();
            tabulate_plate_antiderivative();
            plate_antiderivative_at_window_high_ = plate_antiderivative(window_high_);
        }

        /// @brief Returns the antialiased plate output for this sample's grid excursion.
        /// @param grid_excursion_volts Vg - Vk as the stage's solver returned it.
        /// @return The plate output in volts, inside the plate swing. Always finite.
        float process(float grid_excursion_volts)
        {
            const double excursion = grid_excursion_volts;
            double output = 0.0;
            if (!has_history_)
            {
                evaluate(excursion, previous_input_, previous_antiderivative_);
                output = memoised_transfer(excursion);
                has_history_ = true;
            }
            else if (excursion == previous_excursion_)
            {
                // g and G depend on the excursion alone, so the previous sample's values stand.
                output = memoised_transfer(0.5 * (excursion + previous_excursion_));
            }
            else
            {
                double input = 0.0;
                double antiderivative_value = 0.0;
                evaluate(excursion, input, antiderivative_value);
                const double input_difference = input - previous_input_;
                if (std::fabs(input_difference) < kMinimumInputDifferenceVolts)
                {
                    output = memoised_transfer(0.5 * (excursion + previous_excursion_));
                }
                else
                {
                    output = (antiderivative_value - previous_antiderivative_) / input_difference;
                }
                previous_input_ = input;
                previous_antiderivative_ = antiderivative_value;
            }

            previous_excursion_ = excursion;
            return static_cast<float>(output);
        }

        /// @brief Drops the previous sample, so the next one is the plain transfer.
        void reset()
        {
            has_history_ = false;
            previous_excursion_ = 0.0;
            previous_input_ = 0.0;
            previous_antiderivative_ = 0.0;
        }

        /// @brief Returns the stage input that puts the grid at the given excursion, g(u).
        double input_for_excursion(double excursion) const
        {
            double input = 0.0;
            double antiderivative_value = 0.0;
            evaluate(excursion, input, antiderivative_value);
            return input;
        }

        /// @brief Returns the antiderivative of the plate output with respect to the input, G(u).
        double antiderivative(double excursion) const
        {
            double input = 0.0;
            double antiderivative_value = 0.0;
            evaluate(excursion, input, antiderivative_value);
            return antiderivative_value;
        }

        /// @brief Returns the plate output at one excursion through the table the antiderivative
        /// uses, G'(u) / g'(u).
        double transfer(double excursion) const
        {
            const double logistic_value = models::logistic((excursion - threshold_excursion_) / transition_);
            const double plate_output = plate_swing_.bound(-gain_ * excursion);
            const double conduction_slope = conduction_scale_volts_ / transition_;
            const double numerator = linear_slope_ * plate_output + conduction_slope * remainder_slope(excursion, plate_output);
            const double denominator = linear_slope_ + conduction_slope * logistic_value;
            return numerator / denominator;
        }

    private:
        /// @brief Returns transfer() at the excursion, from the last evaluation when the
        /// excursion is bit for bit the one it was made at.
        double memoised_transfer(double excursion)
        {
            if (has_memo_ && std::memcmp(&excursion, &memo_excursion_, sizeof(double)) == 0)
            {
                return memo_output_;
            }
            memo_excursion_ = excursion;
            memo_output_ = transfer(excursion);
            has_memo_ = true;
            return memo_output_;
        }

        /// @brief Evaluates g(u) and G(u) from one exponential.
        void evaluate(double excursion, double& input, double& antiderivative_value) const
        {
            const double softplus_value = models::softplus((excursion - threshold_excursion_) / transition_);
            input = linear_slope_ * excursion + conduction_scale_volts_ * softplus_value;

            const double plate_value = plate_antiderivative(excursion);
            antiderivative_value = linear_slope_ * plate_value
                                   + conduction_scale_volts_ / transition_ * remainder_value(excursion, plate_value);
        }

        /// @brief Integrates the plate output over the excursion from rest, P(u), in closed form.
        double exact_plate_antiderivative(double excursion) const
        {
            return -plate_swing_.bound_antiderivative(-gain_ * excursion) / gain_;
        }

        /// @brief Evaluates P(u) through its table: the cubic Hermite interpolant on the side the
        /// excursion lies on, the closed form beyond the plate's reach.
        double plate_antiderivative(double excursion) const
        {
            const bool toward_bottom = excursion > 0.0;
            const double spacing = toward_bottom ? bottom_plate_spacing_ : supply_plate_spacing_;
            const std::vector<double>& values = toward_bottom ? bottom_plate_ : supply_plate_;
            const std::vector<double>& slopes = toward_bottom ? bottom_plate_slope_ : supply_plate_slope_;
            const double position = std::fabs(excursion) / spacing;
            if (position >= static_cast<double>(kPlateKnotCount - 1))
            {
                return exact_plate_antiderivative(excursion);
            }

            // Each side is tabulated in |u| from rest, with the slope dP/d|u| at every knot.
            const std::size_t interval = static_cast<std::size_t>(position);
            const double t = position - static_cast<double>(interval);
            const double t2 = t * t;
            const double t3 = t2 * t;
            const double slope_low = slopes[interval] * spacing;
            const double slope_high = slopes[interval + 1] * spacing;
            return (2.0 * t3 - 3.0 * t2 + 1.0) * values[interval] + (t3 - 2.0 * t2 + t) * slope_low
                   + (-2.0 * t3 + 3.0 * t2) * values[interval + 1] + (t3 - t2) * slope_high;
        }

        /// @brief Evaluates the knee integrand, the plate output times the grid's logistic.
        double integrand(double excursion) const
        {
            return plate_swing_.bound(-gain_ * excursion)
                   * models::logistic((excursion - threshold_excursion_) / transition_);
        }

        /// @brief Evaluates the tabulated remainder R(u): zero below the window, the cubic Hermite
        /// interpolant inside it, and the plate antiderivative above it.
        /// @param plate_value P(u) at the same excursion.
        double remainder_value(double excursion, double plate_value) const
        {
            const double position = (excursion - window_low_) / knot_spacing_;
            if (position <= 0.0)
            {
                return 0.0;
            }
            if (position >= static_cast<double>(kKnotCount - 1))
            {
                return remainder_.back() + plate_value - plate_antiderivative_at_window_high_;
            }

            const std::size_t interval = static_cast<std::size_t>(position);
            const double t = position - static_cast<double>(interval);
            const double t2 = t * t;
            const double t3 = t2 * t;
            const double slope_low = integrand_[interval] * knot_spacing_;
            const double slope_high = integrand_[interval + 1] * knot_spacing_;
            return (2.0 * t3 - 3.0 * t2 + 1.0) * remainder_[interval] + (t3 - 2.0 * t2 + t) * slope_low
                   + (-2.0 * t3 + 3.0 * t2) * remainder_[interval + 1] + (t3 - t2) * slope_high;
        }

        /// @brief Evaluates the slope of the tabulated remainder, R'(u): zero below the window,
        /// the Hermite interpolant's derivative inside it, and the plate output above it.
        /// @param plate_output The plate output at the same excursion.
        double remainder_slope(double excursion, double plate_output) const
        {
            const double position = (excursion - window_low_) / knot_spacing_;
            if (position <= 0.0)
            {
                return 0.0;
            }
            if (position >= static_cast<double>(kKnotCount - 1))
            {
                return plate_output;
            }

            const std::size_t interval = static_cast<std::size_t>(position);
            const double t = position - static_cast<double>(interval);
            const double t2 = t * t;
            const double slope_low = integrand_[interval] * knot_spacing_;
            const double slope_high = integrand_[interval + 1] * knot_spacing_;
            return ((6.0 * t2 - 6.0 * t) * remainder_[interval] + (3.0 * t2 - 4.0 * t + 1.0) * slope_low
                    + (-6.0 * t2 + 6.0 * t) * remainder_[interval + 1] + (3.0 * t2 - 2.0 * t) * slope_high)
                   / knot_spacing_;
        }

        /// @brief Fills the plate tables from the closed form: P and dP/d|u| at every knot on
        /// each side, from rest to the plate's reach.
        void tabulate_plate_antiderivative()
        {
            for (std::size_t knot = 0; knot < kPlateKnotCount; ++knot)
            {
                const double supply_excursion = -supply_plate_spacing_ * static_cast<double>(knot);
                const double bottom_excursion = bottom_plate_spacing_ * static_cast<double>(knot);
                supply_plate_[knot] = exact_plate_antiderivative(supply_excursion);
                bottom_plate_[knot] = exact_plate_antiderivative(bottom_excursion);
                // dP/d|u| is -bound(-gain u) on the supply side, where u < 0, and +bound on the bottom side.
                supply_plate_slope_[knot] = -plate_swing_.bound(-gain_ * supply_excursion);
                bottom_plate_slope_[knot] = plate_swing_.bound(-gain_ * bottom_excursion);
            }
        }

        /// @brief Fills the knee table: the integrand at every knot and its running integral from
        /// the window's low edge by eight-point Gauss-Legendre quadrature on each interval.
        void tabulate_remainder()
        {
            constexpr std::array<double, 4> kAbscissae{ 0.1834346424956498, 0.5255324099163290,
                                                        0.7966664774136267, 0.9602898564975363 };
            constexpr std::array<double, 4> kWeights{ 0.3626837833783620, 0.3137066458778873,
                                                      0.2223810344533745, 0.1012285362903763 };
            remainder_[0] = 0.0;
            integrand_[0] = integrand(window_low_);
            for (std::size_t knot = 1; knot < kKnotCount; ++knot)
            {
                const double low = window_low_ + knot_spacing_ * static_cast<double>(knot - 1);
                const double centre = low + 0.5 * knot_spacing_;
                const double half = 0.5 * knot_spacing_;
                double sum = 0.0;
                for (std::size_t point = 0; point < kAbscissae.size(); ++point)
                {
                    sum += kWeights[point]
                           * (integrand(centre - half * kAbscissae[point]) + integrand(centre + half * kAbscissae[point]));
                }
                remainder_[knot] = remainder_[knot - 1] + half * sum;
                integrand_[knot] = integrand(low + knot_spacing_);
            }
        }

        /// @brief The input per volt of excursion with the grid cold, 1 + Rs / Rg.
        double linear_slope_;

        /// @brief The source drop per unit of softplus, Rs times the law's current scale, in volts.
        double conduction_scale_volts_;

        /// @brief The excursion at the knee's centre, the law's threshold above the cathode bias.
        double threshold_excursion_;

        /// @brief The knee's width in volts.
        double transition_;

        /// @brief The inverting linear gain from grid to plate.
        double gain_;

        /// @brief The two headrooms the plate tanh is bounded by.
        PlateSwing plate_swing_;

        /// @brief The excursion at the table's first knot.
        double window_low_;

        /// @brief The excursion between neighbouring knee knots.
        double knot_spacing_;

        /// @brief The excursion at the table's last knot.
        double window_high_;

        /// @brief The excursion between neighbouring plate knots on the supply side.
        double supply_plate_spacing_;

        /// @brief The excursion between neighbouring plate knots on the bottom side.
        double bottom_plate_spacing_;

        /// @brief The remainder R at every knee knot, zero at the first.
        std::vector<double> remainder_;

        /// @brief The integrand, R's exact slope, at every knee knot.
        std::vector<double> integrand_;

        /// @brief P at every plate knot on the supply side, indexed by |u|.
        std::vector<double> supply_plate_;

        /// @brief dP/d|u| at every plate knot on the supply side.
        std::vector<double> supply_plate_slope_;

        /// @brief P at every plate knot on the bottom side, indexed by |u|.
        std::vector<double> bottom_plate_;

        /// @brief dP/d|u| at every plate knot on the bottom side.
        std::vector<double> bottom_plate_slope_;

        /// @brief P at the table's last knot, the constant the remainder carries above the window.
        double plate_antiderivative_at_window_high_ = 0.0;

        /// @brief Whether a previous sample exists to difference against.
        bool has_history_ = false;

        /// @brief The previous sample's grid excursion.
        double previous_excursion_ = 0.0;

        /// @brief The previous sample's input, g(previous excursion).
        double previous_input_ = 0.0;

        /// @brief The previous sample's antiderivative, G(previous excursion).
        double previous_antiderivative_ = 0.0;

        /// @brief Whether memo_excursion_ and memo_output_ hold an evaluation of transfer().
        bool has_memo_ = false;

        /// @brief The excursion transfer() was last evaluated at through memoised_transfer().
        double memo_excursion_ = 0.0;

        /// @brief transfer() at memo_excursion_.
        double memo_output_ = 0.0;
    };
}
