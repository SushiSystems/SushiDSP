/**************************************************************************/
/* benchmark_registry.hpp                                                 */
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
 * @file benchmark_registry.hpp
 * @brief Declares the list each benchmark file adds its cases to, and the run that selects them.
 * @author Mustafa Garip
 */

#pragma once

#include <algorithm>
#include <cstring>
#include <functional>
#include <vector>

namespace SushiDSP::bench
{
    /// @brief One registered group of cases: the name a run selects it by, and its body.
    struct BenchmarkCase
    {
            const char* group;          /**< Selected when any argument is a substring of it. */
            std::function<void()> body; /**< Measures and prints the group's rows. */
    };

    /// @brief Returns every case registered so far, in registration order.
    inline std::vector<BenchmarkCase>& registered_cases()
    {
        static std::vector<BenchmarkCase> cases;
        return cases;
    }

    /// @brief Adds a case to the registry when a benchmark file's static instance is built.
    class BenchmarkRegistration final
    {
        public:
            /// @brief Registers one case.
            /// @param group A string literal naming the group.
            BenchmarkRegistration(const char* group, std::function<void()> body)
            {
                registered_cases().push_back(BenchmarkCase{group, std::move(body)});
            }
    };

    /// @brief Reports whether a group runs: every group with no arguments, otherwise each group
    /// that holds one of the arguments.
    inline bool is_selected(const char* group, int argc, const char* const* argv)
    {
        if (argc < 2)
            return true;
        for (int i = 1; i < argc; ++i)
        {
            if (std::strstr(group, argv[i]) != nullptr)
                return true;
        }
        return false;
    }

    /// @brief Runs every selected case, ordered by group name so the table does not depend on
    /// the order the files were linked in.
    /// @return The number of groups that ran.
    inline int run_selected(int argc, const char* const* argv)
    {
        std::vector<BenchmarkCase> cases = registered_cases();
        std::stable_sort(cases.begin(), cases.end(),
                         [](const BenchmarkCase& left, const BenchmarkCase& right)
                         { return std::strcmp(left.group, right.group) < 0; });
        int ran = 0;
        for (const BenchmarkCase& item : cases)
        {
            if (!is_selected(item.group, argc, argv))
                continue;
            item.body();
            ++ran;
        }
        return ran;
    }
} // namespace SushiDSP::bench
