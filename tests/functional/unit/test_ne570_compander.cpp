/****************************************************************/
/* test_ne570_compander.cpp                                     */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI,
// and it must remain the very first thing in the translation unit.
#define _USE_MATH_DEFINES

/**
 * @file test_ne570_compander.cpp
 * @brief Tests for the NE570 compander model.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "SushiDSP/dd3/Ne570Compander.hpp"
#include "measure/Spectrum.hpp"
#include "measure/Thd.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    /// @brief Settled peak of a compander driven by a sine of the given peak amplitude.
    /// @param half Which half to build.
    /// @param input_peak_volts Peak of the drive signal.
    /// @param frequency_hz Drive frequency.
    /// @return Peak output in volts, measured over the last 20 cycles of a 500 ms run.
    double settled_peak(SushiDSP::dd3::CompanderHalf half, double input_peak_volts, double frequency_hz)
    {
        SushiDSP::dd3::Ne570Compander compander(half);
        compander.prepare(48000.0);
        const std::size_t settle = 24000;                        // 500 ms, 73 time constants
        const std::size_t measure = static_cast<std::size_t>(std::round(20.0 * 48000.0 / frequency_hz));
        double peak = 0.0;
        for (std::size_t n = 0; n < settle + measure; ++n)
        {
            const float y = compander.process_sample(static_cast<float>(
                input_peak_volts * std::sin(2.0 * M_PI * frequency_hz * static_cast<double>(n) / 48000.0)));
            if (n >= settle) { peak = std::max(peak, std::abs(static_cast<double>(y))); }
        }
        return peak;
    }
}

TEST(Unit_Dsp, Ne570CompressorFollowsTheTwoToOneLaw)
{
    // N6: V_out = sqrt(V_ref * V_in) with V_ref = 6.6125 V.
    //   V_in = 0.066125 (0.01 V_ref) -> sqrt(6.6125*0.066125) = 0.66125     (exactly 0.1 V_ref)
    //   V_in = 0.66125  (0.1 V_ref)  -> sqrt(6.6125*0.66125)  = 2.0910639   (0.3162278 V_ref)
    //   V_in = 6.6125   (1.0 V_ref)  -> 6.6125                              (exactly V_ref)
    //   V_in = 26.45    (4.0 V_ref)  -> sqrt(6.6125*26.45)    = 13.225      (exactly 2 V_ref)
    // The 2 % tolerance is the rectifier's own ripple at 1 kHz: the ripple sits at 2 kHz and the
    // 23.4051 Hz pole leaves 23.4051/2000 = 1.17 % of it on the control voltage.
    struct Point { double input; double output; };
    const Point points[4] = { { 0.066125, 0.66125 }, { 0.66125, 2.0910639 },
                              { 6.6125, 6.6125 }, { 26.45, 13.225 } };
    for (const Point& point : points)
    {
        EXPECT_NEAR(settled_peak(SushiDSP::dd3::CompanderHalf::Compressor, point.input, 1000.0),
                    point.output, 0.02 * point.output) << point.input;
    }
}

TEST(Unit_Dsp, Ne570ExpanderFollowsTheOneToTwoLaw)
{
    // N6: V_out = V_in^2 / V_ref.
    //   0.66125    -> 0.66125^2/6.6125    = 0.066125
    //   2.0910639  -> 2.0910639^2/6.6125  = 0.66125
    //   6.6125     -> 6.6125
    //   13.225     -> 13.225^2/6.6125     = 26.45
    struct Point { double input; double output; };
    const Point points[4] = { { 0.66125, 0.066125 }, { 2.0910639, 0.66125 },
                              { 6.6125, 6.6125 }, { 13.225, 26.45 } };
    for (const Point& point : points)
    {
        EXPECT_NEAR(settled_peak(SushiDSP::dd3::CompanderHalf::Expander, point.input, 1000.0),
                    point.output, 0.02 * point.output) << point.input;
    }
}

TEST(Unit_Dsp, Ne570RoundTripIsTheIdentityInTheStaticLimit)
{
    // The two halves are exact inverses on a steady tone: expander(compressor(x)) = x.
    for (const double input : { 0.066125, 0.66125, 2.0910639, 6.6125, 13.225 })
    {
        SushiDSP::dd3::Ne570Compander compressor(SushiDSP::dd3::CompanderHalf::Compressor);
        SushiDSP::dd3::Ne570Compander expander(SushiDSP::dd3::CompanderHalf::Expander);
        compressor.prepare(48000.0);
        expander.prepare(48000.0);
        double peak = 0.0;
        for (std::size_t n = 0; n < 24960; ++n)
        {
            const float y = expander.process_sample(compressor.process_sample(static_cast<float>(
                input * std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(n) / 48000.0))));
            if (n >= 24000) { peak = std::max(peak, std::abs(static_cast<double>(y))); }
        }
        EXPECT_NEAR(peak, input, 0.03 * input) << input;
    }
}

TEST(Unit_Dsp, Ne570EnvelopeReachesSixtyThreePercentAtSixPointEightMilliseconds)
{
    // G(t) = (G_i - G_f)*e^(-t/tau) + G_f with tau = 10k * 0.68u = 6.8 ms exactly.
    // 1 - e^-1 = 0.6321206 of a step, at 0.0068 * 48000 = 326.4 samples.
    SushiDSP::dd3::Ne570Compander expander(SushiDSP::dd3::CompanderHalf::Expander);
    expander.prepare(48000.0);
    const double target = 1.0;                       // a 1 V DC step reads 1 V * pi/2 rectified...
    std::size_t crossing = 0;
    for (std::size_t n = 0; n < 4800; ++n)
    {
        expander.process_sample(static_cast<float>(target));
        // The rectifier reads |x| * pi/2, so its final value is pi/2; the 63.21 % point is
        // 0.6321206 * pi/2 = 0.9929407.
        if (crossing == 0 && expander.envelope_volts() >= 0.6321206 * M_PI / 2.0)
        {
            crossing = n + 1;
        }
    }
    EXPECT_NEAR(static_cast<double>(crossing), 326.4, 3.3);   // +/- 1 %
    EXPECT_NEAR(SushiDSP::dd3::Ne570Compander::kTimeConstantSeconds, 6.8e-3, 1e-12);
    EXPECT_NEAR(SushiDSP::dd3::Ne570Compander::kRippleCornerHz, 23.4051, 1e-3);
}

TEST(Unit_Dsp, Ne570CompressorGainSaturatesAtTheEnvelopeFloor)
{
    // Below V_ref * kEnvelopeFloor = 6.6125e-4 V the gain stops at 1/kEnvelopeFloor = 1e4 and the
    // law goes linear, which is the modelled end of the gain cell's range (N6).
    const double tiny = 1e-9;
    const double output = settled_peak(SushiDSP::dd3::CompanderHalf::Compressor, tiny, 1000.0);
    EXPECT_NEAR(output / tiny, 1.0e4, 1.0e2);
}

TEST(Unit_Dsp, Ne570RectifierRippleReachesTheSignalAtLowFrequencies)
{
    // Dossier 3, "Compander behaviour": the 23.4051 Hz rectifier corner is deliberately fast, and
    // the datasheet's stated penalty - ripple modulating the gain cell into third-harmonic
    // distortion on low-frequency material - is a feature to reproduce, not an artefact to filter
    // out. At 50 Hz the ripple sits at 100 Hz and the pole leaves 23.4051/100 = 23.4 % of it, so
    // the third harmonic must be clearly present. This asserts a floor, not a ceiling: the test
    // fails if a future "improvement" smooths the envelope.
    SushiDSP::dd3::Ne570Compander compressor(SushiDSP::dd3::CompanderHalf::Compressor);
    compressor.prepare(48000.0);
    const double f0 = SushiDSP::measure::bin_exact_frequency(50.0, 48000.0, SushiDSP::measure::kFftSize);
    std::vector<float> output(48000u + SushiDSP::measure::kFftSize, 0.0f);
    for (std::size_t n = 0; n < output.size(); ++n)
    {
        output[n] = compressor.process_sample(static_cast<float>(
            6.6125 * std::sin(2.0 * M_PI * f0 * static_cast<double>(n) / 48000.0)));
    }
    const auto result = SushiDSP::measure::thd(output, f0, 48000.0, 5u);
    std::printf("[ measure ] NE570 compressor at 50 Hz / V_ref: THD %.3f %%, H3 %.2f dB\n",
                result.thd_percent, result.per_harmonic_db[2]);
    EXPECT_GT(result.per_harmonic_db[2], -40.0);
}

TEST(Unit_Dsp, Ne570RectifierDoesNotFoldItsOwnHarmonicsOntoTheCarrier)
{
    // |sin| carries a harmonic at every 2k*f0, so a rectifier sampled at the host rate folds
    // them, and 48000/6 is the tone that folds one worst: the sixth harmonic lands on 48001.5 Hz
    // and aliases to 1.46 Hz, two analysis bins off the carrier and far inside the 23.4051 Hz
    // pole, so it modulates the gain cell at full weight. The board's rectifier is continuous
    // time and does none of this, which is why kRectifierOversampling exists.
    //
    // The fold itself sits at +-2 bins, inside the Blackman-Harris main lobe (+-3 bins) and so
    // unmeasurable here; the search below starts at bin 4 and therefore bounds the comb's tail,
    // not its peak. The worst line outside the main lobe reads -76.05 dB with the sub-stepping
    // and -57.67 dB without it, so the -70 dB bound sits 6 dB above the measurement (headroom
    // against run-to-run noise) and 12.3 dB below what a host-rate rectifier produces (still
    // catching the regression). The tail bound and
    // Ne570RectifierRippleReachesTheSignalAtLowFrequencies's floor are the invariant pair: the
    // genuine 100 Hz ripple a 50 Hz tone makes must survive, and it does, unmoved at -23.75 dB.
    // Driven at 2.0 V, the project-wide full-scale peak (SushiDSP::kFullScaleVolts,
    // include/SushiDSP/core/Calibration.hpp), to catch the fold at the loudest stimulus the
    // converter passes; the ripple test above drives kReferenceVolts because it is exercising
    // the 2:1 law's own pivot point instead.
    SushiDSP::dd3::Ne570Compander compressor(SushiDSP::dd3::CompanderHalf::Compressor);
    compressor.prepare(48000.0);
    const double f0 = SushiDSP::measure::bin_exact_frequency(8000.0, 48000.0, SushiDSP::measure::kFftSize);
    std::vector<float> output(48000u + SushiDSP::measure::kFftSize, 0.0f);
    for (std::size_t n = 0; n < output.size(); ++n)
    {
        output[n] = compressor.process_sample(static_cast<float>(
            2.0 * std::sin(2.0 * M_PI * f0 * static_cast<double>(n) / 48000.0)));
    }
    const auto spectrum = SushiDSP::measure::magnitude_spectrum_db(output, 48000u);
    const std::uint32_t centre = SushiDSP::measure::bin_of(f0, 48000.0);
    const double carrier = SushiDSP::measure::peak_near(spectrum, centre);
    // A bin-exact tone's Blackman-Harris main lobe ends at +-3 bins, so the search starts at 4.
    double worst = -300.0;
    for (int offset = -12; offset <= 12; ++offset)
    {
        if (std::abs(offset) < 4) { continue; }
        worst = std::max(worst, spectrum[static_cast<std::size_t>(
            static_cast<int>(centre) + offset)] - carrier);
    }
    std::printf("[ measure ] NE570 rectifier fold at 48000/6: worst carrier sideband %.2f dB\n",
                worst);
    EXPECT_LE(worst, -70.0);
}

TEST(Unit_Dsp, Ne570HalvesDoNotShareState)
{
    // The expander is open-loop: it tracks the reconstructed signal, never what the compressor
    // did (dossier 1.9 item 2). Two instances driven differently must hold different envelopes.
    SushiDSP::dd3::Ne570Compander first(SushiDSP::dd3::CompanderHalf::Expander);
    SushiDSP::dd3::Ne570Compander second(SushiDSP::dd3::CompanderHalf::Expander);
    first.prepare(48000.0);
    second.prepare(48000.0);
    for (std::size_t n = 0; n < 1000; ++n)
    {
        first.process_sample(1.0f);
        second.process_sample(0.0f);
    }
    EXPECT_GT(first.envelope_volts(), 1.0);
    EXPECT_NEAR(second.envelope_volts(), 0.0, 1e-9);
}
