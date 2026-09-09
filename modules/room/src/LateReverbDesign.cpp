/**************************************************************************/
/* LateReverbDesign.cpp                                                   */
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
 * @file LateReverbDesign.cpp
 * @brief The prime line lengths, the per-line decay fits, the energy handover and the
 * coherence pair.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/LateReverbDesign.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "SushiDSP/room/BandResponse.hpp"
#include "SushiDSP/room/BandResponseFit.hpp"
#include "SushiDSP/room/EyringDecay.hpp"

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Counts the lines.
        constexpr std::size_t kLines = FdnDesign::kLineCount;

        /// @brief Holds pi.
        constexpr double kPi = 3.14159265358979323846;

        /// @brief Scales the mid-band RT60 to the sum of the lines: 3 / 2.2. Spec §6.2.
        constexpr double kOverlapRatio = 1.4;

        /// @brief Names the band whose decay time sets the mean line: 500 Hz.
        constexpr std::size_t kMidBand = 2;

        /// @brief Bounds a line from below, in frames; the smallest prime.
        constexpr std::uint32_t kShortestLine = 2;

        /// @brief Holds the window before t_c whose taps set the handover level, in seconds.
        constexpr double kHandoverWindowSeconds = 0.005;

        /// @brief Holds the inter-ear spacing the diffuse-field coherence is evaluated at, in
        /// metres. Spec §6.4.
        constexpr double kEarSpacingMetres = 0.17;

        /// @brief Holds 6 ln 10, the rate that takes a decay 60 dB down in one RT60.
        const double kDecayRatePerRt60 = 6.0 * std::log(10.0);

        /// @brief Holds the section that passes its input unchanged.
        constexpr FdnDesign::Section kUnitySection = { 1.0, 0.0, 0.0, 0.0, 0.0 };

        /// @brief Holds the cascade that passes its input unchanged.
        constexpr FdnDesign::Cascade kUnityCascade = { kUnitySection, kUnitySection };

        /// @brief Returns whether n is prime by trial division up to its square root.
        bool is_prime(std::uint32_t n) noexcept
        {
            if (n < 2u)
            {
                return false;
            }
            for (std::uint32_t divisor = 2u; divisor * divisor <= n; ++divisor)
            {
                if (n % divisor == 0u)
                {
                    return false;
                }
            }
            return true;
        }

        /// @brief Returns the smallest prime not below n.
        std::uint32_t next_prime(std::uint32_t n) noexcept
        {
            std::uint32_t candidate = std::max(n, kShortestLine);
            while (!is_prime(candidate))
            {
                ++candidate;
            }
            return candidate;
        }

        /// @brief Returns 10^(-3 tau / RT60(f)) per band, the gain a line of tau seconds keeps
        /// so that RT60(f) seconds of recirculation lose 60 dB; a band with no decay time is
        /// silenced.
        BandResponse line_decay(double tau_seconds, const std::array<double, kBandCount>& rt60)
        {
            BandResponse gains{};
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                gains.gain[band] = rt60[band] > 0.0
                    ? static_cast<float>(std::pow(10.0, -3.0 * tau_seconds / rt60[band]))
                    : 0.0f;
            }
            return gains;
        }

        /// @brief Returns Jot's tone correction sqrt(1 - g(f)^2) for the mean line, scaled to
        /// unity at 500 Hz so it tilts the tail without changing its mid-band level.
        BandResponse tone_correction(double mean_tau_seconds,
                                     const std::array<double, kBandCount>& rt60)
        {
            const BandResponse gain = line_decay(mean_tau_seconds, rt60);
            std::array<double, kBandCount> envelope{};
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                const double g = static_cast<double>(gain.gain[band]);
                envelope[band] = std::sqrt(std::max(1.0 - g * g, 1e-12));
            }
            BandResponse correction{};
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                correction.gain[band] = static_cast<float>(envelope[band] / envelope[kMidBand]);
            }
            return correction;
        }

        /// @brief Returns sin(k d) / (k d), the diffuse-field interaural coherence at one
        /// frequency. Spec §6.4.
        double coherence_at(double frequency_hz) noexcept
        {
            const double kd = 2.0 * kPi * frequency_hz * kEarSpacingMetres / kSpeedOfSound;
            return std::sin(kd) / kd;
        }

        /// @brief Returns the frequency where the coherence first falls to 1 / sqrt(2), found
        /// by bisection on the first lobe of the sinc.
        double coherence_corner_hz() noexcept
        {
            const double target = 1.0 / std::sqrt(2.0);
            double low = 1.0;
            double high = kSpeedOfSound / (2.0 * kEarSpacingMetres);
            for (int step = 0; step < 60; ++step)
            {
                const double mid = 0.5 * (low + high);
                (coherence_at(mid) > target ? low : high) = mid;
            }
            return 0.5 * (low + high);
        }

        /// @brief Returns the RBJ second-order Butterworth section at one corner, a low-pass
        /// when low is true and its power-complementary high-pass otherwise.
        FdnDesign::Section butterworth(double corner_hz, double sample_rate, bool low) noexcept
        {
            const double omega = 2.0 * kPi * corner_hz / sample_rate;
            const double cos_omega = std::cos(omega);
            const double alpha = std::sin(omega) / std::sqrt(2.0);
            const double a0 = 1.0 + alpha;
            const double b0 = low ? 0.5 * (1.0 - cos_omega) : 0.5 * (1.0 + cos_omega);
            const double b1 = low ? 1.0 - cos_omega : -(1.0 + cos_omega);
            return { b0 / a0, b1 / a0, b0 / a0, -2.0 * cos_omega / a0, (1.0 - alpha) / a0 };
        }

        /// @brief Returns one tap's energy: its gain squared times its filter's mean power over
        /// the six centres.
        double tap_energy(const EarlyTap& tap, double sample_rate)
        {
            const BandResponseFit::Coefficients coefficients{ tap.section_a, tap.section_b };
            double power_sum = 0.0;
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                const double db =
                    BandResponseFit::magnitude_db(coefficients, kBandCentresHz[band], sample_rate);
                power_sum += std::pow(10.0, db / 10.0);
            }
            const double gain = static_cast<double>(tap.gain);
            return gain * gain * power_sum / static_cast<double>(kBandCount);
        }

        /// @brief Returns the early set's energy per second at t_c: the taps of the last 5 ms
        /// decayed from the window's centre to t_c, plus the scattered energy spread as a decay
        /// that starts at t_c.
        double target_energy_rate(const EarlyTapSet& early, double sample_rate, double rt60_mid)
        {
            const double crossover_s = static_cast<double>(early.crossover_time_s);
            const double window_start_frames = (crossover_s - kHandoverWindowSeconds) * sample_rate;
            double window_energy = 0.0;
            const std::uint32_t count =
                std::min(early.count, static_cast<std::uint32_t>(EarlyTapSet::kMaxEarlyTaps));
            for (std::uint32_t index = 0; index < count; ++index)
            {
                const EarlyTap& tap = early.taps[index];
                if (static_cast<double>(tap.delay_frames) >= window_start_frames)
                {
                    window_energy += tap_energy(tap, sample_rate);
                }
            }
            const double decay_to_crossover =
                std::exp(-kDecayRatePerRt60 * (0.5 * kHandoverWindowSeconds) / rt60_mid);
            const double early_rate = window_energy / kHandoverWindowSeconds * decay_to_crossover;
            const double scattered_rate =
                static_cast<double>(early.scattered_energy) * kDecayRatePerRt60 / rt60_mid;
            return early_rate + scattered_rate;
        }

        /// @brief Returns the gain that makes sixteen lines of mean length tau, each holding
        /// gain^2 of energy, deliver energy_rate per second: sqrt(energy_rate tau / 16).
        float input_gain_for(double energy_rate, double mean_tau_seconds)
        {
            return static_cast<float>(
                std::sqrt(energy_rate * mean_tau_seconds / static_cast<double>(kLines)));
        }
    }

    std::array<std::uint32_t, kLines> LateReverbDesign::line_frames(double mean_seconds,
                                                                    double sample_rate)
    {
        const double mean_frames = mean_seconds * sample_rate;
        std::array<std::uint32_t, kLines> frames{};
        std::uint32_t previous = 0u;
        for (std::size_t line = 0; line < kLines; ++line)
        {
            const double octave = static_cast<double>(line) / static_cast<double>(kLines - 1) - 0.5;
            const double target = mean_frames * std::pow(2.0, octave);
            const auto rounded = static_cast<std::uint32_t>(std::max(std::round(target), 0.0));
            // Every line is strictly longer than the one before it, so no two ever coincide.
            frames[line] = next_prime(std::max(rounded, previous + 1u));
            previous = frames[line];
        }
        return frames;
    }

    FdnDesign LateReverbDesign::design(const RoomScene& scene, const EarlyTapSet& early,
                                       double sample_rate, ListenerKind listener)
    {
        const std::array<double, kBandCount> rt60 = EyringDecay::rt60_seconds(scene);
        const double rt60_mid = rt60[kMidBand];
        const double mean_seconds = kOverlapRatio * rt60_mid / static_cast<double>(kLines);

        FdnDesign design{};
        design.line_frames = line_frames(mean_seconds, sample_rate);
        double mean_tau = 0.0;
        for (std::size_t line = 0; line < kLines; ++line)
        {
            const double tau = static_cast<double>(design.line_frames[line]) / sample_rate;
            mean_tau += tau / static_cast<double>(kLines);
            const BandResponseFit::Result fit =
                BandResponseFit::fit(line_decay(tau, rt60), sample_rate);
            design.line_section_a[line] = fit.coefficients.section_a;
            design.line_section_b[line] = fit.coefficients.section_b;
        }

        const BandResponseFit::Coefficients tone =
            BandResponseFit::fit(tone_correction(mean_tau, rt60), sample_rate).coefficients;
        design.tone_correction = { tone.section_a, tone.section_b };

        if (listener == ListenerKind::Ears)
        {
            // A Butterworth pair is power complementary, so the right ear carries the left
            // ear's energy whatever the split between the coherent and incoherent sums.
            const double corner_hz = coherence_corner_hz();
            design.coherence_a = { butterworth(corner_hz, sample_rate, true), kUnitySection };
            design.coherence_b = { butterworth(corner_hz, sample_rate, false), kUnitySection };
        }
        else
        {
            design.coherence_a = kUnityCascade;
            design.coherence_b = kUnityCascade;
        }

        design.delay_to_crossover_frames =
            static_cast<float>(std::round(static_cast<double>(early.crossover_time_s) * sample_rate));
        design.input_gain = rt60_mid > 0.0
            ? input_gain_for(target_energy_rate(early, sample_rate, rt60_mid), mean_tau)
            : 0.0f;
        return design;
    }
}
