# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

# Internal module: test-side code that the test programs share. Its headers are never
# installed, and no product links it.

add_library(sushi_dsp_golden_compare INTERFACE)
add_library(SushiDSP::sushi_dsp_golden_compare ALIAS sushi_dsp_golden_compare)
target_include_directories(sushi_dsp_golden_compare INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
sushidsp_require_same_tier_edge(golden_compare log)
target_link_libraries(sushi_dsp_golden_compare INTERFACE SushiDSP::sushi_dsp_log)
