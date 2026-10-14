/**************************************************************************/
/* spice_block_compare.hpp                                                */
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
 * @file spice_block_compare.hpp
 * @brief Defines the steps every block test applies to an LTspice reference before comparing.
 * @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <SushiDSP/foundation/math/biquad.hpp>

#include <SushiDSP/foundation/spice_reference/spice_csv.hpp.hpp>

namespace SushiDSP::test_circuits
{
    /// @brief Path of a generated LTspice CSV; a missing file means "not generated yet".
    /// @param block The SPICE block directory name.
    /// @param kind Either "ac" or "tran".
    /// @return The absolute path the export tool would write.
    inline std::string spice_path(const char* block, const char* kind)
    {
        return std::string(SUSHIDSP_SPICE_DIR) + "/" + block + "/" + block + "_" + kind + ".csv";
    }

    /// @brief Applies a 4th-order Butterworth low-pass, as two cascaded Biquad sections.
    /// @param signal The signal to filter, modified in place.
    /// @param sample_rate Sample rate in hertz.
    /// @param cutoff_hz Corner frequency in hertz.
    /// @see modules/foundation/circuit_blocks/README.md, "spice_block_compare.hpp", for what the filter removes.
    inline void band_limit(std::vector<float>& signal, double sample_rate, double cutoff_hz)
    {
        Biquad first;
        Biquad second;
        first.set_low_pass(sample_rate, cutoff_hz, 0.54119610);
        second.set_low_pass(sample_rate, cutoff_hz, 1.30656296);
        for (float& sample : signal)
        {
            sample = second.process_sample(first.process_sample(sample));
        }
    }

    /// @brief Linearly interpolates one setting's magnitude curve out of a loaded AC sweep.
    /// @param sweep The parsed sweep.
    /// @param setting The stepped-control index to read.
    /// @param frequency_hz The frequency to evaluate at.
    /// @param magnitude_db Receives the interpolated magnitude when the frequency is in range.
    /// @return False when the setting has fewer than two rows or the frequency is out of range.
    inline bool interpolate_magnitude(const measure::AcSweep& sweep, int setting,
                                      double frequency_hz, double& magnitude_db)
    {
        bool have_previous = false;
        double previous_frequency = 0.0;
        double previous_magnitude = 0.0;
        for (std::size_t row = 0; row < sweep.frequency_hz.size(); ++row)
        {
            if (sweep.setting[row] != setting)
            {
                continue;
            }
            const double frequency = sweep.frequency_hz[row];
            const double magnitude = sweep.magnitude_db[row];
            if (have_previous && frequency_hz >= previous_frequency && frequency_hz <= frequency)
            {
                const double span = frequency - previous_frequency;
                const double blend = span > 0.0 ? (frequency_hz - previous_frequency) / span : 0.0;
                magnitude_db = previous_magnitude + blend * (magnitude - previous_magnitude);
                return true;
            }
            previous_frequency = frequency;
            previous_magnitude = magnitude;
            have_previous = true;
        }
        return false;
    }
} // namespace SushiDSP::test_circuits
