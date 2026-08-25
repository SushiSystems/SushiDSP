/**************************************************************************/
/* ChannelSelection.hpp                                                   */
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

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace SushiDSP
{
    /// @brief Which physical device channel(s) a host app reads from / writes to. A rig node
    /// itself is always mono; this only picks which of a (stereo-capable) device's channels
    /// feeds it or receives its output.
    enum class ChannelSelection { Left, Right, Stereo };

    /// @brief Parses "L"/"l"/"R"/"r"/"stereo" into a ChannelSelection.
    /// @param value The flag's value, e.g. from "--input=L".
    /// @param flag_name The flag's name, used only in the exception message (e.g. "--input").
    /// @return The parsed selection.
    /// @throws std::runtime_error if value matches none of the accepted spellings.
    ChannelSelection parse_channel_selection(const std::string& value, const char* flag_name);

    /// @brief A short human-readable label for a selection: "L", "R", or "stereo".
    /// @param selection The selection to label.
    /// @return A statically-allocated label string.
    const char* channel_selection_label(ChannelSelection selection);

    /// @brief Maps a single-channel selection to a device input-array index: Left -> 0, Right
    /// -> 1. Not meaningful for Stereo (callers only pass Left/Right here, since capturing one
    /// mono signal always reads exactly one physical channel).
    /// @param selection Left or Right.
    /// @return 0 for Left, 1 for Right.
    std::size_t channel_selection_input_index(ChannelSelection selection);

    /// @brief Fans a processed mono signal out to up to 2 physical output channels. Real-time
    /// safe: no allocation, no lock, no syscall, no throw.
    /// @param mono The mono signal to fan out, `frames` samples.
    /// @param outputs Pointer to at least 2 output channel buffers (index 0 = left/channel A,
    /// index 1 = right/channel B). Both are always written (zeroed where not selected), so the
    /// caller never needs a separate memset.
    /// @param frames Sample count per channel.
    /// @param output_selection Left writes only outputs[0]; Right writes only outputs[1];
    /// Stereo writes both.
    void fan_out_mono_to_stereo(const float* mono, float* const* outputs, std::uint32_t frames, ChannelSelection output_selection);
}
