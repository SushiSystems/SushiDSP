/**************************************************************************/
/* profile_session.cpp                                                    */
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
 * @file profile_session.cpp
 * @brief Defines the profiling session over SushiProfiler's Session.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/profile/profile_session.hpp>

#include <SushiDSP/foundation/profile/profile.hpp>
#include <SushiDSP/foundation/profile/profile_runtime.hpp>

#include "block_timeline_binding.hpp"

#include <cstdlib>
#include <ostream>
#include <streambuf>
#include <string>

#if SD_ENABLE_PROFILER
#include "profile_record_impl.hpp"

#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

#include <SushiProfiler/config/profiler_config.hpp>
#include <SushiProfiler/periods/period_timeline.hpp>
#include <SushiProfiler/registry/registry.hpp>
#include <SushiProfiler/session/session.hpp>

#include <fstream>
#include <iterator>
#include <utility>
#endif

namespace SushiDSP::profile
{
    namespace
    {
        /// @brief A buffer that takes every character and keeps none.
        class DiscardingBuffer final : public std::streambuf
        {
            protected:
                /// @brief Accepts @p character and drops it.
                int overflow(int character) override
                {
                    return character;
                }
        };
    } // namespace

    std::ostream& discarded_output() noexcept
    {
        static DiscardingBuffer buffer;
        static std::ostream stream(&buffer);
        return stream;
    }

#if SD_ENABLE_PROFILER
    /// @brief Holds the registry, the running session and the timeline the last mark used.
    struct ProfileSession::Impl
    {
            /// @brief Builds an idle holder whose report goes to @p report_output.
            explicit Impl(std::ostream& report_output) : report(report_output)
            {

            }

            /// @brief Starts a session from @p config and binds the block timeline to it.
            ProfileStart begin(const SushiProfiler::ProfilerConfig& config)
            {
                if (session != nullptr)
                    return ProfileStart{ProfileStartStatus::AlreadyRunning,
                                        "this object already runs a session"};
                SushiProfiler::register_builtin_writers(registry, report);
                SushiProfiler::SessionStart started =
                    SushiProfiler::Session::start(config, registry);
                if (!started.ok())
                    return ProfileStart{started.error.rfind("a session is already running", 0) == 0
                                            ? ProfileStartStatus::AlreadyRunning
                                            : ProfileStartStatus::StartRefused,
                                        started.error};
                session = std::move(started.session);
                Detail::bind_block_timeline(&session->timeline(names::K_BLOCK_TIMELINE));
                return ProfileStart{ProfileStartStatus::Started, std::string()};
            }

            std::ostream& report;                            /**< Where the terminal writer prints. */
            SushiProfiler::Registry registry;                /**< Where the writers come from. */
            std::unique_ptr<SushiProfiler::Session> session; /**< The running session, or null. */
            std::string marked_name;                         /**< The name `marked` has. */
            SushiProfiler::PeriodTimeline* marked = nullptr; /**< The timeline the last mark used. */
    };

    ProfileSession::ProfileSession(std::ostream& report_output)
        : impl_(std::make_unique<Impl>(report_output))
    {

    }

    ProfileSession::~ProfileSession()
    {
        if (impl_->session != nullptr)
            static_cast<void>(finish());
    }

    ProfileStart ProfileSession::start_from_environment()
    {
        const char* const path = std::getenv(K_CONFIG_VARIABLE);
        if (path == nullptr || path[0] == '\0')
            return ProfileStart{};
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return ProfileStart{ProfileStartStatus::ConfigUnreadable,
                                std::string(K_CONFIG_VARIABLE) + " names " + path +
                                    ", which could not be opened"};
        const std::string text((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
        return start(text);
    }

    ProfileStart ProfileSession::start(std::string_view config_json)
    {
        const SushiProfiler::ConfigReadResult read = SushiProfiler::read_profiler_config(config_json);
        if (!read.ok())
            return ProfileStart{ProfileStartStatus::ConfigRefused, read.error};
        return impl_->begin(*read.config);
    }

    ProfileStart ProfileSession::start_window(std::string_view timeline, std::uint32_t warmup,
                                              std::uint32_t active)
    {
        SushiProfiler::ProfilerConfig config;
        config.timeline = std::string(timeline);
        config.window.warmup = warmup;
        config.window.active = active;
        return impl_->begin(config);
    }

    bool ProfileSession::running() const noexcept
    {
        return impl_->session != nullptr;
    }

    void ProfileSession::mark(std::string_view timeline, std::uint64_t budget_ns)
    {
        if (impl_->session == nullptr)
            return;
        if (impl_->marked == nullptr || impl_->marked_name != timeline)
        {
            impl_->marked_name = std::string(timeline);
            impl_->marked = &impl_->session->timeline(timeline);
        }
        static_cast<void>(impl_->marked->mark(budget_ns));
    }

    ProfileFinish ProfileSession::finish()
    {
        ProfileFinish finished;
        if (impl_->session == nullptr)
            return finished;
        Detail::bind_block_timeline(nullptr);
        impl_->marked = nullptr;
        SushiProfiler::SessionStop stopped = impl_->session->stop();
        const SushiProfiler::SessionReport report = impl_->session->report(stopped.record);
        finished.problems = std::move(stopped.problems);
        finished.problems.insert(finished.problems.end(), report.problems.begin(),
                                 report.problems.end());
        finished.record = ProfileRecord(
            std::make_shared<const ProfileRecord::Impl>(std::move(stopped.record)));
        impl_->session.reset();
        return finished;
    }
#else
    /// @brief Holds nothing: this build records nothing.
    struct ProfileSession::Impl
    {

    };

    ProfileSession::ProfileSession(std::ostream& report_output) : impl_(std::make_unique<Impl>())
    {
        static_cast<void>(report_output);
    }

    ProfileSession::~ProfileSession() = default;

    ProfileStart ProfileSession::start_from_environment()
    {
        const char* const path = std::getenv(K_CONFIG_VARIABLE);
        if (path == nullptr || path[0] == '\0')
            return ProfileStart{};
        return ProfileStart{ProfileStartStatus::NotBuilt,
                            std::string(K_CONFIG_VARIABLE) +
                                " is set, but this build records nothing (SD_ENABLE_PROFILER=OFF)"};
    }

    ProfileStart ProfileSession::start(std::string_view config_json)
    {
        static_cast<void>(config_json);
        return ProfileStart{ProfileStartStatus::NotBuilt, "this build records nothing"};
    }

    ProfileStart ProfileSession::start_window(std::string_view timeline, std::uint32_t warmup,
                                              std::uint32_t active)
    {
        static_cast<void>(timeline);
        static_cast<void>(warmup);
        static_cast<void>(active);
        return ProfileStart{ProfileStartStatus::NotBuilt, "this build records nothing"};
    }

    bool ProfileSession::running() const noexcept
    {
        return false;
    }

    void ProfileSession::mark(std::string_view timeline, std::uint64_t budget_ns)
    {
        static_cast<void>(timeline);
        static_cast<void>(budget_ns);
    }

    ProfileFinish ProfileSession::finish()
    {
        return ProfileFinish{};
    }
#endif
} // namespace SushiDSP::profile
