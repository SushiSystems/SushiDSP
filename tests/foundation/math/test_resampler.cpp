/**************************************************************************/
/* test_resampler.cpp                                                     */
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
 * @file test_resampler.cpp
 * @brief Tests for the arbitrary-ratio polyphase Resampler.
 * @author Mustafa Garip & Sushi Systems
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/math/resampler.hpp>
#include <SushiDSP/foundation/math/resampler_design.hpp>
#include <SushiDSP/foundation/signal_measure/null_test.hpp>
#include <SushiDSP/foundation/signal_measure/spectrum.hpp>

#include "support/tolerance.hpp"

namespace
{
    /// Pi to double precision, spelled out so the tests do not depend on M_PI being visible.
    constexpr double K_PI = 3.14159265358979323846;

    /// Frames at the outer rate that a round trip's start-up covers.
    ///
    /// A stage's first outputs are formed against a zero history where the signal's own past
    /// belongs, so they carry whatever that past held. The error dies inside one kernel half
    /// span, widest at the slowest ratio used here: 34 / (20000/48000) = 81.6 source samples.
    constexpr std::size_t K_SETTLING_FRAMES = 256;

    /// Linear-interpolation error of the kernel table, from tools/dsp/design_resampler.py.
    constexpr double K_INTERPOLATION_ERROR = 3.8127e-6;

    /// The most taps one output touches: two loops of 82 at the slowest clock of 20 kHz,
    /// 2 * ceil(34 / (20000 / 48000)) = 164.
    constexpr std::size_t K_WORST_TAPS = 164;

    /// Rounded operations on the longest path to a latency in seconds: the scale, the division
    /// by the lower rate, the conversion and the minimum.
    constexpr std::size_t K_LATENCY_OPERATIONS = 4;

    /// The residual, in decibels, below which a round trip must null. A characterisation level:
    /// the kernel's own limit is -100 dB (stopband) and -108 dB (interpolation floor), two
    /// stages add in power to about -97 dB, and 17 dB stay for the float buffers.
    constexpr double K_ROUND_TRIP_LEVEL_DB = -80.0;

    /// The level, in decibels against the tone, below which the fold of a tone above the output
    /// Nyquist must sit. 15 kHz is 0.75 cycles per sample at the lower rate, deep in the stopband
    /// (K_RESAMPLER_STOPBAND_DECIBELS = -100.477), so 10 dB stay for the window and the float output.
    constexpr double K_FOLD_REJECTION_DB = -90.0;

    /// The level, in decibels against the tone, below which the image of an upsampled tone must
    /// sit: the stopband (-100.477 dB) and the interpolation floor (20 log10 of K_INTERPOLATION_ERROR,
    /// -108.4 dB) add in power to -99.8 dB, which leaves 4.8 dB for the float output.
    constexpr double K_IMAGE_REJECTION_DB = -95.0;

    /// The most a tone at the passband edge may change level, in decibels: the promise itself.
    constexpr double K_PASSBAND_TOLERANCE_DB = 0.10;

    /// The level, in decibels, below which the tail of a starved run must null against the live
    /// input. A characterisation level: the 7 kHz content sits in the passband at 1:1, so the
    /// kernel's own error is below -100 dB, and 40 dB stay for the dropped backlog's seam.
    constexpr double K_STARVED_TAIL_LEVEL_DB = -60.0;

    /// Blocks at the end of a starved run in which the output buffer is large again.
    constexpr std::size_t K_RECOVERY_BLOCKS = 8;

    /// Frames per block in the block-wise tests.
    constexpr std::uint32_t K_BLOCK = 256;

    /// Blocks that the output-count test pushes.
    constexpr std::uint32_t K_COUNT_BLOCKS = 4000;

    /// The slack on the output count: the start-up deficit is at most the latency in output
    /// frames, K_RESAMPLER_HALF_WIDTH_ZERO_CROSSINGS = 34, plus one for the floor of the phase.
    constexpr double K_COUNT_BOUND = SushiDSP::K_RESAMPLER_HALF_WIDTH_ZERO_CROSSINGS + 1.0;

    /// Outputs skipped at the start of the sweep: the kernel half span in output frames,
    /// widest at the 82.5 kHz start, 34 * 82500 / 48000 = 58.4.
    constexpr std::size_t K_SWEEP_SETTLING_OUTPUTS = 64;

    /// Largest step of a 100 Hz unit sine at the slowest output clock of 20 kHz: 2 pi 100 / 20000.
    constexpr double K_SINE_STEP = 2.0 * K_PI * 100.0 / 20000.0;

    /// Streams a signal through a resampler in fixed-size pushes.
    /// @param resampler A prepared resampler.
    /// @param input The signal at the resampler's source rate.
    /// @param push Frames per process() call.
    /// @return Everything the resampler produced, in order.
    std::vector<float> stream(SushiDSP::Resampler& resampler, const std::vector<float>& input,
                              std::uint32_t push)
    {
        std::vector<float> output;
        std::vector<float> scratch(resampler.maximum_output_frames(push) + 8u, 0.0f);
        for (std::size_t position = 0; position < input.size(); position += push)
        {
            const std::uint32_t frames =
                static_cast<std::uint32_t>(std::min<std::size_t>(push, input.size() - position));
            const std::uint32_t produced =
                resampler.process(input.data() + position, frames, scratch.data(),
                                  static_cast<std::uint32_t>(scratch.size()));
            output.insert(output.end(), scratch.begin(), scratch.begin() + produced);
        }
        return output;
    }

    /// Streams a signal through a resampler with an output capacity chosen per block.
    /// @param resampler A prepared resampler.
    /// @param input The signal at the resampler's source rate.
    /// @param capacity_of Maps a block index to the output capacity handed to process().
    /// @return Everything the resampler produced, in order.
    template <typename Capacity>
    std::vector<float> stream_with_capacity(SushiDSP::Resampler& resampler,
                                            const std::vector<float>& input, Capacity capacity_of)
    {
        std::vector<float> output;
        std::vector<float> scratch(1024u, 0.0f);
        for (std::size_t block = 0; block * K_BLOCK < input.size(); ++block)
        {
            const std::uint32_t produced = resampler.process(
                input.data() + block * K_BLOCK, K_BLOCK, scratch.data(), capacity_of(block));
            output.insert(output.end(), scratch.begin(), scratch.begin() + produced);
        }
        return output;
    }

    /// Residual of b against a once both have left their start-up region.
    /// @param a Reference signal, sample-aligned with b.
    /// @param b Signal under test.
    /// @pre Both signals hold more than K_SETTLING_FRAMES samples.
    /// @return Residual in dB.
    double settled_null_test(const std::vector<float>& a, const std::vector<float>& b)
    {
        const std::vector<float> settled_a(a.begin() + K_SETTLING_FRAMES, a.end());
        const std::vector<float> settled_b(b.begin() + K_SETTLING_FRAMES, b.end());
        return SushiDSP::measure::null_test(settled_a, settled_b, 512u);
    }

    /// Sum of unit-amplitude sines, scaled so the peak stays inside +/-1.
    /// @param frequencies_hz The partials.
    /// @param sample_rate Sample rate.
    /// @param length Samples to generate.
    /// @return The signal.
    std::vector<float> multitone(const std::vector<double>& frequencies_hz, double sample_rate,
                                 std::size_t length)
    {
        std::vector<float> signal(length, 0.0f);
        const double amplitude = 0.8 / static_cast<double>(frequencies_hz.size());
        for (std::size_t n = 0; n < length; ++n)
        {
            double value = 0.0;
            for (const double frequency : frequencies_hz)
            {
                value += amplitude *
                         std::sin(2.0 * K_PI * frequency * static_cast<double>(n) / sample_rate);
            }
            signal[n] = static_cast<float>(value);
        }
        return signal;
    }

    /// A sine.
    /// @param frequency_hz The frequency.
    /// @param sample_rate Sample rate.
    /// @param length Samples to generate.
    /// @param amplitude Peak value.
    /// @return The signal.
    std::vector<float> tone(double frequency_hz, double sample_rate, std::size_t length,
                            double amplitude)
    {
        std::vector<float> signal(length, 0.0f);
        for (std::size_t n = 0; n < length; ++n)
        {
            signal[n] = static_cast<float>(
                amplitude * std::sin(2.0 * K_PI * frequency_hz * static_cast<double>(n) / sample_rate));
        }
        return signal;
    }

    /// Worst-case error of a settled unity DC level, with nothing cancelling: every one of the
    /// K_WORST_TAPS taps carries the table's interpolation error and the float rounding of an
    /// entry no larger than two.
    double dc_error_bound()
    {
        return static_cast<double>(K_WORST_TAPS) *
               (K_INTERPOLATION_ERROR + 2.0 * TestSupport::unit_roundoff<float>());
    }

    /// Bound on the recurrence residual of a 100 Hz sinusoid decimated 16:1.
    ///
    /// At 16:1 every output reads the table at the same phase, so the output is a sinusoid up to
    /// rounding. The three terms are floats off by at most u each, and the float input sine's own
    /// u passes through a kernel whose absolute sum is at most two, so each term is off by 3 u.
    /// @param recurrence_coefficient 2 cos(w), so the weights are 1, 1 and its magnitude.
    /// @return The bound.
    double recurrence_bound(double recurrence_coefficient)
    {
        return (2.0 + std::abs(recurrence_coefficient)) * 3.0 * TestSupport::unit_roundoff<float>();
    }

    /// Bound on the step between neighbouring outputs of the swept 100 Hz sine: the step of a
    /// sampled sine at the slowest clock, K_SINE_STEP, grown by the passband gain bound of
    /// K_PASSBAND_TOLERANCE_DB and by the rounding of the two floats subtracted.
    double sweep_step_bound()
    {
        return K_SINE_STEP * std::pow(10.0, K_PASSBAND_TOLERANCE_DB / 20.0) +
               2.0 * TestSupport::unit_roundoff<float>();
    }

    /// Frames of input after which a settled analysis window of the output fits.
    ///
    /// The output window is the last K_FFT_SIZE outputs, which take K_FFT_SIZE * source / target
    /// input frames; the reference window is the last K_FFT_SIZE input frames. The longer of the
    /// two plus the start-up margin covers both.
    /// @param source_rate Rate of the input.
    /// @param target_rate Rate of the output.
    /// @return Frames to generate.
    std::size_t analysis_frames(double source_rate, double target_rate)
    {
        const double fft_size = static_cast<double>(SushiDSP::measure::K_FFT_SIZE);
        const double for_output = std::ceil(fft_size * source_rate / target_rate);
        return static_cast<std::size_t>(std::max(fft_size, for_output)) + K_SETTLING_FRAMES;
    }

    /// Level of the strongest bin near a frequency in the last transform window of a signal.
    /// @param signal At least K_FFT_SIZE samples.
    /// @param frequency_hz Where to look.
    /// @param sample_rate Rate of the signal.
    /// @return Level in dB, unnormalised; use differences between levels.
    double tone_level_db(const std::vector<float>& signal, double frequency_hz, double sample_rate)
    {
        const auto spectrum = SushiDSP::measure::magnitude_spectrum_db(
            signal, signal.size() - SushiDSP::measure::K_FFT_SIZE);
        return SushiDSP::measure::peak_near(spectrum,
                                            SushiDSP::measure::bin_of(frequency_hz, sample_rate));
    }

    /// Index of the strongest bin, DC and Nyquist excluded, in the last window of a signal.
    /// @param signal At least K_FFT_SIZE samples.
    /// @return The bin index.
    std::uint32_t strongest_bin(const std::vector<float>& signal)
    {
        const auto spectrum = SushiDSP::measure::magnitude_spectrum_db(
            signal, signal.size() - SushiDSP::measure::K_FFT_SIZE);
        std::uint32_t best_bin = 1;
        for (std::uint32_t bin = 2; bin + 1 < spectrum.size(); ++bin)
        {
            if (spectrum[bin] > spectrum[best_bin])
            {
                best_bin = bin;
            }
        }
        return best_bin;
    }

    /// Largest distance of a settled stretch of a signal from the value one.
    /// @param output The signal; it holds more than 2200 samples.
    /// @return The worst error.
    double worst_error_from_one(const std::vector<float>& output)
    {
        double worst = 0.0;
        for (std::size_t n = 2000; n < output.size() - 200; ++n)
        {
            worst = std::max(worst, std::abs(static_cast<double>(output[n]) - 1.0));
        }
        return worst;
    }

    /// Whether every sample is exactly zero.
    bool all_zero(const std::vector<float>& signal)
    {
        return std::all_of(signal.begin(), signal.end(), [](float sample) { return sample == 0.0f; });
    }

    /// Whether every sample is finite and no larger than one in magnitude.
    bool all_finite_and_unit_bounded(const std::vector<float>& signal)
    {
        return std::all_of(signal.begin(), signal.end(),
                           [](float sample) { return std::isfinite(sample) && std::abs(sample) <= 1.0f; });
    }

    /// One stage of a round trip.
    struct Stage
    {
            double source_rate;
            double target_rate;
            std::uint32_t max_frames;
            double minimum_ratio;
    };

    /// One round trip through a pair of stages.
    struct RoundTripRow
    {
            const char* name;
            std::vector<double> partials;
            std::size_t length;
            Stage down;
            Stage up;
            std::uint32_t down_push;
            std::uint32_t up_push;
    };

    const RoundTripRow kRoundTripRows[] = {
        // The 7 kHz content sits inside the 8000 Hz passband of the 20 kHz stage.
        {"Dd3SlowClock",
         {100.0, 1000.0, 3000.0, 7000.0},
         96000u,
         {48000.0, 20000.0, 256u, 20000.0 / 48000.0},
         {20000.0, 48000.0, 256u, 20000.0 / 82500.0},
         256u,
         256u},
        {"Dd3FastClock",
         {100.0, 1000.0, 7000.0, 15000.0},
         96000u,
         {48000.0, 82500.0, 256u, 31250.0 / 48000.0},
         {82500.0, 48000.0, 512u, 48000.0 / 82500.0},
         256u,
         256u},
        // 12 kHz content survives at the SPX90 ratio.
        {"Spx90Ratio",
         {100.0, 1000.0, 7000.0, 12000.0},
         96000u,
         {48000.0, 31250.0, 256u, 31250.0 / 48000.0},
         {31250.0, 48000.0, 512u, 48000.0 / 82500.0},
         256u,
         256u},
    };

    /// An upsample-only instance, whose history is sized on min(1, minimum ratio).
    const RoundTripRow kUpsampleOnlyRow = {
        "UpsampleOnly",
        {100.0, 1000.0, 7000.0},
        24000u,
        {48000.0, 192000.0, 8u, 2.0},
        {192000.0, 48000.0, 128u, 48000.0 / 192000.0},
        8u,
        128u};

    /// Residual of a round trip against its own input.
    /// @param row The stages, the content and the push sizes.
    /// @return Residual in dB.
    double round_trip_residual_db(const RoundTripRow& row)
    {
        const std::vector<float> input = multitone(row.partials, row.down.source_rate, row.length);
        SushiDSP::Resampler down;
        SushiDSP::Resampler up;
        down.prepare(row.down.source_rate, row.down.target_rate, row.down.max_frames,
                     row.down.minimum_ratio);
        up.prepare(row.up.source_rate, row.up.target_rate, row.up.max_frames, row.up.minimum_ratio);
        const std::vector<float> middle = stream(down, input, row.down_push);
        const std::vector<float> back = stream(up, middle, row.up_push);
        return settled_null_test(input, back);
    }

    std::string round_trip_name(const ::testing::TestParamInfo<RoundTripRow>& info)
    {
        return info.param.name;
    }

    /// One target clock for the DC test.
    struct DcRow
    {
            const char* name;
            double target_rate;
    };

    const DcRow kDcRows[] = {
        {"Clock20000", 20000.0}, {"Clock31250", 31250.0}, {"Clock48000", 48000.0},
        {"Clock82500", 82500.0}, {"Clock96000", 96000.0},
    };

    std::string dc_name(const ::testing::TestParamInfo<DcRow>& info)
    {
        return info.param.name;
    }

    /// One pair of rates for the latency test.
    struct LatencyRow
    {
            const char* name;
            double source_rate;
            double target_rate;
            std::uint32_t expected_samples;
    };

    // The delay is 34 samples of the lower rate, counted in source samples and rounded.
    const LatencyRow kLatencyRows[] = {
        {"Same48000", 48000.0, 48000.0, 34u},
        {"Up48000To82500", 48000.0, 82500.0, 34u},
        {"Down82500To48000", 82500.0, 48000.0, 58u}, // 34 / (48000/82500) = 58.4375 -> 58
        {"Down48000To20000", 48000.0, 20000.0, 82u},  // 34 / (20000/48000) = 81.6 -> 82
        {"Up20000To48000", 20000.0, 48000.0, 34u},
        {"Down48000To31250", 48000.0, 31250.0, 52u},  // 34 / (31250/48000) = 52.224 -> 52
    };

    std::string latency_name(const ::testing::TestParamInfo<LatencyRow>& info)
    {
        return info.param.name;
    }

    class Small_ResamplerDcLevel : public ::testing::TestWithParam<DcRow>
    {
    };

    class Small_ResamplerRoundTrip : public ::testing::TestWithParam<RoundTripRow>
    {
    };

    class Small_ResamplerLatency : public ::testing::TestWithParam<LatencyRow>
    {
    };

    /// Pushes silence and counts what comes out.
    /// @param resampler A prepared resampler.
    /// @param blocks Blocks of K_BLOCK frames to push.
    /// @return Total outputs produced.
    std::uint64_t outputs_after_silence(SushiDSP::Resampler& resampler, std::uint32_t blocks)
    {
        const std::vector<float> silence(K_BLOCK, 0.0f);
        std::vector<float> scratch(resampler.maximum_output_frames(K_BLOCK) + 8u, 0.0f);
        std::uint64_t produced = 0;
        for (std::uint32_t block = 0; block < blocks; ++block)
        {
            produced += resampler.process(silence.data(), K_BLOCK, scratch.data(),
                                          static_cast<std::uint32_t>(scratch.size()));
        }
        return produced;
    }

    /// What a run with an output buffer too small for most of its blocks leaves behind.
    struct StarvedRun
    {
            std::vector<float> tail;
            std::uint32_t worst_excess = 0;
            bool every_block_produced = true;
            bool bounded = true;
    };

    /// Pushes a signal at 1:1 with room for 100 outputs per block, where 256 are due, and room
    /// for everything in the last K_RECOVERY_BLOCKS blocks.
    /// @param input The signal; a whole number of blocks.
    /// @return The tail from the blocks with room, and the bounds of every block.
    StarvedRun run_starved(const std::vector<float>& input)
    {
        SushiDSP::Resampler resampler;
        resampler.prepare(48000.0, 48000.0, K_BLOCK, 20000.0 / 48000.0);
        const std::size_t blocks = input.size() / K_BLOCK;
        std::vector<float> scratch(1024u, 0.0f);
        StarvedRun run;
        for (std::size_t block = 0; block < blocks; ++block)
        {
            const bool starved = block + K_RECOVERY_BLOCKS < blocks;
            const std::uint32_t capacity = starved ? 100u : 1024u;
            const std::uint32_t produced = resampler.process(
                input.data() + block * K_BLOCK, K_BLOCK, scratch.data(), capacity);
            run.every_block_produced = run.every_block_produced && produced > 0u;
            run.worst_excess = std::max(run.worst_excess, produced > capacity ? produced - capacity : 0u);
            const std::vector<float> block_output(scratch.begin(), scratch.begin() + produced);
            run.bounded = run.bounded && all_finite_and_unit_bounded(block_output);
            if (!starved)
            {
                run.tail.insert(run.tail.end(), block_output.begin(), block_output.end());
            }
        }
        return run;
    }

    /// What the rate change after a starved start leaves behind.
    struct ReclampRun
    {
            std::vector<float> output;
            bool sane = true;
    };

    /// Starves a 1:1 resampler until its phase pins on the floor, then decimates 16:1.
    /// @param input A 100 Hz unit sine at 48 kHz, a whole number of blocks.
    /// @return The output after the rate change, and whether every sample was finite and bounded.
    ReclampRun run_reclamped(const std::vector<float>& input)
    {
        SushiDSP::Resampler resampler;
        resampler.prepare(48000.0, 48000.0, K_BLOCK, 3000.0 / 48000.0);
        std::vector<float> scratch(1024u, 0.0f);
        const std::size_t starved_blocks = 64u;
        for (std::size_t block = 0; block < starved_blocks; ++block)
        {
            resampler.process(input.data() + block * K_BLOCK, K_BLOCK, scratch.data(), 100u);
        }
        resampler.set_rates(48000.0, 3000.0);

        ReclampRun run;
        for (std::size_t block = starved_blocks; block < input.size() / K_BLOCK; ++block)
        {
            const std::uint32_t produced =
                resampler.process(input.data() + block * K_BLOCK, K_BLOCK, scratch.data(),
                                  static_cast<std::uint32_t>(scratch.size()));
            const std::vector<float> block_output(scratch.begin(), scratch.begin() + produced);
            run.sane = run.sane && all_finite_and_unit_bounded(block_output);
            run.output.insert(run.output.end(), block_output.begin(), block_output.end());
        }
        return run;
    }

    /// Largest residual of y[n-1] + y[n+1] - 2 cos(w) y[n] over a signal.
    /// @param output The signal, at least three samples.
    /// @param recurrence_coefficient 2 cos(w) of the sinusoid the signal should be.
    /// @return The worst residual.
    double worst_recurrence_residual(const std::vector<float>& output, double recurrence_coefficient)
    {
        double worst = 0.0;
        for (std::size_t n = 1; n + 1 < output.size(); ++n)
        {
            const double residual =
                std::abs(static_cast<double>(output[n - 1]) + static_cast<double>(output[n + 1]) -
                         recurrence_coefficient * static_cast<double>(output[n]));
            worst = std::max(worst, residual);
        }
        return worst;
    }

    /// Streams a 100 Hz sine through 2000 sub-blocks of 8 frames while the target clock sweeps
    /// from 82.5 kHz to 20 kHz, set at every sub-block.
    /// @return Everything the resampler produced.
    std::vector<float> sweep_output()
    {
        SushiDSP::Resampler resampler;
        resampler.prepare(48000.0, 82500.0, 8u, 20000.0 / 48000.0);
        std::vector<float> scratch(resampler.maximum_output_frames(8u) + 8u, 0.0f);
        std::vector<float> output;
        const std::vector<float> input = tone(100.0, 48000.0, 2000u * 8u, 1.0);
        for (std::uint32_t sub_block = 0; sub_block < 2000u; ++sub_block)
        {
            const double position = static_cast<double>(sub_block) / 2000.0;
            resampler.set_rates(48000.0, 82500.0 * std::pow(4.125, -position));
            const std::uint32_t produced =
                resampler.process(input.data() + sub_block * 8u, 8u, scratch.data(),
                                  static_cast<std::uint32_t>(scratch.size()));
            output.insert(output.end(), scratch.begin(), scratch.begin() + produced);
        }
        return output;
    }

    /// Largest step between neighbouring samples from an index on.
    /// @param output The signal.
    /// @param first The index of the first sample whose step to its predecessor counts.
    /// @return The largest absolute step.
    double largest_step(const std::vector<float>& output, std::size_t first)
    {
        double largest = 0.0;
        for (std::size_t n = first; n < output.size(); ++n)
        {
            largest = std::max(largest, std::abs(static_cast<double>(output[n]) -
                                                 static_cast<double>(output[n - 1])));
        }
        return largest;
    }

    /// Pushes the same block through a resampler a number of times.
    void process_repeatedly(SushiDSP::Resampler& resampler, const std::vector<float>& input,
                            std::vector<float>& output, int blocks)
    {
        for (int block = 0; block < blocks; ++block)
        {
            resampler.process(input.data(), static_cast<std::uint32_t>(input.size()), output.data(),
                              static_cast<std::uint32_t>(output.size()));
        }
    }

    /// Re-clocks a resampler across 20 to 82.5 kHz in 32 steps per block, 8 frames each.
    void sweep_clock_repeatedly(SushiDSP::Resampler& resampler, const std::vector<float>& input,
                                std::array<float, 64>& output, int blocks)
    {
        for (int block = 0; block < blocks; ++block)
        {
            for (int sub_block = 0; sub_block < 32; ++sub_block)
            {
                const double clock_hz =
                    20000.0 + (82500.0 - 20000.0) * static_cast<double>(sub_block) / 31.0;
                resampler.set_rates(48000.0, clock_hz);
                resampler.process(input.data(), 8u, output.data(),
                                  static_cast<std::uint32_t>(output.size()));
            }
        }
    }
} // namespace

// Every polyphase branch has unity DC gain. See tests/foundation/math/README.md.
TEST_P(Small_ResamplerDcLevel, ResamplerHoldsADcLevelAtEveryRatio)
{
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, GetParam().target_rate, K_BLOCK, 20000.0 / 48000.0);
    const std::vector<float> ones(48000u, 1.0f);
    const std::vector<float> output = stream(resampler, ones, K_BLOCK);
    ASSERT_GT(output.size(), 4000u);

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(worst_error_from_one(output), 0.0, dc_error_bound(), 1.0)));
}

INSTANTIATE_TEST_SUITE_P(Clocks, Small_ResamplerDcLevel, ::testing::ValuesIn(kDcRows), dc_name);

// The round trip is limited by the kernel's own error. See tests/foundation/math/README.md.
TEST_P(Small_ResamplerRoundTrip, ResamplerRoundTripNulls)
{
    EXPECT_LE(round_trip_residual_db(GetParam()), K_ROUND_TRIP_LEVEL_DB);
}

INSTANTIATE_TEST_SUITE_P(Clocks, Small_ResamplerRoundTrip, ::testing::ValuesIn(kRoundTripRows),
                         round_trip_name);

// The history is sized on min(1, minimum_ratio). See tests/foundation/math/README.md.
TEST(Small_Resampler, ResamplerSizesTheHistoryForAnUpsampleOnlyInstance)
{
    EXPECT_LE(round_trip_residual_db(kUpsampleOnlyRow), K_ROUND_TRIP_LEVEL_DB);
}

// 15 kHz into 20 kHz would fold to 5000 Hz. See tests/foundation/math/README.md.
TEST(Small_Resampler, ResamplerRejectsAToneAboveTheOutputNyquist)
{
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 20000.0, K_BLOCK, 20000.0 / 48000.0);
    const std::vector<float> input = tone(15000.0, 48000.0, analysis_frames(48000.0, 20000.0), 0.8);
    const std::vector<float> output = stream(resampler, input, K_BLOCK);
    ASSERT_GE(output.size(), SushiDSP::measure::K_FFT_SIZE);

    // The 0 dB reference is the input at its own rate. See tests/foundation/math/README.md.
    const double fold = tone_level_db(output, 5000.0, 20000.0) - tone_level_db(input, 15000.0, 48000.0);
    EXPECT_LE(fold, K_FOLD_REJECTION_DB);
}

// 0.399 of the source rate is inside the 0.4 passband edge. See tests/foundation/math/README.md.
TEST(Small_Resampler, ResamplerPassesAToneAtThePassbandEdgeAtItsLevel)
{
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 96000.0, K_BLOCK, 1.0);
    const double frequency =
        SushiDSP::measure::bin_exact_frequency(19152.0, 96000.0, SushiDSP::measure::K_FFT_SIZE);
    const std::vector<float> input = tone(frequency, 48000.0, analysis_frames(48000.0, 96000.0), 0.5);
    const std::vector<float> output = stream(resampler, input, K_BLOCK);
    ASSERT_GE(output.size(), SushiDSP::measure::K_FFT_SIZE);

    const double change = tone_level_db(output, frequency, 96000.0) - tone_level_db(input, frequency, 48000.0);
    EXPECT_NEAR(change, 0.0, K_PASSBAND_TOLERANCE_DB);
}

// The image of f sits at 48000 - f, inside the stopband. See tests/foundation/math/README.md.
TEST(Small_Resampler, ResamplerSuppressesTheImageOfAnUpsampledTone)
{
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 96000.0, K_BLOCK, 1.0);
    const double frequency =
        SushiDSP::measure::bin_exact_frequency(14400.0, 96000.0, SushiDSP::measure::K_FFT_SIZE);
    const std::vector<float> input = tone(frequency, 48000.0, analysis_frames(48000.0, 96000.0), 0.5);
    const std::vector<float> output = stream(resampler, input, K_BLOCK);
    ASSERT_GE(output.size(), SushiDSP::measure::K_FFT_SIZE);

    const double image =
        tone_level_db(output, 48000.0 - frequency, 96000.0) - tone_level_db(output, frequency, 96000.0);
    EXPECT_LE(image, K_IMAGE_REJECTION_DB);
}

// The output count stays within the start-up deficit of the ratio. See tests/foundation/math/README.md.
TEST(Medium_Resampler, ResamplerOutputCountFollowsTheRatioWithoutDrift)
{
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 20000.0, K_BLOCK, 20000.0 / 48000.0);

    const std::uint64_t produced = outputs_after_silence(resampler, K_COUNT_BLOCKS);

    // 4000 * 256 = 1 024 000 input frames; 1 024 000 * 20000/48000 = 426 666.67 outputs.
    const double expected = static_cast<double>(K_COUNT_BLOCKS) * K_BLOCK * 20000.0 / 48000.0;
    EXPECT_NEAR(static_cast<double>(produced), expected, K_COUNT_BOUND);
}

// Deferred outputs come out on the next call. See tests/foundation/math/README.md.
TEST(Small_Resampler, ResamplerResumesAfterAnUnderSizedOutputBuffer)
{
    const std::vector<float> input = multitone({100.0, 1000.0, 7000.0}, 48000.0, 8192u);
    SushiDSP::Resampler reference;
    SushiDSP::Resampler starved;
    reference.prepare(48000.0, 48000.0, K_BLOCK, 20000.0 / 48000.0);
    starved.prepare(48000.0, 48000.0, K_BLOCK, 20000.0 / 48000.0);
    const std::vector<float> expected = stream(reference, input, K_BLOCK);

    // Room for 100 outputs on every fourth push, where about 256 are due.
    const std::vector<float> actual = stream_with_capacity(
        starved, input, [](std::size_t block) { return block % 4u == 0u ? 100u : 1024u; });

    EXPECT_EQ(actual.size(), expected.size());
    EXPECT_TRUE(actual == expected);
}

// process() never writes more than it has room for. See tests/foundation/math/README.md.
TEST(Small_Resampler, ResamplerNeverProducesMoreThanTheCapacityItWasGiven)
{
    const StarvedRun run = run_starved(multitone({100.0, 1000.0, 7000.0}, 48000.0, 65536u));

    EXPECT_EQ(run.worst_excess, 0u);
    EXPECT_TRUE(run.every_block_produced);
}

// process() drops the backlog instead of wrapping. See tests/foundation/math/README.md.
TEST(Small_Resampler, ResamplerBoundsThePhaseWhenTheOutputBufferStaysTooSmall)
{
    const std::vector<float> input = multitone({100.0, 1000.0, 7000.0}, 48000.0, 65536u);
    const StarvedRun run = run_starved(input);
    ASSERT_GT(run.tail.size(), 1024u);

    SushiDSP::Resampler probe;
    probe.prepare(48000.0, 48000.0, K_BLOCK, 20000.0 / 48000.0);
    const std::size_t latency = probe.latency_samples();
    const std::vector<float> window(input.end() - static_cast<std::ptrdiff_t>(latency + run.tail.size()),
                                    input.end() - static_cast<std::ptrdiff_t>(latency));
    EXPECT_TRUE(run.bounded);
    EXPECT_LE(SushiDSP::measure::null_test(window, run.tail, 256u), K_STARVED_TAIL_LEVEL_DB);
}

// y[n-1] + y[n+1] = 2*cos(w)*y[n] for a sampled sinusoid. See tests/foundation/math/README.md.
TEST(Small_Resampler, ResamplerReclampsThePhaseWhenARateChangeWidensTheKernel)
{
    const ReclampRun run = run_reclamped(tone(100.0, 48000.0, 262144u, 1.0));
    ASSERT_GT(run.output.size(), 1000u);
    EXPECT_TRUE(run.sane);

    const double recurrence = 2.0 * std::cos(2.0 * K_PI * 100.0 / 3000.0);
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        worst_recurrence_residual(run.output, recurrence), 0.0, recurrence_bound(recurrence))));
}

// The delay is 34 / min(source, target) seconds. See tests/foundation/math/README.md.
TEST_P(Small_ResamplerLatency, ResamplerLatencyIsThirtyFourSamplesOfTheLowerRate)
{
    SushiDSP::Resampler resampler;
    resampler.prepare(GetParam().source_rate, GetParam().target_rate, K_BLOCK, 20000.0 / 48000.0);

    const double lower_rate = std::min(GetParam().source_rate, GetParam().target_rate);
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<double>(
        resampler.latency_seconds(), 34.0 / lower_rate, K_LATENCY_OPERATIONS)));
    EXPECT_EQ(resampler.latency_samples(), GetParam().expected_samples);
}

INSTANTIATE_TEST_SUITE_P(Rates, Small_ResamplerLatency, ::testing::ValuesIn(kLatencyRows), latency_name);

// A bin-exact tone lands on its own bin. See tests/foundation/math/README.md.
TEST(Small_Resampler, ResamplerKeepsAToneAtItsFrequency)
{
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 20000.0, K_BLOCK, 20000.0 / 48000.0);
    const double frequency =
        SushiDSP::measure::bin_exact_frequency(1000.0, 20000.0, SushiDSP::measure::K_FFT_SIZE);
    const std::vector<float> input = tone(frequency, 48000.0, analysis_frames(48000.0, 20000.0), 0.5);
    const std::vector<float> output = stream(resampler, input, K_BLOCK);
    ASSERT_GE(output.size(), SushiDSP::measure::K_FFT_SIZE);

    EXPECT_EQ(strongest_bin(output), SushiDSP::measure::bin_of(frequency, 20000.0));
}

// At 20 kHz a 100 Hz sine steps at most 0.0314. See tests/foundation/math/README.md.
TEST(Small_Resampler, ResamplerRateChangeMidStreamIsContinuous)
{
    const std::vector<float> output = sweep_output();
    ASSERT_GT(output.size(), 10000u);

    EXPECT_LE(largest_step(output, K_SWEEP_SETTLING_OUTPUTS), sweep_step_bound());
}

// Reset leaves no tail of the loud signal. See tests/foundation/math/README.md.
TEST(Small_Resampler, ResamplerResetReturnsItToSilence)
{
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 20000.0, K_BLOCK, 20000.0 / 48000.0);
    stream(resampler, std::vector<float>(4096u, 0.9f), K_BLOCK);
    resampler.reset();

    const std::vector<float> output = stream(resampler, std::vector<float>(4096u, 0.0f), K_BLOCK);

    ASSERT_FALSE(output.empty());
    EXPECT_TRUE(all_zero(output));
}

// At the slowest clock interpolate() touches 164 taps. See tests/foundation/math/README.md.
TEST(Medium_Resampler, ProcessPerformsNoHeapAllocation)
{
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 20000.0, K_BLOCK, 20000.0 / 48000.0);
    const std::vector<float> input = tone(440.0, 48000.0, K_BLOCK, 0.3);
    std::vector<float> output(resampler.maximum_output_frames(K_BLOCK) + 16u, 0.0f);

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    process_repeatedly(resampler, input, output, 200);
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

// One 256-frame block calls set_rates 32 times. See tests/foundation/math/README.md.
TEST(Medium_Resampler, RateChangePerformsNoHeapAllocation)
{
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 82500.0, 8u, 20000.0 / 48000.0);
    const std::vector<float> input = tone(440.0, 48000.0, 8u, 0.25);
    std::array<float, 64> output{};

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    sweep_clock_repeatedly(resampler, input, output, 200);
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}
