# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_library(peavey5150 STATIC
    source/peavey5150_tone_stack.cpp
    source/peavey5150.cpp
)
add_library(SushiDSP::peavey5150 ALIAS peavey5150)
target_link_libraries(peavey5150 PUBLIC SushiDSP::sushi_dsp_circuits SushiDSP::sushi_dsp_core SushiDSP::sushi_dsp_math SushiDSP::sushi_dsp_profile)
sushidsp_apply_optimization_flags(peavey5150)
sushidsp_apply_sanitizers(peavey5150)
target_include_directories(peavey5150 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
