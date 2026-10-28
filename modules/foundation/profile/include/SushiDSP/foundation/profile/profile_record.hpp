/**************************************************************************/
/* profile_record.hpp                                                     */
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
 * @file profile_record.hpp
 * @brief Declares what a finished profiling run gives back: its samples, summaries and verdicts.
 * @author Mustafa Garip
 * @see modules/foundation/profile/README.md
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace SushiDSP::profile
{
    /// @brief One measured subject and its samples, one per active period.
    struct ProfileSubject
    {
            std::string name;            /**< `<kind>:<name>`, such as `zone:dsp.process`. */
            std::string unit;            /**< `ns` for times, else the site's unit. */
            std::vector<double> samples; /**< The samples, in period order. */
    };

    /// @brief How a lookup of a subject by name ended.
    enum class ProfileLookupStatus : std::uint8_t
    {
        Found,    /**< Exactly one subject has the name. */
        Missing,  /**< No subject has the name. */
        Ambiguous /**< Several subjects share the name, as two call sites share one counter name. */
    };

    /// @brief The answer to a lookup of a subject by name.
    struct ProfileLookup
    {
            ProfileLookupStatus status = ProfileLookupStatus::Missing; /**< How the lookup ended. */
            const ProfileSubject* subject = nullptr; /**< The first subject of that name, or null. */
            std::size_t matches = 0U;                /**< How many subjects have the name. */
    };

    /// @brief The statistics of one timeline's active periods, in nanoseconds.
    struct TimelineSummary
    {
            std::size_t periods = 0U; /**< Active periods the timeline has. */
            double budget_ns = 0.0;   /**< The budget of the first active period, or 0. */
            double minimum_ns = 0.0;  /**< The shortest period. */
            double median_ns = 0.0;   /**< The median period. */
            double p99_ns = 0.0;      /**< The 99th percentile. */
            double maximum_ns = 0.0;  /**< The longest period. */
            double jitter_ns = 0.0;   /**< The median absolute difference of successive periods. */
    };

    /// @brief What one check found.
    struct ProfileVerdict
    {
            std::string subject;   /**< The subject the check bounds. */
            std::string statistic; /**< The statistic bounded: median, p99, max. */
            bool passed = false;   /**< Whether the value is within the bounds. */
            double value = 0.0;    /**< The statistic's value. */
            std::string reason;    /**< Why it failed, such as `max is 3 ns, above at most 1 ns`. */
    };

    /// @brief The verdicts of a checks file, or why it was refused.
    struct ProfileCheckReport
    {
            std::string error;                    /**< The refusal, or empty. */
            std::vector<ProfileVerdict> verdicts; /**< One per check, in file order. */

            /// @brief Returns whether the file was read and every check passed.
            [[nodiscard]] bool all_passed() const noexcept;
    };

    class ProfileSession;

    /**
     * @brief Holds one run's record and answers the questions a benchmark or a test asks of it.
     *
     * It keeps SushiProfiler's types out of its callers. An empty record answers every question
     * with nothing.
     */
    class ProfileRecord final
    {
        public:
            /// @brief Builds an empty record.
            ProfileRecord();

            /// @brief Returns whether the run left no record, as when the build records nothing.
            [[nodiscard]] bool empty() const noexcept;

            /// @brief Returns the periods the window marked active.
            [[nodiscard]] std::size_t active_periods() const noexcept;

            /// @brief Returns the periods the window marked warm-up and recorded.
            [[nodiscard]] std::size_t warmup_periods() const noexcept;

            /// @brief Returns every subject that has samples.
            [[nodiscard]] const std::vector<ProfileSubject>& subjects() const noexcept;

            /// @brief Returns the one subject named @p name, or null when none has the name or
            /// several share it. Use lookup() to tell the two apart.
            [[nodiscard]] const ProfileSubject* find(std::string_view name) const noexcept;

            /// @brief Returns how the search for @p name ended: found, missing, or shared by
            /// several subjects. Each call site of a profiling macro is its own subject, so two
            /// sites with one name are ambiguous and are never read as one.
            [[nodiscard]] ProfileLookup lookup(std::string_view name) const noexcept;

            /// @brief Returns the statistics of timeline @p timeline; zero periods when unknown.
            [[nodiscard]] TimelineSummary summarise(std::string_view timeline) const;

            /// @brief Returns how many times a zone labelled @p label ran in the active periods.
            [[nodiscard]] std::uint64_t zone_calls(std::string_view label) const;

            /// @brief Returns the name of every lane a zone ran in during the active periods,
            /// sorted and without repeats.
            [[nodiscard]] std::vector<std::string> lanes() const;

            /// @brief Returns the verdicts of the JSON array of checks @p checks_json.
            [[nodiscard]] ProfileCheckReport check(std::string_view checks_json) const;

        private:
            struct Impl;

            /// @brief Builds a record that shares @p impl.
            explicit ProfileRecord(std::shared_ptr<const Impl> impl) noexcept;

            friend class ProfileSession;

            /// @brief The captured run, or null for an empty record.
            std::shared_ptr<const Impl> impl_;
    };
} // namespace SushiDSP::profile
