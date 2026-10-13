/**************************************************************************/
/* room_arrangement.cpp                                                   */
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
 * @file room_arrangement.cpp
 * @brief Implements the room arrangement and the field-by-field scene comparison.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/room_arrangement.hpp"

#include <algorithm>

#include "SushiDSP/room/starter_scene.hpp"

namespace SushiDSP::Room
{
    namespace
    {
        /// @brief Reports whether two points are the same point.
        bool same_point(const room::Vec3& left, const room::Vec3& right) noexcept
        {
            return left.x == right.x && left.y == right.y && left.z == right.z;
        }

        /// @brief Finds the entry of a list that carries an identity.
        /// @return An iterator to the entry, or the list's end.
        template <typename List>
        auto find_by_id(List& list, std::uint64_t id) noexcept
        {
            return std::find_if(list.begin(), list.end(),
                                [id](const auto& entry)
                                {
                                    return entry.id == id;
                                });
        }
    } // namespace

    Arrangement::Arrangement() : shell_(room::starter_scene())
    {
        listener_ = Pose{shell_.listener_position, shell_.listener_yaw_rad};
    }

    Arrangement Arrangement::from_scene(const room::RoomScene& scene)
    {
        Arrangement arrangement;
        arrangement.shell_ = scene;
        arrangement.listener_ = Pose{scene.listener_position, scene.listener_yaw_rad};
        arrangement.sources_.push_back(
            SourcePlacement{0u, Pose{scene.source_position, scene.source_yaw_rad}, scene.cabinet});
        return arrangement;
    }

    const room::RoomScene& Arrangement::shell() const noexcept
    {
        return shell_;
    }

    void Arrangement::set_shell(const room::RoomScene& shell)
    {
        shell_ = shell;
    }

    const Pose& Arrangement::listener() const noexcept
    {
        return listener_;
    }

    void Arrangement::set_listener(const Pose& pose) noexcept
    {
        listener_ = pose;
    }

    const std::vector<SourcePlacement>& Arrangement::sources() const noexcept
    {
        return sources_;
    }

    bool Arrangement::add_source(const SourcePlacement& source, std::string& reason_out)
    {
        if (sources_.size() >= K_MAX_SOURCES)
        {
            reason_out = "A room takes at most " + std::to_string(K_MAX_SOURCES) + " cabinets.";
            return false;
        }
        if (find_source(source.id) != nullptr)
        {
            reason_out = "The room already holds cabinet " + std::to_string(source.id) + ".";
            return false;
        }
        sources_.push_back(source);
        return true;
    }

    bool Arrangement::move_source(std::uint64_t id, const Pose& pose) noexcept
    {
        const auto found = find_by_id(sources_, id);
        if (found == sources_.end())
        {
            return false;
        }
        found->pose = pose;
        return true;
    }

    bool Arrangement::set_source_cabinet(std::uint64_t id,
                                         const room::CabinetGeometry& cabinet) noexcept
    {
        for (SourcePlacement& source : sources_)
        {
            if (source.id == id)
            {
                source.cabinet = cabinet;
                return true;
            }
        }
        return false;
    }

    bool Arrangement::remove_source(std::uint64_t id) noexcept
    {
        const auto found = find_by_id(sources_, id);
        if (found == sources_.end())
        {
            return false;
        }
        sources_.erase(found);
        return true;
    }

    const SourcePlacement* Arrangement::find_source(std::uint64_t id) const noexcept
    {
        const auto found = find_by_id(sources_, id);
        return found == sources_.end() ? nullptr : &*found;
    }

    const std::vector<MicrophonePlacement>& Arrangement::microphones() const noexcept
    {
        return microphones_;
    }

    bool Arrangement::add_microphone(const MicrophonePlacement& microphone,
                                     std::string& reason_out)
    {
        if (microphones_.size() >= K_MAX_MICROPHONES)
        {
            reason_out =
                "A room takes at most " + std::to_string(K_MAX_MICROPHONES) + " microphones.";
            return false;
        }
        if (find_microphone(microphone.id) != nullptr)
        {
            reason_out =
                "The room already holds microphone " + std::to_string(microphone.id) + ".";
            return false;
        }
        microphones_.push_back(microphone);
        return true;
    }

    bool Arrangement::update_microphone(const MicrophonePlacement& microphone) noexcept
    {
        const auto found = find_by_id(microphones_, microphone.id);
        if (found == microphones_.end())
        {
            return false;
        }
        *found = microphone;
        return true;
    }

    bool Arrangement::remove_microphone(std::uint64_t id) noexcept
    {
        const auto found = find_by_id(microphones_, id);
        if (found == microphones_.end())
        {
            return false;
        }
        microphones_.erase(found);
        return true;
    }

    const MicrophonePlacement* Arrangement::find_microphone(std::uint64_t id) const noexcept
    {
        const auto found = find_by_id(microphones_, id);
        return found == microphones_.end() ? nullptr : &*found;
    }

    void Arrangement::rename(std::uint64_t from, std::uint64_t to) noexcept
    {
        for (SourcePlacement& source : sources_)
        {
            if (source.id == from)
            {
                source.id = to;
            }
        }
        for (MicrophonePlacement& microphone : microphones_)
        {
            if (microphone.id == from)
            {
                microphone.id = to;
            }
        }
    }

    room::RoomScene Arrangement::pair_scene(std::size_t source, std::size_t receiver) const
    {
        room::RoomScene scene = shell_;
        scene.listener_position = listener_.position;
        scene.listener_yaw_rad = listener_.yaw_rad;
        // The receiver a pair does not read follows the listener, so a stale pose never refuses it.
        scene.microphone_position = listener_.position;
        scene.microphone_yaw_rad = listener_.yaw_rad;
        if (source < sources_.size())
        {
            scene.source_position = sources_[source].pose.position;
            scene.source_yaw_rad = sources_[source].pose.yaw_rad;
            scene.cabinet = sources_[source].cabinet;
        }
        else
        {
            scene.source_position = listener_.position;
            scene.source_yaw_rad = listener_.yaw_rad;
        }
        if (receiver >= 1 && receiver - 1 < microphones_.size())
        {
            const MicrophonePlacement& microphone = microphones_[receiver - 1];
            scene.microphone_position = microphone.pose.position;
            scene.microphone_yaw_rad = microphone.pose.yaw_rad;
            scene.microphone_pattern = microphone.pattern;
            scene.microphone_proximity = microphone.proximity;
        }
        return scene;
    }

    bool Arrangement::is_valid(std::string& reason_out) const
    {
        if (sources_.empty())
        {
            return pair_scene(0, 0).is_valid(reason_out);
        }
        for (std::size_t source = 0; source < sources_.size(); ++source)
        {
            for (std::size_t receiver = 0; receiver <= microphones_.size(); ++receiver)
            {
                std::string why;
                if (!pair_scene(source, receiver).is_valid(why))
                {
                    reason_out = "Cabinet " + std::to_string(source + 1) + ", " +
                                 (receiver == 0 ? std::string("listener")
                                                : "microphone " + std::to_string(receiver)) +
                                 ": " + why;
                    return false;
                }
            }
        }
        return true;
    }

    bool shells_equal(const room::RoomScene& left, const room::RoomScene& right) noexcept
    {
        if (!same_point(left.dimensions, right.dimensions) || left.surfaces != right.surfaces ||
            left.scattering != right.scattering || left.patch_count != right.patch_count ||
            left.furnishing_count != right.furnishing_count ||
            left.built_room_loss != right.built_room_loss)
        {
            return false;
        }
        for (std::uint32_t index = 0; index < left.patch_count; ++index)
        {
            const room::RoomScene::Patch& a = left.patches[index];
            const room::RoomScene::Patch& b = right.patches[index];
            if (a.surface != b.surface || a.material != b.material || a.area_m2 != b.area_m2)
            {
                return false;
            }
        }
        for (std::uint32_t index = 0; index < left.furnishing_count; ++index)
        {
            const room::RoomScene::Furnishing& a = left.furnishings[index];
            const room::RoomScene::Furnishing& b = right.furnishings[index];
            const bool same_place =
                a.placed == b.placed && a.x_m == b.x_m && a.y_m == b.y_m && a.yaw_rad == b.yaw_rad;
            if (a.item != b.item || a.quantity != b.quantity || !same_place)
            {
                return false;
            }
        }
        return room::same_geometry(left.cabinet, right.cabinet);
    }

    bool scenes_equal(const room::RoomScene& left, const room::RoomScene& right) noexcept
    {
        return shells_equal(left, right) &&
               same_point(left.source_position, right.source_position) &&
               left.source_yaw_rad == right.source_yaw_rad &&
               same_point(left.listener_position, right.listener_position) &&
               left.listener_yaw_rad == right.listener_yaw_rad &&
               same_point(left.microphone_position, right.microphone_position) &&
               left.microphone_yaw_rad == right.microphone_yaw_rad &&
               left.microphone_pattern == right.microphone_pattern &&
               left.microphone_proximity == right.microphone_proximity;
    }
} // namespace SushiDSP::Room
