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
 * @file test_damped_newton_solver.cpp
 * @brief Tests DampedNewtonSolver on hand-checked systems.
 * @author Mustafa Garip
 */

#include <array>
#include <cmath>
#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/math/dk/damped_newton_solver.hpp>

namespace
{
    /// @brief Holds the residual limit at which the solver documents that it stops.
    constexpr double K_RESIDUAL_TOLERANCE = 1e-12;

    /// @brief Holds the relative step limit the tests set on every solver.
    constexpr double K_STEP_TOLERANCE = 1e-9;

    /// @brief Returns how far a converged iterate may sit from its root.
    ///
    /// The solver stops on a residual below K_RESIDUAL_TOLERANCE, which leaves an error of at most
    /// that times the infinity norm of the inverse Jacobian, or on a step below K_STEP_TOLERANCE
    /// times (1 + |root|), and a Newton step is never smaller than the error left after it.
    /// @param root The infinity norm of the root.
    /// @param inverse_norm The infinity norm of the inverse Jacobian at the root.
    double root_bound(double root, double inverse_norm)
    {
        return inverse_norm * K_RESIDUAL_TOLERANCE + K_STEP_TOLERANCE * (1.0 + std::fabs(root));
    }

    /// @brief Applies the step tolerance the bounds above assume to a solver.
    template <std::size_t N> void configure(SushiDSP::dk::DampedNewtonSolver<N>& solver, std::uint32_t iterations)
    {
        solver.set_relative_tolerance(K_STEP_TOLERANCE);
        solver.set_maximum_iterations(iterations);
    }

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

    /// @brief Holds the infinity norm of the inverse Jacobian of CircleAndLine at its root: the Jacobian
    /// [[2 sqrt2, 2 sqrt2], [1, -1]] has determinant -4 sqrt2 and an inverse with row sums (1 + 2 sqrt2) / (4 sqrt2).
    const double K_CIRCLE_INVERSE_NORM = (1.0 + 2.0 * std::sqrt(2.0)) / (4.0 * std::sqrt(2.0));

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

    /// @brief r(v) = exp(v) - exp(target), whose root is v = target and whose slope there is exp(target).
    struct Shifted
    {
            double target;

            /// @brief Evaluates the residual and its Jacobian at value.
            /// @param value The iterate.
            /// @param r The residual, overwritten.
            /// @param jacobian The Jacobian, overwritten.
            void residual_and_jacobian(const std::array<double, 1>& value, std::array<double, 1>& r,
                                       std::array<std::array<double, 1>, 1>& jacobian) const
            {
                r[0] = std::exp(value[0]) - std::exp(target);
                jacobian[0][0] = std::exp(value[0]);
            }
    };

    /// @brief Holds a warm start: the previous root, and the root one step of a slow track later.
    struct WarmStartRow
    {
            const char* name;
            double previous;
            double target;
    };

    constexpr double K_TRACK_STEP = 0.001;

    const WarmStartRow kWarmStartRows[] = {
        {"NearZero", 0.0, K_TRACK_STEP},
        {"BelowOne", 0.5, 0.5 + K_TRACK_STEP},
        {"AtOne", 1.0, 1.0 + K_TRACK_STEP},
        {"AboveOne", 2.0, 2.0 + K_TRACK_STEP},
    };

    /// @brief Holds a start of the last-evaluation check.
    struct StartRow
    {
            const char* name;
            double start;
    };

    const StartRow kLastEvaluationStarts[] = {
        {"AtZero", 0.0}, {"Five", 5.0}, {"MinusFive", -5.0}, {"Twelve", 12.0}, {"JustBelowTheRoot", 0.2499999},
    };

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

    /// @brief A two-variable nonlinear system with a known root, for the solver trap.
    ///
    /// The pair (exp(v0) - 1 + v1/2, v0/2 + exp(v1) - 1) has its root at v0 = v1 = 0, where its
    /// Jacobian is [[1, 1/2], [1/2, 1]] and so comfortably non-singular. The exponentials make
    /// the line search do real work from a distant start rather than converging in one step.
    struct ExponentialPairSystem
    {
            /// @brief Evaluates the residual and its Jacobian at one iterate.
            /// @param v The iterate.
            /// @param residual Receives the residual.
            /// @param jacobian Receives the 2x2 Jacobian.
            void residual_and_jacobian(const std::array<double, 2>& v, std::array<double, 2>& residual,
                                       std::array<std::array<double, 2>, 2>& jacobian) const
            {
                const double first = std::exp(v[0]);
                const double second = std::exp(v[1]);
                residual[0] = first - 1.0 + 0.5 * v[1];
                residual[1] = 0.5 * v[0] + second - 1.0;
                jacobian[0][0] = first;
                jacobian[0][1] = 0.5;
                jacobian[1][0] = 0.5;
                jacobian[1][1] = second;
            }
    };

    /// @brief Holds the infinity norm of the inverse Jacobian of ExponentialPairSystem at its root: the
    /// Jacobian [[1, 1/2], [1/2, 1]] has determinant 3/4 and an inverse with row sums (1 + 1/2) / (3/4).
    constexpr double K_PAIR_INVERSE_NORM = 2.0;

    /// @brief Solves the pair system from a distant start a number of times and returns how many converged.
    /// @param solver The solver under test.
    /// @param v Receives the last iterate.
    /// @param repetitions The number of solves.
    int solve_pair_repeatedly(SushiDSP::dk::DampedNewtonSolver<2>& solver, std::array<double, 2>& v, int repetitions)
    {
        const ExponentialPairSystem system;
        int converged = 0;
        for (int solve = 0; solve < repetitions; ++solve)
        {
            v = {2.0, -1.0};
            converged += solver.solve(system, v) ? 1 : 0;
        }
        return converged;
    }

    std::string warm_start_name(const ::testing::TestParamInfo<WarmStartRow>& info)
    {
        return std::string(info.param.name);
    }

    std::string start_name(const ::testing::TestParamInfo<StartRow>& info)
    {
        return std::string(info.param.name);
    }
} // namespace

TEST(Small_DampedNewtonSolver, DampedNewtonFindsTheIntersectionOfACircleAndALine)
{
    SushiDSP::dk::DampedNewtonSolver<2> solver;
    configure(solver, 8);
    std::array<double, 2> v{1.0, 1.0};
    ASSERT_TRUE(solver.solve(CircleAndLine{}, v));
    const double bound = root_bound(std::sqrt(2.0), K_CIRCLE_INVERSE_NORM);
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(v[0], std::sqrt(2.0), bound)));
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(v[1], std::sqrt(2.0), bound)));
}

TEST(Small_DampedNewtonSolver, DampedNewtonCountsAConvergedCallInItsStatistics)
{
    SushiDSP::dk::DampedNewtonSolver<2> solver;
    configure(solver, 8);
    std::array<double, 2> v{1.0, 1.0};
    ASSERT_TRUE(solver.solve(CircleAndLine{}, v));
    EXPECT_EQ(solver.call_count(), 1u);
    EXPECT_EQ(solver.non_converged_count(), 0u);
    EXPECT_GE(solver.maximum_iterations_seen(), 1u);
}

TEST(Small_DampedNewtonSolver, DampedNewtonConvergesWhereUndampedNewtonDiverges)
{
    SushiDSP::dk::DampedNewtonSolver<1> solver;
    configure(solver, 20);
    std::array<double, 1> v{2.0};
    ASSERT_TRUE(solver.solve(ArcTangent{}, v));
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(v[0], 0.0, root_bound(0.0, 1.0))));
    EXPECT_EQ(solver.non_converged_count(), 0u);
}

TEST(Small_DampedNewtonSolver, DampedNewtonWithHalvingDisabledFailsOnTheDivergentStart)
{
    // With no halving, every iteration takes the full step. See tests/foundation/math/dk/README.md.
    SushiDSP::dk::DampedNewtonSolver<1> solver;
    solver.set_maximum_halvings(0);
    solver.set_maximum_iterations(20);
    std::array<double, 1> v{2.0};
    EXPECT_FALSE(solver.solve(ArcTangent{}, v));
    EXPECT_EQ(solver.non_converged_count(), 1u);
    EXPECT_TRUE(std::isfinite(v[0]));
}

TEST(Small_DampedNewtonSolver, DampedNewtonSurvivesASteepStartWithALargeInitialResidual)
{
    SushiDSP::dk::DampedNewtonSolver<1> solver;
    configure(solver, 60);
    std::array<double, 1> v{12.0};
    ASSERT_TRUE(solver.solve(SteepExponential{}, v));
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(v[0], 0.0, root_bound(0.0, 1.0))));
    EXPECT_EQ(solver.non_converged_count(), 0u);
}

TEST(Small_DampedNewtonSolver, DampedNewtonCountsANonConvergedCall)
{
    SushiDSP::dk::DampedNewtonSolver<1> solver;
    solver.set_maximum_iterations(1);
    std::array<double, 1> v{12.0};
    EXPECT_FALSE(solver.solve(SteepExponential{}, v));
    EXPECT_EQ(solver.non_converged_count(), 1u);
}

TEST(Small_DampedNewtonSolver, DampedNewtonLeavesTheIterateFiniteWhenItFailsToConverge)
{
    SushiDSP::dk::DampedNewtonSolver<1> solver;
    solver.set_maximum_iterations(1);
    std::array<double, 1> v{12.0};
    EXPECT_FALSE(solver.solve(SteepExponential{}, v));
    EXPECT_TRUE(std::isfinite(v[0]));
}

TEST(Small_DampedNewtonSolver, ResetStatisticsClearsTheCallAndNonConvergedCounts)
{
    SushiDSP::dk::DampedNewtonSolver<1> solver;
    solver.set_maximum_iterations(1);
    std::array<double, 1> v{12.0};
    EXPECT_FALSE(solver.solve(SteepExponential{}, v));
    solver.reset_statistics();
    EXPECT_EQ(solver.non_converged_count(), 0u);
    EXPECT_EQ(solver.call_count(), 0u);
}

class Small_DampedNewtonSolverWarmStart : public ::testing::TestWithParam<WarmStartRow>
{
};

// A warm start needs at most a couple of iterations. See tests/foundation/math/dk/README.md.
TEST_P(Small_DampedNewtonSolverWarmStart, ConvergesWithinThreeIterationsOnASmoothTrack)
{
    SushiDSP::dk::DampedNewtonSolver<1> solver;
    configure(solver, 8);
    std::array<double, 1> v{GetParam().previous};
    ASSERT_TRUE(solver.solve(Shifted{GetParam().target}, v));
    EXPECT_LE(solver.maximum_iterations_seen(), 3u);
}

TEST_P(Small_DampedNewtonSolverWarmStart, LandsOnTheNextRootOfTheTrack)
{
    SushiDSP::dk::DampedNewtonSolver<1> solver;
    configure(solver, 8);
    std::array<double, 1> v{GetParam().previous};
    ASSERT_TRUE(solver.solve(Shifted{GetParam().target}, v));
    const double inverse_norm = std::exp(-GetParam().target);
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(v[0], GetParam().target, root_bound(GetParam().target, inverse_norm))));
}

INSTANTIATE_TEST_SUITE_P(Track, Small_DampedNewtonSolverWarmStart, ::testing::ValuesIn(kWarmStartRows),
                         warm_start_name);

TEST(Small_DampedNewtonSolver, DampedNewtonReturnsTheBestIterateWhenItFailsToConverge)
{
    // A failed solve returns its lowest-residual iterate. See tests/foundation/math/dk/README.md.
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

class Small_DampedNewtonSolverLastEvaluation : public ::testing::TestWithParam<StartRow>
{
};

// The default budget of 8 iterations is too tight here. See tests/foundation/math/dk/README.md.
TEST_P(Small_DampedNewtonSolverLastEvaluation, IsAtTheIterateTheSolverReturns)
{
    SushiDSP::dk::DampedNewtonSolver<1> solver;
    configure(solver, 32);
    RecordingSystem system;
    std::array<double, 1> v{GetParam().start};
    ASSERT_TRUE(solver.solve(system, v));
    EXPECT_EQ(system.last_evaluated[0], v[0]);
}

INSTANTIATE_TEST_SUITE_P(Starts, Small_DampedNewtonSolverLastEvaluation, ::testing::ValuesIn(kLastEvaluationStarts),
                         start_name);

TEST(Medium_DampedNewtonSolver, SolvePerformsNoHeapAllocation)
{
    SushiDSP::dk::DampedNewtonSolver<2> solver;
    configure(solver, 8);
    std::array<double, 2> v{0.0, 0.0};

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    const int converged = solve_pair_repeatedly(solver, v, 256);
    trap.disarm();

    EXPECT_EQ(converged, 256);
    EXPECT_EQ(trap.count(), 0);
}

TEST(Small_DampedNewtonSolver, SolveFromADistantStartConvergesToTheRootOfTheExponentialPair)
{
    SushiDSP::dk::DampedNewtonSolver<2> solver;
    configure(solver, 8);
    std::array<double, 2> v{0.0, 0.0};
    ASSERT_EQ(solve_pair_repeatedly(solver, v, 1), 1);
    const double bound = root_bound(0.0, K_PAIR_INVERSE_NORM);
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(v[0], 0.0, bound)));
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(v[1], 0.0, bound)));
}
