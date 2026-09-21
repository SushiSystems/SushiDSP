/**************************************************************************/
/* RigPersistence.cpp                                                     */
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

/** @file RigPersistence.cpp
 *  @brief Implements the preset codec, including the room scene's JSON form.
 *  @author Mustafa Garip
 */

#include "RigPersistence.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/node_registry/NodeRegistry.hpp"
#include "SushiDSP/rig_preset/RigPreset.hpp"

namespace
{
    /// @brief Pairs one enumerator with the word a preset writes for it.
    template <typename Enum>
    struct Word
    {
        Enum        value;
        const char* text;
    };

    /// @brief Names each material as a preset writes it, in enumerator order.
    constexpr Word<SushiDSP::room::MaterialId> kMaterialWords[] = {
        { SushiDSP::room::MaterialId::ConcreteUnpainted, "concrete_unpainted" },
        { SushiDSP::room::MaterialId::ConcreteBlockPainted, "concrete_block_painted" },
        { SushiDSP::room::MaterialId::GypsumBoardOnStuds, "gypsum_board_on_studs" },
        { SushiDSP::room::MaterialId::HeavyCarpetOnConcrete, "heavy_carpet_on_concrete" },
        { SushiDSP::room::MaterialId::GlassWindow, "glass_window" },
        { SushiDSP::room::MaterialId::OpenDoorway, "open_doorway" },
    };

    /// @brief Names each scattering word as a preset writes it.
    constexpr Word<SushiDSP::room::Scattering> kScatteringWords[] = {
        { SushiDSP::room::Scattering::Flat, "flat" },
        { SushiDSP::room::Scattering::Textured, "textured" },
        { SushiDSP::room::Scattering::Cluttered, "cluttered" },
    };

    /// @brief Names each face as a preset writes it.
    constexpr Word<SushiDSP::room::Surface> kSurfaceWords[] = {
        { SushiDSP::room::Surface::MinusX, "minus_x" },
        { SushiDSP::room::Surface::PlusX, "plus_x" },
        { SushiDSP::room::Surface::MinusY, "minus_y" },
        { SushiDSP::room::Surface::PlusY, "plus_y" },
        { SushiDSP::room::Surface::Floor, "floor" },
        { SushiDSP::room::Surface::Ceiling, "ceiling" },
    };

    /// @brief Names each polar pattern as a preset writes it.
    constexpr Word<SushiDSP::room::PolarPattern> kPatternWords[] = {
        { SushiDSP::room::PolarPattern::Omni, "omni" },
        { SushiDSP::room::PolarPattern::Cardioid, "cardioid" },
        { SushiDSP::room::PolarPattern::Supercardioid, "supercardioid" },
        { SushiDSP::room::PolarPattern::FigureEight, "figure_eight" },
    };

    /// @brief Returns the word of an enumerator.
    /// @throws std::runtime_error for a value no row names.
    template <typename Enum, std::size_t N>
    std::string word_of(const Word<Enum> (&words)[N], Enum value)
    {
        for (const Word<Enum>& row : words)
        {
            if (row.value == value)
            {
                return row.text;
            }
        }
        throw std::runtime_error("RigController: a room field holds a value no preset word names");
    }

    /// @brief Returns the enumerator of a word.
    /// @throws std::runtime_error for a word no row carries.
    template <typename Enum, std::size_t N>
    Enum value_of(const Word<Enum> (&words)[N], const std::string& text, const char* field)
    {
        for (const Word<Enum>& row : words)
        {
            if (text == row.text)
            {
                return row.value;
            }
        }
        throw std::runtime_error(std::string("RigController: unknown ") + field + " '" + text
                                 + "' in a preset's room");
    }

    /// @brief Writes a point as a three-number array.
    nlohmann::json vec3_to_json(const SushiDSP::room::Vec3& point)
    {
        return nlohmann::json::array({ point.x, point.y, point.z });
    }

    /// @brief Reads a three-number array as a point.
    /// @throws std::runtime_error when the array does not hold three numbers.
    SushiDSP::room::Vec3 vec3_from_json(const nlohmann::json& array, const char* field)
    {
        if (!array.is_array() || array.size() != 3)
        {
            throw std::runtime_error(std::string("RigController: a preset's room ") + field
                                     + " is not three numbers");
        }
        return SushiDSP::room::Vec3{ array.at(0).get<double>(), array.at(1).get<double>(),
                                     array.at(2).get<double>() };
    }

    /// @brief Writes a pose as its position and its yaw.
    nlohmann::json pose_to_json(const SushiDSP::room::Vec3& position, double yaw_rad)
    {
        nlohmann::json pose = nlohmann::json::object();
        pose["position"] = vec3_to_json(position);
        pose["yaw_rad"] = yaw_rad;
        return pose;
    }

    /// @brief Reads a pose written by pose_to_json into a position and a yaw, leaving either
    /// as it was when the preset omits it.
    void pose_from_json(const nlohmann::json& pose, const char* field,
                        SushiDSP::room::Vec3& position, double& yaw_rad)
    {
        if (pose.contains("position"))
        {
            position = vec3_from_json(pose.at("position"), field);
        }
        yaw_rad = pose.value("yaw_rad", yaw_rad);
    }

    /// @brief Writes a scene as the "room" object of a preset node: the dimensions, the six
    /// materials and scattering words, the patches, the three poses, the microphone's pattern
    /// and the cabinet's geometry.
    nlohmann::json room_scene_to_json(const SushiDSP::room::RoomScene& scene)
    {
        nlohmann::json room = nlohmann::json::object();
        room["dimensions"] = vec3_to_json(scene.dimensions);

        nlohmann::json surfaces = nlohmann::json::array();
        nlohmann::json scattering = nlohmann::json::array();
        for (std::size_t face = 0; face < SushiDSP::room::kSurfaceCount; ++face)
        {
            surfaces.push_back(word_of(kMaterialWords, scene.surfaces[face]));
            scattering.push_back(word_of(kScatteringWords, scene.scattering[face]));
        }
        room["surfaces"] = surfaces;
        room["scattering"] = scattering;

        nlohmann::json patches = nlohmann::json::array();
        for (std::uint32_t index = 0; index < scene.patch_count; ++index)
        {
            const SushiDSP::room::RoomScene::Patch& patch = scene.patches[index];
            nlohmann::json row = nlohmann::json::object();
            row["surface"] = word_of(kSurfaceWords, patch.surface);
            row["material"] = word_of(kMaterialWords, patch.material);
            row["area_m2"] = patch.area_m2;
            patches.push_back(row);
        }
        room["patches"] = patches;

        room["source"] = pose_to_json(scene.source_position, scene.source_yaw_rad);
        room["listener"] = pose_to_json(scene.listener_position, scene.listener_yaw_rad);
        nlohmann::json microphone = pose_to_json(scene.microphone_position,
                                                 scene.microphone_yaw_rad);
        microphone["pattern"] = word_of(kPatternWords, scene.microphone_pattern);
        room["microphone"] = microphone;

        nlohmann::json cabinet = nlohmann::json::object();
        cabinet["cone_radius_m"] = scene.cabinet.cone_radius_m;
        cabinet["driver_spacing_m"] = scene.cabinet.driver_spacing_m;
        cabinet["baffle_width_m"] = scene.cabinet.baffle_width_m;
        cabinet["breakup_frequency_hz"] = scene.cabinet.breakup_frequency_hz;
        room["cabinet"] = cabinet;
        return room;
    }

    /// @brief Reads a "room" object written by room_scene_to_json; a field the object omits
    /// keeps the default scene's value.
    /// @throws std::runtime_error for a word or an array the codec does not read.
    SushiDSP::room::RoomScene room_scene_from_json(const nlohmann::json& room)
    {
        SushiDSP::room::RoomScene scene;
        if (room.contains("dimensions"))
        {
            scene.dimensions = vec3_from_json(room.at("dimensions"), "dimensions");
        }
        if (room.contains("surfaces"))
        {
            const nlohmann::json& surfaces = room.at("surfaces");
            for (std::size_t face = 0;
                 face < surfaces.size() && face < SushiDSP::room::kSurfaceCount; ++face)
            {
                scene.surfaces[face] =
                    value_of(kMaterialWords, surfaces.at(face).get<std::string>(), "material");
            }
        }
        if (room.contains("scattering"))
        {
            const nlohmann::json& scattering = room.at("scattering");
            for (std::size_t face = 0;
                 face < scattering.size() && face < SushiDSP::room::kSurfaceCount; ++face)
            {
                scene.scattering[face] = value_of(
                    kScatteringWords, scattering.at(face).get<std::string>(), "scattering");
            }
        }
        if (room.contains("patches"))
        {
            scene.patch_count = 0;
            for (const nlohmann::json& row : room.at("patches"))
            {
                if (scene.patch_count >= SushiDSP::room::RoomScene::kMaxPatches)
                {
                    throw std::runtime_error(
                        "RigController: a preset's room carries more than "
                        + std::to_string(SushiDSP::room::RoomScene::kMaxPatches) + " patches");
                }
                SushiDSP::room::RoomScene::Patch& patch = scene.patches[scene.patch_count++];
                patch.surface = value_of(kSurfaceWords, row.at("surface").get<std::string>(),
                                         "surface");
                patch.material = value_of(kMaterialWords, row.at("material").get<std::string>(),
                                          "material");
                patch.area_m2 = row.at("area_m2").get<double>();
            }
        }
        if (room.contains("source"))
        {
            pose_from_json(room.at("source"), "source", scene.source_position,
                           scene.source_yaw_rad);
        }
        if (room.contains("listener"))
        {
            pose_from_json(room.at("listener"), "listener", scene.listener_position,
                           scene.listener_yaw_rad);
        }
        if (room.contains("microphone"))
        {
            const nlohmann::json& microphone = room.at("microphone");
            pose_from_json(microphone, "microphone", scene.microphone_position,
                           scene.microphone_yaw_rad);
            if (microphone.contains("pattern"))
            {
                scene.microphone_pattern = value_of(
                    kPatternWords, microphone.at("pattern").get<std::string>(), "pattern");
            }
        }
        if (room.contains("cabinet"))
        {
            const nlohmann::json& cabinet = room.at("cabinet");
            scene.cabinet.cone_radius_m =
                cabinet.value("cone_radius_m", scene.cabinet.cone_radius_m);
            scene.cabinet.driver_spacing_m =
                cabinet.value("driver_spacing_m", scene.cabinet.driver_spacing_m);
            scene.cabinet.baffle_width_m =
                cabinet.value("baffle_width_m", scene.cabinet.baffle_width_m);
            scene.cabinet.breakup_frequency_hz =
                cabinet.value("breakup_frequency_hz", scene.cabinet.breakup_frequency_hz);
        }
        return scene;
    }
}

RigPersistence::RigPersistence(const SushiDSP::PrepareInfo& info, SlotGraph& graph,
                               SessionTimeline& session)
    : info_(info)
    , graph_(graph)
    , session_(session)
{
}

std::string RigPersistence::serialize_nodes() const
{
    if (graph_.slot_order().empty())
    {
        return "{\"nodes\":[]}";
    }

    std::unordered_map<SlotGraph::SlotId, SushiDSP::NodeHandle> handles;
    const std::unique_ptr<SushiDSP::RigGraph> graph = graph_.build_graph(false, handles);

    std::unordered_map<SushiDSP::NodeHandle, SlotGraph::SlotId> slot_by_handle;
    for (const auto& [slot, handle] : handles)
    {
        slot_by_handle[handle] = slot;
    }

    return SushiDSP::serialize_rig_to_json(*graph,
        [this, &slot_by_handle](SushiDSP::NodeHandle handle, nlohmann::json& node_json)
        {
            const auto it = slot_by_handle.find(handle);
            if (it == slot_by_handle.end())
            {
                return;
            }
            const std::optional<SlotGraph::SlotSnapshot> slot = graph_.snapshot(it->second);
            if (!slot.has_value())
            {
                return;
            }

            nlohmann::json params_json = nlohmann::json::object();
            for (const auto& [param_id, value] : slot->parameters)
            {
                params_json[param_id] = value;
            }
            node_json["params"] = params_json;

            if (!slot->cabinet_path.empty())
            {
                node_json["cabinet_ir"] = slot->cabinet_path;
            }
            if (slot->room_scene.has_value())
            {
                node_json["room"] = room_scene_to_json(*slot->room_scene);
            }
            if (slot->bypassed)
            {
                node_json["bypassed"] = true;
            }
        });
}

std::string RigPersistence::save_to_json() const
{
    nlohmann::json document = nlohmann::json::parse(serialize_nodes());
    if (!graph_.ir_folder().empty())
    {
        document["ir_folder"] = graph_.ir_folder();
    }
    if (!session_.take_folder().empty())
    {
        document["take_folder"] = session_.take_folder();
    }

    const SushiDSP::TrackPlayer& player = session_.player();
    nlohmann::json tracks_json = nlohmann::json::array();
    for (std::size_t index = 0; index < session_.track_count(); ++index)
    {
        if (session_.clip_count(index) == 0)
        {
            continue;
        }
        nlohmann::json track_json = nlohmann::json::object();
        track_json["index"] = index;
        track_json["name"] = session_.track_name(index);
        track_json["path"] = session_.track_path(index);
        track_json["gain"] = player.gain(index);
        track_json["pan"] = player.pan(index);
        track_json["muted"] = player.muted(index);
        track_json["soloed"] = player.soloed(index);

        nlohmann::json clips_json = nlohmann::json::array();
        for (std::size_t clip_index = 0; clip_index < session_.clip_count(index); ++clip_index)
        {
            const SessionTimeline::ClipPlacement placement = session_.clip(index, clip_index);
            nlohmann::json clip_json = nlohmann::json::object();
            clip_json["path"] = placement.path;
            clip_json["start_frame"] = placement.start_frame;
            clip_json["take_offset"] = placement.take_offset;
            clip_json["length_frames"] = placement.length_frames;
            clips_json.push_back(clip_json);
        }
        track_json["clips"] = clips_json;
        tracks_json.push_back(track_json);
    }
    if (!tracks_json.empty())
    {
        document["tracks"] = tracks_json;
    }

    const SushiDSP::Transport& transport = session_.transport();
    if (transport.has_loop())
    {
        nlohmann::json loop_json = nlohmann::json::object();
        loop_json["begin_frames"] = transport.loop_begin_frames();
        loop_json["end_frames"] = transport.loop_end_frames();
        document["loop"] = loop_json;
    }

    // The playback speed is a practice setting, not a session's, so it is written nowhere. See
    // docs/superpowers/specs/2026-09-08-arrangement-view-design.md §8.
    nlohmann::json tempo_json = nlohmann::json::object();
    tempo_json["bpm"] = session_.musical_clock().beats_per_minute();
    tempo_json["beats_per_bar"] = session_.musical_clock().beats_per_bar();
    tempo_json["beat_unit"] = session_.musical_clock().beat_unit();
    tempo_json["metronome_enabled"] = session_.metronome().enabled();
    tempo_json["metronome_level"] = session_.metronome().level();
    tempo_json["playback_speed"] = transport.speed();
    document["tempo"] = tempo_json;

    return document.dump(2);
}

void RigPersistence::load_from_json(const std::string& json_text)
{
    // Size the scratch graph's buffer pool from the preset's own node count (plus the same +1
    // headroom publish() uses). RigGraph::compile() only assert()s the pool's capacity, so a
    // fixed guess would hand the audio thread null buffers on any preset larger than that guess.
    const nlohmann::json document = nlohmann::json::parse(json_text);
    const std::size_t node_count = document.at("nodes").size();
    SushiDSP::RigGraph scratch(info_.max_block, static_cast<std::uint32_t>(node_count + 1));
    std::unordered_map<SushiDSP::NodeHandle, std::string> cabinet_paths_by_handle;
    std::unordered_map<SushiDSP::NodeHandle, bool> bypassed_by_handle;
    std::unordered_map<SushiDSP::NodeHandle, SushiDSP::room::RoomScene> room_scenes_by_handle;
    SushiDSP::deserialize_rig_from_json(scratch, json_text, info_,
        [&cabinet_paths_by_handle, &bypassed_by_handle, &room_scenes_by_handle](
            SushiDSP::NodeHandle handle, const nlohmann::json& node_json)
        {
            if (node_json.contains("cabinet_ir"))
            {
                cabinet_paths_by_handle[handle] = node_json.at("cabinet_ir").get<std::string>();
            }
            if (node_json.contains("room"))
            {
                room_scenes_by_handle[handle] = room_scene_from_json(node_json.at("room"));
            }
            if (node_json.contains("bypassed"))
            {
                bypassed_by_handle[handle] = node_json.at("bypassed").get<bool>();
            }
        });
    scratch.compile(info_);

    graph_.clear();
    graph_.set_ir_folder(document.contains("ir_folder")
                             ? document.at("ir_folder").get<std::string>()
                             : std::string());
    session_.set_take_folder(document.contains("take_folder")
                                 ? document.at("take_folder").get<std::string>()
                                 : std::string());

    // Every track is emptied to its defaults first, so a preset brings its own mixer state.
    for (std::size_t index = 0; index < session_.track_count(); ++index)
    {
        session_.clear_track(index);
    }
    if (document.contains("tracks"))
    {
        std::string track_error;
        for (const nlohmann::json& track_json : document.at("tracks"))
        {
            const std::size_t index = track_json.value("index", session_.track_count());
            if (index >= session_.track_count())
            {
                continue;
            }
            const std::string row_path = track_json.value("path", std::string());

            std::vector<SessionTimeline::ClipPlacement> clips;
            if (track_json.contains("clips"))
            {
                for (const nlohmann::json& clip_json : track_json.at("clips"))
                {
                    SessionTimeline::ClipPlacement placement;
                    placement.path = clip_json.value("path", std::string());
                    placement.start_frame = clip_json.value("start_frame", std::uint64_t{ 0 });
                    placement.take_offset = clip_json.value("take_offset", std::uint64_t{ 0 });
                    placement.length_frames = clip_json.value("length_frames", std::uint64_t{ 0 });
                    if (!placement.path.empty())
                    {
                        clips.push_back(placement);
                    }
                }
            }
            else if (!row_path.empty())
            {
                // A preset written before the timeline names one file per row, which is what one
                // clip at frame 0 means. Spec §8.
                SessionTimeline::ClipPlacement placement;
                placement.path = row_path;
                clips.push_back(placement);
            }

            session_.publish_lane(index, std::move(clips), track_error);
            session_.set_track_name(index, track_json.value(
                "name", SessionTimeline::path_stem(session_.track_path(index))));
            session_.player().set_gain(index, track_json.value("gain", 1.0f));
            session_.player().set_pan(index, track_json.value("pan", 0.0f));
            session_.player().set_mute(index, track_json.value("muted", false));
            session_.player().set_solo(index, track_json.value("soloed", false));
        }
    }

    session_.transport().clear_loop();
    if (document.contains("loop"))
    {
        const nlohmann::json& loop_json = document.at("loop");
        session_.transport().set_loop(loop_json.value("begin_frames", std::uint64_t{ 0 }),
                                      loop_json.value("end_frames", std::uint64_t{ 0 }));
    }

    // A session carries no playback speed, so every session opens at 1.0. Spec §8.
    session_.transport().set_speed(1.0);

    // A preset written before the timeline carries no tempo, which meant 120 BPM in 4/4 with the
    // click off. Spec §8.
    const nlohmann::json tempo_json = document.contains("tempo")
        ? document.at("tempo")
        : nlohmann::json::object();
    session_.musical_clock().set_tempo(tempo_json.value("bpm", 120.0));
    session_.musical_clock().set_meter(tempo_json.value("beats_per_bar", std::uint32_t{ 4 }),
                                       tempo_json.value("beat_unit", std::uint32_t{ 4 }));
    session_.musical_clock().set_sample_rate(info_.sample_rate);
    session_.metronome().set_enabled(tempo_json.value("metronome_enabled", false));
    session_.metronome().set_level(tempo_json.value("metronome_level", 0.5f));
    session_.transport().set_speed(tempo_json.value("playback_speed", 1.0));

    std::unordered_map<SushiDSP::NodeHandle, SlotGraph::SlotId> handle_to_slot;
    for (SushiDSP::NodeHandle handle : scratch.nodes())
    {
        SushiDSP::IAudioNode* node = scratch.node(handle);
        SlotGraph::SlotSnapshot snapshot;
        snapshot.descriptor_id = node->type_id();
        for (const auto& entry : SushiDSP::node_registry())
        {
            if (snapshot.descriptor_id == entry.descriptor->id)
            {
                for (std::size_t p = 0; p < entry.descriptor->parameter_count; ++p)
                {
                    const char* param_id = entry.descriptor->parameters[p].id;
                    snapshot.parameters[param_id] = node->get_parameter(param_id);
                }
                break;
            }
        }
        const auto cabinet_it = cabinet_paths_by_handle.find(handle);
        if (cabinet_it != cabinet_paths_by_handle.end())
        {
            snapshot.cabinet_path = cabinet_it->second;
        }
        const auto bypass_it = bypassed_by_handle.find(handle);
        if (bypass_it != bypassed_by_handle.end())
        {
            snapshot.bypassed = bypass_it->second;
        }
        const auto room_it = room_scenes_by_handle.find(handle);
        if (room_it != room_scenes_by_handle.end())
        {
            snapshot.room_scene = room_it->second;
        }
        handle_to_slot[handle] = graph_.restore(snapshot, false);
    }
    for (SushiDSP::NodeHandle handle : scratch.nodes())
    {
        for (SushiDSP::NodeHandle upstream_handle : scratch.upstream_of(handle))
        {
            graph_.connect(handle_to_slot.at(upstream_handle), handle_to_slot.at(handle), false);
        }
    }

    graph_.publish();
}
