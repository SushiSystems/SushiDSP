/**************************************************************************/
/* test_clap_plugin.cpp                                                   */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_clap_plugin.cpp
 * @brief Tests the counters of the plugin instance through a host that offers nothing.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <clap/clap.h>

#include "clap_plugin.hpp"

#include <memory>
#include <optional>

namespace
{
    /// @brief Answers every extension request with nothing and ignores every request.
    const void* CLAP_ABI no_extension(const clap_host_t* host, const char* id)
    {
        static_cast<void>(host);
        static_cast<void>(id);
        return nullptr;
    }

    /// @brief Ignores a request the plugin makes of the host.
    void CLAP_ABI ignore_request(const clap_host_t* host)
    {
        static_cast<void>(host);
    }

    /// @brief Builds a host that offers no extension.
    clap_host_t make_host()
    {
        clap_host_t host{};
        host.clap_version = CLAP_VERSION_INIT;
        host.name = "test";
        host.get_extension = &no_extension;
        host.request_restart = &ignore_request;
        host.request_process = &ignore_request;
        host.request_callback = &ignore_request;
        return host;
    }
} // namespace

TEST(Small_ClapPlugin, ABlockWithNoAudioPortsIsAnErrorAndIsCounted)
{
    const clap_host_t host = make_host();
    SushiDSP::Plugin::ClapRigPlugin plugin(&host, std::nullopt);
    ASSERT_TRUE(plugin.init());
    ASSERT_TRUE(plugin.activate(48000.0, 256));
    clap_process_t block{};
    block.frames_count = 256;
    block.audio_inputs_count = 0;
    block.audio_outputs_count = 0;

    EXPECT_EQ(plugin.process(block), CLAP_PROCESS_ERROR);
    EXPECT_EQ(plugin.process(block), CLAP_PROCESS_ERROR);

    EXPECT_EQ(plugin.bad_block_count().value(), 2U);
}

TEST(Small_ClapPlugin, OnMainThreadRunsWithNothingToReport)
{
    const clap_host_t host = make_host();
    SushiDSP::Plugin::ClapRigPlugin plugin(&host, std::nullopt);
    ASSERT_TRUE(plugin.init());

    plugin.on_main_thread();

    EXPECT_EQ(plugin.bad_block_count().value(), 0U);
}
