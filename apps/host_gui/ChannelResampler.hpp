/****************************************************************/
/* ChannelResampler.hpp                                         */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file ChannelResampler.hpp
 * @brief Declares the rate conversion the host applies to a decoded file before it is written.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstddef>
#include <vector>

namespace SushiDSP::host
{
    /// @brief Converts planar samples from one rate to another over SushiDSP::Resampler, one
    /// channel at a time. Control thread only: it allocates.
    /// @param channels One vector of samples per channel; every channel must be the same length.
    /// @param source_rate The rate the samples carry, in Hz.
    /// @param target_rate The rate wanted, in Hz.
    /// @return The converted channels, round(frames * target_rate / source_rate) long. The input
    /// is returned unchanged when the rates match or either rate is not positive.
    std::vector<std::vector<float>> resample_channels(
        const std::vector<std::vector<float>>& channels, double source_rate, double target_rate);

    /// @brief Counts the frames a conversion produces.
    /// @param source_frames Frames going in.
    /// @param source_rate The rate they carry, in Hz.
    /// @param target_rate The rate wanted, in Hz.
    /// @return round(source_frames * target_rate / source_rate), or source_frames when either
    /// rate is not positive.
    std::size_t resampled_frame_count(std::size_t source_frames, double source_rate,
                                      double target_rate);
}
