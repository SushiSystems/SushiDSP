/****************************************************************/
/* SpiceBlockCompare.hpp                                        */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "SushiDSP/math/Biquad.hpp"

#include "measure/SpiceCsv.hpp"

/// @file
/// @brief The three things every block test does to an LTspice reference before comparing it.
///
/// Each DK block test finds its CSV, band-limits both signals the same way and reads a
/// magnitude off a stepped sweep. Three test files would otherwise carry three copies of a
/// 4th-order Butterworth. Test-side only; no product includes it.
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
    ///
    /// The same filter shape and cutoff are applied to the reference and to the circuit under
    /// test, so the band-limiting cannot flatter either signal: it removes the top octave, where
    /// LTspice's variable-step integrator and our fixed-step trapezoidal rule are known to
    /// disagree (D6b plan, Risks).
    /// @param signal The signal to filter, modified in place.
    /// @param sample_rate Sample rate in hertz.
    /// @param cutoff_hz Corner frequency in hertz.
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
    inline bool interpolate_magnitude(const measure::AcSweep& sweep,
                                      int                     setting,
                                      double                  frequency_hz,
                                      double&                 magnitude_db)
    {
        bool        have_previous = false;
        double      previous_frequency = 0.0;
        double      previous_magnitude = 0.0;
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
}
