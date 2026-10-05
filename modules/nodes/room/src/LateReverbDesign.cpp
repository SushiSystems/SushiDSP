/****************************************************************/
/* LateReverbDesign.cpp                                         */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file LateReverbDesign.cpp
 * @brief The prime line lengths, the per-line decay fits, the band energies handed to the tail
 * and the coherence pair.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/LateReverbDesign.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "SushiDSP/room/BandResponse.hpp"
#include "SushiDSP/room/BandResponseFit.hpp"
#include "SushiDSP/room/DiffuseField.hpp"
#include "SushiDSP/room/EyringDecay.hpp"
#include "SushiDSP/room/MicrophoneModel.hpp"

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

        /// @brief Bounds the ratio of the longest line to the shortest from below.
        constexpr double kMinSpread = 2.0;

        /// @brief Holds the inter-ear spacing the diffuse-field coherence is evaluated at, in
        /// metres. Spec §6.4.
        constexpr double kEarSpacingMetres = 0.17;

        /// @brief Holds the six band energies of one signal.
        using BandEnergy = std::array<double, kBandCount>;

        /// @brief Bounds a band's share of the tail from below at -30 dB of the largest band's,
        /// the range the two-section fit follows; a band the taps already fill gets that much.
        constexpr double kShareFloor = 1e-3;

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

        /// @brief Returns the energy the taps past the direct sound carry per band: each tap's
        /// gain squared times its fitted filter's power at the band centre, summed.
        /// @pre The taps are sorted by arrival, so tap 0 is the direct sound whenever count > 0.
        BandEnergy reflected_band_energy(const EarlyTapSet& early, double sample_rate)
        {
            BandEnergy energy{};
            const std::uint32_t count =
                std::min(early.count, static_cast<std::uint32_t>(EarlyTapSet::kMaxEarlyTaps));
            for (std::uint32_t index = 1; index < count; ++index)
            {
                const EarlyTap& tap = early.taps[index];
                const BandResponseFit::Coefficients coefficients{ tap.section_a, tap.section_b };
                const double gain = static_cast<double>(tap.gain);
                for (std::size_t band = 0; band < kBandCount; ++band)
                {
                    const double db = BandResponseFit::magnitude_db(
                        coefficients, kBandCentresHz[band], sample_rate);
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
        BandEnergy network_band_energy(const std::array<std::uint32_t, kLines>& frames,
                                       double sample_rate,
                                       const std::array<double, kBandCount>& rt60)
        {
            BandEnergy energy{};
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                if (!(rt60[band] > 0.0))
                {
                    continue;
                }
                double kept = 0.0;
                for (std::size_t line = 0; line < kLines; ++line)
                {
                    const double tau = static_cast<double>(frames[line]) / sample_rate;
                    kept += std::pow(10.0, -6.0 * tau / rt60[band]) / static_cast<double>(kLines);
                }
                energy[band] = static_cast<double>(kLines) * kept / (1.0 - kept);
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
            for (std::size_t band = 0; band < kBandCount; ++band)
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
        /// network energy, each band's share floored at kShareFloor of the largest, and the
        /// output gains sqrt of each band's share of that mean, so each band's delivered energy
        /// is its remainder.
        TailLevel tail_level(const BandEnergy& remainder, const BandEnergy& network) noexcept
        {
            BandEnergy share{};
            double largest = 0.0;
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                share[band] = network[band] > 0.0 ? remainder[band] / network[band] : 0.0;
                largest = std::max(largest, share[band]);
            }
            double mean = 0.0;
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                share[band] = std::max(share[band], kShareFloor * largest);
                mean += share[band] / static_cast<double>(kBandCount);
            }
            TailLevel level{};
            level.input_gain = static_cast<float>(std::sqrt(mean));
            for (std::size_t band = 0; band < kBandCount; ++band)
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

        /// @brief Returns the sum of sixteen lines spread geometrically over [1, spread], in
        /// units of the shortest line.
        double spread_sum(double spread) noexcept
        {
            double sum = 0.0;
            for (std::size_t line = 0; line < kLines; ++line)
            {
                sum += std::pow(spread, static_cast<double>(line)
                                            / static_cast<double>(kLines - 1));
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
            const double omega = 2.0 * kPi * corner_hz / sample_rate;
            const double cos_omega = std::cos(omega);
            const double alpha = std::sin(omega) / std::sqrt(2.0);
            const double a0 = 1.0 + alpha;
            const double b0 = low ? 0.5 * (1.0 - cos_omega) : 0.5 * (1.0 + cos_omega);
            const double b1 = low ? 1.0 - cos_omega : -(1.0 + cos_omega);
            return { b0 / a0, b1 / a0, b0 / a0, -2.0 * cos_omega / a0, (1.0 - alpha) / a0 };
        }

    }

    std::array<std::uint32_t, kLines> LateReverbDesign::line_frames(double shortest_seconds,
                                                                    double spread,
                                                                    double sample_rate)
    {
        const double shortest_frames = shortest_seconds * sample_rate;
        std::array<std::uint32_t, kLines> frames{};
        std::uint32_t previous = 0u;
        for (std::size_t line = 0; line < kLines; ++line)
        {
            const double step = static_cast<double>(line) / static_cast<double>(kLines - 1);
            const double target = shortest_frames * std::pow(spread, step);
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
        const double total_frames = kOverlapRatio * rt60_mid * sample_rate;
        const double handover_frames =
            std::round(static_cast<double>(early.crossover_time_s) * sample_rate);

        // The network emits nothing until its shortest line has run, so that line is what the
        // handover time bounds; the sum of the sixteen stays kOverlapRatio times the mid-band
        // decay, which is what fixes the tail's modal density (spec §6.2).
        double shortest_frames = std::max(handover_frames, 1.0);
        double spread = kMinSpread;
        const double wanted = total_frames / shortest_frames;
        if (wanted <= spread_sum(kMinSpread))
        {
            // A room whose handover exceeds its whole delay budget cannot start there; the
            // budget sets the shortest line and the input delay carries what is left.
            shortest_frames = total_frames / spread_sum(kMinSpread);
        }
        else
        {
            const double capacity_frames = FdnDesign::kMaxLineSeconds * sample_rate;
            const double widest = std::max(capacity_frames / shortest_frames, kMinSpread);
            spread = spread_for(wanted, kMinSpread, widest);
        }

        FdnDesign design{};
        design.line_frames = line_frames(shortest_frames / sample_rate, spread, sample_rate);
        for (std::size_t line = 0; line < kLines; ++line)
        {
            const double tau = static_cast<double>(design.line_frames[line]) / sample_rate;
            const BandResponseFit::Result fit =
                BandResponseFit::fit(line_decay(tau, rt60), sample_rate);
            design.line_section_a[line] = fit.coefficients.section_a;
            design.line_section_b[line] = fit.coefficients.section_b;
        }

        const DiffuseField field(scene);
        const BandEnergy remainder =
            remainder_of(field.band_power(), receiver_efficiency(scene, listener),
                         reflected_band_energy(early, sample_rate));
        const TailLevel level =
            tail_level(remainder, network_band_energy(design.line_frames, sample_rate, rt60));
        const BandResponseFit::Coefficients output =
            BandResponseFit::fit(level.output, sample_rate).coefficients;
        design.tone_correction = { output.section_a, output.section_b };
        design.input_gain = level.input_gain;

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

        // The shortest line is the network's own latency, so the delay ahead of it is what is
        // left of the handover time.
        design.delay_to_crossover_frames = static_cast<float>(
            std::max(handover_frames - static_cast<double>(design.line_frames[0]), 0.0));
        return design;
    }
}
