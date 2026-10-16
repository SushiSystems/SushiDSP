/**************************************************************************/
/* sushi_log_record.h                                                     */
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
 * @file sushi_log_record.h
 * @brief Declares the C interface a host outside C++ joins the SushiDSP logger through.
 * @author Mustafa Garip
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /// @brief Holds one record for a C sink; no string is NUL-terminated.
    /// A later version only adds fields at the end; a host reads only what its own size covers.
    typedef struct SushiLogRecord
    {
            uint32_t size;        /**< sizeof(SushiLogRecord) of the logger that filled it. */
            uint8_t level;        /**< The LogLevel number, Trace 0 to Fatal 5. */
            const char* project;  /**< The project name. */
            size_t project_size;  /**< The characters of project. */
            const char* module;   /**< The module name. */
            size_t module_size;   /**< The characters of module. */
            const char* file;     /**< The call's file name, without its folders. */
            size_t file_size;     /**< The characters of file. */
            uint32_t line;        /**< The call's line. */
            uint32_t thread;      /**< The logger's number for the calling thread. */
            int64_t time_ns;      /**< System clock time, ns since the Unix epoch. */
            const char* message;  /**< The formatted message alone. */
            size_t message_size;  /**< The characters of message. */
            const char* text;     /**< The whole record line. */
            size_t text_size;     /**< The characters of text. */
    } SushiLogRecord;

    /// @brief Receives each record; it must not throw or unwind into the logger.
    typedef void (*SushiLogSinkFunction)(const SushiLogRecord* record, void* user);

    /// @brief Replaces the sink with a C function; a null function restores stderr.
    void sd_set_log_sink(SushiLogSinkFunction function, void* user);

    /// @brief Nonzero waits for the queue, then delivers each record on the calling thread.
    void sd_set_log_synchronous(int synchronous);

#ifdef __cplusplus
}
#endif
