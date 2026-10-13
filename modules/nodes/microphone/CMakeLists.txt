# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_library(microphone STATIC
    source/microphone_node.cpp
)
add_library(SushiDSP::microphone ALIAS microphone)
target_link_libraries(microphone PUBLIC SushiDSP::sushi_dsp_core)
sushidsp_apply_optimization_flags(microphone)
target_include_directories(microphone PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
