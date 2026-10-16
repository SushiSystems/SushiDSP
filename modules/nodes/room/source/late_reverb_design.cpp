/**************************************************************************/
/* late_reverb_design.cpp                                                 */
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
 * @file late_reverb_design.cpp
 * @brief The prime line lengths, the per-line decay fits, the band energies handed to the tail
 * and the coherence pair.
 * @author Mustafa Garip & Sushi Systems
 */

#include <SushiDSP/nodes/room/late_reverb_design.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <SushiDSP/nodes/room/band_response.hpp>
#include <SushiDSP/nodes/room/band_response_fit.hpp>
#include <SushiDSP/nodes/room/diffuse_field.hpp>
#include <SushiDSP/nodes/room/eyring_decay.hpp>
#include <SushiDSP/nodes/room/microphone_model.hpp>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Counts the lines.
        constexpr std::size_t K_LINES = FdnDesign::K_LINE_COUNT;

        /// @brief Holds pi.
        constexpr double K_PI = 3.14159265358979323846;

        /// @brief Scales the mid-band RT60 to the sum of the lines: 3 / 2.2. Spec §6.2.
        constexpr double K_OVERLAP_RATIO = 1.4;

        /// @brief Names the band whose decay time sets the mean line: 500 Hz.
        constexpr std::size_t K_MID_BAND = 2;

        /// @brief Bounds a line from below, in frames; the smallest prime.
        constexpr std::uint32_t K_SHORTEST_LINE = 2;

        /// @brief Bounds the ratio of the longest line to the shortest from below.
        constexpr double K_MIN_SPREAD = 2.0;

        /// @brief Holds the inter-ear spacing the diffuse-field coherence is evaluated at, in
        /// metres. Spec §6.4.
        constexpr double K_EAR_SPACING_METRES = 0.17;

        /// @brief Holds the six band energies of one signal.
        using BandEnergy = std::array<double, K_BAND_COUNT>;

        /// @brief Bounds a band's share of the tail from below at -30 dB of the largest band's,
        /// the range the two-section fit follows; a band the taps already fill gets that much.
        constexpr double K_SHARE_FLOOR = 1e-3;

        /// @brief Holds the section that passes its input unchanged.
        constexpr FdnDesign::Section K_UNITY_SECTION = {1.0, 0.0, 0.0, 0.0, 0.0};

        /// @brief Holds the cascade that passes its input unchanged.
        constexpr FdnDesign::Cascade K_UNITY_CASCADE = {K_UNITY_SECTION, K_UNITY_SECTION};

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
            std::uint32_t candidate = std::max(n, K_SHORTEST_LINE);
            while (!is_prime(candidate))
            {
                ++candidate;
            }
            return candidate;
        }

        /// @brief Returns 10^(-3 tau / RT60(f)) per band, the gain a line of tau seconds keeps
        /// so that RT60(f) seconds of recirculation lose 60 dB; a band with no decay time is
        /// silenced.
        BandResponse line_decay(double tau_seconds, const std::array<double, K_BAND_COUNT>& rt60)
        {
            BandResponse gains{};
            for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
            {
                gains.gain[band] =
                    rt60[band] > 0.0
                        ? static_cast<float>(std::pow(10.0, -3.0 * tau_seconds / rt60[band]))
                        : 0.0f;
            }
            return gains;
        }

        /// @brief Returns the energy the taps past the direct sound carry per band: each tap's
        /// gain squared times its fitted filter's power at the band centre, summed.
        /// @pre The set's first direct_count taps are the direct sound.
        BandEnergy reflected_band_energy(const EarlyTapSet& early, double sample_rate)
        {
            BandEnergy energy{};
            const std::uint32_t count =
                std::min(early.count, static_cast<std::uint32_t>(EarlyTapSet::K_MAX_EARLY_TAPS));
            for (std::uint32_t index = early.direct_count; index < count; ++index)
            {
                const EarlyTap& tap = early.taps[index];
                const BandResponseFit::Coefficients coefficients{tap.section_a, tap.section_b};
                const double gain = static_cast<double>(tap.gain);
                for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
                {
                    const double db = BandResponseFit::magnitude_db(
                        coefficients, K_BAND_CENTRES_HZ[band], sample_rate);
                    energy[band] += gain * gain * std::pow(10.0, db / 10.0);
                }
            }
            return energy;
        }

        /// @brief Returns the energy the network delivers per band for a unit input gain:
        /// sixteen lines each keeping g_i(f)^2 = 10^(-6 tau_i / RT60(f)) of a pass, the mixing
        /// handing every line's output to all sixteen alike, deliver 16 G / (1 - G) with G the
        /// mean of the g_i^2; a band with no decay time delivers nothing. Module README, "The
        /// wave layer and the seam".
        BandEnergy network_band_energy(const std::array<std::uint32_t, K_LINES>& frames,
                                       double sample_rate,
                                       const std::array<double, K_BAND_COUNT>& rt60)
        {
            BandEnergy energy{};
            for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
            {
                if (!(rt60[band] > 0.0))
                {
                    continue;
                }
                double kept = 0.0;
                for (std::size_t line = 0; line < K_LINES; ++line)
                {
                    const double tau = static_cast<double>(frames[line]) / sample_rate;
                    kept += std::pow(10.0, -6.0 * tau / rt60[band]) / static_cast<double>(K_LINES);
                }
                energy[band] = static_cast<double>(K_LINES) * kept / (1.0 - kept);
            }
            return energy;
        }

        /// @brief Returns the share of a diffuse field's power the receiver picks up: 1 for the
        /// ears, which read pressure, and a^2 + (1 - a)^2 / 3 for a microphone whose pattern
        /// is a + (1 - a) cos(theta), the pattern's power averaged over every direction.
        double receiver_efficiency(const RoomScene& scene, ListenerKind listener) noexcept
        {
            if (listener != ListenerKind::Microphone)
            {
                return 1.0;
            }
            const double a = pattern_pressure_share(scene.microphone_pattern);
            return a * a + (1.0 - a) * (1.0 - a) / 3.0;
        }

        /// @brief Returns per band what the receiver's share of the diffuse field holds beyond
        /// the reflections, never under zero.
        BandEnergy remainder_of(const BandEnergy& field, double efficiency,
                                const BandEnergy& reflected) noexcept
        {
            BandEnergy remainder{};
            for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
            {
                remainder[band] = std::max(field[band] * efficiency - reflected[band], 0.0);
            }
            return remainder;
        }

        /// @brief Holds the input gain and the output cascade's band targets that put one
        /// remainder into the network.
        struct TailLevel
        {
                /// @brief Stores the linear gain into every line.
                float input_gain;

                /// @brief Stores the output cascade's band gains, unity in every band when the
                /// remainder is zero throughout.
                BandResponse output;
        };

        /// @brief Returns the input gain whose square is the mean over bands of remainder over
        /// network energy, each band's share floored at K_SHARE_FLOOR of the largest, and the
        /// output gains sqrt of each band's share of that mean, so each band's delivered energy
        /// is its remainder.
        TailLevel tail_level(const BandEnergy& remainder, const BandEnergy& network) noexcept
        {
            BandEnergy share{};
            double largest = 0.0;
            for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
            {
                share[band] = network[band] > 0.0 ? remainder[band] / network[band] : 0.0;
                largest = std::max(largest, share[band]);
            }
            double mean = 0.0;
            for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
            {
                share[band] = std::max(share[band], K_SHARE_FLOOR * largest);
                mean += share[band] / static_cast<double>(K_BAND_COUNT);
            }
            TailLevel level{};
            level.input_gain = static_cast<float>(std::sqrt(mean));
            for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
            {
                level.output.gain[band] =
                    mean > 0.0 ? static_cast<float>(std::sqrt(share[band] / mean)) : 1.0f;
            }
            return level;
        }

        /// @brief Returns sin(k d) / (k d), the diffuse-field interaural coherence at one
        /// frequency. Spec §6.4.
        double coherence_at(double frequency_hz) noexcept
        {
            const double kd = 2.0 * K_PI * frequency_hz * K_EAR_SPACING_METRES / K_SPEED_OF_SOUND;
            return std::sin(kd) / kd;
        }

        /// @brief Returns the frequency where the coherence first falls to 1 / sqrt(2), found
        /// by bisection on the first lobe of the sinc.
        double coherence_corner_hz() noexcept
        {
            const double target = 1.0 / std::sqrt(2.0);
            double low = 1.0;
            double high = K_SPEED_OF_SOUND / (2.0 * K_EAR_SPACING_METRES);
            for (int step = 0; step < 60; ++step)
            {
                const double mid = 0.5 * (low + high);
                (coherence_at(mid) > target ? low : high) = mid;
            }
            return 0.5 * (low + high);
        }

        /// @brief Returns the sum of sixteen lines spread geometrically over [1, spread], in
        /// units of the shortest line.
        double spread_sum(double spread) noexcept
        {
            double sum = 0.0;
            for (std::size_t line = 0; line < K_LINES; ++line)
            {
                sum +=
                    std::pow(spread, static_cast<double>(line) / static_cast<double>(K_LINES - 1));
            }
            return sum;
        }

        /// @brief Returns the spread whose sixteen lines sum to ratio times the shortest,
        /// bisected over [low, high].
        double spread_for(double ratio, double low, double high) noexcept
        {
            for (int step = 0; step < 60; ++step)
            {
                const double middle = 0.5 * (low + high);
                (spread_sum(middle) < ratio ? low : high) = middle;
            }
            return 0.5 * (low + high);
        }

        /// @brief Returns the RBJ second-order Butterworth section at one corner, a low-pass
        /// when low is true and its power-complementary high-pass otherwise.
        FdnDesign::Section butterworth(double corner_hz, double sample_rate, bool low) noexcept
        {
            const double omega = 2.0 * K_PI * corner_hz / sample_rate;
            const double cos_omega = std::cos(omega);
            const double alpha = std::sin(omega) / std::sqrt(2.0);
            const double a0 = 1.0 + alpha;
            const double b0 = low ? 0.5 * (1.0 - cos_omega) : 0.5 * (1.0 + cos_omega);
            const double b1 = low ? 1.0 - cos_omega : -(1.0 + cos_omega);
            return {b0 / a0, b1 / a0, b0 / a0, -2.0 * cos_omega / a0, (1.0 - alpha) / a0};
        }

    } // namespace

    std::array<std::uint32_t, K_LINES>
    LateReverbDesign::line_frames(double shortest_seconds, double spread, double sample_rate)
    {
        const double shortest_frames = shortest_seconds * sample_rate;
        std::array<std::uint32_t, K_LINES> frames{};
        std::uint32_t previous = 0u;
        for (std::size_t line = 0; line < K_LINES; ++line)
        {
            const double step = static_cast<double>(line) / static_cast<double>(K_LINES - 1);
            const double target = shortest_frames * std::pow(spread, step);
            const auto rounded = static_cast<std::uint32_t>(std::max(std::round(target), 0.0));
            // Every line is strictly longer than the one before it, so no two ever coincide.
            frames[line] = next_prime(std::max(rounded, previous + 1u));
            previous = frames[line];
        }
        return frames;
    }

    LateReverbDesign::Network LateReverbDesign::network(const RoomScene& scene,
                                                        float crossover_time_s,
                                                        double sample_rate)
    {
        const std::array<double, K_BAND_COUNT> rt60 = EyringDecay::rt60_seconds(scene);
        const double rt60_mid = rt60[K_MID_BAND];
        const double total_frames = K_OVERLAP_RATIO * rt60_mid * sample_rate;
        const double handover_frames =
            std::round(static_cast<double>(crossover_time_s) * sample_rate);

        // The handover time bounds the shortest line; see modules/nodes/room/source/README.md.
        double shortest_frames = std::max(handover_frames, 1.0);
        double spread = K_MIN_SPREAD;
        const double wanted = total_frames / shortest_frames;
        if (wanted <= spread_sum(K_MIN_SPREAD))
        {
            // The delay budget caps the shortest line; see modules/nodes/room/source/README.md.
            shortest_frames = total_frames / spread_sum(K_MIN_SPREAD);
        }
        else
        {
            const double capacity_frames = FdnDesign::K_MAX_LINE_SECONDS * sample_rate;
            const double widest = std::max(capacity_frames / shortest_frames, K_MIN_SPREAD);
            spread = spread_for(wanted, K_MIN_SPREAD, widest);
        }

        Network result{};
        FdnDesign& design = result.design;
        design.line_frames = line_frames(shortest_frames / sample_rate, spread, sample_rate);
        for (std::size_t line = 0; line < K_LINES; ++line)
        {
            const double tau = static_cast<double>(design.line_frames[line]) / sample_rate;
            const BandResponseFit::Result fit =
                BandResponseFit::fit(line_decay(tau, rt60), sample_rate);
            design.line_section_a[line] = fit.coefficients.section_a;
            design.line_section_b[line] = fit.coefficients.section_b;
        }
        result.energy = network_band_energy(design.line_frames, sample_rate, rt60);
        result.handover_frames = handover_frames;
        return result;
    }

    FdnDesign LateReverbDesign::design(const RoomScene& scene, const EarlyTapSet& early,
                                       double sample_rate, ListenerKind listener)
    {
        return design(network(scene, early.crossover_time_s, sample_rate), scene, early,
                      sample_rate, listener);
    }

    FdnDesign LateReverbDesign::design(const Network& network, const RoomScene& scene,
                                       const EarlyTapSet& early, double sample_rate,
                                       ListenerKind listener)
    {
        FdnDesign design = network.design;
        const double handover_frames = network.handover_frames;

        const DiffuseField field(scene);
        const BandEnergy remainder =
            remainder_of(field.band_power(), receiver_efficiency(scene, listener),
                         reflected_band_energy(early, sample_rate));
        const TailLevel level = tail_level(remainder, network.energy);
        const BandResponseFit::Coefficients output =
            BandResponseFit::fit(level.output, sample_rate).coefficients;
        design.tone_correction = {output.section_a, output.section_b};
        design.input_gain = level.input_gain;

        if (listener == ListenerKind::Ears)
        {
            // A Butterworth pair is power complementary; see modules/nodes/room/source/README.md.
            const double corner_hz = coherence_corner_hz();
            design.coherence_a = {butterworth(corner_hz, sample_rate, true), K_UNITY_SECTION};
            design.coherence_b = {butterworth(corner_hz, sample_rate, false), K_UNITY_SECTION};
        }
        else
        {
            design.coherence_a = K_UNITY_CASCADE;
            design.coherence_b = K_UNITY_CASCADE;
        }

        // The shortest line is the network's own latency; see modules/nodes/room/source/README.md.
        design.delay_to_crossover_frames = static_cast<float>(
            std::max(handover_frames - static_cast<double>(design.line_frames[0]), 0.0));
        return design;
    }
} // namespace SushiDSP::room
