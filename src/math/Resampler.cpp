/**************************************************************************/
/* Resampler.cpp                                                          */
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
 * @file Resampler.cpp
 * @brief Arbitrary-ratio polyphase resampler over the committed prototype filter.
 * @author Mustafa Garip & Sushi Systems
 */

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "SushiDSP/math/Resampler.hpp"
#include "SushiDSP/math/ResamplerCoefficients.hpp"

namespace SushiDSP
{
    namespace
    {
        /// @brief Smallest power of two that holds at least `minimum` samples, so the ring
        /// index can wrap with a mask.
        std::uint32_t ring_capacity(std::size_t minimum)
        {
            std::uint32_t capacity = 1u;
            while (capacity < minimum)
            {
                capacity *= 2u;
            }
            return capacity;
        }

        /// @brief The prototype's half width as a double, which every span below is measured in.
        constexpr double kHalfWidth = static_cast<double>(kResamplerHalfWidthZeroCrossings);
    }

    void Resampler::prepare(double source_rate, double target_rate, std::uint32_t max_input_frames,
                            double minimum_ratio)
    {
        assert(minimum_ratio > 0.0 && "prepare() needs a positive minimum ratio");

        minimum_ratio_ = minimum_ratio;
        max_input_frames_ = max_input_frames;

        // The slowest ratio gives the widest kernel span, so it is what the history is sized on.
        // Interpolating never narrows that span below the prototype's own half width, so a
        // minimum ratio above 1 sizes the history exactly as a minimum ratio of 1 does.
        const double widest_half_span = kHalfWidth / std::min(1.0, minimum_ratio_);
        const std::size_t required = 2u * static_cast<std::size_t>(std::ceil(widest_half_span))
                                   + static_cast<std::size_t>(max_input_frames) + 4u;
        history_.assign(ring_capacity(required), 0.0f);
        history_mask_ = static_cast<std::uint32_t>(history_.size()) - 1u;

        set_rates(source_rate, target_rate);
        reset();
    }

    void Resampler::set_rates(double source_rate, double target_rate)
    {
        assert(source_rate > 0.0 && "set_rates() needs a positive source rate");

        source_rate_ = source_rate;
        ratio_ = std::min(std::max(target_rate / source_rate, minimum_ratio_), kMaximumRatio);
        target_rate_ = source_rate_ * ratio_;
        scale_ = std::min(1.0, ratio_);                                        // scale_
        step_ = 1.0 / ratio_;                                                  // step_
        half_span_ = kHalfWidth / scale_;                                      // half_span_
        taps_per_side_ = static_cast<std::uint32_t>(std::ceil(half_span_));    // taps_per_side_

        // The ring holds history_.size() samples ending at the newest input, and interpolate()
        // reaches half_span_ + 1 below the phase. Costing that against max_input_frames_ rather
        // than the push in hand makes the floor conservative by up to max_input_frames_ minus
        // that push, which errs towards dropping a deferred output the ring could still have
        // formed and never towards reading a slot it has overwritten.
        position_floor_ = static_cast<double>(max_input_frames_)
                        - static_cast<double>(history_.size()) + half_span_ + 1.0;

        // position_ and the history stay as they are: a mid-stream rate change is the DD-3's
        // pitch glide, and resetting the phase there is the click the design exists to avoid.
    }

    double Resampler::kernel_at(double distance) const
    {
        const double x = distance * static_cast<double>(kResamplerPhasesPerZeroCrossing);
        // The table's last entry is a guard, so clamping to the one before it keeps i + 1 in bounds.
        const std::uint32_t i = std::min(static_cast<std::uint32_t>(x), kResamplerKernelLength - 2u);
        const double f = x - static_cast<double>(i);
        const double lower = static_cast<double>(kResamplerKernel[i]);
        const double upper = static_cast<double>(kResamplerKernel[i + 1u]);
        return lower + f * (upper - lower);
    }

    std::uint32_t Resampler::ring_index(std::uint32_t origin, std::int32_t offset) const
    {
        // Unsigned wraparound is the modular arithmetic wanted here: the ring length divides 2^32.
        return (origin + static_cast<std::uint32_t>(offset)) & history_mask_;
    }

    double Resampler::interpolate(std::uint32_t origin, double t) const
    {
        const double left_time = std::floor(t);                                // left
        const std::int32_t left = static_cast<std::int32_t>(left_time);
        const double frac = t - left_time;                                     // frac
        double sum = 0.0;

        for (std::uint32_t k = 0; k < taps_per_side_; ++k)                     // taps at and below t
        {
            const double distance = (frac + static_cast<double>(k)) * scale_;
            // taps_per_side_ is a ceiling, so the last tap of a loop can sit on the kernel's far zero.
            if (distance >= kHalfWidth)
            {
                break;
            }
            const std::int32_t offset = left - static_cast<std::int32_t>(k);
            sum += static_cast<double>(history_[ring_index(origin, offset)]) * kernel_at(distance);
        }

        for (std::uint32_t k = 0; k < taps_per_side_; ++k)                     // taps above t
        {
            const double distance = ((1.0 - frac) + static_cast<double>(k)) * scale_;
            if (distance >= kHalfWidth)
            {
                break;
            }
            const std::int32_t offset = left + 1 + static_cast<std::int32_t>(k);
            sum += static_cast<double>(history_[ring_index(origin, offset)]) * kernel_at(distance);
        }

        return sum * scale_;                    // the scale keeps the DC gain at 1 when decimating
    }

    std::uint32_t Resampler::process(const float* input, std::uint32_t input_frames,
                                     float* output, std::uint32_t output_capacity)
    {
        assert(input_frames <= max_input_frames_ && "process() exceeds the prepared push size");

        // A set_rates() that lowers the ratio widens half_span_ and so raises the floor, which
        // can leave a phase the previous call pinned sitting below it. Re-clamp before forming
        // anything: the floor in force is the one the ratio in hand asks for.
        position_ = std::max(position_, position_floor_);

        for (std::uint32_t frame = 0; frame < input_frames; ++frame)  // append input to the history ring
        {
            history_[write_index_ & history_mask_] = input[frame];
            ++write_index_;
        }

        // Input sample j of this call sits at ring slot origin + j and at time j.
        const std::uint32_t origin = write_index_ - input_frames;
        const double newest_time = static_cast<double>(input_frames) - 1.0;
        std::uint32_t produced = 0u;
        while (position_ + half_span_ <= newest_time && produced < output_capacity)
        {
            output[produced] = static_cast<float>(interpolate(origin, position_));
            ++produced;
            position_ += step_;
        }

        // An under-sized output buffer stops the loop without moving position_ past what was
        // written, so the next call resumes on the same output instead of dropping it. The ring
        // bounds how much backlog that can carry: below position_floor_ the deferred outputs
        // would read history the ring has overwritten, so they are dropped. Clamping here as
        // well as on entry keeps the phase bounded across a long starved run.
        position_ -= static_cast<double>(input_frames);   // rebase into the next call's coordinates
        position_ = std::max(position_, position_floor_);
        return produced;
    }

    std::uint32_t Resampler::maximum_output_frames(std::uint32_t input_frames) const
    {
        return static_cast<std::uint32_t>(std::ceil(static_cast<double>(input_frames) * ratio_)) + 2u;
    }

    double Resampler::latency_seconds() const
    {
        return kHalfWidth / std::min(source_rate_, target_rate_);
    }

    std::uint32_t Resampler::latency_samples() const
    {
        return static_cast<std::uint32_t>(std::lround(latency_seconds() * source_rate_));
    }

    void Resampler::reset()
    {
        std::fill(history_.begin(), history_.end(), 0.0f);
        write_index_ = 0u;
        position_ = 0.0;
    }
}
