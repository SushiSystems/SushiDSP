/**************************************************************************/
/* test_sushi_log_record.cpp                                              */
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
 * @file test_sushi_log_record.cpp
 * @brief Tests the C interface a host joins the SushiDSP logger through.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <string>

#include <SushiDSP/foundation/log/default_logger.hpp>
#include <SushiDSP/foundation/log/sushi_log_record.h>
#include <SushiDSP/foundation/log/sushidsp_log_project.hpp>

namespace
{
    /// @brief Holds what the C sink saw.
    struct Seen
    {
            std::string module;  /**< The module of the last record. */
            std::string text;    /**< The line of the last record. */
            unsigned size = 0;   /**< The size field of the last record. */
            unsigned level = 9;  /**< The level number of the last record. */
    };

    /// @brief Copies the record into the Seen the user pointer names.
    void c_sink(const SushiLogRecord* record, void* user)
    {
        Seen& seen = *static_cast<Seen*>(user);
        seen.module.assign(record->module, record->module_size);
        seen.text.assign(record->text, record->text_size);
        seen.size = record->size;
        seen.level = record->level;
    }
} // namespace

TEST(Small_SushiLogRecord, CSinkReceivesEveryFieldAndANullFunctionRestoresStderr)
{
    constexpr SushiDSP::LogCategory category =
        SushiDSP::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "c_test");
    Seen seen;
    sd_set_log_synchronous(1);
    sd_set_log_sink(&c_sink, &seen);
    SushiDSP::default_logger().log(SushiDSP::LogLevel::Error, category,
                                   SushiDSP::LogSite{__FILE__, __LINE__}, "failed");
    sd_set_log_sink(nullptr, nullptr);

    EXPECT_EQ(seen.module, "c_test");
    EXPECT_EQ(seen.size, sizeof(SushiLogRecord));
    EXPECT_EQ(seen.level, 4u);
    EXPECT_NE(seen.text.find("[ERROR] [sushidsp.c_test]"), std::string::npos) << seen.text;
}