# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

# Internal module: test-side code that the test programs share. Its headers are never
# installed, and no product links it.
sushidsp_require_same_tier_edge(signal_measure core)
sushidsp_require_same_tier_edge(signal_measure math)

add_library(sushi_dsp_signal_measure INTERFACE)
add_library(SushiDSP::sushi_dsp_signal_measure ALIAS sushi_dsp_signal_measure)
target_include_directories(sushi_dsp_signal_measure INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(sushi_dsp_signal_measure INTERFACE
    SushiDSP::sushi_dsp_core
    SushiDSP::sushi_dsp_math
)
