/**************************************************************************/
/* clap_extensions.cpp                                                    */
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
 * @file clap_extensions.cpp
 * @brief Implements the audio-ports, state, latency and params tables over ClapRigPlugin.
 * @author Mustafa Garip
 */

#include "clap_extensions.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include <clap/clap.h>

#include "clap_boundary.hpp"
#include "clap_plugin.hpp"
#include "rig_adapter.hpp"
#include "rig_parameter.hpp"

namespace SushiDSP::Plugin
{
    namespace
    {
        /// @brief Names the one audio port each direction has.
        constexpr clap_id K_MAIN_PORT_ID = 0;

        /// @brief Counts the bytes one clap_istream read asks for.
        constexpr std::size_t K_STREAM_CHUNK_BYTES = 4096;

        /// @brief Copies text into one of CLAP's fixed character arrays, cut to fit.
        template <std::size_t Size> void copy_text(char (&target)[Size], const std::string& text)
        {
            std::snprintf(target, Size, "%s", text.c_str());
        }

        /// @brief Finds a listed parameter by its id.
        /// @return The parameter, or nullptr when the rig holds no such parameter.
        const RigParameter* find_parameter(const clap_plugin_t* plugin, clap_id id)
        {
            const std::vector<RigParameter>& parameters = ClapRigPlugin::of(plugin).parameters();
            const auto found = std::find_if(parameters.begin(), parameters.end(),
                                            [id](const RigParameter& parameter)
                                            {
                                                return parameter.id == id;
                                            });
            return found != parameters.end() ? &*found : nullptr;
        }

        /// @brief Reports one stereo port in each direction.
        std::uint32_t CLAP_ABI audio_ports_count(const clap_plugin_t* plugin, bool is_input)
        {
            static_cast<void>(plugin);
            static_cast<void>(is_input);
            return 1;
        }

        /// @brief Describes the stereo port of one direction, paired in place with the other.
        bool CLAP_ABI audio_ports_get(const clap_plugin_t* plugin, std::uint32_t index,
                                      bool is_input, clap_audio_port_info_t* info)
        {
            static_cast<void>(plugin);
            if (index != 0 || info == nullptr)
            {
                return false;
            }
            return guarded(false,
                           [is_input, info]
                           {
                               info->id = K_MAIN_PORT_ID;
                               copy_text(info->name, is_input ? "Input" : "Output");
                               info->flags = CLAP_AUDIO_PORT_IS_MAIN;
                               info->channel_count = RigAdapter::K_CHANNELS;
                               info->port_type = CLAP_PORT_STEREO;
                               info->in_place_pair = K_MAIN_PORT_ID;
                               return true;
                           });
        }

        /// @brief Writes the rig's preset text to the host's stream, whole.
        bool CLAP_ABI state_save(const clap_plugin_t* plugin, const clap_ostream_t* stream)
        {
            return guarded(false,
                           [plugin, stream]
                           {
                               std::string text;
                               if (stream == nullptr || !ClapRigPlugin::of(plugin).save_rig(text))
                               {
                                   return false;
                               }
                               std::size_t offset = 0;
                               while (offset < text.size())
                               {
                                   const std::int64_t written = stream->write(
                                       stream, text.data() + offset, text.size() - offset);
                                   if (written <= 0)
                                   {
                                       return false;
                                   }
                                   offset += static_cast<std::size_t>(written);
                               }
                               return true;
                           });
        }

        /// @brief Reads the host's stream to its end and loads the text as the rig's preset.
        bool CLAP_ABI state_load(const clap_plugin_t* plugin, const clap_istream_t* stream)
        {
            return guarded(false,
                           [plugin, stream]
                           {
                               if (stream == nullptr)
                               {
                                   return false;
                               }
                               std::string text;
                               char chunk[K_STREAM_CHUNK_BYTES];
                               std::int64_t read = stream->read(stream, chunk, sizeof(chunk));
                               while (read > 0)
                               {
                                   text.append(chunk, static_cast<std::size_t>(read));
                                   read = stream->read(stream, chunk, sizeof(chunk));
                               }
                               return read == 0 && ClapRigPlugin::of(plugin).load_rig(text);
                           });
        }

        /// @brief Reports the rig's latency in frames.
        std::uint32_t CLAP_ABI latency_get(const clap_plugin_t* plugin)
        {
            return guarded(std::uint32_t{0},
                           [plugin] { return ClapRigPlugin::of(plugin).latency(); });
        }

        /// @brief Counts the rig's parameters.
        std::uint32_t CLAP_ABI params_count(const clap_plugin_t* plugin)
        {
            return static_cast<std::uint32_t>(ClapRigPlugin::of(plugin).parameters().size());
        }

        /// @brief Describes the parameter at one place in the rig's list.
        bool CLAP_ABI params_get_info(const clap_plugin_t* plugin, std::uint32_t index,
                                      clap_param_info_t* info)
        {
            return guarded(false,
                           [plugin, index, info]
                           {
                               const std::vector<RigParameter>& parameters =
                                   ClapRigPlugin::of(plugin).parameters();
                               if (info == nullptr || index >= parameters.size())
                               {
                                   return false;
                               }
                               const RigParameter& parameter = parameters[index];
                               info->id = parameter.id;
                               info->flags = CLAP_PARAM_IS_AUTOMATABLE;
                               if (parameter.is_stepped)
                               {
                                   info->flags |= CLAP_PARAM_IS_STEPPED;
                               }
                               info->cookie = nullptr;
                               copy_text(info->name, parameter.name);
                               copy_text(info->module, parameter.module);
                               info->min_value = parameter.minimum;
                               info->max_value = parameter.maximum;
                               info->default_value = parameter.default_value;
                               return true;
                           });
        }

        /// @brief Reads one parameter's current value.
        bool CLAP_ABI params_get_value(const clap_plugin_t* plugin, clap_id id, double* value)
        {
            return guarded(false,
                           [plugin, id, value]
                           {
                               const std::optional<float> current =
                                   ClapRigPlugin::of(plugin).parameter_value(id);
                               if (value == nullptr || !current.has_value())
                               {
                                   return false;
                               }
                               *value = *current;
                               return true;
                           });
        }

        /// @brief Writes a value as text, with the parameter's unit when it has one.
        bool CLAP_ABI params_value_to_text(const clap_plugin_t* plugin, clap_id id, double value,
                                           char* text, std::uint32_t capacity)
        {
            return guarded(false,
                           [plugin, id, value, text, capacity]
                           {
                               const RigParameter* parameter = find_parameter(plugin, id);
                               if (parameter == nullptr || text == nullptr || capacity == 0)
                               {
                                   return false;
                               }
                               const char* format = parameter->is_stepped ? "%.0f" : "%.3f";
                               const int written = std::snprintf(text, capacity, format, value);
                               if (written > 0 && !parameter->unit.empty() &&
                                   static_cast<std::uint32_t>(written) < capacity)
                               {
                                   std::snprintf(text + written,
                                                 capacity - static_cast<std::uint32_t>(written),
                                                 " %s", parameter->unit.c_str());
                               }
                               return written > 0;
                           });
        }

        /// @brief Reads the number a text begins with as a value of the parameter.
        bool CLAP_ABI params_text_to_value(const clap_plugin_t* plugin, clap_id id,
                                           const char* text, double* value)
        {
            return guarded(false,
                           [plugin, id, text, value]
                           {
                               if (find_parameter(plugin, id) == nullptr || text == nullptr ||
                                   value == nullptr)
                               {
                                   return false;
                               }
                               char* end = nullptr;
                               const double parsed = std::strtod(text, &end);
                               if (end == text)
                               {
                                   return false;
                               }
                               *value = parsed;
                               return true;
                           });
        }

        /// @brief Applies parameter events the host sends outside a block.
        void CLAP_ABI params_flush(const clap_plugin_t* plugin, const clap_input_events_t* in,
                                   const clap_output_events_t* out)
        {
            static_cast<void>(out);
            guarded(false,
                    [plugin, in]
                    {
                        ClapRigPlugin::of(plugin).flush_parameters(in);
                        return true;
                    });
        }

        /// @brief Holds the audio-ports extension's callbacks.
        const clap_plugin_audio_ports_t K_AUDIO_PORTS = {audio_ports_count, audio_ports_get};

        /// @brief Holds the state extension's callbacks.
        const clap_plugin_state_t K_STATE = {state_save, state_load};

        /// @brief Holds the latency extension's callbacks.
        const clap_plugin_latency_t K_LATENCY = {latency_get};

        /// @brief Holds the params extension's callbacks.
        const clap_plugin_params_t K_PARAMS = {
            params_count,         params_get_info,      params_get_value,
            params_value_to_text, params_text_to_value, params_flush,
        };

        /// @brief Pairs an extension's CLAP id with its callback table.
        struct Extension
        {
                const char* id;    ///< Names the extension to a host.
                const void* table; ///< Points at the extension's callbacks.
        };

        /// @brief Lists every extension the plugin answers.
        const Extension K_EXTENSIONS[] = {
            {CLAP_EXT_AUDIO_PORTS, &K_AUDIO_PORTS},
            {CLAP_EXT_STATE, &K_STATE},
            {CLAP_EXT_LATENCY, &K_LATENCY},
            {CLAP_EXT_PARAMS, &K_PARAMS},
        };
    } // namespace

    const void* find_extension(const char* id) noexcept
    {
        if (id == nullptr)
        {
            return nullptr;
        }
        for (const Extension& extension : K_EXTENSIONS)
        {
            if (std::strcmp(id, extension.id) == 0)
            {
                return extension.table;
            }
        }
        return nullptr;
    }
} // namespace SushiDSP::Plugin
