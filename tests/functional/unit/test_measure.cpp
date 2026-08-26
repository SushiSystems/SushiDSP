/**************************************************************************/
/* test_measure.cpp                                                       */
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

#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/core/IAudioNode.hpp"
#include "measure/AliasFloor.hpp"
#include "measure/FrequencyResponse.hpp"
#include "measure/NullTest.hpp"
#include "measure/Render.hpp"
#include "measure/Spectrum.hpp"
#include "measure/Thd.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    /// @brief y = tanh(g x): a known nonlinearity whose harmonic series is easy to bound.
    class TanhNode : public SushiDSP::IAudioNode
    {
    public:
        explicit TanhNode(float gain) : gain_(gain) {}

        void prepare(const SushiDSP::PrepareInfo&) override {}

        void process(SushiDSP::ProcessContext& context) override
        {
            for (std::uint32_t i = 0; i < context.frames; ++i)
            {
                context.outputs[0][i] = std::tanh(gain_ * context.inputs[0][i]);
            }
        }

        SushiDSP::PortLayout ports() const override { return { 1, 1 }; }

        float get_parameter(const char*) const override { return 0.0f; }
        void set_parameter(const char*, float) override {}

        void reset() override {}

    private:
        float gain_;
    };

    /// @brief y = x: the pass-through reference used to pin the analysis floor.
    class IdentityNode : public SushiDSP::IAudioNode
    {
    public:
        void prepare(const SushiDSP::PrepareInfo&) override {}

        void process(SushiDSP::ProcessContext& context) override
        {
            for (std::uint32_t i = 0; i < context.frames; ++i)
            {
                context.outputs[0][i] = context.inputs[0][i];
            }
        }

        SushiDSP::PortLayout ports() const override { return { 1, 1 }; }

        float get_parameter(const char*) const override { return 0.0f; }
        void set_parameter(const char*, float) override {}

        void reset() override {}
    };

    /// @brief y = x - x^3/3: third harmonic (A^3/12) / (A - A^3/4) of the fundamental.
    class CubicNode : public SushiDSP::IAudioNode
    {
    public:
        void prepare(const SushiDSP::PrepareInfo&) override {}

        void process(SushiDSP::ProcessContext& context) override
        {
            for (std::uint32_t i = 0; i < context.frames; ++i)
            {
                const float x = context.inputs[0][i];
                context.outputs[0][i] = x - x * x * x / 3.0f;
            }
        }

        SushiDSP::PortLayout ports() const override { return { 1, 1 }; }

        float get_parameter(const char*) const override { return 0.0f; }
        void set_parameter(const char*, float) override {}

        void reset() override {}
    };

    /// @brief First-order lowpass at 1 kHz: -3.01 dB at 1 kHz, -20.04 dB at 10 kHz (analytic RC).
    class OnePoleNode : public SushiDSP::IAudioNode
    {
    public:
        void prepare(const SushiDSP::PrepareInfo& info) override
        {
            coefficient_ = static_cast<float>(std::exp(-2.0 * M_PI * 1000.0 / info.sample_rate));
        }

        void process(SushiDSP::ProcessContext& context) override
        {
            for (std::uint32_t i = 0; i < context.frames; ++i)
            {
                state_ = (1.0f - coefficient_) * context.inputs[0][i] + coefficient_ * state_;
                context.outputs[0][i] = state_;
            }
        }

        SushiDSP::PortLayout ports() const override { return { 1, 1 }; }

        float get_parameter(const char*) const override { return 0.0f; }
        void set_parameter(const char*, float) override {}

        void reset() override { state_ = 0.0f; }

    private:
        float coefficient_ = 0.0f;
        float state_ = 0.0f;
    };
}

TEST(Unit_Dsp, MeasureSpectrumFloorOfPureSineIsBelowMinus110dB)
{
    const double sample_rate = 48000.0;
    const double f0 = SushiDSP::measure::bin_exact_frequency(1000.0, sample_rate, SushiDSP::measure::kFftSize);
    const auto signal = SushiDSP::measure::sine_stimulus(f0, sample_rate, SushiDSP::measure::kFftSize, 0.5f);
    const auto spectrum = SushiDSP::measure::magnitude_spectrum_db(signal, 0);
    const std::uint32_t fundamental_bin = SushiDSP::measure::bin_of(f0, sample_rate);
    double worst = -300.0;
    for (std::uint32_t bin = 8; bin < spectrum.size(); ++bin)
    {
        if (bin + 8 > fundamental_bin && bin < fundamental_bin + 8)
        {
            continue;
        }
        worst = std::max(worst, spectrum[bin] - spectrum[fundamental_bin]);
    }
    EXPECT_LE(worst, -110.0);
}

TEST(Unit_Dsp, MeasureThdOfPureSineIsNegligible)
{
    const double sample_rate = 48000.0;
    const double f0 = SushiDSP::measure::bin_exact_frequency(1000.0, sample_rate, SushiDSP::measure::kFftSize);
    const auto signal = SushiDSP::measure::sine_stimulus(f0, sample_rate, SushiDSP::measure::kFftSize, 0.5f);
    const auto result = SushiDSP::measure::thd(signal, f0, sample_rate);
    EXPECT_LT(result.thd_percent, 1e-3);
    ASSERT_EQ(result.per_harmonic_db.size(), 20u);
    EXPECT_LT(result.per_harmonic_db[1], -100.0);
}

TEST(Unit_Dsp, MeasureThdOfCubicMatchesAnalyticThirdHarmonic)
{
    // A = 0.5: third harmonic 0.010417 / 0.46875 = 2.2222 % of the fundamental.
    const double sample_rate = 48000.0;
    const double f0 = SushiDSP::measure::bin_exact_frequency(1000.0, sample_rate, SushiDSP::measure::kFftSize);
    CubicNode node;
    const auto output = SushiDSP::measure::render(node, SushiDSP::measure::sine_stimulus(f0, sample_rate, SushiDSP::measure::kFftSize, 0.5f), sample_rate, 256);
    const auto result = SushiDSP::measure::thd(output, f0, sample_rate);
    EXPECT_NEAR(result.thd_percent, 2.2222, 0.01);
    EXPECT_NEAR(result.per_harmonic_db[2], 20.0 * std::log10(0.022222), 0.05);
}

TEST(Unit_Dsp, MeasureAliasFloorOfIdentityIsAtTheAnalysisFloor)
{
    IdentityNode node;
    EXPECT_LE(SushiDSP::measure::alias_floor(node, 48000.0), -110.0);
}

TEST(Unit_Dsp, MeasureAliasFloorOfUnoversampledTanhIsPoor)
{
    // A 20 kHz tone through tanh at 48 kHz folds its harmonics straight back into band; the
    // folded third harmonic (60 kHz -> 12 kHz) is tens of dB up. This pins that the sweep
    // counts folded harmonics as aliases rather than masking them.
    TanhNode node(4.0f);
    EXPECT_GT(SushiDSP::measure::alias_floor(node, 48000.0), -40.0);
}

TEST(Unit_Dsp, MeasureFrequencyResponseOfIdentityIsFlat)
{
    IdentityNode node;
    const auto response = SushiDSP::measure::frequency_response(node, 48000.0, { 100.0, 1000.0, 10000.0, 20000.0 });
    ASSERT_EQ(response.size(), 4u);
    for (const auto& point : response)
    {
        EXPECT_NEAR(point.magnitude_db, 0.0, 0.05) << point.frequency_hz;
    }
}

TEST(Unit_Dsp, MeasureFrequencyResponseTracksAOnePoleLowpass)
{
    OnePoleNode node;
    const auto response = SushiDSP::measure::frequency_response(node, 48000.0, { 1000.0, 10000.0 });
    // Expectations are the difference equation's own |H|, not the analogue RC it is derived
    // from: with a = exp(-2*pi*1000/48000), |H(w)| = (1 - a) / |1 - a*exp(-jw)| is -3.004 dB at
    // 1 kHz and -19.415 dB at 10 kHz. The analogue RC's -20.04 dB at 10 kHz is 0.63 dB away,
    // which is the impulse-invariant transform's warping, not measurement error.
    EXPECT_NEAR(response[0].magnitude_db, -3.004, 0.1);
    EXPECT_NEAR(response[1].magnitude_db, -19.415, 0.1);
}

TEST(Unit_Dsp, MeasureNullTestAlignsDelayAndGain)
{
    const double sample_rate = 48000.0;
    const auto a = SushiDSP::measure::sine_stimulus(440.0, sample_rate, 48000, 0.5f);
    std::vector<float> b(a.size(), 0.0f);
    for (std::size_t i = 37; i < b.size(); ++i)
    {
        b[i] = 0.25f * a[i - 37];
    }
    EXPECT_LT(SushiDSP::measure::null_test(a, b, 64), -120.0);
}

TEST(Unit_Dsp, MeasureNullTestReportsARealDifference)
{
    const double sample_rate = 48000.0;
    const auto a = SushiDSP::measure::sine_stimulus(440.0, sample_rate, 48000, 0.5f);
    auto b = a;
    for (std::size_t i = 0; i < b.size(); ++i)
    {
        b[i] += 0.005f * static_cast<float>(std::sin(2.0 * M_PI * 3000.0 * static_cast<double>(i) / sample_rate));
    }
    EXPECT_NEAR(SushiDSP::measure::null_test(a, b, 64), -40.0, 0.5);
}

#include "SushiDSP/jcm800/Jcm800.hpp"
#include "SushiDSP/tubescreamer/TubeScreamer.hpp"

// Prints the pre-D6a alias floors so the D6a oversampler acceptance (>= 30 dB better) has a
// measured baseline. Assertions are deliberately loose: this case documents, it does not gate.
TEST(Unit_Dsp, MeasureAliasFloorBaselineIsPrinted)
{
    SushiDSP::TubeScreamer tube_screamer;
    tube_screamer.set_drive(1.0f);
    tube_screamer.set_tone(0.5f);
    const double tube_screamer_floor = SushiDSP::measure::alias_floor(tube_screamer, 48000.0);

    SushiDSP::Jcm800 jcm800;
    jcm800.set_preamp(1.0f);
    jcm800.set_master(0.0f);
    jcm800.set_bass(0.5f);
    jcm800.set_middle(0.5f);
    jcm800.set_treble(0.5f);
    jcm800.set_presence(0.5f);
    const double jcm800_floor = SushiDSP::measure::alias_floor(jcm800, 48000.0);

    std::printf("[ measure ] alias_floor @48k: TubeScreamer drive=1 %.1f dB, Jcm800 preamp=10 %.1f dB\n", tube_screamer_floor, jcm800_floor);
    EXPECT_LT(tube_screamer_floor, 0.0);
    EXPECT_LT(jcm800_floor, 0.0);
}

