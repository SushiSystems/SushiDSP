# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

sushidsp_require_same_tier_edge(core io)

add_library(sushi_dsp_core STATIC
    source/buffer_pool.cpp
    source/rig_graph.cpp
    source/rt_graph_slot.cpp
)
add_library(SushiDSP::sushi_dsp_core ALIAS sushi_dsp_core)
sushidsp_apply_optimization_flags(sushi_dsp_core)
sushidsp_apply_warning_flags(sushi_dsp_core)
sushidsp_apply_sanitizers(sushi_dsp_core)
target_include_directories(sushi_dsp_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_include_directories(sushi_dsp_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_link_libraries(sushi_dsp_core PUBLIC SushiDSP::sushi_dsp_io)
