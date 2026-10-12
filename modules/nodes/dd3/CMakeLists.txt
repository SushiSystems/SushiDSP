# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_library(dd3 STATIC
    source/dd3_analog_filters.cpp
    source/ne570_compander.cpp
    source/dd3_converter.cpp
    source/dd3_clock.cpp
    source/dd3_delay.cpp
)
add_library(SushiDSP::dd3 ALIAS dd3)
target_link_libraries(dd3 PUBLIC SushiDSP::sushi_dsp_core SushiDSP::sushi_dsp_math)
sushidsp_apply_optimization_flags(dd3)
target_include_directories(dd3 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
