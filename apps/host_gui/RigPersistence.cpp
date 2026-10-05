/****************************************************************/
/* RigPersistence.cpp                                           */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/** @file RigPersistence.cpp
 *  @brief Implements the preset codec, including the room scene's JSON form.
 *  @author Mustafa Garip
 */

#include "RigPersistence.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/node_registry/NodeRegistry.hpp"
#include "SushiDSP/rig_preset/RigPreset.hpp"
#include "SushiDSP/room/Furniture.hpp"
#include "SushiDSP/room/Materials.hpp"
#include "SushiDSP/room/RoomScene.hpp"
#include "SushiDSP/room/StarterScene.hpp"

namespace
{
    /// @brief Pairs one enumerator with the word a preset writes for it.
    template <typename Enum>
    struct Word
    {
        Enum        value;
        const char* text;
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

    /// @brief Returns the preset word of a material, read from the absorption table.
    /// @throws std::runtime_error for a value no table row names.
    std::string material_word(SushiDSP::room::MaterialId id)
    {
        if (static_cast<std::size_t>(id) >= SushiDSP::room::kMaterialCount)
        {
            throw std::runtime_error(
                "RigController: a room field holds a value no preset word names");
        }
        return SushiDSP::room::material(id).key;
    }

    /// @brief Returns the material a preset word names, read from the absorption table.
    /// @throws std::runtime_error for a word no table row carries.
    SushiDSP::room::MaterialId material_from_word(const std::string& text, const char* field)
    {
        SushiDSP::room::MaterialId id{};
        if (!SushiDSP::room::find_material(text.c_str(), id))
        {
            throw std::runtime_error(std::string("RigController: unknown ") + field + " '" + text
                                     + "' in a preset's room");
        }
        return id;
    }

    /// @brief Returns the preset word of a furnishing, read from the furniture table.
    /// @throws std::runtime_error for a value no table row names.
    std::string furniture_word(SushiDSP::room::FurnitureId id)
    {
        if (static_cast<std::size_t>(id) >= SushiDSP::room::kFurnitureCount)
        {
            throw std::runtime_error(
                "RigController: a room field holds a value no preset word names");
        }
        return SushiDSP::room::furniture(id).key;
    }

    /// @brief Returns the furnishing a preset word names, read from the furniture table.
    /// @throws std::runtime_error for a word no table row carries.
    SushiDSP::room::FurnitureId furniture_from_word(const std::string& text, const char* field)
    {
        SushiDSP::room::FurnitureId id{};
        if (!SushiDSP::room::find_furniture(text.c_str(), id))
        {
            throw std::runtime_error(std::string("RigController: unknown ") + field + " '" + text
                                     + "' in a preset's room");
        }
        return id;
    }

    /// @brief Writes a lane's colour as its four channels, red first.
    nlohmann::json color_to_json(SessionTimeline::TrackColor color)
    {
        return nlohmann::json::array({ (color >> 24) & 0xFFu, (color >> 16) & 0xFFu,
                                       (color >> 8) & 0xFFu, color & 0xFFu });
    }

    /// @brief Reads a colour written by color_to_json.
    /// @throws std::runtime_error when the array does not hold four channels in 0-255.
    SessionTimeline::TrackColor color_from_json(const nlohmann::json& array)
    {
        if (!array.is_array() || array.size() != 4)
        {
            throw std::runtime_error("RigController: a preset's track colour is not four numbers");
        }
        SessionTimeline::TrackColor color = 0;
        for (const nlohmann::json& channel : array)
        {
            if (!channel.is_number_unsigned() || channel.get<std::uint64_t>() > 255u)
            {
                throw std::runtime_error(
                    "RigController: a preset's track colour channel is not a byte");
            }
            color = (color << 8) | channel.get<SessionTimeline::TrackColor>();
        }
        return color;
    }

    /// @brief Holds one preset track row, already read and checked, so the session is only
    /// touched once every row has parsed.
    struct TrackRow
    {
        std::size_t                                 index = 0;     ///< Holds the lane it restores.
        std::string                                 name;          ///< Holds the lane's name.
        SessionTimeline::TrackColor                 color = 0;     ///< Holds the lane's colour.
        bool                                        has_color = false; ///< Marks a row that named a colour.
        bool                                        has_name = false;  ///< Marks a row that named a name.
        float                                       gain = 1.0f;   ///< Holds the lane's gain.
        float                                       pan = 0.0f;    ///< Holds the lane's pan.
        bool                                        muted = false; ///< Marks the lane muted.
        bool                                        soloed = false;///< Marks the lane soloed.
        std::vector<SessionTimeline::ClipPlacement> clips;         ///< Holds the lane's clips.
    };

    /// @brief Reads every "tracks" row of a preset, dropping a row whose index no lane of the
    /// coming arrangement carries.
    /// @param document The preset.
    /// @param track_count How many lanes the arrangement will carry.
    /// @return The rows, in the order the preset wrote them.
    /// @throws std::runtime_error for a row the codec does not read.
    std::vector<TrackRow> track_rows_from_json(const nlohmann::json& document,
                                               std::size_t           track_count)
    {
        std::vector<TrackRow> rows;
        if (!document.contains("tracks"))
        {
            return rows;
        }
        for (const nlohmann::json& track_json : document.at("tracks"))
        {
            const std::size_t index = track_json.value("index", track_count);
            if (index >= track_count)
            {
                continue;
            }

            TrackRow row;
            row.index = index;
            row.has_name = track_json.contains("name");
            row.name = track_json.value("name", std::string());
            row.has_color = track_json.contains("color");
            if (row.has_color)
            {
                row.color = color_from_json(track_json.at("color"));
            }
            row.gain = track_json.value("gain", 1.0f);
            row.pan = track_json.value("pan", 0.0f);
            row.muted = track_json.value("muted", false);
            row.soloed = track_json.value("soloed", false);

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
                        row.clips.push_back(placement);
                    }
                }
            }
            else if (!track_json.value("path", std::string()).empty())
            {
                // A row naming only a file is one clip at frame 0; see apps/host_gui/README.md.
                SessionTimeline::ClipPlacement placement;
                placement.path = track_json.at("path").get<std::string>();
                row.clips.push_back(placement);
            }
            rows.push_back(std::move(row));
        }
        return rows;
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
            surfaces.push_back(material_word(scene.surfaces[face]));
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
            row["material"] = material_word(patch.material);
            row["area_m2"] = patch.area_m2;
            patches.push_back(row);
        }
        room["patches"] = patches;

        if (scene.furnishing_count > 0)
        {
            nlohmann::json furnishings = nlohmann::json::array();
            const std::uint32_t live = std::min<std::uint32_t>(
                scene.furnishing_count,
                static_cast<std::uint32_t>(SushiDSP::room::RoomScene::kMaxFurnishings));
            for (std::uint32_t index = 0; index < live; ++index)
            {
                const SushiDSP::room::RoomScene::Furnishing& entry = scene.furnishings[index];
                nlohmann::json row = nlohmann::json::object();
                row["item"] = furniture_word(entry.item);
                row["quantity"] = entry.quantity;
                furnishings.push_back(row);
            }
            room["furnishings"] = furnishings;
        }
        room["built_room_loss"] = scene.built_room_loss;

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
    /// keeps the starter scene's value, except furnishings, which an omitting object holds none of,
    /// and the built room's loss, which an omitting object leaves off.
    /// @throws std::runtime_error for a word or an array the codec does not read.
    SushiDSP::room::RoomScene room_scene_from_json(const nlohmann::json& room)
    {
        SushiDSP::room::RoomScene scene = SushiDSP::room::starter_scene();
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
                    material_from_word(surfaces.at(face).get<std::string>(), "material");
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
                patch.material = material_from_word(row.at("material").get<std::string>(),
                                                    "material");
                patch.area_m2 = row.at("area_m2").get<double>();
            }
        }
        if (room.contains("furnishings"))
        {
            const nlohmann::json& furnishings = room.at("furnishings");
            if (!furnishings.is_array())
            {
                throw std::runtime_error(
                    "RigController: a preset's room holds furnishings that are not an array");
            }
            scene.furnishing_count = 0;
            for (const nlohmann::json& row : furnishings)
            {
                if (scene.furnishing_count >= SushiDSP::room::RoomScene::kMaxFurnishings)
                {
                    throw std::runtime_error(
                        "RigController: a preset's room carries more than "
                        + std::to_string(SushiDSP::room::RoomScene::kMaxFurnishings)
                        + " furnishings");
                }
                const nlohmann::json& quantity = row.at("quantity");
                if (!quantity.is_number())
                {
                    throw std::runtime_error(
                        "RigController: a preset's furnishing quantity is not a number");
                }
                SushiDSP::room::RoomScene::Furnishing& entry =
                    scene.furnishings[scene.furnishing_count++];
                entry.item = furniture_from_word(row.at("item").get<std::string>(), "furnishing");
                entry.quantity = quantity.get<double>();
            }
        }
        else
        {
            scene.furnishing_count = 0;
        }
        scene.built_room_loss = false;
        if (room.contains("built_room_loss"))
        {
            const nlohmann::json& loss = room.at("built_room_loss");
            if (!loss.is_boolean())
            {
                throw std::runtime_error(
                    "RigController: a preset's room holds a built_room_loss that is not a boolean");
            }
            scene.built_room_loss = loss.get<bool>();
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
    document["track_count"] = session_.track_count();
    nlohmann::json tracks_json = nlohmann::json::array();
    for (std::size_t index = 0; index < session_.track_count(); ++index)
    {
        nlohmann::json track_json = nlohmann::json::object();
        track_json["index"] = index;
        track_json["name"] = session_.track_name(index);
        track_json["color"] = color_to_json(session_.track_color(index));
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

    // The playback speed is written nowhere; see apps/host_gui/README.md.
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
    // The scratch graph's pool is sized from the preset's node count; see apps/host_gui/README.md.
    const nlohmann::json document = nlohmann::json::parse(json_text);

    // Every track row is read and checked here, before the session is touched.
    const std::size_t track_count = std::min<std::size_t>(
        document.value("track_count", SessionTimeline::kMaxTracks), SessionTimeline::kMaxTracks);
    const std::vector<TrackRow> track_rows = track_rows_from_json(document, track_count);

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

    // A preset with no "track_count" carries the fixed set of lanes the host used to open with.
    std::string lane_error;
    while (session_.track_count() > track_count)
    {
        session_.remove_track(session_.track_count() - 1, lane_error);
    }
    while (session_.track_count() < track_count)
    {
        session_.add_track(lane_error);
    }

    // Every track is emptied to its defaults first, so a preset brings its own mixer state.
    for (std::size_t index = 0; index < session_.track_count(); ++index)
    {
        session_.clear_track(index);
    }
    for (const TrackRow& row : track_rows)
    {
        session_.publish_lane(row.index, row.clips, lane_error);
        session_.set_track_name(row.index,
                                row.has_name
                                    ? row.name
                                    : SessionTimeline::path_stem(session_.track_path(row.index)));
        session_.set_track_color(row.index, row.has_color
                                                ? row.color
                                                : SessionTimeline::palette_color(row.index));
        session_.player().set_gain(row.index, row.gain);
        session_.player().set_pan(row.index, row.pan);
        session_.player().set_mute(row.index, row.muted);
        session_.player().set_solo(row.index, row.soloed);
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

    // A preset with no tempo object means 120 BPM in 4/4, click off; see apps/host_gui/README.md.
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
