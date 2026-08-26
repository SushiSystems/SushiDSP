/**************************************************************************/
/* Thd.hpp                                                                */
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

#include <cmath>
#include <cstdint>
#include <vector>

#include "measure/Spectrum.hpp"

namespace SushiDSP::measure
{
    /// @brief THD and the per-harmonic breakdown returned by thd().
    struct ThdResult
    {
        double              thd_percent;
        std::vector<double> per_harmonic_db;   // index k-1 holds harmonic k relative to the fundamental; index 0 is 0 dB
    };

    /// @brief THD of a steady-state signal at a bin-exact fundamental.
    /// @param signal At least kFftSize samples; the last kFftSize are analysed.
    /// @param f0 Fundamental in Hz (use bin_exact_frequency).
    /// @param sample_rate Sample rate.
    /// @param harmonics Number of harmonics reported, including the fundamental.
    /// @return THD in percent and each harmonic's level relative to the fundamental
    ///         (-300 for harmonics above Nyquist).
    inline ThdResult thd(const std::vector<float>& signal, double f0, double sample_rate, std::uint32_t harmonics = 20)
    {
        const auto spectrum = magnitude_spectrum_db(signal, signal.size() - kFftSize);
        const double fundamental = peak_near(spectrum, bin_of(f0, sample_rate));
        ThdResult result{ 0.0, {} };
        double harmonic_power = 0.0;
        for (std::uint32_t k = 1; k <= harmonics; ++k)
        {
            const double frequency = f0 * k;
            if (frequency >= sample_rate / 2.0)
            {
                result.per_harmonic_db.push_back(-300.0);
                continue;
            }
            const double level = peak_near(spectrum, bin_of(frequency, sample_rate)) - fundamental;
            result.per_harmonic_db.push_back(level);
            if (k > 1)
            {
                harmonic_power += std::pow(10.0, level / 10.0);
            }
        }
        result.thd_percent = 100.0 * std::sqrt(harmonic_power);
        return result;
    }
}
