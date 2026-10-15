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
 * @file test_dense_lu.cpp
 * @brief Tests that the dense LU factorisation solves known systems and refuses singular ones.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/math/dk/dense_lu.hpp>

#include <array>

TEST(DenseLu, SolvesAKnownThreeByThreeSystem)
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

TEST(DenseLu, PivotsWhenTheLeadingEntryIsZero)
{
    const std::array<std::array<double, 2>, 2> matrix{{{0.0, 1.0}, {1.0, 0.0}}};
    SushiDSP::dk::DenseLu<2> lu;
    ASSERT_TRUE(lu.factorise(matrix));
    std::array<double, 2> solution{};
    lu.solve(std::array<double, 2>{{3.0, 5.0}}, solution);
    EXPECT_NEAR(solution[0], 5.0, 1e-12);
    EXPECT_NEAR(solution[1], 3.0, 1e-12);
}

TEST(DenseLu, RejectsASingularMatrix)
{
    const std::array<std::array<double, 2>, 2> singular{{{1.0, 2.0}, {2.0, 4.0}}};
    SushiDSP::dk::DenseLu<2> lu;
    EXPECT_FALSE(lu.factorise(singular));
}
