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

    void fan_out_mono_to_stereo(const float* mono, float* const* outputs, std::uint32_t frames, ChannelSelection output_selection)
    {
        const bool write_left = output_selection != ChannelSelection::Right;
        const bool write_right = output_selection != ChannelSelection::Left;

        if (write_left)
        {
            std::memcpy(outputs[0], mono, frames * sizeof(float));
        }
        else
        {
            std::memset(outputs[0], 0, frames * sizeof(float));
        }
        if (write_right)
        {
            std::memcpy(outputs[1], mono, frames * sizeof(float));
        }
        else
        {
            std::memset(outputs[1], 0, frames * sizeof(float));
        }
    }
}
