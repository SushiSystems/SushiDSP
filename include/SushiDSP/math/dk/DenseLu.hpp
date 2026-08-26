/**************************************************************************/
/* DenseLu.hpp                                                            */
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

namespace SushiDSP::dk
{
    /// @brief Fixed-size LU factorisation with partial pivoting for a dense N x N system.
    ///
    /// All storage is member std::array, so factorise() and solve() are allocation-free and
    /// usable on the audio thread. The factors persist after factorise(), so one factorisation
    /// can serve several right-hand sides; DkCircuit uses that when it inverts the augmented
    /// nodal matrix column by column. The size-aware overloads let a caller hold one
    /// compile-time-maximum array and factorise only the leading order x order block it
    /// actually filled.
    /// @tparam N System dimension.
    template <std::size_t N>
    class DenseLu
    {
    public:
        /// @brief Smallest pivot magnitude accepted as non-singular.
        static constexpr double kMinimumPivot = 1e-300;

        /// @brief Factorises the matrix into the internal L/U storage.
        /// @param matrix The system matrix; not modified.
        /// @return False when a pivot is numerically zero; solve() must not be called after that.
        bool factorise(const std::array<std::array<double, N>, N>& matrix)
        {
            return factorise(matrix, N);
        }

        /// @brief Factorises only the leading order x order block of the matrix.
        /// @param matrix The system matrix; only its leading block is read, and it is not modified.
        /// @param order Active dimension, at most N. An order of zero always succeeds.
        /// @return False when a pivot is numerically zero; solve() must not be called after that.
        bool factorise(const std::array<std::array<double, N>, N>& matrix, std::size_t order)
        {
            for (std::size_t row = 0; row < order; ++row)
            {
                for (std::size_t column = 0; column < order; ++column)
                {
                    factors_[row][column] = matrix[row][column];
                }
            }

            for (std::size_t step = 0; step < order; ++step)
            {
                std::size_t pivot_row = step;
                double      pivot_magnitude = std::fabs(factors_[step][step]);
                for (std::size_t row = step + 1; row < order; ++row)
                {
                    const double magnitude = std::fabs(factors_[row][step]);
                    if (magnitude > pivot_magnitude)
                    {
                        pivot_magnitude = magnitude;
                        pivot_row = row;
                    }
                }

                if (!(pivot_magnitude >= kMinimumPivot))
                {
                    return false;
                }

                pivots_[step] = pivot_row;
                if (pivot_row != step)
                {
                    for (std::size_t column = 0; column < order; ++column)
                    {
                        const double swapped = factors_[step][column];
                        factors_[step][column] = factors_[pivot_row][column];
                        factors_[pivot_row][column] = swapped;
                    }
                }

                const double pivot = factors_[step][step];
                for (std::size_t row = step + 1; row < order; ++row)
                {
                    const double multiplier = factors_[row][step] / pivot;
                    factors_[row][step] = multiplier;
                    for (std::size_t column = step + 1; column < order; ++column)
                    {
                        factors_[row][column] -= multiplier * factors_[step][column];
                    }
                }
            }
            return true;
        }

        /// @brief Solves A x = b using the factors from the last successful factorise().
        /// @param right_hand_side b.
        /// @param solution x, overwritten.
        void solve(const std::array<double, N>& right_hand_side, std::array<double, N>& solution) const
        {
            solve(right_hand_side, solution, N);
        }

        /// @brief Solves the leading order x order block using the factors from the last
        /// successful factorise() of the same order.
        /// @param right_hand_side b; only its leading order entries are read.
        /// @param solution x; only its leading order entries are written.
        /// @param order Active dimension, at most N and matching the last factorise().
        void solve(const std::array<double, N>& right_hand_side, std::array<double, N>& solution,
                   std::size_t order) const
        {
            for (std::size_t row = 0; row < order; ++row)
            {
                solution[row] = right_hand_side[row];
            }

            for (std::size_t step = 0; step < order; ++step)
            {
                const std::size_t pivot_row = pivots_[step];
                if (pivot_row != step)
                {
                    const double swapped = solution[step];
                    solution[step] = solution[pivot_row];
                    solution[pivot_row] = swapped;
                }
            }

            for (std::size_t step = 0; step < order; ++step)
            {
                for (std::size_t row = step + 1; row < order; ++row)
                {
                    solution[row] -= factors_[row][step] * solution[step];
                }
            }

            for (std::size_t reverse = order; reverse > 0; --reverse)
            {
                const std::size_t row = reverse - 1;
                double            accumulator = solution[row];
                for (std::size_t column = row + 1; column < order; ++column)
                {
                    accumulator -= factors_[row][column] * solution[column];
                }
                solution[row] = accumulator / factors_[row][row];
            }
        }

    private:
        std::array<std::array<double, N>, N> factors_{};
        std::array<std::size_t, N>           pivots_{};
    };

    /// @brief One-dimensional LU, which is a single division; the nonlinear diode fast path
    /// pays nothing for the general machinery.
    template <>
    class DenseLu<1>
    {
    public:
        /// @brief Smallest pivot magnitude accepted as non-singular.
        static constexpr double kMinimumPivot = 1e-300;

        /// @brief Stores the single matrix entry as the pivot.
        /// @param matrix The 1x1 system matrix.
        /// @return False when the entry is numerically zero.
        bool factorise(const std::array<std::array<double, 1>, 1>& matrix)
        {
            if (!(std::fabs(matrix[0][0]) >= kMinimumPivot))
            {
                return false;
            }
            pivot_ = matrix[0][0];
            return true;
        }

        /// @brief Stores the single matrix entry, or succeeds trivially at order zero.
        /// @param matrix The 1x1 system matrix.
        /// @param order Active dimension, either zero or one.
        /// @return False when the entry is numerically zero at order one.
        bool factorise(const std::array<std::array<double, 1>, 1>& matrix, std::size_t order)
        {
            if (order == 0)
            {
                return true;
            }
            return factorise(matrix);
        }

        /// @brief Divides by the factorised entry.
        /// @param right_hand_side b.
        /// @param solution x, overwritten.
        void solve(const std::array<double, 1>& right_hand_side, std::array<double, 1>& solution) const
        {
            solution[0] = right_hand_side[0] / pivot_;
        }

        /// @brief Divides by the factorised entry, or leaves the solution alone at order zero.
        /// @param right_hand_side b.
        /// @param solution x, overwritten at order one.
        /// @param order Active dimension, either zero or one.
        void solve(const std::array<double, 1>& right_hand_side, std::array<double, 1>& solution,
                   std::size_t order) const
        {
            if (order != 0)
            {
                solve(right_hand_side, solution);
            }
        }

    private:
        double pivot_{ 0.0 };
    };
}
