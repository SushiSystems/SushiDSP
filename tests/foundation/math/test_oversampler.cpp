/**************************************************************************/
/* test_oversampler.cpp                                                   */
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
 * @file test_oversampler.cpp
 * @brief Tests the halfband filters and the 4x and 8x oversamplers.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/math/halfband_coefficients.hpp>
#include <SushiDSP/foundation/math/oversampler.hpp>
#include <SushiDSP/foundation/signal_measure/spectrum.hpp>

namespace
{
    namespace measure = SushiDSP::measure;

    const double kU = TestSupport::unit_roundoff<float>();

    /// Pi to double precision, spelled out so the tests do not depend on M_PI being visible.
    constexpr double K_PI = 3.14159265358979323846;

    /// Spec D6a: every image and alias is at least this far below the tone, in decibels.
    constexpr double K_SPEC_REJECTION_DB = -90.0;

    /// Spec D6a: the 20 kHz round trip droops by at most this much, in decibels.
    constexpr double K_SPEC_DROOP_DB = 0.1;

    /// The block size of the block-wise round trips, in base-rate frames.
    constexpr std::uint32_t K_BLOCK = 256;

    /// The base-rate window of the spectrum checks.
    constexpr std::size_t K_BASE_FRAMES = measure::K_FFT_SIZE;

    /// The edge of the passband, as a fraction of the base rate; folds above it are a don't-care region.
    constexpr double K_PASSBAND_EDGE = 0.45;

    /// @brief Returns length samples of a sine at cycles_per_sample.
    std::vector<float> sine(double cycles_per_sample, std::size_t length, float amplitude)
    {
        std::vector<float> signal(length);
        for (std::size_t i = 0; i < length; ++i)
        {
            signal[i] = amplitude * static_cast<float>(std::sin(2.0 * K_PI * cycles_per_sample * static_cast<double>(i)));
        }
        return signal;
    }

    /// @brief Returns the frequency, in cycles per base-rate sample, nearest the fraction on a bin of the window.
    double bin_exact_cycles(double fraction)
    {
        return std::round(fraction * static_cast<double>(K_BASE_FRAMES)) / static_cast<double>(K_BASE_FRAMES);
    }

    /// @brief Returns the sum of the magnitudes of the taps.
    double magnitude_sum(const SushiDSP::HalfbandDesign& design)
    {
        double sum = 0.0;
        for (std::uint32_t i = 0; i < design.tap_count; ++i)
        {
            sum += std::abs(static_cast<double>(design.taps[i]));
        }
        return sum;
    }

    /// @brief Returns the pairs of taps a halfband stage sums per output: the odd centre offsets.
    std::size_t pair_count(const SushiDSP::HalfbandDesign& design)
    {
        return ((design.tap_count - 1) / 2 + 1) / 2;
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

    /// @brief Returns the largest absolute difference between two equally long signals.
    double worst_difference(const std::vector<float>& computed, const std::vector<float>& reference)
    {
        double worst = 0.0;
        for (std::size_t i = 0; i < computed.size(); ++i)
        {
            worst = std::max(worst, std::abs(static_cast<double>(computed[i]) - reference[i]));
        }
        return worst;
    }

    /// @brief Returns the largest absolute sample.
    double peak_of(const std::vector<float>& signal)
    {
        double peak = 0.0;
        for (const float sample : signal)
        {
            peak = std::max(peak, std::abs(static_cast<double>(sample)));
        }
        return peak;
    }

    /// @brief Returns the error bound of a float halfband output against the double-precision direct
    /// form: the accumulator sums the pair terms and the centre term, each with two roundings of its own
    /// (the pair sum and the product), and the reference is rounded to float once more; the terms
    /// add up to at most Σ|h| max|x'|, with x' the zero-stuffed input.
    double halfband_bound(const SushiDSP::HalfbandDesign& design, double largest_stuffed_sample)
    {
        const double operations = static_cast<double>(pair_count(design)) + 4.0;
        return operations * kU * magnitude_sum(design) * largest_stuffed_sample;
    }

    /// @brief Runs the halfband upsampler over the input in two equal blocks, so state crosses a boundary.
    std::vector<float> upsample_in_two_blocks(const std::vector<float>& input)
    {
        SushiDSP::HalfbandUpsampler2x upsampler(SushiDSP::K_HALFBAND_STAGE1);
        std::vector<float> output(input.size() * 2, 0.0f);
        const std::uint32_t half = static_cast<std::uint32_t>(input.size() / 2);
        upsampler.process(input.data(), half, output.data());
        upsampler.process(input.data() + half, half, output.data() + half * 2);
        return output;
    }

    /// @brief Runs the halfband downsampler over the input in two equal blocks.
    std::vector<float> downsample_in_two_blocks(const std::vector<float>& input)
    {
        SushiDSP::HalfbandDownsampler2x downsampler(SushiDSP::K_HALFBAND_STAGE1);
        std::vector<float> output(input.size() / 2, 0.0f);
        const std::uint32_t half = static_cast<std::uint32_t>(input.size() / 2);
        downsampler.process(input.data(), half, output.data());
        downsampler.process(input.data() + half, half, output.data() + half / 2);
        return output;
    }

    /// @brief Runs the input through an oversampler block by block and returns what comes back at the base rate.
    template <typename Oversampler, std::uint32_t Factor>
    std::vector<float> round_trip(const std::vector<float>& input)
    {
        Oversampler oversampler(K_BLOCK);
        std::vector<float> output(input.size(), 0.0f);
        for (std::size_t position = 0; position < input.size(); position += K_BLOCK)
        {
            const float* up = oversampler.upsample(input.data() + position, K_BLOCK);
            oversampler.downsample(up, K_BLOCK * Factor, output.data() + position);
        }
        return output;
    }

    /// @brief Returns the reported latency of a fresh oversampler.
    template <typename Oversampler>
    std::uint32_t reported_latency()
    {
        return Oversampler(K_BLOCK).latency_samples();
    }

    /// @brief Returns the index of the largest magnitude of the round trip of a unit impulse.
    template <typename Oversampler, std::uint32_t Factor>
    std::size_t impulse_peak_index()
    {
        std::vector<float> impulse(2048, 0.0f);
        impulse[0] = 1.0f;
        const std::vector<float> output = round_trip<Oversampler, Factor>(impulse);
        std::size_t peak = 0;
        for (std::size_t i = 1; i < output.size(); ++i)
        {
            peak = std::abs(output[i]) > std::abs(output[peak]) ? i : peak;
        }
        return peak;
    }

    /// @brief Returns the gain in decibels of the 20 kHz round trip, read over whole cycles of the settled half.
    template <typename Oversampler, std::uint32_t Factor>
    double round_trip_gain_db_at_20k()
    {
        const double cycles = bin_exact_cycles(20000.0 / 48000.0);
        const std::vector<float> input = sine(cycles, K_BASE_FRAMES * 2, 0.5f);
        const std::vector<float> output = round_trip<Oversampler, Factor>(input);
        double input_power = 0.0;
        double output_power = 0.0;
        for (std::size_t i = K_BASE_FRAMES; i < input.size(); ++i)
        {
            input_power += static_cast<double>(input[i]) * input[i];
            output_power += static_cast<double>(output[i]) * output[i];
        }
        return 10.0 * std::log10(output_power / input_power);
    }

    /// One oversampler class under test, with the probes it shares with the others.
    struct ClassRow
    {
            const char* name;
            double (*gain_db_at_20k)();
            std::size_t (*impulse_peak)();
            std::uint32_t (*latency)();
    };

    const ClassRow kClassRows[] = {
        {"Oversampler4x", &round_trip_gain_db_at_20k<SushiDSP::Oversampler4x, 4>,
         &impulse_peak_index<SushiDSP::Oversampler4x, 4>, &reported_latency<SushiDSP::Oversampler4x>},
        {"Oversampler8x", &round_trip_gain_db_at_20k<SushiDSP::Oversampler8x, 8>,
         &impulse_peak_index<SushiDSP::Oversampler8x, 8>, &reported_latency<SushiDSP::Oversampler8x>},
    };

    std::string class_name(const ::testing::TestParamInfo<ClassRow>& info)
    {
        return info.param.name;
    }

    class Small_OversamplerClass : public ::testing::TestWithParam<ClassRow>
    {
    };

    /// @brief Returns the largest level, relative to the tone, of any line of the 8x spectrum outside the
    /// tone's main lobe, for a tone at the given fraction of the base rate.
    double worst_image_db(double fraction)
    {
        const std::size_t base_frames = measure::K_FFT_SIZE / 8;
        const double cycles =
            std::round(fraction * static_cast<double>(base_frames)) / static_cast<double>(base_frames);
        const std::vector<float> input = sine(cycles, base_frames * 2, 0.5f);
        SushiDSP::Oversampler8x oversampler(K_BLOCK);
        std::vector<float> oversampled(input.size() * 8, 0.0f);
        for (std::size_t position = 0; position < input.size(); position += K_BLOCK)
        {
            const float* block = oversampler.upsample(input.data() + position, K_BLOCK);
            std::copy(block, block + K_BLOCK * 8, oversampled.begin() + static_cast<std::ptrdiff_t>(position * 8));
        }
        const std::vector<double> spectrum =
            measure::magnitude_spectrum_db(oversampled, oversampled.size() - measure::K_FFT_SIZE);
        const std::uint32_t tone_bin = static_cast<std::uint32_t>(std::lround(cycles / 8.0 * measure::K_FFT_SIZE));
        const double fundamental = measure::peak_near(spectrum, tone_bin);
        double worst = -300.0;
        for (std::uint32_t bin = 8; bin < spectrum.size(); ++bin)
        {
            const bool in_main_lobe = bin + 8 > tone_bin && bin < tone_bin + 8;
            worst = in_main_lobe ? worst : std::max(worst, spectrum[bin] - fundamental);
        }
        return worst;
    }

    /// @brief Returns the level of the strongest line below the passband edge after an 8x tone at the
    /// given fraction of the base rate is decimated, relative to a bin-exact in-band reference.
    double worst_alias_db(int step)
    {
        static const double reference = [] {
            const std::vector<float> tone = sine(bin_exact_cycles(0.1), K_BASE_FRAMES * 2, 0.5f);
            const std::vector<double> spectrum = measure::magnitude_spectrum_db(tone, K_BASE_FRAMES);
            return *std::max_element(spectrum.begin(), spectrum.end());
        }();
        const double fraction = step * 0.05;
        const double cycles_at_8x = bin_exact_cycles(fraction) / 8.0;
        const std::vector<float> input = sine(cycles_at_8x, K_BASE_FRAMES * 8 * 2, 0.5f);
        SushiDSP::Oversampler8x oversampler(K_BLOCK);
        std::vector<float> output(K_BASE_FRAMES * 2, 0.0f);
        for (std::size_t position = 0; position < output.size(); position += K_BLOCK)
        {
            oversampler.downsample(input.data() + position * 8, K_BLOCK * 8, output.data() + position);
        }
        const std::vector<double> spectrum = measure::magnitude_spectrum_db(output, output.size() - measure::K_FFT_SIZE);
        const std::uint32_t band_edge = static_cast<std::uint32_t>(K_PASSBAND_EDGE * measure::K_FFT_SIZE);
        double worst = -300.0;
        for (std::uint32_t bin = 8; bin <= band_edge; ++bin)
        {
            worst = std::max(worst, spectrum[bin]);
        }
        return worst - reference;
    }

    /// @brief Returns the sweep steps, in units of 0.05 of the base rate, whose fold lands outside the
    /// stage-1 transition band; the integer steps keep near-integer fractions from drifting.
    std::vector<int> alias_steps()
    {
        std::vector<int> steps;
        for (int step = 11; step <= 79; ++step)
        {
            const double fraction = step * 0.05;
            const double remainder = std::fmod(fraction, 1.0);
            const double folded = remainder > 0.5 ? 1.0 - remainder : remainder;
            if (folded <= K_PASSBAND_EDGE)
            {
                steps.push_back(step);
            }
        }
        return steps;
    }

    std::string step_name(const ::testing::TestParamInfo<int>& info)
    {
        return "Step" + std::to_string(info.param);
    }

    std::string fraction_name(const ::testing::TestParamInfo<double>& info)
    {
        return "Fraction" + std::to_string(static_cast<int>(std::lround(info.param * 100.0)));
    }

    class Small_OversamplerAlias : public ::testing::TestWithParam<int>
    {
    };

    class Small_OversamplerImage : public ::testing::TestWithParam<double>
    {
    };

    struct DesignRow
    {
            const char* name;
            const SushiDSP::HalfbandDesign* design;
    };

    const DesignRow kDesignRows[] = {
        {"Stage1", &SushiDSP::K_HALFBAND_STAGE1},
        {"Stage2", &SushiDSP::K_HALFBAND_STAGE2},
        {"Stage3", &SushiDSP::K_HALFBAND_STAGE3},
    };

    std::string design_name(const ::testing::TestParamInfo<DesignRow>& info)
    {
        return info.param.name;
    }

    class Small_OversamplerDesign : public ::testing::TestWithParam<DesignRow>
    {
    };

    /// What a used-then-reset instance and a never-used one return for the same probe.
    struct ResetOutcome
    {
            std::vector<float> after_reset;
            std::vector<float> never_used;
    };

    const std::vector<float> kBurst = sine(0.1913, 512, 0.9f);
    const std::vector<float> kProbe = sine(0.0611, 128, 0.6f);

    ResetOutcome upsampler_reset_outcome()
    {
        SushiDSP::HalfbandUpsampler2x used(SushiDSP::K_HALFBAND_STAGE1);
        SushiDSP::HalfbandUpsampler2x fresh(SushiDSP::K_HALFBAND_STAGE1);
        std::vector<float> scratch(kBurst.size() * 2, 0.0f);
        used.process(kBurst.data(), static_cast<std::uint32_t>(kBurst.size()), scratch.data());
        used.reset();
        ResetOutcome outcome{std::vector<float>(kProbe.size() * 2, 0.0f), std::vector<float>(kProbe.size() * 2, 0.0f)};
        used.process(kProbe.data(), static_cast<std::uint32_t>(kProbe.size()), outcome.after_reset.data());
        fresh.process(kProbe.data(), static_cast<std::uint32_t>(kProbe.size()), outcome.never_used.data());
        return outcome;
    }

    ResetOutcome downsampler_reset_outcome()
    {
        SushiDSP::HalfbandDownsampler2x used(SushiDSP::K_HALFBAND_STAGE1);
        SushiDSP::HalfbandDownsampler2x fresh(SushiDSP::K_HALFBAND_STAGE1);
        std::vector<float> scratch(kBurst.size() / 2, 0.0f);
        used.process(kBurst.data(), static_cast<std::uint32_t>(kBurst.size()), scratch.data());
        used.reset();
        ResetOutcome outcome{std::vector<float>(kProbe.size() / 2, 0.0f), std::vector<float>(kProbe.size() / 2, 0.0f)};
        used.process(kProbe.data(), static_cast<std::uint32_t>(kProbe.size()), outcome.after_reset.data());
        fresh.process(kProbe.data(), static_cast<std::uint32_t>(kProbe.size()), outcome.never_used.data());
        return outcome;
    }

    /// @brief Returns the oversampled probe followed by its base-rate return. The oversampler's internal
    /// buffer is reused, so the oversampled block is copied out before it is decimated.
    template <typename Oversampler, std::uint32_t Factor>
    std::vector<float> probe_through(Oversampler& oversampler)
    {
        const float* up = oversampler.upsample(kProbe.data(), 128);
        std::vector<float> result(up, up + 128 * Factor);
        std::vector<float> back(128, 0.0f);
        oversampler.downsample(result.data(), 128 * Factor, back.data());
        result.insert(result.end(), back.begin(), back.end());
        return result;
    }

    template <typename Oversampler, std::uint32_t Factor>
    ResetOutcome oversampler_reset_outcome()
    {
        Oversampler used(128);
        Oversampler fresh(128);
        std::vector<float> scratch(128, 0.0f);
        for (std::size_t position = 0; position + 128 <= kBurst.size(); position += 128)
        {
            const float* up = used.upsample(kBurst.data() + position, 128);
            used.downsample(up, 128 * Factor, scratch.data());
        }
        used.reset();
        return ResetOutcome{probe_through<Oversampler, Factor>(used), probe_through<Oversampler, Factor>(fresh)};
    }

    struct ResetRow
    {
            const char* name;
            ResetOutcome (*outcome)();
    };

    const ResetRow kResetRows[] = {
        {"HalfbandUpsampler", &upsampler_reset_outcome},
        {"HalfbandDownsampler", &downsampler_reset_outcome},
        {"Oversampler4x", &oversampler_reset_outcome<SushiDSP::Oversampler4x, 4>},
        {"Oversampler8x", &oversampler_reset_outcome<SushiDSP::Oversampler8x, 8>},
    };

    std::string reset_name(const ::testing::TestParamInfo<ResetRow>& info)
    {
        return info.param.name;
    }

    class Small_OversamplerReset : public ::testing::TestWithParam<ResetRow>
    {
    };

    /// @brief Returns the sample at the end of the block after the 8x round trip has run on a constant 1.
    float eight_times_dc_output()
    {
        SushiDSP::Oversampler8x oversampler(K_BLOCK);
        const std::vector<float> input(K_BLOCK, 1.0f);
        std::vector<float> output(K_BLOCK, 0.0f);
        for (int block = 0; block < 8; ++block)
        {
            const float* up = oversampler.upsample(input.data(), K_BLOCK);
            oversampler.downsample(up, K_BLOCK * 8, output.data());
        }
        return output.back();
    }

    /// @brief Returns the error bound of the 8x DC reading: per stage the accumulator's pair terms,
    /// summing to at most 3 Σ|h| (twice for the upsampler's doubled pairs, once for the downsampler),
    /// and the table's own float rounding, u Σ|h| for each of the two directions.
    double eight_times_dc_bound()
    {
        double bound = 0.0;
        for (const DesignRow& row : kDesignRows)
        {
            const double sum = magnitude_sum(*row.design);
            bound += kU * (static_cast<double>(pair_count(*row.design)) + 3.0) * 3.0 * sum + 2.0 * kU * sum;
        }
        return bound;
    }

    /// @brief Fills a block with a short repeating pattern.
    std::vector<float> pattern_block(std::uint32_t size)
    {
        std::vector<float> block(size, 0.0f);
        for (std::uint32_t i = 0; i < size; ++i)
        {
            block[i] = static_cast<float>(i % 5) * 0.1f - 0.2f;
        }
        return block;
    }
} // namespace

TEST_P(Small_OversamplerDesign, HasACentreTapOfExactlyOneHalf)
{
    const SushiDSP::HalfbandDesign& design = *GetParam().design;
    ASSERT_EQ(design.tap_count % 4, 3u);

    EXPECT_EQ(design.taps[(design.tap_count - 1) / 2], 0.5f);
}

TEST_P(Small_OversamplerDesign, MeetsTheStopbandSpec)
{
    EXPECT_LE(GetParam().design->stopband_db, K_SPEC_REJECTION_DB);
}

INSTANTIATE_TEST_SUITE_P(Designs, Small_OversamplerDesign, ::testing::ValuesIn(kDesignRows), design_name);

TEST(Small_Oversampler, HalfbandUpsamplerMatchesDirectFormOfZeroStuffedInput)
{
    const std::vector<float> input = sine(0.0731, 4096, 0.7f);

    const std::vector<float> output = upsample_in_two_blocks(input);

    const double bound = halfband_bound(SushiDSP::K_HALFBAND_STAGE1, 2.0 * peak_of(input));
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(worst_difference(output, reference_upsample(SushiDSP::K_HALFBAND_STAGE1, input)), 0.0,
                                   bound)));
}

TEST(Small_Oversampler, HalfbandDownsamplerMatchesDirectForm)
{
    const std::vector<float> input = sine(0.0377, 8192, 0.7f);

    const std::vector<float> output = downsample_in_two_blocks(input);

    const double bound = halfband_bound(SushiDSP::K_HALFBAND_STAGE1, peak_of(input));
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        worst_difference(output, reference_downsample(SushiDSP::K_HALFBAND_STAGE1, input)), 0.0, bound)));
}

// A tone's images sit at k*f_base +- f. See tests/foundation/math/README.md.
TEST_P(Small_OversamplerImage, ImagesOfAnInBandToneAreBelowTheSpecRejection)
{
    EXPECT_LE(worst_image_db(GetParam()), K_SPEC_REJECTION_DB);
}

INSTANTIATE_TEST_SUITE_P(Fractions, Small_OversamplerImage, ::testing::Values(0.30, 0.35, 0.40, 0.45), fraction_name);

// Folds into (0.45..0.5)*f_base are not in the rows. See tests/foundation/math/README.md.
TEST_P(Small_OversamplerAlias, AliasesOfAnOutOfBandToneAreBelowTheSpecRejection)
{
    EXPECT_LE(worst_alias_db(GetParam()), K_SPEC_REJECTION_DB);
}

INSTANTIATE_TEST_SUITE_P(Steps, Small_OversamplerAlias, ::testing::ValuesIn(alias_steps()), step_name);

// 20 kHz is 0.4167*f_base, inside the 0.45 passband. See tests/foundation/math/README.md.
TEST_P(Small_OversamplerClass, RoundTripDroopAt20kIsWithinTheSpecDroop)
{
    EXPECT_LE(std::abs(GetParam().gain_db_at_20k()), K_SPEC_DROOP_DB);
}

// The reported floor is within one sample of the peak. See tests/foundation/math/README.md.
TEST_P(Small_OversamplerClass, LatencyMatchesMeasuredImpulseDelay)
{
    const long difference = static_cast<long>(GetParam().impulse_peak()) - static_cast<long>(GetParam().latency());

    EXPECT_GE(difference, 0);
    EXPECT_LE(difference, 1);
}

INSTANTIATE_TEST_SUITE_P(Classes, Small_OversamplerClass, ::testing::ValuesIn(kClassRows), class_name);

TEST(Small_Oversampler, Oversampler8xPassesDcAtUnity)
{
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(eight_times_dc_output(), 1.0, eight_times_dc_bound())));
}

// A reset instance matches a never-used one bit for bit. See tests/foundation/math/README.md.
TEST_P(Small_OversamplerReset, ReturnsTheStageToItsAsConstructedState)
{
    const ResetOutcome outcome = GetParam().outcome();

    EXPECT_EQ(outcome.after_reset, outcome.never_used);
}

INSTANTIATE_TEST_SUITE_P(Stages, Small_OversamplerReset, ::testing::ValuesIn(kResetRows), reset_name);

TEST(Small_Oversampler, Oversampler8xRoundTripPerformsNoHeapAllocation)
{
    constexpr std::uint32_t block_size = 64;
    SushiDSP::Oversampler8x oversampler(block_size);
    const std::vector<float> input = pattern_block(block_size);
    std::vector<float> output(block_size, 0.0f);

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    const float* oversampled = oversampler.upsample(input.data(), block_size);
    oversampler.downsample(oversampled, block_size * 8, output.data());
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}
