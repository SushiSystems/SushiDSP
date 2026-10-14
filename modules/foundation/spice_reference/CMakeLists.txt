# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

# Internal module: test-side code that the test programs share. Its headers are never
# installed, and no product links it.
sushidsp_require_same_tier_edge(spice_reference math)

add_library(sushi_dsp_spice_reference INTERFACE)
add_library(SushiDSP::sushi_dsp_spice_reference ALIAS sushi_dsp_spice_reference)
target_include_directories(sushi_dsp_spice_reference INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(sushi_dsp_spice_reference INTERFACE
    SushiDSP::sushi_dsp_math
)
