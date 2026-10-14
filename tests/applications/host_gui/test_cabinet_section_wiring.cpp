/**************************************************************************/
/* test_cabinet_section_wiring.cpp                                        */
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
 * @file test_cabinet_section_wiring.cpp
 * @brief Tests the cabinet section's wiring and the mixer's strip list as data.
 * @author Mustafa Garip & Sushi Systems
 */

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "cabinet_section_wiring.hpp"
#include "rig_shape.hpp"
#include "ui/mixer_model.hpp"

namespace
{
    using Slot = ui::SlotId;

    // Paths end in 10, 11 and 12; microphone m is slot 20 + m and its strip 40 + m.
    constexpr Slot K_JOIN = 1;
    constexpr Slot K_ROOM = 2;
    constexpr Slot K_LISTENER = 3;
    constexpr Slot K_AMP_BUS = 4;
    constexpr Slot K_MASTER_STRIP = 5;
    constexpr Slot K_BUS = 6;
    constexpr Slot K_ROOM_SUM = 7;
    constexpr Slot K_ROOM_STRIP = 8;
    constexpr Slot K_DI_STRIP = 9;
    constexpr Slot K_MASTER_BUS = 13;

    /// @brief Returns a request with the slots every section has and nothing else.
    CabinetSectionRequest base_request()
    {
        CabinetSectionRequest request;
        request.join = K_JOIN;
        request.room = K_ROOM;
        request.listener_strip = K_LISTENER;
        request.amp_bus = K_AMP_BUS;
        request.master_strip = K_MASTER_STRIP;
        return request;
    }

    /// @brief Adds `count` microphones to a request, the first `room_count` of them marked room,
    /// and the slots their strips need.
    void add_microphones(CabinetSectionRequest& request, std::uint32_t count,
                         std::uint32_t room_count)
    {
        for (std::uint32_t index = 0; index < count; ++index)
        {
            request.microphones.push_back(
                MicrophoneWire{20 + index, 40 + index, index < room_count});
        }
        const CabinetSectionNeeds needs =
            CabinetSectionNeeds::of(request.microphones, request.di_strip.has_value(),
                                    request.di_position);
        if (needs.bus)
        {
            request.bus = K_BUS;
        }
        if (needs.room_strip)
        {
            request.room_sum = K_ROOM_SUM;
            request.room_strip = K_ROOM_STRIP;
        }
    }

    /// @brief Reports whether the wiring holds an edge.
    bool has_edge(const CabinetSectionWiring& wiring, Slot from, Slot to, std::uint32_t port = 0)
    {
        return std::any_of(wiring.edges.begin(), wiring.edges.end(),
                           [&](const CabinetSectionEdge& edge)
                           {
                               return edge.from == from && edge.to == to && edge.from_port == port;
                           });
    }

    /// @brief Returns the largest number of edges any one slot receives.
    std::size_t widest_fan_in(const CabinetSectionWiring& wiring)
    {
        std::map<Slot, std::size_t> count;
        std::size_t widest = 0;
        for (const CabinetSectionEdge& edge : wiring.edges)
        {
            widest = std::max(widest, ++count[edge.to]);
        }
        return widest;
    }
} // namespace

TEST(CabinetSectionWiring, CabinetSectionNeedsFollowTheMicrophonesAndTheDiChoice)
{
    const std::vector<MicrophoneWire> none;
    const std::vector<MicrophoneWire> close{{20, 40, false}};
    const std::vector<MicrophoneWire> overhead{{20, 40, false}, {21, 41, true}};

    const CabinetSectionNeeds bare = CabinetSectionNeeds::of(none, false, DiPosition::AfterEffects);
    EXPECT_FALSE(bare.bus);
    EXPECT_FALSE(bare.room_strip);
    EXPECT_FALSE(bare.di_strip);
    EXPECT_FALSE(bare.master_bus);

    EXPECT_TRUE(CabinetSectionNeeds::of(close, false, DiPosition::AfterEffects).bus);
    EXPECT_FALSE(CabinetSectionNeeds::of(close, false, DiPosition::AfterEffects).room_strip);
    EXPECT_TRUE(CabinetSectionNeeds::of(overhead, false, DiPosition::AfterEffects).room_strip);

    const CabinetSectionNeeds after = CabinetSectionNeeds::of(none, true, DiPosition::AfterEffects);
    EXPECT_TRUE(after.di_strip);
    EXPECT_TRUE(after.master_bus);
    const CabinetSectionNeeds before =
        CabinetSectionNeeds::of(none, true, DiPosition::BeforeEffects);
    EXPECT_TRUE(before.di_strip);
    EXPECT_FALSE(before.master_bus);
}

TEST(CabinetSectionWiring, CabinetSectionWiresOneCabinetAndTheListenerWithNoMicrophone)
{
    CabinetSectionRequest request = base_request();
    request.chain_edges = {{30, K_JOIN}};
    request.sources = {K_JOIN};

    const CabinetSectionWiring wiring = wire_cabinet_section(request);

    EXPECT_TRUE(has_edge(wiring, 30, K_JOIN));
    EXPECT_TRUE(has_edge(wiring, K_JOIN, K_ROOM));
    EXPECT_TRUE(has_edge(wiring, K_ROOM, K_LISTENER, 0));
    EXPECT_TRUE(has_edge(wiring, K_JOIN, K_AMP_BUS));
    EXPECT_TRUE(has_edge(wiring, K_LISTENER, K_AMP_BUS));
    EXPECT_TRUE(has_edge(wiring, K_AMP_BUS, K_MASTER_STRIP));
    EXPECT_EQ(wiring.edges.size(), 6u);
    EXPECT_EQ(wiring.amp_bus_feeders, (std::vector<Slot>{K_JOIN, K_LISTENER}));
    EXPECT_TRUE(wiring.bus_feeders.empty());
}

TEST(CabinetSectionWiring, CabinetSectionFeedsTheRoomOneSourcePerPathInOrder)
{
    CabinetSectionRequest request = base_request();
    request.sources = {10, 11, 12};

    const CabinetSectionWiring wiring = wire_cabinet_section(request);

    std::vector<Slot> into_room;
    for (const CabinetSectionEdge& edge : wiring.edges)
    {
        if (edge.to == K_ROOM)
        {
            into_room.push_back(edge.from);
        }
    }
    EXPECT_EQ(into_room, (std::vector<Slot>{10, 11, 12}));
}

TEST(CabinetSectionWiring, CabinetSectionGivesEachMicrophoneItsPortItsStripAndTheBus)
{
    CabinetSectionRequest request = base_request();
    request.sources = {K_JOIN};
    add_microphones(request, 2, 0);

    const CabinetSectionWiring wiring = wire_cabinet_section(request);

    EXPECT_TRUE(has_edge(wiring, K_ROOM, 20, 1));
    EXPECT_TRUE(has_edge(wiring, K_ROOM, 21, 2));
    EXPECT_TRUE(has_edge(wiring, 20, 40));
    EXPECT_TRUE(has_edge(wiring, 21, 41));
    EXPECT_TRUE(has_edge(wiring, 40, K_BUS));
    EXPECT_TRUE(has_edge(wiring, 41, K_BUS));
    EXPECT_TRUE(has_edge(wiring, K_BUS, K_AMP_BUS));
    EXPECT_EQ(wiring.bus_feeders, (std::vector<Slot>{40, 41}));
    EXPECT_EQ(wiring.amp_bus_feeders, (std::vector<Slot>{K_JOIN, K_BUS, K_LISTENER}));
}

TEST(CabinetSectionWiring, CabinetSectionSumsRoomMicrophonesOnTheRoomStrip)
{
    CabinetSectionRequest request = base_request();
    request.sources = {K_JOIN};
    add_microphones(request, 3, 1);

    const CabinetSectionWiring wiring = wire_cabinet_section(request);

    // Microphone 0 is marked room: its strip joins the Room sum, not the bus.
    EXPECT_TRUE(has_edge(wiring, 40, K_ROOM_SUM));
    EXPECT_FALSE(has_edge(wiring, 40, K_BUS));
    EXPECT_TRUE(has_edge(wiring, K_ROOM_SUM, K_ROOM_STRIP));
    EXPECT_TRUE(has_edge(wiring, K_ROOM_STRIP, K_BUS));
    EXPECT_EQ(wiring.room_sum_feeders, (std::vector<Slot>{40}));
    EXPECT_EQ(wiring.bus_feeders, (std::vector<Slot>{41, 42, K_ROOM_STRIP}));
}

TEST(CabinetSectionWiring, CabinetSectionNeverGivesANodeMoreThanEightInputs)
{
    for (std::uint32_t room_count = 0; room_count <= 8; ++room_count)
    {
        CabinetSectionRequest request = base_request();
        request.sources = {10, 11, 12};
        request.di_strip = K_DI_STRIP;
        request.di_position = DiPosition::BeforeEffects;
        add_microphones(request, 8, room_count);

        const CabinetSectionWiring wiring = wire_cabinet_section(request);

        EXPECT_LE(widest_fan_in(wiring), 8u) << room_count << " room microphones";
        EXPECT_EQ(wiring.bus_feeders.size() + wiring.room_sum_feeders.size(),
                  8u + (room_count > 0 ? 1u : 0u));
    }
}

TEST(CabinetSectionWiring, CabinetSectionPutsThePostStageAfterTheAmpBus)
{
    CabinetSectionRequest request = base_request();
    request.sources = {K_JOIN};
    request.post = {50, 51};

    const CabinetSectionWiring wiring = wire_cabinet_section(request);

    EXPECT_TRUE(has_edge(wiring, K_AMP_BUS, 50));
    EXPECT_TRUE(has_edge(wiring, 50, 51));
    EXPECT_TRUE(has_edge(wiring, 51, K_MASTER_STRIP));
    EXPECT_FALSE(has_edge(wiring, K_AMP_BUS, K_MASTER_STRIP));
}

TEST(CabinetSectionWiring, CabinetSectionJoinsTheDiBeforeOrAfterTheEffects)
{
    CabinetSectionRequest before = base_request();
    before.post = {50};
    before.di_strip = K_DI_STRIP;
    before.di_position = DiPosition::BeforeEffects;
    const CabinetSectionWiring early = wire_cabinet_section(before);
    EXPECT_TRUE(has_edge(early, K_DI_STRIP, K_AMP_BUS));
    EXPECT_TRUE(has_edge(early, 50, K_MASTER_STRIP));
    EXPECT_EQ(early.amp_bus_feeders, (std::vector<Slot>{K_JOIN, K_LISTENER, K_DI_STRIP}));
    EXPECT_TRUE(early.master_bus_feeders.empty());

    CabinetSectionRequest after = base_request();
    after.post = {50};
    after.di_strip = K_DI_STRIP;
    after.di_position = DiPosition::AfterEffects;
    after.master_bus = K_MASTER_BUS;
    const CabinetSectionWiring late = wire_cabinet_section(after);
    EXPECT_FALSE(has_edge(late, K_DI_STRIP, K_AMP_BUS));
    EXPECT_TRUE(has_edge(late, 50, K_MASTER_BUS));
    EXPECT_TRUE(has_edge(late, K_DI_STRIP, K_MASTER_BUS));
    EXPECT_TRUE(has_edge(late, K_MASTER_BUS, K_MASTER_STRIP));
    EXPECT_EQ(late.master_bus_feeders, (std::vector<Slot>{50, K_DI_STRIP}));
}

TEST(CabinetSectionWiring, MixerModelListsTheStripsInDrawingOrderWithTheirFlags)
{
    HiddenSlots slots;
    slots.room = K_ROOM;
    slots.listener_strip = K_LISTENER;
    slots.amp_bus = K_AMP_BUS;
    slots.master_strip = K_MASTER_STRIP;
    slots.bus = K_BUS;
    slots.room_sum = K_ROOM_SUM;
    slots.room_strip = K_ROOM_STRIP;
    slots.di_strip = K_DI_STRIP;
    slots.master_bus = K_MASTER_BUS;
    slots.join = K_JOIN;
    slots.microphones = {{20, 40}, {21, 41}};
    slots.room_sum_feeders = {40};
    slots.bus_feeders = {41, K_ROOM_STRIP};
    slots.amp_bus_feeders = {K_JOIN, K_BUS, K_LISTENER};
    slots.master_bus_feeders = {K_AMP_BUS, K_DI_STRIP};

    const std::vector<ui::MixerStrip> strips = ui::mixer_strips(slots);

    ASSERT_EQ(strips.size(), 8u);
    EXPECT_EQ(strips[0].label, "Mic 1");
    EXPECT_TRUE(strips[0].room_microphone);
    ASSERT_TRUE(strips[0].flags.has_value());
    EXPECT_EQ(strips[0].flags->bus, K_ROOM_SUM);
    EXPECT_EQ(strips[0].flags->input, 0u);

    EXPECT_EQ(strips[1].label, "Mic 2");
    EXPECT_FALSE(strips[1].room_microphone);
    ASSERT_TRUE(strips[1].flags.has_value());
    EXPECT_EQ(strips[1].flags->bus, K_BUS);
    EXPECT_EQ(strips[1].flags->input, 0u);
    EXPECT_EQ(strips[1].microphone, std::optional<Slot>(21));

    EXPECT_EQ(strips[2].kind, ui::MixerStripKind::Room);
    EXPECT_TRUE(strips[2].stereo);
    EXPECT_EQ(strips[2].flags->bus, K_BUS);
    EXPECT_EQ(strips[2].flags->input, 1u);

    EXPECT_EQ(strips[3].kind, ui::MixerStripKind::Listener);
    EXPECT_EQ(strips[3].flags->bus, K_AMP_BUS);
    EXPECT_EQ(strips[3].flags->input, 2u);

    EXPECT_EQ(strips[4].kind, ui::MixerStripKind::Cabinet);
    EXPECT_FALSE(strips[4].strip.has_value());
    EXPECT_EQ(strips[4].flags->input, 0u);

    EXPECT_EQ(strips[5].kind, ui::MixerStripKind::Bus);
    EXPECT_EQ(strips[5].flags->input, 1u);

    EXPECT_EQ(strips[6].kind, ui::MixerStripKind::Di);
    EXPECT_EQ(strips[6].flags->bus, K_MASTER_BUS);
    EXPECT_EQ(strips[6].flags->input, 1u);

    EXPECT_EQ(strips[7].kind, ui::MixerStripKind::Master);
    EXPECT_FALSE(strips[7].flags.has_value());
}

TEST(CabinetSectionWiring, MixerModelIsEmptyForARigWithNoCabinetSection)
{
    EXPECT_TRUE(ui::mixer_strips(HiddenSlots{}).empty());
}

TEST(CabinetSectionWiring, JoinContentNamesTheStripByWhetherACabinetFeedsIt)
{
    EXPECT_EQ(join_content_of(1, 1), JoinContent::Cabinets);
    EXPECT_EQ(join_content_of(3, 3), JoinContent::Cabinets);
    EXPECT_EQ(join_content_of(0, 1), JoinContent::Line);
    EXPECT_EQ(join_content_of(0, 0), JoinContent::Line);
    EXPECT_EQ(join_content_of(1, 3), JoinContent::Mixed);

    HiddenSlots slots;
    slots.amp_bus = 9;
    slots.join = 4;
    slots.amp_bus_feeders = {4};
    const auto join_strip = [&slots](JoinContent content)
    {
        return ui::mixer_strips(slots, content).front();
    };
    EXPECT_EQ(join_strip(JoinContent::Cabinets).label, "Cabinet");
    EXPECT_EQ(join_strip(JoinContent::Cabinets).kind, ui::MixerStripKind::Cabinet);
    EXPECT_EQ(join_strip(JoinContent::Line).label, "Line");
    EXPECT_EQ(join_strip(JoinContent::Line).kind, ui::MixerStripKind::Line);
    EXPECT_EQ(join_strip(JoinContent::Mixed).label, "Cabinet + Line");
    // Whatever it is called, the strip's mute and solo are the join's input of the amp bus.
    ASSERT_TRUE(join_strip(JoinContent::Line).flags.has_value());
    EXPECT_EQ(join_strip(JoinContent::Line).flags->bus, 9u);
}
