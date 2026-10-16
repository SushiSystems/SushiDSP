# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

# Internal module: test-side code that the test programs share. Its headers are never
# installed, and no product links it.

add_library(sushi_dsp_log_capture STATIC
    source/log_capture.cpp
)
add_library(SushiDSP::sushi_dsp_log_capture ALIAS sushi_dsp_log_capture)
sushidsp_apply_optimization_flags(sushi_dsp_log_capture)
sushidsp_apply_warning_flags(sushi_dsp_log_capture)
sushidsp_apply_sanitizers(sushi_dsp_log_capture)
target_include_directories(sushi_dsp_log_capture PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
sushidsp_require_same_tier_edge(log_capture log)
target_link_libraries(sushi_dsp_log_capture PUBLIC SushiDSP::sushi_dsp_log)