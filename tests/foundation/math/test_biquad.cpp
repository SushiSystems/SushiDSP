/**************************************************************************/
/* test_biquad.cpp                                                        */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_biquad.cpp
 * @brief Tests the Biquad filter designs against their analog prototypes.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/math/biquad.hpp>

namespace
{
    const double kUf = TestSupport::unit_roundoff<float>();
    const double kUd = TestSupport::unit_roundoff<double>();

    /// Pi to double precision, spelled out so the tests do not depend on M_PI being visible.
    constexpr double K_PI = 3.14159265358979323846;

    /// The design a row configures.
    enum class Design
    {
        LowPass,
        HighPass,
        LowShelf,
        HighShelf,
        Peaking,
        OnePoleLowPass,
        FirstOrderHighPass,
        FirstOrderShelf
    };

    /// One probe: a design, and a drive at bin / length cycles per sample (whole cycles, so the
    /// single-bin projection has no leakage). Bin 0 drives a constant and reads the settled output.
    struct Row
    {
            const char* name;
            Design design;
            double sample_rate;
            /// Cutoff, centre or shelf corner; the zero of a first-order shelf.
            double corner_hz;
            /// The pole of a first-order shelf; unused elsewhere.
            double other_hz;
            /// Gain in decibels for the RBJ shelves and peaking; the low-frequency gain for a first-order shelf.
            double gain;
            double q;
            long long length;
            long long bin;
    };

    /// The analog prototype N(s) / D(s) in a normalised variable s = j * omega, with
    /// N = n2 s^2 + n1 s + n0 and D = d2 s^2 + d1 s + d0.
    struct Analog
    {
            double n0, n1, n2;
            double d0, d1, d2;
    };

    /// A configured filter with the oracle for its row.
    struct Case
    {
            SushiDSP::Biquad filter;
            /// The analog magnitude at the probe frequency.
            double reference = 0.0;
            /// The largest pole radius the design has, from mapping the analog poles.
            double pole_radius = 0.0;
            /// Relative error the design itself is allowed against the reference; zero for exact maps.
            double budget = 0.0;
    };

    /// @brief Evaluates |N(j w) / D(j w)|.
    double magnitude_at(const Analog& analog, double omega)
    {
        const std::complex<double> numerator(analog.n0 - analog.n2 * omega * omega, analog.n1 * omega);
        const std::complex<double> denominator(analog.d0 - analog.d2 * omega * omega, analog.d1 * omega);
        return std::abs(numerator) / std::abs(denominator);
    }

    /// @brief Maps the poles of D through z = (1 + t p) / (1 - t p) and returns the largest radius.
    /// @param tangent The prewarp tangent t, so the analog variable p is in units of 1 / t.
    double mapped_pole_radius(const Analog& analog, double tangent)
    {
        std::vector<std::complex<double>> poles;
        if (analog.d2 == 0.0)
        {
            poles.push_back(-analog.d0 / analog.d1);
        }
        else
        {
            const std::complex<double> root =
                std::sqrt(std::complex<double>(analog.d1 * analog.d1 - 4.0 * analog.d2 * analog.d0, 0.0));
            poles.push_back((-analog.d1 + root) / (2.0 * analog.d2));
            poles.push_back((-analog.d1 - root) / (2.0 * analog.d2));
        }
        double radius = 0.0;
        for (const std::complex<double>& pole : poles)
        {
            radius = std::max(radius, std::abs((1.0 + tangent * pole) / (1.0 - tangent * pole)));
        }
        return radius;
    }

    /// @brief Returns the RBJ prototype of a second-order design at its corner, where s = j omega
    /// and omega is the frequency over the prewarped corner.
    Analog rbj_prototype(Design design, double gain_db, double q)
    {
        const double a = std::pow(10.0, gain_db / 40.0);
        const double root_a = std::sqrt(a);
        switch (design)
        {
            case Design::LowPass:
                return {1.0, 0.0, 0.0, 1.0, 1.0 / q, 1.0};
            case Design::HighPass:
                return {0.0, 0.0, 1.0, 1.0, 1.0 / q, 1.0};
            case Design::LowShelf:
                return {a * a, a * root_a / q, a, 1.0, root_a / q, a};
            case Design::HighShelf:
                return {a, a * root_a / q, a * a, a, root_a / q, 1.0};
            default:
                return {1.0, a / q, 1.0, 1.0, 1.0 / (a * q), 1.0};
        }
    }

    /// @brief Builds the configured filter of a row and the oracle for its probe.
    Case build_case(const Row& row)
    {
        Case built;
        const double probe_hz = row.sample_rate * static_cast<double>(row.bin) / static_cast<double>(row.length);
        const double probe_tangent = std::tan(K_PI * probe_hz / row.sample_rate);
        const double corner_tangent = std::tan(K_PI * row.corner_hz / row.sample_rate);

        switch (row.design)
        {
            case Design::OnePoleLowPass:
            {
                // H = 1 / (1 + j T / t_c) with T = tan(pi f / fs): the bilinear map with the corner prewarped.
                built.filter.set_one_pole_low_pass(row.sample_rate, row.corner_hz);
                const Analog analog{1.0, 0.0, 0.0, 1.0, 1.0 / corner_tangent, 0.0};
                built.reference = magnitude_at(analog, probe_tangent);
                built.pole_radius = mapped_pole_radius(analog, 1.0);
                break;
            }
            case Design::FirstOrderHighPass:
            {
                // The analog RC; the impulse-invariant map is allowed omega_c^2 of relative error.
                built.filter.set_first_order_high_pass(row.sample_rate, row.corner_hz);
                const Analog analog{0.0, 1.0, 0.0, 1.0, 1.0, 0.0};
                built.reference = magnitude_at(analog, probe_hz / row.corner_hz);
                built.pole_radius = std::exp(-2.0 * K_PI * row.corner_hz / row.sample_rate);
                const double corner_omega = 2.0 * K_PI * row.corner_hz / row.sample_rate;
                built.budget = corner_omega * corner_omega;
                break;
            }
            case Design::FirstOrderShelf:
            {
                // H = g (1 + j T / t_z) / (1 + j T / t_p): both corners prewarped separately.
                built.filter.set_first_order_shelf(row.sample_rate, row.corner_hz, row.other_hz, row.gain);
                const double pole_tangent = std::tan(K_PI * row.other_hz / row.sample_rate);
                const Analog analog{row.gain, row.gain / corner_tangent, 0.0, 1.0, 1.0 / pole_tangent, 0.0};
                built.reference = magnitude_at(analog, probe_tangent);
                built.pole_radius = mapped_pole_radius(analog, 1.0);
                break;
            }
            default:
            {
                switch (row.design)
                {
                    case Design::LowPass:
                        built.filter.set_low_pass(row.sample_rate, row.corner_hz, row.q);
                        break;
                    case Design::HighPass:
                        built.filter.set_high_pass(row.sample_rate, row.corner_hz, row.q);
                        break;
                    case Design::LowShelf:
                        built.filter.set_low_shelf(row.sample_rate, row.corner_hz, row.gain, row.q);
                        break;
                    case Design::HighShelf:
                        built.filter.set_high_shelf(row.sample_rate, row.corner_hz, row.gain, row.q);
                        break;
                    default:
                        built.filter.set_peaking(row.sample_rate, row.corner_hz, row.gain, row.q);
                        break;
                }
                // The RBJ designs are the exact bilinear map of the prototype, omega = T / t_corner.
                const Analog analog = rbj_prototype(row.design, row.gain, row.q);
                built.reference = magnitude_at(analog, probe_tangent / corner_tangent);
                built.pole_radius = mapped_pole_radius(analog, corner_tangent);
                break;
            }
        }
        return built;
    }

    /// @brief Returns the samples after which a transient of unit size is below the float roundoff:
    /// twice ln(u) / ln(r), the factor covering the n * r^n of a repeated pole.
    long long settle_samples(double pole_radius)
    {
        return 2 * static_cast<long long>(std::ceil(std::log(kUf) / std::log(pole_radius)));
    }

    /// @brief Returns the l1 norm of the impulse response over the given length.
    double impulse_l1(SushiDSP::Biquad& filter, long long length)
    {
        filter.reset();
        double sum = std::abs(static_cast<double>(filter.process_sample(1.0f)));
        for (long long n = 1; n < length; ++n)
        {
            sum += std::abs(static_cast<double>(filter.process_sample(0.0f)));
        }
        return sum;
    }

    /// @brief Drives the filter with sin(2 pi bin n / length), skips the settle, and projects the
    /// output onto sine and cosine over whole cycles. Bin 0 drives a constant 1 and returns |y|.
    double measure_gain(SushiDSP::Biquad& filter, const Row& row, long long settle)
    {
        filter.reset();
        if (row.bin == 0)
        {
            float output = 0.0f;
            for (long long n = 0; n <= settle; ++n)
            {
                output = filter.process_sample(1.0f);
            }
            return std::abs(static_cast<double>(output));
        }

        double real_part = 0.0;
        double imaginary_part = 0.0;
        for (long long n = 0; n < settle + row.length; ++n)
        {
            const double phase = 2.0 * K_PI * static_cast<double>(row.bin) *
                                 static_cast<double>(n % row.length) / static_cast<double>(row.length);
            const float output = filter.process_sample(static_cast<float>(std::sin(phase)));
            if (n >= settle)
            {
                real_part += output * std::sin(phase);
                imaginary_part += output * std::cos(phase);
            }
        }
        return 2.0 * std::sqrt(real_part * real_part + imaginary_part * imaginary_part) /
               static_cast<double>(row.length);
    }

    /// @brief Returns the error bound of a gain measurement: the design budget on the reference plus
    /// the float rounding of the input and of the output, 4 u ||h||_1 (each is at most 2 u ||h||_1
    /// after the projection's 2 / N average), the settled transient being below u ||h||_1 by construction.
    double gain_bound(const Case& built, double l1_norm)
    {
        return built.budget * built.reference + 4.0 * kUf * l1_norm;
    }

    /// @brief Runs a row and returns the error ratio of its measured gain against its oracle.
    TestSupport::ErrorRatio gain_error(const Row& row)
    {
        Case built = build_case(row);
        const long long settle = settle_samples(built.pole_radius);
        const double l1_norm = impulse_l1(built.filter, settle);
        const double measured = measure_gain(built.filter, row, settle);
        return TestSupport::measure_error(measured, built.reference, gain_bound(built, l1_norm));
    }

    /// The sample rate of the 48 kHz rows, with a 4800 sample window so that bin k is k * 10 Hz.
    constexpr double K_RATE_48K = 48000.0;
    constexpr long long K_WINDOW_48K = 4800;

    const Row kMagnitudeRows[] = {
        {"LowPassPassesDcAtUnityGain", Design::LowPass, K_RATE_48K, 1000.0, 0.0, 0.0, 0.707, K_WINDOW_48K, 0},
        {"LowPassAttenuatesWellAboveCutoff", Design::LowPass, K_RATE_48K, 500.0, 0.0, 0.0, 0.707, K_WINDOW_48K, 1000},
        {"HighPassBlocksDcCompletely", Design::HighPass, K_RATE_48K, 1000.0, 0.0, 0.0, 0.707, K_WINDOW_48K, 0},
        {"HighPassPassesWellAboveCutoffAtUnityGain", Design::HighPass, K_RATE_48K, 500.0, 0.0, 0.0, 0.707,
         K_WINDOW_48K, 1000},
        {"LowShelfBoostsDcByItsGain", Design::LowShelf, K_RATE_48K, 200.0, 0.0, 6.0, 0.707, K_WINDOW_48K, 0},
        {"LowShelfLeavesHighFrequenciesNearUnity", Design::LowShelf, K_RATE_48K, 200.0, 0.0, 6.0, 0.707,
         K_WINDOW_48K, 1000},
        {"HighShelfBoostsHighFrequencyByItsGain", Design::HighShelf, K_RATE_48K, 2000.0, 0.0, 6.0, 0.707,
         K_WINDOW_48K, 1500},
        {"PeakingBoostsCenterFrequency", Design::Peaking, K_RATE_48K, 1000.0, 0.0, 9.0, 1.0, K_WINDOW_48K, 100},
        {"PeakingLeavesFarFrequenciesNearUnity", Design::Peaking, K_RATE_48K, 1000.0, 0.0, 9.0, 1.0, K_WINDOW_48K,
         5},
        {"OnePoleLowPassAt1kHz", Design::OnePoleLowPass, 384000.0, 13800.0, 0.0, 0.0, 0.0, 3840, 10},
        {"OnePoleLowPassAtTwiceItsCorner", Design::OnePoleLowPass, 384000.0, 13800.0, 0.0, 0.0, 0.0, 3840, 276},
        {"FirstOrderShelfAt20Hz", Design::FirstOrderShelf, 96000.0, 594.07, 3386.28, 33.0 / 47.0, 0.0, 9600, 2},
        {"FirstOrderShelfAt1kHz", Design::FirstOrderShelf, 96000.0, 594.07, 3386.28, 33.0 / 47.0, 0.0, 9600, 100},
        {"FirstOrderShelfAt6kHz", Design::FirstOrderShelf, 96000.0, 594.07, 3386.28, 33.0 / 47.0, 0.0, 9600, 600},
        {"EqualCornerShelfAt30Hz", Design::FirstOrderShelf, K_RATE_48K, 1000.0, 1000.0, 2.0, 0.0, K_WINDOW_48K, 3},
        {"EqualCornerShelfAt300Hz", Design::FirstOrderShelf, K_RATE_48K, 1000.0, 1000.0, 2.0, 0.0, K_WINDOW_48K,
         30},
        {"EqualCornerShelfAt3kHz", Design::FirstOrderShelf, K_RATE_48K, 1000.0, 1000.0, 2.0, 0.0, K_WINDOW_48K,
         300},
        {"EqualCornerShelfAt15kHz", Design::FirstOrderShelf, K_RATE_48K, 1000.0, 1000.0, 2.0, 0.0, K_WINDOW_48K,
         1500},
        {"FirstOrderHighPassIsMinusThreeDecibelsAtItsCorner", Design::FirstOrderHighPass, K_RATE_48K, 100.0, 0.0,
         0.0, 0.0, K_WINDOW_48K, 10},
        {"FirstOrderHighPassIsMinusPointZeroFourDecibelsADecadeAboveItsCorner", Design::FirstOrderHighPass,
         K_RATE_48K, 100.0, 0.0, 0.0, 0.0, K_WINDOW_48K, 100},
    };

    /// The low corners of a first-order high-pass at the 384 kHz oversampled rate; the probe is 1 kHz.
    const Row kLowCornerRows[] = {
        {"Corner2Hz", Design::FirstOrderHighPass, 384000.0, 2.0, 0.0, 0.0, 0.0, 3840, 10},
        {"Corner5Hz", Design::FirstOrderHighPass, 384000.0, 5.0, 0.0, 0.0, 0.0, 3840, 10},
        {"Corner7Point23Hz", Design::FirstOrderHighPass, 384000.0, 7.23, 0.0, 0.0, 0.0, 3840, 10},
        {"Corner10Point58Hz", Design::FirstOrderHighPass, 384000.0, 10.58, 0.0, 0.0, 0.0, 3840, 10},
    };

    std::string row_name(const ::testing::TestParamInfo<Row>& info)
    {
        return info.param.name;
    }

    class Small_BiquadMagnitude : public ::testing::TestWithParam<Row>
    {
    };

    class Small_BiquadLowCorner : public ::testing::TestWithParam<Row>
    {
    };

    /// @brief Returns count samples of a linear congruential noise in [-0.5, 0.5).
    std::vector<float> noise_samples(std::size_t count)
    {
        std::vector<float> samples(count);
        std::uint32_t state = 22695477u;
        for (float& sample : samples)
        {
            state = state * 1103515245u + 12345u;
            sample = 0.5f * (static_cast<float>(state >> 16) / 32768.0f - 1.0f);
        }
        return samples;
    }

    /// The outcome of running a signal through two filters in series.
    struct PairRun
    {
            double max_error = 0.0;
            double max_intermediate = 0.0;
            double max_input = 0.0;
    };

    /// @brief Runs the input through first then second, comparing the output with the input.
    PairRun run_pair(SushiDSP::Biquad& first, SushiDSP::Biquad& second, const std::vector<float>& input)
    {
        first.reset();
        second.reset();
        PairRun run;
        for (const float sample : input)
        {
            const float middle = first.process_sample(sample);
            const float output = second.process_sample(middle);
            run.max_error = std::max(run.max_error, std::abs(static_cast<double>(output) - sample));
            run.max_intermediate = std::max(run.max_intermediate, std::abs(static_cast<double>(middle)));
            run.max_input = std::max(run.max_input, std::abs(static_cast<double>(sample)));
        }
        return run;
    }

    /// @brief Feeds a constant to the filter count times.
    void run_constant(SushiDSP::Biquad& filter, float value, int count)
    {
        for (int n = 0; n < count; ++n)
        {
            filter.process_sample(value);
        }
    }
} // namespace

TEST_P(Small_BiquadMagnitude, MeasuredGainMatchesTheAnalogPrototype)
{
    EXPECT_TRUE(TestSupport::is_within(gain_error(GetParam())));
}

INSTANTIATE_TEST_SUITE_P(Probes, Small_BiquadMagnitude, ::testing::ValuesIn(kMagnitudeRows), row_name);

// The oracle is the literal 1 / sqrt(2), not the formula. See tests/foundation/math/README.md.
TEST(Small_Biquad, OnePoleLowPassIsMinusThreeDecibelsAtItsCorner)
{
    const Row corner = {"Corner", Design::OnePoleLowPass, 384000.0, 13800.0, 0.0, 0.0, 0.0, 3840, 138};
    Case built = build_case(corner);
    const long long settle = settle_samples(built.pole_radius);
    const double l1_norm = impulse_l1(built.filter, settle);
    const double measured = measure_gain(built.filter, corner, settle);

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(measured, 1.0 / std::sqrt(2.0), gain_bound(built, l1_norm))));
}

// A one-pole loses 6.02 dB per octave, not 12. See tests/foundation/math/README.md.
TEST(Small_Biquad, OnePoleLowPassLosesSixDecibelsPerOctaveFarAboveItsCorner)
{
    constexpr double sample_rate = 384000.0;
    constexpr double corner_hz = 100.0;
    const Row lower = {"Lower", Design::OnePoleLowPass, sample_rate, corner_hz, 0.0, 0.0, 0.0, 3840, 40};
    const Row upper = {"Upper", Design::OnePoleLowPass, sample_rate, corner_hz, 0.0, 0.0, 0.0, 3840, 80};
    Case low = build_case(lower);
    Case high = build_case(upper);
    const long long settle = settle_samples(low.pole_radius);
    const double low_l1 = impulse_l1(low.filter, settle);
    const double high_l1 = impulse_l1(high.filter, settle);
    const double low_gain = measure_gain(low.filter, lower, settle);
    const double high_gain = measure_gain(high.filter, upper, settle);

    const double drop_db = 20.0 * std::log10(high_gain / low_gain);
    const double e_db = 10.0 / std::log(10.0);
    const double upper_x = K_PI * 8000.0 / sample_rate;
    const double lower_omega = std::tan(K_PI * 4000.0 / sample_rate) / std::tan(K_PI * corner_hz / sample_rate);
    const double warp_db = 2.0 * e_db * upper_x * upper_x / 2.0 + e_db / (lower_omega * lower_omega);
    // Each measured gain is off by at most gain_bound / gain relatively, which is 20 log10(e) per unit in dB.
    const double measurement_db = 2.0 * e_db * (gain_bound(high, high_l1) / high_gain + gain_bound(low, low_l1) / low_gain);

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(drop_db, -20.0 * std::log10(2.0), warp_db + measurement_db, 1.0)));
}

// The pair is the identity, up to the float rounding between stages. See tests/foundation/math/README.md.
TEST(Small_Biquad, FirstOrderShelfPairCancelsToTheInputWithinRounding)
{
    const Row pre_row = {"Pre", Design::FirstOrderShelf, K_RATE_48K, 594.07, 3386.28, 33.0 / 47.0, 0.0, 1, 0};
    const Row de_row = {"De", Design::FirstOrderShelf, K_RATE_48K, 3386.28, 594.07, 47.0 / 33.0, 0.0, 1, 0};
    Case pre = build_case(pre_row);
    Case de = build_case(de_row);
    const double de_l1 = impulse_l1(de.filter, settle_samples(de.pole_radius));
    const std::vector<float> input = noise_samples(4096);

    const PairRun run = run_pair(pre.filter, de.filter, input);

    const double bound = kUf * (run.max_intermediate * de_l1 + run.max_input);
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(run.max_error, 0.0, bound)));
}

TEST(Small_Biquad, ResetClearsState)
{
    SushiDSP::Biquad filter;
    filter.set_low_pass(48000.0, 1000.0, 0.707);
    run_constant(filter, 1.0f, 100);

    filter.reset();

    EXPECT_EQ(filter.process_sample(0.0f), 0.0f);
}

// The step response is y[0] R^n. See tests/foundation/math/README.md.
TEST_P(Small_BiquadLowCorner, StepResponseDecaysAsTheRealPolePowerN)
{
    const Row row = GetParam();
    SushiDSP::Biquad filter;
    filter.set_first_order_high_pass(row.sample_rate, row.corner_hz);
    const long long last_index = 2 * static_cast<long long>(row.sample_rate) - 1;

    const float first = filter.process_sample(1.0f);
    float last = first;
    for (long long n = 1; n <= last_index; ++n)
    {
        last = filter.process_sample(1.0f);
    }

    const double expected = std::exp(-2.0 * K_PI * row.corner_hz / row.sample_rate * static_cast<double>(last_index));
    const double relative_bound = 2.0 * kUf + 3.0 * static_cast<double>(last_index) * kUd;
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(static_cast<double>(last) / first, expected, expected * relative_bound)));
}

TEST_P(Small_BiquadLowCorner, PassbandGainMatchesTheAnalogRc)
{
    EXPECT_TRUE(TestSupport::is_within(gain_error(GetParam())));
}

INSTANTIATE_TEST_SUITE_P(Corners, Small_BiquadLowCorner, ::testing::ValuesIn(kLowCornerRows), row_name);

TEST(Small_Biquad, SetCoefficientsLoadsThemVerbatim)
{
    // b = {0.5, 0.5, 0}, a = {0, 0} is a two-tap average whose impulse response is 0.5, 0.5, 0.
    SushiDSP::Biquad average;
    average.set_coefficients(0.5, 0.5, 0.0, 0.0, 0.0);
    EXPECT_FLOAT_EQ(average.process_sample(1.0f), 0.5f);
    EXPECT_FLOAT_EQ(average.process_sample(0.0f), 0.5f);
    EXPECT_FLOAT_EQ(average.process_sample(0.0f), 0.0f);

    // The impulse response is 1, 1/2, 1/4, 1/8. See tests/foundation/math/README.md.
    SushiDSP::Biquad one_pole;
    one_pole.set_coefficients(1.0, 0.0, 0.0, -0.5, 0.0);
    EXPECT_FLOAT_EQ(one_pole.process_sample(1.0f), 1.0f);
    EXPECT_FLOAT_EQ(one_pole.process_sample(0.0f), 0.5f);
    EXPECT_FLOAT_EQ(one_pole.process_sample(0.0f), 0.25f);
    EXPECT_FLOAT_EQ(one_pole.process_sample(0.0f), 0.125f);
}
