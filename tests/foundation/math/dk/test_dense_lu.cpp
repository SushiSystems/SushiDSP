/**************************************************************************/
/* test_dense_lu.cpp                                                      */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_dense_lu.cpp
 * @brief Tests that the dense LU factorisation solves known systems and refuses singular ones.
 * @author Mustafa Garip
 */

#include <cmath>
#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/math/dk/dense_lu.hpp>

#include <array>

namespace
{
    /// @brief Holds a square system A x = b of order three for the residual check.
    struct System3
    {
            const char* name;
            std::array<std::array<double, 3>, 3> matrix;
            std::array<double, 3> right_hand_side;
    };

    /// @brief Holds a tridiagonal system with the solution [1, 2, 3], and the hand-solved system
    /// A = [[2, 1, -1], [-3, -1, 2], [-2, 1, 2]], b = [8, -11, -3] with the solution [2, 3, -1] (det A = -1).
    const System3 kSystems[] = {
        {"Tridiagonal", {{{4.0, 1.0, 0.0}, {1.0, 3.0, 1.0}, {0.0, 1.0, 2.0}}}, {6.0, 10.0, 8.0}},
        {"HandChecked", {{{2.0, 1.0, -1.0}, {-3.0, -1.0, 2.0}, {-2.0, 1.0, 2.0}}}, {8.0, -11.0, -3.0}},
    };

    /// @brief Checks that A x reproduces b in the first @p order rows within the LU backward-error bound.
    ///
    /// Partial pivoting on a matrix this small gives (A + dA) x = b with |dA| <= n u |A| up to a growth factor
    /// the threshold of 16 absorbs (Higham, Accuracy and Stability, section 9.3), so each row of A x differs from
    /// b by at most n u sum |a x|.
    template <std::size_t N>
    ::testing::AssertionResult reproduces(const std::array<std::array<double, N>, N>& matrix,
                                          const std::array<double, N>& right_hand_side,
                                          const std::array<double, N>& solution, std::size_t order)
    {
        for (std::size_t row = 0; row < order; ++row)
        {
            double product = 0.0;
            double magnitude = 0.0;
            for (std::size_t column = 0; column < order; ++column)
            {
                product += matrix[row][column] * solution[column];
                magnitude += std::fabs(matrix[row][column] * solution[column]);
            }
            const TestSupport::ErrorRatio measured =
                TestSupport::measure_sum_error<double>(product, right_hand_side[row], magnitude, order);
            if (!measured.is_within())
            {
                return ::testing::AssertionFailure() << "row " << row << ": " << measured.describe();
            }
        }
        return ::testing::AssertionSuccess();
    }

    std::string system_name(const ::testing::TestParamInfo<System3>& info)
    {
        return std::string(info.param.name);
    }
} // namespace

class Small_DenseLuSolve : public ::testing::TestWithParam<System3>
{
};

TEST_P(Small_DenseLuSolve, ReproducesTheRightHandSideWithinTheBackwardErrorBound)
{
    SushiDSP::dk::DenseLu<3> lu;
    ASSERT_TRUE(lu.factorise(GetParam().matrix));
    std::array<double, 3> solution{};
    lu.solve(GetParam().right_hand_side, solution);
    EXPECT_TRUE(reproduces(GetParam().matrix, GetParam().right_hand_side, solution, 3));
}

INSTANTIATE_TEST_SUITE_P(Systems, Small_DenseLuSolve, ::testing::ValuesIn(kSystems), system_name);

// Without partial pivoting the first step divides by zero. See tests/foundation/math/dk/README.md.
TEST(Small_DenseLu, PivotsWhenTheLeadingEntryIsZero)
{
    const std::array<std::array<double, 2>, 2> matrix{{{0.0, 1.0}, {1.0, 0.0}}};
    SushiDSP::dk::DenseLu<2> lu;
    ASSERT_TRUE(lu.factorise(matrix));
    std::array<double, 2> solution{};
    lu.solve(std::array<double, 2>{{3.0, 5.0}}, solution);
    EXPECT_EQ(solution[0], 5.0);
    EXPECT_EQ(solution[1], 3.0);
}

TEST(Small_DenseLu, RejectsASingularMatrix)
{
    const std::array<std::array<double, 2>, 2> singular{{{1.0, 2.0}, {2.0, 4.0}}};
    SushiDSP::dk::DenseLu<2> lu;
    EXPECT_FALSE(lu.factorise(singular));
}

TEST(Small_DenseLu, OneByOneSolvesByDivision)
{
    const std::array<std::array<double, 1>, 1> matrix{{{4.0}}};
    SushiDSP::dk::DenseLu<1> lu;
    ASSERT_TRUE(lu.factorise(matrix));
    std::array<double, 1> solution{};
    lu.solve(std::array<double, 1>{{6.0}}, solution);
    EXPECT_EQ(solution[0], 1.5);
}

TEST(Small_DenseLu, OneByOneRefusesAZeroEntry)
{
    const std::array<std::array<double, 1>, 1> matrix{{{0.0}}};
    SushiDSP::dk::DenseLu<1> lu;
    EXPECT_FALSE(lu.factorise(matrix));
}

// Only the leading 3x3 block may be touched. See tests/foundation/math/dk/README.md.
TEST(Small_DenseLu, SolvesTheLeadingBlockOfAnOversizedArray)
{
    std::array<std::array<double, 5>, 5> matrix{};
    std::array<double, 5> right_hand_side{};
    for (std::size_t row = 0; row < 3; ++row)
    {
        for (std::size_t column = 0; column < 3; ++column)
        {
            matrix[row][column] = kSystems[1].matrix[row][column];
        }
        right_hand_side[row] = kSystems[1].right_hand_side[row];
    }
    SushiDSP::dk::DenseLu<5> lu;
    ASSERT_TRUE(lu.factorise(matrix, 3));
    std::array<double, 5> solution{};
    lu.solve(right_hand_side, solution, 3);
    EXPECT_TRUE(reproduces(matrix, right_hand_side, solution, 3));
}

TEST(Small_DenseLu, LeavesTheEntriesBeyondTheLeadingBlockUntouched)
{
    std::array<std::array<double, 5>, 5> matrix{};
    matrix[0][0] = 2.0;
    matrix[1][1] = 4.0;
    matrix[2][2] = 8.0;
    SushiDSP::dk::DenseLu<5> lu;
    ASSERT_TRUE(lu.factorise(matrix, 3));
    std::array<double, 5> solution{0.0, 0.0, 0.0, 11.0, 13.0};
    lu.solve(std::array<double, 5>{{2.0, 4.0, 8.0, 99.0, 99.0}}, solution, 3);
    EXPECT_EQ(solution[3], 11.0);
    EXPECT_EQ(solution[4], 13.0);
}

TEST(Small_DenseLu, RefusesASingularLeadingBlock)
{
    std::array<std::array<double, 4>, 4> matrix{};
    matrix[0][0] = 1.0;
    matrix[0][1] = 2.0;
    matrix[1][0] = 2.0;
    matrix[1][1] = 4.0;
    matrix[2][2] = 1.0;
    matrix[3][3] = 1.0;
    SushiDSP::dk::DenseLu<4> lu;
    EXPECT_FALSE(lu.factorise(matrix, 2));
}

TEST(Small_DenseLu, AcceptsTheNonSingularLeadingBlockOfASingularMatrix)
{
    std::array<std::array<double, 4>, 4> matrix{};
    matrix[0][0] = 1.0;
    matrix[0][1] = 2.0;
    matrix[1][0] = 2.0;
    matrix[1][1] = 4.0;
    matrix[2][2] = 1.0;
    matrix[3][3] = 1.0;
    SushiDSP::dk::DenseLu<4> lu;
    EXPECT_TRUE(lu.factorise(matrix, 1));
}

TEST(Small_DenseLu, TreatsAnEmptyLeadingBlockAsANoOperation)
{
    const std::array<std::array<double, 2>, 2> matrix{{{1.0, 0.0}, {0.0, 1.0}}};
    SushiDSP::dk::DenseLu<2> lu;
    EXPECT_TRUE(lu.factorise(matrix, 0));
    std::array<double, 2> solution{7.0, 9.0};
    lu.solve(std::array<double, 2>{{1.0, 1.0}}, solution, 0);
    EXPECT_EQ(solution[0], 7.0);
    EXPECT_EQ(solution[1], 9.0);
}

// [[4, 3], [6, 3]] x = [10, 12] has the answer x = [1, 2]. See tests/foundation/math/dk/README.md.
TEST(Small_DenseLu, SolvesATwoByTwoSystemThatNeedsARowSwap)
{
    const std::array<std::array<double, 2>, 2> matrix{{{4.0, 3.0}, {6.0, 3.0}}};
    const std::array<double, 2> right_hand_side{{10.0, 12.0}};
    SushiDSP::dk::DenseLu<2> lu;
    ASSERT_TRUE(lu.factorise(matrix));
    std::array<double, 2> solution{};
    lu.solve(right_hand_side, solution);
    EXPECT_TRUE(reproduces(matrix, right_hand_side, solution, 2));
}

TEST(Small_DenseLu, SolvesTheLeadingOneByOneBlockOfATwoByTwoArray)
{
    const std::array<std::array<double, 2>, 2> matrix{{{5.0, 99.0}, {99.0, 99.0}}};
    SushiDSP::dk::DenseLu<2> lu;
    ASSERT_TRUE(lu.factorise(matrix, 1));
    std::array<double, 2> solution{};
    lu.solve(std::array<double, 2>{{10.0, 0.0}}, solution, 1);
    EXPECT_EQ(solution[0], 2.0);
}
