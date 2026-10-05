/****************************************************************/
/* GoldenFile.hpp                                               */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

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
            signal[i] = static_cast<float>(0.251189 * std::sin(2.0 * M_PI * 220.0 * t) + 0.0501187 * std::sin(2.0 * M_PI * 1700.0 * t));
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

    /// @brief Compares rendered output with tests/golden/<name>.f32, or rewrites that file
    /// when SUSHIDSP_REGENERATE_GOLDEN is set in the environment.
    /// @param name File stem, e.g. "jcm800_48k".
    /// @param rendered The node's output for stimulus().
    /// @return Residual in dB; -200 after a regeneration; 0 when the file is missing or short.
    inline double compare_or_regenerate(const char* name, const std::vector<float>& rendered)
    {
        const std::string path = std::string(SUSHIDSP_GOLDEN_DIR) + "/" + name + ".f32";
        if (std::getenv("SUSHIDSP_REGENERATE_GOLDEN") != nullptr)
        {
            std::FILE* file = std::fopen(path.c_str(), "wb");
            if (file == nullptr)
            {
                return 0.0;
            }
            std::fwrite(rendered.data(), sizeof(float), rendered.size(), file);
            std::fclose(file);
            std::fprintf(stderr, "[golden] regenerated %s\n", path.c_str());
            return -200.0;
        }
        std::FILE* file = std::fopen(path.c_str(), "rb");
        if (file == nullptr)
        {
            return 0.0;
        }
        std::vector<float> stored(rendered.size(), 0.0f);
        const std::size_t read = std::fread(stored.data(), sizeof(float), stored.size(), file);
        std::fclose(file);
        if (read != rendered.size())
        {
            return 0.0;
        }
        return residual_db(stored, rendered);
    }
}
