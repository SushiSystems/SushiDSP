/**************************************************************************/
/* sushi_log_record.cpp                                                   */
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
 * @file sushi_log_record.cpp
 * @brief Defines the C interface of the SushiDSP logger.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/log/sushi_log_record.h>

#include <SushiDSP/foundation/log/default_logger.hpp>

extern "C" void sd_set_log_sink(SushiLogSinkFunction function, void* user)
{
    if (function == nullptr)
    {
        SushiDSP::set_log_sink(SushiDSP::LogSink());
        return;
    }
    SushiDSP::set_log_sink(SushiDSP::LogSink(
        [function, user](const SushiDSP::LogRecord& record)
        {
            const SushiLogRecord c_record{static_cast<std::uint32_t>(sizeof(SushiLogRecord)),
                                          static_cast<std::uint8_t>(record.level),
                                          record.project.data(),
                                          record.project.size(),
                                          record.module.data(),
                                          record.module.size(),
                                          record.file.data(),
                                          record.file.size(),
                                          record.line,
                                          record.thread,
                                          record.time_ns,
                                          record.message.data(),
                                          record.message.size(),
                                          record.text.data(),
                                          record.text.size()};
            function(&c_record, user);
        }));
}

extern "C" void sd_set_log_synchronous(int synchronous)
{
    SushiDSP::set_log_synchronous(synchronous != 0);
}
