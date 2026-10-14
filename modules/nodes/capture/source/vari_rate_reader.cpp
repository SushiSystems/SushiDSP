/**************************************************************************/
/* vari_rate_reader.cpp                                                   */
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
 * @file vari_rate_reader.cpp
 * @brief Reads a Take at a chosen playback speed, one Resampler per channel.
 * @author Mustafa Garip & Sushi Systems
 */

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>

#include <SushiDSP/foundation/math/resampler_design.hpp>

#include <SushiDSP/nodes/capture/transport.hpp>
#include <SushiDSP/nodes/capture/vari_rate_reader.hpp>

namespace SushiDSP
{
    static_assert(Transport::K_MIN_SPEED == VariRateReader::K_MIN_SPEED,
                  "Transport and VariRateReader must clamp the speed to the same floor");
    static_assert(Transport::K_MAX_SPEED == VariRateReader::K_MAX_SPEED,
                  "Transport and VariRateReader must clamp the speed to the same ceiling");

    namespace
    {
        /// @brief Pushes past the steady-state cost that one read() may need: the kernel's half
        /// span at the widest ratio, counted both sides, plus room for a deferred output.
        constexpr std::uint64_t K_INPUT_BOUND_SLACK =
            2u * (static_cast<std::uint64_t>(
                      static_cast<double>(K_RESAMPLER_HALF_WIDTH_ZERO_CROSSINGS) *
                      VariRateReader::K_MAX_SPEED) +
                  1u) +
            8u;
    } // namespace

    void VariRateReader::prepare(std::uint32_t channels, std::uint32_t max_output_frames)
    {
        prepared_channels_ = std::min(channels, K_MAX_CHANNELS);
        max_output_frames_ = max_output_frames;
        speed_ = 1.0;

        for (std::uint32_t channel = 0; channel < prepared_channels_; ++channel)
        {
            converters_[channel].prepare(1.0, 1.0, 1u, 1.0 / K_MAX_SPEED);
        }
    }

    void VariRateReader::reset() noexcept
    {
        for (std::uint32_t channel = 0; channel < prepared_channels_; ++channel)
        {
            converters_[channel].reset();
        }
    }

    float VariRateReader::sample_at(const Take& take, std::uint32_t channel,
                                    std::uint64_t frame) noexcept
    {
        if (take.channels() == 0u || frame >= take.frames())
        {
            return 0.0f;
        }
        const std::uint32_t source = std::min(channel, take.channels() - 1u);
        return take.channel(source)[static_cast<std::size_t>(frame)];
    }

    std::uint64_t VariRateReader::read(const Take& take, std::uint64_t take_frame, double speed,
                                       float* const* outputs, std::uint32_t channels,
                                       std::uint32_t output_frames) noexcept
    {
        assert(output_frames <= max_output_frames_ && "read() exceeds the prepared block size");

        if (outputs == nullptr || channels == 0u || output_frames == 0u || prepared_channels_ == 0u)
        {
            return take_frame;
        }

        const double clamped = std::min(std::max(speed, K_MIN_SPEED), K_MAX_SPEED);
        if (clamped != speed_)
        {
            for (std::uint32_t channel = 0; channel < prepared_channels_; ++channel)
            {
                converters_[channel].set_rates(clamped, 1.0);
            }
            speed_ = clamped;
        }

        const std::uint32_t converted = std::min(channels, prepared_channels_);
        for (std::uint32_t channel = converted; channel < channels; ++channel)
        {
            std::fill(outputs[channel], outputs[channel] + output_frames, 0.0f);
        }

        // Every converter emits the same count per push; see modules/nodes/capture/README.md.
        const std::uint64_t input_bound = static_cast<std::uint64_t>(std::ceil(
                                              static_cast<double>(output_frames) * K_MAX_SPEED)) +
                                          K_INPUT_BOUND_SLACK;
        std::uint32_t produced = 0u;
        std::uint64_t consumed = 0u;

        while (produced < output_frames && consumed < input_bound)
        {
            std::uint32_t written = 0u;
            for (std::uint32_t channel = 0; channel < converted; ++channel)
            {
                const float input = sample_at(take, channel, take_frame + consumed);
                written = converters_[channel].process(&input, 1u, outputs[channel] + produced,
                                                       output_frames - produced);
            }
            ++consumed;
            produced += written;
        }

        for (std::uint32_t channel = 0; channel < converted; ++channel)
        {
            std::fill(outputs[channel] + produced, outputs[channel] + output_frames, 0.0f);
        }

        return take_frame + consumed;
    }
} // namespace SushiDSP
