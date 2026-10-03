/**************************************************************************/
/* test_dd3_acceptance.cpp                                                */
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
 * @file test_dd3_acceptance.cpp
 * @brief Acceptance measurements for the Dd3Delay node.
 * @author Mustafa Garip & Sushi Systems
 */

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI,
// and <cmath> may otherwise be pulled in transitively before this point.
#define _USE_MATH_DEFINES

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "SushiDSP/dd3/Dd3Delay.hpp"
#include "measure/AliasFloor.hpp"
#include "measure/Render.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Every case here carries the Regression_Dsp prefix although the file sits under unit/: each
// one renders seconds of audio, and docs/CONTRIBUTING.md section 4 keeps `sd test -s unit` a
// fast edit loop. tests/functional/unit/test_measure.cpp's alias-floor cases do the same.

namespace
{
    constexpr double        kSampleRate = 48000.0;
    constexpr std::uint32_t kBlock      = 256u;

    /// @brief The stimulus the DD-3's own service note measures at, in full-scale units.
    ///
    /// Page 2 of the notes takes its whole check-point ladder at 1 kHz and 100 mVp-p in, i.e.
    /// 0.05 V peak; against the core's 2 V peak full scale that is 0.025 of full scale (dossier
    /// docs/archive/research/2026-08-26-boss-dd3-design-dossier.md, check-point levels).
    constexpr double kServiceNoteStimulus = 0.025;

    /// @brief Half the resampler prototype's width, counted in zero crossings, so the two
    /// conversions cost twice this between them (Task 1.1 committed 34, not the plan's 32).
    ///
    /// The literal restates what Resampler::latency_seconds() documents rather than calling it,
    /// so that a prototype whose half width moves without this file moving fails here instead
    /// of agreeing with itself. Reading the number off the node would make the row an identity.
    constexpr double kResamplerHalfWidth = 34.0;

    /// @brief The wet path's delay from the plan's N2 arithmetic, recomputed here rather than
    /// read off the node, so a wrong N or a wrong clock has nothing to hide behind.
    /// @param ring_length The converter's buffer length for this mode.
    /// @param clock_hz The sample clock the TIME setting asks for.
    /// @param sample_rate The host rate.
    /// @return Ring time, plus both conversions' availability latency, plus the wet prime.
    double predicted_wet_seconds(double ring_length, double clock_hz, double sample_rate)
    {
        const double round_trip = 2.0 * kResamplerHalfWidth / std::min(clock_hz, sample_rate);
        const double prime = static_cast<double>(SushiDSP::Dd3Delay::kWetPrimeFrames) / sample_rate;
        return ring_length / clock_hz + round_trip + prime;
    }

    /// @brief Streams a half-open range of a stimulus through an already-prepared node.
    /// @param node The node under test; prepare() must already have run at this block size.
    /// @param stimulus The whole stimulus.
    /// @param begin First sample of the range.
    /// @param end One past the last sample of the range.
    /// @param block Frames per process() call; the final call carries the remainder.
    /// @param output Destination, at least end samples long.
    void stream_range(SushiDSP::IAudioNode& node, const std::vector<float>& stimulus,
                      std::size_t begin, std::size_t end, std::uint32_t block,
                      std::vector<float>& output)
    {
        for (std::size_t offset = begin; offset < end; offset += block)
        {
            const std::uint32_t frames =
                static_cast<std::uint32_t>(std::min<std::size_t>(block, end - offset));
            const float* inputs[1] = { stimulus.data() + offset };
            float* outputs[1] = { output.data() + offset };
            SushiDSP::ProcessContext context{ outputs, inputs, frames,
                                              static_cast<double>(offset) / kSampleRate };
            node.process(context);
        }
    }

    /// @brief Sample index of the largest magnitude in a half-open window.
    /// @param samples The signal.
    /// @param begin First sample searched.
    /// @param end One past the last sample searched.
    /// @return The index of the peak, or begin for an all-zero window.
    std::size_t peak_index(const std::vector<float>& samples, std::size_t begin, std::size_t end)
    {
        std::size_t found = begin;
        float largest = 0.0f;
        for (std::size_t n = begin; n < end && n < samples.size(); ++n)
        {
            if (std::abs(samples[n]) > largest)
            {
                largest = std::abs(samples[n]);
                found = n;
            }
        }
        return found;
    }

    /// @brief Sample counts between successive upward zero crossings.
    /// @param samples The signal.
    /// @return One entry per crossing pair; the instantaneous period, quantised to whole samples.
    std::vector<double> upward_crossing_periods(const std::vector<float>& samples)
    {
        std::vector<double> periods;
        std::size_t previous = 0;
        for (std::size_t n = 1; n < samples.size(); ++n)
        {
            if (samples[n - 1] <= 0.0f && samples[n] > 0.0f)
            {
                if (previous != 0)
                {
                    periods.push_back(static_cast<double>(n - previous));
                }
                previous = n;
            }
        }
        return periods;
    }

    constexpr double kBatterySweepSeconds   = 2.0;
    constexpr double kBatteryBurstSeconds   = 0.5;
    constexpr double kBatteryStepSeconds    = 0.75;
    constexpr double kBatterySilenceSeconds = 0.75;
    constexpr double kBatterySeconds =
        kBatterySweepSeconds + 3.0 * kBatteryBurstSeconds + kBatteryStepSeconds + kBatterySilenceSeconds;

    /// @brief The D6b stimulus battery, in full-scale sample units rather than volts.
    ///
    /// Copied in shape from tests/functional/unit/test_dk_triode_stage.cpp's convergence case:
    /// a 2 s logarithmic 20 Hz to 20 kHz sweep at -12 dBFS, three 1 kHz bursts at 0.05, 0.5 and
    /// 1.0 full scale, a DC step, and digital silence.
    /// @param sample_rate Rate the battery is generated at.
    /// @return kBatterySeconds seconds of stimulus.
    std::vector<float> battery_stimulus(double sample_rate)
    {
        const std::size_t sweep_samples = static_cast<std::size_t>(kBatterySweepSeconds * sample_rate);
        const std::size_t burst_samples = static_cast<std::size_t>(kBatteryBurstSeconds * sample_rate);
        const std::size_t step_samples  = static_cast<std::size_t>(kBatteryStepSeconds * sample_rate);
        const std::size_t total         = static_cast<std::size_t>(kBatterySeconds * sample_rate);

        std::vector<float> stimulus(total, 0.0f);
        const double low = 20.0;
        const double high = 20000.0;
        const double rate = std::log(high / low);
        for (std::size_t n = 0; n < sweep_samples; ++n)
        {
            const double seconds = static_cast<double>(n) / sample_rate;
            const double phase = 2.0 * M_PI * low * kBatterySweepSeconds / rate
                               * (std::exp(seconds * rate / kBatterySweepSeconds) - 1.0);
            stimulus[n] = 0.251189f * static_cast<float>(std::sin(phase));
        }

        std::size_t position = sweep_samples;
        for (const float amplitude : { 0.05f, 0.5f, 1.0f })
        {
            for (std::size_t n = 0; n < burst_samples && position < total; ++n, ++position)
            {
                stimulus[position] = amplitude * static_cast<float>(
                    std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(n) / sample_rate));
            }
        }
        for (std::size_t n = 0; n < step_samples && position < total; ++n, ++position)
        {
            stimulus[position] = 1.0f;
        }
        // The tail is left at zero: it is both the step's falling edge and the silence case.
        return stimulus;
    }
}

TEST(Regression_Dsp, Dd3AcceptanceClockCurveMatchesThePublishedRange)
{
    // Acceptance row 1: within 2 % of the published 20-82.5 kHz curve. The check is on fs, which
    // is what the service note publishes (a scope on the sample-and-hold); the derived delay
    // times are printed beside it, because N = 1024/4096/16384 while the published times were
    // computed with N = 1000/4000/16000, which is dossier open question 4.
    using Clock = SushiDSP::dd3::Dd3Clock;
    EXPECT_LE(std::abs(Clock::clock_for_time(0.0) - 82500.0) / 82500.0, 0.02);
    EXPECT_LE(std::abs(Clock::clock_for_time(1.0) - 20000.0) / 20000.0, 0.02);

    struct Mode { const char* name; SushiDSP::dd3::Dd3Mode mode; double ring_length; };
    const Mode modes[3] = { { "S", SushiDSP::dd3::Dd3Mode::Short,  1024.0 },
                            { "M", SushiDSP::dd3::Dd3Mode::Middle, 4096.0 },
                            { "L", SushiDSP::dd3::Dd3Mode::Long,   16384.0 } };
    for (const Mode& item : modes)
    {
        for (const float time : { 0.0f, 0.5f, 1.0f })
        {
            SushiDSP::Dd3Delay node;
            node.set_mode(item.mode);
            node.set_time(time);
            node.set_feedback(0.0f);
            node.set_level(1.0f);
            node.prepare({ kSampleRate, kBlock, 1u });

            const double clock_hz = Clock::clock_for_time(static_cast<double>(time));
            const double predicted = predicted_wet_seconds(item.ring_length, clock_hz, kSampleRate);
            // prepare() snaps the R56/C52 pole, so the only slack here is the float the clock
            // keeps log(fs) in, which is 7e-7 of the answer.
            EXPECT_NEAR(node.wet_delay_seconds(), predicted, 1e-4 * predicted)
                << item.name << " " << time;
            std::printf("[ measure ] DD-3 mode %s, TIME %.2f: fs %.1f Hz, ring %.3f ms, "
                        "wet %.3f ms\n",
                        item.name, static_cast<double>(time), clock_hz,
                        1000.0 * item.ring_length / clock_hz, 1000.0 * node.wet_delay_seconds());
        }
    }
}

TEST(Regression_Dsp, Dd3AcceptanceMeasuredEchoLandsWhereTheClockCurveSaysItWill)
{
    // Row 1 again, as a measurement rather than as arithmetic: wet_delay_seconds() is derived
    // from the same clock the prediction uses, so on its own it could agree with a wrong fs.
    // An impulse's echo cannot. TIME 0.5 is the midpoint the endpoints do not cover, and mode L
    // there is 403 ms of ring, which is where a factor-of-four error in N would show first.
    struct Mode { const char* name; SushiDSP::dd3::Dd3Mode mode; double ring_length; };
    const Mode modes[3] = { { "S", SushiDSP::dd3::Dd3Mode::Short,  1024.0 },
                            { "M", SushiDSP::dd3::Dd3Mode::Middle, 4096.0 },
                            { "L", SushiDSP::dd3::Dd3Mode::Long,   16384.0 } };
    const double clock_hz = SushiDSP::dd3::Dd3Clock::clock_for_time(0.5);
    for (const Mode& item : modes)
    {
        SushiDSP::Dd3Delay node;
        node.set_mode(item.mode);
        node.set_time(0.5f);
        node.set_feedback(0.0f);
        node.set_level(1.0f);

        std::vector<float> input(static_cast<std::size_t>(0.6 * kSampleRate), 0.0f);
        input[0] = 1.0f;
        const auto output = SushiDSP::measure::render(node, input, kSampleRate, kBlock);

        const double predicted = predicted_wet_seconds(item.ring_length, clock_hz, kSampleRate);
        const double predicted_samples = predicted * kSampleRate;
        // The search starts past the dry impulse; with F.BACK down there is only one echo.
        const double measured = static_cast<double>(peak_index(output, 8u, output.size()));
        std::printf("[ measure ] DD-3 mode %s echo at TIME 0.50: %.0f samples, predicted %.2f\n",
                    item.name, measured, predicted_samples);
        // Two 7 kHz cascades spread the echo's peak and land it about 13 samples late, so the
        // floor is absolute. It is 48 rather than the 32 test_dd3_delay.cpp uses at TIME 0
        // because the peak is broader here: TIME 0.5 clocks the ring at 40620 Hz, below the
        // host rate, so each conversion's kernel covers 34/40620 s = 40 host samples where at
        // TIME 0 it covers 34/48000 s = 34. The centre of that spread is already in the
        // prediction; what widens is how far the argmax can sit from it. Above 4800 samples the
        // published curve's own 2 % is the looser bound, and 1 % is what stays inside it.
        EXPECT_NEAR(measured, predicted_samples, std::max(48.0, 0.01 * predicted_samples))
            << item.name;
    }
}

TEST(Regression_Dsp, Dd3AcceptancePitchSlursRatherThanJumping)
{
    // Acceptance row 4: "frequency trace under a time-knob sweep shows the R56/C52 glide, not a
    // jump". Fill the ring with a steady tone, freeze it with HOLD so the clock is the only
    // thing left that can move the pitch, then turn D.TIME across its whole range and trace the
    // wet signal's upward zero crossings.
    //
    // The pole itself is pinned exactly in test_dd3_clock.cpp: tau = 0.594 ms, 24.4657 % of the
    // remaining distance per 8-frame sub-block. A single full-scale TIME step is not something
    // a zero-crossing trace can resolve, because the glide ends before the trace takes its
    // first reading: at 48 kHz it is over in about 3 ms, while the probe has already arrived at
    // the destination clock, where a 1 kHz tone reads out at 242.4 Hz and its period is 198
    // samples, or 4.1 ms. The transcription in the task report puts the first crossing at
    // sample 148 and every crossing after it on that settled 198-sample grid. What the trace
    // can resolve, and what the row is about, is the knob being turned: it must walk the whole
    // 4.125:1 range without a step, where an ungliding clock would move it 4.125x between one
    // period and the next.
    using Converter = SushiDSP::dd3::Dd3Converter;
    // 397 cycles per ring pass, so the frozen loop joins its own tail without a phase step.
    // The read pointer does lap the ring during the case - the 20 ms settle spends 1650 ticks
    // at 82500 Hz and the sweep 15474 more, against a 16384-sample ring - and that continuity
    // is what makes the lap harmless rather than one anomalous period in the trace.
    constexpr double kRingCycles = 397.0;
    constexpr double kProbeHz = kRingCycles * SushiDSP::dd3::Dd3Clock::kFastRateHz
                              / static_cast<double>(Converter::kLongLength);
    constexpr double kProbeAmplitude = kServiceNoteStimulus;
    constexpr std::uint32_t kSubBlock = SushiDSP::Dd3Delay::kClockUpdateFrames;
    constexpr double kFillSeconds = 0.6;         // over three passes of the 199 ms ring
    constexpr double kSettleSeconds = 0.02;
    constexpr double kSweepSeconds = 0.35;       // 15474 clock ticks, one ring pass of sweep

    SushiDSP::Dd3Delay node;
    node.set_mode(SushiDSP::dd3::Dd3Mode::Long);
    node.set_time(0.0f);
    node.set_feedback(0.0f);
    node.set_level(1.0f);
    node.prepare({ kSampleRate, kSubBlock, 1u });

    const std::size_t fill_samples = static_cast<std::size_t>(kFillSeconds * kSampleRate);
    std::vector<float> fill(fill_samples, 0.0f);
    for (std::size_t n = 0; n < fill_samples; ++n)
    {
        fill[n] = static_cast<float>(kProbeAmplitude
            * std::sin(2.0 * M_PI * kProbeHz * static_cast<double>(n) / kSampleRate));
    }
    std::vector<float> filled(fill_samples, 0.0f);
    stream_range(node, fill, 0u, fill_samples, kSubBlock, filled);

    // Long and Hold share N, so the switch keeps what the ring holds instead of clearing it.
    node.set_mode(SushiDSP::dd3::Dd3Mode::Hold);

    const std::size_t settle_samples = static_cast<std::size_t>(kSettleSeconds * kSampleRate);
    const std::size_t sweep_samples  = static_cast<std::size_t>(kSweepSeconds * kSampleRate);
    const std::vector<float> silence(settle_samples + sweep_samples, 0.0f);
    std::vector<float> held(silence.size(), 0.0f);
    stream_range(node, silence, 0u, settle_samples, kSubBlock, held);
    for (std::size_t offset = settle_samples; offset < held.size(); offset += kSubBlock)
    {
        node.set_time(static_cast<float>(static_cast<double>(offset - settle_samples)
                                         / static_cast<double>(sweep_samples)));
        stream_range(node, silence, offset,
                     std::min(offset + kSubBlock, held.size()), kSubBlock, held);
    }

    const std::vector<float> swept(held.begin() + static_cast<std::ptrdiff_t>(settle_samples),
                                   held.end());
    const std::vector<double> periods = upward_crossing_periods(swept);
    ASSERT_GT(periods.size(), 250u);

    double worst_step = 1.0;
    for (std::size_t i = 1; i < periods.size(); ++i)
    {
        worst_step = std::max(worst_step, periods[i] / periods[i - 1]);
        // Monotone downward in pitch. The trace quantises the period to whole samples and it
        // starts at 24, so one crossing sample is 4 % and the bound cannot be tighter than
        // that. It is 0.90 rather than 0.96 because 4 % is the quantum of a single reading and
        // two adjacent readings can round opposite ways: the transcription's worst dip is
        // 23/24 = 0.9600, and the extra 6 % is what the resamplers' own smoothing of the trace
        // is allowed to cost.
        EXPECT_GE(periods[i], periods[i - 1] * 0.90) << i;
        // The glide's own largest step is 1.042, an ungliding clock's would be 4.125.
        EXPECT_LE(periods[i] / periods[i - 1], 1.20) << i;
    }
    std::printf("[ measure ] DD-3 pitch slur: period %.0f -> %.0f samples over %zu crossings, "
                "worst step %.4f (a jump would be 4.125)\n",
                periods.front(), periods.back(), periods.size(), worst_step);
    // The glide really did travel: the knob's own range is 4.125 to 1.
    EXPECT_GT(periods.back(), 3.5 * periods.front());
}

TEST(Regression_Dsp, Dd3AcceptanceProducesNoNonFiniteSampleUnderTheStimulusBattery)
{
    // The D6b battery at feedback 0.0, 0.5 and 1.0 - the last of which is past self-oscillation
    // - in every mode. HOLD gets two runs: one from a cold ring, and one entered halfway so
    // that what it freezes is real content rather than the zeros reset() left.
    struct Case { const char* name; SushiDSP::dd3::Dd3Mode initial; SushiDSP::dd3::Dd3Mode later; };
    const Case cases[5] = {
        { "S",      SushiDSP::dd3::Dd3Mode::Short,  SushiDSP::dd3::Dd3Mode::Short },
        { "M",      SushiDSP::dd3::Dd3Mode::Middle, SushiDSP::dd3::Dd3Mode::Middle },
        { "L",      SushiDSP::dd3::Dd3Mode::Long,   SushiDSP::dd3::Dd3Mode::Long },
        { "H",      SushiDSP::dd3::Dd3Mode::Hold,   SushiDSP::dd3::Dd3Mode::Hold },
        { "L to H", SushiDSP::dd3::Dd3Mode::Long,   SushiDSP::dd3::Dd3Mode::Hold },
    };
    const std::vector<float> stimulus = battery_stimulus(kSampleRate);
    const std::size_t switch_at = static_cast<std::size_t>(kBatterySweepSeconds * kSampleRate);

    double loudest = 0.0;
    for (const Case& item : cases)
    {
        for (const float feedback : { 0.0f, 0.5f, 1.0f })
        {
            SushiDSP::Dd3Delay node;
            node.set_mode(item.initial);
            node.set_feedback(feedback);
            node.set_level(1.0f);
            node.set_time(0.35f);
            node.prepare({ kSampleRate, kBlock, 1u });

            std::vector<float> output(stimulus.size(), 0.0f);
            stream_range(node, stimulus, 0u, switch_at, kBlock, output);
            node.set_mode(item.later);
            stream_range(node, stimulus, switch_at, stimulus.size(), kBlock, output);

            for (std::size_t n = 0; n < output.size(); ++n)
            {
                ASSERT_TRUE(std::isfinite(output[n])) << item.name << " " << feedback << " " << n;
                // The converter's own rails bound the loop at about 1.15 full scale, so this is
                // a tripwire for divergence rather than a level the model approaches.
                ASSERT_LT(std::abs(output[n]), 32.0f) << item.name << " " << feedback << " " << n;
                loudest = std::max(loudest, static_cast<double>(std::abs(output[n])));
            }
        }
    }
    std::printf("[ measure ] DD-3 battery: %zu samples over 5 mode cases x 3 F.BACK settings, "
                "0 non-finite, loudest %.4f full scale\n",
                stimulus.size() * 15u, loudest);
}

TEST(Regression_Dsp, Dd3AcceptanceReportsZeroLatencyAndDelaysOnlyItsWetPath)
{
    // Ruling 5. The node reports no latency, and what backs the claim is that the dry path is
    // genuinely sample-aligned: at E.LEVEL 0 the node is bit-close to a wire, which
    // test_dd3_delay.cpp's Dd3DelayAtZeroLevelIsAWire pins. The wet delay is reported apart
    // from it, because it is the effect rather than something a host should compensate.
    SushiDSP::Dd3Delay node;
    node.prepare({ kSampleRate, kBlock, 1u });
    EXPECT_EQ(node.latency_samples(), 0u);
    EXPECT_GT(node.wet_delay_seconds(), 0.0);
}

TEST(Regression_Dsp, Dd3AcceptanceAliasFloorIsMeasuredNotAssumed)
{
    // Ruling 7 deferred oversampling of the analog chain and promised a number instead of a
    // paragraph. alias_floor() sweeps 1-20 kHz tones and reports the worst non-harmonic line.
    // The assertion is deliberately loose: the DD-3 is a 12-bit converter behind a 7 kHz wall
    // and is not supposed to be clean. The printed figure is the point - if it comes back worse
    // than -60 dB, Ruling 7 is revisited with data.
    //
    // The sweep runs at the service note's stimulus, not at alias_floor()'s -12 dBFS default.
    // -12 dBFS is 20 dB above the level this pedal is specified at, and the compressor puts
    // 7.26 V peak on a converter whose rails are +-2.4988 V: 78 % of the converter's samples
    // clip and the sweep then measures that clipping rather than the conversion, reading -20.4
    // dB where an 8 kHz tone's fifth harmonic folds about the 40620 Hz clock onto 619 Hz.
    //
    // Nothing clips at the note's level, and what was left there was the NE570's rectifier
    // aliasing rather than a fold of the test tone: |sin| carries a harmonic at every 2k*f0 and
    // 6*8000.24 Hz folded to 1.46 Hz, inside the rectifier's own 23.4 Hz pole. The Ruling 7
    // revisit took that reading and bought Ne570Compander::kRectifierOversampling with it, which
    // moved this row from -35.86 dB to -55.32 dB. The number still misses the -60 dB the printf
    // names, by 4.7 dB; 16 sub-steps would reach -65.13 dB, and the cost curve that chose 8 is in
    // docs/archive/superpowers/specs/2026-09-05-dd3-spx90-design.md, Model corrections.
    SushiDSP::Dd3Delay node;
    node.set_mode(SushiDSP::dd3::Dd3Mode::Middle);
    node.set_time(0.5f);
    node.set_feedback(0.0f);
    node.set_level(1.0f);
    const double floor_db = SushiDSP::measure::alias_floor(
        node, kSampleRate, static_cast<float>(kServiceNoteStimulus));
    std::printf("[ measure ] DD-3 alias floor at %.3f full scale: %.2f dB "
                "(Ruling 7 revisit threshold -60 dB)\n",
                kServiceNoteStimulus, floor_db);
    EXPECT_LE(floor_db, -40.0);
}
