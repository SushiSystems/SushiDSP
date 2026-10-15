# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_library(jcm800 STATIC
    source/jcm_tone_stack.cpp
    source/jcm800.cpp
)
add_library(SushiDSP::jcm800 ALIAS jcm800)
target_link_libraries(jcm800 PUBLIC SushiDSP::sushi_dsp_circuits SushiDSP::sushi_dsp_core SushiDSP::sushi_dsp_math)
sushidsp_apply_optimization_flags(jcm800)
sushidsp_apply_sanitizers(jcm800)
target_include_directories(jcm800 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
