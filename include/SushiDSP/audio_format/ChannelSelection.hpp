/**************************************************************************/
/* ChannelSelection.hpp                                                   */
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
 * @file ChannelSelection.hpp
 * @brief Declares the device channel selection and the rig-to-device output adaptation.
 * @author Mustafa Garip
 */

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

    /// @brief Reports whether a rig output width can be written to a device output width. The
    /// table is the one RigGraph::channels_adapt carries, clamped to the two widths
    /// write_rig_output_to_device implements: 1 into 1, 2 into 2, 1 into 2 (copied) and 2 into 1
    /// (summed and halved). See docs/archive/superpowers/specs/2026-09-07-stereo-tracks-design.md.
    /// @param rig_channels The rig's output channel count.
    /// @param device_channels The device's output channel count.
    /// @return False when the pair has no rule, which the caller turns into a setup-time refusal.
    bool rig_output_adapts_to_device(std::uint32_t rig_channels, std::uint32_t device_channels);

    /// @brief Writes a rig's output block into a device's output channels, adapting the width by
    /// the rig_output_adapts_to_device table. Real-time safe: no allocation, lock, syscall or throw.
    /// @param rig_outputs One pointer per rig output channel, each `frames` samples.
    /// @param device_outputs One pointer per device output channel; every one is written in full.
    /// @param output_selection Gates a two-channel device: Left writes only device_outputs[0],
    /// Right only device_outputs[1], Stereo both. A one-channel device ignores it.
    /// @pre rig_output_adapts_to_device(rig_channels, device_channels) is true.
    void write_rig_output_to_device(const float* const* rig_outputs, std::uint32_t rig_channels,
                                    float* const* device_outputs, std::uint32_t device_channels,
                                    std::uint32_t frames, ChannelSelection output_selection);
}
