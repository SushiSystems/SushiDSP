/**************************************************************************/
/* session_file.cpp                                                       */
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
 *  @file session_file.cpp
 *  @brief Implements the project document and a track's rig preset file.
 *  @author Mustafa Garip & Sushi Systems
 */

#include "session_file.hpp"

#include <cmath>
#include <exception>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <nlohmann/json.hpp>

namespace
{
    /// @brief Writes a strip as a JSON object.
    nlohmann::json strip_to_json(const TrackStrip& strip)
    {
        nlohmann::json object = nlohmann::json::object();
        object["level_db"] = strip.level_db;
        object["pan"] = strip.pan;
        object["muted"] = strip.muted;
        object["soloed"] = strip.soloed;
        object["rig_bypassed"] = strip.rig_bypassed;
        return object;
    }

    /// @brief Reads a strip strip_to_json() wrote; a missing key keeps its default.
    TrackStrip strip_from_json(const nlohmann::json& object)
    {
        TrackStrip strip;
        strip.level_db = object.value("level_db", strip.level_db);
        strip.pan = object.value("pan", strip.pan);
        strip.muted = object.value("muted", strip.muted);
        strip.soloed = object.value("soloed", strip.soloed);
        strip.rig_bypassed = object.value("rig_bypassed", strip.rig_bypassed);
        return strip;
    }

    /// @brief Reads the strip a lane of a one-rig preset had: the lane's own gain, pan, mute and
    /// solo, passing its rig by.
    TrackStrip strip_of_lane(const Session& session, std::size_t index)
    {
        const SushiDSP::TrackPlayer& player = session.timeline().player();
        TrackStrip strip;
        const float gain = player.gain(index);
        strip.level_db = gain > 0.0f ? 20.0f * std::log10(gain) : -120.0f;
        strip.pan = player.pan(index);
        strip.muted = player.muted(index);
        strip.soloed = player.soloed(index);
        strip.rig_bypassed = true;
        return strip;
    }
} // namespace

std::string session_to_json(const Session& session)
{
    nlohmann::json document = nlohmann::json::parse(session.rig(0).save_to_json());
    nlohmann::json rigs = nlohmann::json::array();
    for (std::size_t index = 0; index < session.track_count(); ++index)
    {
        nlohmann::json row = nlohmann::json::object();
        row["index"] = index;
        row["strip"] = strip_to_json(session.strip(index));
        row["preset_path"] = session.preset_path(index);
        // The first track's rig is the document's own nodes, so a rig-only reader still loads it.
        if (index > 0)
        {
            row["rig"] = nlohmann::json::parse(session.rig(index).save_rig_to_json());
        }
        rigs.push_back(row);
    }
    document["track_rigs"] = rigs;
    return document.dump(2);
}

void session_from_json(Session& session, const std::string& json_text)
{
    const nlohmann::json document = nlohmann::json::parse(json_text);
    session.rig(0).load_from_json(json_text);
    session.match_timeline();

    if (!document.contains("track_rigs"))
    {
        // A preset of one rig: its rig is the first track's, and its lanes played beside it.
        for (std::size_t index = 1; index < session.track_count(); ++index)
        {
            session.rig(index).load_rig_from_json(R"({"nodes":[]})");
            session.set_strip(index, strip_of_lane(session, index));
            session.set_preset_path(index, std::string());
        }
        session.set_strip(0, TrackStrip());
        session.select(0);
        session.touch_rigs();
        return;
    }

    for (const nlohmann::json& row : document.at("track_rigs"))
    {
        const std::size_t index = row.value("index", std::size_t{0});
        if (index >= session.track_count())
        {
            continue;
        }
        if (index > 0 && row.contains("rig"))
        {
            session.rig(index).load_rig_from_json(row.at("rig").dump());
        }
        session.set_strip(index, row.contains("strip") ? strip_from_json(row.at("strip"))
                                                        : TrackStrip());
        session.set_preset_path(index, row.value("preset_path", std::string()));
    }
    session.select(0);
    session.touch_rigs();
}

void new_project(Session& session)
{
    std::string ignored;
    while (session.track_count() > 1)
    {
        session.remove_track(session.track_count() - 1, false, ignored);
    }
    SessionTimeline& timeline = session.timeline();
    timeline.clear_track(0);
    timeline.set_track_name(0, SessionTimeline::default_track_name(0));
    session.rig(0).load_rig_from_json(R"({"nodes":[]})");
    session.set_strip(0, TrackStrip());
    session.set_preset_path(0, std::string());
    session.select(0);
    session.touch_rigs();
}

bool load_track_rig(Session& session, std::size_t index, const std::string& path,
                    std::string& error_out)
{
    std::string text;
    if (index >= session.track_count() || !read_text_file(path, text, error_out))
    {
        return false;
    }
    try
    {
        session.rig(index).load_rig_from_json(text);
    }
    catch (const std::exception& refusal)
    {
        error_out = refusal.what();
        return false;
    }
    session.set_preset_path(index, path);
    session.touch_rigs();
    return true;
}

bool save_track_rig(Session& session, std::size_t index, const std::string& path,
                    std::string& error_out)
{
    if (index >= session.track_count())
    {
        return false;
    }
    std::string text;
    try
    {
        // The rig is serialized before the file is opened, since opening truncates it.
        text = session.rig(index).save_rig_to_json();
    }
    catch (const std::exception& refusal)
    {
        error_out = refusal.what();
        return false;
    }
    if (!write_text_file(path, text, error_out))
    {
        return false;
    }
    session.set_preset_path(index, path);
    return true;
}

bool read_text_file(const std::string& path, std::string& text_out, std::string& error_out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        error_out = "Cannot open " + path + ".";
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    text_out = buffer.str();
    return true;
}

bool write_text_file(const std::string& path, const std::string& text, std::string& error_out)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        error_out = "Cannot write " + path + ".";
        return false;
    }
    out << text;
    return static_cast<bool>(out);
}
