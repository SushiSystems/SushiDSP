/****************************************************************/
/* VariRateReader.hpp                                           */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file VariRateReader.hpp
 * @brief Reads a Take at a chosen playback speed, one Resampler per channel.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>

#include "SushiDSP/math/Resampler.hpp"

#include "SushiDSP/capture/Take.hpp"

namespace SushiDSP
{
    /// @brief Reads a Take at a chosen speed, one Resampler per channel, in exact output counts.
    ///
    /// The pitch falls with the speed and nothing here stretches time, which
    /// docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md §5a states as the behaviour
    /// wanted rather than a limitation. Every buffer is sized in prepare(); read() and reset()
    /// are allocation-, lock-, syscall- and throw-free.
    class VariRateReader
    {
    public:
        /// @brief Bounds the slowest playback speed read() will honour.
        static constexpr double kMinSpeed = 0.5;

        /// @brief Bounds the fastest playback speed read() will honour.
        static constexpr double kMaxSpeed = 2.0;

        /// @brief Caps how many channels one reader converts.
        static constexpr std::uint32_t kMaxChannels = 2u;

        /// @brief Sizes the converters for the worst case they will be asked for. Control
        /// thread; allocates.
        /// @param channels Channels to convert; anything above kMaxChannels is capped there.
        /// @param max_output_frames Largest output_frames a single read() will carry.
        void prepare(std::uint32_t channels, std::uint32_t max_output_frames);

        /// @brief Discards the converter history and the phase, so the next read() starts from
        /// silence. Audio thread; call it after a seek, a speed change or a move to another clip.
        void reset() noexcept;

        /// @brief Reads exactly output_frames samples per channel, starting at take_frame.
        /// @param speed Take frames consumed per output frame; clamped to [kMinSpeed, kMaxSpeed].
        /// @param outputs One writable buffer of at least output_frames samples per channel.
        /// @pre prepare() has been called with at least channels and output_frames.
        /// @return The take frame the next call should start at.
        /// @see modules/nodes/capture/README.md, "What a `VariRateReader::read` costs"
        std::uint64_t read(const Take& take, std::uint64_t take_frame, double speed,
                           float* const* outputs, std::uint32_t channels,
                           std::uint32_t output_frames) noexcept;

        /// @brief Reports the speed the converters are currently set to. Either thread.
        /// @return The clamped speed the last read() ran at, 1.0 before the first.
        double speed() const noexcept { return speed_; }

    private:
        /// @brief Reads one take sample, or zero past the take's end.
        /// @param take The recording to read.
        /// @param channel Output channel index, folded onto a channel the take carries.
        /// @param frame Take frame to read.
        /// @return The sample, or 0 outside the take.
        static float sample_at(const Take& take, std::uint32_t channel, std::uint64_t frame) noexcept;

        /// @brief Holds one converter per channel, each carrying that channel's kernel history.
        std::array<Resampler, kMaxChannels> converters_{};

        /// @brief Holds how many converters prepare() sized.
        std::uint32_t prepared_channels_ = 0u;

        /// @brief Holds the largest output block prepare() was told to expect.
        std::uint32_t max_output_frames_ = 0u;

        /// @brief Holds the clamped speed the converters are set to, so an unchanged speed
        /// costs no set_rates() call.
        double speed_ = 1.0;
    };
}
