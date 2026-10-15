# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_library(diezel_vh4 STATIC
    source/diezel_vh4_tone_stack.cpp
    source/diezel_vh4.cpp
)
add_library(SushiDSP::diezel_vh4 ALIAS diezel_vh4)
target_link_libraries(diezel_vh4 PUBLIC SushiDSP::sushi_dsp_circuits SushiDSP::sushi_dsp_core SushiDSP::sushi_dsp_math)
sushidsp_apply_optimization_flags(diezel_vh4)
sushidsp_apply_sanitizers(diezel_vh4)
target_include_directories(diezel_vh4 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
