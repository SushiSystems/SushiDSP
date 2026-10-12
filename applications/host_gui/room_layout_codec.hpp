/**************************************************************************/
/* room_layout_codec.hpp                                                  */
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
 * @file room_layout_codec.hpp
 * @brief Declares the JSON codec for the cabinet section's layout: listener, cabinets and
 * microphones, without the room's shell.
 * @author Mustafa Garip
 */

#pragma once

#include <nlohmann/json.hpp>

#include "room_layout.hpp"

/// @brief A layout read back from JSON, with the room section's open flag.
struct RoomLayoutSnapshot
{
        /// @brief The restored layout; its shell is the starter room.
        RoomLayout layout;

        /// @brief True when the room section was open.
        bool room_open = false;
};

/// @brief Writes a layout as a version 1 JSON object.
/// @param layout The layout to write; its shell is left out.
/// @param room_open True when the room section is open.
/// @return The JSON object.
nlohmann::json room_layout_to_json(const RoomLayout& layout, bool room_open);

/// @brief Reads a version 1 JSON object back into a layout, keeping each microphone's id.
/// @param json The JSON object.
/// @return The layout and the open flag.
/// @throws std::runtime_error for a bad shape, a missing key, an unknown version, an unknown
/// pattern name, a refused cabinet or a microphone id the layout refuses.
RoomLayoutSnapshot room_layout_from_json(const nlohmann::json& json);
