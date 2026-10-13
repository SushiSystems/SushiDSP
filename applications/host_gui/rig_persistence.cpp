/**************************************************************************/
/* rig_persistence.cpp                                                    */
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

/** @file rig_persistence.cpp
 *  @brief Implements the preset codec, including the room scene's JSON form.
 *  @author Mustafa Garip
 */

#include "rig_persistence.hpp"

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

#include "SushiDSP/core/rig_graph.hpp"
#include "SushiDSP/node_registry/node_registry.hpp"
#include "SushiDSP/rig_preset/rig_preset.hpp"
#include "SushiDSP/room/furniture.hpp"
#include "SushiDSP/log/log.hpp"
#include "SushiDSP/room/materials.hpp"
#include "SushiDSP/room/room_arrangement.hpp"
#include "SushiDSP/room/room_scene.hpp"
#include "SushiDSP/room/starter_scene.hpp"

#include "host_gui_log_category.hpp"
#include "rig_shape.hpp"

namespace
{
    /// @brief Pairs one enumerator with the word a preset writes for it.
    template <typename Enum> struct Word
    {
            Enum value;
            const char* text;
    };

    /// @brief Names each scattering word as a preset writes it.
    constexpr Word<SushiDSP::room::Scattering> K_SCATTERING_WORDS[] = {
        {SushiDSP::room::Scattering::Flat, "flat"},
        {SushiDSP::room::Scattering::Textured, "textured"},
        {SushiDSP::room::Scattering::Cluttered, "cluttered"},
    };

    /// @brief Names each face as a preset writes it.
    constexpr Word<SushiDSP::room::Surface> K_SURFACE_WORDS[] = {
        {SushiDSP::room::Surface::MinusX, "minus_x"}, {SushiDSP::room::Surface::PlusX, "plus_x"},
        {SushiDSP::room::Surface::MinusY, "minus_y"}, {SushiDSP::room::Surface::PlusY, "plus_y"},
        {SushiDSP::room::Surface::Floor, "floor"},    {SushiDSP::room::Surface::Ceiling, "ceiling"},
    };

    /// @brief Names each polar pattern as a preset writes it.
    constexpr Word<SushiDSP::room::PolarPattern> K_PATTERN_WORDS[] = {
        {SushiDSP::room::PolarPattern::Omni, "omni"},
        {SushiDSP::room::PolarPattern::Cardioid, "cardioid"},
        {SushiDSP::room::PolarPattern::Supercardioid, "supercardioid"},
        {SushiDSP::room::PolarPattern::FigureEight, "figure_eight"},
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
        throw std::runtime_error(std::string("RigController: unknown ") + field + " '" + text +
                                 "' in a preset's room");
    }

    /// @brief Returns the preset word of a material, read from the absorption table.
    /// @throws std::runtime_error for a value no table row names.
    std::string material_word(SushiDSP::room::MaterialId id)
    {
        if (static_cast<std::size_t>(id) >= SushiDSP::room::K_MATERIAL_COUNT)
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
            throw std::runtime_error(std::string("RigController: unknown ") + field + " '" + text +
                                     "' in a preset's room");
        }
        return id;
    }

    /// @brief Returns the preset word of a furnishing, read from the furniture table.
    /// @throws std::runtime_error for a value no table row names.
    std::string furniture_word(SushiDSP::room::FurnitureId id)
    {
        if (static_cast<std::size_t>(id) >= SushiDSP::room::K_FURNITURE_COUNT)
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
            throw std::runtime_error(std::string("RigController: unknown ") + field + " '" + text +
                                     "' in a preset's room");
        }
        return id;
    }

    /// @brief Writes a lane's colour as its four channels, red first.
    nlohmann::json color_to_json(SessionTimeline::TrackColor color)
    {
        return nlohmann::json::array(
            {(color >> 24) & 0xFFu, (color >> 16) & 0xFFu, (color >> 8) & 0xFFu, color & 0xFFu});
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
            std::size_t index = 0;                             ///< Holds the lane it restores.
            std::string name;                                  ///< Holds the lane's name.
            SessionTimeline::TrackColor color = 0;             ///< Holds the lane's colour.
            bool has_color = false;                            ///< Marks a row that named a colour.
            bool has_name = false;                             ///< Marks a row that named a name.
            float gain = 1.0f;                                 ///< Holds the lane's gain.
            float pan = 0.0f;                                  ///< Holds the lane's pan.
            bool muted = false;                                ///< Marks the lane muted.
            bool soloed = false;                               ///< Marks the lane soloed.
            std::vector<SessionTimeline::ClipPlacement> clips; ///< Holds the lane's clips.
    };

    /// @brief Reads every "tracks" row of a preset, dropping a row whose index no lane of the
    /// coming arrangement carries.
    /// @param document The preset.
    /// @param track_count How many lanes the arrangement will carry.
    /// @return The rows, in the order the preset wrote them.
    /// @throws std::runtime_error for a row the codec does not read.
    std::vector<TrackRow> track_rows_from_json(const nlohmann::json& document,
                                               std::size_t track_count)
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
                    placement.start_frame = clip_json.value("start_frame", std::uint64_t{0});
                    placement.take_offset = clip_json.value("take_offset", std::uint64_t{0});
                    placement.length_frames = clip_json.value("length_frames", std::uint64_t{0});
                    if (!placement.path.empty())
                    {
                        row.clips.push_back(placement);
                    }
                }
            }
            else if (!track_json.value("path", std::string()).empty())
            {
                // A row naming only a file is one clip at frame 0; see applications/host_gui/README.md.
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
        return nlohmann::json::array({point.x, point.y, point.z});
    }

    /// @brief Reads a three-number array as a point.
    /// @throws std::runtime_error when the array does not hold three numbers.
    SushiDSP::room::Vec3 vec3_from_json(const nlohmann::json& array, const char* field)
    {
        if (!array.is_array() || array.size() != 3)
        {
            throw std::runtime_error(std::string("RigController: a preset's room ") + field +
                                     " is not three numbers");
        }
        return SushiDSP::room::Vec3{array.at(0).get<double>(), array.at(1).get<double>(),
                                    array.at(2).get<double>()};
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
        for (std::size_t face = 0; face < SushiDSP::room::K_SURFACE_COUNT; ++face)
        {
            surfaces.push_back(material_word(scene.surfaces[face]));
            scattering.push_back(word_of(K_SCATTERING_WORDS, scene.scattering[face]));
        }
        room["surfaces"] = surfaces;
        room["scattering"] = scattering;

        nlohmann::json patches = nlohmann::json::array();
        for (std::uint32_t index = 0; index < scene.patch_count; ++index)
        {
            const SushiDSP::room::RoomScene::Patch& patch = scene.patches[index];
            nlohmann::json row = nlohmann::json::object();
            row["surface"] = word_of(K_SURFACE_WORDS, patch.surface);
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
                static_cast<std::uint32_t>(SushiDSP::room::RoomScene::K_MAX_FURNISHINGS));
            for (std::uint32_t index = 0; index < live; ++index)
            {
                const SushiDSP::room::RoomScene::Furnishing& entry = scene.furnishings[index];
                nlohmann::json row = nlohmann::json::object();
                row["item"] = furniture_word(entry.item);
                row["quantity"] = entry.quantity;
                if (entry.placed)
                {
                    row["position"] = nlohmann::json::array({entry.x_m, entry.y_m});
                    row["yaw"] = entry.yaw_rad;
                }
                furnishings.push_back(row);
            }
            room["furnishings"] = furnishings;
        }
        room["built_room_loss"] = scene.built_room_loss;

        room["source"] = pose_to_json(scene.source_position, scene.source_yaw_rad);
        room["listener"] = pose_to_json(scene.listener_position, scene.listener_yaw_rad);
        nlohmann::json microphone =
            pose_to_json(scene.microphone_position, scene.microphone_yaw_rad);
        microphone["pattern"] = word_of(K_PATTERN_WORDS, scene.microphone_pattern);
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
                 face < surfaces.size() && face < SushiDSP::room::K_SURFACE_COUNT; ++face)
            {
                scene.surfaces[face] =
                    material_from_word(surfaces.at(face).get<std::string>(), "material");
            }
        }
        if (room.contains("scattering"))
        {
            const nlohmann::json& scattering = room.at("scattering");
            for (std::size_t face = 0;
                 face < scattering.size() && face < SushiDSP::room::K_SURFACE_COUNT; ++face)
            {
                scene.scattering[face] = value_of(
                    K_SCATTERING_WORDS, scattering.at(face).get<std::string>(), "scattering");
            }
        }
        if (room.contains("patches"))
        {
            scene.patch_count = 0;
            for (const nlohmann::json& row : room.at("patches"))
            {
                if (scene.patch_count >= SushiDSP::room::RoomScene::K_MAX_PATCHES)
                {
                    throw std::runtime_error(
                        "RigController: a preset's room carries more than " +
                        std::to_string(SushiDSP::room::RoomScene::K_MAX_PATCHES) + " patches");
                }
                SushiDSP::room::RoomScene::Patch& patch = scene.patches[scene.patch_count++];
                patch.surface =
                    value_of(K_SURFACE_WORDS, row.at("surface").get<std::string>(), "surface");
                patch.material =
                    material_from_word(row.at("material").get<std::string>(), "material");
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
                if (scene.furnishing_count >= SushiDSP::room::RoomScene::K_MAX_FURNISHINGS)
                {
                    throw std::runtime_error(
                        "RigController: a preset's room carries more than " +
                        std::to_string(SushiDSP::room::RoomScene::K_MAX_FURNISHINGS) +
                        " furnishings");
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
                entry.placed = false;
                entry.x_m = 0.0;
                entry.y_m = 0.0;
                entry.yaw_rad = 0.0;
                if (row.contains("position"))
                {
                    const nlohmann::json& position = row.at("position");
                    if (!position.is_array() || position.size() != 2 ||
                        !position[0].is_number() || !position[1].is_number())
                    {
                        throw std::runtime_error(
                            "RigController: a preset's furnishing position is not two numbers");
                    }
                    entry.placed = true;
                    entry.x_m = position[0].get<double>();
                    entry.y_m = position[1].get<double>();
                    entry.yaw_rad = row.value("yaw", 0.0);
                }
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
                    K_PATTERN_WORDS, microphone.at("pattern").get<std::string>(), "pattern");
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
    /// @brief Holds the preset format this build writes. Format 1 carried a room as a slot of the
    /// chain; format 2 carries the cabinet section: the rig's shape, its edges with their ports,
    /// and the room's arrangement.
    constexpr int K_PRESET_FORMAT = 2;

    /// @brief Names the descriptor of the room.
    constexpr const char* K_ROOM_TYPE = "sushidsp.room";

    /// @brief Maps each slot to the id its node carries in the preset being written.
    using PresetIdBySlot = std::unordered_map<SlotGraph::SlotId, SushiDSP::NodeHandle>;

    /// @brief Maps each node id of the preset being read to the slot built for it.
    using SlotByPresetId = std::unordered_map<int, SlotGraph::SlotId>;

    /// @brief Names each chain type as a preset writes it.
    constexpr Word<ChainType> K_CHAIN_WORDS[] = {
        {ChainType::Single, "single"},
        {ChainType::TwoWay, "two_way"},
        {ChainType::ThreeWay, "three_way"},
        {ChainType::Parallel, "parallel"},
    };

    /// @brief Names each DI position as a preset writes it.
    constexpr Word<DiPosition> K_DI_POSITION_WORDS[] = {
        {DiPosition::BeforeEffects, "before_effects"},
        {DiPosition::AfterEffects, "after_effects"},
    };

    /// @brief Pairs one hidden slot with the key a preset writes it under.
    struct HiddenSlotKey
    {
            const char* key;
            std::optional<ui::SlotId> HiddenSlots::* slot;
    };

    /// @brief Names each single hidden slot as a preset writes it.
    constexpr HiddenSlotKey K_HIDDEN_SLOT_KEYS[] = {
        {"splitter", &HiddenSlots::splitter},
        {"mixer", &HiddenSlots::mixer},
        {"left_strip", &HiddenSlots::left_strip},
        {"right_strip", &HiddenSlots::right_strip},
        {"di_strip", &HiddenSlots::di_strip},
        {"master_bus", &HiddenSlots::master_bus},
        {"room", &HiddenSlots::room},
        {"listener_strip", &HiddenSlots::listener_strip},
        {"bus", &HiddenSlots::bus},
        {"room_sum", &HiddenSlots::room_sum},
        {"room_strip", &HiddenSlots::room_strip},
        {"amp_bus", &HiddenSlots::amp_bus},
        {"master_strip", &HiddenSlots::master_strip},
        {"join", &HiddenSlots::join},
    };

    /// @brief Pairs one feeder list with the key a preset writes it under.
    struct FeederListKey
    {
            const char* key;
            std::vector<ui::SlotId> HiddenSlots::* list;
    };

    /// @brief Names each feeder list as a preset writes it.
    constexpr FeederListKey K_FEEDER_LIST_KEYS[] = {
        {"bus_feeders", &HiddenSlots::bus_feeders},
        {"room_sum_feeders", &HiddenSlots::room_sum_feeders},
        {"amp_bus_feeders", &HiddenSlots::amp_bus_feeders},
        {"master_bus_feeders", &HiddenSlots::master_bus_feeders},
    };

    /// @brief Writes a list of slots as their preset ids; a slot the preset does not hold is
    /// left out.
    nlohmann::json slots_to_json(const std::vector<ui::SlotId>& slots, const PresetIdBySlot& ids)
    {
        nlohmann::json list = nlohmann::json::array();
        for (const ui::SlotId slot : slots)
        {
            const auto found = ids.find(slot);
            if (found != ids.end())
            {
                list.push_back(found->second);
            }
        }
        return list;
    }

    /// @brief Reads a list of preset ids as slots; an id the preset does not hold is left out.
    std::vector<ui::SlotId> slots_from_json(const nlohmann::json& list, const SlotByPresetId& slots)
    {
        std::vector<ui::SlotId> result;
        if (!list.is_array())
        {
            return result;
        }
        for (const nlohmann::json& id : list)
        {
            const auto found = id.is_number_integer() ? slots.find(id.get<int>()) : slots.end();
            if (found != slots.end())
            {
                result.push_back(found->second);
            }
        }
        return result;
    }

    /// @brief Writes an arrangement as the "room" object of a preset's room node: the shell and
    /// the listener as a scene writes them, then one row per cabinet and per microphone. A row
    /// names the node it stands for when the preset holds that node.
    nlohmann::json arrangement_to_json(const SushiDSP::Room::Arrangement& arrangement,
                                       const PresetIdBySlot& ids)
    {
        SushiDSP::room::RoomScene scene = arrangement.shell();
        scene.listener_position = arrangement.listener().position;
        scene.listener_yaw_rad = arrangement.listener().yaw_rad;
        if (!arrangement.sources().empty())
        {
            scene.source_position = arrangement.sources().front().pose.position;
            scene.source_yaw_rad = arrangement.sources().front().pose.yaw_rad;
        }
        nlohmann::json room = room_scene_to_json(scene);

        const auto name_node = [&ids](nlohmann::json& row, std::uint64_t id)
        {
            row["id"] = id;
            const auto found = ids.find(static_cast<SlotGraph::SlotId>(id));
            if (id <= 0xFFFFFFFFu && found != ids.end())
            {
                row["node"] = found->second;
            }
        };
        nlohmann::json sources = nlohmann::json::array();
        for (const SushiDSP::Room::SourcePlacement& source : arrangement.sources())
        {
            nlohmann::json row = pose_to_json(source.pose.position, source.pose.yaw_rad);
            name_node(row, source.id);
            // A geometry that is no shipped cabinet is the shell's, which the room object holds.
            SushiDSP::room::CabinetKind kind{};
            if (SushiDSP::room::cabinet_kind_of(source.cabinet, kind))
            {
                row["cabinet"] = SushiDSP::room::cabinet_preset(kind).key;
            }
            sources.push_back(row);
        }
        room["sources"] = sources;

        nlohmann::json microphones = nlohmann::json::array();
        for (const SushiDSP::Room::MicrophonePlacement& microphone : arrangement.microphones())
        {
            nlohmann::json row = pose_to_json(microphone.pose.position, microphone.pose.yaw_rad);
            row["pattern"] = word_of(K_PATTERN_WORDS, microphone.pattern);
            row["proximity"] = microphone.proximity;
            row["model"] = SushiDSP::room::microphone_preset(microphone.kind).key;
            name_node(row, microphone.id);
            microphones.push_back(row);
        }
        room["microphones"] = microphones;
        return room;
    }

    /// @brief Reads a "room" object written by arrangement_to_json. A row that names a node the
    /// preset holds takes that node's slot as its identity; any other row keeps the identity it
    /// was written with.
    /// @throws std::runtime_error when the object is malformed or holds more cabinets or
    /// microphones than a room takes.
    SushiDSP::Room::Arrangement arrangement_from_json(const nlohmann::json& room,
                                                      const SlotByPresetId& slots)
    {
        const SushiDSP::room::RoomScene scene = room_scene_from_json(room);
        SushiDSP::Room::Arrangement arrangement;
        arrangement.set_shell(scene);
        arrangement.set_listener({scene.listener_position, scene.listener_yaw_rad});

        const auto identity = [&slots](const nlohmann::json& row, std::size_t index)
        {
            if (row.contains("node") && row.at("node").is_number_integer())
            {
                const auto found = slots.find(row.at("node").get<int>());
                if (found != slots.end())
                {
                    return static_cast<std::uint64_t>(found->second);
                }
            }
            return row.value("id", static_cast<std::uint64_t>(index));
        };
        std::string reason;
        std::size_t index = 0;
        for (const nlohmann::json& row : room.value("sources", nlohmann::json::array()))
        {
            SushiDSP::Room::SourcePlacement source;
            source.pose = {scene.source_position, scene.source_yaw_rad};
            pose_from_json(row, "source", source.pose.position, source.pose.yaw_rad);
            source.id = identity(row, index++);
            source.cabinet = scene.cabinet;
            if (row.contains("cabinet"))
            {
                SushiDSP::room::CabinetKind kind{};
                const std::string word = row.at("cabinet").get<std::string>();
                if (!SushiDSP::room::find_cabinet(word.c_str(), kind))
                {
                    throw std::runtime_error("RigController: unknown cabinet '" + word +
                                             "' in a preset's room");
                }
                source.cabinet = SushiDSP::room::cabinet_preset(kind).geometry;
            }
            if (!arrangement.add_source(source, reason))
            {
                throw std::runtime_error("RigController: a preset's room: " + reason);
            }
        }
        index = 0;
        for (const nlohmann::json& row : room.value("microphones", nlohmann::json::array()))
        {
            SushiDSP::Room::MicrophonePlacement microphone;
            microphone.pose = {scene.microphone_position, scene.microphone_yaw_rad};
            pose_from_json(row, "microphone", microphone.pose.position, microphone.pose.yaw_rad);
            if (row.contains("pattern"))
            {
                microphone.pattern =
                    value_of(K_PATTERN_WORDS, row.at("pattern").get<std::string>(), "pattern");
            }
            microphone.proximity = row.value("proximity", true);
            if (row.contains("model"))
            {
                const std::string word = row.at("model").get<std::string>();
                if (!SushiDSP::room::find_microphone(word.c_str(), microphone.kind))
                {
                    throw std::runtime_error("RigController: unknown microphone '" + word +
                                             "' in a preset's room");
                }
            }
            microphone.id = identity(row, index++);
            if (!arrangement.add_microphone(microphone, reason))
            {
                throw std::runtime_error("RigController: a preset's room: " + reason);
            }
        }
        return arrangement;
    }

    /// @brief Writes a rig's shape as the "shape" object of a preset, every slot as its node id.
    nlohmann::json shape_to_json(const RigShape& shape, const PresetIdBySlot& ids)
    {
        nlohmann::json json = nlohmann::json::object();
        json["chain_type"] = word_of(K_CHAIN_WORDS, shape.chain_type);
        nlohmann::json branches = nlohmann::json::array();
        for (const std::vector<ui::SlotId>& branch : shape.branches)
        {
            branches.push_back(slots_to_json(branch, ids));
        }
        json["branches"] = branches;
        json["post"] = slots_to_json(shape.post, ids);
        json["di"] = shape.di;
        json["di_position"] = word_of(K_DI_POSITION_WORDS, shape.di_position);

        nlohmann::json hidden = nlohmann::json::object();
        for (const HiddenSlotKey& row : K_HIDDEN_SLOT_KEYS)
        {
            const std::optional<ui::SlotId>& slot = shape.hidden.*(row.slot);
            const auto found = slot.has_value() ? ids.find(*slot) : ids.end();
            if (found != ids.end())
            {
                hidden[row.key] = found->second;
            }
        }
        nlohmann::json microphones = nlohmann::json::array();
        for (const MicrophoneSlots& slots : shape.hidden.microphones)
        {
            const auto microphone = ids.find(slots.microphone);
            const auto strip = ids.find(slots.strip);
            if (microphone != ids.end() && strip != ids.end())
            {
                nlohmann::json row = nlohmann::json::object();
                row["microphone"] = microphone->second;
                row["strip"] = strip->second;
                microphones.push_back(row);
            }
        }
        hidden["microphones"] = microphones;
        for (const FeederListKey& row : K_FEEDER_LIST_KEYS)
        {
            hidden[row.key] = slots_to_json(shape.hidden.*(row.list), ids);
        }
        json["hidden"] = hidden;
        return json;
    }

    /// @brief Reads a "shape" object written by shape_to_json. A node id the preset does not
    /// hold is dropped, so a hand-edited preset loses a slot rather than naming a wrong one.
    /// @throws std::runtime_error for a chain type or a DI position no word names.
    RigShape shape_from_json(const nlohmann::json& json, const SlotByPresetId& slots)
    {
        RigShape shape;
        shape.chain_type = value_of(K_CHAIN_WORDS, json.value("chain_type", std::string("single")),
                                    "chain type");
        for (const nlohmann::json& branch : json.value("branches", nlohmann::json::array()))
        {
            shape.branches.push_back(slots_from_json(branch, slots));
        }
        shape.post = slots_from_json(json.value("post", nlohmann::json::array()), slots);
        shape.di = json.value("di", false);
        shape.di_position =
            value_of(K_DI_POSITION_WORDS, json.value("di_position", std::string("after_effects")),
                     "DI position");

        const nlohmann::json hidden = json.value("hidden", nlohmann::json::object());
        for (const HiddenSlotKey& row : K_HIDDEN_SLOT_KEYS)
        {
            if (hidden.contains(row.key) && hidden.at(row.key).is_number_integer())
            {
                const auto found = slots.find(hidden.at(row.key).get<int>());
                if (found != slots.end())
                {
                    shape.hidden.*(row.slot) = found->second;
                }
            }
        }
        for (const nlohmann::json& row : hidden.value("microphones", nlohmann::json::array()))
        {
            const auto microphone = slots.find(row.value("microphone", -1));
            const auto strip = slots.find(row.value("strip", -1));
            if (microphone != slots.end() && strip != slots.end())
            {
                shape.hidden.microphones.push_back(
                    MicrophoneSlots{microphone->second, strip->second});
            }
        }
        for (const FeederListKey& row : K_FEEDER_LIST_KEYS)
        {
            shape.hidden.*(row.list) =
                slots_from_json(hidden.value(row.key, nlohmann::json::array()), slots);
        }
        return shape;
    }
} // namespace

RigPersistence::RigPersistence(const SushiDSP::PrepareInfo& info, SlotGraph& graph,
                               SessionTimeline& session)
    : info_(info), graph_(graph), session_(session)
{
}

std::string RigPersistence::serialize_nodes(
    std::unordered_map<SlotGraph::SlotId, SushiDSP::NodeHandle>& handles_out) const
{
    handles_out.clear();
    if (graph_.slot_order().empty())
    {
        return "{\"nodes\":[]}";
    }

    const std::unique_ptr<SushiDSP::RigGraph> graph = graph_.build_graph(false, handles_out);

    std::unordered_map<SushiDSP::NodeHandle, SlotGraph::SlotId> slot_by_handle;
    for (const auto& [slot, handle] : handles_out)
    {
        slot_by_handle[handle] = slot;
    }

    return SushiDSP::serialize_rig_to_json(
        *graph,
        [this, &slot_by_handle, &handles_out](SushiDSP::NodeHandle handle,
                                              nlohmann::json& node_json)
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
            if (slot->room_arrangement.has_value())
            {
                node_json["room"] = arrangement_to_json(*slot->room_arrangement, handles_out);
            }
            if (slot->bypassed)
            {
                node_json["bypassed"] = true;
            }
        });
}

std::string RigPersistence::save_to_json(PresetScope scope) const
{
    std::unordered_map<SlotGraph::SlotId, SushiDSP::NodeHandle> handles;
    nlohmann::json document = nlohmann::json::parse(serialize_nodes(handles));
    document["format"] = K_PRESET_FORMAT;
    if (!graph_.ir_folder().empty())
    {
        document["ir_folder"] = graph_.ir_folder();
    }
    if (scope == PresetScope::Session && !session_.take_folder().empty())
    {
        document["take_folder"] = session_.take_folder();
    }

    // The nodes' own "inputs" carry no port, so the edges are written again with theirs.
    nlohmann::json edges_json = nlohmann::json::array();
    for (const SlotGraph::EdgeRecord& edge : graph_.edge_records())
    {
        const auto from = handles.find(edge.from);
        const auto to = handles.find(edge.to);
        if (from == handles.end() || to == handles.end())
        {
            continue;
        }
        nlohmann::json edge_json = nlohmann::json::object();
        edge_json["from"] = from->second;
        edge_json["to"] = to->second;
        edge_json["port"] = edge.from_port;
        edges_json.push_back(edge_json);
    }
    document["edges"] = edges_json;
    if (graph_.shape().has_value())
    {
        document["shape"] = shape_to_json(*graph_.shape(), handles);
    }

    // A rig alone carries no lane, no loop and no tempo: those are the session's.
    if (scope == PresetScope::Session)
    {
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

        // The playback speed is written nowhere; see applications/host_gui/README.md.
        nlohmann::json tempo_json = nlohmann::json::object();
        tempo_json["bpm"] = session_.musical_clock().beats_per_minute();
        tempo_json["beats_per_bar"] = session_.musical_clock().beats_per_bar();
        tempo_json["beat_unit"] = session_.musical_clock().beat_unit();
        tempo_json["metronome_enabled"] = session_.metronome().enabled();
        tempo_json["metronome_level"] = session_.metronome().level();
        tempo_json["playback_speed"] = transport.speed();
        document["tempo"] = tempo_json;
    }

    return document.dump(2);
}

void RigPersistence::load_from_json(const std::string& json_text, PresetScope scope)
{
    // The scratch graph's pool is sized from the preset's node count; see applications/host_gui/README.md.
    const nlohmann::json document = nlohmann::json::parse(json_text);

    // Every track row is read and checked here, before the session is touched.
    const std::size_t track_count =
        std::min<std::size_t>(document.value("track_count", SessionTimeline::K_MAX_TRACKS),
                              SessionTimeline::K_MAX_TRACKS);
    const std::vector<TrackRow> track_rows = track_rows_from_json(document, track_count);

    // Format 2 is read node by node, with its own edges; format 1 is read whole and compiled.
    const int format = document.value("format", 1);
    if (format > K_PRESET_FORMAT || format < 1)
    {
        throw std::runtime_error("RigController: the preset is format " + std::to_string(format) +
                                 "; this build reads formats 1 to " +
                                 std::to_string(K_PRESET_FORMAT));
    }
    const bool with_shape = format >= 2;
    std::string nodes_text = json_text;
    if (with_shape)
    {
        nlohmann::json nodes_only = nlohmann::json::object();
        nodes_only["nodes"] = document.at("nodes");
        for (nlohmann::json& node_json : nodes_only["nodes"])
        {
            node_json["inputs"] = nlohmann::json::array();
        }
        nodes_text = nodes_only.dump();
    }

    const std::size_t node_count = document.at("nodes").size();
    SushiDSP::RigGraph scratch(info_.max_block, static_cast<std::uint32_t>(node_count + 1));
    std::unordered_map<SushiDSP::NodeHandle, std::string> cabinet_paths_by_handle;
    std::unordered_map<SushiDSP::NodeHandle, bool> bypassed_by_handle;
    std::unordered_map<SushiDSP::NodeHandle, nlohmann::json> rooms_by_handle;
    std::unordered_map<int, SushiDSP::NodeHandle> handle_by_preset_id;
    std::vector<SushiDSP::NodeHandle> legacy_rooms;
    std::optional<LegacyRoom> legacy;
    SushiDSP::deserialize_rig_from_json(
        scratch, nodes_text, info_,
        [&](SushiDSP::NodeHandle handle, const nlohmann::json& node_json)
        {
            handle_by_preset_id[node_json.at("id").get<int>()] = handle;
            if (node_json.contains("cabinet_ir"))
            {
                cabinet_paths_by_handle[handle] = node_json.at("cabinet_ir").get<std::string>();
            }
            if (node_json.contains("bypassed"))
            {
                bypassed_by_handle[handle] = node_json.at("bypassed").get<bool>();
            }
            const bool is_room = node_json.at("type").get<std::string>() == K_ROOM_TYPE;
            if (is_room && with_shape && node_json.contains("room"))
            {
                rooms_by_handle[handle] = node_json.at("room");
            }
            if (is_room && !with_shape)
            {
                // A format 1 room was a slot of the chain with its listener, level and mix.
                legacy_rooms.push_back(handle);
                if (!legacy.has_value())
                {
                    const nlohmann::json& params = node_json.at("params");
                    LegacyRoom room;
                    room.scene = node_json.contains("room")
                                     ? room_scene_from_json(node_json.at("room"))
                                     : SushiDSP::room::starter_scene();
                    room.ears = params.value("listener", 0.0f) < 0.5f;
                    room.level_db = params.value("level", 0.0f);
                    room.mix = params.value("mix", 1.0f);
                    legacy = room;
                }
            }
        });
    if (!with_shape)
    {
        scratch.compile(info_);
    }
    if (legacy_rooms.size() > 1)
    {
        SD_LOG_WARN(K_HOST_GUI_LOG_CATEGORY,
                    "the preset holds {} rooms; the first becomes the cabinet section's room and "
                    "the others are dropped",
                    legacy_rooms.size());
    }

    // Every room is read and checked here, before the rig is touched.
    for (const auto& [handle, room_json] : rooms_by_handle)
    {
        std::string reason;
        if (!arrangement_from_json(room_json, SlotByPresetId{}).is_valid(reason))
        {
            throw std::runtime_error("RigController: a preset's room: " + reason);
        }
    }

    graph_.clear();
    graph_.set_ir_folder(document.contains("ir_folder")
                             ? document.at("ir_folder").get<std::string>()
                             : std::string());
    // A rig loaded alone leaves the lanes, the loop and the tempo as they are.
    if (scope == PresetScope::Session)
    {
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
            session_.set_track_name(
                row.index,
                row.has_name ? row.name : SessionTimeline::path_stem(session_.track_path(row.index)));
            session_.set_track_color(
                row.index, row.has_color ? row.color : SessionTimeline::palette_color(row.index));
            session_.player().set_gain(row.index, row.gain);
            session_.player().set_pan(row.index, row.pan);
            session_.player().set_mute(row.index, row.muted);
            session_.player().set_solo(row.index, row.soloed);
        }

        session_.transport().clear_loop();
        if (document.contains("loop"))
        {
            const nlohmann::json& loop_json = document.at("loop");
            session_.transport().set_loop(loop_json.value("begin_frames", std::uint64_t{0}),
                                          loop_json.value("end_frames", std::uint64_t{0}));
        }

        // A session carries no playback speed, so every session opens at 1.0. Spec §8.
        session_.transport().set_speed(1.0);

        // A preset with no tempo object means 120 BPM in 4/4, click off; see applications/host_gui/README.md.
        const nlohmann::json tempo_json =
            document.contains("tempo") ? document.at("tempo") : nlohmann::json::object();
        session_.musical_clock().set_tempo(tempo_json.value("bpm", 120.0));
        session_.musical_clock().set_meter(tempo_json.value("beats_per_bar", std::uint32_t{4}),
                                           tempo_json.value("beat_unit", std::uint32_t{4}));
        session_.musical_clock().set_sample_rate(info_.sample_rate);
        session_.metronome().set_enabled(tempo_json.value("metronome_enabled", false));
        session_.metronome().set_level(tempo_json.value("metronome_level", 0.5f));
        session_.transport().set_speed(tempo_json.value("playback_speed", 1.0));
    }

    const auto is_legacy_room = [&legacy_rooms](SushiDSP::NodeHandle handle)
    {
        return std::find(legacy_rooms.begin(), legacy_rooms.end(), handle) != legacy_rooms.end();
    };

    std::unordered_map<SushiDSP::NodeHandle, SlotGraph::SlotId> handle_to_slot;
    for (SushiDSP::NodeHandle handle : scratch.nodes())
    {
        if (is_legacy_room(handle))
        {
            continue;
        }
        SushiDSP::AudioNode* node = scratch.node(handle);
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
        handle_to_slot[handle] = graph_.restore(snapshot, false);
    }

    SlotByPresetId slot_by_preset_id;
    for (const auto& [preset_id, handle] : handle_by_preset_id)
    {
        const auto slot = handle_to_slot.find(handle);
        if (slot != handle_to_slot.end())
        {
            slot_by_preset_id[preset_id] = slot->second;
        }
    }

    // A room's rows name their nodes, so its arrangement waits for every slot to have its id.
    for (const auto& [handle, room_json] : rooms_by_handle)
    {
        std::string reason;
        graph_.set_room_arrangement(handle_to_slot.at(handle),
                                    arrangement_from_json(room_json, slot_by_preset_id), reason);
    }

    if (with_shape && !document.contains("edges"))
    {
        for (const nlohmann::json& node_json : document.at("nodes"))
        {
            const auto to = slot_by_preset_id.find(node_json.at("id").get<int>());
            for (const nlohmann::json& input_id : node_json.at("inputs"))
            {
                const auto from = slot_by_preset_id.find(input_id.get<int>());
                if (from != slot_by_preset_id.end() && to != slot_by_preset_id.end())
                {
                    graph_.connect(from->second, to->second, false);
                }
            }
        }
    }
    if (with_shape)
    {
        for (const nlohmann::json& edge : document.value("edges", nlohmann::json::array()))
        {
            const auto from = slot_by_preset_id.find(edge.value("from", -1));
            const auto to = slot_by_preset_id.find(edge.value("to", -1));
            if (from != slot_by_preset_id.end() && to != slot_by_preset_id.end())
            {
                graph_.connect(from->second, to->second, false,
                               edge.value("port", std::uint32_t{0}));
            }
        }
        if (document.contains("shape"))
        {
            graph_.set_shape(shape_from_json(document.at("shape"), slot_by_preset_id));
        }
    }
    else
    {
        // A room that left the chain is bridged: what fed it feeds what it fed.
        for (SushiDSP::NodeHandle handle : scratch.nodes())
        {
            if (is_legacy_room(handle))
            {
                continue;
            }
            for (SushiDSP::NodeHandle upstream_handle : scratch.upstream_of(handle))
            {
                if (!is_legacy_room(upstream_handle))
                {
                    graph_.connect(handle_to_slot.at(upstream_handle), handle_to_slot.at(handle),
                                   false);
                    continue;
                }
                for (SushiDSP::NodeHandle before_room : scratch.upstream_of(upstream_handle))
                {
                    if (!is_legacy_room(before_room))
                    {
                        graph_.connect(handle_to_slot.at(before_room), handle_to_slot.at(handle),
                                       false);
                    }
                }
            }
        }
        if (legacy.has_value())
        {
            graph_.set_legacy_room(*legacy);
        }
    }

    graph_.publish();
}
