/**************************************************************************/
/* node_registry.cpp                                                      */
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
 * @file node_registry.cpp
 * @brief Implements the node catalog and the factory that builds a node by id.
 * @author Mustafa Garip
 */

#include "SushiDSP/node_registry/node_registry.hpp"

#include <cstring>

#include "SushiDSP/cabinet/cabinet_node.hpp"
#include "SushiDSP/dd3/dd3_delay.hpp"
#include "SushiDSP/diezel_vh4/diezel_vh4.hpp"
#include "SushiDSP/jcm800/jcm800.hpp"
#include "SushiDSP/mesa_mark2c/mesa_mark2c.hpp"
#include "SushiDSP/peavey5150/peavey5150.hpp"
#include "SushiDSP/microphone/microphone_node.hpp"
#include "SushiDSP/room/room_node.hpp"
#include "SushiDSP/tubescreamer/tube_screamer.hpp"
#include "SushiDSP/routing/bus_node.hpp"
#include "SushiDSP/routing/channel_strip_node.hpp"
#include "SushiDSP/routing/mixer_node.hpp"
#include "SushiDSP/routing/pan_node.hpp"
#include "SushiDSP/routing/splitter_node.hpp"
#include "SushiDSP/routing/stereo_strip_node.hpp"

namespace SushiDSP
{
    const std::vector<NodeEntry>& node_registry()
    {
        static const std::vector<NodeEntry> entries{
            {&tube_screamer_descriptor(), NodeCategory{"Distortion"}},
            {&jcm800_descriptor(), NodeCategory{"Amp"}},
            {&peavey5150_descriptor(), NodeCategory{"Amp"}},
            {&mesa_mark2c_descriptor(), NodeCategory{"Amp"}},
            {&diezel_vh4_descriptor(), NodeCategory{"Amp"}},
            {&dd3_descriptor(), NodeCategory{"Delay"}},
            {&splitter_descriptor(), NodeCategory{"Routing"}},
            {&mixer_descriptor(), NodeCategory{"Routing"}},
            {&pan_descriptor(), NodeCategory{"Routing"}},
            {&channel_strip_descriptor(), NodeCategory{"Routing"}},
            {&stereo_strip_descriptor(), NodeCategory{"Routing"}},
            {&bus_descriptor(), NodeCategory{"Routing"}},
            {&room::room_descriptor(), NodeCategory{"Room"}},
            {&microphone_descriptor(), NodeCategory{"Room"}},
            {&cabinet_descriptor(), NodeCategory{"Cabinet"}},
        };
        return entries;
    }

    std::unique_ptr<AudioNode> make_node(const char* id, const PrepareInfo& info)
    {
        for (const NodeEntry& entry : node_registry())
        {
            if (std::strcmp(entry.descriptor->id, id) == 0)
            {
                return entry.descriptor->make(info);
            }
        }
        return nullptr;
    }
} // namespace SushiDSP
