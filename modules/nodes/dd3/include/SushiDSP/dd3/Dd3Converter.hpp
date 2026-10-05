/****************************************************************/
/* Dd3Converter.hpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file Dd3Converter.hpp
 * @brief The DD-3's 12-bit A/D-D/A characteristic and fixed-length ring.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstdint>
#include <vector>

namespace SushiDSP::dd3
{
    /// @brief The mode switch's four positions (dossier 1.8, S1 SBM1024 truth table).
    enum class Dd3Mode
    {
        Short,
        Middle,
        Long,
        Hold
    };

    /// @brief Models the SBM1024's 12-bit quantiser and its S/M/L/H delay ring.
    ///
    /// Ruling 13: the SBM1024 is a plain uniform mid-tread quantiser here; the R-2R ladder's
    /// per-bit DNL has no measured target yet, so it is not modelled — a table would replace
    /// @ref quantise's linear step if one is ever measured. Ruling 14: MODE changes swap the
    /// ring length and clear it, except that Long and Hold share one length, so switching
    /// between them must not clear the loop HOLD is meant to freeze. Ruling 15: the converter
    /// saturates at both rails rather than wrapping on overflow.
    ///
    /// State-machine note: @ref set_mode only ever records the requested mode; it resizes or
    /// clears @ref ring_ itself only once @ref prepare has allocated it (`ring_` is otherwise
    /// empty, whatever mode was requested), so a mode selected before @ref prepare cannot leave
    /// `length_` pointing past an empty ring. @ref process_sample requires @ref prepare to have
    /// run first; on an unprepared object it asserts in debug and, in release, returns 0.0F
    /// without touching `ring_` — the empty-ring check that makes this safe is unconditional,
    /// not compiled out, so the out-of-bounds read cannot happen in either build.
    class Dd3Converter
    {
    public:
        /// @brief Allocates the ring at its maximum length and applies the pending mode.
        /// Control thread only.
        void prepare();

        /// @brief Selects the buffer length, clearing the ring only when N changes (Ruling 14).
        /// Before @ref prepare has run this only records the mode for @ref prepare to apply.
        /// @param mode The new mode; Long and Hold share N = 16384, so switching between them
        /// keeps whatever the loop holds, which is what HOLD is for.
        void set_mode(Dd3Mode mode);

        /// @brief Quantises, writes (unless HOLD) and reads one sample at the ring's clock.
        /// @param volts The sample to store, in volts at the converter's own scale.
        /// @return What the ring held N samples ago, dequantised; 0.0F if @ref prepare has not
        /// run yet.
        /// @pre prepare() has been called at least once.
        float process_sample(float volts);

        /// @brief Zeroes the ring without changing the mode.
        void reset();

        /// @brief The active buffer length in samples.
        /// @return 1024, 4096 or 16384.
        std::uint32_t ring_length() const;

        /// @brief The 12-bit mid-tread characteristic, saturating at both rails (Ruling 15).
        /// @param volts Input voltage.
        /// @return The dequantised voltage.
        static double quantise(double volts);

        static constexpr double        kFullScaleVolts    = 2.5;
        static constexpr double        kStepVolts         = 5.0 / 4096.0;
        static constexpr std::int32_t  kMaximumCode       = 2047;
        static constexpr std::int32_t  kMinimumCode       = -2048;
        static constexpr std::uint32_t kShortLength       = 1024u;
        static constexpr std::uint32_t kMiddleLength      = 4096u;
        static constexpr std::uint32_t kLongLength        = 16384u;
        static constexpr std::uint32_t kMaximumRingLength = 16384u;

    private:
        /// @brief Maps a mode to its ring length, Long and Hold sharing kLongLength.
        static std::uint32_t length_for_mode(Dd3Mode mode);

        std::vector<float> ring_;
        std::uint32_t      length_ = kLongLength;
        std::uint32_t      index_  = 0u;
        Dd3Mode            mode_   = Dd3Mode::Long;
    };
}
