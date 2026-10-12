/**************************************************************************/
/* rig_topology.cpp                                                       */
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

/** @file rig_topology.cpp
 *  @brief Implements the host's chain-type model and the stage lists it routes.
 *  @author Mustafa Garip
 */

#include "rig_topology.hpp"

#include <array>
#include <algorithm>

namespace
{
    /// @brief Appends an edge to a list, skipping it when the list already holds it.
    /// @param edges The list to append to.
    /// @param edge The edge to append.
    void push_unique(std::vector<std::pair<ui::SlotId, ui::SlotId>>& edges,
                     std::pair<ui::SlotId, ui::SlotId> edge)
    {
        if (std::find(edges.begin(), edges.end(), edge) == edges.end())
        {
            edges.push_back(edge);
        }
    }

    /// @brief Appends the consecutive-neighbour edges of a linear slot list.
    /// @param edges The list to append to.
    /// @param slots The linear chain to link.
    void push_chain(std::vector<std::pair<ui::SlotId, ui::SlotId>>& edges,
                    const std::vector<ui::SlotId>& slots)
    {
        for (std::size_t i = 0; i + 1 < slots.size(); ++i)
        {
            push_unique(edges, {slots[i], slots[i + 1]});
        }
    }
} // namespace

std::size_t RigTopology::branch_count(ChainType type) noexcept
{
    switch (type)
    {
    case ChainType::Single:
        return 1;
    case ChainType::TwoWay:
        return 2;
    case ChainType::ThreeWay:
        return 3;
    case ChainType::Parallel:
        return 2;
    }
    return 1;
}

ChainType RigTopology::chain_type() const noexcept
{
    return type_;
}

ChainTypeChange RigTopology::set_chain_type(ChainType type)
{
    if (type == type_)
    {
        return {};
    }

    ChainTypeChange change;
    const std::size_t wanted = branch_count(type);

    for (std::size_t branch = wanted; branch < branches_.size(); ++branch)
    {
        change.discarded.insert(change.discarded.end(), branches_[branch].begin(),
                                branches_[branch].end());
    }
    for (std::size_t branch = branches_.size(); branch < wanted; ++branch)
    {
        change.opened.push_back(branch);
    }

    branches_.resize(wanted);
    type_ = type;
    return change;
}

const std::vector<RigTopology::SlotId>& RigTopology::stage(const StageAddress& address) const
{
    static const std::vector<SlotId> K_EMPTY;
    switch (address.kind)
    {
    case StageKind::Post:
        return post_;
    case StageKind::Branch:
        if (address.branch < branches_.size())
        {
            return branches_[address.branch];
        }
        return K_EMPTY;
    }
    return K_EMPTY;
}

bool RigTopology::is_visible(const StageAddress& address) const
{
    switch (address.kind)
    {
    // One path carries no master bus; applications/host_gui/README.md says why.
    case StageKind::Post:
        return branches_.size() > 1;
    case StageKind::Branch:
        return address.branch < branches_.size();
    }
    return false;
}

namespace
{
    /// @brief Resolves a stage address to the mutable list it names.
    /// @param address The stage to resolve.
    /// @param post The master bus list.
    /// @param branches The path lists.
    /// @return A pointer to the named list, or nullptr for an out-of-range path.
    std::vector<ui::SlotId>* resolve_stage(const StageAddress& address,
                                           std::vector<ui::SlotId>& post,
                                           std::vector<std::vector<ui::SlotId>>& branches)
    {
        switch (address.kind)
        {
        case StageKind::Post:
            return &post;
        case StageKind::Branch:
            if (address.branch < branches.size())
            {
                return &branches[address.branch];
            }
            return nullptr;
        }
        return nullptr;
    }
} // namespace

bool RigTopology::insert(const StageAddress& address, std::size_t index, SlotId slot)
{
    if (!is_visible(address))
    {
        return false;
    }
    std::vector<SlotId>* target = resolve_stage(address, post_, branches_);
    if (target == nullptr)
    {
        return false;
    }
    const std::size_t clamped = std::min(index, target->size());
    target->insert(target->begin() + static_cast<std::ptrdiff_t>(clamped), slot);
    return true;
}

std::optional<StageAddress> RigTopology::find(SlotId slot) const
{
    if (std::find(post_.begin(), post_.end(), slot) != post_.end())
    {
        return StageAddress{StageKind::Post, 0};
    }
    for (std::size_t branch = 0; branch < branches_.size(); ++branch)
    {
        if (std::find(branches_[branch].begin(), branches_[branch].end(), slot) !=
            branches_[branch].end())
        {
            return StageAddress{StageKind::Branch, branch};
        }
    }
    return std::nullopt;
}

bool RigTopology::move(SlotId slot, const StageAddress& address, std::size_t index)
{
    const std::optional<StageAddress> current = find(slot);
    if (!current.has_value() || !is_visible(address))
    {
        return false;
    }

    std::vector<SlotId>* source = resolve_stage(*current, post_, branches_);
    if (source == nullptr)
    {
        return false;
    }
    source->erase(std::find(source->begin(), source->end(), slot));

    std::vector<SlotId>* destination = resolve_stage(address, post_, branches_);
    if (destination == nullptr)
    {
        return false;
    }
    const std::size_t clamped = std::min(index, destination->size());
    destination->insert(destination->begin() + static_cast<std::ptrdiff_t>(clamped), slot);
    return true;
}

bool RigTopology::remove(SlotId slot)
{
    const std::optional<StageAddress> address = find(slot);
    if (!address.has_value() || !is_visible(*address))
    {
        return false;
    }
    std::vector<SlotId>* target = resolve_stage(*address, post_, branches_);
    if (target == nullptr)
    {
        return false;
    }
    target->erase(std::find(target->begin(), target->end(), slot));
    return true;
}

bool RigTopology::replace(SlotId from, SlotId to)
{
    const std::optional<StageAddress> address = find(from);
    if (!address.has_value())
    {
        return false;
    }
    std::vector<SlotId>* target = resolve_stage(*address, post_, branches_);
    if (target == nullptr)
    {
        return false;
    }
    *std::find(target->begin(), target->end(), from) = to;
    return true;
}

std::vector<RigTopology::SlotId> RigTopology::active_slots() const
{
    std::vector<SlotId> result;
    for (const std::vector<SlotId>& branch : branches_)
    {
        result.insert(result.end(), branch.begin(), branch.end());
    }
    if (branches_.size() > 1)
    {
        result.insert(result.end(), post_.begin(), post_.end());
    }
    return result;
}

std::vector<std::pair<RigTopology::SlotId, RigTopology::SlotId>>
RigTopology::edges(std::optional<SlotId> splitter, std::optional<SlotId> mixer) const
{
    std::vector<std::pair<SlotId, SlotId>> result;

    // One path needs no splitter and no mixer: it runs straight into the master bus.
    if (branches_.size() == 1)
    {
        push_chain(result, branches_.front());
        return result;
    }

    for (const std::vector<SlotId>& branch : branches_)
    {
        if (!splitter.has_value() || !mixer.has_value())
        {
            continue;
        }
        if (branch.empty())
        {
            push_unique(result, {*splitter, *mixer});
            continue;
        }
        push_unique(result, {*splitter, branch.front()});
        push_chain(result, branch);
        push_unique(result, {branch.back(), *mixer});
    }

    if (!post_.empty() && mixer.has_value())
    {
        push_unique(result, {*mixer, post_.front()});
    }
    push_chain(result, post_);

    return result;
}

std::vector<std::pair<RigTopology::SlotId, RigTopology::SlotId>>
RigTopology::parallel_edges(SlotId left_strip, SlotId right_strip, SlotId bus) const
{
    std::vector<std::pair<SlotId, SlotId>> result;
    if (branches_.size() != 2)
    {
        return result;
    }

    // An empty path feeds its strip directly; the applier binds that strip to its input channel.
    const std::array<SlotId, 2> strips = {left_strip, right_strip};
    for (std::size_t side = 0; side < 2; ++side)
    {
        const std::vector<SlotId>& path = branches_[side];
        if (!path.empty())
        {
            push_chain(result, path);
            push_unique(result, {path.back(), strips[side]});
        }
        push_unique(result, {strips[side], bus});
    }

    if (!post_.empty())
    {
        push_unique(result, {bus, post_.front()});
    }
    push_chain(result, post_);

    return result;
}

RigTopology RigTopology::from_linear_order(std::vector<SlotId> order)
{
    RigTopology topology;
    topology.branches_.assign(1, std::move(order));
    return topology;
}
