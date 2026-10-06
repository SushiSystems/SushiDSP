/**************************************************************************/
/* PanelLibrary.cpp                                                       */
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
 * @file PanelLibrary.cpp
 * @brief Implements the front-panel table and the generic grid panel.
 * @author Mustafa Garip
 */

#include "ui/PanelLibrary.hpp"

#include <cmath>
#include <cstring>
#include <iterator>

namespace ui
{
    namespace
    {
        constexpr ControlPlacement kJcm800Controls[] = {
            { "presence",   ControlKind::Knob, { 0.34f, 0.78f }, 0.14f, "Presence"   },
            { "bass",       ControlKind::Knob, { 0.42f, 0.78f }, 0.14f, "Bass"       },
            { "middle",     ControlKind::Knob, { 0.50f, 0.78f }, 0.14f, "Middle"     },
            { "treble",     ControlKind::Knob, { 0.58f, 0.78f }, 0.14f, "Treble"     },
            { "master",     ControlKind::Knob, { 0.66f, 0.78f }, 0.14f, "Master"     },
            { "preamp",     ControlKind::Knob, { 0.74f, 0.78f }, 0.14f, "Pre Amp"    },
            { "input_trim", ControlKind::Knob, { 0.13f, 0.78f }, 0.10f, "Input Trim" },
            { "output",     ControlKind::Knob, { 0.21f, 0.78f }, 0.10f, "Output"     },
        };

        constexpr ControlPlacement kPeavey5150Controls[] = {
            { "input_trim", ControlKind::Knob, { 0.13f, 0.78f }, 0.10f, "Input Trim" },
            { "output",     ControlKind::Knob, { 0.21f, 0.78f }, 0.10f, "Output"     },
            { "pre_gain",   ControlKind::Knob, { 0.30f, 0.78f }, 0.14f, "Pre Gain"   },
            { "low",        ControlKind::Knob, { 0.38f, 0.78f }, 0.14f, "Low"        },
            { "mid",        ControlKind::Knob, { 0.46f, 0.78f }, 0.14f, "Mid"        },
            { "high",       ControlKind::Knob, { 0.54f, 0.78f }, 0.14f, "High"       },
            { "post_gain",  ControlKind::Knob, { 0.62f, 0.78f }, 0.14f, "Post Gain"  },
            { "resonance",  ControlKind::Knob, { 0.70f, 0.78f }, 0.14f, "Resonance"  },
            { "presence",   ControlKind::Knob, { 0.78f, 0.78f }, 0.14f, "Presence"   },
        };

        // Mark IIC+: knob rows left, equalizer faders right; see apps/host_gui/ui/README.md.
        constexpr ControlPlacement kMesaMark2cControls[] = {
            { "input_trim",  ControlKind::Knob,   { 0.055f, 0.665f }, 0.075f, "Trim"        },
            { "output",      ControlKind::Knob,   { 0.055f, 0.865f }, 0.075f, "Output"      },
            { "volume_1",    ControlKind::Knob,   { 0.115f, 0.665f }, 0.105f, "Volume 1"    },
            { "treble",      ControlKind::Knob,   { 0.205f, 0.665f }, 0.105f, "Treble"      },
            { "bass",        ControlKind::Knob,   { 0.295f, 0.665f }, 0.105f, "Bass"        },
            { "middle",      ControlKind::Knob,   { 0.385f, 0.665f }, 0.105f, "Middle"      },
            { "lead_drive",  ControlKind::Knob,   { 0.115f, 0.865f }, 0.105f, "Lead Drive"  },
            { "lead_master", ControlKind::Knob,   { 0.205f, 0.865f }, 0.105f, "Lead Master" },
            { "master_1",    ControlKind::Knob,   { 0.295f, 0.865f }, 0.105f, "Master 1"    },
            { "presence",    ControlKind::Knob,   { 0.385f, 0.865f }, 0.105f, "Presence"    },
            { "eq_80",       ControlKind::Slider, { 0.580f, 0.755f }, 0.240f, "80"          },
            { "eq_240",      ControlKind::Slider, { 0.655f, 0.755f }, 0.240f, "240"         },
            { "eq_750",      ControlKind::Slider, { 0.730f, 0.755f }, 0.240f, "750"         },
            { "eq_2200",     ControlKind::Slider, { 0.805f, 0.755f }, 0.240f, "2200"        },
            { "eq_6600",     ControlKind::Slider, { 0.880f, 0.755f }, 0.240f, "6600"        },
            { "bright",      ControlKind::Switch, { 0.115f, 0.480f }, 0.060f, "Bright"      },
            { "deep",        ControlKind::Switch, { 0.295f, 0.480f }, 0.060f, "Deep"        },
            { "eq_mode",     ControlKind::Switch, { 0.730f, 0.480f }, 0.060f, "EQ"          },
        };

        constexpr ControlPlacement kDiezelVh4Controls[] = {
            { "input_trim", ControlKind::Knob, { 0.13f, 0.78f }, 0.10f, "Input Trim" },
            { "output",     ControlKind::Knob, { 0.21f, 0.78f }, 0.10f, "Output"     },
            { "gain",       ControlKind::Knob, { 0.28f, 0.78f }, 0.14f, "Gain"       },
            { "bass",       ControlKind::Knob, { 0.35f, 0.78f }, 0.14f, "Bass"       },
            { "middle",     ControlKind::Knob, { 0.42f, 0.78f }, 0.14f, "Middle"     },
            { "treble",     ControlKind::Knob, { 0.49f, 0.78f }, 0.14f, "Treble"     },
            { "volume",     ControlKind::Knob, { 0.56f, 0.78f }, 0.14f, "Volume"     },
            { "deep",       ControlKind::Knob, { 0.63f, 0.78f }, 0.14f, "Deep"       },
            { "presence",   ControlKind::Knob, { 0.70f, 0.78f }, 0.14f, "Presence"   },
            { "master",     ControlKind::Knob, { 0.77f, 0.78f }, 0.14f, "Master"     },
        };

        constexpr ControlPlacement kTubeScreamerControls[] = {
            { "level",      ControlKind::Knob, { 0.50f, 0.13f }, 0.13f, "Level"      },
            { "drive",      ControlKind::Knob, { 0.24f, 0.22f }, 0.13f, "Drive"      },
            { "tone",       ControlKind::Knob, { 0.76f, 0.22f }, 0.13f, "Tone"       },
            { "input_trim", ControlKind::Knob, { 0.50f, 0.36f }, 0.09f, "Input Trim" },
        };

        constexpr ControlPlacement kMixerControls[] = {
            { "gain_0", ControlKind::Knob, { 0.10f, 0.55f }, 0.16f, "1" },
            { "gain_1", ControlKind::Knob, { 0.21f, 0.55f }, 0.16f, "2" },
            { "gain_2", ControlKind::Knob, { 0.33f, 0.55f }, 0.16f, "3" },
            { "gain_3", ControlKind::Knob, { 0.44f, 0.55f }, 0.16f, "4" },
            { "gain_4", ControlKind::Knob, { 0.56f, 0.55f }, 0.16f, "5" },
            { "gain_5", ControlKind::Knob, { 0.67f, 0.55f }, 0.16f, "6" },
            { "gain_6", ControlKind::Knob, { 0.79f, 0.55f }, 0.16f, "7" },
            { "gain_7", ControlKind::Knob, { 0.90f, 0.55f }, 0.16f, "8" },
        };

        constexpr ControlPlacement kPanControls[] = {
            { "pan", ControlKind::Knob, { 0.50f, 0.58f }, 0.42f, "Pan" },
        };

        /// @brief Hides the gain knobs of mixer inputs nothing is connected to.
        bool mixer_control_visible(std::size_t control_index, std::size_t upstream_count)
        {
            return control_index < upstream_count;
        }
    }

    const std::vector<PanelDescriptor>& panel_table()
    {
        static const std::vector<PanelDescriptor> table{
            { "sushidsp.jcm800", { 900.0f, 420.0f },
              { rgba(26, 26, 28), rgba(158, 124, 58), rgba(214, 168, 74), rgba(28, 22, 8), nullptr },
              kJcm800Controls, std::size(kJcm800Controls), nullptr },
            { "sushidsp.peavey5150", { 900.0f, 420.0f },
              { rgba(20, 20, 22), rgba(120, 120, 128), rgba(200, 200, 210), rgba(10, 10, 12), nullptr },
              kPeavey5150Controls, std::size(kPeavey5150Controls), nullptr },
            { "sushidsp.mesa_mark2c", { 900.0f, 420.0f },
              { rgba(14, 13, 12), rgba(212, 202, 178), rgba(198, 158, 74), rgba(30, 25, 16), nullptr },
              kMesaMark2cControls, std::size(kMesaMark2cControls), nullptr },
            { "sushidsp.diezel_vh4", { 900.0f, 420.0f },
              { rgba(18, 20, 26), rgba(50, 70, 120), rgba(120, 160, 230), rgba(8, 10, 16), nullptr },
              kDiezelVh4Controls, std::size(kDiezelVh4Controls), nullptr },
            { "sushidsp.tubescreamer", { 320.0f, 560.0f },
              { rgba(46, 112, 58), rgba(32, 78, 42), rgba(226, 214, 178), rgba(232, 232, 224), nullptr },
              kTubeScreamerControls, std::size(kTubeScreamerControls), nullptr },
            { "sushidsp.splitter", { 320.0f, 200.0f },
              { rgba(38, 38, 44), rgba(28, 28, 32), rgba(214, 168, 74), rgba(228, 228, 232), nullptr },
              nullptr, 0, nullptr },
            { "sushidsp.mixer", { 640.0f, 300.0f },
              { rgba(38, 38, 44), rgba(28, 28, 32), rgba(214, 168, 74), rgba(228, 228, 232), nullptr },
              kMixerControls, std::size(kMixerControls), &mixer_control_visible },
            { "sushidsp.pan", { 320.0f, 200.0f },
              { rgba(38, 38, 44), rgba(28, 28, 32), rgba(214, 168, 74), rgba(228, 228, 232), nullptr },
              kPanControls, std::size(kPanControls), nullptr },
        };
        return table;
    }

    const PanelDescriptor* panel_for(const char* descriptor_id)
    {
        for (const PanelDescriptor& panel : panel_table())
        {
            if (std::strcmp(panel.descriptor_id, descriptor_id) == 0)
            {
                return &panel;
            }
        }
        return nullptr;
    }

    PanelDescriptor auto_panel(const SushiDSP::NodeDescriptor& node,
                               std::vector<ControlPlacement>& storage)
    {
        const std::size_t count = node.parameter_count;
        const std::size_t columns = count == 0
            ? 1
            : static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(count))));
        const std::size_t rows = count == 0 ? 1 : (count + columns - 1) / columns;

        storage.clear();
        storage.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            const SushiDSP::ParameterDescriptor& param = node.parameters[i];
            ControlKind kind = ControlKind::Knob;
            if (param.kind == SushiDSP::ParamKind::Boolean)
            {
                kind = ControlKind::Switch;
            }
            else if (param.kind == SushiDSP::ParamKind::Choice)
            {
                kind = ControlKind::Readout;
            }

            const std::size_t column = i % columns;
            const std::size_t row = i / columns;
            const float x = (static_cast<float>(column) + 0.5f) / static_cast<float>(columns);
            const float y = (static_cast<float>(row) + 0.5f) / static_cast<float>(rows);
            storage.push_back({ param.id, kind, { x, y }, 0.55f / static_cast<float>(rows), nullptr });
        }

        return PanelDescriptor{
            node.id,
            { 200.0f * static_cast<float>(columns), 160.0f * static_cast<float>(rows) },
            { rgba(38, 38, 44), rgba(28, 28, 32), rgba(214, 168, 74), rgba(228, 228, 232), nullptr },
            storage.data(),
            storage.size(),
            nullptr
        };
    }
}
