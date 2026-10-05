/****************************************************************/
/* test_cabinet_directivity.cpp                                 */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_cabinet_directivity.cpp
 * @brief Tests for CabinetDirectivity, the piston-in-a-baffle model of the room module.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>

#include "SushiDSP/room/CabinetDirectivity.hpp"

namespace
{
    /// @brief Holds pi.
    constexpr double kPi = 3.14159265358979323846;
}

TEST(Unit_Dsp, CabinetDirectivityIsUnityOnAxis)
{
    const SushiDSP::room::CabinetDirectivity cabinet(SushiDSP::room::kCabinet4x12);
    const SushiDSP::room::BandResponse on_axis = cabinet.gain_for(0.0, 0.0);
    for (std::size_t band = 0; band < 6u; ++band)
    {
        EXPECT_NEAR(on_axis.gain[band], 1.0f, 1e-6f);
    }
}

namespace
{
    /// @brief Returns the 4x12 closed form at one frequency in the front hemisphere: the piston
    /// factor at the shrinking effective radius times the two-by-two array factor.
    double closed_form(double frequency_hz, double left, double up)
    {
        const SushiDSP::room::CabinetGeometry& g = SushiDSP::room::kCabinet4x12;
        const double wavenumber = 2.0 * kPi * frequency_hz / SushiDSP::room::kSpeedOfSound;
        const double ratio = frequency_hz / g.breakup_frequency_hz;
        const double radius = g.cone_radius_m / std::sqrt(1.0 + ratio * ratio);
        const double sine = std::sqrt(left * left + up * up);
        const double piston =
            SushiDSP::room::CabinetDirectivity::piston_factor(wavenumber * radius * sine);
        const double half_spacing = 0.5 * wavenumber * g.driver_spacing_m;
        return std::fabs(piston) * std::fabs(std::cos(half_spacing * left))
            * std::fabs(std::cos(half_spacing * up));
    }

    /// @brief Returns the energy mean of the closed form over one octave band, in decibels, by
    /// the midpoint rule over 1024 equal log-frequency intervals.
    double octave_mean_db(std::size_t band, double left, double up)
    {
        constexpr int points = 1024;
        double power_sum = 0.0;
        for (int point = 0; point < points; ++point)
        {
            const double octaves = (static_cast<double>(point) + 0.5) / points - 0.5;
            const double frequency = SushiDSP::room::kBandCentresHz[band] * std::exp2(octaves);
            const double factor = closed_form(frequency, left, up);
            power_sum += factor * factor;
        }
        return 10.0 * std::log10(power_sum / points);
    }
}

TEST(Unit_Dsp, CabinetDirectivityBeamsAtTwoKilohertzAndNotAtOneTwentyFive)
{
    const SushiDSP::room::CabinetDirectivity cabinet(SushiDSP::room::kCabinet4x12);
    const SushiDSP::room::BandResponse off = cabinet.gain_for(kPi / 3.0, 0.0);   // 60 degrees
    // At the 2 kHz centre itself: a(f) = 0.125 / sqrt(1 + 0.64) = 0.097614 m, k = 36.6300 /m,
    // k*a*sin60 = 3.0958 so the piston is 0.195099 (-14.19 dB), and the array is
    // |cos(36.6300 * 0.30 * 0.866025 / 2)| = 0.046834 (-26.59 dB), together -40.78 dB. The
    // array factor's null sits 26 Hz above the centre, so the band's energy mean over 1414 to
    // 2828 Hz reads -15.29 dB by the 1024-point midpoint rule of the same closed form.
    EXPECT_NEAR(off.decibels()[4], -15.29f, 0.05f);
    // 125 Hz: the piston is 0.992358 and the array 0.956086 at the centre, -0.46 dB; the
    // energy mean over the octave reads -0.49 dB.
    EXPECT_NEAR(off.decibels()[0], -0.49f, 0.05f);
}

TEST(Unit_Dsp, CabinetDirectivityAveragesTheArrayNullOverTheOctave)
{
    // The reference garage's direct path leaves the baffle atan(0.7 / 2.5) = 15.64 degrees
    // up. There the two rows of drivers, 0.30 m apart, cancel at 343 / (2 * 0.30 * sin 15.64)
    // = 2120 Hz: at the 2 kHz centre the array factor alone is -21.02 dB and the closed form
    // -22.05 dB, while the energy mean over the octave reads -11.48 dB, since the null is
    // narrow and the array factor recovers to 0 dB at both edges of the band.
    const SushiDSP::room::CabinetDirectivity cabinet(SushiDSP::room::kCabinet4x12);
    const double elevation = std::atan2(0.7, 2.5);
    const std::array<float, 6> direct = cabinet.gain_for(0.0, elevation).decibels();
    EXPECT_NEAR(20.0 * std::log10(closed_form(2000.0, 0.0, std::sin(elevation))), -22.05, 0.02);
    EXPECT_NEAR(direct[4], -11.48f, 0.05f);
    EXPECT_NEAR(direct[5], -3.49f, 0.05f);
}

TEST(Unit_Dsp, CabinetDirectivityMatchesTheOctaveMeanOfItsClosedForm)
{
    // The model samples sixteen points per octave; the test integrates the same closed form
    // with 1024, and the two agree within 0.05 dB in every band at every direction tried,
    // 60 degrees off axis, 15.64 degrees up, and 52.85 degrees up.
    const SushiDSP::room::CabinetDirectivity cabinet(SushiDSP::room::kCabinet4x12);
    for (const double elevation : { 0.0, std::atan2(0.7, 2.5), std::atan2(3.3, 2.5) })
    {
        const double azimuth = elevation == 0.0 ? kPi / 3.0 : 0.0;
        const double left = std::cos(elevation) * std::sin(azimuth);
        const double up = std::sin(elevation);
        const std::array<float, 6> got = cabinet.gain_for(azimuth, elevation).decibels();
        for (std::size_t band = 0; band < 6u; ++band)
        {
            EXPECT_NEAR(got[band], octave_mean_db(band, left, up), 0.05)
                << "elevation " << elevation << " band " << band;
        }
    }
}

TEST(Unit_Dsp, CabinetDirectivityPistonFactorMatchesTheBesselSeries)
{
    EXPECT_NEAR(SushiDSP::room::CabinetDirectivity::piston_factor(0.0), 1.0, 1e-12);
    EXPECT_NEAR(SushiDSP::room::CabinetDirectivity::piston_factor(1.0), 0.880101, 1e-6);
    EXPECT_NEAR(SushiDSP::room::CabinetDirectivity::piston_factor(3.831706), 0.0, 1e-4);
    EXPECT_NEAR(SushiDSP::room::CabinetDirectivity::piston_factor(-1.0), 0.880101, 1e-6);
}

TEST(Unit_Dsp, CabinetDirectivityIndexIsSmallWhereTheConeIsOmnidirectional)
{
    // 125 Hz gives k*a = 0.286, so the piston and array together cost under 0.2 dB over the
    // sphere; the closed box's rear shelf reads -1.55 dB over the back hemisphere, which is
    // 10*log10(2 / (1 + 0.70)) = 0.71 dB of index on its own. Together the index stays under
    // 1.5 dB, and by 2 kHz the beam is over 10 dB.
    const SushiDSP::room::CabinetDirectivity cabinet(SushiDSP::room::kCabinet4x12);
    const std::array<float, 6> index = cabinet.directivity_index_db();
    EXPECT_GT(index[0], 0.5f);
    EXPECT_LT(index[0], 1.5f);
    EXPECT_GT(index[4], 10.0f);
    for (std::size_t band = 1; band < 6u; ++band)
    {
        EXPECT_GT(index[band], index[band - 1u]);
    }
}

TEST(Unit_Dsp, CabinetDirectivityIsSymmetricLeftAndRight)
{
    const SushiDSP::room::CabinetDirectivity cabinet(SushiDSP::room::kCabinet4x12);
    for (const double azimuth : { 0.2, 0.7, 1.4, 2.4 })
    {
        const SushiDSP::room::BandResponse left = cabinet.gain_for(azimuth, 0.0);
        const SushiDSP::room::BandResponse right = cabinet.gain_for(-azimuth, 0.0);
        for (std::size_t band = 0; band < 6u; ++band)
        {
            EXPECT_NEAR(left.gain[band], right.gain[band], 1e-6f) << "azimuth " << azimuth;
        }
    }
}

TEST(Unit_Dsp, CabinetDirectivityFallsSixDecibelsBehindTheBaffleAboveTheStep)
{
    // Straight behind, the piston and the array are both 1 and only the rear shelf remains.
    // The 4x12's step is 115 / 0.75 = 153.3 Hz. At 4 kHz the shelf is
    // sqrt((1 + (4000/306.7)^2) / (1 + (4000/153.3)^2)) = sqrt(171.1 / 681.6) = -6.00 dB;
    // at 125 Hz it is sqrt(1.1662 / 1.6647) = -1.55 dB.
    const SushiDSP::room::CabinetDirectivity cabinet(SushiDSP::room::kCabinet4x12);
    const std::array<float, 6> behind = cabinet.gain_for(kPi, 0.0).decibels();
    EXPECT_NEAR(behind[5], -6.00f, 0.05f);
    EXPECT_NEAR(behind[0], -1.55f, 0.05f);
    EXPECT_NEAR(behind[5] - behind[0], -4.45f, 0.10f);
}
