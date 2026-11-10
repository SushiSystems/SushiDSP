/**************************************************************************/
/* test_tap_tempo.cpp                                                     */
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
 * @file test_tap_tempo.cpp
 * @brief Tests the tempo TapTempo reads from taps.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <initializer_list>
#include <optional>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"
#include "ui/tap_tempo.hpp"

namespace
{
    /// @brief The time of the tap that follows a gap, in seconds.
    constexpr double K_TAP_AFTER_GAP_SECONDS = 10.0;

    /// @brief The time of the second tap after the gap, in seconds.
    constexpr double K_SECOND_TAP_AFTER_GAP_SECONDS = 10.4;

    /// @brief The interval the two taps after the gap are apart, in seconds.
    constexpr double K_INTERVAL_AFTER_GAP_SECONDS = 0.4;

    /// @brief The tempo that interval reads as, in beats per minute. The interval is a difference of
    /// two times that each carry a rounding, and 60 is divided by it, which sets the bound.
    constexpr double K_TEMPO_AFTER_GAP = 150.0;

    /// @brief Presses the tap at each time and returns what the last press read.
    /// @param taps The tempo reader.
    /// @param times The times of the presses, in seconds.
    std::optional<double> press(ui::TapTempo& taps, std::initializer_list<double> times)
    {
        std::optional<double> last;
        for (const double seconds : times)
        {
            last = taps.tap(seconds);
        }
        return last;
    }
} // namespace

TEST(Small_TapTempo, TapTempoFirstTapNamesNoTempo)
{
    ui::TapTempo taps;

    EXPECT_FALSE(taps.tap(0.0).has_value());
}

TEST(Small_TapTempo, TapTempoReadsFourEvenTapsAsTheirTempo)
{
    ui::TapTempo taps;

    const std::optional<double> tempo = press(taps, {0.0, 0.5, 1.0, 1.5});

    ASSERT_TRUE(tempo.has_value());
    EXPECT_DOUBLE_EQ(*tempo, 120.0);
}

TEST(Small_TapTempo, TapTempoKeepsNoMoreTapsThanItsWindowHolds)
{
    ui::TapTempo taps;
    static_cast<void>(press(taps, {0.0, 0.5, 1.0, 1.5}));

    EXPECT_EQ(taps.tap_count(), ui::TapTempo::K_TAP_COUNT);
}

TEST(Small_TapTempo, TapTempoAveragesOverTheLastFourTapsOnly)
{
    ui::TapTempo taps;

    const std::optional<double> tempo = press(taps, {0.0, 1.0, 2.0, 3.0, 3.5});

    ASSERT_TRUE(tempo.has_value());
    EXPECT_DOUBLE_EQ(*tempo, 72.0);
}

TEST(Small_TapTempo, TapTempoDropsTheOldestTapWhenTheWindowSlides)
{
    ui::TapTempo taps;

    // The window now holds 2.0, 3.0, 3.5 and 4.0; the 0.0 and 1.0 taps have left it.
    const std::optional<double> tempo = press(taps, {0.0, 1.0, 2.0, 3.0, 3.5, 4.0});

    ASSERT_TRUE(tempo.has_value());
    EXPECT_DOUBLE_EQ(*tempo, 90.0);
}

TEST(Small_TapTempo, TapTempoStartsAgainAfterAGapOverTwoSeconds)
{
    ui::TapTempo taps;
    static_cast<void>(press(taps, {0.0, 0.5}));

    EXPECT_FALSE(taps.tap(K_TAP_AFTER_GAP_SECONDS).has_value());
    EXPECT_EQ(taps.tap_count(), 1u);
}

TEST(Small_TapTempo, TapTempoReadsTheTempoOfTheTapsSinceTheGap)
{
    ui::TapTempo taps;

    const std::optional<double> tempo =
        press(taps, {0.0, 0.5, K_TAP_AFTER_GAP_SECONDS, K_SECOND_TAP_AFTER_GAP_SECONDS});

    ASSERT_TRUE(tempo.has_value());
    const double bound = K_TEMPO_AFTER_GAP * TestSupport::unit_roundoff<double>() *
                         (K_SECOND_TAP_AFTER_GAP_SECONDS / K_INTERVAL_AFTER_GAP_SECONDS + 2.0);
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(*tempo, K_TEMPO_AFTER_GAP, bound)));
}

TEST(Small_TapTempo, TapTempoHoldsAFastTempoAtTheMaximum)
{
    ui::TapTempo taps;

    const std::optional<double> tempo = press(taps, {0.0, 0.1});

    ASSERT_TRUE(tempo.has_value());
    EXPECT_DOUBLE_EQ(*tempo, ui::TapTempo::K_MAXIMUM_TEMPO);
}

TEST(Small_TapTempo, TapTempoHoldsASlowTempoAtTheMinimum)
{
    ui::TapTempo taps;

    const std::optional<double> tempo = press(taps, {0.0, 2.0});

    ASSERT_TRUE(tempo.has_value());
    EXPECT_DOUBLE_EQ(*tempo, ui::TapTempo::K_MINIMUM_TEMPO);
}

TEST(Small_TapTempo, TapTempoResetForgetsEveryTap)
{
    ui::TapTempo taps;
    static_cast<void>(press(taps, {0.0, 0.5}));

    taps.reset();

    EXPECT_EQ(taps.tap_count(), 0u);
}
