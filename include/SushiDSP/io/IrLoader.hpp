/****************************************************************/
/* IrLoader.hpp                                                 */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file IrLoader.hpp
 * @brief Declares the loader that conditions cabinet impulse responses for convolution.
 * @author Mustafa Garip
 */

#pragma once

#include <string>
#include <vector>

#include "SushiDSP/io/Ir.hpp"

namespace SushiDSP
{
    /// @brief Conditioning applied to an impulse response as it is loaded.
    ///
    /// A max_length_ms of zero or less disables truncation, leaving the impulse response at
    /// whatever length the source file yields after resampling.
    struct IrLoadOptions
    {
        float max_length_ms    = 200.0f;
        float fade_ms          = 5.0f;
        bool  normalise        = true;
        float normalise_low_hz = 100.0f;
        float normalise_high_hz = 5000.0f;
    };

    /// @brief Loads cabinet impulse responses and conditions them for convolution.
    ///
    /// Resamples to the host rate, truncates to a bounded length with a raised-cosine fade-out,
    /// and normalises the mid-band average magnitude to 0 dB so swapping IRs does not change
    /// perceived loudness. Control-thread only: allocates, throws, and touches the filesystem.
    class IrLoader
    {
    public:
        /// @brief Loads an impulse response from a WAV file and conditions it.
        /// @param path Filesystem path to the WAV file.
        /// @param target_sample_rate Sample rate the impulse response is needed at, in Hz.
        /// @param options Conditioning options.
        /// @return The conditioned impulse response at target_sample_rate.
        /// @throws std::runtime_error if the file cannot be opened or parsed.
        static Ir load(const std::string& path, double target_sample_rate, const IrLoadOptions& options = {});

        /// @brief Conditions in-memory samples through the same pipeline load() uses.
        /// @param samples Source impulse response samples.
        /// @param source_rate Sample rate the samples were captured at, in Hz.
        /// @param target_sample_rate Sample rate the impulse response is needed at, in Hz.
        /// @param options Conditioning options.
        /// @return The conditioned impulse response at target_sample_rate.
        /// @throws std::invalid_argument if either sample rate is not positive.
        static Ir prepare(std::vector<float> samples, double source_rate, double target_sample_rate,
                          const IrLoadOptions& options = {});
    };
}
