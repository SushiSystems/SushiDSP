/**************************************************************************/
/* test_oversampler.cpp                                                   */
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

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI.
#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/math/HalfbandCoefficients.hpp"
#include "SushiDSP/math/Oversampler.hpp"
#include "measure/Spectrum.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    std::vector<float> sine(double cycles_per_sample, std::size_t length, float amplitude)
    {
        std::vector<float> signal(length);
        for (std::size_t i = 0; i < length; ++i)
        {
            signal[i] = amplitude * static_cast<float>(std::sin(2.0 * M_PI * cycles_per_sample * static_cast<double>(i)));
        }
        return signal;
    }

    /// @brief Direct-form FIR of the zero-stuffed input: the textbook definition polyphase must match.
    std::vector<float> reference_upsample(const SushiDSP::HalfbandDesign& design, const std::vector<float>& input)
    {
        std::vector<float> stuffed(input.size() * 2, 0.0f);
        for (std::size_t i = 0; i < input.size(); ++i)
        {
            stuffed[2 * i] = 2.0f * input[i];
        }
        std::vector<float> output(stuffed.size(), 0.0f);
        for (std::size_t n = 0; n < stuffed.size(); ++n)
        {
            double accumulator = 0.0;
            for (std::uint32_t k = 0; k < design.tap_count && k <= n; ++k)
            {
                accumulator += static_cast<double>(design.taps[k]) * stuffed[n - k];
            }
            output[n] = static_cast<float>(accumulator);
        }
        return output;
    }

    /// @brief Direct-form FIR of the input, keeping only the even-indexed filtered samples.
    std::vector<float> reference_downsample(const SushiDSP::HalfbandDesign& design, const std::vector<float>& input)
    {
        std::vector<float> output(input.size() / 2, 0.0f);
        for (std::size_t m = 0; m < output.size(); ++m)
        {
            const std::size_t n = 2 * m;
            double accumulator = 0.0;
            for (std::uint32_t k = 0; k < design.tap_count && k <= n; ++k)
            {
                accumulator += static_cast<double>(design.taps[k]) * input[n - k];
            }
            output[m] = static_cast<float>(accumulator);
        }
        return output;
    }

    double peak_db(const std::vector<double>& spectrum)
    {
        double peak = -300.0;
        for (const double value : spectrum)
        {
            peak = std::max(peak, value);
        }
        return peak;
    }
}

TEST(Unit_Dsp, HalfbandDesignsHaveExactZeroTapsAndUnityDcGain)
{
    for (const SushiDSP::HalfbandDesign* design : { &SushiDSP::kHalfbandStage1, &SushiDSP::kHalfbandStage2, &SushiDSP::kHalfbandStage3 })
    {
        ASSERT_EQ(design->tap_count % 4, 3u);
        const std::uint32_t centre = (design->tap_count - 1) / 2;
        double sum = 0.0;
        for (std::uint32_t i = 0; i < design->tap_count; ++i)
        {
            // tap_count = 4M-1 puts the centre at index 2M-1. The halfband zeros sit at even
            // distances from the centre; the surviving taps are at odd distances 1, 3, 5, ...
            if (i != centre && (i > centre ? i - centre : centre - i) % 2 == 0)
            {
                EXPECT_EQ(design->taps[i], 0.0f) << i;
            }
            EXPECT_EQ(design->taps[i], design->taps[design->tap_count - 1 - i]) << i;
            sum += design->taps[i];
        }
        EXPECT_EQ(design->taps[centre], 0.5f);
        EXPECT_NEAR(sum, 1.0, 1e-6);
        EXPECT_LE(design->stopband_db, -90.0f);
    }
}

TEST(Unit_Dsp, HalfbandUpsamplerMatchesDirectFormOfZeroStuffedInput)
{
    const auto input = sine(0.0731, 4096, 0.7f);
    SushiDSP::HalfbandUpsampler2x upsampler(SushiDSP::kHalfbandStage1);
    std::vector<float> output(input.size() * 2, 0.0f);
    // Two blocks, so streaming state across a block boundary is covered.
    upsampler.process(input.data(), 2048, output.data());
    upsampler.process(input.data() + 2048, 2048, output.data() + 4096);
    const auto reference = reference_upsample(SushiDSP::kHalfbandStage1, input);
    for (std::size_t i = 0; i < output.size(); ++i)
    {
        ASSERT_NEAR(output[i], reference[i], 1e-6f) << i;
    }
}

TEST(Unit_Dsp, HalfbandDownsamplerMatchesDirectForm)
{
    const auto input = sine(0.0377, 8192, 0.7f);
    SushiDSP::HalfbandDownsampler2x downsampler(SushiDSP::kHalfbandStage1);
    std::vector<float> output(input.size() / 2, 0.0f);
    downsampler.process(input.data(), 4096, output.data());
    downsampler.process(input.data() + 4096, 4096, output.data() + 2048);
    const auto reference = reference_downsample(SushiDSP::kHalfbandStage1, input);
    for (std::size_t i = 0; i < output.size(); ++i)
    {
        ASSERT_NEAR(output[i], reference[i], 1e-6f) << i;
    }
}

TEST(Unit_Dsp, Oversampler8xImagesOfInBandTonesAreBelowMinus90dB)
{
    // Spec D6a: image rejection >= 90 dB at 0.3*f_base and beyond. A tone's images sit at
    // k*f_base +- f; every line in the 8x spectrum other than the tone must be >= 90 dB down.
    const std::size_t base_frames = SushiDSP::measure::kFftSize / 8;
    for (const double fraction : { 0.30, 0.35, 0.40, 0.45 })
    {
        const double cycles = std::round(fraction * base_frames) / base_frames;
        const auto input = sine(cycles, base_frames * 2, 0.5f);
        SushiDSP::Oversampler8x oversampler(256);
        std::vector<float> oversampled(input.size() * 8, 0.0f);
        for (std::size_t position = 0; position < input.size(); position += 256)
        {
            const float* block = oversampler.upsample(input.data() + position, 256);
            std::copy(block, block + 2048, oversampled.begin() + position * 8);
        }
        const auto spectrum = SushiDSP::measure::magnitude_spectrum_db(oversampled, oversampled.size() - SushiDSP::measure::kFftSize);
        const std::uint32_t tone_bin = static_cast<std::uint32_t>(std::lround(cycles / 8.0 * SushiDSP::measure::kFftSize));
        const double fundamental = SushiDSP::measure::peak_near(spectrum, tone_bin);
        double worst = -300.0;
        for (std::uint32_t bin = 8; bin < spectrum.size(); ++bin)
        {
            if (bin + 8 > tone_bin && bin < tone_bin + 8)
            {
                continue;
            }
            worst = std::max(worst, spectrum[bin] - fundamental);
        }
        EXPECT_LE(worst, -90.0) << "fraction " << fraction;
    }
}

TEST(Unit_Dsp, Oversampler8xAliasesOfOutOfBandTonesAreBelowMinus90dB)
{
    // Tones at the 8x rate from 0.55*f_base to 3.95*f_base in 0.05 steps, decimated to base
    // rate: whatever folds into [0, 0.45*f_base] must be >= 90 dB below the tone. Folds that
    // land in the stage-1 transition band (0.45..0.5)*f_base are the filter's don't-care
    // region and are skipped.
    const std::size_t base_frames = SushiDSP::measure::kFftSize;
    // Reference level: the same-amplitude tone analysed with the same window at base rate. It
    // has to be bin-exact, as the folded aliases are; a tone off-bin loses up to 0.83 dB to
    // Blackman-Harris scalloping, which would flatter every alias by that much.
    const double reference_cycles = std::round(0.1 * base_frames) / base_frames;
    const double reference = peak_db(SushiDSP::measure::magnitude_spectrum_db(sine(reference_cycles, base_frames * 2, 0.5f), base_frames));
    // Integer-stepped: accumulating += 0.05 in double drifts, and near-integer fractions
    // then fold to ~1e-7 instead of ~0, where the sweep would assert nothing.
    for (int step = 11; step <= 79; ++step)
    {
        const double fraction = step * 0.05;
        double folded = std::fmod(fraction, 1.0);
        folded = folded > 0.5 ? 1.0 - folded : folded;
        if (folded > 0.45)
        {
            continue;
        }
        const double cycles_at_8x = std::round(fraction / 8.0 * base_frames * 8) / (base_frames * 8);
        const auto input = sine(cycles_at_8x, base_frames * 8 * 2, 0.5f);
        SushiDSP::Oversampler8x oversampler(256);
        std::vector<float> output(base_frames * 2, 0.0f);
        for (std::size_t position = 0; position < output.size(); position += 256)
        {
            oversampler.downsample(input.data() + position * 8, 2048, output.data() + position);
        }
        const auto spectrum = SushiDSP::measure::magnitude_spectrum_db(output, output.size() - SushiDSP::measure::kFftSize);
        const std::uint32_t band_edge = static_cast<std::uint32_t>(0.45 * SushiDSP::measure::kFftSize);
        double worst = -300.0;
        for (std::uint32_t bin = 8; bin <= band_edge; ++bin)
        {
            worst = std::max(worst, spectrum[bin]);
        }
        EXPECT_LE(worst - reference, -90.0) << "fraction " << fraction;
    }
}

TEST(Unit_Dsp, Oversampler8xRoundTripDroopAt20kIsWithinTenthOfADecibel)
{
    // 20 kHz at 48 kHz is 0.4167*f_base, inside the 0.45 passband; round trip through six stages.
    const std::size_t base_frames = SushiDSP::measure::kFftSize;
    const double cycles = std::round(20000.0 / 48000.0 * base_frames) / base_frames;
    const auto input = sine(cycles, base_frames * 2, 0.5f);
    SushiDSP::Oversampler8x oversampler(256);
    std::vector<float> output(input.size(), 0.0f);
    for (std::size_t position = 0; position < input.size(); position += 256)
    {
        const float* up = oversampler.upsample(input.data() + position, 256);
        oversampler.downsample(up, 2048, output.data() + position);
    }
    double input_rms = 0.0, output_rms = 0.0;
    for (std::size_t i = base_frames; i < input.size(); ++i)
    {
        input_rms += static_cast<double>(input[i]) * input[i];
        output_rms += static_cast<double>(output[i]) * output[i];
    }
    EXPECT_NEAR(10.0 * std::log10(output_rms / input_rms), 0.0, 0.1);
}

TEST(Unit_Dsp, Oversampler8xLatencyMatchesMeasuredImpulseDelay)
{
    SushiDSP::Oversampler8x oversampler(256);
    std::vector<float> input(2048, 0.0f);
    input[0] = 1.0f;
    std::vector<float> output(2048, 0.0f);
    for (std::size_t position = 0; position < input.size(); position += 256)
    {
        const float* up = oversampler.upsample(input.data() + position, 256);
        oversampler.downsample(up, 2048, output.data() + position);
    }
    std::size_t peak = 0;
    for (std::size_t i = 1; i < output.size(); ++i)
    {
        if (std::fabs(output[i]) > std::fabs(output[peak]))
        {
            peak = i;
        }
    }
    // The true group delay is fractional at base rate; the reported floor is within one sample.
    const long difference = static_cast<long>(peak) - static_cast<long>(oversampler.latency_samples());
    EXPECT_GE(difference, 0);
    EXPECT_LE(difference, 1);
}

TEST(Unit_Dsp, Oversampler4xRoundTripDroopAt20kIsWithinTenthOfADecibel)
{
    // 20 kHz at 48 kHz is 0.4167*f_base, inside the 0.45 passband; round trip through the two
    // stages shared with Oversampler8x's own stages 1 and 2.
    const std::size_t base_frames = SushiDSP::measure::kFftSize;
    const double cycles = std::round(20000.0 / 48000.0 * base_frames) / base_frames;
    const auto input = sine(cycles, base_frames * 2, 0.5f);
    SushiDSP::Oversampler4x oversampler(256);
    std::vector<float> output(input.size(), 0.0f);
    for (std::size_t position = 0; position < input.size(); position += 256)
    {
        const float* up = oversampler.upsample(input.data() + position, 256);
        oversampler.downsample(up, 1024, output.data() + position);
    }
    double input_rms = 0.0, output_rms = 0.0;
    for (std::size_t i = base_frames; i < input.size(); ++i)
    {
        input_rms += static_cast<double>(input[i]) * input[i];
        output_rms += static_cast<double>(output[i]) * output[i];
    }
    EXPECT_NEAR(10.0 * std::log10(output_rms / input_rms), 0.0, 0.1);
}

TEST(Unit_Dsp, Oversampler4xLatencyMatchesMeasuredImpulseDelay)
{
    SushiDSP::Oversampler4x oversampler(256);
    std::vector<float> input(2048, 0.0f);
    input[0] = 1.0f;
    std::vector<float> output(2048, 0.0f);
    for (std::size_t position = 0; position < input.size(); position += 256)
    {
        const float* up = oversampler.upsample(input.data() + position, 256);
        oversampler.downsample(up, 1024, output.data() + position);
    }
    std::size_t peak = 0;
    for (std::size_t i = 1; i < output.size(); ++i)
    {
        if (std::fabs(output[i]) > std::fabs(output[peak]))
        {
            peak = i;
        }
    }
    // The impulse peak is the round trip: 31.75 base-rate samples up and 31.75 back, 63.5 in
    // all, fractional because the 115-tap stage runs at 2x (see the derivation in
    // Oversampler4x::latency_samples). The reported floor is within one sample of it.
    const long difference = static_cast<long>(peak) - static_cast<long>(oversampler.latency_samples());
    EXPECT_GE(difference, 0);
    EXPECT_LE(difference, 1);
}

TEST(Unit_Dsp, Oversampler8xPassesDcAtUnity)
{
    SushiDSP::Oversampler8x oversampler(256);
    std::vector<float> input(256, 1.0f);
    std::vector<float> output(256, 0.0f);
    for (int block = 0; block < 8; ++block)
    {
        const float* up = oversampler.upsample(input.data(), 256);
        oversampler.downsample(up, 2048, output.data());
    }
    EXPECT_NEAR(output[255], 1.0f, 1e-4f);
}

TEST(Unit_Dsp, ResetReturnsEveryStageToItsAsConstructedState)
{
    // An instance that has processed a burst and then been reset must produce bit-identical
    // output to a never-used instance, on all four classes. Oversampler4x is here because it is
    // what every shipping D6c node runs; TubeScreamer::reset() is only as good as this.
    const auto burst = sine(0.1913, 512, 0.9f);
    const auto probe = sine(0.0611, 128, 0.6f);

    {
        SushiDSP::HalfbandUpsampler2x used(SushiDSP::kHalfbandStage1);
        SushiDSP::HalfbandUpsampler2x fresh(SushiDSP::kHalfbandStage1);
        std::vector<float> scratch(burst.size() * 2, 0.0f);
        used.process(burst.data(), static_cast<std::uint32_t>(burst.size()), scratch.data());
        used.reset();
        std::vector<float> after_reset(probe.size() * 2, 0.0f), never_used(probe.size() * 2, 0.0f);
        used.process(probe.data(), static_cast<std::uint32_t>(probe.size()), after_reset.data());
        fresh.process(probe.data(), static_cast<std::uint32_t>(probe.size()), never_used.data());
        EXPECT_EQ(after_reset, never_used);
    }

    {
        SushiDSP::HalfbandDownsampler2x used(SushiDSP::kHalfbandStage1);
        SushiDSP::HalfbandDownsampler2x fresh(SushiDSP::kHalfbandStage1);
        std::vector<float> scratch(burst.size() / 2, 0.0f);
        used.process(burst.data(), static_cast<std::uint32_t>(burst.size()), scratch.data());
        used.reset();
        std::vector<float> after_reset(probe.size() / 2, 0.0f), never_used(probe.size() / 2, 0.0f);
        used.process(probe.data(), static_cast<std::uint32_t>(probe.size()), after_reset.data());
        fresh.process(probe.data(), static_cast<std::uint32_t>(probe.size()), never_used.data());
        EXPECT_EQ(after_reset, never_used);
    }

    {
        SushiDSP::Oversampler4x used(128);
        SushiDSP::Oversampler4x fresh(128);
        std::vector<float> scratch(128, 0.0f);
        for (std::size_t position = 0; position + 128 <= burst.size(); position += 128)
        {
            const float* up = used.upsample(burst.data() + position, 128);
            used.downsample(up, 512, scratch.data());
        }
        used.reset();
        // The 4x buffer is internally owned and reused, so each block is copied out before the
        // next call overwrites it.
        std::vector<float> after_reset_up(512, 0.0f), never_used_up(512, 0.0f);
        std::vector<float> after_reset(128, 0.0f), never_used(128, 0.0f);
        const float* used_up = used.upsample(probe.data(), 128);
        std::copy(used_up, used_up + 512, after_reset_up.begin());
        used.downsample(after_reset_up.data(), 512, after_reset.data());
        const float* fresh_up = fresh.upsample(probe.data(), 128);
        std::copy(fresh_up, fresh_up + 512, never_used_up.begin());
        fresh.downsample(never_used_up.data(), 512, never_used.data());
        EXPECT_EQ(after_reset_up, never_used_up);
        EXPECT_EQ(after_reset, never_used);
    }

    {
        SushiDSP::Oversampler8x used(128);
        SushiDSP::Oversampler8x fresh(128);
        std::vector<float> scratch(128, 0.0f);
        for (std::size_t position = 0; position + 128 <= burst.size(); position += 128)
        {
            const float* up = used.upsample(burst.data() + position, 128);
            used.downsample(up, 1024, scratch.data());
        }
        used.reset();
        // The 8x buffer is internally owned and reused, so each block is copied out before the
        // next call overwrites it.
        std::vector<float> after_reset_up(1024, 0.0f), never_used_up(1024, 0.0f);
        std::vector<float> after_reset(128, 0.0f), never_used(128, 0.0f);
        const float* used_up = used.upsample(probe.data(), 128);
        std::copy(used_up, used_up + 1024, after_reset_up.begin());
        used.downsample(after_reset_up.data(), 1024, after_reset.data());
        const float* fresh_up = fresh.upsample(probe.data(), 128);
        std::copy(fresh_up, fresh_up + 1024, never_used_up.begin());
        fresh.downsample(never_used_up.data(), 1024, never_used.data());
        EXPECT_EQ(after_reset_up, never_used_up);
        EXPECT_EQ(after_reset, never_used);
    }
}
