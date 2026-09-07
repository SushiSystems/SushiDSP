/**************************************************************************/
/* ChannelSelection.cpp                                                   */
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

#include "SushiDSP/device/ChannelSelection.hpp"

#include <cstring>

namespace SushiDSP
{
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
        if (rig_channels == 0 || device_channels == 0)
        {
            return false;
        }
        return rig_channels == device_channels
            || (rig_channels == 1 && device_channels == 2)
            || (rig_channels == 2 && device_channels == 1);
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
