/**
 * @file log_join.cpp
 * @brief Defines the C call a host makes to join the plug-in's logger to its own.
 * @author Mustafa Garip
 */

#include "log_join.hpp"

extern "C" void sushidsp_join_log(SushiLogSinkFunction forward, void* user)
{
    sl_set_log_synchronous(1);
    sl_set_log_sink(forward, user);
}