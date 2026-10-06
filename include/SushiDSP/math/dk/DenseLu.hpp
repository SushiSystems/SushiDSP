/**************************************************************************/
/* DenseLu.hpp                                                            */
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
 * @file DenseLu.hpp
 * @brief Defines fixed-size LU factorisation with partial pivoting.
 * @author Mustafa Garip
 */

#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace SushiDSP::dk
{
    /// @brief Factorises a dense N x N system by LU with partial pivoting, in fixed storage.
    ///
    /// factorise() and solve() allocate nothing, and the factors persist after factorise(), so
    /// one factorisation serves several right-hand sides. The size-aware overloads work on the
    /// leading order x order block of a compile-time-maximum array (include/SushiDSP/README.md).
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

    /// @brief Closed-form 2x2 LU with partial pivoting; N = 2 is the shape of every triode block
    /// and of the D6c clip stage's diode pair, so it is the hot Jacobian size in the tree.
    template <>
    class DenseLu<2>
    {
    public:
        /// @brief Smallest pivot magnitude accepted as non-singular.
        static constexpr double kMinimumPivot = 1e-300;

        /// @brief Factorises the 2x2 matrix into the internal L/U storage.
        /// @param matrix The system matrix; not modified.
        /// @return False when a pivot is numerically zero; solve() must not be called after that.
        bool factorise(const std::array<std::array<double, 2>, 2>& matrix)
        {
            return factorise(matrix, 2);
        }

        /// @brief Factorises the leading order x order block of the 2x2 matrix.
        /// @param matrix The system matrix; only its leading block is read, and it is not modified.
        /// @param order Active dimension, 0, 1, or 2. An order of zero always succeeds.
        /// @return False when a pivot is numerically zero; solve() must not be called after that.
        bool factorise(const std::array<std::array<double, 2>, 2>& matrix, std::size_t order)
        {
            if (order == 0)
            {
                return true;
            }
            if (order == 1)
            {
                if (!(std::fabs(matrix[0][0]) >= kMinimumPivot))
                {
                    return false;
                }
                pivot_ = matrix[0][0];
                return true;
            }

            pivot_row_zero_ = std::fabs(matrix[0][0]) >= std::fabs(matrix[1][0]);
            const double pivot = pivot_row_zero_ ? matrix[0][0] : matrix[1][0];
            if (!(std::fabs(pivot) >= kMinimumPivot))
            {
                return false;
            }

            const double other = pivot_row_zero_ ? matrix[1][0] : matrix[0][0];
            const double pivot_column_1 = pivot_row_zero_ ? matrix[0][1] : matrix[1][1];
            const double other_column_1 = pivot_row_zero_ ? matrix[1][1] : matrix[0][1];

            const double multiplier = other / pivot;
            const double schur = other_column_1 - multiplier * pivot_column_1;
            if (!(std::fabs(schur) >= kMinimumPivot))
            {
                return false;
            }

            pivot_ = pivot;
            pivot_column_1_ = pivot_column_1;
            multiplier_ = multiplier;
            schur_ = schur;
            return true;
        }

        /// @brief Solves the 2x2 system A x = b using the factors from the last factorise().
        /// @param right_hand_side b.
        /// @param solution x, overwritten.
        void solve(const std::array<double, 2>& right_hand_side, std::array<double, 2>& solution) const
        {
            solve(right_hand_side, solution, 2);
        }

        /// @brief Solves the leading order x order block using the factors from the last
        /// factorise() of the same order.
        /// @param right_hand_side b; only its leading order entries are read.
        /// @param solution x; only its leading order entries are written.
        /// @param order Active dimension, 0, 1, or 2, matching the last factorise().
        void solve(const std::array<double, 2>& right_hand_side, std::array<double, 2>& solution,
                   std::size_t order) const
        {
            if (order == 0)
            {
                return;
            }
            if (order == 1)
            {
                solution[0] = right_hand_side[0] / pivot_;
                return;
            }

            const double pivot_entry = pivot_row_zero_ ? right_hand_side[0] : right_hand_side[1];
            const double other_entry = pivot_row_zero_ ? right_hand_side[1] : right_hand_side[0];

            const double y_1 = other_entry - multiplier_ * pivot_entry;
            const double x_1 = y_1 / schur_;
            const double x_0 = (pivot_entry - pivot_column_1_ * x_1) / pivot_;

            solution[0] = x_0;
            solution[1] = x_1;
        }

    private:
        double pivot_{ 0.0 };
        double pivot_column_1_{ 0.0 };
        double multiplier_{ 0.0 };
        double schur_{ 0.0 };
        bool   pivot_row_zero_{ true };
    };
}
