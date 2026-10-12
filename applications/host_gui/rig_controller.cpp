/**************************************************************************/
/* rig_controller.cpp                                                     */
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

/** @file rig_controller.cpp
 *  @brief Implements the rig facade as forwarders onto the three bricks it holds.
 *  @author Mustafa Garip
 */

#include "rig_controller.hpp"

#include <utility>

RigController::RigController(const SushiDSP::PrepareInfo& info)
    : info_(info), graph_(info), session_(info), persistence_(info, graph_, session_)
{
}

RigController::SlotId RigController::add_node(const std::string& descriptor_id, bool publish_now)
{
    return graph_.add_node(descriptor_id, publish_now);
}

void RigController::connect(SlotId from, SlotId to, bool publish_now, std::uint32_t from_port)
{
    graph_.connect(from, to, publish_now, from_port);
}

void RigController::disconnect(SlotId from, SlotId to, bool publish_now)
{
    graph_.disconnect(from, to, publish_now);
}

void RigController::remove(SlotId slot, bool publish_now)
{
    graph_.remove(slot, publish_now);
}

void RigController::publish()
{
    graph_.publish();
}

void RigController::set_bypassed(SlotId slot, bool bypassed)
{
    graph_.set_bypassed(slot, bypassed);
}

bool RigController::is_bypassed(SlotId slot) const
{
    return graph_.is_bypassed(slot);
}

RigController::SlotId RigController::append(const std::string& descriptor_id)
{
    return graph_.append(descriptor_id);
}

RigController::SlotId RigController::insert_before(std::optional<SlotId> following,
                                                   const std::string& descriptor_id)
{
    return graph_.insert_before(following, descriptor_id);
}

std::optional<RigController::SlotId> RigController::add_branch(SlotId splitter,
                                                               const std::string& descriptor_id)
{
    return graph_.add_branch(splitter, descriptor_id);
}

void RigController::move_before(SlotId moved, std::optional<SlotId> following)
{
    graph_.move_before(moved, following);
}

void RigController::set_parameter(SlotId slot, const char* param_id, float value)
{
    graph_.set_parameter(slot, param_id, value);
}

void RigController::set_input_channel(SlotId slot, std::uint32_t channel, bool publish_now)
{
    graph_.set_input_channel(slot, channel, publish_now);
}

float RigController::get_parameter(SlotId slot, const char* param_id) const
{
    return graph_.get_parameter(slot, param_id);
}

bool RigController::set_cabinet(SlotId slot, const std::string& path)
{
    return graph_.set_cabinet(slot, path);
}

void RigController::reclaim()
{
    graph_.reclaim();
    session_.reclaim();
}

std::size_t RigController::dropped_parameter_changes() const
{
    return graph_.dropped_parameter_changes();
}

std::vector<RigController::SlotId> RigController::slot_order() const
{
    return graph_.slot_order();
}

std::string RigController::descriptor_id(SlotId slot) const
{
    return graph_.descriptor_id(slot);
}

std::string RigController::cabinet_path(SlotId slot) const
{
    return graph_.cabinet_path(slot);
}

bool RigController::hosts_cabinet(SlotId slot) const
{
    return graph_.hosts_cabinet(slot);
}

bool RigController::hosts_room(SlotId slot) const
{
    return graph_.hosts_room(slot);
}

SushiDSP::room::RoomHost* RigController::room_host(SlotId slot)
{
    return graph_.room_host(slot);
}

SushiDSP::room::RoomScene RigController::room_scene(SlotId slot) const
{
    return graph_.room_scene(slot);
}

bool RigController::set_room_scene(SlotId slot, const SushiDSP::room::RoomScene& scene,
                                   std::string& error_out)
{
    return graph_.set_room_scene(slot, scene, error_out);
}

std::size_t RigController::room_publishes(SlotId slot) const
{
    return graph_.room_publishes(slot);
}

bool RigController::set_hrtf_path(const std::string& path, std::string& error_out)
{
    return graph_.set_hrtf_path(path, error_out);
}

std::string RigController::hrtf_path() const
{
    return graph_.hrtf_path();
}

std::vector<RigController::SlotId> RigController::upstream_of(SlotId slot) const
{
    return graph_.upstream_of(slot);
}

std::vector<std::pair<RigController::SlotId, RigController::SlotId>> RigController::edges() const
{
    return graph_.edges();
}

RoomLayout& RigController::room_layout() noexcept
{
    return graph_.room_layout();
}

const RoomLayout& RigController::room_layout() const noexcept
{
    return graph_.room_layout();
}

std::vector<SlotGraph::EdgeRecord> RigController::edge_records() const
{
    return graph_.edge_records();
}

std::optional<RigController::SlotSnapshot> RigController::snapshot(SlotId slot) const
{
    return graph_.snapshot(slot);
}

RigController::SlotId RigController::restore(const SlotSnapshot& snapshot, bool publish_now)
{
    return graph_.restore(snapshot, publish_now);
}

void RigController::clear_edges(bool publish_now)
{
    graph_.clear_edges(publish_now);
}

SushiDSP::RigGraph* RigController::active_graph()
{
    return graph_.active_graph();
}

SushiDSP::RtGraphSlot& RigController::graph_slot()
{
    return graph_.graph_slot();
}

void RigController::set_ir_folder(const std::string& path)
{
    graph_.set_ir_folder(path);
}

std::string RigController::ir_folder() const
{
    return graph_.ir_folder();
}

void RigController::set_input_source(RigInputSource source)
{
    graph_.set_input_source(source);
}

bool RigController::load_track(std::size_t index, const std::string& path, std::string& error_out)
{
    return session_.load_track(index, path, error_out);
}

bool RigController::place_clip(std::size_t track, const std::string& path,
                               std::uint64_t start_frame, std::string& error_out)
{
    return session_.place_clip(track, path, start_frame, error_out);
}

bool RigController::remove_clip(std::size_t track, std::size_t clip_index, std::string& error_out)
{
    return session_.remove_clip(track, clip_index, error_out);
}

bool RigController::move_clip(std::size_t track, std::size_t clip_index, std::uint64_t start_frame,
                              std::string& error_out)
{
    return session_.move_clip(track, clip_index, start_frame, error_out);
}

std::size_t RigController::clip_count(std::size_t track) const
{
    return session_.clip_count(track);
}

RigController::ClipPlacement RigController::clip(std::size_t track, std::size_t clip_index) const
{
    return session_.clip(track, clip_index);
}

void RigController::clear_track(std::size_t index)
{
    session_.clear_track(index);
}

bool RigController::track_is_loaded(std::size_t index) const
{
    return session_.track_is_loaded(index);
}

std::string RigController::track_name(std::size_t index) const
{
    return session_.track_name(index);
}

std::size_t RigController::track_count() const
{
    return session_.track_count();
}

bool RigController::add_track(std::string& error_out)
{
    return session_.add_track(error_out);
}

bool RigController::remove_track(std::size_t index, std::string& error_out)
{
    return session_.remove_track(index, error_out);
}

bool RigController::remove_track_and_files(std::size_t index, std::string& error_out)
{
    return session_.remove_track_and_files(index, error_out);
}

SessionTimeline::TrackColor RigController::track_color(std::size_t index) const
{
    return session_.track_color(index);
}

void RigController::set_track_color(std::size_t index, SessionTimeline::TrackColor color)
{
    session_.set_track_color(index, color);
}

void RigController::set_track_name(std::size_t index, const std::string& name)
{
    session_.set_track_name(index, name);
}

std::string RigController::track_path(std::size_t index) const
{
    return session_.track_path(index);
}

bool RigController::set_reamp_track(std::size_t index, std::string& error_out)
{
    return session_.set_reamp_track(index, error_out);
}

void RigController::clear_reamp_track()
{
    session_.clear_reamp_track();
}

void RigController::set_take_folder(const std::string& path)
{
    session_.set_take_folder(path);
}

std::string RigController::take_folder() const
{
    return session_.take_folder();
}

void RigController::set_record_track(std::optional<std::size_t> track)
{
    session_.set_record_track(track);
}

bool RigController::arm_recording(const std::string& take_name, std::string& error_out)
{
    return session_.arm_recording(take_name, error_out);
}

void RigController::stop_recording(std::string* error_out)
{
    session_.stop_recording(error_out);
}

bool RigController::poll_recording(std::string& error_out)
{
    return session_.poll_recording(error_out);
}

std::string RigController::save_to_json() const
{
    return persistence_.save_to_json();
}

void RigController::load_from_json(const std::string& json_text)
{
    persistence_.load_from_json(json_text);
}
