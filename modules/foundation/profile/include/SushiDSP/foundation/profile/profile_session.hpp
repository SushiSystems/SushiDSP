/**************************************************************************/
/* profile_session.hpp                                                    */
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
 * @file profile_session.hpp
 * @brief Declares the profiling session a product or a benchmark starts and finishes.
 * @author Mustafa Garip
 * @see modules/foundation/profile/README.md
 */

#pragma once

#include <SushiDSP/foundation/profile/profile_record.hpp>

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace SushiDSP::profile
{
    /// @brief Returns a stream that discards what is written to it.
    [[nodiscard]] std::ostream& discarded_output() noexcept;

    /// @brief Names how a start request ended.
    enum class ProfileStartStatus : std::uint8_t
    {
        Started,          /**< A session runs. */
        NotRequested,     /**< No configuration was asked for; nothing runs. */
        NotBuilt,         /**< The build records nothing. */
        AlreadyRunning,   /**< This object, or the process, already runs a session. */
        ConfigUnreadable, /**< The configuration file could not be opened. */
        ConfigRefused,    /**< The configuration is not valid. */
        StartRefused      /**< SushiProfiler refused to start. */
    };

    /// @brief How a start request ended, with the reason when it did not start.
    struct ProfileStart
    {
            ProfileStartStatus status = ProfileStartStatus::NotRequested; /**< The outcome. */
            std::string message; /**< Why it did not start, or empty. */

            /// @brief Returns whether a session runs.
            [[nodiscard]] bool started() const noexcept
            {
                return status == ProfileStartStatus::Started;
            }
    };

    /// @brief What a finished session hands back.
    struct ProfileFinish
    {
            ProfileRecord record;              /**< The run. */
            std::vector<std::string> problems; /**< What went wrong outside the record. */
    };

    /**
     * @brief Runs one SushiProfiler session for a product or a benchmark.
     *
     * One session runs per process. While it runs, the `dsp.block` timeline is bound, so
     * `mark_block` from a device callback opens its periods. Control thread only.
     */
    class ProfileSession final
    {
        public:
            /// @brief The environment variable that names a configuration file.
            static constexpr const char* K_CONFIG_VARIABLE = "SD_PROFILE_CONFIG";

            /// @brief Builds an idle session whose configured report goes to @p report_output.
            /// @param report_output Where the terminal writer prints; it outlives the session. The
            /// default discards the report, and a console program passes its own stream.
            explicit ProfileSession(std::ostream& report_output = discarded_output());

            /// @brief Finishes the session if it still runs, dropping its record.
            ~ProfileSession();

            ProfileSession(const ProfileSession&) = delete;
            ProfileSession& operator=(const ProfileSession&) = delete;
            ProfileSession(ProfileSession&&) = delete;
            ProfileSession& operator=(ProfileSession&&) = delete;

            /// @brief Starts from the file the variable SD_PROFILE_CONFIG names, if it is set.
            [[nodiscard]] ProfileStart start_from_environment();

            /// @brief Starts from the JSON configuration @p config_json.
            [[nodiscard]] ProfileStart start(std::string_view config_json);

            /**
             * @brief Starts a session that follows @p timeline with a window and no writers.
             * @param warmup Periods recorded and marked warm-up.
             * @param active Periods recorded as active; at least 1.
             */
            [[nodiscard]] ProfileStart start_window(std::string_view timeline,
                                                    std::uint32_t warmup, std::uint32_t active);

            /// @brief Returns whether a session runs.
            [[nodiscard]] bool running() const noexcept;

            /// @brief Opens the next period of @p timeline with @p budget_ns, 0 for none.
            void mark(std::string_view timeline, std::uint64_t budget_ns);

            /// @brief Stops the session, prints the configured report and returns the run.
            [[nodiscard]] ProfileFinish finish();

        private:
            struct Impl;

            /// @brief Holds SushiProfiler's objects, so no caller includes them.
            std::unique_ptr<Impl> impl_;
    };
} // namespace SushiDSP::profile
