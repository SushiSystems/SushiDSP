# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_library(sushi_dsp_log STATIC
    source/async_log_sink.cpp
    source/capturing_log_sink.cpp
    source/default_logger.cpp
    source/log_argument.cpp
    source/log_formatter.cpp
    source/log_line_queue.cpp
    source/logger.cpp
    source/scoped_log_sink.cpp
    source/stderr_log_sink.cpp
)
add_library(SushiDSP::sushi_dsp_log ALIAS sushi_dsp_log)
sushidsp_apply_optimization_flags(sushi_dsp_log)
sushidsp_apply_warning_flags(sushi_dsp_log)
sushidsp_apply_sanitizers(sushi_dsp_log)
target_include_directories(sushi_dsp_log PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_include_directories(sushi_dsp_log PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
