/****************************************************************/
/* DeviceSettings.hpp                                           */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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

    /// @brief Draws the device and channel pickers in their own window, which does not block
    /// input to the windows behind it.
    /// @param asio_drivers Every ASIO driver name, or empty when ASIO is not built in.
    /// @param choice In/out: the current selection.
    /// @param already_running True when a rig is playing, which the window warns will be replaced.
    /// @param error Text from the last failed connection attempt, or empty.
    /// @param open In/out: the window draws only while this is true, and Close clears it.
    /// Connect leaves it set, so a failed connection keeps the window up to show the error; the
    /// caller clears it once the device opens.
    /// @return True on the frame Connect is clicked, meaning the caller should open the device.
    bool DrawDeviceSettings(const std::vector<std::string>& asio_drivers, DeviceChoice& choice,
                            bool already_running, const std::string& error, bool& open);
}
