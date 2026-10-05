/****************************************************************/
/* DeviceSettings.hpp                                           */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file DeviceSettings.hpp
 * @brief Declares the device and channel settings window.
 * @author Mustafa Garip
 */

#pragma once

#include <string>
#include <vector>

#include "SushiDSP/audio_format/ChannelSelection.hpp"

namespace ui
{
    /// @brief What the settings window's controls currently select.
    struct DeviceChoice
    {
        int                          device_index;   ///< 0 is SDL2; 1..N index asio_drivers.
        SushiDSP::ChannelSelection   input;
        SushiDSP::ChannelSelection   output;
    };

    /// @brief Draws the device and channel pickers in a window that does not block the others.
    /// @param asio_drivers Every ASIO driver name, or empty when ASIO is not built in.
    /// @param choice In/out: the current selection.
    /// @param already_running True when a rig is playing, which the window warns will be replaced.
    /// @param error Text from the last failed connection attempt, or empty.
    /// @param open In/out: the window draws while this is true; the caller clears it on success.
    /// @return True on the frame Connect is clicked, meaning the caller should open the device.
    /// @see apps/host_gui/ui/README.md
    bool DrawDeviceSettings(const std::vector<std::string>& asio_drivers, DeviceChoice& choice,
                            bool already_running, const std::string& error, bool& open);
}
