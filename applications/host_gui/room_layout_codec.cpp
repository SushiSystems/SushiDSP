/**************************************************************************/
/* room_layout_codec.cpp                                                  */
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
 * @file room_layout_codec.cpp
 * @brief Implements the JSON codec for the cabinet section's layout.
 * @author Mustafa Garip
 */

#include "room_layout_codec.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
    /// @brief The schema version this codec writes and reads.
    constexpr int K_VERSION = 1;

    /// @brief Pairs each polar pattern with its saved name.
    constexpr std::array<std::pair<SushiDSP::room::PolarPattern, const char*>, 4> K_PATTERNS{{
        {SushiDSP::room::PolarPattern::Omni, "Omni"},
        {SushiDSP::room::PolarPattern::Cardioid, "Cardioid"},
        {SushiDSP::room::PolarPattern::Supercardioid, "Supercardioid"},
        {SushiDSP::room::PolarPattern::FigureEight, "FigureEight"},
    }};

    /// @brief Throws the codec's error.
    /// @param what What is wrong.
    [[noreturn]] void fail(const std::string& what)
    {
        throw std::runtime_error("RoomLayoutCodec: " + what);
    }

    /// @brief Returns a pattern's saved name.
    /// @param pattern The pattern.
    /// @return The name.
    const char* pattern_name(SushiDSP::room::PolarPattern pattern)
    {
        for (const auto& row : K_PATTERNS)
        {
            if (row.first == pattern)
            {
                return row.second;
            }
        }
        fail("a microphone holds a pattern no name covers");
    }

    /// @brief Returns the pattern a saved name names.
    /// @param name The name.
    /// @return The pattern.
    SushiDSP::room::PolarPattern pattern_from_name(const std::string& name)
    {
        for (const auto& row : K_PATTERNS)
        {
            if (name == row.second)
            {
                return row.first;
            }
        }
        fail("unknown pattern '" + name + "'");
    }

    /// @brief Returns a required member of an object.
    /// @param object The object.
    /// @param key The member's key.
    /// @return The member.
    const nlohmann::json& member(const nlohmann::json& object, const char* key)
    {
        if (!object.is_object())
        {
            fail(std::string("expected an object holding '") + key + "'");
        }
        const auto found = object.find(key);
        if (found == object.end())
        {
            fail(std::string("missing key '") + key + "'");
        }
        return *found;
    }

    /// @brief Reads a required number member.
    /// @param object The object.
    /// @param key The member's key.
    /// @return The number.
    double read_number(const nlohmann::json& object, const char* key)
    {
        const nlohmann::json& value = member(object, key);
        if (!value.is_number())
        {
            fail(std::string("'") + key + "' is not a number");
        }
        return value.get<double>();
    }

    /// @brief Reads a required boolean member.
    /// @param object The object.
    /// @param key The member's key.
    /// @return The boolean.
    bool read_bool(const nlohmann::json& object, const char* key)
    {
        const nlohmann::json& value = member(object, key);
        if (!value.is_boolean())
        {
            fail(std::string("'") + key + "' is not a boolean");
        }
        return value.get<bool>();
    }

    /// @brief Reads a required array member.
    /// @param object The object.
    /// @param key The member's key.
    /// @return The array.
    const nlohmann::json& read_array(const nlohmann::json& object, const char* key)
    {
        const nlohmann::json& value = member(object, key);
        if (!value.is_array())
        {
            fail(std::string("'") + key + "' is not an array");
        }
        return value;
    }

    /// @brief Writes a placement's position and yaw into an object.
    /// @param place The placement.
    /// @param out The object written into.
    void write_placement(const RoomPlacement& place, nlohmann::json& out)
    {
        out["position"] = nlohmann::json::array(
            {place.position.x, place.position.y, place.position.z});
        out["yaw_rad"] = place.yaw_rad;
    }

    /// @brief Reads a placement's position and yaw from an object.
    /// @param object The object.
    /// @return The placement.
    RoomPlacement read_placement(const nlohmann::json& object)
    {
        const nlohmann::json& position = read_array(object, "position");
        if (position.size() != 3)
        {
            fail("'position' does not hold three numbers");
        }
        for (const auto& component : position)
        {
            if (!component.is_number())
            {
                fail("'position' holds a value that is not a number");
            }
        }
        RoomPlacement place;
        place.position = {position[0].get<double>(), position[1].get<double>(),
                          position[2].get<double>()};
        place.yaw_rad = read_number(object, "yaw_rad");
        return place;
    }

    /// @brief Reads one microphone entry.
    /// @param object The entry.
    /// @return The microphone, with its saved id.
    MicrophoneSpot read_microphone(const nlohmann::json& object)
    {
        const nlohmann::json& id = member(object, "id");
        if (!id.is_number_unsigned() || id.get<std::uint64_t>() < 1 ||
            id.get<std::uint64_t>() > UINT32_MAX)
        {
            fail("a microphone 'id' is not an integer from 1 up");
        }
        const nlohmann::json& pattern = member(object, "pattern");
        if (!pattern.is_string())
        {
            fail("a microphone 'pattern' is not a string");
        }
        MicrophoneSpot spot;
        spot.id = static_cast<std::uint32_t>(id.get<std::uint64_t>());
        spot.place = read_placement(object);
        spot.pattern = pattern_from_name(pattern.get<std::string>());
        spot.room_mic = read_bool(object, "room_mic");
        return spot;
    }
}

nlohmann::json room_layout_to_json(const RoomLayout& layout, bool room_open)
{
    nlohmann::json out;
    out["version"] = K_VERSION;
    out["open"] = room_open;

    nlohmann::json listener;
    write_placement(layout.listener(), listener);
    out["listener"] = std::move(listener);

    nlohmann::json cabinets = nlohmann::json::array();
    for (const RoomPlacement& place : layout.cabinets())
    {
        nlohmann::json cabinet;
        write_placement(place, cabinet);
        cabinets.push_back(std::move(cabinet));
    }
    out["cabinets"] = std::move(cabinets);

    nlohmann::json microphones = nlohmann::json::array();
    for (const MicrophoneSpot& spot : layout.microphones())
    {
        nlohmann::json microphone;
        microphone["id"] = spot.id;
        write_placement(spot.place, microphone);
        microphone["pattern"] = pattern_name(spot.pattern);
        microphone["room_mic"] = spot.room_mic;
        microphones.push_back(std::move(microphone));
    }
    out["microphones"] = std::move(microphones);

    return out;
}

RoomLayoutSnapshot room_layout_from_json(const nlohmann::json& json)
{
    if (!json.is_object())
    {
        fail("the layout is not an object");
    }
    const nlohmann::json& version = member(json, "version");
    if (!version.is_number_integer() || version.get<long long>() != K_VERSION)
    {
        fail("unsupported version " + version.dump());
    }

    RoomLayoutSnapshot snapshot;
    snapshot.room_open = read_bool(json, "open");
    snapshot.layout.set_listener(read_placement(member(json, "listener")));

    std::string reason;
    const nlohmann::json& cabinets = read_array(json, "cabinets");
    for (std::size_t index = 0; index < cabinets.size(); ++index)
    {
        if (!snapshot.layout.set_cabinet(index, read_placement(cabinets[index]), reason))
        {
            fail("cabinet " + std::to_string(index) + " refused: " + reason);
        }
    }

    for (const auto& entry : read_array(json, "microphones"))
    {
        if (!snapshot.layout.restore_microphone(read_microphone(entry), reason))
        {
            fail("microphone refused: " + reason);
        }
    }
    return snapshot;
}
