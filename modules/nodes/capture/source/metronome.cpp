/**************************************************************************/
/* metronome.cpp                                                          */
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
 * @file metronome.cpp
 * @brief Implements the metronome's click bursts and their placement on the beat grid.
 * @author Mustafa Garip
 */

#include <SushiDSP/nodes/capture/metronome.hpp>

#include <cmath>

namespace SushiDSP
{
    namespace
    {
        constexpr double K_FIXED_POINT_SCALE = 4294967296.0;
        constexpr double K_DEFAULT_SAMPLE_RATE = 48000.0;
        constexpr double K_TWO_PI = 6.283185307179586;
        constexpr int K_INDEX_CORRECTION_LIMIT = 4;
    } // namespace

    Metronome::Metronome() : Metronome(K_DEFAULT_SAMPLE_RATE) {}

    Metronome::Metronome(double sample_rate)
        : downbeat_burst_(), beat_burst_(),
          sample_rate_(std::isfinite(sample_rate) && sample_rate > 0.0 ? sample_rate
                                                                       : K_DEFAULT_SAMPLE_RATE),
          burst_position_(0), burst_is_downbeat_(false)
    {
        build_burst(downbeat_burst_, K_DOWNBEAT_HERTZ);
        build_burst(beat_burst_, K_BEAT_HERTZ);
        burst_position_ = beat_burst_.size();
    }

    void Metronome::set_sample_rate(double sample_rate)
    {
        if (!std::isfinite(sample_rate) || sample_rate <= 0.0)
        {
            return;
        }

        sample_rate_ = sample_rate;
        build_burst(downbeat_burst_, K_DOWNBEAT_HERTZ);
        build_burst(beat_burst_, K_BEAT_HERTZ);
        burst_position_ = beat_burst_.size();
        burst_is_downbeat_ = false;
    }

    void Metronome::build_burst(std::vector<float>& burst, double frequency_hertz)
    {
        const auto length = static_cast<std::size_t>(std::llround(K_BURST_SECONDS * sample_rate_));
        const double attack_frames = K_ATTACK_SECONDS * sample_rate_;

        burst.assign(length, 0.0f);

        for (std::size_t index = 0; index < length; ++index)
        {
            // The burst is sampled from one frame in; see modules/nodes/capture/README.md.
            const double seconds = static_cast<double>(index + 1) / sample_rate_;
            const double attack =
                static_cast<double>(index + 1) < attack_frames
                    ? 0.5 * (1.0 - std::cos(3.141592653589793 * static_cast<double>(index + 1) /
                                            attack_frames))
                    : 1.0;
            const double decay = std::exp(-seconds / K_DECAY_SECONDS);
            burst[index] =
                static_cast<float>(std::sin(K_TWO_PI * frequency_hertz * seconds) * attack * decay);
        }
    }

    double Metronome::sample_rate() const noexcept
    {
        return sample_rate_;
    }

    void Metronome::set_enabled(bool enabled) noexcept
    {
        enabled_.store(enabled, std::memory_order_release);
    }

    bool Metronome::enabled() const noexcept
    {
        return enabled_.load(std::memory_order_acquire);
    }

    void Metronome::set_level(float level) noexcept
    {
        level_.store(level > 0.0f ? level : 0.0f, std::memory_order_release);
    }

    float Metronome::level() const noexcept
    {
        return level_.load(std::memory_order_acquire);
    }

    std::uint64_t Metronome::beat_frame(std::uint64_t index, std::uint64_t step) noexcept
    {
        return (index * step) >> 32;
    }

    std::uint64_t Metronome::first_beat_at_or_after(std::uint64_t position,
                                                    std::uint64_t step) noexcept
    {
        const double frames_per_beat = static_cast<double>(step) / K_FIXED_POINT_SCALE;
        std::uint64_t index =
            static_cast<std::uint64_t>(static_cast<double>(position) / frames_per_beat);

        for (int guard = 0; guard < K_INDEX_CORRECTION_LIMIT && index > 0 &&
                            beat_frame(index - 1u, step) >= position;
             ++guard)
        {
            --index;
        }

        for (int guard = 0; guard < K_INDEX_CORRECTION_LIMIT && beat_frame(index, step) < position;
             ++guard)
        {
            ++index;
        }

        return index;
    }

    void Metronome::render_burst(float* const* outputs, std::uint32_t channels,
                                 std::uint32_t offset, std::uint32_t frames, float gain) noexcept
    {
        const std::vector<float>& burst = burst_is_downbeat_ ? downbeat_burst_ : beat_burst_;

        for (std::uint32_t frame = offset; frame < frames; ++frame)
        {
            if (burst_position_ >= burst.size())
            {
                return;
            }

            const float sample = burst[burst_position_] * gain;
            ++burst_position_;

            for (std::uint32_t channel = 0; channel < channels; ++channel)
            {
                outputs[channel][frame] += sample;
            }
        }
    }

    void Metronome::process(float* const* outputs, std::uint32_t channels, std::uint32_t frames,
                            const Transport& transport, const MusicalClock& clock) noexcept
    {
        if (!enabled_.load(std::memory_order_acquire) ||
            transport.state() == TransportState::Stopped || outputs == nullptr || channels == 0 ||
            frames == 0)
        {
            return;
        }

        const std::uint64_t step = clock.beat_step_q32();

        if (step == 0)
        {
            return;
        }

        const float gain = level_.load(std::memory_order_acquire);
        const std::uint64_t position = transport.position_frames();
        const std::uint64_t end = position + frames;
        const std::uint64_t beats_per_bar = clock.beats_per_bar();

        render_burst(outputs, channels, 0u, frames, gain);

        for (std::uint64_t index = first_beat_at_or_after(position, step);; ++index)
        {
            const std::uint64_t frame = beat_frame(index, step);

            if (frame >= end)
            {
                return;
            }

            burst_position_ = 0;
            burst_is_downbeat_ = beats_per_bar != 0 && index % beats_per_bar == 0;
            render_burst(outputs, channels, static_cast<std::uint32_t>(frame - position), frames,
                         gain);
        }
    }
} // namespace SushiDSP
