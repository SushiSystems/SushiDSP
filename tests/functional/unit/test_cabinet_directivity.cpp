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
    // The 2 kHz band's energy mean reads -15.29 dB. See tests/functional/unit/README.md.
    EXPECT_NEAR(off.decibels()[4], -15.29f, 0.05f);
    // The 125 Hz band's energy mean reads -0.49 dB. See tests/functional/unit/README.md.
    EXPECT_NEAR(off.decibels()[0], -0.49f, 0.05f);
}

TEST(Unit_Dsp, CabinetDirectivityAveragesTheArrayNullOverTheOctave)
{
    // The row null at 2120 Hz averages to -11.48 dB. See tests/functional/unit/README.md.
    const SushiDSP::room::CabinetDirectivity cabinet(SushiDSP::room::kCabinet4x12);
    const double elevation = std::atan2(0.7, 2.5);
    const std::array<float, 6> direct = cabinet.gain_for(0.0, elevation).decibels();
    EXPECT_NEAR(20.0 * std::log10(closed_form(2000.0, 0.0, std::sin(elevation))), -22.05, 0.02);
    EXPECT_NEAR(direct[4], -11.48f, 0.05f);
    EXPECT_NEAR(direct[5], -3.49f, 0.05f);
}

TEST(Unit_Dsp, CabinetDirectivityMatchesTheOctaveMeanOfItsClosedForm)
{
    // Sixteen points per octave match 1024 within 0.05 dB. See tests/functional/unit/README.md.
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
    // The index stays under 1.5 dB at 125 Hz. See tests/functional/unit/README.md.
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
    // Straight behind, only the rear shelf remains. See tests/functional/unit/README.md.
    const SushiDSP::room::CabinetDirectivity cabinet(SushiDSP::room::kCabinet4x12);
    const std::array<float, 6> behind = cabinet.gain_for(kPi, 0.0).decibels();
    EXPECT_NEAR(behind[5], -6.00f, 0.05f);
    EXPECT_NEAR(behind[0], -1.55f, 0.05f);
    EXPECT_NEAR(behind[5] - behind[0], -4.45f, 0.10f);
}
