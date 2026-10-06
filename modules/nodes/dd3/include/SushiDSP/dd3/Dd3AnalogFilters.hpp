/**************************************************************************/
/* Dd3AnalogFilters.hpp                                                   */
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
 * @file Dd3AnalogFilters.hpp
 * @brief The DD-3's four linear networks: two emphasis shelves and two Sallen-Key cascades.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include "SushiDSP/math/Biquad.hpp"

#include <cstddef>

namespace SushiDSP::dd3
{
    /// @brief Series resistance of every Sallen-Key section: R9-R11, R20-R25, R44-R49.
    inline constexpr double kSallenKeyResistanceOhms = 10000.0;

    /// @brief Anti-alias corners f0 = 1/(2*pi*R*sqrt(C1*C2)) for Q3, Q4, Q6.
    ///
    /// Component values in the dossier's 1.3: C9 4700p / C8 2700p, C15 6800p / C14 470p,
    /// C17 0.018u / C16 220p, all against 10 kOhm.
    inline constexpr double kAntiAliasSectionFrequencies[3] = { 4467.754702402405,
                                                                8902.597656125990,
                                                                7997.836814639136 };

    /// @brief Anti-alias Q = 0.5*sqrt(C1/C2) for Q3, Q4, Q6, taking the buffer as ideal.
    inline constexpr double kAntiAliasSectionQ[3] = { 0.6596856715021064,
                                                      1.9018468068158760,
                                                      4.5226701686664540 };

    /// @brief Reconstruction corners for Q10 and Q9, dossier 1.4.
    ///
    /// C38 2700p / C37 1200p and C35 6800p / C36 150p, again against 10 kOhm.
    inline constexpr double kReconstructionSectionFrequencies[2] = { 8841.941282883075,
                                                                     15758.687585503258 };

    /// @brief Reconstruction Q for Q10 and Q9, taking the buffer as ideal.
    inline constexpr double kReconstructionSectionQ[2] = { 0.7500000000000000,
                                                           3.3665016461206925 };

    /// @brief Effective damping factor of each Sallen-Key section's emitter-follower buffer.
    ///
    /// Not the follower's loaded voltage gain, which is around 0.987; it absorbs the
    /// follower's output impedance and its base-current loading of the shunt cap, and it is
    /// fitted, because the window is a knife edge in it: 0.98 would put the delay path
    /// +3.18 dB at 7 kHz. The published window bounds it to 0.9431071-0.9665162 and this is
    /// the midpoint. Derivation, interval and provenance:
    /// docs/archive/superpowers/specs/2026-09-05-dd3-spx90-design.md, DR2 model corrections.
    inline constexpr double kEmitterFollowerGain = 0.9548116;

    /// @brief The band edge both schematic legends and the published response name, 7 kHz.
    ///
    /// Every section prewarps its bilinear corner here instead of at its own f0. That holds
    /// the delay path's 7 kHz point to a 0.005 dB spread over 48, 96 and 192 kHz, against
    /// 1.339 dB per-section, and leaves 1.794 dB of margin to the published -3 dB limit at
    /// 48 kHz against 0.366 dB. See the same DR2 model-corrections section.
    inline constexpr double kPassbandEdgeHz = 7000.0;

    /// @brief Emphasis zero, 1/(2*pi*(R7+R8)*C4) with R7 47K, R8 10K, C4 0.0047u.
    inline constexpr double kEmphasisZeroHz = 594.0834008656042;

    /// @brief Emphasis pole, 1/(2*pi*R8*C4).
    inline constexpr double kEmphasisPoleHz = 3386.2753849339438;

    /// @brief Pre-emphasis gain below both corners, R12/R7.
    inline constexpr double kPreEmphasisLowGain = 33.0 / 47.0;

    /// @brief De-emphasis gain below both corners, R16/R14.
    inline constexpr double kDeEmphasisLowGain = 47.0 / 33.0;

    /// @brief Q of one section once the emitter follower's finite gain is carried.
    /// @param unity_gain_q The section's Q for an ideal buffer, 0.5*sqrt(C1/C2).
    /// @return 1 / (1/q + 2*(1 - kEmitterFollowerGain)*q), which is below q.
    double emitter_follower_damped_q(double unity_gain_q);

    /// @brief Digital corner that puts a section on the cascade-wide prewarp.
    /// @param corner_hz The section's analogue f0 in Hz.
    /// @param sample_rate Sample rate in Hz.
    /// @pre sample_rate > 2 * kPassbandEdgeHz, so the anchor stays below Nyquist.
    /// @return The corner for Biquad::set_low_pass; equals corner_hz at kPassbandEdgeHz.
    double prewarped_corner_hz(double corner_hz, double sample_rate);

    /// @brief IC1b's pre-emphasis shelf, the treble boost ahead of the converter.
    ///
    /// One first-order shelf from kEmphasisZeroHz to kEmphasisPoleHz. C5 33 pF across R12
    /// is dropped: its pole is at 146.148 kHz, above Nyquist at 48 and 96 kHz alike.
    class Dd3PreEmphasis
    {
    public:
        /// @brief Places the shelf at this rate. Control thread only.
        /// @param sample_rate Sample rate in Hz.
        void prepare(double sample_rate);

        /// @brief Clears the shelf's state.
        void reset();

        /// @brief Filters one sample. Real-time safe.
        /// @param input The input sample.
        /// @return The boosted sample.
        float process_sample(float input);

    private:
        SushiDSP::Biquad shelf_;
    };

    /// @brief IC1a's de-emphasis shelf, the treble cut after the converter.
    ///
    /// The same two corners swapped and the reciprocal low-frequency gain, so a
    /// Dd3PreEmphasis in front of it cancels to the last bit. C12 33 pF is dropped for
    /// the same reason as C5: its pole is at 102.614 kHz.
    class Dd3DeEmphasis
    {
    public:
        /// @brief Places the shelf at this rate. Control thread only.
        /// @param sample_rate Sample rate in Hz.
        void prepare(double sample_rate);

        /// @brief Clears the shelf's state.
        void reset();

        /// @brief Filters one sample. Real-time safe.
        /// @param input The input sample.
        /// @return The cut sample.
        float process_sample(float input);

    private:
        SushiDSP::Biquad shelf_;
    };

    /// @brief The 6th-order anti-alias filter ahead of the converter: Q3, Q4 and Q6.
    ///
    /// Three equal-R Sallen-Key low-passes at the corners the components give, with every Q
    /// damped by emitter_follower_damped_q and every corner prewarped by prewarped_corner_hz.
    /// The feedback path runs through this filter, as it does on the board.
    class Dd3AntiAliasFilter
    {
    public:
        /// @brief Places all three sections at this rate. Control thread only.
        /// @param sample_rate Sample rate in Hz.
        /// @pre sample_rate > 2 * kPassbandEdgeHz.
        void prepare(double sample_rate);

        /// @brief Clears every section's state.
        void reset();

        /// @brief Filters one sample through the cascade. Real-time safe.
        /// @param input The input sample.
        /// @return The band-limited sample.
        float process_sample(float input);

        static constexpr std::size_t kSectionCount = 3;

    private:
        SushiDSP::Biquad sections_[kSectionCount];
    };

    /// @brief The 4th-order reconstruction filter after the converter: Q10 and Q9.
    ///
    /// Two sections rather than three, and section 2 sits at 15.8 kHz, above Nyquist at the
    /// 20 kHz clock. The asymmetry against Dd3AntiAliasFilter is on the board and is kept.
    class Dd3ReconstructionFilter
    {
    public:
        /// @brief Places both sections at this rate. Control thread only.
        /// @param sample_rate Sample rate in Hz.
        /// @pre sample_rate > 2 * kPassbandEdgeHz.
        void prepare(double sample_rate);

        /// @brief Clears every section's state.
        void reset();

        /// @brief Filters one sample through the cascade. Real-time safe.
        /// @param input The input sample.
        /// @return The reconstructed sample.
        float process_sample(float input);

        static constexpr std::size_t kSectionCount = 2;

    private:
        SushiDSP::Biquad sections_[kSectionCount];
    };
}
