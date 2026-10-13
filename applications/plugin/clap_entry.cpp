/**************************************************************************/
/* clap_entry.cpp                                                         */
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
 * @file clap_entry.cpp
 * @brief Defines the CLAP entry point and the factory that lists and builds the rig plugin.
 * @author Mustafa Garip
 */

#include <cstdint>
#include <cstring>

#include <clap/clap.h>

#include "clap_plugin.hpp"

namespace SushiDSP::Plugin
{
    namespace
    {
        /// @brief Counts the plugins the factory lists.
        std::uint32_t CLAP_ABI factory_plugin_count(const clap_plugin_factory_t* factory)
        {
            static_cast<void>(factory);
            return 1;
        }

        /// @brief Returns the rig plugin's descriptor for index 0.
        const clap_plugin_descriptor_t* CLAP_ABI
        factory_plugin_descriptor(const clap_plugin_factory_t* factory, std::uint32_t index)
        {
            static_cast<void>(factory);
            return index == 0 ? &ClapRigPlugin::descriptor() : nullptr;
        }

        /// @brief Builds the rig plugin for a host whose CLAP version this build can speak to.
        /// @return The instance, or nullptr for another plugin id or an incompatible host.
        const clap_plugin_t* CLAP_ABI factory_create_plugin(const clap_plugin_factory_t* factory,
                                                            const clap_host_t* host,
                                                            const char* plugin_id)
        {
            static_cast<void>(factory);
            if (host == nullptr || plugin_id == nullptr ||
                !clap_version_is_compatible(host->clap_version) ||
                std::strcmp(plugin_id, ClapRigPlugin::K_ID) != 0)
            {
                return nullptr;
            }
            return ClapRigPlugin::create(host);
        }

        /// @brief Holds the plugin factory's callbacks.
        const clap_plugin_factory_t K_FACTORY = {factory_plugin_count, factory_plugin_descriptor,
                                                 factory_create_plugin};

        /// @brief Accepts clap_entry.init; the library holds no state of its own to set up.
        bool CLAP_ABI entry_init(const char* plugin_path)
        {
            static_cast<void>(plugin_path);
            return true;
        }

        /// @brief Accepts clap_entry.deinit; the library holds no state of its own to release.
        void CLAP_ABI entry_deinit()
        {

        }

        /// @brief Answers clap_entry.get_factory with the plugin factory and no other.
        const void* CLAP_ABI entry_get_factory(const char* factory_id)
        {
            if (factory_id == nullptr || std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) != 0)
            {
                return nullptr;
            }
            return &K_FACTORY;
        }
    } // namespace
} // namespace SushiDSP::Plugin

extern "C"
{
    /// @brief Holds the symbol a CLAP host looks up when it loads the library.
    CLAP_EXPORT extern const clap_plugin_entry_t clap_entry = {
        CLAP_VERSION_INIT,
        SushiDSP::Plugin::entry_init,
        SushiDSP::Plugin::entry_deinit,
        SushiDSP::Plugin::entry_get_factory,
    };
}
