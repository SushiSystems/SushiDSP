/**************************************************************************/
/* golden_file.hpp                                                        */
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
 * @file golden_file.hpp
 * @brief Defines the golden-file stimulus, the residual rule and the comparison step.
 * @author Mustafa Garip
 */

#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include <SushiDSP/foundation/golden_compare/file_golden_store.hpp>
#include <SushiDSP/foundation/golden_compare/golden_compare_log_category.hpp>
#include <SushiDSP/foundation/golden_compare/golden_store.hpp>
#include <SushiDSP/foundation/log/log.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SushiDSP::golden
{
    /// @brief One second: a 220 Hz tone at -12 dBFS plus a 1.7 kHz tone at -26 dBFS, so the
    /// snapshot carries intermodulation as well as harmonic content. Deterministic per rate.
    /// @param sample_rate Sample rate.
    /// @return sample_rate samples.
    inline std::vector<float> stimulus(double sample_rate)
    {
        std::vector<float> signal(static_cast<std::size_t>(sample_rate));
        for (std::size_t i = 0; i < signal.size(); ++i)
        {
            const double t = static_cast<double>(i) / sample_rate;
            signal[i] = static_cast<float>(0.251189 * std::sin(2.0 * M_PI * 220.0 * t) +
                                           0.0501187 * std::sin(2.0 * M_PI * 1700.0 * t));
        }
        return signal;
    }

    /// @brief RMS residual of b against a, in dB relative to a's RMS; no alignment, since a
    /// golden file and its regeneration share every delay.
    /// @param a Golden samples.
    /// @param b Rendered samples.
    /// @return 20*log10(rms(a-b)/rms(a)), or -200 when identical.
    inline double residual_db(const std::vector<float>& a, const std::vector<float>& b)
    {
        double difference = 0.0, reference = 0.0;
        for (std::size_t i = 0; i < a.size() && i < b.size(); ++i)
        {
            const double d = static_cast<double>(a[i]) - b[i];
            difference += d * d;
            reference += static_cast<double>(a[i]) * a[i];
        }
        if (difference <= 0.0 || reference <= 0.0)
        {
            return -200.0;
        }
        return 10.0 * std::log10(difference / reference);
    }

    /// @brief What a golden comparison does with the reference file.
    enum class GoldenMode
    {
        /// @brief Reads the file and measures the residual against it.
        Compare,
        /// @brief Rewrites the file from the rendered samples and measures nothing.
        Regenerate,
    };

    /// @brief Reads the mode from the environment: Regenerate when SUSHIDSP_REGENERATE_GOLDEN is set.
    /// @return The mode a test run asked for.
    inline GoldenMode golden_mode_from_environment()
    {
        return std::getenv("SUSHIDSP_REGENERATE_GOLDEN") != nullptr ? GoldenMode::Regenerate
                                                                    : GoldenMode::Compare;
    }

    /// @brief Compares rendered output with <directory>/<name>.f32 in @p store, or rewrites that entry.
    /// @param store Where the reference samples live.
    /// @param directory Folder that holds the reference files.
    /// @param name File stem, e.g. "jcm800_48k".
    /// @param rendered The node's output for stimulus().
    /// @param mode Compare, or Regenerate to rewrite the entry.
    /// @return Residual in dB; 0 when no comparison was made: after a regeneration, or when the
    /// entry is missing or short. A regeneration therefore never reports a pass.
    inline double compare_or_regenerate(GoldenStore& store, const std::string& directory, const char* name,
                                        const std::vector<float>& rendered, GoldenMode mode)
    {
        const std::string path = directory + "/" + name + ".f32";
        if (mode == GoldenMode::Regenerate)
        {
            if (!store.write(path, rendered))
            {
                SD_LOG_ERROR(K_GOLDEN_COMPARE_LOG_CATEGORY,
                             "cannot open {} to regenerate it; reporting a 0 dB residual", path);
                return 0.0;
            }
            SD_LOG_WARN(K_GOLDEN_COMPARE_LOG_CATEGORY,
                        "regenerated {} from {} samples; reporting a 0 dB residual", path,
                        rendered.size());
            return 0.0;
        }
        std::vector<float> stored;
        if (!store.read(path, stored, rendered.size()))
        {
            SD_LOG_ERROR(K_GOLDEN_COMPARE_LOG_CATEGORY,
                         "cannot open golden file {}; reporting a 0 dB residual", path);
            return 0.0;
        }
        if (stored.size() != rendered.size())
        {
            SD_LOG_ERROR(K_GOLDEN_COMPARE_LOG_CATEGORY,
                         "golden file {} holds {} samples, not {}; reporting a 0 dB residual",
                         path, stored.size(), rendered.size());
            return 0.0;
        }
        return residual_db(stored, rendered);
    }

    /// @brief Compares rendered output with <directory>/<name>.f32, or rewrites that file.
    /// @param directory Folder that holds the reference files.
    /// @param name File stem, e.g. "jcm800_48k".
    /// @param rendered The node's output for stimulus().
    /// @param mode Compare, or Regenerate to rewrite the file.
    /// @return As the overload that takes a store, over files.
    inline double compare_or_regenerate(const std::string& directory, const char* name,
                                        const std::vector<float>& rendered, GoldenMode mode)
    {
        FileGoldenStore store;
        return compare_or_regenerate(store, directory, name, rendered, mode);
    }

    /// @brief Compares rendered output with <directory>/<name>.f32 in the mode the environment
    /// names; see golden_mode_from_environment().
    /// @param directory Folder that holds the reference files.
    /// @param name File stem, e.g. "jcm800_48k".
    /// @param rendered The node's output for stimulus().
    /// @return As the overload that takes a mode.
    inline double compare_or_regenerate(const std::string& directory, const char* name,
                                        const std::vector<float>& rendered)
    {
        return compare_or_regenerate(directory, name, rendered, golden_mode_from_environment());
    }
} // namespace SushiDSP::golden
