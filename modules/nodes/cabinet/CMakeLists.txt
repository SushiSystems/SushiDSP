# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_library(cabinet STATIC
    source/cabinet_node.cpp
)
add_library(SushiDSP::cabinet ALIAS cabinet)
target_link_libraries(cabinet PUBLIC SushiDSP::sushi_dsp_core SushiDSP::sushi_dsp_math)
sushidsp_apply_optimization_flags(cabinet)
sushidsp_apply_sanitizers(cabinet)
target_include_directories(cabinet PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
