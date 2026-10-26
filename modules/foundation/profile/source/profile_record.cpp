/**************************************************************************/
/* profile_record.cpp                                                     */
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
 * @file profile_record.cpp
 * @brief Defines the questions a finished run answers.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/profile/profile_record.hpp>

#include <SushiDSP/foundation/profile/profile.hpp>

#include <utility>

#if SD_ENABLE_PROFILER
#include "profile_record_impl.hpp"

#include <SushiProfiler/analysis/subject_samples.hpp>
#include <SushiProfiler/checks/check.hpp>
#include <SushiProfiler/checks/verdict.hpp>
#include <SushiProfiler/statistics/dispersion.hpp>
#include <SushiProfiler/statistics/sorted_samples.hpp>

#include <set>
#include <string>
#include <utility>
#endif

namespace SushiDSP::profile
{
    bool ProfileCheckReport::all_passed() const noexcept
    {
        if (!error.empty() || verdicts.empty())
            return false;
        for (const ProfileVerdict& verdict : verdicts)
        {
            if (!verdict.passed)
                return false;
        }
        return true;
    }

#if SD_ENABLE_PROFILER
    ProfileRecord::Impl::Impl(SushiProfiler::RunRecord record) : run(std::move(record))
    {
        for (const SushiProfiler::PeriodRecord& period : run.periods)
        {
            if (period.warmup)
                ++warmup;
            else
                ++active;
        }
        std::vector<SushiProfiler::SubjectSamples> found = SushiProfiler::subject_samples(run);
        subjects.reserve(found.size());
        for (SushiProfiler::SubjectSamples& item : found)
        {
            subjects.push_back(
                ProfileSubject{std::move(item.subject), std::move(item.unit),
                               std::move(item.samples)});
        }
    }

    ProfileRecord::ProfileRecord() = default;

    ProfileRecord::ProfileRecord(std::shared_ptr<const Impl> impl) noexcept
        : impl_(std::move(impl))
    {

    }

    bool ProfileRecord::empty() const noexcept
    {
        return impl_ == nullptr;
    }

    std::size_t ProfileRecord::active_periods() const noexcept
    {
        return impl_ == nullptr ? 0U : impl_->active;
    }

    std::size_t ProfileRecord::warmup_periods() const noexcept
    {
        return impl_ == nullptr ? 0U : impl_->warmup;
    }

    const std::vector<ProfileSubject>& ProfileRecord::subjects() const noexcept
    {
        static const std::vector<ProfileSubject> K_NONE;
        return impl_ == nullptr ? K_NONE : impl_->subjects;
    }

    const ProfileSubject* ProfileRecord::find(std::string_view name) const noexcept
    {
        for (const ProfileSubject& subject : subjects())
        {
            if (subject.name == name)
                return &subject;
        }
        return nullptr;
    }

    TimelineSummary ProfileRecord::summarise(std::string_view timeline) const
    {
        TimelineSummary summary;
        const ProfileSubject* const subject = find("timeline:" + std::string(timeline));
        if (subject == nullptr || subject->samples.empty())
            return summary;
        const SushiProfiler::SortedSamples sorted(subject->samples);
        summary.periods = sorted.size();
        summary.minimum_ns = sorted.minimum();
        summary.median_ns = sorted.median();
        summary.p99_ns = sorted.percentile(99.0);
        summary.maximum_ns = sorted.maximum();
        summary.jitter_ns = SushiProfiler::jitter(subject->samples);
        for (const SushiProfiler::PeriodRecord& period : impl_->run.periods)
        {
            if (!period.warmup && impl_->run.name_of(period.timeline) == timeline)
            {
                summary.budget_ns = static_cast<double>(period.budget_ns);
                break;
            }
        }
        return summary;
    }

    std::uint64_t ProfileRecord::zone_calls(std::string_view label) const
    {
        if (impl_ == nullptr)
            return 0U;
        std::uint64_t calls = 0U;
        for (const SushiProfiler::PeriodRecord& period : impl_->run.periods)
        {
            if (period.warmup)
                continue;
            for (const SushiProfiler::ZoneTree& tree : period.zones)
            {
                for (const SushiProfiler::ZoneNode& node : tree.nodes)
                {
                    const SushiProfiler::SiteRecord* const site = impl_->run.site(node.site);
                    const std::string_view node_label =
                        node.name != 0U ? impl_->run.name_of(node.name)
                                        : (site != nullptr ? std::string_view(site->name)
                                                           : std::string_view());
                    if (node_label == label)
                        calls += node.calls;
                }
            }
        }
        return calls;
    }

    std::vector<std::string> ProfileRecord::lanes() const
    {
        std::set<std::string> found;
        if (impl_ != nullptr)
        {
            for (const SushiProfiler::PeriodRecord& period : impl_->run.periods)
            {
                if (period.warmup)
                    continue;
                for (const SushiProfiler::ZoneTree& tree : period.zones)
                {
                    if (tree.lane != 0U)
                        found.insert(std::string(impl_->run.name_of(tree.lane)));
                }
            }
        }
        return std::vector<std::string>(found.begin(), found.end());
    }

    ProfileCheckReport ProfileRecord::check(std::string_view checks_json) const
    {
        ProfileCheckReport report;
        const SushiProfiler::ChecksReadResult read = SushiProfiler::read_checks(checks_json);
        if (!read.ok())
        {
            report.error = read.error;
            return report;
        }
        if (impl_ == nullptr)
        {
            report.error = "the run left no record";
            return report;
        }
        for (const SushiProfiler::Verdict& verdict : SushiProfiler::check(impl_->run, read.checks))
        {
            report.verdicts.push_back(ProfileVerdict{
                verdict.check.subject,
                std::string(SushiProfiler::statistic_word(verdict.check.statistic)),
                verdict.passed, verdict.value, verdict.reason});
        }
        return report;
    }
#else
    namespace
    {
        /// @brief Holds the subjects of a record that was never made.
        const std::vector<ProfileSubject> K_NO_SUBJECTS;
    } // namespace

    struct ProfileRecord::Impl
    {

    };

    ProfileRecord::ProfileRecord() = default;

    ProfileRecord::ProfileRecord(std::shared_ptr<const Impl> impl) noexcept
        : impl_(std::move(impl))
    {

    }

    bool ProfileRecord::empty() const noexcept
    {
        return true;
    }

    std::size_t ProfileRecord::active_periods() const noexcept
    {
        return 0U;
    }

    std::size_t ProfileRecord::warmup_periods() const noexcept
    {
        return 0U;
    }

    const std::vector<ProfileSubject>& ProfileRecord::subjects() const noexcept
    {
        return K_NO_SUBJECTS;
    }

    const ProfileSubject* ProfileRecord::find(std::string_view name) const noexcept
    {
        static_cast<void>(name);
        return nullptr;
    }

    TimelineSummary ProfileRecord::summarise(std::string_view timeline) const
    {
        static_cast<void>(timeline);
        return TimelineSummary{};
    }

    std::uint64_t ProfileRecord::zone_calls(std::string_view label) const
    {
        static_cast<void>(label);
        return 0U;
    }

    std::vector<std::string> ProfileRecord::lanes() const
    {
        return std::vector<std::string>();
    }

    ProfileCheckReport ProfileRecord::check(std::string_view checks_json) const
    {
        static_cast<void>(checks_json);
        ProfileCheckReport report;
        report.error = "this build records nothing";
        return report;
    }
#endif
} // namespace SushiDSP::profile
