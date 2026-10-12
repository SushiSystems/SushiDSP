# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_library(tubescreamer STATIC
    source/tube_screamer.cpp
)
add_library(SushiDSP::tubescreamer ALIAS tubescreamer)
target_link_libraries(tubescreamer PUBLIC SushiDSP::sushi_dsp_circuits SushiDSP::sushi_dsp_core SushiDSP::sushi_dsp_math)
sushidsp_apply_optimization_flags(tubescreamer)
target_include_directories(tubescreamer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
