/**
 * @file log_join.hpp
 * @brief Declares the C call a host makes to join the plug-in's logger to its own.
 * @author Mustafa Garip
 */

#pragma once

#include <SushiLog/log/log_c_sink.h>

#if defined(_WIN32)
/// @brief Exports a symbol from the plug-in so a host can look it up by name.
#define SUSHIDSP_PLUGIN_EXPORT __declspec(dllexport)
#else
/// @brief Exports a symbol from the plug-in so a host can look it up by name.
#define SUSHIDSP_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

extern "C"
{
    /// @brief Sends every record of the plug-in's private SushiLog to the host's, in order.
    /// The host calls it once, before the plug-in logs, with sl_forward_record of its own
    /// SushiLog and the user pointer that function expects.
    SUSHIDSP_PLUGIN_EXPORT void sushidsp_join_log(SushiLogSinkFunction forward, void* user);
}