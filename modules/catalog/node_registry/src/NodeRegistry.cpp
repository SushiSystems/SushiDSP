/****************************************************************/
/* NodeRegistry.cpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#include "SushiDSP/node_registry/NodeRegistry.hpp"

#include <cstring>

#include "SushiDSP/cabinet/CabinetNode.hpp"
#include "SushiDSP/dd3/Dd3Delay.hpp"
#include "SushiDSP/diezel_vh4/DiezelVh4.hpp"
#include "SushiDSP/jcm800/Jcm800.hpp"
#include "SushiDSP/mesa_mark2c/MesaMark2c.hpp"
#include "SushiDSP/peavey5150/Peavey5150.hpp"
#include "SushiDSP/room/RoomNode.hpp"
#include "SushiDSP/tubescreamer/TubeScreamer.hpp"
#include "SushiDSP/routing/MixerNode.hpp"
#include "SushiDSP/routing/PanNode.hpp"
#include "SushiDSP/routing/SplitterNode.hpp"

namespace SushiDSP
{
    const std::vector<NodeEntry>& node_registry()
    {
        static const std::vector<NodeEntry> entries{
            { &tube_screamer_descriptor(), NodeCategory{ "Distortion" } },
            { &jcm800_descriptor(), NodeCategory{ "Amp" } },
            { &peavey5150_descriptor(), NodeCategory{ "Amp" } },
            { &mesa_mark2c_descriptor(), NodeCategory{ "Amp" } },
            { &diezel_vh4_descriptor(), NodeCategory{ "Amp" } },
            { &dd3_descriptor(), NodeCategory{ "Delay" } },
            { &splitter_descriptor(), NodeCategory{ "Routing" } },
            { &mixer_descriptor(), NodeCategory{ "Routing" } },
            { &pan_descriptor(), NodeCategory{ "Routing" } },
            { &room::room_descriptor(), NodeCategory{ "Room" } },
            { &cabinet_descriptor(), NodeCategory{ "Cabinet" } },
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
