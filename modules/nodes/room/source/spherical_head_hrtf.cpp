/**************************************************************************/
/* spherical_head_hrtf.cpp                                                */
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
 * @file spherical_head_hrtf.cpp
 * @brief The Duda-Martens series per direction, folded to minimum phase through the cepstrum.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/spherical_head_hrtf.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "SushiDSP/math/fft.hpp"
#include "SushiDSP/room/ambisonic_encoder.hpp"

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds pi.
        constexpr double K_PI = 3.14159265358979323846;

        /// @brief Holds the grid step in radians, 10 degrees.
        constexpr double K_GRID_STEP = K_PI / 18.0;

        /// @brief Counts the bins of the one-sided magnitude grid, 0 to Nyquist inclusive.
        constexpr std::uint32_t K_BIN_COUNT = 513;

        /// @brief Holds the transform size the cepstrum is folded on.
        constexpr std::uint32_t K_FFT_SIZE = 2 * (K_BIN_COUNT - 1);

        /// @brief Counts the series terms taken past the normalised frequency.
        constexpr std::uint32_t K_EXTRA_TERMS = 40;

        /// @brief Holds one complex value in double precision.
        using Complex = std::complex<double>;

        /// @brief Holds one ear's magnitude on the one-sided grid.
        using Magnitudes = std::array<double, K_BIN_COUNT>;

        /// @brief Holds one ear's diffuse-field filter.
        using DiffuseTaps = std::array<float, HrtfSet::K_DIFFUSE_TAPS>;

        static_assert(HrtfSet::K_DIFFUSE_TAPS == HrtfSet::K_TAP_COUNT,
                      "the folder writes K_TAP_COUNT taps and the diffuse filter must hold them");

        /// @brief Returns the far-field rigid-sphere pressure ratio of Duda and Martens at one
        /// normalised frequency mu = k a and one cosine of the angle from the ear.
        Complex sphere_response(double mu, double cos_theta) noexcept
        {
            if (mu <= 0.0)
            {
                return Complex(1.0, 0.0);
            }
            const Complex phase = std::exp(Complex(0.0, mu));
            Complex hankel_previous = phase * Complex(0.0, -1.0 / mu);
            Complex hankel = hankel_previous * Complex(1.0 / mu, -1.0);
            double legendre_previous = 1.0;
            double legendre = cos_theta;
            Complex rotation(0.0, 1.0);
            Complex sum = rotation / (-hankel);
            // Term m is (-i)^(m-1) (2m+1) P_m(cos theta) / h_m'(mu); m = 0 is the line above.
            const auto term_count = static_cast<std::uint32_t>(std::ceil(mu)) + K_EXTRA_TERMS;
            for (std::uint32_t m = 1; m < term_count; ++m)
            {
                const double order = static_cast<double>(m);
                rotation *= Complex(0.0, -1.0);
                const Complex derivative = hankel_previous - (order + 1.0) / mu * hankel;
                const Complex term = rotation * (2.0 * order + 1.0) * legendre / derivative;
                if (!std::isfinite(term.real()) || !std::isfinite(term.imag()))
                {
                    break;
                }
                sum += term;
                const Complex hankel_next = (2.0 * order + 1.0) / mu * hankel - hankel_previous;
                hankel_previous = hankel;
                hankel = hankel_next;
                const double legendre_next =
                    ((2.0 * order + 1.0) * cos_theta * legendre - order * legendre_previous) /
                    (order + 1.0);
                legendre_previous = legendre;
                legendre = legendre_next;
            }
            return sum / (mu * mu);
        }

        /// @brief Returns the signed lateral angle of a direction: its angle from the median
        /// plane, positive to the left.
        double lateral_angle(double azimuth_rad, double elevation_rad) noexcept
        {
            const double sine =
                std::clamp(std::cos(elevation_rad) * std::sin(azimuth_rad), -1.0, 1.0);
            return std::asin(sine);
        }

        /// @brief Fills one ear's magnitude grid from the cosine of its angle to the source.
        void ear_magnitudes(double cos_theta, double bin_hz, Magnitudes& magnitude) noexcept
        {
            const double mu_per_hz = 2.0 * K_PI * SphericalHeadHrtf::K_HEAD_RADIUS_METRES /
                                     SphericalHeadHrtf::K_SPEED_OF_SOUND;
            for (std::uint32_t bin = 0; bin < K_BIN_COUNT; ++bin)
            {
                magnitude[bin] = std::abs(sphere_response(bin * bin_hz * mu_per_hz, cos_theta));
            }
        }

        /// @brief Turns a one-sided magnitude grid into its minimum-phase response by cepstral
        /// folding and keeps the first K_TAP_COUNT taps.
        class MinimumPhaseFolder
        {
            public:
                /// @brief Sizes the transform and its scratch once.
                MinimumPhaseFolder()
                    : fft_(K_FFT_SIZE), cepstrum_(K_FFT_SIZE), real_(K_FFT_SIZE), imag_(K_FFT_SIZE)
                {
                }

                /// @brief Writes the minimum-phase taps of a magnitude grid into out.
                void fold(const Magnitudes& magnitude, float* out)
                {
                    for (std::uint32_t bin = 0; bin < K_FFT_SIZE; ++bin)
                    {
                        const std::uint32_t folded = bin < K_BIN_COUNT ? bin : K_FFT_SIZE - bin;
                        real_[bin] = static_cast<float>(std::log(magnitude[folded]));
                        imag_[bin] = 0.0f;
                    }
                    fft_.inverse(real_.data(), imag_.data(), cepstrum_.data());
                    // The real cepstrum of a minimum-phase response is its causal half, doubled.
                    for (std::uint32_t n = 1; n < K_BIN_COUNT - 1; ++n)
                    {
                        cepstrum_[n] *= 2.0f;
                        cepstrum_[K_FFT_SIZE - n] = 0.0f;
                    }
                    fft_.forward(cepstrum_.data(), real_.data(), imag_.data());
                    for (std::uint32_t bin = 0; bin < K_FFT_SIZE; ++bin)
                    {
                        const float gain = std::exp(real_[bin]);
                        const float angle = imag_[bin];
                        real_[bin] = gain * std::cos(angle);
                        imag_[bin] = gain * std::sin(angle);
                    }
                    fft_.inverse(real_.data(), imag_.data(), cepstrum_.data());
                    std::copy(cepstrum_.begin(), cepstrum_.begin() + HrtfSet::K_TAP_COUNT, out);
                }

            private:
                /// @brief Holds the transform plan.
                Fft fft_;

                /// @brief Holds the real cepstrum, then the time-domain result.
                std::vector<float> cepstrum_;

                /// @brief Holds the real half of the spectra.
                std::vector<float> real_;

                /// @brief Holds the imaginary half of the spectra.
                std::vector<float> imag_;
        };

        /// @brief Writes the azimuth and elevation of one grid index in double precision: 17
        /// rings of 36 azimuths from -170 to 180 degrees, then the lower and the upper pole.
        void grid_point(std::uint32_t index, double& azimuth_rad, double& elevation_rad) noexcept
        {
            constexpr std::uint32_t ring_total =
                SphericalHeadHrtf::K_AZIMUTH_STEPS * SphericalHeadHrtf::K_ELEVATION_RINGS;
            if (index >= ring_total)
            {
                azimuth_rad = 0.0;
                elevation_rad = index == ring_total ? -K_PI / 2.0 : K_PI / 2.0;
                return;
            }
            const std::uint32_t ring = index / SphericalHeadHrtf::K_AZIMUTH_STEPS;
            const std::uint32_t step = index % SphericalHeadHrtf::K_AZIMUTH_STEPS;
            azimuth_rad = (static_cast<double>(step) - 17.0) * K_GRID_STEP;
            elevation_rad = (static_cast<double>(ring) - 8.0) * K_GRID_STEP;
        }

        /// @brief Returns the solid angle one grid index stands for in steradians, so that an
        /// average weighted by it runs over the sphere and not over the grid's points, which
        /// crowd towards the poles: a ring point takes its ring's band divided by the ring's
        /// count, a pole takes the cap above the top ring's band.
        double solid_angle(std::uint32_t index) noexcept
        {
            constexpr std::uint32_t ring_total =
                SphericalHeadHrtf::K_AZIMUTH_STEPS * SphericalHeadHrtf::K_ELEVATION_RINGS;
            if (index >= ring_total)
            {
                return 2.0 * K_PI * (1.0 - std::sin(K_PI / 2.0 - K_GRID_STEP / 2.0));
            }
            const std::uint32_t ring = index / SphericalHeadHrtf::K_AZIMUTH_STEPS;
            const double elevation = (static_cast<double>(ring) - 8.0) * K_GRID_STEP;
            return 4.0 * K_PI * std::cos(elevation) * std::sin(K_GRID_STEP / 2.0) /
                   static_cast<double>(SphericalHeadHrtf::K_AZIMUTH_STEPS);
        }

        /// @brief Accumulates one ear's power over the sphere and renders the average as a
        /// minimum-phase filter. Spec §6.4.
        class DiffuseFieldAverage
        {
            public:
                /// @brief Starts from no power and no weight.
                DiffuseFieldAverage() noexcept : power_{}, weight_(0.0) {}

                /// @brief Adds one direction's magnitude grid, weighted by its solid angle.
                void add(const Magnitudes& magnitude, double solid_angle) noexcept
                {
                    for (std::uint32_t bin = 0; bin < K_BIN_COUNT; ++bin)
                    {
                        power_[bin] += solid_angle * magnitude[bin] * magnitude[bin];
                    }
                    weight_ += solid_angle;
                }

                /// @brief Writes the root of the mean power, folded to minimum phase, into out.
                /// @pre At least one direction was added.
                void render(MinimumPhaseFolder& folder, DiffuseTaps& out) const
                {
                    Magnitudes magnitude{};
                    for (std::uint32_t bin = 0; bin < K_BIN_COUNT; ++bin)
                    {
                        magnitude[bin] = std::sqrt(power_[bin] / weight_);
                    }
                    folder.fold(magnitude, out.data());
                }

            private:
                /// @brief Holds the solid-angle-weighted sum of squared magnitudes per bin.
                std::array<double, K_BIN_COUNT> power_;

                /// @brief Holds the sum of the solid angles added.
                double weight_;
        };

        /// @brief Returns Woodworth's delay in frames for a lateral angle.
        double woodworth_frames(double theta, double sample_rate) noexcept
        {
            return SphericalHeadHrtf::K_HEAD_RADIUS_METRES / SphericalHeadHrtf::K_SPEED_OF_SOUND *
                   (theta + std::sin(theta)) * sample_rate;
        }

        static_assert(HrtfSet::K_DECODER_TAPS + HrtfSet::K_TAP_COUNT <= K_FFT_SIZE,
                      "a delayed HRIR must fit the transform the decoder is summed on");

        /// @brief Returns the max-rE weight of one order at the encoder's order: the Legendre
        /// polynomial of that order at cos(137.9 degrees / (N + 1.51)) (Zotter and Frank,
        /// "All-round Ambisonic panning and decoding", JAES 60(10), 2012).
        double max_re_weight(std::uint32_t order) noexcept
        {
            const double x = std::cos(137.9 * K_PI / 180.0 /
                                      (static_cast<double>(AmbisonicEncoder::K_ORDER) + 1.51));
            double previous = 1.0;
            double current = x;
            if (order == 0)
            {
                return 1.0;
            }
            for (std::uint32_t n = 1; n < order; ++n)
            {
                const double degree = static_cast<double>(n);
                const double next =
                    ((2.0 * degree + 1.0) * x * current - degree * previous) / (degree + 1.0);
                previous = current;
                current = next;
            }
            return current;
        }

        /// @brief Returns the order of one ACN channel, floor(sqrt(channel)).
        std::uint32_t acn_order(std::uint32_t channel) noexcept
        {
            std::uint32_t order = 0;
            while ((order + 1) * (order + 1) <= channel)
            {
                ++order;
            }
            return order;
        }

        /// @brief Sums every grid direction's HRIR pair, delayed by its ITD and weighted by its
        /// solid angle, its encode gains and (2n + 1) times the max-rE weight of each order, into
        /// the 32 decoder filters: the least-squares decoder of a grid that samples the sphere
        /// by solid angle. Spec §9.4.
        class SamplingDecoder
        {
            public:
                /// @brief Counts the accumulated filters, left ear then right, ACN order.
                static constexpr std::uint32_t K_FILTERS = 2 * AmbisonicEncoder::K_CHANNELS;

                /// @brief Sizes the transform, its scratch and the 32 spectrum accumulators.
                SamplingDecoder()
                    : fft_(K_FFT_SIZE), time_(K_FFT_SIZE), real_(K_FFT_SIZE), imag_(K_FFT_SIZE),
                      accumulated_real_(static_cast<std::size_t>(K_FILTERS) * K_BIN_COUNT, 0.0),
                      accumulated_imag_(static_cast<std::size_t>(K_FILTERS) * K_BIN_COUNT, 0.0)
                {
                    for (std::uint32_t channel = 0; channel < AmbisonicEncoder::K_CHANNELS;
                         ++channel)
                    {
                        const std::uint32_t order = acn_order(channel);
                        order_weight_[channel] =
                            (2.0 * order + 1.0) * max_re_weight(order) / (4.0 * K_PI);
                    }
                }

                /// @brief Adds one direction's pair; itd_frames positive means the right ear later.
                void add(const float* left, const float* right, double itd_frames,
                         double solid_angle, double azimuth_rad, double elevation_rad)
                {
                    std::array<float, AmbisonicEncoder::K_CHANNELS> gains{};
                    AmbisonicEncoder::encode(azimuth_rad, elevation_rad, gains);
                    std::array<double, AmbisonicEncoder::K_CHANNELS> weights{};
                    for (std::uint32_t channel = 0; channel < AmbisonicEncoder::K_CHANNELS;
                         ++channel)
                    {
                        weights[channel] = solid_angle * order_weight_[channel] * gains[channel];
                    }
                    add_ear(0, left, std::max(-itd_frames, 0.0), weights);
                    add_ear(1, right, std::max(itd_frames, 0.0), weights);
                }

                /// @brief Returns the 32 filters, K_DECODER_TAPS each, left ear then right, ACN order.
                std::vector<float> render()
                {
                    std::vector<float> filters(static_cast<std::size_t>(K_FILTERS) *
                                               HrtfSet::K_DECODER_TAPS);
                    for (std::uint32_t filter = 0; filter < K_FILTERS; ++filter)
                    {
                        const std::size_t base = static_cast<std::size_t>(filter) * K_BIN_COUNT;
                        for (std::uint32_t bin = 0; bin < K_FFT_SIZE; ++bin)
                        {
                            const std::uint32_t folded = bin < K_BIN_COUNT ? bin : K_FFT_SIZE - bin;
                            const double sign = bin < K_BIN_COUNT ? 1.0 : -1.0;
                            real_[bin] = static_cast<float>(accumulated_real_[base + folded]);
                            imag_[bin] =
                                static_cast<float>(sign * accumulated_imag_[base + folded]);
                        }
                        // A real filter has real end bins; the delay phase leaves a remainder there.
                        imag_[0] = 0.0f;
                        imag_[K_BIN_COUNT - 1] = 0.0f;
                        fft_.inverse(real_.data(), imag_.data(), time_.data());
                        std::copy(time_.begin(), time_.begin() + HrtfSet::K_DECODER_TAPS,
                                  filters.begin() + static_cast<std::ptrdiff_t>(filter) *
                                                        HrtfSet::K_DECODER_TAPS);
                    }
                    return filters;
                }

            private:
                /// @brief Transforms one ear's taps, delays them by delay_frames and adds them to
                /// that ear's sixteen accumulators at the channel weights.
                void add_ear(std::uint32_t ear, const float* taps, double delay_frames,
                             const std::array<double, AmbisonicEncoder::K_CHANNELS>& weights)
                {
                    std::fill(time_.begin(), time_.end(), 0.0f);
                    std::copy(taps, taps + HrtfSet::K_TAP_COUNT, time_.begin());
                    fft_.forward(time_.data(), real_.data(), imag_.data());
                    for (std::uint32_t bin = 0; bin < K_BIN_COUNT; ++bin)
                    {
                        const double phase = -2.0 * K_PI * bin * delay_frames / K_FFT_SIZE;
                        const double cosine = std::cos(phase);
                        const double sine = std::sin(phase);
                        const double delayed_real = real_[bin] * cosine - imag_[bin] * sine;
                        const double delayed_imag = real_[bin] * sine + imag_[bin] * cosine;
                        for (std::uint32_t channel = 0; channel < AmbisonicEncoder::K_CHANNELS;
                             ++channel)
                        {
                            const std::size_t index =
                                (static_cast<std::size_t>(ear) * AmbisonicEncoder::K_CHANNELS +
                                 channel) *
                                    K_BIN_COUNT +
                                bin;
                            accumulated_real_[index] += weights[channel] * delayed_real;
                            accumulated_imag_[index] += weights[channel] * delayed_imag;
                        }
                    }
                }

                /// @brief Holds the transform plan.
                Fft fft_;

                /// @brief Holds one response in the time domain.
                std::vector<float> time_;

                /// @brief Holds the real half of one spectrum.
                std::vector<float> real_;

                /// @brief Holds the imaginary half of one spectrum.
                std::vector<float> imag_;

                /// @brief Holds each filter's accumulated real spectrum, K_BIN_COUNT bins per filter.
                std::vector<double> accumulated_real_;

                /// @brief Holds each filter's accumulated imaginary spectrum.
                std::vector<double> accumulated_imag_;

                /// @brief Holds (2n + 1) times the max-rE weight over 4 pi for each channel's order.
                std::array<double, AmbisonicEncoder::K_CHANNELS> order_weight_{};
        };
    } // namespace

    double SphericalHeadHrtf::interaural_time_difference(double azimuth_rad) noexcept
    {
        const double theta = lateral_angle(azimuth_rad, 0.0);
        return K_HEAD_RADIUS_METRES / K_SPEED_OF_SOUND * (theta + std::sin(theta));
    }

    HrtfSet SphericalHeadHrtf::build(double sample_rate)
    {
        std::vector<HrtfSet::Direction> directions(K_DIRECTION_COUNT);
        const double bin_hz = sample_rate / static_cast<double>(K_FFT_SIZE);
        const std::size_t tap_total =
            static_cast<std::size_t>(K_DIRECTION_COUNT) * HrtfSet::K_TAP_COUNT;
        std::vector<float> left(tap_total);
        std::vector<float> right(tap_total);
        MinimumPhaseFolder folder;
        Magnitudes magnitude{};
        DiffuseFieldAverage diffuse_left;
        DiffuseFieldAverage diffuse_right;
        SamplingDecoder decoder;
        for (std::uint32_t index = 0; index < K_DIRECTION_COUNT; ++index)
        {
            double azimuth = 0.0;
            double elevation = 0.0;
            grid_point(index, azimuth, elevation);
            const double weight = solid_angle(index);
            const std::size_t offset = static_cast<std::size_t>(index) * HrtfSet::K_TAP_COUNT;
            // The left ear sits at +90 degrees azimuth and the right at -90, both level.
            const double cos_left = std::cos(elevation) * std::sin(azimuth);
            ear_magnitudes(cos_left, bin_hz, magnitude);
            folder.fold(magnitude, left.data() + offset);
            diffuse_left.add(magnitude, weight);
            ear_magnitudes(-cos_left, bin_hz, magnitude);
            folder.fold(magnitude, right.data() + offset);
            diffuse_right.add(magnitude, weight);
            const double itd_frames =
                woodworth_frames(lateral_angle(azimuth, elevation), sample_rate);
            decoder.add(left.data() + offset, right.data() + offset, itd_frames, weight, azimuth,
                        elevation);
            directions[index] = {static_cast<float>(azimuth), static_cast<float>(elevation),
                                 static_cast<float>(itd_frames)};
        }
        DiffuseTaps diffuse_left_taps{};
        DiffuseTaps diffuse_right_taps{};
        diffuse_left.render(folder, diffuse_left_taps);
        diffuse_right.render(folder, diffuse_right_taps);
        HrtfSet set(sample_rate, std::move(directions), std::move(left), std::move(right));
        set.set_diffuse_field(diffuse_left_taps, diffuse_right_taps);
        set.set_decoder(AmbisonicEncoder::K_ORDER, decoder.render());
        return set;
    }
} // namespace SushiDSP::room
