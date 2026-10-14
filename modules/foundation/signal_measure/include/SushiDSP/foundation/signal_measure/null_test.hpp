/**************************************************************************/
/* null_test.hpp                                                          */
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
 * @file null_test.hpp
 * @brief Defines the null test that aligns two signals and reports their residual.
 * @author Mustafa Garip
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace SushiDSP::measure
{
    /// @brief Returns the residual of b against a after integer-delay and gain alignment.
    /// @param a Reference signal.
    /// @param b Signal under test.
    /// @param max_lag Largest delay searched, in samples; the cost is O(length * max_lag).
    /// @return Residual in dB (negative; -200 when the residual is exactly zero).
    /// @see modules/foundation/signal_measure/README.md, "null_test.hpp", for the lag, gain and overlap rules.
    inline double null_test(const std::vector<float>& a, const std::vector<float>& b,
                            std::uint32_t max_lag = 4096)
    {
        const std::size_t length = std::min(a.size(), b.size());
        long best_lag = 0;
        double best_correlation = -1.0;
        for (long lag = -static_cast<long>(max_lag); lag <= static_cast<long>(max_lag); ++lag)
        {
            double correlation = 0.0;
            for (std::size_t i = 0; i < length; ++i)
            {
                const long j = static_cast<long>(i) - lag;
                if (j >= 0 && j < static_cast<long>(length))
                {
                    correlation += static_cast<double>(a[i]) * b[static_cast<std::size_t>(j)];
                }
            }
            if (std::fabs(correlation) > best_correlation)
            {
                best_correlation = std::fabs(correlation);
                best_lag = lag;
            }
        }
        double ab = 0.0, bb = 0.0, aa = 0.0;
        for (std::size_t i = 0; i < length; ++i)
        {
            const long j = static_cast<long>(i) - best_lag;
            if (j >= 0 && j < static_cast<long>(length))
            {
                const double bj = b[static_cast<std::size_t>(j)];
                ab += static_cast<double>(a[i]) * bj;
                bb += bj * bj;
                aa += static_cast<double>(a[i]) * a[i];
            }
        }
        const double gain = bb > 0.0 ? ab / bb : 0.0;
        double residual = 0.0;
        for (std::size_t i = 0; i < length; ++i)
        {
            const long j = static_cast<long>(i) - best_lag;
            if (j < 0 || j >= static_cast<long>(length))
            {
                continue;
            }
            const double difference =
                static_cast<double>(a[i]) - gain * b[static_cast<std::size_t>(j)];
            residual += difference * difference;
        }
        if (residual <= 0.0 || aa <= 0.0)
        {
            return -200.0;
        }
        return 10.0 * std::log10(residual / aa);
    }
} // namespace SushiDSP::measure
