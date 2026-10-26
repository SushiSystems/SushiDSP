/**************************************************************************/
/* clap_plugin.cpp                                                        */
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
 * @file clap_plugin.cpp
 * @brief Implements the CLAP plugin instance and the callback table a host calls it through.
 * @author Mustafa Garip
 */

#include "clap_plugin.hpp"

#include <SushiDSP/foundation/log/log.hpp>
#include <SushiDSP/foundation/profile/profile.hpp>
#include <SushiDSP/foundation/profile/profile_runtime.hpp>
#include <SushiDSP/foundation/profile/profile_session.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

#include "clap_boundary.hpp"
#include "clap_extensions.hpp"
#include "plugin_log_category.hpp"
#include "startup_rig.hpp"

namespace SushiDSP::Plugin
{
    namespace
    {
        /// @brief Lists the features a host files the plugin under, closed by a null entry.
        const char* const K_FEATURES[] = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
                                          CLAP_PLUGIN_FEATURE_STEREO, nullptr};

        /// @brief Holds the descriptor the factory lists and every instance points at.
        const clap_plugin_descriptor_t K_DESCRIPTOR = {
            CLAP_VERSION_INIT,
            ClapRigPlugin::K_ID,
            "SushiDSP Rig",
            "Sushi Systems",
            "https://sushisystems.io",
            "",
            "",
            SUSHIDSP_PLUGIN_VERSION,
            "Runs a rig preset saved by the SushiDSP standalone host.",
            K_FEATURES,
        };

        /// @brief Calls a function with the id and value of every parameter value event.
        /// @param events The host's event list; may be null.
        /// @param apply Takes a clap_id and a float.
        template <typename Apply>
        void for_each_parameter_value(const clap_input_events_t* events, Apply&& apply)
        {
            if (events == nullptr)
            {
                return;
            }
            const std::uint32_t count = events->size(events);
            for (std::uint32_t index = 0; index < count; ++index)
            {
                const clap_event_header_t* header = events->get(events, index);
                if (header == nullptr || header->space_id != CLAP_CORE_EVENT_SPACE_ID ||
                    header->type != CLAP_EVENT_PARAM_VALUE)
                {
                    continue;
                }
                const auto* change = reinterpret_cast<const clap_event_param_value_t*>(header);
                apply(change->param_id, static_cast<float>(change->value));
            }
        }

        /// @brief Forwards clap_plugin.init to the instance.
        bool CLAP_ABI plugin_init(const clap_plugin_t* plugin)
        {
            return guarded("plugin.init", false,
                           [plugin] { return ClapRigPlugin::of(plugin).init(); });
        }

        /// @brief Frees the instance clap_plugin.destroy names.
        void CLAP_ABI plugin_destroy(const clap_plugin_t* plugin)
        {
            delete &ClapRigPlugin::of(plugin);
        }

        /// @brief Forwards clap_plugin.activate to the instance.
        bool CLAP_ABI plugin_activate(const clap_plugin_t* plugin, double sample_rate,
                                      std::uint32_t min_frames, std::uint32_t max_frames)
        {
            static_cast<void>(min_frames);
            return guarded("plugin.activate", false,
                           [plugin, sample_rate, max_frames]
                           {
                               return ClapRigPlugin::of(plugin).activate(sample_rate, max_frames);
                           });
        }

        /// @brief Forwards clap_plugin.deactivate to the instance.
        void CLAP_ABI plugin_deactivate(const clap_plugin_t* plugin)
        {
            guarded("plugin.deactivate", false,
                    [plugin]
                    {
                        ClapRigPlugin::of(plugin).deactivate();
                        return true;
                    });
        }

        /// @brief Accepts clap_plugin.start_processing; the rig needs nothing started.
        bool CLAP_ABI plugin_start_processing(const clap_plugin_t* plugin)
        {
            static_cast<void>(plugin);
            return true;
        }

        /// @brief Accepts clap_plugin.stop_processing; the rig needs nothing stopped.
        void CLAP_ABI plugin_stop_processing(const clap_plugin_t* plugin)
        {
            static_cast<void>(plugin);
        }

        /// @brief Accepts clap_plugin.reset and clears nothing; see applications/plugin/README.md.
        void CLAP_ABI plugin_reset(const clap_plugin_t* plugin)
        {
            static_cast<void>(plugin);
        }

        /// @brief Forwards clap_plugin.process to the instance.
        clap_process_status CLAP_ABI plugin_process(const clap_plugin_t* plugin,
                                                    const clap_process_t* process)
        {
            return ClapRigPlugin::of(plugin).process(*process);
        }

        /// @brief Answers clap_plugin.get_extension from the extension table.
        const void* CLAP_ABI plugin_get_extension(const clap_plugin_t* plugin, const char* id)
        {
            static_cast<void>(plugin);
            return find_extension(id);
        }

        /// @brief Forwards clap_plugin.on_main_thread to the instance.
        void CLAP_ABI plugin_on_main_thread(const clap_plugin_t* plugin)
        {
            guarded("plugin.on_main_thread", false,
                    [plugin]
                    {
                        ClapRigPlugin::of(plugin).on_main_thread();
                        return true;
                    });
        }
    } // namespace

    const clap_plugin_descriptor_t& ClapRigPlugin::descriptor() noexcept
    {
        return K_DESCRIPTOR;
    }

    const clap_plugin_t* ClapRigPlugin::create(const clap_host_t* host) noexcept
    {
        return guarded("plugin.create", static_cast<const clap_plugin_t*>(nullptr),
                       [host]() -> const clap_plugin_t*
                       {
                           return &(new ClapRigPlugin(host))->plugin_;
                       });
    }

    ClapRigPlugin& ClapRigPlugin::of(const clap_plugin_t* plugin) noexcept
    {
        return *static_cast<ClapRigPlugin*>(plugin->plugin_data);
    }

    ClapRigPlugin::ClapRigPlugin(const clap_host_t* host) : host_(host), plugin_{}
    {
        plugin_.desc = &K_DESCRIPTOR;
        plugin_.plugin_data = this;
        plugin_.init = plugin_init;
        plugin_.destroy = plugin_destroy;
        plugin_.activate = plugin_activate;
        plugin_.deactivate = plugin_deactivate;
        plugin_.start_processing = plugin_start_processing;
        plugin_.stop_processing = plugin_stop_processing;
        plugin_.reset = plugin_reset;
        plugin_.process = plugin_process;
        plugin_.get_extension = plugin_get_extension;
        plugin_.on_main_thread = plugin_on_main_thread;
        adapter_.add_counters_to(counter_reporter_);
        counter_reporter_.add("bad_block", bad_block_);
    }

    ClapRigPlugin::~ClapRigPlugin() = default;

    bool ClapRigPlugin::init()
    {
        host_params_ = static_cast<const clap_host_params_t*>(
            host_->get_extension(host_, CLAP_EXT_PARAMS));
        host_latency_ = static_cast<const clap_host_latency_t*>(
            host_->get_extension(host_, CLAP_EXT_LATENCY));

        const std::optional<std::string> startup = read_startup_rig();
        if (startup.has_value())
        {
            // A refused startup rig leaves the empty rig in place, which passes audio through.
            std::string error;
            if (adapter_.load_state(*startup, error) != RigStateStatus::Ok)
            {
                SD_LOG_WARN(K_PLUGIN_LOG_CATEGORY,
                            "the startup rig was refused: {}; the plugin starts on the empty rig",
                            error);
            }
        }
        return true;
    }

    bool ClapRigPlugin::activate(double sample_rate, std::uint32_t max_frames)
    {
        std::string error;
        if (adapter_.prepare(sample_rate, max_frames, error) != RigStateStatus::Ok)
        {
            SD_LOG_ERROR(K_PLUGIN_LOG_CATEGORY,
                         "activate() could not prepare the rig at {} Hz with blocks of {} frames: "
                         "{}; the host sees a failed activation",
                         sample_rate, max_frames, error);
            return false;
        }
        sample_rate_ = sample_rate;
        start_profiling();
        const std::uint32_t latency = adapter_.latency_frames();
        if (latency != announced_latency_ && host_latency_ != nullptr)
        {
            announced_latency_ = latency;
            host_latency_->changed(host_);
        }
        is_active_.store(true, std::memory_order_release);
        return true;
    }

    void ClapRigPlugin::start_profiling()
    {
        if (profile_session_ != nullptr)
        {
            return;
        }
        auto session = std::make_unique<profile::ProfileSession>();
        const profile::ProfileStart started = session->start_from_environment();
        if (started.started())
        {
            profile_session_ = std::move(session);
        }
        else if (started.status != profile::ProfileStartStatus::NotRequested)
        {
            SD_LOG_WARN(K_PLUGIN_LOG_CATEGORY,
                        "profiling was asked for and did not start: {}; the plugin runs without "
                        "it",
                        started.message);
        }
    }

    void ClapRigPlugin::deactivate()
    {
        is_active_.store(false, std::memory_order_release);
        if (profile_session_ != nullptr)
        {
            const profile::ProfileFinish finished = profile_session_->finish();
            for (const std::string& problem : finished.problems)
            {
                SD_LOG_WARN(K_PLUGIN_LOG_CATEGORY, "the profiling session reported: {}", problem);
            }
            profile_session_.reset();
        }
        adapter_.reclaim();
        if (rescan_is_due_)
        {
            rescan_is_due_ = false;
            if (host_params_ != nullptr)
            {
                host_params_->rescan(host_, CLAP_PARAM_RESCAN_ALL);
            }
        }
    }

    clap_process_status ClapRigPlugin::process(const clap_process_t& block) noexcept
    {
        profile::register_this_thread();
        profile::mark_block(block.frames_count, sample_rate_);
        SD_ZONE_GROUP(profile::names::K_PROCESS_ZONE, profile::names::K_SESSION_GROUP);
        if (++blocks_since_request_ >= K_BLOCKS_PER_CALLBACK_REQUEST)
        {
            blocks_since_request_ = 0;
            host_->request_callback(host_);
        }
        if (block.audio_inputs_count < 1 || block.audio_outputs_count < 1)
        {
            bad_block_.increment();
            return CLAP_PROCESS_ERROR;
        }
        const clap_audio_buffer_t& input = block.audio_inputs[0];
        const clap_audio_buffer_t& output = block.audio_outputs[0];
        if (input.channel_count != RigAdapter::K_CHANNELS ||
            output.channel_count != RigAdapter::K_CHANNELS || input.data32 == nullptr ||
            output.data32 == nullptr)
        {
            bad_block_.increment();
            return CLAP_PROCESS_ERROR;
        }

        for_each_parameter_value(block.in_events,
                                 [this](clap_id id, float value)
                                 {
                                     adapter_.queue_parameter_for_audio_thread(id, value);
                                 });
        adapter_.process(input.data32, output.data32, RigAdapter::K_CHANNELS, block.frames_count);
        return CLAP_PROCESS_CONTINUE;
    }

    void ClapRigPlugin::on_main_thread()
    {
        adapter_.reclaim();
        const std::string line = counter_reporter_.poll(RtCounterReporter::Clock::now());
        if (!line.empty())
        {
            SD_LOG_WARN(K_PLUGIN_LOG_CATEGORY, "{}", line);
        }
    }

    bool ClapRigPlugin::load_rig(const std::string& json_text)
    {
        std::string error;
        if (adapter_.load_state(json_text, error) != RigStateStatus::Ok)
        {
            SD_LOG_ERROR(K_PLUGIN_LOG_CATEGORY,
                         "the saved state could not be loaded: {}; the rig stands as it was",
                         error);
            return false;
        }
        if (is_active_.load(std::memory_order_acquire))
        {
            // CLAP allows a full parameter rescan only while the plugin is not active.
            rescan_is_due_ = true;
            host_->request_restart(host_);
        }
        else if (host_params_ != nullptr)
        {
            host_params_->rescan(host_, CLAP_PARAM_RESCAN_ALL);
        }
        host_->request_callback(host_);
        return true;
    }

    bool ClapRigPlugin::save_rig(std::string& json_text_out)
    {
        std::string error;
        if (adapter_.save_state(json_text_out, error) != RigStateStatus::Ok)
        {
            SD_LOG_ERROR(K_PLUGIN_LOG_CATEGORY,
                         "the rig could not be saved: {}; the host gets no state", error);
            return false;
        }
        return true;
    }

    std::uint32_t ClapRigPlugin::latency()
    {
        announced_latency_ = adapter_.latency_frames();
        return announced_latency_;
    }

    const std::vector<RigParameter>& ClapRigPlugin::parameters() const noexcept
    {
        return adapter_.parameters();
    }

    std::optional<float> ClapRigPlugin::parameter_value(clap_id id) const
    {
        return adapter_.parameter_value(id);
    }

    void ClapRigPlugin::flush_parameters(const clap_input_events_t* events)
    {
        if (is_active_.load(std::memory_order_acquire))
        {
            for_each_parameter_value(events,
                                     [this](clap_id id, float value)
                                     {
                                         adapter_.queue_parameter_for_audio_thread(id, value);
                                     });
            adapter_.flush_parameters_for_audio_thread();
            return;
        }
        for_each_parameter_value(events,
                                 [this](clap_id id, float value)
                                 {
                                     adapter_.set_parameter(id, value);
                                 });
    }
} // namespace SushiDSP::Plugin
