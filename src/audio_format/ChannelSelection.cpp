/**************************************************************************/
/* ChannelSelection.cpp                                                   */
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
 * @file ChannelSelection.cpp
 * @brief Implements channel selection parsing and the rig-to-device output adaptation.
 * @author Mustafa Garip
 */

#include "SushiDSP/audio_format/ChannelSelection.hpp"

#include <cstring>

namespace SushiDSP
{
    namespace
    {
        /// @brief Caps the widths write_rig_output_to_device knows how to read and write.
        constexpr std::uint32_t kMaxAdaptableChannels = 2;
    }

    ChannelSelection parse_channel_selection(const std::string& value, const char* flag_name)
    {
        if (value == "L" || value == "l") return ChannelSelection::Left;
        if (value == "R" || value == "r") return ChannelSelection::Right;
        if (value == "stereo") return ChannelSelection::Stereo;
        throw std::runtime_error(std::string(flag_name) + " must be one of L, R, stereo (got '" + value + "')");
    }

    const char* channel_selection_label(ChannelSelection selection)
    {
        switch (selection)
        {
            case ChannelSelection::Left:   return "L";
            case ChannelSelection::Right:  return "R";
            case ChannelSelection::Stereo: return "stereo";
        }
        return "?";
    }

    std::size_t channel_selection_input_index(ChannelSelection selection)
    {
        return selection == ChannelSelection::Right ? 1u : 0u;
    }

    bool rig_output_adapts_to_device(std::uint32_t rig_channels, std::uint32_t device_channels)
    {
        // Every pair this accepts is one write_rig_output_to_device indexes within bounds.
        return rig_channels != 0 && rig_channels <= kMaxAdaptableChannels
            && device_channels != 0 && device_channels <= kMaxAdaptableChannels;
    }

    void write_rig_output_to_device(const float* const* rig_outputs, std::uint32_t rig_channels,
                                    float* const* device_outputs, std::uint32_t device_channels,
                                    std::uint32_t frames, ChannelSelection output_selection)
    {
        if (device_channels == 1)
        {
            if (rig_channels == 1)
            {
                std::memcpy(device_outputs[0], rig_outputs[0], frames * sizeof(float));
            }
            else
            {
                for (std::uint32_t frame = 0; frame < frames; ++frame)
                {
                    device_outputs[0][frame] = (rig_outputs[0][frame] + rig_outputs[1][frame]) * 0.5f;
                }
            }
            return;
        }

        const float* left_source = rig_outputs[0];
        const float* right_source = rig_channels == 1 ? rig_outputs[0] : rig_outputs[1];
        const bool write_left = output_selection != ChannelSelection::Right;
        const bool write_right = output_selection != ChannelSelection::Left;

        if (write_left)
        {
            std::memcpy(device_outputs[0], left_source, frames * sizeof(float));
        }
        else
        {
            std::memset(device_outputs[0], 0, frames * sizeof(float));
        }
        if (write_right)
        {
            std::memcpy(device_outputs[1], right_source, frames * sizeof(float));
        }
        else
        {
            std::memset(device_outputs[1], 0, frames * sizeof(float));
        }
    }
}
