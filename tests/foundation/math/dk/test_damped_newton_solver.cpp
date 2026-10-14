/**************************************************************************/
/* test_damped_newton_solver.cpp                                          */
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
 * @file test_damped_newton_solver.cpp
 * @brief Tests DenseLu and DampedNewtonSolver on hand-checked systems.
 * @author Mustafa Garip
 */

#include <array>
#include <cmath>
#include <cstdint>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/math/dk/damped_newton_solver.hpp>
#include <SushiDSP/foundation/math/dk/dense_lu.hpp>

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

TEST(Unit_Dsp, DenseLuSolvesAHandCheckedThreeByThreeSystem)
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

TEST(Unit_Dsp, DenseLuPivotsWhenTheLeadingEntryIsZero)
{
    // Without partial pivoting the first step divides by zero. See tests/functional/unit/README.md.
    std::array<std::array<double, 2>, 2> matrix{{{0.0, 1.0}, {1.0, 0.0}}};
    SushiDSP::dk::DenseLu<2> lu;
    ASSERT_TRUE(lu.factorise(matrix));
    std::array<double, 2> solution{};
    lu.solve({3.0, 5.0}, solution);
    EXPECT_NEAR(solution[0], 5.0, 1e-12);
    EXPECT_NEAR(solution[1], 3.0, 1e-12);
}

TEST(Unit_Dsp, DenseLuReportsASingularMatrix)
{
    std::array<std::array<double, 2>, 2> matrix{{{1.0, 2.0}, {2.0, 4.0}}};
    SushiDSP::dk::DenseLu<2> lu;
    EXPECT_FALSE(lu.factorise(matrix));
}

TEST(Unit_Dsp, DenseLuOneByOneSolvesByDivision)
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

TEST(Unit_Dsp, DenseLuSolvesTheLeadingBlockOfAnOversizedArray)
{
    // Only the leading 3x3 block may be touched. See tests/functional/unit/README.md.
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

TEST(Unit_Dsp, DenseLuReportsASingularLeadingBlock)
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

TEST(Unit_Dsp, DenseLuTreatsAnEmptyLeadingBlockAsANoOperation)
{
    const std::array<std::array<double, 2>, 2> matrix{{{1.0, 0.0}, {0.0, 1.0}}};
    SushiDSP::dk::DenseLu<2> lu;
    EXPECT_TRUE(lu.factorise(matrix, 0));
    std::array<double, 2> solution{7.0, 9.0};
    lu.solve({1.0, 1.0}, solution, 0);
    EXPECT_NEAR(solution[0], 7.0, 1e-12);
    EXPECT_NEAR(solution[1], 9.0, 1e-12);
}

namespace
{
    /// @brief r(v) = [v0^2 + v1^2 - 4, v0 - v1]; the root in the first quadrant is
    /// v0 = v1 = sqrt(2) = 1.41421356237309504880.
    struct CircleAndLine
    {
            /// @brief Evaluates the residual and its Jacobian at v.
            /// @param v The iterate.
            /// @param r The residual, overwritten.
            /// @param jacobian The Jacobian, overwritten.
            void residual_and_jacobian(const std::array<double, 2>& v, std::array<double, 2>& r,
                                       std::array<std::array<double, 2>, 2>& jacobian) const
            {
                r[0] = v[0] * v[0] + v[1] * v[1] - 4.0;
                r[1] = v[0] - v[1];
                jacobian[0][0] = 2.0 * v[0];
                jacobian[0][1] = 2.0 * v[1];
                jacobian[1][0] = 1.0;
                jacobian[1][1] = -1.0;
            }
    };

    /// @brief r(v) = exp(v) - 1 in one dimension, whose root is v = 0. From v = 12 the initial
    /// residual is about 1.6e5 while each Newton step moves v by barely more than one, so this
    /// is the long-slow-approach case: it needs many iterations, not damping.
    struct SteepExponential
    {
            /// @brief Evaluates the residual and its Jacobian at v.
            /// @param v The iterate.
            /// @param r The residual, overwritten.
            /// @param jacobian The Jacobian, overwritten.
            void residual_and_jacobian(const std::array<double, 1>& v, std::array<double, 1>& r,
                                       std::array<std::array<double, 1>, 1>& jacobian) const
            {
                r[0] = std::exp(v[0]) - 1.0;
                jacobian[0][0] = std::exp(v[0]);
            }
    };

    /// @brief r(v) = atan(v), root v = 0, the textbook Newton divergence case: the undamped step
    /// -atan(v)*(1 + v^2) overshoots past the root to a strictly larger |atan| for any
    /// |v| > 1.3917, so from v = 2 undamped Newton runs away to infinity. Halving the first step
    /// (v = 2 - 5.536 -> 2 - 2.768 = -0.768) lands back inside the basin.
    struct ArcTangent
    {
            /// @brief Evaluates the residual and its Jacobian at v.
            /// @param v The iterate.
            /// @param r The residual, overwritten.
            /// @param jacobian The Jacobian, overwritten.
            void residual_and_jacobian(const std::array<double, 1>& v, std::array<double, 1>& r,
                                       std::array<std::array<double, 1>, 1>& jacobian) const
            {
                r[0] = std::atan(v[0]);
                jacobian[0][0] = 1.0 / (1.0 + v[0] * v[0]);
            }
    };
} // namespace

TEST(Unit_Dsp, DampedNewtonFindsTheIntersectionOfACircleAndALine)
{
    SushiDSP::dk::DampedNewtonSolver<2> solver;
    std::array<double, 2> v{1.0, 1.0};
    EXPECT_TRUE(solver.solve(CircleAndLine{}, v));
    EXPECT_NEAR(v[0], std::sqrt(2.0), 1e-10);
    EXPECT_NEAR(v[1], std::sqrt(2.0), 1e-10);
    EXPECT_EQ(solver.non_converged_count(), 0u);
    EXPECT_EQ(solver.call_count(), 1u);
    EXPECT_LE(solver.maximum_iterations_seen(), 8u);
}

TEST(Unit_Dsp, DampedNewtonConvergesWhereUndampedNewtonDiverges)
{
    SushiDSP::dk::DampedNewtonSolver<1> solver;
    std::array<double, 1> v{2.0};
    solver.set_maximum_iterations(20);
    EXPECT_TRUE(solver.solve(ArcTangent{}, v));
    EXPECT_NEAR(v[0], 0.0, 1e-9);
    EXPECT_EQ(solver.non_converged_count(), 0u);
}

TEST(Unit_Dsp, DampedNewtonWithHalvingDisabledFailsOnTheDivergentStart)
{
    // With no halving, every iteration takes the full step. See tests/functional/unit/README.md.
    SushiDSP::dk::DampedNewtonSolver<1> solver;
    solver.set_maximum_halvings(0);
    solver.set_maximum_iterations(20);
    std::array<double, 1> v{2.0};
    EXPECT_FALSE(solver.solve(ArcTangent{}, v));
    EXPECT_EQ(solver.non_converged_count(), 1u);
    EXPECT_TRUE(std::isfinite(v[0]));
}

TEST(Unit_Dsp, DampedNewtonSurvivesASteepStartWithALargeInitialResidual)
{
    SushiDSP::dk::DampedNewtonSolver<1> solver;
    std::array<double, 1> v{12.0};
    solver.set_maximum_iterations(60);
    EXPECT_TRUE(solver.solve(SteepExponential{}, v));
    EXPECT_NEAR(v[0], 0.0, 1e-9);
    EXPECT_EQ(solver.non_converged_count(), 0u);
}

TEST(Unit_Dsp, DampedNewtonCountsANonConvergedCallAndLeavesTheIterateFinite)
{
    SushiDSP::dk::DampedNewtonSolver<1> solver;
    solver.set_maximum_iterations(1);
    std::array<double, 1> v{12.0};
    EXPECT_FALSE(solver.solve(SteepExponential{}, v));
    EXPECT_EQ(solver.non_converged_count(), 1u);
    EXPECT_TRUE(std::isfinite(v[0]));
    solver.reset_statistics();
    EXPECT_EQ(solver.non_converged_count(), 0u);
    EXPECT_EQ(solver.call_count(), 0u);
}

TEST(Unit_Dsp, DampedNewtonWarmStartConvergesInOneIterationOnASmoothTrack)
{
    // A warm start needs at most a couple of iterations. See tests/functional/unit/README.md.
    SushiDSP::dk::DampedNewtonSolver<1> solver;
    std::array<double, 1> v{0.0};
    for (int step = 0; step < 100; ++step)
    {
        const double target = 0.001 * static_cast<double>(step);
        /// @brief r(v) = exp(v) - exp(target), whose root is v = target.
        struct Shifted
        {
                double target;

                /// @brief Evaluates the residual and its Jacobian at value.
                /// @param value The iterate.
                /// @param r The residual, overwritten.
                /// @param jacobian The Jacobian, overwritten.
                void residual_and_jacobian(const std::array<double, 1>& value,
                                           std::array<double, 1>& r,
                                           std::array<std::array<double, 1>, 1>& jacobian) const
                {
                    r[0] = std::exp(value[0]) - std::exp(target);
                    jacobian[0][0] = std::exp(value[0]);
                }
        };
        ASSERT_TRUE(solver.solve(Shifted{target}, v));
        EXPECT_NEAR(v[0], target, 1e-9);
    }
    EXPECT_LE(solver.maximum_iterations_seen(), 3u);
}

TEST(Unit_Dsp, DampedNewtonReturnsTheBestIterateWhenItFailsToConverge)
{
    // A failed solve returns its lowest-residual iterate. See tests/functional/unit/README.md.
    SushiDSP::dk::DampedNewtonSolver<1> solver;
    solver.set_maximum_halvings(0);
    solver.set_maximum_iterations(1);

    const std::array<double, 1> start{2.0};
    std::array<double, 1> v = start;
    EXPECT_FALSE(solver.solve(ArcTangent{}, v));

    std::array<double, 1> r{};
    std::array<std::array<double, 1>, 1> jacobian{};
    ArcTangent system;
    system.residual_and_jacobian(start, r, jacobian);
    const double starting_norm = std::fabs(r[0]);
    system.residual_and_jacobian(v, r, jacobian);
    EXPECT_LE(std::fabs(r[0]), starting_norm);
}

TEST(Unit_Dsp, DenseLuTwoByTwoSolvesTheSameSystemsAsTheGeneralTemplate)
{
    // [[4, 3], [6, 3]] x = [10, 12] has the answer x = [1, 2]. See tests/functional/unit/README.md.
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

namespace
{
    /// @brief Records the point of the last residual_and_jacobian() call, to pin the contract
    /// DkCircuit::process_sample relies on: on success the solver's last model evaluation was at
    /// the iterate it returns, so the caller may reuse whatever that evaluation cached.
    struct RecordingSystem
    {
            mutable std::array<double, 1> last_evaluated{0.0};

            void residual_and_jacobian(const std::array<double, 1>& v, std::array<double, 1>& r,
                                       std::array<std::array<double, 1>, 1>& jacobian) const
            {
                last_evaluated = v;
                r[0] = std::exp(v[0]) - std::exp(0.25);
                jacobian[0][0] = std::exp(v[0]);
            }
    };
} // namespace

TEST(Unit_Dsp, DampedNewtonLeavesItsLastEvaluationAtTheIterateItReturns)
{
    SushiDSP::dk::DampedNewtonSolver<1> solver;
    // The default budget of 8 iterations is too tight here. See tests/functional/unit/README.md.
    solver.set_maximum_iterations(32);
    for (const double start : {0.0, 5.0, -5.0, 12.0, 0.2499999})
    {
        RecordingSystem system;
        std::array<double, 1> v{start};
        ASSERT_TRUE(solver.solve(system, v)) << start;
        EXPECT_DOUBLE_EQ(system.last_evaluated[0], v[0]) << start;
        EXPECT_NEAR(v[0], 0.25, 1e-9) << start;
    }
}
