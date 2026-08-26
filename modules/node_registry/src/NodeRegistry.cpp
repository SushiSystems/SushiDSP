/**************************************************************************/
/* NodeRegistry.cpp                                                       */
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

#include "SushiDSP/node_registry/NodeRegistry.hpp"

#include <cstring>

#include "SushiDSP/jcm800/Jcm800.hpp"
#include "SushiDSP/tubescreamer/TubeScreamer.hpp"
#include "SushiDSP/routing/MixerNode.hpp"
#include "SushiDSP/routing/SplitterNode.hpp"

namespace SushiDSP
{
    const std::vector<NodeEntry>& node_registry()
    {
        static const std::vector<NodeEntry> entries{
            { &tube_screamer_descriptor(), NodeCategory{ "Distortion" } },
            { &jcm800_descriptor(), NodeCategory{ "Amp" } },
            { &splitter_descriptor(), NodeCategory{ "Routing" } },
            { &mixer_descriptor(), NodeCategory{ "Routing" } },
        };
        return entries;
    }

    std::unique_ptr<IAudioNode> make_node(const char* id, const PrepareInfo& info)
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
}
