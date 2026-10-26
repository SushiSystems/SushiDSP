/**************************************************************************/
/* clap_plugin.hpp                                                        */
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
 * @file clap_plugin.hpp
 * @brief Declares the CLAP plugin instance that drives one RigAdapter.
 * @author Mustafa Garip
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <clap/clap.h>

#include <SushiDSP/foundation/core/rt_counter.hpp>
#include <SushiDSP/foundation/core/rt_counter_report.hpp>

#include "rig_adapter.hpp"
#include "rig_parameter.hpp"

namespace SushiDSP::profile
{
    class ProfileSession;
} // namespace SushiDSP::profile

namespace SushiDSP::Plugin
{
    /// @brief Maps one CLAP plugin instance onto a RigAdapter: the life cycle, the block and the
    /// news the host is owed when the rig changes. It reads no CLAP extension struct; the
    /// extension tables in clap_extensions.cpp do that and call the methods below.
    /// @see applications/plugin/README.md
    class ClapRigPlugin final
    {
        public:
            /// @brief Names the plugin to a host; a session stores it, so it never changes.
            static constexpr const char* K_ID = "io.sushisystems.sushidsp.rig";

            /// @brief Returns the descriptor the factory lists.
            [[nodiscard]] static const clap_plugin_descriptor_t& descriptor() noexcept;

            /// @brief Builds an instance the host owns until it calls destroy.
            /// @param host The host's interface; outlives the instance.
            /// @return The instance's CLAP handle, or nullptr when it could not be built.
            [[nodiscard]] static const clap_plugin_t* create(const clap_host_t* host) noexcept;

            /// @brief Returns the instance behind a CLAP handle create() returned.
            [[nodiscard]] static ClapRigPlugin& of(const clap_plugin_t* plugin) noexcept;

            explicit ClapRigPlugin(const clap_host_t* host);

            ClapRigPlugin(const ClapRigPlugin&) = delete;
            ClapRigPlugin& operator=(const ClapRigPlugin&) = delete;
            ClapRigPlugin(ClapRigPlugin&&) = delete;
            ClapRigPlugin& operator=(ClapRigPlugin&&) = delete;
            ~ClapRigPlugin();

            /// @brief Reads the host's extensions and loads the startup rig when one is named.
            /// @return True; a startup rig that will not load leaves the empty rig in place.
            bool init();

            /// @brief Prepares the rig at the host's rate and block size. Main thread.
            /// @return False when the adapter refuses the format.
            bool activate(double sample_rate, std::uint32_t max_frames);

            /// @brief Ends the active span and gives the host the parameter rescan it is owed.
            void deactivate();

            /// @brief Applies the block's parameter events, then renders the block. Audio thread;
            /// allocates nothing and locks nothing.
            /// @return CLAP_PROCESS_CONTINUE, or CLAP_PROCESS_ERROR for a block that is not one
            /// stereo 32-bit input and one stereo 32-bit output.
            clap_process_status process(const clap_process_t& block) noexcept;

            /// @brief Frees what the audio thread has moved past and writes the counters of the
            /// real-time path as one WARN per interval when any moved. Main thread.
            void on_main_thread();

            /// @brief Returns the blocks answered with CLAP_PROCESS_ERROR; read it off the audio thread.
            [[nodiscard]] const RtCounter& bad_block_count() const noexcept
            {
                return bad_block_;
            }

            /// @brief Replaces the rig from preset text and tells the host what changed.
            /// @return False when the adapter refuses the preset; the rig then stands as it was.
            bool load_rig(const std::string& json_text);

            /// @brief Writes the rig as preset text.
            /// @return False when the rig will not serialize.
            bool save_rig(std::string& json_text_out);

            /// @brief Reports the rig's latency and records that the host has read it.
            std::uint32_t latency();

            /// @brief Lists the rig's parameters. Main thread.
            [[nodiscard]] const std::vector<RigParameter>& parameters() const noexcept;

            /// @brief Reads a parameter's value. Main thread.
            /// @return The value, or std::nullopt when the rig holds no such parameter.
            [[nodiscard]] std::optional<float> parameter_value(clap_id id) const;

            /// @brief Applies parameter events outside a block: on the audio thread while the
            /// plugin is active, on the main thread otherwise.
            /// @param events The host's event list; may be null.
            void flush_parameters(const clap_input_events_t* events);

        private:
            /// @brief How often the main thread reports the counters of the real-time path.
            static constexpr std::chrono::seconds K_REPORT_INTERVAL{5};

            /// @brief How many blocks pass between two requests for a main-thread callback.
            static constexpr std::uint32_t K_BLOCKS_PER_CALLBACK_REQUEST = 512;

            /// @brief Starts the profiling session the environment asks for, once.
            void start_profiling();

            /// @brief Names the host that owns this instance.
            const clap_host_t* host_;

            /// @brief Names the host's parameter extension, or nullptr when it has none.
            const clap_host_params_t* host_params_ = nullptr;

            /// @brief Names the host's latency extension, or nullptr when it has none.
            const clap_host_latency_t* host_latency_ = nullptr;

            /// @brief Holds the callback table the host calls this instance through.
            clap_plugin_t plugin_;

            /// @brief Owns the rig.
            RigAdapter adapter_;

            /// @brief Reports that the host has activated the plugin and not yet deactivated it.
            std::atomic<bool> is_active_{false};

            /// @brief Records that the rig changed while active, so deactivate() owes a rescan.
            bool rescan_is_due_ = false;

            /// @brief Holds the latency the host last read or was told to read again.
            std::uint32_t announced_latency_ = 0;

            /// @brief Counts the blocks answered with CLAP_PROCESS_ERROR.
            RtCounter bad_block_;

            /// @brief Reads the counters of the adapter and of this instance once per interval.
            RtCounterReporter counter_reporter_{K_REPORT_INTERVAL};

            /// @brief Holds the rate the plugin was activated at, for the block's budget.
            double sample_rate_ = RigAdapter::K_DEFAULT_SAMPLE_RATE;

            /// @brief Counts the blocks since the last request for a main-thread callback.
            std::uint32_t blocks_since_request_ = 0;

            /// @brief Runs the profiling session the environment asks for, or none.
            std::unique_ptr<profile::ProfileSession> profile_session_;
    };
} // namespace SushiDSP::Plugin
