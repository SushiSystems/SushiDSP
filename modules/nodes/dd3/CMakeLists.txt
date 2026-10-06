# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_library(dd3 STATIC
    src/Dd3AnalogFilters.cpp
    src/Ne570Compander.cpp
    src/Dd3Converter.cpp
    src/Dd3Clock.cpp
    src/Dd3Delay.cpp
)
add_library(SushiDSP::dd3 ALIAS dd3)
target_link_libraries(dd3 PUBLIC SushiDSP::sushi_dsp)
sushidsp_apply_optimization_flags(dd3)
target_include_directories(dd3 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
