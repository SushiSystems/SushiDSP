/****************************************************************/
/* NullTest.hpp                                                 */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace SushiDSP::measure
{
    /// @brief Residual of b against a after integer-delay and least-squares gain alignment.
    ///
    /// The lag is the integer in [-max_lag, max_lag] maximising the magnitude of the
    /// cross-correlation, so a b that is inverted relative to a (an odd number of inverting
    /// stages in the path under test) still aligns on its true lag rather than on whatever
    /// lag happens to make the signed correlation least negative; the least-squares gain then
    /// comes out negative on its own. The gain is that least-squares scalar over the
    /// overlapping region. The residual is
    /// 20*log10(rms(a - g*b_shifted) / rms(a)), measured over that same overlapping region:
    /// the samples the shift leaves without a partner in b carry no information about how
    /// well the two signals match, and counting them would floor the result at the ratio of
    /// the lag to the length. Cost is O(length * max_lag): pass a small max_lag when the
    /// delay is known to be small.
    /// @param a Reference signal.
    /// @param b Signal under test.
    /// @param max_lag Largest delay searched, in samples.
    /// @return Residual in dB (negative; -200 when the residual is exactly zero).
    inline double null_test(const std::vector<float>& a, const std::vector<float>& b, std::uint32_t max_lag = 4096)
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
            const double difference = static_cast<double>(a[i]) - gain * b[static_cast<std::size_t>(j)];
            residual += difference * difference;
        }
        if (residual <= 0.0 || aa <= 0.0)
        {
            return -200.0;
        }
        return 10.0 * std::log10(residual / aa);
    }
}
