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
 *  @brief Implements the session document, the track document and a track's rig preset file.
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
    /// @brief Holds the format number a track document is written with.
    constexpr int K_TRACK_FORMAT = 1;

    /// @brief Names what a track document is, so another JSON file is not taken for one.
    constexpr const char* K_TRACK_KIND = "sushidsp.track";

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

std::string track_to_json(const Session& session, std::size_t index)
{
    const SessionTimeline& timeline = session.timeline();
    nlohmann::json document = nlohmann::json::object();
    document["kind"] = K_TRACK_KIND;
    document["format"] = K_TRACK_FORMAT;
    document["name"] = timeline.track_name(index);
    document["color"] = timeline.track_color(index);
    document["strip"] = strip_to_json(session.strip(index));
    document["preset_path"] = session.preset_path(index);
    document["rig"] = nlohmann::json::parse(session.rig(index).save_rig_to_json());

    nlohmann::json clips = nlohmann::json::array();
    for (std::size_t clip = 0; clip < timeline.clip_count(index); ++clip)
    {
        const SessionTimeline::ClipPlacement placement = timeline.clip(index, clip);
        nlohmann::json row = nlohmann::json::object();
        row["path"] = placement.path;
        row["start_frame"] = placement.start_frame;
        row["take_offset"] = placement.take_offset;
        row["length_frames"] = placement.length_frames;
        clips.push_back(row);
    }
    document["clips"] = clips;
    return document.dump(2);
}

bool track_from_json(Session& session, const std::string& json_text, std::string& error_out)
{
    nlohmann::json document;
    std::vector<SessionTimeline::ClipPlacement> clips;
    try
    {
        document = nlohmann::json::parse(json_text);
        if (document.value("kind", std::string()) != K_TRACK_KIND)
        {
            error_out = "The file is not a SushiDSP track.";
            return false;
        }
        if (document.value("format", 0) > K_TRACK_FORMAT)
        {
            error_out = "The track file is newer than this build reads.";
            return false;
        }
        for (const nlohmann::json& row : document.value("clips", nlohmann::json::array()))
        {
            SessionTimeline::ClipPlacement placement;
            placement.path = row.at("path").get<std::string>();
            placement.start_frame = row.value("start_frame", std::uint64_t{0});
            placement.take_offset = row.value("take_offset", std::uint64_t{0});
            placement.length_frames = row.value("length_frames", std::uint64_t{0});
            clips.push_back(placement);
        }
    }
    catch (const std::exception& refusal)
    {
        error_out = refusal.what();
        return false;
    }

    if (!session.add_track(error_out))
    {
        return false;
    }
    const std::size_t index = session.track_count() - 1;
    SessionTimeline& timeline = session.timeline();
    try
    {
        if (document.contains("rig"))
        {
            session.rig(index).load_rig_from_json(document.at("rig").dump());
        }
    }
    catch (const std::exception& refusal)
    {
        std::string ignored;
        session.remove_track(index, false, ignored);
        error_out = refusal.what();
        return false;
    }
    timeline.set_track_name(index, document.value("name", timeline.track_name(index)));
    if (document.contains("color"))
    {
        timeline.set_track_color(index, document.at("color").get<SessionTimeline::TrackColor>());
    }
    session.set_strip(index, document.contains("strip") ? strip_from_json(document.at("strip"))
                                                         : TrackStrip());
    session.set_preset_path(index, document.value("preset_path", std::string()));
    session.select(index);
    session.touch_rigs();
    // The track stands even when a clip's file is gone; the reason is handed back.
    error_out.clear();
    if (!clips.empty())
    {
        timeline.publish_lane(index, clips, error_out);
    }
    return true;
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
