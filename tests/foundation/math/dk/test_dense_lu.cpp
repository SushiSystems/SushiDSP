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

#include <gtest/gtest.h>

#include <SushiDSP/foundation/math/dk/dense_lu.hpp>

#include <array>

TEST(Small_DenseLu, SolvesAKnownThreeByThreeSystem)
{
    const std::array<std::array<double, 3>, 3> matrix{{{4.0, 1.0, 0.0}, {1.0, 3.0, 1.0}, {0.0, 1.0, 2.0}}};
    const std::array<double, 3> expected{{1.0, 2.0, 3.0}};
    const std::array<double, 3> right_hand_side{{6.0, 10.0, 8.0}};

    SushiDSP::dk::DenseLu<3> lu;
    ASSERT_TRUE(lu.factorise(matrix));
    std::array<double, 3> solution{};
    lu.solve(right_hand_side, solution);
    for (std::size_t i = 0; i < 3; ++i)
    {
        EXPECT_NEAR(solution[i], expected[i], 1e-9);
    }
}

TEST(Small_DenseLu, PivotsWhenTheLeadingEntryIsZero)
{
    const std::array<std::array<double, 2>, 2> matrix{{{0.0, 1.0}, {1.0, 0.0}}};
    SushiDSP::dk::DenseLu<2> lu;
    ASSERT_TRUE(lu.factorise(matrix));
    std::array<double, 2> solution{};
    lu.solve(std::array<double, 2>{{3.0, 5.0}}, solution);
    EXPECT_NEAR(solution[0], 5.0, 1e-12);
    EXPECT_NEAR(solution[1], 3.0, 1e-12);
}

TEST(Small_DenseLu, RejectsASingularMatrix)
{
    const std::array<std::array<double, 2>, 2> singular{{{1.0, 2.0}, {2.0, 4.0}}};
    SushiDSP::dk::DenseLu<2> lu;
    EXPECT_FALSE(lu.factorise(singular));
}

namespace
{
    /// @brief The 3x3 system used for the LU golden vectors, and its hand-solved answer.
    ///
    /// A = [[2, 1, -1], [-3, -1, 2], [-2, 1, 2]], b = [8, -11, -3].
    /// Gaussian elimination by hand gives x = [2, 3, -1] exactly:
    ///   2*2 + 1*3 - 1*(-1)  =  4 + 3 + 1 =  8
    ///  -3*2 - 1*3 + 2*(-1)  = -6 - 3 - 2 = -11
    ///  -2*2 + 1*3 + 2*(-1)  = -4 + 3 - 2 = -3
    /// det(A) = 2(-1*2 - 2*1) - 1(-3*2 - 2*(-2)) + (-1)(-3*1 - (-1)(-2)) = -8 + 2 + 5 = -1.
    constexpr double K_MATRIX[3][3] = {{2.0, 1.0, -1.0}, {-3.0, -1.0, 2.0}, {-2.0, 1.0, 2.0}};
} // namespace

TEST(Small_DenseLu, DenseLuSolvesAHandCheckedThreeByThreeSystem)
{
    std::array<std::array<double, 3>, 3> matrix{};
    for (std::size_t row = 0; row < 3; ++row)
    {
        for (std::size_t column = 0; column < 3; ++column)
        {
            matrix[row][column] = K_MATRIX[row][column];
        }
    }
    SushiDSP::dk::DenseLu<3> lu;
    ASSERT_TRUE(lu.factorise(matrix));
    std::array<double, 3> solution{};
    lu.solve({8.0, -11.0, -3.0}, solution);
    EXPECT_NEAR(solution[0], 2.0, 1e-12);
    EXPECT_NEAR(solution[1], 3.0, 1e-12);
    EXPECT_NEAR(solution[2], -1.0, 1e-12);
}

TEST(Small_DenseLu, DenseLuPivotsWhenTheLeadingEntryIsZero)
{
    // Without partial pivoting the first step divides by zero. See tests/foundation/math/dk/README.md.
    std::array<std::array<double, 2>, 2> matrix{{{0.0, 1.0}, {1.0, 0.0}}};
    SushiDSP::dk::DenseLu<2> lu;
    ASSERT_TRUE(lu.factorise(matrix));
    std::array<double, 2> solution{};
    lu.solve({3.0, 5.0}, solution);
    EXPECT_NEAR(solution[0], 5.0, 1e-12);
    EXPECT_NEAR(solution[1], 3.0, 1e-12);
}

TEST(Small_DenseLu, DenseLuReportsASingularMatrix)
{
    std::array<std::array<double, 2>, 2> matrix{{{1.0, 2.0}, {2.0, 4.0}}};
    SushiDSP::dk::DenseLu<2> lu;
    EXPECT_FALSE(lu.factorise(matrix));
}

TEST(Small_DenseLu, DenseLuOneByOneSolvesByDivision)
{
    std::array<std::array<double, 1>, 1> matrix{{{4.0}}};
    SushiDSP::dk::DenseLu<1> lu;
    ASSERT_TRUE(lu.factorise(matrix));
    std::array<double, 1> solution{};
    lu.solve({6.0}, solution);
    EXPECT_NEAR(solution[0], 1.5, 1e-12);
    matrix[0][0] = 0.0;
    EXPECT_FALSE(lu.factorise(matrix));
}

TEST(Small_DenseLu, DenseLuSolvesTheLeadingBlockOfAnOversizedArray)
{
    // Only the leading 3x3 block may be touched. See tests/foundation/math/dk/README.md.
    std::array<std::array<double, 5>, 5> matrix{};
    for (std::size_t row = 0; row < 3; ++row)
    {
        for (std::size_t column = 0; column < 3; ++column)
        {
            matrix[row][column] = K_MATRIX[row][column];
        }
    }
    SushiDSP::dk::DenseLu<5> lu;
    ASSERT_TRUE(lu.factorise(matrix, 3));
    const std::array<double, 5> right_hand_side{8.0, -11.0, -3.0, 0.0, 0.0};
    std::array<double, 5> solution{};
    lu.solve(right_hand_side, solution, 3);
    EXPECT_NEAR(solution[0], 2.0, 1e-12);
    EXPECT_NEAR(solution[1], 3.0, 1e-12);
    EXPECT_NEAR(solution[2], -1.0, 1e-12);
}

TEST(Small_DenseLu, DenseLuReportsASingularLeadingBlock)
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
    EXPECT_TRUE(lu.factorise(matrix, 1));
}

TEST(Small_DenseLu, DenseLuTreatsAnEmptyLeadingBlockAsANoOperation)
{
    const std::array<std::array<double, 2>, 2> matrix{{{1.0, 0.0}, {0.0, 1.0}}};
    SushiDSP::dk::DenseLu<2> lu;
    EXPECT_TRUE(lu.factorise(matrix, 0));
    std::array<double, 2> solution{7.0, 9.0};
    lu.solve({1.0, 1.0}, solution, 0);
    EXPECT_NEAR(solution[0], 7.0, 1e-12);
    EXPECT_NEAR(solution[1], 9.0, 1e-12);
}

TEST(Small_DenseLu, DenseLuTwoByTwoSolvesTheSameSystemsAsTheGeneralTemplate)
{
    // [[4, 3], [6, 3]] x = [10, 12] has the answer x = [1, 2]. See tests/foundation/math/dk/README.md.
    std::array<std::array<double, 2>, 2> matrix{{{4.0, 3.0}, {6.0, 3.0}}};
    SushiDSP::dk::DenseLu<2> lu;
    ASSERT_TRUE(lu.factorise(matrix));
    std::array<double, 2> solution{};
    lu.solve({10.0, 12.0}, solution);
    EXPECT_NEAR(solution[0], 1.0, 1e-14);
    EXPECT_NEAR(solution[1], 2.0, 1e-14);

    // The pivoting case the general template is tested on: [[0, 1], [1, 0]] x = [3, 5] -> [5, 3].
    std::array<std::array<double, 2>, 2> pivoting{{{0.0, 1.0}, {1.0, 0.0}}};
    ASSERT_TRUE(lu.factorise(pivoting));
    lu.solve({3.0, 5.0}, solution);
    EXPECT_NEAR(solution[0], 5.0, 1e-14);
    EXPECT_NEAR(solution[1], 3.0, 1e-14);

    // Singular, and the order-aware overload restricted to the leading 1x1 block.
    std::array<std::array<double, 2>, 2> singular{{{1.0, 2.0}, {2.0, 4.0}}};
    EXPECT_FALSE(lu.factorise(singular));
    std::array<std::array<double, 2>, 2> leading{{{5.0, 99.0}, {99.0, 99.0}}};
    ASSERT_TRUE(lu.factorise(leading, 1));
    lu.solve({10.0, 0.0}, solution, 1);
    EXPECT_NEAR(solution[0], 2.0, 1e-14);
}
