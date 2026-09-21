/**************************************************************************/
/* Dd3Delay.cpp                                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file Dd3Delay.cpp
 * @brief Closes the DD-3's loop: emphasis, converter, compander and the clock that moves them.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/dd3/Dd3Delay.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <memory>

#include "SushiDSP/core/Calibration.hpp"

namespace SushiDSP
{
    namespace
    {
        /// @brief Time constant of the F.BACK and E.LEVEL smoothers, matching the other pedals.
        constexpr float kSmoothingMs = 20.0f;

        constexpr float kDefaultTime     = 0.3f;
        constexpr float kDefaultFeedback = 0.3f;
        constexpr float kDefaultLevel    = 0.5f;
        constexpr float kDefaultModeIndex = static_cast<float>(static_cast<int>(Dd3Delay::kDefaultMode));

        dd3::Dd3Mode mode_for_index(float value)
        {
            const long index = std::lround(static_cast<double>(value));
            return static_cast<dd3::Dd3Mode>(std::clamp<long>(index, 0, 3));
        }
    }

    Dd3Delay::Dd3Delay()
    {
        // The clock and the converter start from the members, so no default is written twice.
        clock_.set_time(time_);
        converter_.set_mode(mode_);
    }

    void Dd3Delay::apply_pending_mode()
    {
        const dd3::Dd3Mode pending = pending_mode_.load(std::memory_order_relaxed);
        if (pending == mode_)
        {
            return;
        }
        mode_ = pending;
        converter_.set_mode(mode_);
    }

    void Dd3Delay::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;

        pre_emphasis_.prepare(sample_rate_);
        anti_alias_.prepare(sample_rate_);
        compressor_.prepare(sample_rate_);
        expander_.prepare(sample_rate_);
        reconstruction_.prepare(sample_rate_);
        de_emphasis_.prepare(sample_rate_);
        converter_.prepare();
        clock_.prepare(sample_rate_);

        feedback_.prepare(sample_rate_, kSmoothingMs);
        level_.prepare(sample_rate_, kSmoothingMs);
        feedback_.snap(feedback_.target());
        level_.snap(level_.target());

        // Sizing the upward push at kFsScratchCapacity makes up_'s precondition structural.
        down_.prepare(sample_rate_, dd3::Dd3Clock::kFastRateHz, 1u,
                      dd3::Dd3Clock::kSlowRateHz / sample_rate_);
        up_.prepare(dd3::Dd3Clock::kFastRateHz, sample_rate_, kFsScratchCapacity,
                    sample_rate_ / dd3::Dd3Clock::kFastRateHz);

        prepared_ = true;
        reset();
    }

    void Dd3Delay::process(ProcessContext& context)
    {
        apply_pending_mode();

        const float* input = context.inputs[0];
        float* output = context.outputs[0];
        std::uint32_t offset = 0u;

        while (offset < context.frames)
        {
            if (frames_until_clock_ == 0u)
            {
                const double clock_hz = clock_.advance(kClockUpdateFrames);
                down_.set_rates(sample_rate_, clock_hz);
                up_.set_rates(clock_hz, sample_rate_);
                frames_until_clock_ = kClockUpdateFrames;
            }

            const std::uint32_t frames = std::min(frames_until_clock_, context.frames - offset);
            for (std::uint32_t i = 0; i < frames; ++i)
            {
                output[offset + i] = process_sample(input[offset + i]);
            }
            frames_until_clock_ -= frames;
            offset += frames;
        }
    }

    float Dd3Delay::process_sample(float input)
    {
        // The core's 2.0 V calibration, not dd3::Dd3Converter::kFullScaleVolts (2.5 V), the
        // converter's own ADC/DAC rail.
        const double dry = static_cast<double>(input) * SushiDSP::kFullScaleVolts;
        const double injected = static_cast<double>(feedback_.next())
                              * kMaximumFeedback * kFeedbackPathGain * wet_previous_;
        // The input buffer sits ahead of the summing node, so only the dry branch carries it.
        const float summed = static_cast<float>(dry * kInputPathGainToCheckPointOne + injected);

        float drive = pre_emphasis_.process_sample(summed);
        drive = anti_alias_.process_sample(drive);
        drive = compressor_.process_sample(drive);

        const float delayed = convert_sample(drive);
        const float expanded = static_cast<float>(static_cast<double>(expander_.process_sample(delayed))
                                                  * kExpanderOutputStageGain);
        const float reconstructed = reconstruction_.process_sample(expanded);

        // Dossier 1.6 reading (b): F.BACK and E.LEVEL take off at the wet leg's output.
        const float wet = de_emphasis_.process_sample(reconstructed);
        wet_previous_ = static_cast<double>(wet);

        return static_cast<float>((static_cast<double>(wet) * static_cast<double>(level_.next())
                                   * static_cast<double>(kLevelUnityGain) + dry)
                                   / SushiDSP::kFullScaleVolts);
    }

    float Dd3Delay::convert_sample(float drive)
    {
        const std::uint32_t produced =
            down_.process(&drive, 1u, fs_scratch_.data(), kFsScratchCapacity);
        for (std::uint32_t i = 0; i < produced; ++i)
        {
            fs_scratch_[i] = converter_.process_sample(fs_scratch_[i]);
        }

        const std::uint32_t returned =
            up_.process(fs_scratch_.data(), produced, wet_scratch_.data(), kWetScratchCapacity);
        for (std::uint32_t i = 0; i < returned; ++i)
        {
            wet_fifo_.push(wet_scratch_[i]);
        }
        return wet_fifo_.pop();
    }

    PortLayout Dd3Delay::ports() const
    {
        return { 1, 1 };
    }

    void Dd3Delay::reset()
    {
        pre_emphasis_.reset();
        anti_alias_.reset();
        compressor_.reset();
        expander_.reset();
        reconstruction_.reset();
        de_emphasis_.reset();
        converter_.reset();
        clock_.reset();

        wet_fifo_.reset();
        frames_until_clock_ = 0u;
        wet_previous_ = 0.0;

        if (prepared_)
        {
            down_.reset();
            up_.reset();
            const double clock_hz = clock_.sample_rate_hz();
            down_.set_rates(sample_rate_, clock_hz);
            up_.set_rates(clock_hz, sample_rate_);
        }
    }

    std::uint32_t Dd3Delay::latency_samples() const
    {
        return 0u;
    }

    const char* Dd3Delay::type_id() const { return "sushidsp.dd3"; }

    void Dd3Delay::set_time(float normalized)
    {
        time_ = std::clamp(normalized, 0.0f, 1.0f);
        clock_.set_time(time_);
    }

    void Dd3Delay::set_feedback(float normalized)
    {
        feedback_.set_target(std::clamp(normalized, 0.0f, 1.0f));
    }

    void Dd3Delay::set_level(float normalized)
    {
        level_.set_target(std::clamp(normalized, 0.0f, 1.0f));
    }

    void Dd3Delay::set_mode(dd3::Dd3Mode mode)
    {
        pending_mode_.store(mode, std::memory_order_relaxed);
        apply_pending_mode();
    }

    double Dd3Delay::wet_delay_seconds() const
    {
        assert(prepared_ && "Dd3Delay::wet_delay_seconds requires prepare() first");
        if (!prepared_)
        {
            return 0.0;
        }

        const double clock_hz = clock_.sample_rate_hz();
        const double ring = static_cast<double>(converter_.ring_length()) / clock_hz;
        // Both conversions are zero phase, so each adds only the half kernel span it waits for.
        const double round_trip = down_.latency_seconds() + up_.latency_seconds();
        const double prime = static_cast<double>(kWetPrimeFrames) / sample_rate_;
        return ring + round_trip + prime;
    }

    const ParameterTable<Dd3Delay, 4>& Dd3Delay::parameters()
    {
        static const ParameterTable<Dd3Delay, 4> table{ {{
            { { "time", "D.Time", 0.0f, 1.0f, kDefaultTime, ParamKind::Linear, "" },
              [](Dd3Delay& n, float v) { n.set_time(v); },
              [](const Dd3Delay& n) { return n.time_; } },
            { { "feedback", "F.Back", 0.0f, 1.0f, kDefaultFeedback, ParamKind::Linear, "" },
              [](Dd3Delay& n, float v) { n.set_feedback(v); },
              [](const Dd3Delay& n) { return n.feedback_.target(); } },
            { { "level", "E.Level", 0.0f, 1.0f, kDefaultLevel, ParamKind::Linear, "" },
              [](Dd3Delay& n, float v) { n.set_level(v); },
              [](const Dd3Delay& n) { return n.level_.target(); } },
            // Recorded only: clearing 16384 samples inside the host's drain is not RT-safe, so
            // this row stores the pending mode rather than applying it (see apply_pending_mode).
            { { "mode", "Mode", 0.0f, 3.0f, kDefaultModeIndex, ParamKind::Choice, "" },
              [](Dd3Delay& n, float v) {
                  n.pending_mode_.store(mode_for_index(v), std::memory_order_relaxed);
              },
              [](const Dd3Delay& n) {
                  return static_cast<float>(
                      static_cast<int>(n.pending_mode_.load(std::memory_order_relaxed)));
              } },
        }} };
        return table;
    }

    float Dd3Delay::get_parameter(const char* id) const { return parameters().read(*this, id); }

    void Dd3Delay::set_parameter_for_control_thread(const char* id, float value)
    {
        parameters().write(*this, id, value);
        // Only a MODE write clears the ring here; another id would do it under a live process().
        if (std::strcmp(id, "mode") == 0)
        {
            apply_pending_mode();
        }
    }

    void Dd3Delay::set_parameter_for_audio_thread(std::uint16_t index, float value)
    {
        parameters().write(*this, index, value);
    }

    namespace
    {
        std::unique_ptr<IAudioNode> make_dd3(const PrepareInfo&)
        {
            return std::make_unique<Dd3Delay>();
        }
    }

    const NodeDescriptor& dd3_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.dd3",
            "DD-3 Digital Delay",
            Dd3Delay::parameters().descriptors(),
            Dd3Delay::parameters().count(),
            PortLayout{ 1, 1 },
            &make_dd3,
        };
        return descriptor;
    }
}
