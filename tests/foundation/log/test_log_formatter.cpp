/**************************************************************************/
/* test_log_formatter.cpp                                                 */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
/**************************************************************************/

/**
 * @file test_log_formatter.cpp
 * @brief Tests the positional formatter: marker order, argument counts, value kinds and cutting.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/log_formatter.hpp>

namespace
{
    /// @brief Formats into a buffer of 256 characters and returns what was written.
    template <typename... Values>
    std::string formatted(std::string_view format, const Values&... values)
    {
        char buffer[256];
        const std::size_t length =
            SushiDSP::format_log_message(buffer, sizeof(buffer), format, values...);
        return std::string(buffer, length);
    }
} // namespace

TEST(LogFormatter, FormatterReplacesMarkersWithArgumentsInOrder)
{
    EXPECT_EQ(formatted("{} + {} = {}", 1, 2, 3), "1 + 2 = 3");
    EXPECT_EQ(formatted("{}{}", "left", "right"), "leftright");
}

TEST(LogFormatter, FormatterKeepsAMarkerThatHasNoArgument)
{
    EXPECT_EQ(formatted("a {} b {}", 7), "a 7 b {}");
    EXPECT_EQ(formatted("plain {}"), "plain {}");
}

TEST(LogFormatter, FormatterDoesNotPrintAnArgumentThatHasNoMarker)
{
    EXPECT_EQ(formatted("a {}", 1, 2), "a 1");
    EXPECT_EQ(formatted("no marker", 1), "no marker");
}

TEST(LogFormatter, FormatterCopiesBracesThatAreNotAMarker)
{
    EXPECT_EQ(formatted("{ } }{ {x}", 1), "{ } }{ {x}");
}

TEST(LogFormatter, FormatterPrintsBoolCharacterAndIntegers)
{
    EXPECT_EQ(formatted("{} {}", true, false), "true false");
    EXPECT_EQ(formatted("{}", 'x'), "x");
    EXPECT_EQ(formatted("{}", -42), "-42");
    EXPECT_EQ(formatted("{}", 18446744073709551615ull), "18446744073709551615");
    EXPECT_EQ(formatted("{}", static_cast<std::size_t>(4096)), "4096");
    EXPECT_EQ(formatted("{}", static_cast<short>(-7)), "-7");
}

TEST(LogFormatter, FormatterPrintsRealsWithSixSignificantDigits)
{
    EXPECT_EQ(formatted("{}", 0.5), "0.5");
    EXPECT_EQ(formatted("{}", 48000.0f), "48000");
    EXPECT_EQ(formatted("{}", 3.14159265), "3.14159");
}

TEST(LogFormatter, FormatterPrintsTextOfEveryKind)
{
    const std::string owned = "owned";
    const std::string_view view = "view";
    const char* pointer = "pointer";
    const char* missing = nullptr;

    EXPECT_EQ(formatted("{} {} {} {}", "literal", owned, view, pointer),
              "literal owned view pointer");
    EXPECT_EQ(formatted("{}", missing), "(null)");
}

TEST(LogFormatter, FormatterCutsTheTextAtTheCapacity)
{
    char buffer[8];

    EXPECT_EQ(SushiDSP::format_log_message(buffer, sizeof(buffer), "0123456789"), 8u);
    EXPECT_EQ(std::string(buffer, 8), "01234567");

    EXPECT_EQ(SushiDSP::format_log_message(buffer, 5, "ab{}yz", "cdefghij"), 5u);
    EXPECT_EQ(std::string(buffer, 5), "abcde");

    EXPECT_EQ(SushiDSP::format_log_message(buffer, 0, "anything {}", 1), 0u);
}
