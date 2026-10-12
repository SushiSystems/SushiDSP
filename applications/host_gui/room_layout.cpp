/**************************************************************************/
/* room_layout.cpp                                                        */
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
 * @file room_layout.cpp
 * @brief Implements the host's record of the cabinet section.
 * @author Mustafa Garip
 */

#include "room_layout.hpp"

#include <algorithm>

#include "SushiDSP/room/starter_scene.hpp"

RoomLayout::RoomLayout() : shell_(SushiDSP::room::starter_scene())
{
    listener_.position = shell_.listener_position;
    listener_.yaw_rad = shell_.listener_yaw_rad;
}

const SushiDSP::room::RoomScene& RoomLayout::shell() const noexcept
{
    return shell_;
}

void RoomLayout::set_shell(const SushiDSP::room::RoomScene& shell)
{
    shell_ = shell;
}

const RoomPlacement& RoomLayout::listener() const noexcept
{
    return listener_;
}

void RoomLayout::set_listener(const RoomPlacement& place)
{
    listener_ = place;
}

const std::vector<RoomPlacement>& RoomLayout::cabinets() const noexcept
{
    return cabinets_;
}

bool RoomLayout::set_cabinet(std::size_t index, const RoomPlacement& place, std::string& reason_out)
{
    if (index >= K_MAX_CABINETS)
    {
        reason_out = "a room takes at most " + std::to_string(K_MAX_CABINETS) + " cabinets";
        return false;
    }
    if (index > cabinets_.size())
    {
        reason_out = "place the cabinet of path " + std::to_string(cabinets_.size()) + " first";
        return false;
    }
    if (index == cabinets_.size())
    {
        cabinets_.push_back(place);
    }
    else
    {
        cabinets_[index] = place;
    }
    return true;
}

const std::vector<MicrophoneSpot>& RoomLayout::microphones() const noexcept
{
    return microphones_;
}

std::uint32_t RoomLayout::add_microphone(MicrophoneSpot spot, std::string& reason_out)
{
    if (microphones_.size() >= K_MAX_MICROPHONES)
    {
        reason_out = "a room takes at most " + std::to_string(K_MAX_MICROPHONES) + " microphones";
        return 0;
    }
    spot.id = next_id_++;
    microphones_.push_back(spot);
    return spot.id;
}

bool RoomLayout::restore_microphone(const MicrophoneSpot& spot, std::string& reason_out)
{
    if (microphones_.size() >= K_MAX_MICROPHONES)
    {
        reason_out = "a room takes at most " + std::to_string(K_MAX_MICROPHONES) + " microphones";
        return false;
    }
    if (spot.id == 0 || find_microphone(spot.id) != nullptr)
    {
        reason_out = "microphone id " + std::to_string(spot.id) + " is taken or invalid";
        return false;
    }
    microphones_.push_back(spot);
    next_id_ = std::max(next_id_, spot.id + 1);
    return true;
}

bool RoomLayout::remove_microphone(std::uint32_t id)
{
    const auto it = std::find_if(microphones_.begin(), microphones_.end(),
                                 [id](const MicrophoneSpot& spot)
                                 {
                                     return spot.id == id;
                                 });
    if (it == microphones_.end())
    {
        return false;
    }
    microphones_.erase(it);
    return true;
}

bool RoomLayout::update_microphone(const MicrophoneSpot& spot)
{
    for (MicrophoneSpot& held : microphones_)
    {
        if (held.id == spot.id)
        {
            held = spot;
            return true;
        }
    }
    return false;
}

const MicrophoneSpot* RoomLayout::find_microphone(std::uint32_t id) const noexcept
{
    for (const MicrophoneSpot& spot : microphones_)
    {
        if (spot.id == id)
        {
            return &spot;
        }
    }
    return nullptr;
}

SushiDSP::room::RoomScene RoomLayout::pair_scene(std::size_t cabinet_index,
                                                 std::size_t receiver_index) const
{
    SushiDSP::room::RoomScene scene = shell_;
    if (cabinet_index < cabinets_.size())
    {
        scene.source_position = cabinets_[cabinet_index].position;
        scene.source_yaw_rad = cabinets_[cabinet_index].yaw_rad;
    }
    if (receiver_index == 0)
    {
        scene.listener_position = listener_.position;
        scene.listener_yaw_rad = listener_.yaw_rad;
    }
    else if (receiver_index - 1 < microphones_.size())
    {
        const MicrophoneSpot& spot = microphones_[receiver_index - 1];
        scene.microphone_position = spot.place.position;
        scene.microphone_yaw_rad = spot.place.yaw_rad;
        scene.microphone_pattern = spot.pattern;
    }
    return scene;
}
