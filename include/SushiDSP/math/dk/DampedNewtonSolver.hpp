/**************************************************************************/
/* DampedNewtonSolver.hpp                                                 */
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
#include <cstdint>

#include "SushiDSP/math/dk/DenseLu.hpp"

namespace SushiDSP::dk
{
    /// @brief Damped multivariate Newton iteration for the nonlinear port of a DK circuit.
    ///
    /// The caller supplies a system object exposing
    /// `void residual_and_jacobian(const std::array<double, N>& v, std::array<double, N>& r,
    /// std::array<std::array<double, N>, N>& jacobian)`, which must evaluate both together: the
    /// solver never asks for one alone, so a system may share the exponentials between them.
    /// Every buffer is a member std::array and no step allocates, locks, or throws, so solve()
    /// is safe on the audio thread. Statistics are counters the host can poll to see how hard
    /// the circuit is working.
    /// @tparam N Dimension of the nonlinear port.
    template <std::size_t N>
    class DampedNewtonSolver
    {
    public:
        /// @brief Iterates from the supplied starting point until the residual vanishes.
        /// @param system The residual/Jacobian evaluator.
        /// @param v In: the starting iterate (warm-starting from the previous sample is the
        /// intended use). Out: on success the converged iterate; on failure the lowest-residual
        /// finite iterate visited, which is never worse than the one passed in.
        /// @return True on convergence, meaning either a residual below the absolute tolerance or
        /// a norm-reducing step that no longer moves the iterate. A step that stopped moving
        /// without reducing the residual is a stall, not convergence, and keeps iterating; that
        /// distinction is what stops a Jacobian underflowing far from the root from being
        /// reported as a solution. False when the iteration budget ran out, the Jacobian stayed
        /// singular, or the system produced a non-finite value.
        template <typename System>
        bool solve(System&& system, std::array<double, N>& v)
        {
            ++call_count_;

            bool   residual_is_current = false;
            double residual_norm = 0.0;

            best_is_valid_ = false;
            best_residual_norm_ = 0.0;

            for (std::uint32_t iteration = 0; iteration < maximum_iterations_; ++iteration)
            {
                if (!residual_is_current)
                {
                    system.residual_and_jacobian(v, residual_, jacobian_);
                    if (!is_finite(residual_) || !is_finite(jacobian_))
                    {
                        return fail(iteration, v);
                    }
                    residual_norm = infinity_norm(residual_);
                    residual_is_current = true;
                    note_best(v, residual_norm);
                }

                if (residual_norm <= kAbsoluteTolerance)
                {
                    note_iterations(iteration);
                    return true;
                }

                if (!factorise_with_one_retry())
                {
                    return fail(iteration, v);
                }

                for (std::size_t index = 0; index < N; ++index)
                {
                    negated_residual_[index] = -residual_[index];
                }
                lu_.solve(negated_residual_, step_);
                if (!is_finite(step_))
                {
                    return fail(iteration, v);
                }

                double damping = 1.0;
                bool   residual_decreased = false;
                if (!line_search(system, v, residual_norm, damping, residual_decreased))
                {
                    return fail(iteration, v);
                }

                v = trial_v_;
                residual_.swap(trial_residual_);
                jacobian_.swap(trial_jacobian_);
                residual_norm = infinity_norm(residual_);
                note_best(v, residual_norm);

                const double step_norm = damping * infinity_norm(step_);
                if (residual_decreased && step_norm <= relative_tolerance_ * (1.0 + infinity_norm(v)))
                {
                    note_iterations(iteration + 1);
                    return true;
                }
            }

            return fail(maximum_iterations_, v);
        }

        /// @brief Sets the iteration budget of a single solve() call.
        /// @param iterations Maximum Newton iterations; values below one are clamped to one.
        void set_maximum_iterations(std::uint32_t iterations)
        {
            maximum_iterations_ = iterations < 1u ? 1u : iterations;
        }

        /// @brief Sets how many times the step may be halved while looking for a norm reduction.
        /// @param halvings Maximum halvings per iteration.
        void set_maximum_halvings(std::uint32_t halvings)
        {
            maximum_halvings_ = halvings;
        }

        /// @brief Sets the step-size convergence tolerance, relative to the iterate magnitude.
        /// @param tolerance Relative tolerance.
        void set_relative_tolerance(double tolerance)
        {
            relative_tolerance_ = tolerance;
        }

        /// @brief Number of solve() calls that failed to converge since the last reset.
        /// @return The non-convergence count.
        std::uint64_t non_converged_count() const
        {
            return non_converged_count_;
        }

        /// @brief Number of solve() calls since the last reset.
        /// @return The call count.
        std::uint64_t call_count() const
        {
            return call_count_;
        }

        /// @brief Largest iteration count any single solve() call needed since the last reset.
        /// @return The worst-case iteration count.
        std::uint32_t maximum_iterations_seen() const
        {
            return maximum_iterations_seen_;
        }

        /// @brief Clears the call, non-convergence, and worst-case iteration counters.
        void reset_statistics()
        {
            call_count_ = 0;
            non_converged_count_ = 0;
            maximum_iterations_seen_ = 0;
        }

    private:
        static constexpr double        kAbsoluteTolerance = 1e-12;
        static constexpr double        kDiagonalPerturbation = 1e-12;
        static constexpr std::uint32_t kDefaultMaximumIterations = 8;
        static constexpr std::uint32_t kDefaultMaximumHalvings = 4;
        static constexpr double        kDefaultRelativeTolerance = 1e-9;

        static bool is_finite(const std::array<double, N>& vector)
        {
            for (std::size_t index = 0; index < N; ++index)
            {
                if (!std::isfinite(vector[index]))
                {
                    return false;
                }
            }
            return true;
        }

        static bool is_finite(const std::array<std::array<double, N>, N>& matrix)
        {
            for (std::size_t row = 0; row < N; ++row)
            {
                if (!is_finite(matrix[row]))
                {
                    return false;
                }
            }
            return true;
        }

        static double infinity_norm(const std::array<double, N>& vector)
        {
            double norm = 0.0;
            for (std::size_t index = 0; index < N; ++index)
            {
                const double magnitude = std::fabs(vector[index]);
                if (magnitude > norm)
                {
                    norm = magnitude;
                }
            }
            return norm;
        }

        bool factorise_with_one_retry()
        {
            if (lu_.factorise(jacobian_))
            {
                return true;
            }
            perturbed_jacobian_ = jacobian_;
            for (std::size_t index = 0; index < N; ++index)
            {
                perturbed_jacobian_[index][index]
                    += kDiagonalPerturbation * (1.0 + std::fabs(jacobian_[index][index]));
            }
            return lu_.factorise(perturbed_jacobian_);
        }

        template <typename System>
        bool evaluate_trial(System&& system, const std::array<double, N>& v, double damping)
        {
            for (std::size_t index = 0; index < N; ++index)
            {
                trial_v_[index] = v[index] + damping * step_[index];
            }
            if (!is_finite(trial_v_))
            {
                return false;
            }
            system.residual_and_jacobian(trial_v_, trial_residual_, trial_jacobian_);
            return is_finite(trial_residual_) && is_finite(trial_jacobian_);
        }

        template <typename System>
        bool line_search(System&& system, const std::array<double, N>& v, double residual_norm,
                         double& accepted_damping, bool& residual_decreased)
        {
            residual_decreased = false;
            double damping = 1.0;
            double smallest_finite_damping = 0.0;
            bool   any_finite = false;

            for (std::uint32_t halving = 0;; ++halving)
            {
                if (evaluate_trial(system, v, damping))
                {
                    any_finite = true;
                    smallest_finite_damping = damping;
                    if (infinity_norm(trial_residual_) < residual_norm)
                    {
                        accepted_damping = damping;
                        residual_decreased = true;
                        return true;
                    }
                }
                if (halving >= maximum_halvings_)
                {
                    break;
                }
                damping *= 0.5;
            }

            if (!any_finite)
            {
                return false;
            }
            if (smallest_finite_damping != damping)
            {
                evaluate_trial(system, v, smallest_finite_damping);
            }
            accepted_damping = smallest_finite_damping;
            return true;
        }

        void note_best(const std::array<double, N>& v, double residual_norm)
        {
            if (best_is_valid_ && residual_norm >= best_residual_norm_)
            {
                return;
            }
            best_residual_norm_ = residual_norm;
            best_v_ = v;
            best_is_valid_ = true;
        }

        /// @brief Reports non-convergence, leaving the caller's iterate at the best one seen.
        ///
        /// A line search that found no norm reduction still takes its step, so that the next
        /// iteration can leave a flat region; without this the iterate handed back on failure
        /// could be strictly worse than the one the caller started from.
        bool fail(std::uint32_t iterations, std::array<double, N>& v)
        {
            if (best_is_valid_)
            {
                v = best_v_;
            }
            ++non_converged_count_;
            note_iterations(iterations);
            return false;
        }

        void note_iterations(std::uint32_t iterations)
        {
            if (iterations > maximum_iterations_seen_)
            {
                maximum_iterations_seen_ = iterations;
            }
        }

        DenseLu<N>                           lu_{};
        std::array<std::array<double, N>, N> jacobian_{};
        std::array<std::array<double, N>, N> perturbed_jacobian_{};
        std::array<std::array<double, N>, N> trial_jacobian_{};
        std::array<double, N>                residual_{};
        std::array<double, N>                negated_residual_{};
        std::array<double, N>                trial_residual_{};
        std::array<double, N>                step_{};
        std::array<double, N>                trial_v_{};
        std::array<double, N>                best_v_{};

        double best_residual_norm_{ 0.0 };
        bool   best_is_valid_{ false };

        std::uint32_t maximum_iterations_{ kDefaultMaximumIterations };
        std::uint32_t maximum_halvings_{ kDefaultMaximumHalvings };
        double        relative_tolerance_{ kDefaultRelativeTolerance };

        std::uint64_t call_count_{ 0 };
        std::uint64_t non_converged_count_{ 0 };
        std::uint32_t maximum_iterations_seen_{ 0 };
    };
}
