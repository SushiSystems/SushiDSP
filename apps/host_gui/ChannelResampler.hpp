/**************************************************************************/
/* ChannelResampler.hpp                                                   */
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
